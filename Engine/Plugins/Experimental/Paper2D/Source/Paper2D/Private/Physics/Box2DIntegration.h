// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_BOX2D

#include <Box2D/Box2D.h>

//////////////////////////////////////////////////////////////////////////

// Tick function that starts the physics tick
struct FStartPhysics2DTickFunction : public FTickFunction
{
	struct FPhysicsScene2D* Target;

	// FTickFunction interface
	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) OVERRIDE;
	virtual FString DiagnosticMessage() OVERRIDE;
	// End of FTickFunction interface
};

// Tick function that ends the physics tick
struct FEndPhysics2DTickFunction : public FTickFunction
{
	struct FPhysicsScene2D* Target;

	// FTickFunction interface
	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) OVERRIDE;
	virtual FString DiagnosticMessage() OVERRIDE;
	// End of FTickFunction interface
};

//////////////////////////////////////////////////////////////////////////
// FPhysicsWorld2D

struct FPhysicsScene2D
{
	class b2World* World;
	UWorld* UnrealWorld;

	// Tick function for starting physics
	FStartPhysics2DTickFunction StartPhysicsTickFunction;

	// Tick function for ending physics
	FEndPhysics2DTickFunction EndPhysicsTickFunction;

	FPhysicsScene2D(UWorld* AssociatedWorld);
	~FPhysicsScene2D();
};

#endif

//////////////////////////////////////////////////////////////////////////
// FPhysicsIntegration2D

struct FPhysicsIntegration2D : public FGCObject
{
	//@TODO: HACKY
	#define UnrealUnitsPerMeter 100.0f

	static void InitializePhysics();
	static void ShutdownPhysics();

	// FSerializableObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) OVERRIDE;
	// End of FSerializableObject

	static void DrawDebugPhysics(UWorld* World, FPrimitiveDrawInterface* PDI, const FSceneView* View);
#if WITH_BOX2D

private:
	static TMap< UWorld*, TSharedPtr<FPhysicsScene2D> > WorldMappings;

	static FWorldDelegates::FWorldInitializationEvent::FDelegate OnWorldCreatedDelegate;
	static FWorldDelegates::FWorldEvent::FDelegate OnWorldDestroyedDelegate;

public:

	//@TODO: These are both fixed on a XZ coordinate system
	static inline FVector ConvertBoxVectorToUnreal(const b2Vec2& Vector)
	{
		return FVector(Vector.x * UnrealUnitsPerMeter, 0.0f, Vector.y * UnrealUnitsPerMeter);
	}

	static inline b2Vec2 ConvertUnrealVectorToBox(const FVector& Vector)
	{
		return b2Vec2(Vector.X / UnrealUnitsPerMeter, Vector.Z / UnrealUnitsPerMeter);
	}

	// Finds the scene associated with the specified UWorld
	static TSharedPtr<FPhysicsScene2D> FindAssociatedScene(UWorld* Source);

	// Finds the Box2D world associated with the specified UWorld
	static b2World* FindAssociatedWorld(UWorld* Source);

	static void OnWorldCreated(UWorld* World, const UWorld::InitializationValues IVS);
	static void OnWorldDestroyed(UWorld* World);
#endif

private:
	FPhysicsIntegration2D() {}
};
