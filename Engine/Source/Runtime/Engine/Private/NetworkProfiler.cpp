// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NetworkProfiler.cpp: server network profiling support.
=============================================================================*/

#include "EnginePrivate.h"

#if USE_NETWORK_PROFILER

#define SCOPE_LOCK_REF(X) FScopeLock ScopeLock(&X);


/**
 * Whether to track the raw network data or not.
 */
#define NETWORK_PROFILER_TRACK_RAW_NETWORK_DATA		0

#include "Net/UnrealNetwork.h"
#if PLATFORM_MAC
	#include <netinet/in.h>
	#include <arpa/inet.h>
#endif
#include "IPAddress.h"
#include "Net/NetworkProfiler.h"

/** Global network profiler instance. */
FNetworkProfiler GNetworkProfiler;

/** Magic value, determining that file is a network profiler file.				*/
#define NETWORK_PROFILER_MAGIC						0x1DBF348A
/** Version of memory profiler. Incremented on serialization changes.			*/
#define NETWORK_PROFILER_VERSION					4

enum ENetworkProfilingPayloadType
{
	NPTYPE_FrameMarker			= 0,	// Frame marker, signaling beginning of frame.	
	NPTYPE_SocketSendTo,				// FSocket::SendTo
	NPTYPE_SendBunch,					// UChannel::SendBunch
	NPTYPE_SendRPC,						// Sending RPC
	NPTYPE_ReplicateActor,				// Replicated object	
	NPTYPE_ReplicateProperty,			// Property being replicated.
	NPTYPE_EndOfStreamMarker,			// End of stream marker		
	NPTYPE_Event,						// Event
	NPTYPE_RawSocketData,				// Raw socket data being sent
};


/*=============================================================================
	Network profiler header.
=============================================================================*/

struct FNetworkProfilerHeader
{
	/** Magic to ensure we're opening the right file.	*/
	uint32	Magic;
	/** Version number to detect version mismatches.	*/
	uint32	Version;

	/** Offset in file for name table.					*/
	uint32	NameTableOffset;
	/** Number of name table entries.					*/
	uint32	NameTableEntries;

	/** Tag, set via -networkprofiler=TAG				*/
	FString Tag;
	/** Game name, e.g. Example							*/
	FString GameName;
	/** URL used to open/ browse to the map.			*/
	FString URL;

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Header		Header to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FNetworkProfilerHeader Header )
	{
		check( Ar.IsSaving() );
		Ar	<< Header.Magic
			<< Header.Version
			<< Header.NameTableOffset
			<< Header.NameTableEntries;
		Header.Tag.SerializeAsANSICharArray( Ar, 255 );
		Header.GameName.SerializeAsANSICharArray( Ar, 255 );
		Header.URL.SerializeAsANSICharArray( Ar, 255 );
		return Ar;
	}
};


/*=============================================================================
	FMallocProfiler implementation.
=============================================================================*/

/**
 * Constructor, initializing member variables.
 */
FNetworkProfiler::FNetworkProfiler()
:	FileWriter(NULL)
,	bHasNoticeableNetworkTrafficOccured(false)
,	bIsTrackingEnabled(false)
,	CurrentURL(NoInit)
{
}

/**
 * Returns index of passed in name into name array. If not found, adds it.
 *
 * @param	Name	Name to find index for
 * @return	Index of passed in name
 */
int32 FNetworkProfiler::GetNameTableIndex( const FString& Name )
{
	// Index of name in name table.
	int32 Index = INDEX_NONE;

	// Use index if found.
	int32* IndexPtr = NameToNameTableIndexMap.Find( Name );
	if( IndexPtr )
	{
		Index = *IndexPtr;
	}
	// Encountered new name, add to array and set index mapping.
	else
	{
		Index = NameArray.Num();
		new(NameArray)FString(Name);
		NameToNameTableIndexMap.Add(*Name,Index);
	}

	check(Index!=INDEX_NONE);
	return Index;
}

/**
 * Enables/ disables tracking. Emits a session changes if disabled.
 *
 * @param	bShouldEnableTracking	Whether tracking should be enabled or not
 */
