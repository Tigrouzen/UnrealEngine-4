// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "Net/UnrealNetwork.h"

AOnlineBeaconHost::AOnlineBeaconHost(const class FPostConstructInitializeProperties& PCIP) :
Super(PCIP)
{
	NetDriverName = FName(TEXT("BeaconDriver"));
}

void AOnlineBeaconHost::OnNetCleanup(UNetConnection* Connection)
{
	UE_LOG(LogBeacon, Error, TEXT("Cleaning up a beacon host!"));
}

bool AOnlineBeaconHost::InitHost()
{
	FURL URL(NULL, TEXT(""), TRAVEL_Absolute);

	// Allow the command line to override the default port
	int32 PortOverride;
	if (FParse::Value(FCommandLine::Get(), TEXT("BeaconPort="), PortOverride) && PortOverride != 0)
	{
		ListenPort = PortOverride;
	}

	URL.Port = ListenPort;
	if(URL.Valid)
	{
		if (InitBase() && NetDriver)
		{
			FString Error;
			if (NetDriver->InitListen(this, URL, false, Error))
			{
				ListenPort = URL.Port;
				NetDriver->SetWorld(GetWorld());
				NetDriver->Notify = this;
				NetDriver->InitialConnectTimeout = BEACON_CONNECTION_TIMEOUT;
				NetDriver->ConnectionTimeout = BEACON_CONNECTION_TIMEOUT;
				return true;
			}
			else
			{
				// error initializing the network stack...
				UE_LOG(LogNet, Log, TEXT("AOnlineBeaconHost::InitHost failed"));
				OnFailure();
			}
		}
	}

	return false;
}

void AOnlineBeaconHost::HandleNetworkFailure(UWorld* World, class UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (NetDriver && NetDriver->NetDriverName == BeaconNetDriverName)
	{
		// Timeouts from clients are ignored
		if (FailureType != ENetworkFailure::ConnectionTimeout)
		{
			Super::HandleNetworkFailure(World, NetDriver, FailureType, ErrorString);
		}
	}
}

