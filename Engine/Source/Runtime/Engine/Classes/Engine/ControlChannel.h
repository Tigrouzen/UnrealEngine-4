// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ControlChannel.generated.h"

//
// A channel for exchanging connection control messages
//

UCLASS(transient, customConstructor)
class UControlChannel : public UChannel
{
	GENERATED_UCLASS_BODY()

	/**
	 * Used to interrogate the first packet received to determine endianess
	 * of the sending client
	 */
	bool bNeedsEndianInspection;

	/** 
	 * provides an extra buffer beyond RELIABLE_BUFFER for control channel messages
	 * as we must be able to guarantee delivery for them
	 * because they include package map updates and other info critical to client/server synchronization
	 */
	TArray< TArray<uint8> > QueuedMessages;

	/** maximum size of additional buffer
	 * if this is exceeded as well, we kill the connection.  @TODO FIXME temporarily huge until we figure out how to handle 1 asset/package implication on packagemap
	 */
	enum { MAX_QUEUED_CONTROL_MESSAGES = 32768 };

	/**
	 * Inspects the packet for endianess information. Validates this information
	 * against what the client sent. If anything seems wrong, the connection is
	 * closed
	 *
	 * @param Bunch the packet to inspect
	 *
	 * @return true if the packet is good, false otherwise (closes socket)
	 */
	bool CheckEndianess(FInBunch& Bunch);

	/** adds the given bunch to the QueuedMessages list. Closes the connection if MAX_QUEUED_CONTROL_MESSAGES is exceeded */
	void QueueMessage(const FOutBunch* Bunch);

	/**
	 * Default constructor
	 */
	UControlChannel(const class FPostConstructInitializeProperties& PCIP)
		: UChannel(PCIP)
	{
		ChannelClasses[CHTYPE_Control]      = GetClass();
		ChType								= CHTYPE_Control;
	}

	virtual void Init( UNetConnection* InConnection, int32 InChIndex, bool InOpenedLocally );

	// Begin UChannel interface.
	virtual FPacketIdRange SendBunch(FOutBunch* Bunch, bool Merge) OVERRIDE;

	virtual void Tick() OVERRIDE;
	// End UChannel interface.


	/** Handle an incoming bunch. */
	void ReceivedBunch( FInBunch& Bunch );

	/** Describe the text channel. */
	FString Describe();
};
