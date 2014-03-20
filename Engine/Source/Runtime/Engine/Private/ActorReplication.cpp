// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Net/UnrealNetwork.h"
#include "NavigationPathBuilder.h"

/*-----------------------------------------------------------------------------
	AActor networking implementation.
-----------------------------------------------------------------------------*/

//
// Static variables for networking.
//
static bool		SavedbHidden;
static AActor*	SavedOwner;

float AActor::GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, APlayerController* Viewer, UActorChannel* InChannel, float Time, bool bLowBandwidth)
{
	if ( bNetUseOwnerRelevancy && Owner)
	{
		return Owner->GetNetPriority(ViewPos, ViewDir, Viewer, InChannel, Time, bLowBandwidth);
	}

	if ( Instigator && (Instigator == Viewer->GetPawn()) )
	{
		Time *= 4.f; 
	}
	else if ( !bHidden )
	{
		FVector Dir = GetActorLocation() - ViewPos;
		float DistSq = Dir.SizeSquared();
		
		// adjust priority based on distance and whether actor is in front of viewer
		if ( (ViewDir | Dir) < 0.f )
		{
			if ( DistSq > NEARSIGHTTHRESHOLDSQUARED )
			{
				Time *= 0.2f;
			}
			else if ( DistSq > CLOSEPROXIMITYSQUARED )
			{
				Time *= 0.4f;
			}
		}
		else if ( DistSq > MEDSIGHTTHRESHOLDSQUARED )
		{
			Time *= 0.4f;
		}
	}

	return NetPriority * Time;
}

bool AActor::GetNetDormancy(const FVector& ViewPos, const FVector& ViewDir, class APlayerController* Viewer, UActorChannel* InChannel, float Time, bool bLowBandwidth)
{
	// For now, per peer dormancy is not supported
	return false;
}


void AActor::PreNetReceive()
{
	SavedbHidden = bHidden;
	SavedOwner = Owner;
}

void AActor::PostNetReceive()
{
	ExchangeB( bHidden, SavedbHidden );
	Exchange ( Owner, SavedOwner );

	if (bHidden != SavedbHidden)
	{
		SetActorHiddenInGame(SavedbHidden);
	}
	if (Owner != SavedOwner)
	{
		SetOwner(SavedOwner);
	}
}

void AActor::OnRep_ReplicatedMovement()
{
	if( RootComponent && RootComponent->IsSimulatingPhysics() )
	{
		PostNetReceivePhysicState();
	}
	else
	{
		if (Role == ROLE_SimulatedProxy)
		{
			PostNetReceiveVelocity(ReplicatedMovement.LinearVelocity);
			PostNetReceiveLocation();

			if( RootComponent && (ReplicatedMovement.Rotation != GetActorRotation()) )
			{
				RootComponent->MoveComponent(FVector::ZeroVector, ReplicatedMovement.Rotation, false);
			}
		}
	}
}

void AActor::PostNetReceiveLocation()
{
	USceneComponent* RootSceneComp = Cast<USceneComponent>(RootComponent);
	if( RootSceneComp && RootSceneComp->IsRegistered() && (ReplicatedMovement.Location != GetActorLocation()) )
	{
		TeleportTo(ReplicatedMovement.Location, GetActorRotation(), false, true);
	}
}

void AActor::PostNetReceiveVelocity(const FVector& NewVelocity)
{
}

void AActor::PostNetReceivePhysicState()
{
	UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(RootComponent);
	if (RootPrimComp)
	{
		FRigidBodyState NewState;
		ReplicatedMovement.CopyTo(NewState);

		FVector DeltaPos(FVector::ZeroVector);
		RootPrimComp->ConditionalApplyRigidBodyState(NewState, GEngine->PhysicErrorCorrection, DeltaPos);
	}
}

bool AActor::IsNetRelevantFor(APlayerController* RealViewer, AActor* Viewer, const FVector& SrcLocation)
{
	if( bAlwaysRelevant || IsOwnedBy(Viewer) || IsOwnedBy(RealViewer) || this==Viewer || Viewer==Instigator )
	{
		return true;
	}
	else if ( bNetUseOwnerRelevancy && Owner)
	{
		return Owner->IsNetRelevantFor(RealViewer, Viewer, SrcLocation);
	}
	else if ( bOnlyRelevantToOwner )
	{
		return false;
	}
	else if ( RootComponent && RootComponent->AttachParent && RootComponent->AttachParent->GetOwner() && (Cast<USkeletalMeshComponent>(RootComponent->AttachParent) || (RootComponent->AttachParent->GetOwner() == Owner)) )
	{
		return RootComponent->AttachParent->GetOwner()->IsNetRelevantFor( RealViewer, Viewer, SrcLocation );
	}
	else if( bHidden && (!RootComponent || !RootComponent->IsCollisionEnabled()) )
	{
		return false;
	}

	if (!RootComponent)
	{
		UE_LOG(LogNet, Warning, TEXT("Actor %s / %s has no root component in AActor::IsNetRelevantFor. (Make bAlwaysRelevant=true?)"), *GetClass()->GetName(), *GetName() );
		return false;
	}
	if (GetDefault<AGameNetworkManager>()->bUseDistanceBasedRelevancy)
	{
		return ((SrcLocation - GetActorLocation()).SizeSquared() < NetCullDistanceSquared);
	}

	return true;
}

