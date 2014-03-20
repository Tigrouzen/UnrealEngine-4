// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemSteamPrivatePCH.h"
#include "OnlineAsyncTaskManagerSteam.h"
#include "OnlineSubsystemSteam.h"
#include "OnlineSessionInterfaceSteam.h"
#include "OnlineSessionAsyncLobbySteam.h"
#include "OnlineSessionAsyncServerSteam.h"
#include "OnlineSharedCloudInterfaceSteam.h"
#include "OnlineLeaderboardInterfaceSteam.h"
#include "OnlineExternalUIInterfaceSteam.h"
#include "IPAddressSteam.h"
#include "SocketSubsystemSteam.h"
#include "SocketsSteam.h"
#include "OnlineMsgSteam.h"
#include "SteamUtilities.h"

/**
 * Destroy any messages left in the queue
 */
void FOnlineAsyncTaskManagerSteam::FreeMsgQueue()
{
#if WITH_STEAMGC
	FScopeLock ScopeLock(&InMsgQueueLock);
	for(TMap<int64, FOnlineAsyncMsgSteam*>::TIterator It(InMsgQueue); It; ++It)
	{
		It.Value()->Destroy();
	}

	InMsgQueue.Empty();
#endif
}

void FOnlineAsyncTaskManagerSteam::MessagePump()
{
#if WITH_STEAMGC
	uint32 MsgSize = 0;
	while (SteamCoordinator.IsMessageAvailable(&MsgSize))
	{
		TArray<uint8> Buffer(MsgSize);
		Buffer.AddUninitialized(MsgSize);

		uint32 MsgType;
		uint32 NewMsgSize;

		// Retrieve the message type and its payload
		EGCResults eResult = SteamCoordinator.RetrieveMessage(&MsgType, Buffer.GetData(), MsgSize, &NewMsgSize);
		if (eResult != k_EGCResultOK)
		{
			continue;
		}

		check(MsgSize == NewMsgSize);

		uint32 ProtoMsgByteOffset;
		CClientMsgGCHeader MsgHdr;
		// Retrieve the message header and payload offsets
		if (SteamCoordinator.ParseMessage(MsgType, Buffer.GetData(), MsgSize, MsgHdr, &ProtoMsgByteOffset))
		{
			// Who this message is intended for (job that initiated request)
			uint64 JobId = MsgHdr.jobid_target();

			FOnlineAsyncMsgSteam* NewJob = NULL;
			{		
				FScopeLock ScopeLock(&InMsgQueueLock);
				FOnlineAsyncMsgSteam** Job = InMsgQueue.Find(JobId);

				if (Job && *Job)
				{
					NewJob = *Job;
					InMsgQueue.Remove(JobId);
				}
			}

			if (NewJob)
			{
				// Convert the message payload to a usable format and pass to game thread
				check(NewJob->ResponseMessage);
				check(NewJob->GetResponseMsgType() == MsgType);
				if (NewJob->GetResponse()->ParseFromArray(Buffer.GetData() + ProtoMsgByteOffset, MsgSize - ProtoMsgByteOffset))
				{
					NewJob->Deserialize();
				}

				// Always add to out queue for destruction on game thread
				AddToOutQueue(NewJob);
				if (!NewJob->bWasSuccessful)
				{
					UE_LOG_ONLINE(Warning, TEXT("Failed to parse message.  MessageType: %d JobId: %llu"), MsgType, JobId);
				}

				UE_LOG_ONLINE(Log, TEXT("Async msg '%s' completed in %f seconds with %d"), *NewJob->ToString(), NewJob->GetElapsedTime(), NewJob->bWasSuccessful);
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("Unsolicited message from server.  MessageType: %d JobId: %llu"), MsgType, JobId);
			}
		}
	}

	static float CheckInterval = 15.0f;
	static double LastCheckTime = 0.0;
	if (FPlatformTime::Seconds() - LastCheckTime > CheckInterval)
	{
		LastCheckTime = FPlatformTime::Seconds();
		FScopeLock ScopeLock(&InMsgQueueLock);
		for (TMap<int64, FOnlineAsyncMsgSteam*>::TConstIterator It(InMsgQueue); It; ++It)
		{
			uint64 JobId = It.Key();
			FOnlineAsyncMsgSteam* Message = It.Value();
			if (Message->GetElapsedTime() > CheckInterval)
			{
				UE_LOG_ONLINE(Warning, TEXT("Message id %ull type %d in queue for %f without a response."), JobId, Message->GetMsgType(), Message->GetElapsedTime());
			}
		}
	}
#endif
}

void FOnlineAsyncTaskManagerSteam::OnlineTick()
{
	check(SteamSubsystem);

	if (SteamSubsystem->IsSteamClientAvailable())
	{
		SteamAPI_RunCallbacks();
	}

	if (SteamSubsystem->IsSteamServerAvailable())
	{
		SteamGameServer_RunCallbacks();
	}

	MessagePump();
}

/**
 *	Event triggered by Steam backend when a user attempts JIP or accepts an invite request (via Steam client)
 *
 * @param CallbackData All the valid data from Steam related to this event 
 */
