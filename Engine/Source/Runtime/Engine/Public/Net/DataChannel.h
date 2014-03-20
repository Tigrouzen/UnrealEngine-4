// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DataChannel.h: Unreal datachannel class.
=============================================================================*/

#pragma once

/*-----------------------------------------------------------------------------
	UControlChannel base class.
-----------------------------------------------------------------------------*/

/** network control channel message types
 * 
 * to add a new message type, you need to:
 * - add a DEFINE_CONTROL_CHANNEL_MESSAGE_* for the message type with the appropriate parameters to this file
 * - add IMPLEMENT_CONTROL_CHANNEL_MESSAGE for the message type to DataChannel.cpp
 * - implement the fallback behavior (eat an unparsed message) to UControlChannel::ReceivedBunch()
 *
 * @warning: modifying control channel messages breaks network compatibility (update GEngineMinNetVersion)
 */
template<uint8 MessageType> class FNetControlMessage
{
};
/** contains info about a message type retrievable without static binding (e.g. whether it's a valid type, friendly name string, etc) */
class ENGINE_API FNetControlMessageInfo
{
public:
	static inline const TCHAR* GetName(uint8 MessageIndex)
	{
		CheckInitialized();
		return Names[MessageIndex];
	}
	static inline bool IsRegistered(uint8 MessageIndex)
	{
		CheckInitialized();
		return Names[MessageIndex][0] != 0;
	}
	template<uint8 MessageType> friend class FNetControlMessage;
private:
	static void CheckInitialized()
	{
		static bool bInitialized = false;
		if (!bInitialized)
		{
			for (int32 i = 0; i < ARRAY_COUNT(Names); i++)
			{
				Names[i] = TEXT("");
			}
			bInitialized = true;
		}
	}
	static void SetName(uint8 MessageType, const TCHAR* InName)
	{
		CheckInitialized();
		Names[MessageType] = InName;
	}

	static const TCHAR* Names[256];
};

