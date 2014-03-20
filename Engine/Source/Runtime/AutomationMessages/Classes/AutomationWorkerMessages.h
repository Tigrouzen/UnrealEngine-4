// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AutomationWorkerMessages.h: Declares message types used by automation workers.
=============================================================================*/

#pragma once

#include "AutomationWorkerMessages.generated.h"


/* Worker discovery messages
 *****************************************************************************/

/**
 * Implements a message that is published to find automation workers.
 */
USTRUCT()
struct FAutomationWorkerFindWorkers
{
	GENERATED_USTRUCT_BODY()

	/**
	 * Holds the change list number to find workers for.
	 */
	UPROPERTY()
	int32 Changelist;

	/**
	 * The name of the game.
	 */
	UPROPERTY()
	FString GameName;

	/**
	 * The name of the process.
	 */
	UPROPERTY()
	FString ProcessName;

	/**
	 * Holds the session identifier to find workers for.
	 */
	UPROPERTY()
	FGuid SessionId;


	/**
	 * Default constructor.
	 */
	FAutomationWorkerFindWorkers( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FAutomationWorkerFindWorkers( int32 InChangelist, const FString& InGameName, const FString& InProcessName, const FGuid& InSessionId )
		: Changelist(InChangelist)
		, GameName(InGameName)
		, ProcessName(InProcessName)
		, SessionId(InSessionId)
	{ }
};

template<>
struct TStructOpsTypeTraits<FAutomationWorkerFindWorkers> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 * Implements a message that is sent in response to FAutomationWorkerFindWorkers.
 */
USTRUCT()
struct FAutomationWorkerFindWorkersResponse
{
	GENERATED_USTRUCT_BODY()

	/**
	 * Holds the name of the device that the worker is running on.
	 */
	UPROPERTY()
	FString DeviceName;

	/**
	 * Holds the name of the worker's application instance.
	 */
	UPROPERTY()
	FString InstanceName;

	/**
	 * Holds the name of the platform that the worker is running on.
	 */
	UPROPERTY()
	FString Platform;

	/**
	 * Holds the worker's application session identifier.
	 */
	UPROPERTY()
	FGuid SessionId;


	/**
	 * Default constructor.
	 */
	FAutomationWorkerFindWorkersResponse( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FAutomationWorkerFindWorkersResponse( const FString& InDeviceName, const FString& InInstanceName, const FString& InPlatform, const FGuid& InSessionId )
		: DeviceName(InDeviceName)
		, InstanceName(InInstanceName)
		, Platform(InPlatform)
		, SessionId(InSessionId)
	{ }
};

template<>
struct TStructOpsTypeTraits<FAutomationWorkerFindWorkersResponse> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 * Implements a message that notifies automation controllers that a worker went off-line.
 */
USTRUCT()
struct FAutomationWorkerWorkerOffline
{
	GENERATED_USTRUCT_BODY()
};

template<>
struct TStructOpsTypeTraits<FAutomationWorkerWorkerOffline> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 */
USTRUCT()
struct FAutomationWorkerPing
{
	GENERATED_USTRUCT_BODY()
};

template<>
struct TStructOpsTypeTraits<FAutomationWorkerPing> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 */
USTRUCT()
struct FAutomationWorkerResetTests
{
	GENERATED_USTRUCT_BODY()
};

template<>
struct TStructOpsTypeTraits<FAutomationWorkerResetTests> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 */
USTRUCT()
struct FAutomationWorkerPong
{
	GENERATED_USTRUCT_BODY()
};

template<>
struct TStructOpsTypeTraits<FAutomationWorkerPong> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 * Implements a message for requesting available automation tests from a worker.
 */
USTRUCT()
struct FAutomationWorkerRequestTests
{
	GENERATED_USTRUCT_BODY()

	/**
	 * Holds a flag indicating whether the developer directory should be included.
	 */
	UPROPERTY()
	bool DeveloperDirectoryIncluded;

	/**
	 * Holds a flag indicating whether the visual commandlet filter is enabled.
	 */
	UPROPERTY()
	bool VisualCommandletFilterOn;