void FOnlineAsyncTaskManagerSteam::OnInviteAccepted(GameRichPresenceJoinRequested_t* CallbackData)
{
	FOnlineAsyncEventSteamInviteAccepted* NewEvent = 
		new FOnlineAsyncEventSteamInviteAccepted(SteamSubsystem, FUniqueNetIdSteam(CallbackData->m_steamIDFriend), UTF8_TO_TCHAR(CallbackData->m_rgchConnect));
	UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
	AddToOutQueue(NewEvent);
}

/**
 *	Event triggered by Steam backend when a user attempts JIP (via Steam client) or accepts an invite request (via Steam client)
 *
 * @param CallbackData All the valid data from Steam related to this event 
 */
void FOnlineAsyncTaskManagerSteam::OnLobbyInviteAccepted(GameLobbyJoinRequested_t* CallbackData)
{
	if (CallbackData->m_steamIDLobby.IsLobby())
	{
		FUniqueNetIdSteam LobbyId(CallbackData->m_steamIDLobby);

		FOnlineSessionSteamPtr Sessions = StaticCastSharedPtr<FOnlineSessionSteam>(SteamSubsystem->GetSessionInterface());
		if (Sessions.IsValid() && !Sessions->IsMemberOfLobby(LobbyId))
		{
			FOnlineAsyncEventSteamLobbyInviteAccepted* NewEvent = 
				new FOnlineAsyncEventSteamLobbyInviteAccepted(SteamSubsystem, FUniqueNetIdSteam(CallbackData->m_steamIDFriend), LobbyId);
			UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
			AddToOutQueue(NewEvent);
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("Attempting to accept invite to lobby user is already in, ignoring."));
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("OnLobbyInviteAccepted: Invalid LobbyId received."));
	}
}

/**
 * Notification event from Steam that the lobby state has changed (users joining/leaving)
 */
class FOnlineAsyncEventSteamLobbyEnter : public FOnlineAsyncEvent<FOnlineSubsystemSteam>
{
private:
	
	/** Lobby enter state information */
	LobbyEnter_t CallbackResults;

	/** Hidden on purpose */
	FOnlineAsyncEventSteamLobbyEnter() :
		FOnlineAsyncEvent(NULL)
	{
		FMemory::MemZero(CallbackResults);
	}

public:

	FOnlineAsyncEventSteamLobbyEnter(FOnlineSubsystemSteam* InSubsystem, const LobbyEnter_t& InResults) :
		FOnlineAsyncEvent(InSubsystem),
		CallbackResults(InResults)
	{
	}
	
	virtual ~FOnlineAsyncEventSteamLobbyEnter() 
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamLobbyEnter LobbyId: %s Result: %s"), 
			*FUniqueNetIdSteam(CallbackResults.m_ulSteamIDLobby).ToDebugString(),
			*SteamChatRoomEnterResponseString((EChatRoomEnterResponse)CallbackResults.m_EChatRoomEnterResponse));	
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineSessionSteamPtr Sessions = StaticCastSharedPtr<FOnlineSessionSteam>(Subsystem->GetSessionInterface());
		if (Sessions.IsValid())
		{
			FUniqueNetIdSteam LobbyId(CallbackResults.m_ulSteamIDLobby);
			FNamedOnlineSession* Session = Sessions->GetNamedSessionFromLobbyId(LobbyId);
			if (!Session)
			{
				UE_LOG_ONLINE(Warning, TEXT("Entered lobby %s, but not found in sessions list"), *LobbyId.ToDebugString());
			}
		}
	}
};

/**
 *	Event triggered by Steam backend when a user joins a lobby
 *
 * @param CallbackData All the valid data from Steam related to this event 
 */
void FOnlineAsyncTaskManagerSteam::OnLobbyEnter(LobbyEnter_t* CallbackData)
{
	// The owner of the created lobby shouldn't need this information
	if (SteamMatchmaking()->GetLobbyOwner(CallbackData->m_ulSteamIDLobby) != SteamUser()->GetSteamID())
	{
		FOnlineAsyncEventSteamLobbyEnter* NewEvent = new FOnlineAsyncEventSteamLobbyEnter(SteamSubsystem, *CallbackData);
		UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
		AddToOutQueue(NewEvent);
	}
}

/**
 * Notification event from Steam that the lobby state has changed (users joining/leaving)
 */
class FOnlineAsyncEventSteamLobbyChatUpdate : public FOnlineAsyncEvent<FOnlineSubsystemSteam>
{
private:
	
	/** Lobby chat state information */
	LobbyChatUpdate_t CallbackResults;

	/** Hidden on purpose */
	FOnlineAsyncEventSteamLobbyChatUpdate() :
		FOnlineAsyncEvent(NULL)
	{
		FMemory::MemZero(CallbackResults);
	}

public:

	FOnlineAsyncEventSteamLobbyChatUpdate(FOnlineSubsystemSteam* InSubsystem, const LobbyChatUpdate_t& InResults) :
		FOnlineAsyncEvent(InSubsystem),
		CallbackResults(InResults)
	{
	}
	
