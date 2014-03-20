// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysicsPublic.h
	Rigid Body Physics Public Types
=============================================================================*/

#pragma once

#include "DynamicMeshBuilder.h"

/**
 * Physics stats
 */
DECLARE_CYCLE_STAT_EXTERN(TEXT("Total Physics Time"),STAT_TotalPhysicsTime,STATGROUP_Physics, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Start Physics Time"),STAT_PhysicsKickOffDynamicsTime,STATGROUP_Physics, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Fetch Results Time"),STAT_PhysicsFetchDynamicsTime,STATGROUP_Physics, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Phys Events Time"),STAT_PhysicsEventTime,STATGROUP_Physics, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Phys SetBodyTransform"),STAT_SetBodyTransform,STATGROUP_Physics, );


#if WITH_PHYSX

namespace physx
{
	class PxScene;
	class PxConvexMesh;
	class PxTriangleMesh;
	class PxCooking;
	class PxPhysics;
	class PxVec3;
	class PxJoint;
	class PxMat44;
	class PxCpuDispatcher;
	class PxSimulationEventCallback;
	
#if WITH_APEX
	namespace apex
	{
		class NxDestructibleAsset;
		class NxApexScene;
		struct NxApexDamageEventReportData;
		class NxApexSDK;
		class NxModuleDestructible;
		class NxDestructibleActor;
		class NxModuleClothing;
		class NxModule;
		class NxClothingActor;
		class NxClothingAsset;
		class NxApexInterface;
	}
#endif // WITH_APEX
}

using namespace physx;
#if WITH_APEX
	using namespace physx::apex;
#endif	//WITH_APEX

/** Pointer to PhysX SDK object */
extern ENGINE_API PxPhysics*			GPhysXSDK;
/** Pointer to PhysX cooking object */
extern ENGINE_API PxCooking*			GPhysXCooking;
/** Pointer to PhysX allocator */
extern ENGINE_API class FPhysXAllocator* GPhysXAllocator;

/** Pointer to PhysX Command Handler */
extern ENGINE_API class FPhysCommandHandler* GPhysCommandHandler;

/** Convert PhysX PxVec3 to Unreal FVector */
ENGINE_API FVector P2UVector(const PxVec3& PVec);

#if WITH_APEX

namespace NxParameterized
{
	class Interface;
}

/** Pointer to APEX SDK object */
extern ENGINE_API NxApexSDK*			GApexSDK;
/** Pointer to APEX Destructible module object */
extern ENGINE_API NxModuleDestructible*	GApexModuleDestructible;
/** Pointer to APEX legacy module object */
extern ENGINE_API NxModule* 			GApexModuleLegacy;
#if WITH_APEX_CLOTHING
/** Pointer to APEX Clothing module object */
extern ENGINE_API NxModuleClothing*		GApexModuleClothing;
#endif //WITH_APEX_CLOTHING

#else

namespace NxParameterized
{
	typedef void Interface;
};

#endif // #if WITH_APEX

#endif // WITH_PHYSX

/** Information about a specific object involved in a rigid body collision */
struct ENGINE_API FRigidBodyCollisionInfo
{
	/** Actor involved in the collision */
	TWeakObjectPtr<AActor>					Actor;

	/** Component of Actor involved in the collision. */
	TWeakObjectPtr<UPrimitiveComponent>		Component;

	/** Index of body if this is in a PhysicsAsset. INDEX_NONE otherwise. */
	int32									BodyIndex;

	/** Name of bone if a PhysicsAsset */
	FName									BoneName;

	FRigidBodyCollisionInfo() :
		BodyIndex(INDEX_NONE),
		BoneName(NAME_None)
	{}

	/** Utility to set up the body collision info from an FBodyInstance */
	void SetFrom(const FBodyInstance* BodyInst);
	/** Get body instance */
	FBodyInstance* GetBodyInstance() const;
};

/** One entry in the array of collision notifications pending execution at the end of the physics engine run. */
struct ENGINE_API FCollisionNotifyInfo
{
	/** If this notification should be called for the Actor in Info0. */
	bool							bCallEvent0;

	/** If this notification should be called for the Actor in Info1. */
	bool							bCallEvent1;

	/** Information about the first object involved in the collision. */
	FRigidBodyCollisionInfo			Info0;