void FNetworkProfiler::EnableTracking( bool bShouldEnableTracking )
{
	if( bShouldEnableTracking )
	{
		UE_LOG(LogNet, Log, TEXT("Network Profiler: ENABLED"));
	}

	// Flush existing session in progress if we're disabling tracking and it was enabled.
	if( bIsTrackingEnabled && !bShouldEnableTracking )
	{
		TrackSessionChange(false,FURL());
	}
	// Important to not change bIsTrackingEnabled till after we flushed as it's used during flushing.
	bIsTrackingEnabled = bShouldEnableTracking;
}

/**
 * Marks the beginning of a frame.
 */
void FNetworkProfiler::TrackFrameBegin()
{
	if( bIsTrackingEnabled )
	{
		SCOPE_LOCK_REF(CriticalSection);
		uint8 Type = NPTYPE_FrameMarker;
		(*FileWriter) << Type;
		float RelativeTime=  (float)(FPlatformTime::Seconds() - GStartTime);
		(*FileWriter) << RelativeTime;
	}
}

/**
 * Tracks and RPC being sent.
 * 
 * @param	Actor		Actor RPC is being called on
 * @param	Function	Function being called
 * @param	NumBits		Number of bits serialized into bunch for this RPC
 */
void FNetworkProfiler::TrackSendRPC( const AActor* Actor, const UFunction* Function, uint16 NumBits )
{
	if( bIsTrackingEnabled )
	{
		SCOPE_LOCK_REF(CriticalSection);
		uint8 Type = NPTYPE_SendRPC;
		(*FileWriter) << Type;
		int32 ActorNameTableIndex = GetNameTableIndex( Actor->GetName() );
		(*FileWriter) << ActorNameTableIndex;
		int32 FunctionNameTableIndex = GetNameTableIndex( Function->GetName() );
		(*FileWriter) << FunctionNameTableIndex;
		(*FileWriter) << NumBits;
	}
}

/**
 * Low level FSocket::Send information.
 *
 * @param	SocketDesc				Description of socket data is being sent to
 * @param	Data					Data sent
 * @param	BytesSent				Bytes actually being sent
 */
void FNetworkProfiler::TrackSocketSend( const FString& SocketDesc, const void* Data, uint16 BytesSent )
{
	if( bIsTrackingEnabled )
	{
		SCOPE_LOCK_REF(CriticalSection);
		uint32 DummyIp = 0;
		TrackSocketSendToCore( SocketDesc, Data, BytesSent, DummyIp );
	}
}

/**
 * Low level FSocket::SendTo information.
 *
 * @param	SocketDesc				Description of socket data is being sent to
 * @param	Data					Data sent
 * @param	BytesSent				Bytes actually being sent
 * @param	Destination				Destination address
 */
void FNetworkProfiler::TrackSocketSendTo( const FString& SocketDesc, const void* Data, uint16 BytesSent, const FInternetAddr& Destination )
{
	if( bIsTrackingEnabled )
	{
		uint32 NetworkByteOrderIP;
		Destination.GetIp(NetworkByteOrderIP);
		TrackSocketSendToCore( SocketDesc, Data, BytesSent, NetworkByteOrderIP);
	}
}

/**
 * Low level FSocket::SendTo information.
 *
 * @param	SocketDesc				Description of socket data is being sent to
 * @param	Data					Data sent
 * @param	BytesSent				Bytes actually being sent
 * @param	IpAddr					Destination address
 */
void FNetworkProfiler::TrackSocketSendToCore( const FString& SocketDesc, const void* Data, uint16 BytesSent, uint32 IpAddr )
{
	if( bIsTrackingEnabled )
	{
		// Low level socket code is called from multiple threads.
		SCOPE_LOCK_REF(CriticalSection);
		uint8 Type = NPTYPE_SocketSendTo;
		(*FileWriter) << Type;
		uint32 CurrentThreadID = FPlatformTLS::GetCurrentThreadId();
		(*FileWriter) << CurrentThreadID;
		int32 NameTableIndex = GetNameTableIndex( SocketDesc );
		(*FileWriter) << NameTableIndex;
		(*FileWriter) << BytesSent;
		(*FileWriter) << IpAddr;
#if NETWORK_PROFILER_TRACK_RAW_NETWORK_DATA
		Type = NPTYPE_RawSocketData;
		(*FileWriter) << Type;
		(*FileWriter) << BytesSent;
		check( FileWriter->IsSaving() );
		FileWriter->Serialize( const_cast<void*>(Data), BytesSent );
#endif
	}
}