	virtual ~FOnlineAsyncEventSteamLobbyChatUpdate() 
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamLobbyChatUpdate User: %s Instigator: %s Result: %s"), 
			*FUniqueNetIdSteam(CallbackResults.m_ulSteamIDUserChanged).ToDebugString(),
			*FUniqueNetIdSteam(CallbackResults.m_ulSteamIDMakingChange).ToDebugString(),
			*SteamChatMemberStateChangeString((EChatMemberStateChange)CallbackResults.m_rgfChatMemberStateChange));	
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineSessionSteamPtr Sessions = StaticCastSharedPtr<FOnlineSessionSteam>(Subsystem->GetSessionInterface());
		if (Sessions.IsValid())
		{
			FUniqueNetIdSteam LobbyId(CallbackResults.m_ulSteamIDLobby);
			// Lobby data update for existing session
			FNamedOnlineSession* Session = Sessions->GetNamedSessionFromLobbyId(LobbyId);
			if (Session)
			{
				// Recreate the lobby member list
				if (!FillMembersFromLobbyData(LobbyId, *Session))
				{
					UE_LOG_ONLINE(Warning, TEXT("Failed to parse session %s member update %s"), *Session->SessionName.ToString(), *LobbyId.ToDebugString());
				}
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("Received lobby chat update %s, but not found in sessions list"), *LobbyId.ToDebugString());
			}
		}
	}
};

/**
 *	Event triggered by Steam backend when a user joins a lobby
 *
 * @param CallbackData All the valid data from Steam related to this event 
 */
void FOnlineAsyncTaskManagerSteam::OnLobbyChatUpdate(LobbyChatUpdate_t* CallbackData)
{
	FOnlineAsyncEventSteamLobbyChatUpdate* NewEvent = new FOnlineAsyncEventSteamLobbyChatUpdate(SteamSubsystem, *CallbackData);
	UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
	AddToOutQueue(NewEvent);
}

/**
 * Notification event from Steam when new lobby data is available for the given lobby 
 */
class FOnlineAsyncEventSteamLobbyUpdate : public FOnlineAsyncEvent<FOnlineSubsystemSteam> 
{
private:

	/** Id of lobby to update */
	FUniqueNetIdSteam LobbyId;
	/** Hidden on purpose */
	FOnlineAsyncEventSteamLobbyUpdate() 
	{}

public:
	FOnlineAsyncEventSteamLobbyUpdate(FOnlineSubsystemSteam* InSubsystem, const FUniqueNetIdSteam& InLobbyId) :
		FOnlineAsyncEvent(InSubsystem),
		LobbyId(InLobbyId)
	{
	}

	virtual ~FOnlineAsyncEventSteamLobbyUpdate() 
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamLobbyUpdate LobbyId: %s"), *LobbyId.ToDebugString());	
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() OVERRIDE
	{
		FOnlineAsyncEvent::Finalize();

		// Searching for lobbies case (NULL CurrentSessionSearch implies no active search query)
		FOnlineSessionSteamPtr Sessions = StaticCastSharedPtr<FOnlineSessionSteam>(Subsystem->GetSessionInterface());
		if (Sessions.IsValid() && Sessions->CurrentSessionSearch.IsValid() && Sessions->CurrentSessionSearch->SearchState == EOnlineAsyncTaskState::InProgress)
		{
			// Add this lobby as available for adding to search results
			Sessions->PendingSearchLobbyIds.AddUnique(LobbyId);
		}
		else
		{
			// Lobby data update for existing session
			FNamedOnlineSession* Session = Sessions->GetNamedSessionFromLobbyId(LobbyId);
			if (Session)
			{
				// Make sure the session has all the valid session data
				if (!FillSessionFromLobbyData(LobbyId, *Session) ||
					!FillMembersFromLobbyData(LobbyId, *Session))
				{
					UE_LOG_ONLINE(Warning, TEXT("Failed to parse session %s lobby update %s"), *Session->SessionName.ToString(), *LobbyId.ToDebugString());
				}
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("Received lobby update %s, but not found in sessions list"), *LobbyId.ToDebugString());
			}
		}
	}
};

/**
 *	Event triggered by Steam backend when new lobby data is available for the given lobby 
 * Can occur any time host calls SetLobbyData or while searching for lobbies (calls to RequestLobbyData)
 *
 * @param CallbackData All the valid data from Steam related to this event 
 */
void FOnlineAsyncTaskManagerSteam::OnLobbyDataUpdate(LobbyDataUpdate_t* CallbackData)
{
	// Equivalent lobby ids implies it is lobby data that has updated
	if (CallbackData->m_ulSteamIDLobby == CallbackData->m_ulSteamIDMember)
	{
		FUniqueNetIdSteam LobbyId(CallbackData->m_ulSteamIDLobby);
		if (!CallbackData->m_bSuccess)
		{
			// CallbackData->m_bSuccess indicates LobbyID has shut down since 
			// the result was returned but we have to keep the array size in sync
			UE_LOG_ONLINE(Verbose, TEXT("Lobby %s is no longer available."), *LobbyId.ToDebugString());
		}

		ISteamMatchmaking* SteamMatchmakingPtr = SteamMatchmaking();
		check(SteamMatchmakingPtr);

		// The owner of the created lobby shouldn't need this information
		if (SteamMatchmakingPtr->GetLobbyOwner(CallbackData->m_ulSteamIDLobby) != SteamUser()->GetSteamID())
		{
			FOnlineAsyncEventSteamLobbyUpdate* NewEvent = new FOnlineAsyncEventSteamLobbyUpdate(SteamSubsystem, LobbyId);
			UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
			AddToOutQueue(NewEvent);
		}
	}
	else
	{
		// @TODO ONLINE - Player data update
		//const TCHAR* Key = MemberKeyList(i);
		//const ANSICHAR* MemberValue = SteamMatchmakingPtr->GetLobbyMemberData(CallbackData->m_ulSteamIDLobby, CallbackData->m_ulSteamIDMember, TCHAR_TO_UTF8(Key));
	}

	// @TODO ONLINE - SetLobbyOwner triggers this call also
}