	/** Information about the second object involved in the collision. */
	FRigidBodyCollisionInfo			Info1;

	/** Information about the collision itself */
	FCollisionImpactData			RigidCollisionData;

	FCollisionNotifyInfo() :
		bCallEvent0(false),
		bCallEvent1(false)
	{}

	/** Check that is is valid to call a notification for this entry. Looks at the IsPendingKill() flags on both Actors. */
	bool IsValidForNotify() const;
};

namespace PhysCommand
{
	enum Type
	{
		Release,
		ReleasePScene,
		DeleteCPUDispatcher,
		DeleteSimEventCallback,
		Max
	};
}


/** Container used for physics tasks that need to be deferred from GameThread. This is not safe for general purpose multi-therading*/
class FPhysCommandHandler
{
public:

	/** Executes pending commands and clears buffer **/
	void Flush();

#if WITH_APEX
	/** enqueues a command to release destructible actor once apex has finished simulating */
	void ENGINE_API DeferredRelease(physx::apex::NxApexInterface* ApexInterface);
#endif

#if WITH_PHYSX
	void ENGINE_API DeferredRelease(physx::PxScene * PScene);
	void ENGINE_API DeferredDeleteSimEventCallback(physx::PxSimulationEventCallback * SimEventCallback);
	void ENGINE_API DeferredDeleteCPUDispathcer(physx::PxCpuDispatcher * CPUDispatcher);
#endif

private:

	/** Command to execute when physics simulation is done */
	struct FPhysPendingCommand
	{
		union
		{
#if WITH_APEX
			physx::apex::NxApexInterface * ApexInterface;
			physx::apex::NxDestructibleActor * DestructibleActor;
#endif
#if WITH_PHYSX
			physx::PxScene * PScene;
			physx::PxCpuDispatcher * CPUDispatcher;
			physx::PxSimulationEventCallback * SimEventCallback;
#endif
		} Pointer;

		PhysCommand::Type CommandType;
	};

	/** Execute all enqueued commands */
	void ExecuteCommands();

	/** Enqueue a command to the double buffer */
	void EnqueueCommand(const FPhysPendingCommand & Command);

	/** Array of commands waiting to execute once simulation is done */
	TArray<FPhysPendingCommand> PendingCommands;
};

/** Container object for a physics engine 'scene'. */

class FPhysScene
{
public:
	/** Indicates whether the async scene is enabled or not. */
	bool							bAsyncSceneEnabled;

	/** Indicates whether the scene is using substepping */
	bool							bSubstepping;

	/** Stores the number of valid scenes we are working with. This will be PST_MAX or PST_Async, 
		depending on whether the async scene is enabled or not*/
	uint32							NumPhysScenes;

	/** Array of collision notifications, pending execution at the end of the physics engine run. */
	TArray<FCollisionNotifyInfo>	PendingCollisionNotifies;

	/** World that owns this physics scene */
	UWorld*							OwningWorld;

	/** These indices are used to get the actual PxScene or NxApexScene from the GPhysXSceneMap. */
	int32								PhysXSceneIndex[PST_MAX];

	/** Whether or not the given scene is between its execute and sync point. */
	bool							bPhysXSceneExecuting[PST_MAX];

	/** Frame time, weighted with current frame time. */
	float							AveragedFrameTime[PST_MAX];

	/**
	 * Weight for averaged frame time.  Value should be in the range [0.0f, 1.0f].
	 * Weight = 0.0f => no averaging; current frame time always used.
	 * Weight = 1.0f => current frame time ignored; initial value of AveragedFrameTime[i] is always used.
	 */
	float							FrameTimeSmoothingFactor[PST_MAX];

private:
	/** DeltaSeconds from UWorld. */
	float										DeltaSeconds;
	/** DeltaSeconds from the WorldSettings. */
	float										MaxPhysicsDeltaTime;
	/** DeltaSeconds used by the last synchronous scene tick.  This may be used for the async scene tick. */
	float										SyncDeltaSeconds;
	/** LineBatcher from UWorld. */
	ULineBatchComponent*						LineBatcher;

