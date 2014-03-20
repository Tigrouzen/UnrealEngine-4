// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "Ticker.h"
#include "ModuleManager.h"
#include "OnlineSubsystemModule.h"
#include "OnlineJsonSerializer.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineDelegateMacros.h"

ONLINESUBSYSTEM_API DECLARE_LOG_CATEGORY_EXTERN(LogOnline, Display, All);
ONLINESUBSYSTEM_API DECLARE_LOG_CATEGORY_EXTERN(LogOnlineGame, Display, All);

/** Online subsystem stats */
DECLARE_STATS_GROUP(TEXT("Online"), STATGROUP_Online);
/** Total async thread time */
DECLARE_CYCLE_STAT_EXTERN(TEXT("OnlineAsync"), STAT_Online_Async, STATGROUP_Online, ONLINESUBSYSTEM_API);
/** Number of async tasks in queue */
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("NumTasks"), STAT_Online_AsyncTasks, STATGROUP_Online, ONLINESUBSYSTEM_API);
/** Total time to process session interface */
DECLARE_CYCLE_STAT_EXTERN(TEXT("SessionInt"), STAT_Session_Interface, STATGROUP_Online, ONLINESUBSYSTEM_API);
/** Total time to process both local/remote voice */
DECLARE_CYCLE_STAT_EXTERN(TEXT("VoiceInt"), STAT_Voice_Interface, STATGROUP_Online, ONLINESUBSYSTEM_API);