/**
 * Notification event from Steam that a given user's
 * stats/achievements data has been downloaded from the server
 */
class FOnlineAsyncEventSteamStatsReceived : public FOnlineAsyncEvent<FOnlineSubsystemSteam> 
{
private:

	/** User this data is for */
	const FUniqueNetIdSteam UserId;
	/** Result of the download */
	EResult StatsReceivedResult;

	/** Hidden on purpose */
	FOnlineAsyncEventSteamStatsReceived() :
		UserId(0),
		StatsReceivedResult(k_EResultOK)
	{}

public:
	FOnlineAsyncEventSteamStatsReceived(FOnlineSubsystemSteam* InSubsystem, const FUniqueNetIdSteam& InUserId, EResult InResult) :
		FOnlineAsyncEvent(InSubsystem),
		UserId(InUserId),
		StatsReceivedResult(InResult)
	{
	}

	virtual ~FOnlineAsyncEventSteamStatsReceived() 
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamStatsReceived bWasSuccessful: %d User: %s Result: %s"), (StatsReceivedResult == k_EResultOK) ? 1 : 0, *UserId.ToDebugString(), *SteamResultString(StatsReceivedResult));	
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() OVERRIDE
	{
		FOnlineAsyncEvent::Finalize();

		// Add given user's stats to the cached list
		FOnlineLeaderboardsSteamPtr Leaderboards = StaticCastSharedPtr<FOnlineLeaderboardsSteam>(Subsystem->GetLeaderboardsInterface());
		Leaderboards->SetUserStatsState(UserId, (StatsReceivedResult == k_EResultOK) ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed);
	}
};

/**
 *	Event triggered from Steam when the current user's stats have been downloaded from the backend
 *	Possible that the result fails if they have no data for the current game
 *
 * @param CallbackData All the valid data from Steam related to this event 
 */
void FOnlineAsyncTaskManagerSteam::OnUserStatsReceived(UserStatsReceived_t* CallbackData)
{
	const CGameID GameID(SteamSubsystem->GetSteamAppId());
	if (GameID.ToUint64() == CallbackData->m_nGameID)
	{
		FUniqueNetIdSteam UserId(CallbackData->m_steamIDUser);
		if (CallbackData->m_eResult != k_EResultOK)
		{
			if (CallbackData->m_eResult == k_EResultFail)
			{
				UE_LOG_ONLINE(Warning, TEXT("Failed to obtain steam user stats, user: %s has no stats entries"), *UserId.ToDebugString());
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("Failed to obtain steam user stats, user: %s error: %s"), *UserId.ToDebugString(), 
					*SteamResultString(CallbackData->m_eResult));
			}
		}

		FOnlineAsyncEventSteamStatsReceived* NewEvent = new FOnlineAsyncEventSteamStatsReceived(SteamSubsystem, UserId, CallbackData->m_eResult);
		UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
		AddToOutQueue(NewEvent);
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("Obtained steam user stats, but for wrong game! Ignoring."));
	}
}

/**
 * Notification event from Steam that the currently logged in user's
 * stats/achievements data has been stored with the server
 */
class FOnlineAsyncEventSteamStatsStored : public FOnlineAsyncEvent<FOnlineSubsystemSteam>
{
private:
	
	/** User this data is for */
	const FUniqueNetIdSteam UserId;
	/** Result of the download */
	EResult StatsStoredResult;

	/** Hidden on purpose */
	FOnlineAsyncEventSteamStatsStored() :
		FOnlineAsyncEvent(NULL),
		UserId(0),
		StatsStoredResult(k_EResultOK)
	{
	}

public:

	FOnlineAsyncEventSteamStatsStored(FOnlineSubsystemSteam* InSubsystem, const FUniqueNetIdSteam& InUserId, EResult InResult) :
		FOnlineAsyncEvent(InSubsystem),
		UserId(InUserId),
		StatsStoredResult(InResult)
	{
	}
	
	virtual ~FOnlineAsyncEventSteamStatsStored() 
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamStatsStored bWasSuccessful: %d User: %s Result: %s"), (StatsStoredResult == k_EResultOK) ? 1 : 0, *UserId.ToDebugString(), *SteamResultString(StatsStoredResult));	
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEvent::Finalize();
		FOnlineLeaderboardsSteamPtr Leaderboards = StaticCastSharedPtr<FOnlineLeaderboardsSteam>(Subsystem->GetLeaderboardsInterface());
		Leaderboards->SetUserStatsStoreState(UserId, StatsStoredResult == k_EResultOK ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed);
	}
};

/**
 *	Event triggered from Steam when the current user's stats have been stored with the backend
 *	Possible that the result fails with "Invalid Param" meaning the stats went out of range or were out of date
 *  New stats are downloaded in this case and need to be re-evaluated
 *
 * @param CallbackData All the valid data from Steam related to this event 
 */
