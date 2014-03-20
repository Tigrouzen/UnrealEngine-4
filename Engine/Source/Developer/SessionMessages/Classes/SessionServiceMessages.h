// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SessionServiceMessages.h: Declares message types used by session services.
=============================================================================*/

#pragma once

#include "SessionServiceMessages.generated.h"


/* Session discovery messages
 *****************************************************************************/

/**
 * Implements a message that is published to discover existing application sessions.
 */
USTRUCT()
struct FSessionServicePing
{
	GENERATED_USTRUCT_BODY()

};

template<>
struct TStructOpsTypeTraits<FSessionServicePing> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 * Implements a message that is published in response to FSessionServicePing.
 */
USTRUCT()
struct FSessionServicePong
{
	GENERATED_USTRUCT_BODY()

	/**
	 * Holds the application's build date.
	 */
	UPROPERTY()
	FString BuildDate;

	/**
	 * Holds the name of the device that the application is running on.
	 */
	UPROPERTY()
	FString DeviceName;

	/**
	 * Holds the application's instance identifier.
	 */
	UPROPERTY()
	FGuid InstanceId;

	/**
	 * Holds the application's instance name.
	 */
	UPROPERTY()
	FString InstanceName;

	/**
	 * Holds a flag indicating whether the application is running on a console.
	 */
	UPROPERTY()
	bool IsConsoleBuild;

	/**
	 * Holds the name of the platform that the application is running on.
	 */
	UPROPERTY()
	FString PlatformName;

	/**
	 * Holds the identifier of the session that the application belongs to.
	 */
	UPROPERTY()
	FGuid SessionId;

	/**
	 * Holds the user defined name of the session.
	 */
	UPROPERTY()
	FString SessionName;

	/**
	 * Holds the name of the user that started the session.
	 */
	UPROPERTY()
	FString SessionOwner;

	/**
	 * Holds a flag indicating whether the application is the only one in that session.
	 */
	UPROPERTY()
	bool Standalone;
};

template<>
struct TStructOpsTypeTraits<FSessionServicePong> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/* Session status messages
 *****************************************************************************/

/**
 * Implements a message that contains a console log entry.
 */
USTRUCT()
struct FSessionServiceLog
{
	GENERATED_USTRUCT_BODY()

	/**
	 * Holds the log message category.
	 */
	UPROPERTY()
	FName Category;

	/**
	 * Holds the log message data.
	 */
	UPROPERTY()
	FString Data;

	/**
	 * Holds the application instance indentifier.
	 */
	UPROPERTY()
	FGuid InstanceId;

	/**
	 * Holds the time in seconds since the application was started.
	 */
	UPROPERTY()
	double TimeSeconds;

	/**
	 * Holds the log message's verbosity level.
	 */
	UPROPERTY()
	uint8 Verbosity;


	/**
	 * Default constructor.
	 */
	FSessionServiceLog( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FSessionServiceLog( const FName& InCategory, const FString& InData, const FGuid& InInstanceId, double InTimeSeconds, uint8 InVerbosity )
		: Category(InCategory)
		, Data(InData)
		, InstanceId(InInstanceId)
		, TimeSeconds(InTimeSeconds)
		, Verbosity(InVerbosity)
	{ }
};

template<>
struct TStructOpsTypeTraits<FSessionServiceLog> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 * Implements a message to subscribe to an application's console log.
 */
USTRUCT()
struct FSessionServiceLogSubscribe
{
	GENERATED_USTRUCT_BODY()
};

template<>
struct TStructOpsTypeTraits<FSessionServiceLogSubscribe> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 * Implements a message to unsubscribe from an application's console log.
 */
USTRUCT()
struct FSessionServiceLogUnsubscribe
{
	GENERATED_USTRUCT_BODY()
};

template<>
struct TStructOpsTypeTraits<FSessionServiceLogUnsubscribe> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/* Dummy class
 *****************************************************************************/

UCLASS()
class USessionServiceMessages
	: public UObject
{
public:

	GENERATED_UCLASS_BODY()
};