	/**
	 * Default constructor.
	 */
	FAutomationWorkerRequestTests( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FAutomationWorkerRequestTests( bool InDeveloperDirectoryIncluded, bool InVisualCommandletFilterOn )
		: DeveloperDirectoryIncluded(InDeveloperDirectoryIncluded)
		, VisualCommandletFilterOn(InVisualCommandletFilterOn)
	{ }
};

template<>
struct TStructOpsTypeTraits<FAutomationWorkerRequestTests> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 * Implements a message that is sent in response to FAutomationWorkerRequestTests.
 */
USTRUCT()
struct FAutomationWorkerRequestTestsReply
{
	GENERATED_USTRUCT_BODY()

	/**
	 * Holds the test information serialized into a string.
	 */
	UPROPERTY()
	FString TestInfo;

	/**
	 * Holds the total number of tests returned.
	 */
	UPROPERTY()
	int32 TotalNumTests;


	/**
	 * Default constructor.
	 */
	FAutomationWorkerRequestTestsReply( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FAutomationWorkerRequestTestsReply( const FString& InTestInfo, const int32& InTotalNumTests )
		: TestInfo(InTestInfo)
		, TotalNumTests(InTotalNumTests)
	{ }
};

template<>
struct TStructOpsTypeTraits<FAutomationWorkerRequestTestsReply> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 * Implements a message to request the running of automation tests on a worker.
 */
USTRUCT()
struct FAutomationWorkerRunTests
{
	GENERATED_USTRUCT_BODY()

	/**
	 */
	UPROPERTY()
	uint32 ExecutionCount;

	/**
	 */
	UPROPERTY()
	int32 RoleIndex;

	/**
	 * Holds the name of the test to run.
	 */
	UPROPERTY()
	FString TestName;


	/**
	 * Default constructor.
	 */
	FAutomationWorkerRunTests( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FAutomationWorkerRunTests( uint32 InExecutionCount, int32 InRoleIndex, FString InTestName )
		: ExecutionCount(InExecutionCount)
		, RoleIndex(InRoleIndex)
		, TestName(InTestName)
	{ }
};

template<>
struct TStructOpsTypeTraits<FAutomationWorkerRunTests> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 * Implements a message that is sent in response to FAutomationWorkerRunTests.
 */
USTRUCT()
struct FAutomationWorkerRunTestsReply
{
	GENERATED_USTRUCT_BODY()

	/**
	 */
	UPROPERTY()
	float Duration;

	/**
	 */
	UPROPERTY()
	TArray<FString> Errors;

	/**
	 */
	UPROPERTY()
	uint32 ExecutionCount;

	/**
	 */
	UPROPERTY()
	TArray<FString> Logs;

	/**
	 */
	UPROPERTY()
	bool Success;

	/**
	 */
	UPROPERTY()
	FString TestName;

	/**
	 */
	UPROPERTY()
	TArray<FString> Warnings;
};

template<>
struct TStructOpsTypeTraits<FAutomationWorkerRunTestsReply> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 */
USTRUCT()
struct FAutomationWorkerRequestNextNetworkCommand
{
	GENERATED_USTRUCT_BODY()

	/**
	 */
	UPROPERTY()
	uint32 ExecutionCount;


	/**
	 * Default constructor.
	 */
	FAutomationWorkerRequestNextNetworkCommand( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FAutomationWorkerRequestNextNetworkCommand( uint32 InExecutionCount )
		: ExecutionCount(InExecutionCount)
	{ }
};

template<>
struct TStructOpsTypeTraits<FAutomationWorkerRequestNextNetworkCommand> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};


/**
 */
USTRUCT()
struct FAutomationWorkerNextNetworkCommandReply
{
	GENERATED_USTRUCT_BODY()
};

template<>
struct TStructOpsTypeTraits<FAutomationWorkerNextNetworkCommandReply> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};

/**
 * Implements a message that is sent in containing a screen shot run during performance test.
 */
USTRUCT()
struct FAutomationWorkerScreenImage
{
	GENERATED_USTRUCT_BODY()

	// The screen shot data
	UPROPERTY()
	TArray<uint8> ScreenImage;

	// The screen shot name
	UPROPERTY()
	FString ScreenShotName;
};

template<>
struct TStructOpsTypeTraits<FAutomationWorkerScreenImage> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};

/* Dummy class
 *****************************************************************************/

UCLASS()
class UAutomationWorkerMessages
	: public UObject
{
public:

	GENERATED_UCLASS_BODY()
};