void FOnlineAsyncTaskManagerSteam::OnUserStatsStored(UserStatsStored_t* CallbackData)
{
	const CGameID GameID(SteamSubsystem->GetSteamAppId());
	if (GameID.ToUint64() == CallbackData->m_nGameID)
	{
		// Only the current user comes through this way (other user's stats are stored via GameServerStats)
		FUniqueNetIdSteam UserId(SteamUser()->GetSteamID());
		if (CallbackData->m_eResult != k_EResultOK)
		{
			if (CallbackData->m_eResult == k_EResultInvalidParam)
			{
				UE_LOG_ONLINE(Warning, TEXT("Invalid stats data set, stats have been reverted to state prior to last write."));
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("Failed to store steam user stats, error: %s"), *SteamResultString(CallbackData->m_eResult));
			}
		}

		FOnlineAsyncEventSteamStatsStored* NewEvent = new FOnlineAsyncEventSteamStatsStored(SteamSubsystem, UserId, CallbackData->m_eResult);
		UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
		AddToOutQueue(NewEvent);
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("Stored steam user stats, but for wrong game! Ignoring."));
	}
}

/**
 * Notification event from Steam that the currently logged in user's
 * stats/achievements data has been stored with the server
 * FROM VALVE: Steam stats for other users are kept in an LRU with a max queue length of 100
 */
class FOnlineAsyncEventSteamStatsUnloaded : public FOnlineAsyncEvent<FOnlineSubsystemSteam>
{
private:
	/** User whose data has been unloaded */
	FUniqueNetIdSteam UserId;

	/** Hidden on purpose */
	FOnlineAsyncEventSteamStatsUnloaded() : 
		UserId(0)
	{
	}

public:

	FOnlineAsyncEventSteamStatsUnloaded(FOnlineSubsystemSteam* InSubsystem, const FUniqueNetIdSteam& InUserId) :
		FOnlineAsyncEvent(InSubsystem),
		UserId(InUserId)
	{
	}
	
	virtual ~FOnlineAsyncEventSteamStatsUnloaded() 
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamStatsUnloaded UserId: %s"), *UserId.ToDebugString());	
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() OVERRIDE
	{
		FOnlineAsyncEvent::Finalize();
		// Remove given user's stats from the cached list
		FOnlineLeaderboardsSteamPtr Leaderboards = StaticCastSharedPtr<FOnlineLeaderboardsSteam>(Subsystem->GetLeaderboardsInterface());
		Leaderboards->SetUserStatsState(UserId, EOnlineAsyncTaskState::NotStarted);
	}
};

/**
 *	Event triggered from Steam when a previously requested user's stats have been purged in LRU fashion
 * Requesting the data an additional time will bring the data back
 *
 * @param CallbackData All the valid data from Steam related to this event 
 */
void FOnlineAsyncTaskManagerSteam::OnUserStatsUnloaded(UserStatsUnloaded_t* CallbackData)
{
	FOnlineAsyncEventSteamStatsUnloaded* NewEvent = new FOnlineAsyncEventSteamStatsUnloaded(SteamSubsystem, FUniqueNetIdSteam(CallbackData->m_steamIDUser));
	UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
	AddToOutQueue(NewEvent);
}

/**
 *	Delegate registered with Steam to trigger when the Steam Overlay is activated
 *
 * @param CallbackData - Steam struct containing state of the Steam Overlay
 */
void FOnlineAsyncTaskManagerSteam::OnExternalUITriggered(GameOverlayActivated_t* CallbackData)
{
	FOnlineAsyncEventSteamExternalUITriggered* NewEvent = new FOnlineAsyncEventSteamExternalUITriggered(SteamSubsystem, (CallbackData->m_bActive != 0) ? true : false);
	UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
	AddToOutQueue(NewEvent);
}

/**
 * Notification event from Steam that server session connection has changed state 
 */
class FOnlineAsyncEventSteamServerConnectionState : public FOnlineAsyncEvent<FOnlineSubsystemSteam>
{
	/** Connection state change */
	const EOnlineServerConnectionStatus::Type ConnectionState;

	/** Hidden on purpose */
	FOnlineAsyncEventSteamServerConnectionState() :
		ConnectionState(EOnlineServerConnectionStatus::NotConnected)
	{
	}

public:

	FOnlineAsyncEventSteamServerConnectionState(FOnlineSubsystemSteam* InSubsystem, EOnlineServerConnectionStatus::Type InConnectionState) :
		FOnlineAsyncEvent(InSubsystem),
		ConnectionState(InConnectionState)
	{
	}
	
	virtual ~FOnlineAsyncEventSteamServerConnectionState() 
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServerConnectionState StateChange: %s"), EOnlineServerConnectionStatus::ToString(ConnectionState));	
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() OVERRIDE
	{
		Subsystem->TriggerOnConnectionStatusChangedDelegates(ConnectionState);
	}
};

/**
 *	Client API version of the connected to Steam callback (only called in case of a Steam backend disconnect and then reconnect)
 */
