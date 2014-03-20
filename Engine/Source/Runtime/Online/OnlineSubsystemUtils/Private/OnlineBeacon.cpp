// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "Net/DataChannel.h"

DEFINE_LOG_CATEGORY(LogBeacon);

AOnlineBeacon::AOnlineBeacon(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP)
{
	NetDriverName = FName(TEXT("BeaconDriver"));
}

bool AOnlineBeacon::InitBase()
{
	if (BeaconNetDriverName != NAME_None && GEngine->CreateNamedNetDriver(GetWorld(), BeaconNetDriverName, NAME_BeaconNetDriver))
	{
		GEngine->OnNetworkFailure().AddUObject(this, &AOnlineBeacon::HandleNetworkFailure);
		NetDriver = GEngine->FindNamedNetDriver(GetWorld(), BeaconNetDriverName);
		return true;
	}

	return false;
}

void AOnlineBeacon::DestroyBeacon()
{
	UE_LOG(LogBeacon, Verbose, TEXT("Destroying beacon %s, netdriver %s"), *GetName(), NetDriver ? *NetDriver->GetDescription() : TEXT("NULL"));
	GEngine->OnNetworkFailure().RemoveUObject(this, &AOnlineBeacon::HandleNetworkFailure);
	GEngine->DestroyNamedNetDriver(GetWorld(), BeaconNetDriverName);
	NetDriver = NULL;

	Destroy();
}

void AOnlineBeacon::HandleNetworkFailure(UWorld *World, UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (NetDriver && NetDriver->NetDriverName == NetDriverName)
	{
		OnFailure();
	}
}

void AOnlineBeacon::OnFailure()
{
	GEngine->OnNetworkFailure().RemoveUObject(this, &AOnlineBeacon::HandleNetworkFailure);
	GEngine->DestroyNamedNetDriver(GetWorld(), BeaconNetDriverName);
	NetDriver = NULL;
}

void AOnlineBeacon::OnActorChannelOpen(FInBunch& Bunch, UNetConnection* Connection)
{
	Connection->OwningActor = this;
}

EAcceptConnection::Type AOnlineBeacon::NotifyAcceptingConnection()
{
	check(NetDriver);
	if(NetDriver->ServerConnection)
	{
		// We are a client and we don't welcome incoming connections.
		UE_LOG(LogNet, Log, TEXT("NotifyAcceptingConnection: Client refused"));
		return EAcceptConnection::Reject;
	}
	else if(BeaconState == EBeaconState::DenyRequests)
	{
		// Server is down
		UE_LOG(LogNet, Log, TEXT("NotifyAcceptingConnection: Server %s refused"), *GetName());
		return EAcceptConnection::Reject;
	}
	else //if(BeaconState == EBeaconState::AllowRequests)
	{
		// Server is up and running.
		UE_LOG(LogNet, Log, TEXT("NotifyAcceptingConnection: Server %s accept"), *GetName());
		return EAcceptConnection::Accept;
	}
}

void AOnlineBeacon::NotifyAcceptedConnection(UNetConnection* Connection)
{
	check(NetDriver != NULL);
	check(NetDriver->ServerConnection == NULL);
	UE_LOG(LogNet, Log, TEXT("Open %s %0.2f %s"), *GetName(), FPlatformTime::StrTimestamp(), *Connection->LowLevelGetRemoteAddress());
}

bool AOnlineBeacon::NotifyAcceptingChannel(UChannel* Channel)
{
	check(Channel);
	check(Channel->Connection);
	check(Channel->Connection->Driver);
	UNetDriver* Driver = Channel->Connection->Driver;

	if (Driver->ServerConnection)
	{
		// We are a client and the server has just opened up a new channel.
		UE_LOG(LogNet, Log,  TEXT("NotifyAcceptingChannel %i/%i client %s"), Channel->ChIndex, (int32)Channel->ChType, *GetName());
		if (Channel->ChType == CHTYPE_Actor)
		{
			// Actor channel.
			UE_LOG(LogNet, Log,  TEXT("Client accepting actor channel"));
			return 1;
		}
		else
		{
			// Unwanted channel type.
			UE_LOG(LogNet, Log, TEXT("Client refusing unwanted channel of type %i"), (uint8)Channel->ChType);
			return 0;
		}
	}
	else
	{
		// We are the server.
		if (Channel->ChIndex==0 && Channel->ChType==CHTYPE_Control)
		{
			// The client has opened initial channel.
			UE_LOG(LogNet, Log, TEXT("NotifyAcceptingChannel Control %i server %s: Accepted"), Channel->ChIndex, *GetFullName());
			return 1;
		}
		else
		{
			// Client can't open any other kinds of channels.
			UE_LOG(LogNet, Log, TEXT("NotifyAcceptingChannel %i %i server %s: Refused"), (uint8)Channel->ChType, Channel->ChIndex, *GetFullName());
			return 0;
		}
	}
}

void AOnlineBeacon::NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, FInBunch& Bunch)
{
}

UNetConnection* AOnlineBeacon::GetNetConnection()
{
	return BeaconConnection;
}