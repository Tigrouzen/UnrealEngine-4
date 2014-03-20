// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "PhysicsHandleComponent.generated.h"

namespace physx
{
	class PxJoint;
	class PxRigidDynamic;
}

/**
 *	Utility object for moving physics objects around.
 */
UCLASS(collapsecategories, ClassGroup=Physics, hidecategories=Object, HeaderGroup=Component, MinimalAPI, meta=(BlueprintSpawnableComponent))
class UPhysicsHandleComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	/** Component we are currently holding */
	UPROPERTY()
	class UPrimitiveComponent* GrabbedComponent;

	/** Name of bone, if we are grabbing a skeletal component */
	FName GrabbedBoneName;

	/** Physics scene index of the body we are grabbing. */
	int32 SceneIndex;

	/** Are we currently constraining the rotation of the grabbed object. */
	uint32 bRotationConstrained:1;

	/** Linear damping of the handle spring. */
	UPROPERTY(EditAnywhere, Category=PhysicsHandle)
	float LinearDamping;

	/** Linear stiffness of the handle spring */
	UPROPERTY(EditAnywhere, Category=PhysicsHandle)
	float LinearStiffness;

	/** Angular stiffness of the handle spring */
	UPROPERTY(EditAnywhere, Category=PhysicsHandle)
	float AngularDamping;

	/** Angular stiffness of the handle spring */
	UPROPERTY(EditAnywhere, Category=PhysicsHandle)
	float AngularStiffness;

	/** Target transform */
	FTransform TargetTransform;
	/** Current transform */
	FTransform CurrentTransform;

	/** How quickly we interpolate the physics target transform */
	UPROPERTY(EditAnywhere, Category=PhysicsHandle)
	float InterpolationSpeed;


protected:
	/** Pointer to PhysX joint used by the handle*/
	physx::PxJoint* HandleData;
	/** Pointer to kinematic actor jointed to grabbed object */
	physx::PxRigidDynamic* KinActorData;

	// Begin UActorComponent interface.
	virtual void OnUnregister() OVERRIDE;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) OVERRIDE;
	// End UActorComponent interface.

public:

	/** Grab the specified component */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsHandle")
	ENGINE_API void GrabComponent(class UPrimitiveComponent* Component, FName InBoneName, FVector GrabLocation, bool bConstrainRotation);

	/** Release the currently held component */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsHandle")
	ENGINE_API void ReleaseComponent();

	/** Set the target location */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsHandle")
	ENGINE_API void SetTargetLocation(FVector NewLocation);

	/** Set the target rotation */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsHandle")
	ENGINE_API void SetTargetRotation(FRotator NewRotation);

	/** Set target location and rotation */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsHandle")
	ENGINE_API void SetTargetLocationAndRotation(FVector NewLocation, FRotator NewRotation);

	/** Get the current location and rotation */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsHandle")
	ENGINE_API void GetTargetLocationAndRotation(FVector& TargetLocation, FRotator& TargetRotation) const;

protected:
	/** Move the kinematic handle to the specified */
	void UpdateHandleTransform(const FTransform& NewTransform);
};