void FOnlineAsyncTaskManagerSteam::OnSteamServersConnected(SteamServersConnected_t* CallbackData)
{
	FOnlineAsyncEventSteamServerConnectionState* NewEvent = new FOnlineAsyncEventSteamServerConnectionState(SteamSubsystem, EOnlineServerConnectionStatus::Connected);
	UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
	AddToOutQueue(NewEvent);
}

/**
 *	Client API version of the disconnected to Steam callback 
 */
void FOnlineAsyncTaskManagerSteam::OnSteamServersDisconnected(SteamServersDisconnected_t* CallbackData)
{
	FOnlineAsyncEventSteamServerConnectionState* NewEvent = new FOnlineAsyncEventSteamServerConnectionState(SteamSubsystem, SteamConnectionResult(CallbackData->m_eResult));
	UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
	AddToOutQueue(NewEvent);
}

/**
 * Notification event from Steam that server session has connected with the master server
 */
class FOnlineAsyncEventSteamServerConnectedGS : public FOnlineAsyncEvent<FOnlineSubsystemSteam>
{
	/** Newly assigned server id */
	const FUniqueNetIdSteam ServerId;

	/** Hidden on purpose */
	FOnlineAsyncEventSteamServerConnectedGS() :
		ServerId(uint64(0))
	{
	}

public:

	FOnlineAsyncEventSteamServerConnectedGS(FOnlineSubsystemSteam* InSubsystem, const FUniqueNetIdSteam& InServerId) :
		FOnlineAsyncEvent(InSubsystem),
		ServerId(InServerId)
	{
	}
	
	virtual ~FOnlineAsyncEventSteamServerConnectedGS() 
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServerConnectedGS ServerId: %s"), *ServerId.ToDebugString());	
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() OVERRIDE
	{
		FOnlineSessionSteamPtr Sessions = StaticCastSharedPtr<FOnlineSessionSteam>(Subsystem->GetSessionInterface());
		if (Sessions.IsValid())
		{
			Sessions->bSteamworksGameServerConnected = true;
			Sessions->GameServerSteamId = MakeShareable(new FUniqueNetIdSteam(ServerId));

			FSocketSubsystemSteam* SocketSubsystem = (FSocketSubsystemSteam*)ISocketSubsystem::Get(STEAM_SUBSYSTEM);
			if (SocketSubsystem)
			{			
				SocketSubsystem->FixupSockets(*Sessions->GameServerSteamId);
			}
		}

		// log on is not finished until OnPolicyResponse() is called
	}
};

/**
 *	GameServer API version of connected to Steam backend callback, 
 *  initiated by SteamGameServers()->LogOnAnonymous()
 */
void FOnlineAsyncTaskManagerSteam::OnSteamServersConnectedGS(SteamServersConnected_t* CallbackData)
{
	FOnlineAsyncEventSteamServerConnectedGS* NewEvent = new FOnlineAsyncEventSteamServerConnectedGS(SteamSubsystem, FUniqueNetIdSteam(SteamGameServer()->GetSteamID()));
	UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
	AddToOutQueue(NewEvent);
}

/**
 * Notification event from Steam that server session has been disconnected with the master server
 */
class FOnlineAsyncEventSteamServerDisconnectedGS : public FOnlineAsyncEvent<FOnlineSubsystemSteam>
{
private:

	/** Callback data */
	SteamServersDisconnected_t CallbackResults;

	/** Hidden on purpose */
	FOnlineAsyncEventSteamServerDisconnectedGS()
	{
	}

public:

	FOnlineAsyncEventSteamServerDisconnectedGS(FOnlineSubsystemSteam* InSubsystem, SteamServersDisconnected_t& InResults) :
		FOnlineAsyncEvent(InSubsystem),
		CallbackResults(InResults)
	{
	}
	
	virtual ~FOnlineAsyncEventSteamServerDisconnectedGS() 
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServerDisconnectedGS Result: %s"), *SteamResultString(CallbackResults.m_eResult));	
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() OVERRIDE
	{
		bool bTriggerConnectionStatusUpdate = true;
		FOnlineSessionSteamPtr Sessions = StaticCastSharedPtr<FOnlineSessionSteam>(Subsystem->GetSessionInterface());
		if (Sessions.IsValid())
		{
			Sessions->bSteamworksGameServerConnected = false;
			Sessions->GameServerSteamId = NULL;

			// Don't trigger the delegates if a DestroySession() call was made
			FNamedOnlineSession* Session = Sessions->GetGameServerSession();
			if (Session && Session->SessionState == EOnlineSessionState::Destroying)
			{
				bTriggerConnectionStatusUpdate = false;
			}
		}
		
		if (bTriggerConnectionStatusUpdate)
		{
			EOnlineServerConnectionStatus::Type ConnectionState = SteamConnectionResult(CallbackResults.m_eResult);
			Subsystem->TriggerOnConnectionStatusChangedDelegates(ConnectionState);
		}
	}
};

/**
 *	GameServer API version of disconnected from Steam backend callback
 */
void FOnlineAsyncTaskManagerSteam::OnSteamServersDisconnectedGS(SteamServersDisconnected_t* CallbackData)
{
	FOnlineAsyncEventSteamServerDisconnectedGS* NewEvent = new FOnlineAsyncEventSteamServerDisconnectedGS(SteamSubsystem, *CallbackData);
	UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
	AddToOutQueue(NewEvent);
}