	/** Completion event (not tasks) for the physics scenes these are fired by the physics system when it is done; prerequisites for the below */
	FGraphEventRef PhysicsSubsceneCompletion[PST_MAX];
	/** Completion events (not tasks) for the frame lagged physics scenes these are fired by the physics system when it is done; prerequisites for the below */
	FGraphEventRef FrameLaggedPhysicsSubsceneCompletion[PST_MAX];
	/** Completion events (task) for the physics scenes	(both apex and non-apex). This is a "join" of the above. */
	FGraphEventRef PhysicsSceneCompletion;

#if WITH_PHYSX
	/** Dispatcher for CPU tasks */
	class PxCpuDispatcher*			CPUDispatcher;
	/** Simulation event callback object */
	class FPhysXSimEventCallback*			SimEventCallback;
	/** Vehicle scene */
	class FPhysXVehicleManager*			VehicleManager;
#endif	//

public:

#if WITH_PHYSX
	/** Utility for looking up the PxScene of the given EPhysicsSceneType associated with this FPhysScene.  SceneType must be in the range [0,PST_MAX). */
	physx::PxScene*					GetPhysXScene(uint32 SceneType);

	/** Get the vehicle manager */
	FPhysXVehicleManager*						GetVehicleManager();
#endif

#if WITH_APEX
	/** Utility for looking up the NxApexScene of the given EPhysicsSceneType associated with this FPhysScene.  SceneType must be in the range [0,PST_MAX). */
	physx::apex::NxApexScene*				GetApexScene(uint32 SceneType);
#endif
	ENGINE_API FPhysScene();
	ENGINE_API ~FPhysScene();

	/** Start simulation on the physics scene of the given type */
	ENGINE_API void TickPhysScene(uint32 SceneType, FGraphEventRef& InOutCompletionEvent);

	/** Set the gravity and timing of all physics scenes */
	ENGINE_API void SetUpForFrame(const FVector* NewGrav, float InDeltaSeconds = 0.0f, float InMaxPhysicsDeltaTime = 0.0f);

	/** Starts a frame */
	ENGINE_API void StartFrame();

	/** Ends a frame */
	ENGINE_API void EndFrame(ULineBatchComponent* LineBatcher);

	/** returns the completion event for a frame */
	FGraphEventRef GetCompletionEvent()
	{
		return PhysicsSceneCompletion;
	}

	/** Set rather we're doing a static load and want to stall, or are during gameplay and want to distribute over many frames */
	ENGINE_API void SetIsStaticLoading(bool bStaticLoading);

	/** Waits for all physics scenes to complete */
	ENGINE_API void WaitPhysScenes();

	/** Fetches results, fires events, and adds debug lines */
	void ProcessPhysScene(uint32 SceneType);

	/** Sync components in the scene to physics bodies that changed */
	void SyncComponentsToBodies(uint32 SceneType);

	/** Call after WaitPhysScene on the synchronous scene to make deferred OnRigidBodyCollision calls.  */
	void DispatchPhysCollisionNotifies();

	/** Add any debug lines from the physics scene of the given type to the supplied line batcher. */
	ENGINE_API void AddDebugLines(uint32 SceneType, class ULineBatchComponent* LineBatcherToUse);

	/** @return Whether physics scene supports scene origin shifting */
	static bool SupportsOriginShifting() { return true; }

	/** @return Whether physics scene is using substepping */
	bool IsSubstepping()
	{
#if WITH_SUBSTEPPING
		return bSubstepping;
#else
		return false;
#endif
	}
	
	/** Shifts physics scene origin by specified offset */
	void ApplyWorldOffset(FVector InOffset);

	/** Returns whether an async scene is setup and can be used. This depends on the console variable "p.EnableAsyncScene". */
	ENGINE_API bool HasAsyncScene() const { return bAsyncSceneEnabled; }

	/** Lets the scene update anything related to this BodyInstance as it's now being terminated */
	void TermBody(FBodyInstance * BodyInstance);

	/** Adds a force to a body - We need to go through scene to support substepping */
	void AddForce(FBodyInstance * BodyInstance, const FVector & Force);

	/** Adds a force to a body at a specific position - We need to go through scene to support substepping */
	void AddForceAtPosition(FBodyInstance * BodyInstance, const FVector & Force, const FVector & Position);

	/** Adds torque to a body - We need to go through scene to support substepping */
	void AddTorque(FBodyInstance * BodyInstance, const FVector & Torque);