#define ONLINE_LOG_PREFIX TEXT("OSS: ")
#define UE_LOG_ONLINE(Verbosity, Format, ...) \
{ \
	UE_LOG(LogOnline, Verbosity, TEXT("%s%s"), ONLINE_LOG_PREFIX, *FString::Printf(Format, ##__VA_ARGS__)); \
}

/** Forward declarations of all interface classes */
typedef TSharedPtr<class IOnlineSession, ESPMode::ThreadSafe> IOnlineSessionPtr;
typedef TSharedPtr<class IOnlineFriends, ESPMode::ThreadSafe> IOnlineFriendsPtr;
typedef TSharedPtr<class IOnlineSharedCloud, ESPMode::ThreadSafe> IOnlineSharedCloudPtr;
typedef TSharedPtr<class IOnlineUserCloud, ESPMode::ThreadSafe> IOnlineUserCloudPtr;
typedef TSharedPtr<class IOnlineEntitlements, ESPMode::ThreadSafe> IOnlineEntitlementsPtr;
typedef TSharedPtr<class IOnlineLeaderboards, ESPMode::ThreadSafe> IOnlineLeaderboardsPtr;
typedef TSharedPtr<class IOnlineVoice, ESPMode::ThreadSafe> IOnlineVoicePtr;
typedef TSharedPtr<class IOnlineExternalUI, ESPMode::ThreadSafe> IOnlineExternalUIPtr;
typedef TSharedPtr<class IOnlineTime, ESPMode::ThreadSafe> IOnlineTimePtr;
typedef TSharedPtr<class IOnlineIdentity, ESPMode::ThreadSafe> IOnlineIdentityPtr;
typedef TSharedPtr<class IOnlineTitleFile, ESPMode::ThreadSafe> IOnlineTitleFilePtr;
typedef TSharedPtr<class IOnlineStore, ESPMode::ThreadSafe> IOnlineStorePtr;
typedef TSharedPtr<class IOnlineEvents, ESPMode::ThreadSafe> IOnlineEventsPtr;
typedef TSharedPtr<class IOnlineAchievements, ESPMode::ThreadSafe> IOnlineAchievementsPtr;
typedef TSharedPtr<class IOnlineSharing, ESPMode::ThreadSafe> IOnlineSharingPtr;
typedef TSharedPtr<class IOnlineUser, ESPMode::ThreadSafe> IOnlineUserPtr;
typedef TSharedPtr<class IOnlineMessage, ESPMode::ThreadSafe> IOnlineMessagePtr;
typedef TSharedPtr<class IOnlinePresence, ESPMode::ThreadSafe> IOnlinePresencePtr;

/**
 * Called when the connection state as reported by the online platform changes
 *
 * @param ConnectionState state of the connection
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnConnectionStatusChanged, EOnlineServerConnectionStatus::Type);
typedef FOnConnectionStatusChanged::FDelegate FOnConnectionStatusChangedDelegate;


/**
 *	OnlineSubsystem - Series of interfaces to support communicating with various web/platform layer services
 */
class ONLINESUBSYSTEM_API IOnlineSubsystem
{
protected:
	/** Hidden on purpose */
	IOnlineSubsystem() {}

public:
	
	virtual ~IOnlineSubsystem() {}

	/** 
	 * Get the online subsystem for a given service
	 * @param SubsystemName - Name of the requested online service
	 * @return pointer to the appropriate online subsystem
	 */
	static IOnlineSubsystem* Get(const FName& SubsystemName = NAME_None)
	{
		static const FName OnlineSubsystemModuleName = TEXT("OnlineSubsystem");
		FOnlineSubsystemModule& OSSModule = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>(OnlineSubsystemModuleName); 
		return OSSModule.GetOnlineSubsystem(SubsystemName); 
	}

	/** 
	 * Determine if the subsystem for a given interface is already loaded
	 * @param SubsystemName - Name of the requested online service
	 * @return true if module for the subsystem is loaded
	 */
	static bool IsLoaded(const FName& SubsystemName = NAME_None)
	{
		if (FModuleManager::Get().IsModuleLoaded("OnlineSubsystem"))
		{
			FOnlineSubsystemModule& OSSModule = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem"); 
			return OSSModule.IsOnlineSubsystemLoaded(SubsystemName); 
		}
		return false;
	}

	/** 
	 * Get the interface for accessing the session management services
	 * @return Interface pointer for the appropriate session service
	 */
	virtual IOnlineSessionPtr GetSessionInterface() const = 0;
	
	/** 
	 * Get the interface for accessing the player friends services
	 * @return Interface pointer for the appropriate friend service
	 */
	virtual IOnlineFriendsPtr GetFriendsInterface() const = 0;

	/** 
	 * Get the interface for sharing user files in the cloud
	 * @return Interface pointer for the appropriate cloud service
	 */
	virtual IOnlineSharedCloudPtr GetSharedCloudInterface() const = 0;

	/** 
	 * Get the interface for accessing user files in the cloud
	 * @return Interface pointer for the appropriate cloud service
	 */
	virtual IOnlineUserCloudPtr GetUserCloudInterface() const = 0;

	/** 
	 * Get the interface for accessing user entitlements
	 * @return Interface pointer for the appropriate entitlements service
	 */
	virtual IOnlineEntitlementsPtr GetEntitlementsInterface() const = 0;

	/** 
	 * Get the interface for accessing leaderboards/rankings of a service
	 * @return Interface pointer for the appropriate leaderboard service
	 */
	virtual IOnlineLeaderboardsPtr GetLeaderboardsInterface() const = 0;

	/** 
	 * Get the interface for accessing voice related data
	 * @return Interface pointer for the appropriate voice service
	 */
	virtual IOnlineVoicePtr GetVoiceInterface() const = 0;

	/** 
	 * Get the interface for accessing the external UIs of a service
	 * @return Interface pointer for the appropriate external UI service
	 */
	virtual IOnlineExternalUIPtr GetExternalUIInterface() const = 0;

	/** 
	 * Get the interface for accessing the server time from an online service
	 * @return Interface pointer for the appropriate server time service
	 */
	virtual IOnlineTimePtr GetTimeInterface() const = 0;

	/** 
	 * Get the interface for accessing identity online services
	 * @return Interface pointer for the appropriate identity service
	 */
	virtual IOnlineIdentityPtr GetIdentityInterface() const = 0;

	/** 
	 * Get the interface for accessing title file online services
	 * @return Interface pointer for the appropriate title file service
	 */
	virtual IOnlineTitleFilePtr GetTitleFileInterface() const = 0;

	/** 
	 * Get the interface for accessing an online store
	 * @return Interface pointer for the appropriate online store service
	 */
	virtual IOnlineStorePtr GetStoreInterface() const = 0;
	
	/** 
	 * Get the interface for accessing online achievements
	 * @return Interface pointer for the appropriate online achievements service
	 */
	virtual IOnlineEventsPtr GetEventsInterface() const = 0;

	/** 
	 * Get the interface for accessing online achievements
	 * @return Interface pointer for the appropriate online achievements service
	 */
	virtual IOnlineAchievementsPtr GetAchievementsInterface() const = 0;
	
	/** 
	 * Get the interface for accessing online sharing
	 * @return Interface pointer for the appropriate online sharing service
	 */
	virtual IOnlineSharingPtr GetSharingInterface() const = 0;
	
	/** 
	 * Get the interface for accessing online user information
	 * @return Interface pointer for the appropriate online user service
	 */
	virtual IOnlineUserPtr GetUserInterface() const = 0;

	/** 
	 * Get the interface for accessing online messages
	 * @return Interface pointer for the appropriate online user service
	 */
	virtual IOnlineMessagePtr GetMessageInterface() const = 0;

	/** 
	 * Get the interface for managing rich presence information
	 * @return Interface pointer for the appropriate online user service
	 */
	virtual IOnlinePresencePtr GetPresenceInterface() const = 0;

	/** 
	 * Initialize the underlying subsystem APIs
	 * @return true if the subsystem was successfully initialized, false otherwise
	 */
	virtual bool Init() = 0;

	/** 
	 * Shutdown the underlying subsystem APIs
	 * @return true if the subsystem shutdown successfully, false otherwise
	 */
	virtual bool Shutdown() = 0;

	/**
	 * Each online subsystem has a global id for the app
	 *
	 * @return the app id for this app
	 */
	virtual FString GetAppId() const = 0;

	/**
	 * Exec handler that allows the online subsystem to process exec commands
	 *
	 * @param Cmd the exec command being executed
	 * @param Ar the archive to log results to
	 *
	 * @return true if the handler consumed the input, false to continue searching handlers
	 */
	virtual bool Exec(const TCHAR* Cmd, FOutputDevice& Ar) = 0;

	/**
	 * Called when the connection state as reported by the online platform changes
	 *
	 * @param ConnectionState state of the connection
	 */
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnConnectionStatusChanged, EOnlineServerConnectionStatus::Type);
};

/** Public references to the online subsystem pointer should use this */
typedef TSharedPtr<IOnlineSubsystem, ESPMode::ThreadSafe> IOnlineSubsystemPtr;

/**
 * Generates a unique number based off of the current engine package
 *
 * @return the unique number from the current engine package
 */
ONLINESUBSYSTEM_API uint32 GetBuildUniqueId();

/**
 * @return true if this is the server, false otherwise
 */
inline bool IsServer() { return IsServerForOnlineSubsystems(); }

/**
 * Common implementation for finding a player in a session
 *
 * @param SessionInt Session interface to use
 * @param SessionName Session name to check for player
 * @param UniqueId UniqueId of the player
 *
 * @return true if unique id found in session, false otherwise
 */
ONLINESUBSYSTEM_API bool IsPlayerInSessionImpl(IOnlineSession* SessionInt, FName SessionName, const FUniqueNetId& UniqueId);