void AActor::GatherCurrentMovement()
{
	UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(GetRootComponent());
	if (RootPrimComp && RootPrimComp->IsSimulatingPhysics())
	{
		FRigidBodyState RBState;
		RootPrimComp->GetRigidBodyState(RBState);

		ReplicatedMovement.FillFrom(RBState);
	}
	else if(RootComponent != NULL)
	{
		// If we are attached, don't replicate absolute position
		if( RootComponent->AttachParent != NULL )
		{
			// Networking for attachments assumes the RootComponent of the AttachParent actor. 
			// If that's not the case, we can't update this, as the client wouldn't be able to resolve the Component and would detach as a result.
			if( AttachmentReplication.AttachParent != NULL )
			{
				AttachmentReplication.LocationOffset = RootComponent->RelativeLocation;
				AttachmentReplication.RotationOffset = RootComponent->RelativeRotation;
			}
		}
		else
		{
			ReplicatedMovement.Location = RootComponent->GetComponentLocation();
			ReplicatedMovement.Rotation = RootComponent->GetComponentRotation();
			ReplicatedMovement.LinearVelocity = GetVelocity();
			ReplicatedMovement.bRepPhysics = false;
		}
	}
}

static void GetLifetimeBlueprintReplicationList( const AActor * ThisActor, const UBlueprintGeneratedClass * MyClass, TArray< FLifetimeProperty > & OutLifetimeProps )
{
	if ( MyClass == NULL )
	{
		return;
	}

	uint32 PropertiesLeft = MyClass->NumReplicatedProperties;

	for ( TFieldIterator<UProperty> It( MyClass, EFieldIteratorFlags::ExcludeSuper ); It && PropertiesLeft > 0; ++It )
	{
		UProperty * Prop = *It;
		if ( Prop != NULL && Prop->GetPropertyFlags() & CPF_Net )
		{
			PropertiesLeft--;
			OutLifetimeProps.Add( FLifetimeProperty( Prop->RepIndex ) );
		}
	}

	return GetLifetimeBlueprintReplicationList( ThisActor, Cast< UBlueprintGeneratedClass >( MyClass->GetSuperStruct() ), OutLifetimeProps );
}

void AActor::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	GetLifetimeBlueprintReplicationList( this, Cast< UBlueprintGeneratedClass >( GetClass() ), OutLifetimeProps );

	DOREPLIFETIME( AActor, Role );
	DOREPLIFETIME( AActor, RemoteRole );
	DOREPLIFETIME( AActor, bHidden );
	DOREPLIFETIME( AActor, Owner );

	DOREPLIFETIME( AActor, bTearOff );
	DOREPLIFETIME( AActor, bCanBeDamaged );
	DOREPLIFETIME( AActor, AttachmentReplication );

	DOREPLIFETIME( AActor, Instigator );

	DOREPLIFETIME_CONDITION( AActor, ReplicatedMovement, COND_SimulatedOrPhysics );
}

bool AActor::ReplicateSubobjects(UActorChannel *Channel, FOutBunch *Bunch, FReplicationFlags *RepFlags)
{
	check(Channel);
	check(Bunch);
	check(RepFlags);

	bool WroteSomething = false;

	for (int32 CompIdx =0; CompIdx < ReplicatedComponents.Num(); ++CompIdx )
	{
		UActorComponent * ActorComp = ReplicatedComponents[CompIdx].Get();
		if (ActorComp && ActorComp->GetIsReplicated())
		{
			WroteSomething |= Channel->ReplicateSubobject(ActorComp, *Bunch, *RepFlags);
		}
	}
	return WroteSomething;
}

void AActor::OnSubobjectCreatedFromReplication(UObject *NewSubobject)
{
	check(NewSubobject);
	if ( UActorComponent * Component = Cast<UActorComponent>(NewSubobject) )
	{
		Component->RegisterComponent();
		Component->SetIsReplicated(true);
	}
}

/** Called on the actor when a new subobject is dynamically created via replication */
void AActor::OnSubobjectDestroyFromReplication(UObject *NewSubobject)
{
	check(NewSubobject);
	if ( UActorComponent * Component = Cast<UActorComponent>(NewSubobject) )
	{
		Component->DestroyComponent();
	}
}