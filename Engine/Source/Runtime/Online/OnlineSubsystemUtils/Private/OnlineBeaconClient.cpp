// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "Net/DataChannel.h"

#define BEACON_RPC_TIMEOUT 15.0f

AOnlineBeaconClient::AOnlineBeaconClient(const class FPostConstructInitializeProperties& PCIP) :
	Super(PCIP),
	BeaconOwner(NULL)
{
	NetDriverName = FName(TEXT("BeaconDriver"));
	bOnlyRelevantToOwner = true;
}

AOnlineBeaconHost* AOnlineBeaconClient::GetBeaconOwner() const
{
	return BeaconOwner;
}

void AOnlineBeaconClient::SetBeaconOwner(AOnlineBeaconHost* InBeaconOwner)
{
	BeaconOwner = InBeaconOwner;
}

bool AOnlineBeaconClient::InitClient(FURL& URL)
{
	bool bSuccess = false;

	if(URL.Valid)
	{
		if (InitBase() && NetDriver)
		{
			FString Error;
			if (NetDriver->InitConnect(this, URL, Error))
			{
				NetDriver->SetWorld(GetWorld());
				NetDriver->Notify = this;
				NetDriver->InitialConnectTimeout = BEACON_CONNECTION_INITIAL_TIMEOUT;
				NetDriver->ConnectionTimeout = BEACON_CONNECTION_INITIAL_TIMEOUT;

				// Send initial message.
				uint8 IsLittleEndian = uint8(PLATFORM_LITTLE_ENDIAN);
				check(IsLittleEndian == !!IsLittleEndian); // should only be one or zero
				BeaconConnection = NetDriver->ServerConnection;
				FNetControlMessage<NMT_Hello>::Send(NetDriver->ServerConnection, IsLittleEndian, GEngineMinNetVersion, GEngineNetVersion, Cast<UGeneralProjectSettings>(UGeneralProjectSettings::StaticClass()->GetDefaultObject())->ProjectID);
				NetDriver->ServerConnection->FlushNet();

				bSuccess = true;
			}
			else
			{
				// error initializing the network stack...
				UE_LOG(LogNet, Log, TEXT("AOnlineBeaconClient::InitClient failed"));
				OnFailure();
			}
		}
	}

	return bSuccess;
}

void AOnlineBeaconClient::ClientOnConnected_Implementation()
{
	Role = ROLE_Authority;
	SetReplicates(true);
	SetAutonomousProxy(true);

	// Fail safe for connection to server but no client connection RPC
	GetWorldTimerManager().ClearTimer(this, &AOnlineBeaconClient::OnFailure);

	if (NetDriver)
	{
		// Increase timeout while we are connected
		NetDriver->InitialConnectTimeout = BEACON_CONNECTION_TIMEOUT;
		NetDriver->ConnectionTimeout = BEACON_CONNECTION_TIMEOUT;
	}

	// Call the overloaded function for this client class
	OnConnected();
}

void AOnlineBeaconClient::DestroyBeacon()
{
	// Fail safe for connection to server but no client connection RPC
	GetWorldTimerManager().ClearTimer(this, &AOnlineBeaconClient::OnFailure);
	Super::DestroyBeacon();
}

void AOnlineBeaconClient::OnNetCleanup(UNetConnection* Connection)
{
	AOnlineBeaconHost* BeaconHost = Cast<AOnlineBeaconHost>(GetBeaconOwner());
	if (BeaconHost)
	{
		BeaconHost->RemoveClientActor(this);
	}
}

void AOnlineBeaconClient::NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, class FInBunch& Bunch)
{
	if(NetDriver->ServerConnection)
	{
		check(Connection == NetDriver->ServerConnection);

		// We are the client
#if !(UE_BUILD_SHIPPING && WITH_EDITOR)
		UE_LOG(LogNet, Log, TEXT("Beacon client received: %s"), FNetControlMessageInfo::GetName(MessageType));
#endif
		switch (MessageType)
		{
		case NMT_BeaconWelcome:
			{
				Connection->ClientResponse = TEXT("0");
				FNetControlMessage<NMT_Netspeed>::Send(Connection, Connection->CurrentNetSpeed);

				FString BeaconType = GetBeaconType();
				if (!BeaconType.IsEmpty())
				{
					FNetControlMessage<NMT_BeaconJoin>::Send(Connection, BeaconType);
					NetDriver->ServerConnection->FlushNet();
				}
				else
				{
					// Force close the session
					UE_LOG(LogNet, Log, TEXT("Beacon close from invalid beacon type"));
					OnFailure();
				}
				break;
			}
		case NMT_BeaconAssignGUID:
			{
				FNetworkGUID NetGUID;
				FNetControlMessage<NMT_BeaconAssignGUID>::Receive(Bunch, NetGUID);
				if (NetGUID.IsValid())
				{
					Connection->PackageMap->AssignNetGUID(this, NetGUID);

					FString BeaconType = GetBeaconType();
					FNetControlMessage<NMT_BeaconNetGUIDAck>::Send(Connection, BeaconType);
					// Server will send ClientOnConnected() when it gets this control message

					// Fail safe for connection to server but no client connection RPC
					GetWorldTimerManager().SetTimer(this, &AOnlineBeaconClient::OnFailure, BEACON_RPC_TIMEOUT, false);
				}
				else
				{
					// Force close the session
					UE_LOG(LogNet, Log, TEXT("Beacon close from invalid NetGUID"));
					OnFailure();
				}
				break;
			}
		case NMT_Upgrade:
			{
				// Report mismatch.
				int32 RemoteMinVer, RemoteVer;
				FNetControlMessage<NMT_Upgrade>::Receive(Bunch, RemoteMinVer, RemoteVer);
				if (GEngineNetVersion < RemoteMinVer)
				{
					// Upgrade
					const FString ConnectionError = NSLOCTEXT("Engine", "ClientOutdated", "The match you are trying to join is running an incompatible version of the game.  Please try upgrading your game version.").ToString();
					GEngine->BroadcastNetworkFailure(GetWorld(), NetDriver, ENetworkFailure::OutdatedClient, ConnectionError);
				}
				else
				{
					// Downgrade
					const FString ConnectionError = NSLOCTEXT("Engine", "ServerOutdated", "Server's version is outdated").ToString();
					GEngine->BroadcastNetworkFailure(GetWorld(), NetDriver, ENetworkFailure::OutdatedServer, ConnectionError);
				}
				break;
			}
		case NMT_Failure:
			{
				FString ErrorMsg;
				FNetControlMessage<NMT_Failure>::Receive(Bunch, ErrorMsg);
				if (ErrorMsg.IsEmpty())
				{
					ErrorMsg = NSLOCTEXT("NetworkErrors", "GenericBeaconConnectionFailed", "Beacon Connection Failed.").ToString();
				}

				// Force close the session
				UE_LOG(LogNet, Log, TEXT("Beacon close from NMT_Failure %s"), *ErrorMsg);
				OnFailure();
				break;
			}
		case NMT_BeaconJoin:
		case NMT_BeaconNetGUIDAck:
		default:
			{
				// Force close the session
				UE_LOG(LogNet, Log, TEXT("Beacon close from unexpected control message"));
				OnFailure();
				break;
			}
		}
	}	
}
