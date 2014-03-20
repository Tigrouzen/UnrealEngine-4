// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "EngineKismetLibraryClasses.h"

UPhysicsCollisionHandler::UPhysicsCollisionHandler(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	ImpactReFireDelay = 0.1f;
}

void UPhysicsCollisionHandler::DefaultHandleCollision(const FRigidBodyCollisionInfo& MyInfo, const FRigidBodyCollisionInfo& OtherInfo, const FCollisionImpactData& RigidCollisionData)
{
	FRigidBodyContactInfo ContactInfo = RigidCollisionData.ContactInfos[0];

	FBodyInstance* BodyInst0 = MyInfo.GetBodyInstance();
	FBodyInstance* BodyInst1 = OtherInfo.GetBodyInstance();

	if(BodyInst0 && BodyInst1)
	{
		// Find relative velocity.
		FVector Velocity0 = BodyInst0->GetUnrealWorldVelocityAtPoint(ContactInfo.ContactPosition);
		FVector AngularVel0 = BodyInst0->GetUnrealWorldAngularVelocity();

		FVector Velocity1 = BodyInst1->GetUnrealWorldVelocityAtPoint(ContactInfo.ContactPosition);
		FVector AngularVel1 = BodyInst1->GetUnrealWorldAngularVelocity();

		const FVector RelVel = Velocity1 - Velocity0;

		// Then project along contact normal, and take magnitude.
		float ImpactVelMag = FMath::Abs(RelVel | ContactInfo.ContactNormal);

		// Difference in angular velocity between contacting bodies.
		float AngularVelMag = (AngularVel1 - AngularVel0).Size() * 70.f;

		// If bodies collide and are rotating quickly, even if relative linear velocity is not that high, use the value from the angular velocity instead.
		if (ImpactVelMag < AngularVelMag)
		{
			ImpactVelMag = AngularVelMag;
		}

		UWorld* World = GetWorld();

		if( (World != NULL) && (DefaultImpactSound != NULL) && (ImpactVelMag > ImpactThreshold) )
		{
			UGameplayStatics::PlaySoundAtLocation(World, DefaultImpactSound, ContactInfo.ContactPosition);

			LastImpactSoundTime = World->GetTimeSeconds();
		}
	}
}

void UPhysicsCollisionHandler::HandlePhysicsCollisions(const TArray<FCollisionNotifyInfo>& PendingCollisionNotifies)
{
	// Fire any collision notifies in the queue.
	for(int32 i=0; i<PendingCollisionNotifies.Num(); i++)
	{
		// If it hasn't been long enough since our last sound, just bail out
		const float TimeSinceLastImpact = GetWorld()->GetTimeSeconds() - LastImpactSoundTime;
		if(TimeSinceLastImpact < ImpactReFireDelay)
		{
			break;
		}

		// See if this impact is between 2 valid actors
		const FCollisionNotifyInfo& NotifyInfo = PendingCollisionNotifies[i];
		if( NotifyInfo.IsValidForNotify() &&
			NotifyInfo.RigidCollisionData.ContactInfos.Num() > 0 )
		{
			DefaultHandleCollision(NotifyInfo.Info0, NotifyInfo.Info1, NotifyInfo.RigidCollisionData);
		}
	}
}