/**
 *	GameServer API version of disconnected from Steam backend callback
 */
void FOnlineAsyncTaskManagerSteam::OnSteamServersConnectFailureGS(SteamServerConnectFailure_t* CallbackData)
{
	UE_LOG_ONLINE(Warning, TEXT("Steam connection failure."));
}

/**
 * Notification event from Steam that server session has been secured on the backend
 */
class FOnlineAsyncEventSteamServerPolicyResponseGS : public FOnlineAsyncEvent<FOnlineSubsystemSteam>
{
private:

	/** Callback data */
	GSPolicyResponse_t CallbackResults;

	/** Hidden on purpose */
	FOnlineAsyncEventSteamServerPolicyResponseGS()
	{
	}

public:

	FOnlineAsyncEventSteamServerPolicyResponseGS(FOnlineSubsystemSteam* InSubsystem, GSPolicyResponse_t& InResults) :
		FOnlineAsyncEvent(InSubsystem),
		CallbackResults(InResults)
	{
	}
	
	virtual ~FOnlineAsyncEventSteamServerPolicyResponseGS() 
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServerPolicyResponseGS Secure: %d"), CallbackResults.m_bSecure);	
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() OVERRIDE
	{
		FOnlineSessionSteamPtr Sessions = StaticCastSharedPtr<FOnlineSessionSteam>(Subsystem->GetSessionInterface());
		if (Sessions.IsValid())
		{
			Sessions->bPolicyResponseReceived = true;
			if (!Sessions->bSteamworksGameServerConnected || !Sessions->GameServerSteamId->IsValid())
			{
				UE_LOG_ONLINE(Warning, TEXT("Unexpected GSPolicyResponse callback"));
			}
		}
	}
};

/**
 * Notification event from Steam that server session has been secured
 */
void FOnlineAsyncTaskManagerSteam::OnPolicyResponseGS(GSPolicyResponse_t* CallbackData)
{
	FOnlineAsyncEventSteamServerPolicyResponseGS* NewEvent = new FOnlineAsyncEventSteamServerPolicyResponseGS(SteamSubsystem, *CallbackData);
	UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
	AddToOutQueue(NewEvent);
}

/**
 * Notification event from Steam that a P2P connection has been requested from a remote user
 */
class FOnlineAsyncEventSteamConnectionRequest : public FOnlineAsyncEvent<FOnlineSubsystemSteam>
{
private:
	
	/** Proper networking interface that this session is communicating on */
	ISteamNetworking* SteamNetworkingPtr;
	/** Callback data */
	FUniqueNetIdSteam RemoteId;

	/** Hidden on purpose */
	FOnlineAsyncEventSteamConnectionRequest()
	{
	}

public:

	FOnlineAsyncEventSteamConnectionRequest(FOnlineSubsystemSteam* InSubsystem, ISteamNetworking* InSteamNetworkingPtr, const FUniqueNetIdSteam& InRemoteId) :
		FOnlineAsyncEvent(InSubsystem),
		SteamNetworkingPtr(InSteamNetworkingPtr),
		RemoteId(InRemoteId)
	{
	}
	
	virtual ~FOnlineAsyncEventSteamConnectionRequest() 
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamConnectionRequest RemoteId: %s"), *RemoteId.ToDebugString());	
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() OVERRIDE
	{
		FSocketSubsystemSteam* SocketSubsystem = (FSocketSubsystemSteam*)ISocketSubsystem::Get(STEAM_SUBSYSTEM);
		if (SocketSubsystem)
		{
			if (!SocketSubsystem->AcceptP2PConnection(SteamNetworkingPtr, RemoteId))
			{	
				UE_LOG_ONLINE(Log, TEXT("Rejected P2P connection request from %s"), *RemoteId.ToDebugString());
			}
		}
	}
};

/**
 * Notification event from Steam that a P2P connection has failed
 */
class FOnlineAsyncEventSteamConnectionFailed : public FOnlineAsyncEvent<FOnlineSubsystemSteam>
{
private:

	/** Callback data */
	FUniqueNetIdSteam RemoteId;
	/** Error reason */
	EP2PSessionError ErrorCode;

	/** Hidden on purpose */
	FOnlineAsyncEventSteamConnectionFailed()
	{
	}

public:

	FOnlineAsyncEventSteamConnectionFailed(FOnlineSubsystemSteam* InSubsystem, const FUniqueNetIdSteam& InRemoteId, EP2PSessionError InErrorCode) :
		FOnlineAsyncEvent(InSubsystem),
		RemoteId(InRemoteId),
		ErrorCode(InErrorCode)
	{
	}
	
	virtual ~FOnlineAsyncEventSteamConnectionFailed() 
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamConnectionFailed RemoteId: %s Reason: %s"), *RemoteId.ToDebugString(), *SteamP2PConnectError(ErrorCode));	
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() OVERRIDE
	{
		// Mark the relevant sockets with this failure so they can properly notify higher level engine code
		FSocketSubsystemSteam* SocketSubsystem = (FSocketSubsystemSteam*)ISocketSubsystem::Get(STEAM_SUBSYSTEM);
		if (SocketSubsystem)
		{
			SocketSubsystem->ConnectFailure(RemoteId);
		}
	}
};

