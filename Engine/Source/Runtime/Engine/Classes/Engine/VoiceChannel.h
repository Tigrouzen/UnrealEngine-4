// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

//
// A channel for exchanging voice data.
//

#pragma once
#include "Net/DataBunch.h"
#include "Net/VoiceDataCommon.h"
#include "VoiceChannel.generated.h"

UCLASS(transient, customConstructor)
class UVoiceChannel : public UChannel
{
	GENERATED_UCLASS_BODY()

	// Variables.

	/** The set of outgoing voice packets for this channel */
	FVoicePacketList VoicePackets;
	
	/**
	 * Default constructor
	 */
	UVoiceChannel(const class FPostConstructInitializeProperties& PCIP)
		: UChannel(PCIP)
	{
		// Register with the network channel system
		ChannelClasses[CHTYPE_Voice] = GetClass();
		ChType = CHTYPE_Voice;
	}


// UChannel interface
protected:
	/** 
	 * Cleans up any voice data remaining in the queue 
	 */
	virtual void CleanUp();

	/**
	 * Processes the in bound bunch to extract the voice data
	 *
	 * @param Bunch the voice data to process
	 */
	virtual void ReceivedBunch(FInBunch& Bunch);

	/**
	 * Performs any per tick update of the VoIP state
	 */
	virtual void Tick();

	/** Human readable information about the channel */
	virtual FString Describe()
	{
		return FString(TEXT("VoIP: ")) + UChannel::Describe();
	}

public:

	/**
	 * Adds the voice packet to the list to send for this channel
	 *
	 * @param VoicePacket the voice packet to send
	 */
	void AddVoicePacket(TSharedPtr<class FVoicePacket> VoicePacket);
};