/**
 * Mid level UChannel::SendBunch information.
 * 
 * @param	Channel		UChannel data is being sent to/ on
 * @param	OutBunch	FOutBunch being sent
 * @param	NumBits		Num bits to serialize for this bunch (not including merging)
 */
void FNetworkProfiler::TrackSendBunch( FOutBunch* OutBunch, uint16 NumBits )
{
	if( bIsTrackingEnabled )
	{
		SCOPE_LOCK_REF(CriticalSection);
		uint8 Type = NPTYPE_SendBunch;
		(*FileWriter) << Type;
		uint16 ChannelIndex = OutBunch->ChIndex;
		(*FileWriter) << ChannelIndex;
		uint8 ChannelType = OutBunch->ChType;
		(*FileWriter) << ChannelType;
		(*FileWriter) << NumBits;
	}
}

/**
 * Track actor being replicated.
 *
 * @param	Actor		Actor being replicated
 */
void FNetworkProfiler::TrackReplicateActor( const AActor* Actor, FReplicationFlags RepFlags, uint32 Cycles )
{
	if( bIsTrackingEnabled )
	{
		SCOPE_LOCK_REF(CriticalSection);
		uint8 Type = NPTYPE_ReplicateActor;
		(*FileWriter) << Type;
		uint8 NetFlags = (RepFlags.bNetInitial << 1) | (RepFlags.bNetOwner << 2);
		(*FileWriter) << NetFlags;
		int32 NameTableIndex = GetNameTableIndex( Actor->GetName() );
		(*FileWriter) << NameTableIndex;
		float TimeInMS = FPlatformTime::ToMilliseconds(Cycles);	// FIXME: We may want to just pass in cycles to profiler to we don't lose precision
		(*FileWriter) << TimeInMS;
		// Use actor replication as indication whether session is worth keeping or not.
		bHasNoticeableNetworkTrafficOccured = true;
	}
}

/**
 * Track property being replicated.
 *
 * @param	Property	Property being replicated
 * @param	NumBits		Number of bits used to replicate this property
 */
void FNetworkProfiler::TrackReplicateProperty( const UProperty* Property, bool bIsDynamicProperty, bool bIsComponentProperty, uint32 Cycles, uint16 NumPotentialBits, uint16 NumBits )
{
	if( bIsTrackingEnabled )
	{
		SCOPE_LOCK_REF(CriticalSection);
		uint8 Type = NPTYPE_ReplicateProperty;
		(*FileWriter) << Type;
		uint8 Flags = (bIsDynamicProperty << 0) | (bIsComponentProperty << 1);
		(*FileWriter) << Flags;
		float TimeInMS = FPlatformTime::ToMilliseconds(Cycles);	// FIXME: We may want to just pass in cycles to profiler to we don't lose precision
		(*FileWriter) << TimeInMS;
		int32 NameTableIndex = GetNameTableIndex( Property->GetName() );
		(*FileWriter) << NameTableIndex;
		(*FileWriter) << NumPotentialBits;
		(*FileWriter) << NumBits;
	}
}

/**
 * Track event occurring, like e.g. client join/ leave
 *
 * @param	EventName			Name of the event
 * @param	EventDescription	Additional description/ information for event
 */
void FNetworkProfiler::TrackEvent( const FString& EventName, const FString& EventDescription )
{
	if( bIsTrackingEnabled )
	{
		SCOPE_LOCK_REF(CriticalSection);
		uint8 Type = NPTYPE_Event;
		(*FileWriter) << Type;
		int32 EventNameNameTableIndex = GetNameTableIndex( EventName );
		(*FileWriter) << EventNameNameTableIndex;
		int32 EventDescriptionNameTableIndex = GetNameTableIndex( EventDescription );
		(*FileWriter) << EventDescriptionNameTableIndex ;
	}
}

/**
 * Called when the server first starts listening and on round changes or other
 * similar game events. We write to a dummy file that is renamed when the current
 * session ends.
 *
 * @param	bShouldContinueTracking		Whether to continue tracking
 * @param	InURL						URL used for new session
 */
