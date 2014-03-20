// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FileServerMessages.h: Declares message types used by file server commandlets.
=============================================================================*/

#pragma once

#include "FileServerMessages.generated.h"


/**
 * Implements a message that is published by file servers when they're ready to accept connections.
 */
USTRUCT()
struct FFileServerReady
{
	GENERATED_USTRUCT_BODY()
	
	/**
	 * Holds the list of IP addresses that the file server is listening on.
	 */
	UPROPERTY()
	TArray<FString> AddressList;
	
	/**
	 * Holds the file server's application identifier.
	 */
	UPROPERTY()
	FGuid InstanceId;


	/**
	 * Default constructor.
	 */
	FFileServerReady( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FFileServerReady( const TArray<FString>& InAddressList, const FGuid& InInstanceId )
		: AddressList(InAddressList)
		, InstanceId(InInstanceId)
	{ }
};


template<>
struct TStructOpsTypeTraits<FFileServerReady> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithMessageHandling = true
	};
};