	/** Sets a Kinematic actor's target position - We need to do this here to support substepping*/
	void SetKinematicTarget(FBodyInstance * BodyInstance, const FTransform & TargetTM);

private:
	/** Initialize a scene of the given type.  Must only be called once for each scene type. */
	void InitPhysScene(uint32 SceneType);

	/** Terminate a scene of the given type.  Must only be called once for each scene type. */
	void TermPhysScene(uint32 SceneType);

	/** Called when all subscenes of a given scene are complete, calls  ProcessPhysScene*/
	void SceneCompletionTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent, EPhysicsSceneType SceneType);

#if WITH_SUBSTEPPING
	/** Task created from TickPhysScene so we can substep without blocking */
	bool SubstepSimulation(uint32 SceneType, FGraphEventRef& InOutCompletionEvent);
#endif

#if WITH_PHYSX
	/** User data wrapper passed to physx */
	struct FPhysxUserData PhysxUserData;
#endif

#if WITH_SUBSTEPPING
	class FPhysSubstepTask * PhysSubSteppers[PST_MAX];
#endif
};

// Might be handy somewhere...
enum EKCollisionPrimitiveType
{
	KPT_Sphere = 0,
	KPT_Box,
	KPT_Sphyl,
	KPT_Convex,
	KPT_Unknown
};

// Only used for legacy serialization (ver < VER_UE4_REMOVE_PHYS_SCALED_GEOM_CACHES)
class FKCachedConvexDataElement
{
public:
	TArray<uint8>	ConvexElementData;

	friend FArchive& operator<<( FArchive& Ar, FKCachedConvexDataElement& S )
	{
		S.ConvexElementData.BulkSerialize(Ar);
		return Ar;
	}
};

// Only used for legacy serialization (ver < VER_UE4_REMOVE_PHYS_SCALED_GEOM_CACHES)
class FKCachedConvexData
{
public:
	TArray<FKCachedConvexDataElement>	CachedConvexElements;

	friend FArchive& operator<<( FArchive& Ar, FKCachedConvexData& S )
	{
		Ar << S.CachedConvexElements;
		return Ar;
	}
};

// Only used for legacy serialization (ver < VER_UE4_ADD_BODYSETUP_GUID)
struct FKCachedPerTriData
{
	TArray<uint8> CachedPerTriData;

	friend FArchive& operator<<( FArchive& Ar, FKCachedPerTriData& S )
	{
		S.CachedPerTriData.BulkSerialize(Ar);
		return Ar;
	}
};



class FConvexCollisionVertexBuffer : public FVertexBuffer 
{
public:
	TArray<FDynamicMeshVertex> Vertices;

	virtual void InitRHI();
};

class FConvexCollisionIndexBuffer : public FIndexBuffer 
{
public:
	TArray<int32> Indices;

	virtual void InitRHI();
};

class FConvexCollisionVertexFactory : public FLocalVertexFactory
{
public:

	FConvexCollisionVertexFactory()
	{}

	/** Initialization constructor. */
	FConvexCollisionVertexFactory(const FConvexCollisionVertexBuffer* VertexBuffer)
	{
		InitConvexVertexFactory(VertexBuffer);
	}


	void InitConvexVertexFactory(const FConvexCollisionVertexBuffer* VertexBuffer);
};

class FKConvexGeomRenderInfo
{
public:
	FConvexCollisionVertexBuffer* VertexBuffer;
	FConvexCollisionIndexBuffer* IndexBuffer;
	FConvexCollisionVertexFactory* CollisionVertexFactory;
};

/**
 *	Load the required modules for PhysX
 */
ENGINE_API void LoadPhysXModules();
/** 
 *	Unload the required modules for PhysX
 */
void UnloadPhysXModules();

ENGINE_API void	InitGamePhys();
ENGINE_API void	TermGamePhys();


bool	ExecPhysCommands(const TCHAR* Cmd, FOutputDevice* Ar, UWorld* InWorld);

/** Util to list to log all currently awake rigid bodies */
void	ListAwakeRigidBodies(bool bIncludeKinematic, UWorld* world);


FTransform FindBodyTransform(AActor* Actor, FName BoneName);
FBox	FindBodyBox(AActor* Actor, FName BoneName);