void FNetworkProfiler::TrackSessionChange( bool bShouldContinueTracking, const FURL& InURL )
{
#if ALLOW_DEBUG_FILES
	if( bIsTrackingEnabled )
	{
		// Session change might occur while other thread uses low level networking.
		SCOPE_LOCK_REF(CriticalSection);

		// End existing tracking session.
		if( FileWriter )
		{	
			if( bHasNoticeableNetworkTrafficOccured )
			{
				UE_LOG(LogNet, Log, TEXT("Netork Profiler: Writing out session file for '%s'"), *CurrentURL.ToString());

				// Write end of stream marker.
				uint8 Type = NPTYPE_EndOfStreamMarker;
				(*FileWriter) << Type;

				// Real header, written at start of the file but overwritten out right before we close the file.
				FNetworkProfilerHeader Header;
				Header.Magic	= NETWORK_PROFILER_MAGIC;
				Header.Version	= NETWORK_PROFILER_VERSION;
				Header.Tag		= TEXT("");
				FParse::Value(FCommandLine::Get(), TEXT("NETWORKPROFILER="), Header.Tag);
				Header.GameName = FApp::GetGameName();
				Header.URL		= CurrentURL.ToString();

				// Write out name table and update header with offset and count.
				Header.NameTableOffset	= FileWriter->Tell();
				Header.NameTableEntries	= NameArray.Num();
				for( int32 NameIndex=0; NameIndex<NameArray.Num(); NameIndex++ )
				{
					NameArray[NameIndex].SerializeAsANSICharArray( *FileWriter );
				}

				// Seek to the beginning of the file and write out proper header.
				FileWriter->Seek( 0 );
				(*FileWriter) << Header;

				// Close file writer so we can rename the file to its final destination.
				FileWriter->Close();
			
				// Rename/ move file.
				const FString FinalFileName = FPaths::ProfilingDir() + GGameName + TEXT("-") + FDateTime::Now().ToString() + TEXT(".nprof");
				bool bWasMovedSuccessfully = IFileManager::Get().Move( *FinalFileName, *TempFileName );

				// Send data to UnrealConsole to upload to DB.
				if( bWasMovedSuccessfully )
				{
					SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!NETWORK:"), *FinalFileName );
				}
			}
			else
			{
				FileWriter->Close();
			}

			// Clean up.
			delete FileWriter;
			FileWriter = NULL;
			bHasNoticeableNetworkTrafficOccured = false;
		}

		if( bShouldContinueTracking )
		{
			// Start a new tracking session.
			check( FileWriter == NULL );

			// Use a dummy name for sessions in progress that is renamed at end.
			TempFileName = FPaths::ProfilingDir() + TEXT("NetworkProfiling.tmp");

			// Create folder and file writer.
			IFileManager::Get().MakeDirectory( *FPaths::GetPath(TempFileName) );
			FileWriter = IFileManager::Get().CreateFileWriter( *TempFileName, FILEWRITE_EvenIfReadOnly );
			checkf( FileWriter );

			// Serialize dummy header, overwritten when session ends.
			FNetworkProfilerHeader DummyHeader;
			FMemory::Memzero( &DummyHeader, sizeof(DummyHeader) );
			(*FileWriter) << DummyHeader;
		}

		CurrentURL = InURL;
	}
#endif	//#if ALLOW_DEBUG_FILES
}

/**
 * Processes any network profiler specific exec commands
 *
 * @param InWorld	The world in this context
 * @param Cmd		The command to parse
 * @param Ar		The output device to write data to
 *
 * @return			True if processed, false otherwise
*/
bool FNetworkProfiler::Exec( UWorld * InWorld, const TCHAR * Cmd, FOutputDevice & Ar )
{
	if (FParse::Command(&Cmd, TEXT("ENABLE")))
	{
		EnableTracking( true );
	}
	else if (FParse::Command(&Cmd, TEXT("DISABLE")))
	{
		EnableTracking( false );
	} 
	else 
	{
		// Default to toggle
		EnableTracking( !bIsTrackingEnabled );
	}

	// If we are tracking, and we don't have a file writer, force one now 
	if ( bIsTrackingEnabled && FileWriter == NULL ) 
	{
		TrackSessionChange( true, InWorld->URL );
		if ( FileWriter == NULL )
		{
			UE_LOG(LogNet, Warning, TEXT("FNetworkProfiler::Exec: FAILED to create file writer!"));
			EnableTracking( false );
		}
	}

	return true;
}

#endif	//#if USE_NETWORK_PROFILER