#define DEFINE_CONTROL_CHANNEL_MESSAGE_ZEROPARAM(Name, Index) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
		static uint8 Initialize() \
		{ \
			FNetControlMessageInfo::SetName(Index, TEXT(#Name)); \
			return 0; \
		} \
		/** sends a message of this type on the specified connection's control channel */ \
		static void Send(UNetConnection* Conn) \
		{ \
			checkAtCompileTime(Index < 256, CONTROL_CHANNEL_MESSAGE_MUST_BE_BYTE); \
			checkSlow(!Conn->IsA(UChildConnection::StaticClass())); /** control channel messages can only be sent on the parent connection */ \
			if (Conn->Channels[0] != NULL && !Conn->Channels[0]->Closing) \
			{ \
				FControlChannelOutBunch Bunch(Conn->Channels[0], false); \
				uint8 MessageType = Index; \
				Bunch << MessageType; \
				Conn->Channels[0]->SendBunch(&Bunch, true); \
			} \
		} \
	};
#define DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(Name, Index, TypeA) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
		static uint8 Initialize() \
		{ \
			FNetControlMessageInfo::SetName(Index, TEXT(#Name)); \
			return 0; \
		} \
		/** sends a message of this type on the specified connection's control channel \
		 * @note: const not used only because of the FArchive interface; the parameters are not modified \
		 */ \
		static void Send(UNetConnection* Conn, TypeA& ParamA) \
		{ \
			checkAtCompileTime(Index < 256, CONTROL_CHANNEL_MESSAGE_MUST_BE_BYTE); \
			checkSlow(!Conn->IsA(UChildConnection::StaticClass())); /** control channel messages can only be sent on the parent connection */ \
			if (Conn->Channels[0] != NULL && !Conn->Channels[0]->Closing) \
			{ \
				FControlChannelOutBunch Bunch(Conn->Channels[0], false); \
				uint8 MessageType = Index; \
				Bunch << MessageType; \
				Bunch << ParamA; \
				Conn->Channels[0]->SendBunch(&Bunch, true); \
			} \
		} \
		/** receives a message of this type from the passed in bunch */ \
		static void Receive(FInBunch& Bunch, TypeA& ParamA) \
		{ \
			Bunch << ParamA; \
		} \
		/** throws away a message of this type from the passed in bunch */ \
		static void Discard(FInBunch& Bunch) \
		{ \
			TypeA ParamA; \
			Receive(Bunch, ParamA); \
		} \
	};
#define DEFINE_CONTROL_CHANNEL_MESSAGE_TWOPARAM(Name, Index, TypeA, TypeB) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
		static uint8 Initialize() \
		{ \
			FNetControlMessageInfo::SetName(Index, TEXT(#Name)); \
			return 0; \
		} \
		/** sends a message of this type on the specified connection's control channel \
		 * @note: const not used only because of the FArchive interface; the parameters are not modified \
		 */ \
		static void Send(UNetConnection* Conn, TypeA& ParamA, TypeB& ParamB) \
		{ \
			checkAtCompileTime(Index < 256, CONTROL_CHANNEL_MESSAGE_MUST_BE_BYTE); \
			checkSlow(!Conn->IsA(UChildConnection::StaticClass())); /** control channel messages can only be sent on the parent connection */ \
			if (Conn->Channels[0] != NULL && !Conn->Channels[0]->Closing) \
			{ \
				FControlChannelOutBunch Bunch(Conn->Channels[0], false); \
				uint8 MessageType = Index; \
				Bunch << MessageType; \
				Bunch << ParamA; \
				Bunch << ParamB; \
				Conn->Channels[0]->SendBunch(&Bunch, true); \
			} \
		} \
		static void Receive(FInBunch& Bunch, TypeA& ParamA, TypeB& ParamB) \
		{ \
			Bunch << ParamA; \
			Bunch << ParamB; \
		} \
		/** throws away a message of this type from the passed in bunch */ \
		static void Discard(FInBunch& Bunch) \
		{ \
			TypeA ParamA; \
			TypeB ParamB; \
			Receive(Bunch, ParamA, ParamB); \
		} \
	};
#define DEFINE_CONTROL_CHANNEL_MESSAGE_THREEPARAM(Name, Index, TypeA, TypeB, TypeC) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
		static uint8 Initialize() \
		{ \
			FNetControlMessageInfo::SetName(Index, TEXT(#Name)); \
			return 0; \
		} \
		/** sends a message of this type on the specified connection's control channel \
		 * @note: const not used only because of the FArchive interface; the parameters are not modified \
		 */ \
		static void Send(UNetConnection* Conn, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC) \
		{ \
			checkAtCompileTime(Index < 256, CONTROL_CHANNEL_MESSAGE_MUST_BE_BYTE); \
			checkSlow(!Conn->IsA(UChildConnection::StaticClass())); /** control channel messages can only be sent on the parent connection */ \
			if (Conn->Channels[0] != NULL && !Conn->Channels[0]->Closing) \
			{ \
				FControlChannelOutBunch Bunch(Conn->Channels[0], false); \
				uint8 MessageType = Index; \
				Bunch << MessageType; \
				Bunch << ParamA; \
				Bunch << ParamB; \
				Bunch << ParamC; \
				Conn->Channels[0]->SendBunch(&Bunch, true); \
			} \
		} \
		static void Receive(FInBunch& Bunch, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC) \
		{ \
			Bunch << ParamA; \
			Bunch << ParamB; \
			Bunch << ParamC; \
		} \
		/** throws away a message of this type from the passed in bunch */ \
		static void Discard(FInBunch& Bunch) \
		{ \
			TypeA ParamA; \
			TypeB ParamB; \
			TypeC ParamC; \
			Receive(Bunch, ParamA, ParamB, ParamC); \
		} \
	};
#define DEFINE_CONTROL_CHANNEL_MESSAGE_FOURPARAM(Name, Index, TypeA, TypeB, TypeC, TypeD) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
		static uint8 Initialize() \
		{ \
			FNetControlMessageInfo::SetName(Index, TEXT(#Name)); \
			return 0; \
		} \
		/** sends a message of this type on the specified connection's control channel \
		 * @note: const not used only because of the FArchive interface; the parameters are not modified \
		 */ \
		static void Send(UNetConnection* Conn, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD) \
		{ \
			checkAtCompileTime(Index < 256, CONTROL_CHANNEL_MESSAGE_MUST_BE_BYTE); \
			checkSlow(!Conn->IsA(UChildConnection::StaticClass())); /** control channel messages can only be sent on the parent connection */ \
			if (Conn->Channels[0] != NULL && !Conn->Channels[0]->Closing) \
			{ \
				FControlChannelOutBunch Bunch(Conn->Channels[0], false); \
				uint8 MessageType = Index; \
				Bunch << MessageType; \
				Bunch << ParamA; \
				Bunch << ParamB; \
				Bunch << ParamC; \
				Bunch << ParamD; \
				Conn->Channels[0]->SendBunch(&Bunch, true); \
			} \
		} \
		static void Receive(FInBunch& Bunch, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD) \
		{ \
			Bunch << ParamA; \
			Bunch << ParamB; \
			Bunch << ParamC; \
			Bunch << ParamD; \
		} \
		/** throws away a message of this type from the passed in bunch */ \
		static void Discard(FInBunch& Bunch) \
		{ \
			TypeA ParamA; \
			TypeB ParamB; \
			TypeC ParamC; \
			TypeD ParamD; \
			Receive(Bunch, ParamA, ParamB, ParamC, ParamD); \
		} \
	};

#define DEFINE_CONTROL_CHANNEL_MESSAGE_SEVENPARAM(Name, Index, TypeA, TypeB, TypeC, TypeD, TypeE, TypeF, TypeG) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
		static uint8 Initialize() \
		{ \
			FNetControlMessageInfo::SetName(Index, TEXT(#Name)); \
			return 0; \
		} \
		/** sends a message of this type on the specified connection's control channel \
		 * @note: const not used only because of the FArchive interface; the parameters are not modified \
		 */ \
		static void Send(UNetConnection* Conn, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD, TypeE& ParamE, TypeF& ParamF, TypeG& ParamG) \
		{ \
			checkAtCompileTime(Index < 256, CONTROL_CHANNEL_MESSAGE_MUST_BE_BYTE); \
			checkSlow(!Conn->IsA(UChildConnection::StaticClass())); /** control channel messages can only be sent on the parent connection */ \
			if (Conn->Channels[0] != NULL && !Conn->Channels[0]->Closing) \
			{ \
				FControlChannelOutBunch Bunch(Conn->Channels[0], false); \
				uint8 MessageType = Index; \
				Bunch << MessageType; \
				Bunch << ParamA; \
				Bunch << ParamB; \
				Bunch << ParamC; \
				Bunch << ParamD; \
				Bunch << ParamE; \
				Bunch << ParamF; \
				Bunch << ParamG; \
				Conn->Channels[0]->SendBunch(&Bunch, true); \
			} \
		} \
		static void Receive(FInBunch& Bunch, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD, TypeE& ParamE, TypeF& ParamF, TypeG& ParamG) \
		{ \
			Bunch << ParamA; \
			Bunch << ParamB; \
			Bunch << ParamC; \
			Bunch << ParamD; \
			Bunch << ParamE; \
			Bunch << ParamF; \
			Bunch << ParamG; \
		} \
		/** throws away a message of this type from the passed in bunch */ \
		static void Discard(FInBunch& Bunch) \
		{ \
			TypeA ParamA; \
			TypeB ParamB; \
			TypeC ParamC; \
			TypeD ParamD; \
			TypeE ParamE; \
			TypeF ParamF; \
			TypeG ParamG; \
			Receive(Bunch, ParamA, ParamB, ParamC, ParamD, ParamE, ParamF, ParamG); \
		} \
	};

#define DEFINE_CONTROL_CHANNEL_MESSAGE_EIGHTPARAM(Name, Index, TypeA, TypeB, TypeC, TypeD, TypeE, TypeF, TypeG, TypeH) \
	enum { NMT_##Name = Index }; \
	template<> class FNetControlMessage<Index> \
	{ \
	public: \
		static uint8 Initialize() \
		{ \
			FNetControlMessageInfo::SetName(Index, TEXT(#Name)); \
			return 0; \
		} \
		/** sends a message of this type on the specified connection's control channel \
		 * @note: const not used only because of the FArchive interface; the parameters are not modified \
		 */ \
		static void Send(UNetConnection* Conn, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD, TypeE& ParamE, TypeF& ParamF, TypeG& ParamG, TypeH& ParamH) \
		{ \
			checkAtCompileTime(Index < 256, CONTROL_CHANNEL_MESSAGE_MUST_BE_BYTE); \
			checkSlow(!Conn->IsA(UChildConnection::StaticClass())); /** control channel messages can only be sent on the parent connection */ \
			if (Conn->Channels[0] != NULL && !Conn->Channels[0]->Closing) \
			{ \
				FControlChannelOutBunch Bunch(Conn->Channels[0], false); \
				uint8 MessageType = Index; \
				Bunch << MessageType; \
				Bunch << ParamA; \
				Bunch << ParamB; \
				Bunch << ParamC; \
				Bunch << ParamD; \
				Bunch << ParamE; \
				Bunch << ParamF; \
				Bunch << ParamG; \
				Bunch << ParamH; \
				Conn->Channels[0]->SendBunch(&Bunch, true); \
			} \
		} \
		static void Receive(FInBunch& Bunch, TypeA& ParamA, TypeB& ParamB, TypeC& ParamC, TypeD& ParamD, TypeE& ParamE, TypeF& ParamF, TypeG& ParamG, TypeH& ParamH) \
		{ \
			Bunch << ParamA; \
			Bunch << ParamB; \
			Bunch << ParamC; \
			Bunch << ParamD; \
			Bunch << ParamE; \
			Bunch << ParamF; \
			Bunch << ParamG; \
			Bunch << ParamH; \
		} \
		/** throws away a message of this type from the passed in bunch */ \
		static void Discard(FInBunch& Bunch) \
		{ \
			TypeA ParamA; \
			TypeB ParamB; \
			TypeC ParamC; \
			TypeD ParamD; \
			TypeE ParamE; \
			TypeF ParamF; \
			TypeG ParamG; \
			TypeH ParamH; \
			Receive(Bunch, ParamA, ParamB, ParamC, ParamD, ParamE, ParamF, ParamG, ParamH); \
		} \
	};

#define IMPLEMENT_CONTROL_CHANNEL_MESSAGE(Name) static uint8 Dummy##_FNetControlMessage_##Name = FNetControlMessage<NMT_##Name>::Initialize();

// message type definitions
DEFINE_CONTROL_CHANNEL_MESSAGE_FOURPARAM(Hello, 0, uint8, int32, int32, FGuid); // initial client connection message
DEFINE_CONTROL_CHANNEL_MESSAGE_TWOPARAM(Welcome, 1, FString, FString); // server tells client they're ok'ed to load the server's level
DEFINE_CONTROL_CHANNEL_MESSAGE_TWOPARAM(Upgrade, 2, int32, int32); // server tells client their version is incompatible
DEFINE_CONTROL_CHANNEL_MESSAGE_TWOPARAM(Challenge, 3, int32, FString); // server sends client challenge string to verify integrity
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(Netspeed, 4, int32); // client sends requested transfer rate
DEFINE_CONTROL_CHANNEL_MESSAGE_THREEPARAM(Login, 5, FString, FString, FUniqueNetIdRepl); // client requests to be admitted to the game
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(Failure, 6, FString); // indicates connection failure
DEFINE_CONTROL_CHANNEL_MESSAGE_EIGHTPARAM(Uses, 7, FGuid, FString, FString, FString, uint32, int32, FString, uint8); // server tells client about a package they should have/acquire
DEFINE_CONTROL_CHANNEL_MESSAGE_TWOPARAM(Have, 8, FGuid, int32); // client tells server what version of a package it has
DEFINE_CONTROL_CHANNEL_MESSAGE_ZEROPARAM(Join, 9); // final join request (spawns PlayerController)
DEFINE_CONTROL_CHANNEL_MESSAGE_TWOPARAM(JoinSplit, 10, FString, FUniqueNetIdRepl); // child player (splitscreen) join request
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(Skip, 12, FGuid); // client request to skip an optional package
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(Abort, 13, FGuid); // client informs server that it aborted a not-yet-verified package due to an UNLOAD request
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(Unload, 14, FGuid); // server tells client that a package is no longer needed
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(PCSwap, 15, int32); // client tells server it has completed a swap of its Connection->Actor
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(ActorChannelFailure, 16, int32); // client tells server that it failed to open an Actor channel sent by the server (e.g. couldn't serialize Actor archetype)
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(DebugText, 17, FString); // debug text sent to all clients or to server
DEFINE_CONTROL_CHANNEL_MESSAGE_TWOPARAM(NetGUIDAssign, 18, FNetworkGUID, FString); // Explicit NetworkGUID assignment. This is rare and only happens if a netguid is only serialized client->server (this msg goes server->client to tell client what ID to use in that case)

// 			Beacon control channel flow
// Client												Server
//	Send<Hello>
//														Receive<Hello> - compare version / game id
//															Send<Upgrade> if incompatible
//															Send<Failure> if wrong game
//															Send<BeaconWelcome> if good so far
//	Receive<BeaconWelcome>
//		Send<NetSpeed>
//		Send<BeaconJoin> with beacon type
//														Receive<Netspeed>
//														Receive<BeaconJoin> - create requested beacon type and create NetGUID
//															Send<Failure> if unable to create or bad type
//															Send<BeaconAssignGUID> with NetGUID for new beacon actor
//	Receive<BeaconAssignGUID> - assign NetGUID to client actor
//		Send<BeaconNetGUIDAck> acknowledging receipt of NetGUID
//														Receive<BeaconNetGUIDAck> - connection complete

DEFINE_CONTROL_CHANNEL_MESSAGE_ZEROPARAM(BeaconWelcome, 25); // server tells client they're ok to attempt to join (client sends netspeed/beacontype)
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(BeaconJoin, 26, FString);  // server tries to create beacon type requested by client, sends NetGUID for actor sync
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(BeaconAssignGUID, 27, FNetworkGUID); // client assigns NetGUID from server to beacon actor, sends NetGUIDAck
DEFINE_CONTROL_CHANNEL_MESSAGE_ONEPARAM(BeaconNetGUIDAck, 28, FString); // server received NetGUIDAck from client, connection established successfully