/**
 * Notification event from Steam that a P2P connection request has been initiated from a remote connection
 *
 * @param CallbackData information about remote connection request
 */
void FOnlineAsyncTaskManagerSteam::OnP2PSessionRequest(P2PSessionRequest_t* CallbackData)
{
	FUniqueNetIdSteam RemoteId(CallbackData->m_steamIDRemote);
	UE_LOG_ONLINE(Verbose, TEXT("Client connection request Id: %s"), *RemoteId.ToDebugString());

	IOnlineSessionPtr SessionInt = SteamSubsystem->GetSessionInterface();
	// Only accept connections if we have any expectation of being online
	if (SessionInt.IsValid() && SessionInt->GetNumSessions() > 0)
	{
		FOnlineAsyncEventSteamConnectionRequest* NewEvent = new FOnlineAsyncEventSteamConnectionRequest(SteamSubsystem, SteamNetworking(), RemoteId);
		UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
		AddToOutQueue(NewEvent);
	}
}

/**
 * Notification event from Steam that a P2P remote connection has failed
 * 
 * @param CallbackData information about remote connection failure
 */
void FOnlineAsyncTaskManagerSteam::OnP2PSessionConnectFail(P2PSessionConnectFail_t* CallbackData)
{
	FUniqueNetIdSteam RemoteId(CallbackData->m_steamIDRemote);
	FOnlineAsyncEventSteamConnectionFailed* NewEvent = new FOnlineAsyncEventSteamConnectionFailed(SteamSubsystem, RemoteId, (EP2PSessionError)CallbackData->m_eP2PSessionError);
	UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
	AddToOutQueue(NewEvent);
}

/**
 * Notification event from Steam that a P2P connection request has been initiated from a remote connection
 * (GameServer version)
 * 
 * @param CallbackData information about remote connection request
 */
void FOnlineAsyncTaskManagerSteam::OnP2PSessionRequestGS(P2PSessionRequest_t* CallbackData)
{
	FUniqueNetIdSteam RemoteId(CallbackData->m_steamIDRemote);
	FOnlineAsyncEventSteamConnectionRequest* NewEvent = new FOnlineAsyncEventSteamConnectionRequest(SteamSubsystem, SteamGameServerNetworking(), RemoteId);
	UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
	AddToOutQueue(NewEvent);
}

/**
 * Notification event from Steam that a P2P remote connection has failed
 * (GameServer version)
 * 
 * @param CallbackData information about remote connection failure
 */
void FOnlineAsyncTaskManagerSteam::OnP2PSessionConnectFailGS(P2PSessionConnectFail_t* CallbackData)
{
	FUniqueNetIdSteam RemoteId(CallbackData->m_steamIDRemote);
	FOnlineAsyncEventSteamConnectionFailed* NewEvent = new FOnlineAsyncEventSteamConnectionFailed(SteamSubsystem, RemoteId, (EP2PSessionError)CallbackData->m_eP2PSessionError);
	UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
	AddToOutQueue(NewEvent);
}

/**
 * Notification event from Steam that a P2P connection has failed
 */
class FOnlineAsyncEventSteamShutdown : public FOnlineAsyncEvent<FOnlineSubsystemSteam>
{
	FOnlineAsyncEventSteamShutdown() :
		FOnlineAsyncEvent(NULL)
	{
	}

public:

	FOnlineAsyncEventSteamShutdown(FOnlineSubsystemSteam* InSubsystem) :
		FOnlineAsyncEvent(InSubsystem)
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const OVERRIDE
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamShutdown shutdown received."));	
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() OVERRIDE
	{
		FPlatformMisc::RequestExit(false);
	}
};

/**
 *	Delegate registered with Steam to trigger when Steam is shutting down
 *
 * @param CallbackData - Steam struct containing shutdown information
 */
void FOnlineAsyncTaskManagerSteam::OnSteamShutdown(SteamShutdown_t* CallbackData)
{
	FOnlineAsyncEventSteamShutdown* NewEvent = new FOnlineAsyncEventSteamShutdown(SteamSubsystem);
	UE_LOG_ONLINE(Verbose, TEXT("%s"), *NewEvent->ToString());
	AddToOutQueue(NewEvent);
}

void FOnlineAsyncTaskManagerSteam::AddToInMsgQueue(FOnlineAsyncMsgSteam* NewMsg)
{
#if WITH_STEAMGC
	// assert if not game thread
	check(FPlatformTLS::GetCurrentThreadId() == FOnlineAsyncItem::GameThreadId);

	int64 LocalJobId = -1;
	{
		FScopeLock Lock(&InMsgQueueLock);
		if (NewMsg->GetResponseMsgType() > 0)
		{
			static int64 JobId = 0;
			LocalJobId = ++JobId;
			InMsgQueue.Add(LocalJobId, NewMsg);
		}	
	}

	if (LocalJobId != -1)
	{
		SteamCoordinator.BSendMessageExpectResponse(NewMsg->GetMsgType(), LocalJobId, *NewMsg->GetParams());
	}
	else
	{
		SteamCoordinator.BSendMessageNoResponse(NewMsg->GetMsgType(), *NewMsg->GetParams());
	}

	WorkEvent->Trigger();
#endif
}