void AOnlineBeaconHost::NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, class FInBunch& Bunch)
{
	if(NetDriver->ServerConnection == NULL)
	{
		bool bCloseConnection = false;

		// We are the server.
#if !(UE_BUILD_SHIPPING && WITH_EDITOR)
		UE_LOG(LogNet, Verbose, TEXT("Beacon: Host received: %s"), FNetControlMessageInfo::GetName(MessageType));
#endif
		switch (MessageType)
		{
		case NMT_Hello:
			{
				UE_LOG(LogNet, Log, TEXT("Beacon Hello"));
				uint8 IsLittleEndian;
				int32 RemoteMinVer, RemoteVer;
				FGuid RemoteGameGUID;
				FNetControlMessage<NMT_Hello>::Receive(Bunch, IsLittleEndian, RemoteMinVer, RemoteVer, RemoteGameGUID);

				if (!IsNetworkCompatible(Connection->Driver->RequireEngineVersionMatch, RemoteVer, RemoteMinVer))
				{
					FNetControlMessage<NMT_Upgrade>::Send(Connection, GEngineMinNetVersion, GEngineNetVersion);
					bCloseConnection = true;
				}
				else
				{
					Connection->NegotiatedVer = FMath::Min(RemoteVer, GEngineNetVersion);

					// Make sure the server has the same GameGUID as we do
					if( RemoteGameGUID != GetDefault<UGeneralProjectSettings>()->ProjectID )
					{
						FString ErrorMsg = NSLOCTEXT("NetworkErrors", "ServerHostingDifferentGame", "Incompatible game connection.").ToString();
						FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
						bCloseConnection = true;
						break;
					}

					Connection->Challenge = FString::Printf(TEXT("%08X"), FPlatformTime::Cycles());
					FNetControlMessage<NMT_BeaconWelcome>::Send(Connection);
					Connection->FlushNet();
				}
				break;
			}
		case NMT_Netspeed:
			{
				int32 Rate;
				FNetControlMessage<NMT_Netspeed>::Receive(Bunch, Rate);
				Connection->CurrentNetSpeed = FMath::Clamp(Rate, 1800, NetDriver->MaxClientRate);
				UE_LOG(LogNet, Log, TEXT("Beacon: Client netspeed is %i"), Connection->CurrentNetSpeed);
				break;
			}
		case NMT_BeaconJoin:
			{
				FString ErrorMsg;
				FString BeaconType;
				FNetControlMessage<NMT_BeaconJoin>::Receive(Bunch, BeaconType);
				UE_LOG(LogNet, Log, TEXT("Beacon Join %s"), *BeaconType);

				if (Connection->ClientWorldPackageName == NAME_None)
				{
					AOnlineBeaconClient* ClientActor = GetClientActor(Connection);
					if (ClientActor == NULL)
					{
						UWorld* World = GetWorld();
						Connection->ClientWorldPackageName = World->GetOutermost()->GetFName();

						AOnlineBeaconClient* NewClientActor = SpawnBeaconActor();
						if (NewClientActor && BeaconType == NewClientActor->GetBeaconType())
						{
							FNetworkGUID NetGUID = Connection->PackageMap->AssignNewNetGUID(NewClientActor);
							NewClientActor->SetNetConnection(Connection);
							Connection->OwningActor = NewClientActor;
							NewClientActor->Role = ROLE_None;
							NewClientActor->SetReplicates(false);
							ClientActors.Add(NewClientActor);
							FNetControlMessage<NMT_BeaconAssignGUID>::Send(Connection, NetGUID);
						}
						else
						{
							ErrorMsg = NSLOCTEXT("NetworkErrors", "BeaconSpawnFailureError", "Join failure, Couldn't spawn beacon.").ToString();
						}
					}
					else
					{
						ErrorMsg = NSLOCTEXT("NetworkErrors", "BeaconSpawnExistingActorError", "Join failure, existing beacon actor.").ToString();;
					}
				}
				else
				{
					ErrorMsg = NSLOCTEXT("NetworkErrors", "BeaconSpawnClientWorldPackageNameError", "Join failure, existing ClientWorldPackageName.").ToString();;
				}

				if (!ErrorMsg.IsEmpty())
				{
					UE_LOG(LogNet, Log, TEXT("%s"), *ErrorMsg);
					FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
					bCloseConnection = true;
				}

				break;
			}
		case NMT_BeaconNetGUIDAck:
			{
				FString BeaconType;
				FNetControlMessage<NMT_BeaconNetGUIDAck>::Receive(Bunch, BeaconType);

				AOnlineBeaconClient* ClientActor = GetClientActor(Connection);
				if (ClientActor && BeaconType == ClientActor->GetBeaconType())
				{
					ClientActor->Role = ROLE_Authority;
					ClientActor->SetReplicates(true);
					ClientActor->SetAutonomousProxy(true);
					// Send an RPC to the client to open the actor channel and guarantee RPCs will work
					ClientActor->ClientOnConnected();
					UE_LOG(LogNet, Log, TEXT("Beacon Handshake complete!"));
					FOnBeaconConnected* OnBeaconConnectedDelegate = OnBeaconConnectedMapping.Find(FName(*BeaconType));
					if (OnBeaconConnectedDelegate)
					{
						OnBeaconConnectedDelegate->ExecuteIfBound(ClientActor, Connection);
					}
				}
				else
				{
					// Failed to connect.
					FString ErrorMsg = NSLOCTEXT("NetworkErrors", "BeaconSpawnNetGUIDAckError", "Join failure, no actor at NetGUIDAck.").ToString();
					UE_LOG(LogNet, Log, TEXT("%s"), *ErrorMsg);
					FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
					bCloseConnection = true;
				}
				break;
			}
		case NMT_BeaconWelcome:
		case NMT_BeaconAssignGUID:
		default:
			{
				FString ErrorMsg = NSLOCTEXT("NetworkErrors", "BeaconSpawnUnexpectedError", "Join failure, unexpected control message.").ToString();
				UE_LOG(LogNet, Log, TEXT("%s"), *ErrorMsg);
				FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
				bCloseConnection = true;
			}
			break;
		}

		if (bCloseConnection)
		{		
			AOnlineBeaconClient* ClientActor = GetClientActor(Connection);
			if (ClientActor)
			{
				RemoveClientActor(ClientActor);
			}

			Connection->FlushNet(true);
			Connection->Close();
		}
	}
}

AOnlineBeaconClient* AOnlineBeaconHost::GetClientActor(UNetConnection* Connection)
{
	for (int32 ClientIdx=0; ClientIdx < ClientActors.Num(); ClientIdx++)
	{
		if (ClientActors[ClientIdx]->GetNetConnection() == Connection)
		{
			return ClientActors[ClientIdx];
		}
	}

	return NULL;
}

void AOnlineBeaconHost::RemoveClientActor(AOnlineBeaconClient* ClientActor)
{
	if (ClientActor)
	{
		ClientActors.RemoveSingleSwap(ClientActor);
		ClientActor->Destroy();
	}
}

AOnlineBeaconHost::FOnBeaconConnected& AOnlineBeaconHost::OnBeaconConnected(FName BeaconType)
{ 
	FOnBeaconConnected* BeaconDelegate = OnBeaconConnectedMapping.Find(BeaconType);
	if (BeaconDelegate == NULL)
	{
		FOnBeaconConnected NewDelegate;
		OnBeaconConnectedMapping.Add(BeaconType, NewDelegate);
		BeaconDelegate = OnBeaconConnectedMapping.Find(BeaconType);
	}

	return *BeaconDelegate; 
}
