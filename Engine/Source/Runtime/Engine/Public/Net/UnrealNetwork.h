// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnrealNetwork.h: Unreal networking.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Forward declarations.
-----------------------------------------------------------------------------*/

#pragma once

class	UChannel;
class	UControlChannel;
class	UActorChannel;
class	UVoiceChannel;
class	UFileChannel;
class	FInBunch;
class	FOutBunch;
class	UChannelIterator;

/*class	UNetDriver;*/
class	UNetConnection;
class	UPendingNetGame;

/*-----------------------------------------------------------------------------
	Types.
-----------------------------------------------------------------------------*/

// Return the value of Max/2 <= Value-Reference+some_integer*Max < Max/2.
inline int32 BestSignedDifference( int32 Value, int32 Reference, int32 Max )
{
	return ((Value-Reference+Max/2) & (Max-1)) - Max/2;
}
inline int32 MakeRelative( int32 Value, int32 Reference, int32 Max )
{
	return Reference + BestSignedDifference(Value,Reference,Max);
}

/**
 * Determine if a connection is compatible with this instance
 *
 * @param bRequireEngineVersionMatch should the engine versions match exactly
 * @param RemoteVer current version of the remote party
 * @param RemoteMinVer min net compatible version of the remote party
 *
 * @return true if the two instances can communicate, false otherwise
 */
inline bool IsNetworkCompatible(bool bRequireEngineVersionMatch, int32 RemoteVer, int32 RemoteMinVer)
{
	bool bEngineVerMatch = !bRequireEngineVersionMatch || RemoteVer == GEngineNetVersion || GEngineNetVersion == 0 || RemoteVer == 0;
	bool bMinEngineVerMatch = (RemoteVer > GEngineMinNetVersion || RemoteVer == 0) && (RemoteMinVer < GEngineNetVersion || GEngineNetVersion == 0);
	return bEngineVerMatch && bMinEngineVerMatch;
}

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/

#include "DataBunch.h"		// Bunch class.
#include "DataChannel.h"	// Channel class.

/*-----------------------------------------------------------------------------
	Replication.
-----------------------------------------------------------------------------*/

/** wrapper to find replicated properties that also makes sure they're valid */
static UProperty* GetReplicatedProperty(UClass* CallingClass, UClass* PropClass, const FName& PropName)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!CallingClass->IsChildOf(PropClass))
	{
		UE_LOG(LogNet, Fatal,TEXT("Attempt to replicate property '%s.%s' in C++ but class '%s' is not a child of '%s'"), *PropClass->GetName(), *PropName.ToString(), *CallingClass->GetName(), *PropClass->GetName());
	}
#endif
	UProperty* TheProperty = FindFieldChecked<UProperty>(PropClass, PropName);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!(TheProperty->PropertyFlags & CPF_Net))
	{
		UE_LOG(LogNet, Fatal,TEXT("Attempt to replicate property '%s' that was not tagged to replicate! Please use 'Replicated' or 'ReplicatedUsing' keyword in the UPROPERTY() declaration."), *TheProperty->GetFullName());
	}
#endif
	return TheProperty;
}
	
#define DOREPLIFETIME(c,v) \
{ \
	static UProperty* sp##v = GetReplicatedProperty(StaticClass(), c::StaticClass(),GET_MEMBER_NAME_CHECKED(c,v)); \
	for ( int32 i = 0; i < sp##v->ArrayDim; i++ )							\
	{																		\
		OutLifetimeProps.AddUnique( FLifetimeProperty( sp##v->RepIndex + i ) );	\
	}																		\
}

#define DOREPLIFETIME_CONDITION(c,v,cond) \
{ \
	static UProperty* sp##v = GetReplicatedProperty(StaticClass(), c::StaticClass(),GET_MEMBER_NAME_CHECKED(c,v)); \
	OutLifetimeProps.AddUnique( FLifetimeProperty( sp##v->RepIndex, cond ) ); \
}

#define DOREPLIFETIME_ACTIVE_OVERRIDE(c,v,active)	\
{													\
	static UProperty* sp##v = GetReplicatedProperty(StaticClass(), c::StaticClass(),GET_MEMBER_NAME_CHECKED(c,v)); \
	for ( int32 i = 0; i < sp##v->ArrayDim; i++ )											\
	{																						\
		ChangedPropertyTracker.SetCustomIsActiveOverride( sp##v->RepIndex + i, active );	\
	}																						\
}

/*-----------------------------------------------------------------------------
	RPC Parameter Validation Helpers
-----------------------------------------------------------------------------*/

// This macro is for RPC parameter validation.
// It handles the details of what should happen if a validation expression fails
#define RPC_VALIDATE( expression )						\
	if ( !( expression ) )								\
	{													\
		UE_LOG( LogNet, Warning,						\
		TEXT("RPC_VALIDATE Failed: ")					\
		TEXT( PREPROCESSOR_TO_STRING( expression ) )	\
		TEXT(" File: ")									\
		TEXT( PREPROCESSOR_TO_STRING( __FILE__ ) )		\
		TEXT(" Line: ")									\
		TEXT( PREPROCESSOR_TO_STRING( __LINE__ ) ) );	\
		return false;									\
	}
