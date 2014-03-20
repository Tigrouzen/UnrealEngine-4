// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ActorFactory.cpp: 
=============================================================================*/

#include "UnrealEd.h"
#include "ParticleDefinitions.h"
#include "EngineMaterialClasses.h"
#include "EngineDecalClasses.h"
#include "EngineFoliageClasses.h"
#include "SoundDefinitions.h"
#include "BlueprintUtilities.h"
#include "EngineDecalClasses.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetData.h"
#include "ScopedTransaction.h"
#include "BSPOps.h"

#include "AssetRegistryModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogActorFactory, Log, All);

#define LOCTEXT_NAMESPACE "ActorFactory"

/*-----------------------------------------------------------------------------
UActorFactory
-----------------------------------------------------------------------------*/
UActorFactory::UActorFactory(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("DefaultName","Actor");
	bShowInEditorQuickMenu = false;
}

bool UActorFactory::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	// By Default we assume the factory can't work with existing asset data
	return !AssetData.IsValid() || 
		AssetData.ObjectPath == FName(*GetDefaultActor( AssetData )->GetPathName()) || 
		AssetData.ObjectPath == FName(*GetDefaultActor( AssetData )->GetClass()->GetPathName());
}

AActor* UActorFactory::GetDefaultActor( const FAssetData& AssetData )
{
	if ( NewActorClassName != TEXT("") )
	{
		UE_LOG(LogActorFactory, Log, TEXT("Loading ActorFactory Class %s"), *NewActorClassName);
		NewActorClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *NewActorClassName, NULL, LOAD_NoWarn, NULL));
		NewActorClassName = TEXT("");
		if ( NewActorClass == NULL )
		{
			UE_LOG(LogActorFactory, Log, TEXT("ActorFactory Class LOAD FAILED"));
		}
	}
	return NewActorClass ? NewActorClass->GetDefaultObject<AActor>() : NULL;
}

UClass* UActorFactory::GetDefaultActorClass( const FAssetData& AssetData )
{
	if ( !NewActorClass )
	{
		GetDefaultActor( AssetData );
	}

	return NewActorClass;
}

UObject* UActorFactory::GetAssetFromActorInstance(AActor* ActorInstance)
{
	return NULL;
}

AActor* UActorFactory::CreateActor( UObject* Asset, ULevel* InLevel, const FVector& Location, const FRotator* const Rotation, EObjectFlags ObjectFlags, const FName Name )
{
	AActor* DefaultActor = GetDefaultActor( FAssetData( Asset ) );

	FVector SpawnLocation = Location + SpawnPositionOffset;
	FRotator SpawnRotation = Rotation ? *Rotation : DefaultActor ? DefaultActor->GetActorRotation() : FRotator::ZeroRotator;
	AActor* NewActor = NULL;
	if ( PreSpawnActor(Asset, SpawnLocation, SpawnRotation, Rotation != NULL) )
	{
		NewActor = SpawnActor(Asset, InLevel, SpawnLocation, SpawnRotation, ObjectFlags, Name);

		if ( NewActor )
		{
			PostSpawnActor(Asset, NewActor);
		}
	}

	return NewActor;
}

UBlueprint* UActorFactory::CreateBlueprint( UObject* Asset, UObject* Outer, const FName Name, const FName CallingContext )
{
	// @todo sequencer major: Needs to be overridden on any class that needs any custom setup for the new blueprint
	//	(e.g. static mesh reference assignment.)  Basically, anywhere that PostSpawnActor() or CreateActor() is overridden,
	//	we should consider overriding CreateBlueprint(), too.
	UBlueprint* NewBlueprint = FKismetEditorUtilities::CreateBlueprint( NewActorClass, Outer, Name, EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), CallingContext );
	AActor* CDO = CastChecked<AActor>( NewBlueprint->GeneratedClass->ClassDefaultObject );
	PostCreateBlueprint( Asset, CDO );
	return NewBlueprint;
}

bool UActorFactory::PreSpawnActor( UObject* Asset, FVector& InOutLocation, FRotator& InOutRotation, bool bRotationWasSupplied)
{
	// Subclasses may implement this to set up a spawn or to adjust the spawn location or rotation.
	return true;
}

AActor* UActorFactory::SpawnActor( UObject* Asset, ULevel* InLevel, const FVector& Location, const FRotator& Rotation, EObjectFlags ObjectFlags, const FName& Name )
{
	AActor* DefaultActor = GetDefaultActor( FAssetData( Asset ) );
	if ( DefaultActor )
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.OverrideLevel = InLevel;
		SpawnInfo.ObjectFlags = ObjectFlags;
		SpawnInfo.Name = Name;
		return InLevel->OwningWorld->SpawnActor( DefaultActor->GetClass(), &Location, &Rotation, SpawnInfo );
	}

	return NULL;
}

void UActorFactory::PostSpawnActor( UObject* Asset, AActor* NewActor)
{
	// Subclasses may implement this to modify the actor after it has been spawned
}

void UActorFactory::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	// Override this in derived actor factories to initialize the blueprint's CDO based on the asset assigned to the factory!
}


/*-----------------------------------------------------------------------------
UActorFactoryStaticMesh
-----------------------------------------------------------------------------*/
UActorFactoryStaticMesh::UActorFactoryStaticMesh(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("StaticMeshDisplayName", "Static Mesh");
	NewActorClass = AStaticMeshActor::StaticClass();
}

bool UActorFactoryStaticMesh::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( !AssetData.IsValid() || !AssetData.GetClass()->IsChildOf( UStaticMesh::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoStaticMesh", "A valid static mesh must be specified.");
		return false;
	}

	return true;
}

void UActorFactoryStaticMesh::PostSpawnActor( UObject* Asset, AActor* NewActor)
{
	UStaticMesh* StaticMesh = CastChecked<UStaticMesh>( Asset );
	GEditor->SetActorLabelUnique(NewActor, StaticMesh->GetName());

	UE_LOG(LogActorFactory, Log, TEXT("Actor Factory created %s"), *StaticMesh->GetName());

	// Change properties
	AStaticMeshActor* StaticMeshActor = CastChecked<AStaticMeshActor>( NewActor );
	UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->StaticMeshComponent;
	check(StaticMeshComponent);

	StaticMeshComponent->UnregisterComponent();

	StaticMeshComponent->StaticMesh = StaticMesh;
	StaticMeshComponent->StaticMeshDerivedDataKey = StaticMesh->RenderData->DerivedDataKey;

	// Init Component
	StaticMeshComponent->RegisterComponent();
}

UObject* UActorFactoryStaticMesh::GetAssetFromActorInstance(AActor* Instance)
{
	check(Instance->IsA(NewActorClass));
	AStaticMeshActor* SMA = CastChecked<AStaticMeshActor>(Instance);

	check(SMA->StaticMeshComponent);
	return SMA->StaticMeshComponent->StaticMesh;
}

void UActorFactoryStaticMesh::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	UStaticMesh* StaticMesh = CastChecked<UStaticMesh>( Asset );
	AStaticMeshActor* StaticMeshActor = CastChecked<AStaticMeshActor>( CDO );
	UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->StaticMeshComponent;

	StaticMeshComponent->StaticMesh = StaticMesh;
	StaticMeshComponent->StaticMeshDerivedDataKey = StaticMesh->RenderData->DerivedDataKey;
}

/*-----------------------------------------------------------------------------
UActorFactoryDeferredDecal
-----------------------------------------------------------------------------*/
UActorFactoryDeferredDecal::UActorFactoryDeferredDecal(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{ 
	DisplayName = LOCTEXT("DeferredDecalDisplayName", "Deferred Decal");
	NewActorClass = ADecalActor::StaticClass();
	bUseSurfaceOrientation = true;
}

bool UActorFactoryDeferredDecal::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	//We can create a DecalActor without an existing asset
	if ( UActorFactory::CanCreateActorFrom( AssetData, OutErrorMsg ) )
	{
		return true;
	}

	//But if an asset is specified it must be based-on a deferred decal umaterial
	if ( !AssetData.GetClass()->IsChildOf( UMaterialInterface::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoMaterial", "A valid material must be specified.");
		return false;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	uint32 SanityCheck = 0;
	FAssetData CurrentAssetData = AssetData;
	while( SanityCheck < 1000 && !CurrentAssetData.GetClass()->IsChildOf( UMaterial::StaticClass() ) )
	{
		const FString* ObjectPath = CurrentAssetData.TagsAndValues.Find( TEXT("Parent") );
		if ( ObjectPath == NULL )
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoMaterial", "A valid material must be specified.");
			return false;
		}

		CurrentAssetData = AssetRegistry.GetAssetByObjectPath( **ObjectPath );
		if ( !CurrentAssetData.IsValid() )
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoMaterial", "A valid material must be specified.");
			return false;
		}

		++SanityCheck;
	}

	if ( SanityCheck >= 1000 )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "RecursiveParentMaterial", "The specified material must not have a recursive parent.");
		return false;
	}

	if ( !CurrentAssetData.GetClass()->IsChildOf( UMaterial::StaticClass() ) )
	{
		return false;
	}

	const FString* MaterialDomain = CurrentAssetData.TagsAndValues.Find( TEXT("MaterialDomain") );
	if ( MaterialDomain == NULL || *MaterialDomain != TEXT("MD_DeferredDecal") )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NotDecalMaterial", "Only materials with a material domain of DeferredDecal can be specified.");
		return false;
	}

	return true;
}

bool UActorFactoryDeferredDecal::PreSpawnActor(UObject* Asset, FVector& InOutLocation, FRotator& InOutRotation, bool bRotationWasSupplied)
{
	if ( bRotationWasSupplied )
	{
		// Orient the decal in the opposite direction of the receiving surface's normal
		InOutRotation = -InOutRotation;
	}

	return true;
}

void UActorFactoryDeferredDecal::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	UMaterialInterface* Material = GetMaterial( Asset );

	if ( Material != NULL )
	{
		GEditor->SetActorLabelUnique(NewActor, Material->GetName());

		// Change properties
		TArray<UDecalComponent*> DecalComponents;
		NewActor->GetComponents(DecalComponents);

		UDecalComponent* DecalComponent = NULL;
		for (int32 Idx = 0; Idx < DecalComponents.Num() && DecalComponent == NULL; Idx++)
		{
			DecalComponent = DecalComponents[Idx];
		}

		check(DecalComponent);

		DecalComponent->UnregisterComponent();

		DecalComponent->DecalMaterial = Material;

		// Init Component
		DecalComponent->RegisterComponent();
	}
}

void UActorFactoryDeferredDecal::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	UMaterialInterface* Material = GetMaterial( Asset );

	if ( Material != NULL )
	{
		TArray<UDecalComponent*> DecalComponents;
		CDO->GetComponents(DecalComponents);

		UDecalComponent* DecalComponent = NULL;
		for (int32 Idx = 0; Idx < DecalComponents.Num() && DecalComponent == NULL; Idx++)
		{
			DecalComponent = Cast<UDecalComponent>(DecalComponents[Idx]);
		}

		check(DecalComponent);
		DecalComponent->DecalMaterial = Material;
	}
}

UMaterialInterface* UActorFactoryDeferredDecal::GetMaterial( UObject* Asset ) const
{
	UMaterialInterface* TargetMaterial = Cast<UMaterialInterface>( Asset );

	return TargetMaterial 
		&& TargetMaterial->GetMaterial() 
		&& TargetMaterial->GetMaterial()->MaterialDomain == MD_DeferredDecal ? 
TargetMaterial : 
	NULL;
}

/*-----------------------------------------------------------------------------
UActorFactoryTextRender
-----------------------------------------------------------------------------*/
UActorFactoryTextRender::UActorFactoryTextRender(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// Property initialization
	DisplayName = LOCTEXT("TextRenderDisplayName", "Text Render");
	NewActorClass = ATextRenderActor::GetPrivateStaticClass(L"...");
	bUseSurfaceOrientation = true;
}


/*-----------------------------------------------------------------------------
UActorFactoryEmitter
-----------------------------------------------------------------------------*/
UActorFactoryEmitter::UActorFactoryEmitter(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("EmitterDisplayName", "Emitter");
	NewActorClass = AEmitter::StaticClass();
}

bool UActorFactoryEmitter::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( !AssetData.IsValid() || !AssetData.GetClass()->IsChildOf( UParticleSystem::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoParticleSystem", "A valid particle system must be specified.");
		return false;
	}

	return true;
}

void UActorFactoryEmitter::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	UParticleSystem* ParticleSystem = CastChecked<UParticleSystem>(Asset);
	AEmitter* NewEmitter = CastChecked<AEmitter>(NewActor);

	GEditor->SetActorLabelUnique(NewActor, ParticleSystem->GetName());

	// Term Component
	NewEmitter->ParticleSystemComponent->UnregisterComponent();

	// Change properties
	NewEmitter->SetTemplate(ParticleSystem);

	// if we're created by Kismet on the server during gameplay, we need to replicate the emitter
	if (GWorld->HasBegunPlay() && GWorld->GetNetMode() != NM_Client)
	{
		NewEmitter->SetReplicates(true);
		NewEmitter->bAlwaysRelevant = true;
		NewEmitter->NetUpdateFrequency = 0.1f; // could also set bNetTemporary but LD might further trigger it or something
		// call into gameplay code with template so it can set up replication
		NewEmitter->SetTemplate(ParticleSystem);
	}

	// Init Component
	NewEmitter->ParticleSystemComponent->RegisterComponent();
}

UObject* UActorFactoryEmitter::GetAssetFromActorInstance(AActor* Instance)
{
	check(Instance->IsA(NewActorClass));
	AEmitter* Emitter = CastChecked<AEmitter>(Instance);
	return Emitter->ParticleSystemComponent;
}

void UActorFactoryEmitter::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	UParticleSystem* ParticleSystem = CastChecked<UParticleSystem>(Asset);
	AEmitter* Emitter = CastChecked<AEmitter>( CDO );
	Emitter->SetTemplate( ParticleSystem );
}


/*-----------------------------------------------------------------------------
UActorFactoryPlayerStart
-----------------------------------------------------------------------------*/
UActorFactoryPlayerStart::UActorFactoryPlayerStart(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("PlayerStartDisplayName", "Player Start");
	NewActorClass = APlayerStart::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryTargetPoint
-----------------------------------------------------------------------------*/
UActorFactoryTargetPoint::UActorFactoryTargetPoint( const class FPostConstructInitializeProperties& PCIP )
: Super( PCIP )
{
	DisplayName = LOCTEXT( "TargetPointDisplayName", "Target Point" );
	NewActorClass = ATargetPoint::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryNote
-----------------------------------------------------------------------------*/
UActorFactoryNote::UActorFactoryNote( const class FPostConstructInitializeProperties& PCIP )
: Super( PCIP )
{
	DisplayName = LOCTEXT( "NoteDisplayName", "Note" );
	NewActorClass = ANote::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryPhysicsAsset
-----------------------------------------------------------------------------*/
UActorFactoryPhysicsAsset::UActorFactoryPhysicsAsset(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("PhysicsAssetDisplayName", "Skeletal Physics");
	NewActorClass = ASkeletalMeshActor::StaticClass();
}

bool UActorFactoryPhysicsAsset::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( !AssetData.IsValid() || !AssetData.GetClass()->IsChildOf( UPhysicsAsset::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoPhysicsAsset", "A valid physics asset must be specified.");
		return false;
	}

	return true;
}

bool UActorFactoryPhysicsAsset::PreSpawnActor(UObject* Asset, FVector& InOutLocation, FRotator& InOutRotation, bool bRotationWasSupplied)
{
	UPhysicsAsset* PhysicsAsset = CastChecked<UPhysicsAsset>(Asset);
	USkeletalMesh* UseSkelMesh = PhysicsAsset->PreviewSkeletalMesh.Get();

	if(!UseSkelMesh)
	{
		return false;
	}

	return true;
}

void UActorFactoryPhysicsAsset::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	UPhysicsAsset* PhysicsAsset = CastChecked<UPhysicsAsset>(Asset);
	USkeletalMesh* UseSkelMesh = PhysicsAsset->PreviewSkeletalMesh.Get();

	ASkeletalMeshActor* NewSkelActor = CastChecked<ASkeletalMeshActor>(NewActor);
	GEditor->SetActorLabelUnique(NewActor, PhysicsAsset->GetName());

	// Term Component
	NewSkelActor->SkeletalMeshComponent->UnregisterComponent();

	// Change properties
	NewSkelActor->SkeletalMeshComponent->SkeletalMesh = UseSkelMesh;
	if (NewSkelActor->GetWorld()->IsPlayInEditor())
	{
		NewSkelActor->ReplicatedMesh = UseSkelMesh;
		NewSkelActor->ReplicatedPhysAsset = PhysicsAsset;
	}
	NewSkelActor->SkeletalMeshComponent->PhysicsAssetOverride = PhysicsAsset;

	// set physics setup
	NewSkelActor->SkeletalMeshComponent->KinematicBonesUpdateType = EKinematicBonesUpdateToPhysics::SkipAllBones;
	NewSkelActor->SkeletalMeshComponent->BodyInstance.bSimulatePhysics = true;
	NewSkelActor->SkeletalMeshComponent->bBlendPhysics = true;

	NewSkelActor->bAlwaysRelevant = true;
	NewSkelActor->bReplicateMovement = true;
	NewSkelActor->SetReplicates(true);

	// Init Component
	NewSkelActor->SkeletalMeshComponent->RegisterComponent();
}

void UActorFactoryPhysicsAsset::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	UPhysicsAsset* PhysicsAsset = CastChecked<UPhysicsAsset>(Asset);
	ASkeletalMeshActor* SkeletalPhysicsActor = CastChecked<ASkeletalMeshActor>( CDO );

	USkeletalMesh* UseSkelMesh = PhysicsAsset->PreviewSkeletalMesh.Get();

	SkeletalPhysicsActor->SkeletalMeshComponent->SkeletalMesh = UseSkelMesh;
	SkeletalPhysicsActor->SkeletalMeshComponent->PhysicsAssetOverride = PhysicsAsset;

	// set physics setup
	SkeletalPhysicsActor->SkeletalMeshComponent->KinematicBonesUpdateType = EKinematicBonesUpdateToPhysics::SkipAllBones;
	SkeletalPhysicsActor->SkeletalMeshComponent->BodyInstance.bSimulatePhysics = true;
	SkeletalPhysicsActor->SkeletalMeshComponent->bBlendPhysics = true;

	SkeletalPhysicsActor->bAlwaysRelevant = true;
	SkeletalPhysicsActor->bReplicateMovement = true;
	SkeletalPhysicsActor->SetReplicates(true);
}


/*-----------------------------------------------------------------------------
UActorFactoryAnimationAsset
-----------------------------------------------------------------------------*/
UActorFactoryAnimationAsset::UActorFactoryAnimationAsset(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("SingleAnimSkeletalDisplayName", "Single Animation Skeletal");
	NewActorClass = ASkeletalMeshActor::StaticClass();
}

bool UActorFactoryAnimationAsset::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{ 
	if ( !AssetData.IsValid() || 
		( !AssetData.GetClass()->IsChildOf( UAnimSequence::StaticClass() ) && 
		  !AssetData.GetClass()->IsChildOf( UVertexAnimation::StaticClass() )) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoAnimData", "A valid anim data must be specified.");
		return false;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	FAssetData SkeletalMeshData;

	if ( AssetData.GetClass()->IsChildOf( UAnimSequence::StaticClass() ) )
	{
		const FString* SkeletonPath = AssetData.TagsAndValues.Find("Skeleton");
		if ( SkeletonPath == NULL || SkeletonPath->IsEmpty() ) 
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoSkeleton", "UAnimationAssets must have a valid Skeleton.");
			return false;
		}

		FAssetData SkeletonData = AssetRegistry.GetAssetByObjectPath( **SkeletonPath );

		if ( !SkeletonData.IsValid() )
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoSkeleton", "UAnimationAssets must have a valid Skeleton.");
			return false;
		}

		const FString* SkeletalMeshPath = SkeletonData.TagsAndValues.Find( TEXT("PreviewSkeletalMesh") );
		if ( SkeletalMeshPath == NULL || SkeletalMeshPath->IsEmpty() ) 
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "UAnimationAssetNoSkeleton", "UAnimationAssets must have a valid Skeleton with a valid preview skeletal mesh.");
			return false;
		}

		SkeletalMeshData = AssetRegistry.GetAssetByObjectPath( **SkeletalMeshPath );
	}

	if ( AssetData.GetClass()->IsChildOf( UVertexAnimation::StaticClass() ) )
	{
		const FString* BaseSkeletalMeshPath = AssetData.TagsAndValues.Find( TEXT("BaseSkelMesh") );
		if ( BaseSkeletalMeshPath == NULL || BaseSkeletalMeshPath->IsEmpty() ) 
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "UVertexAnimationNoSkeleton", "UVertexAnimations must have a valid base skeletal mesh.");
			return false;
		}

		SkeletalMeshData = AssetRegistry.GetAssetByObjectPath( **BaseSkeletalMeshPath );
	}

	if ( !SkeletalMeshData.IsValid() )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoSkeletalMeshAss", "No valid skeletal mesh was found associated with the animation sequence.");
		return false;
	}

	// Check to see if it's actually a DestructibleMesh, in which case we won't use this factory
	if ( SkeletalMeshData.GetClass()->IsChildOf( UDestructibleMesh::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoDestructibleMesh", "The animation sequence must not have a DestructibleMesh associated with it.");
		return false;
	}

	return true;
}

USkeletalMesh* UActorFactoryAnimationAsset::GetSkeletalMeshFromAsset( UObject* Asset ) const
{
	USkeletalMesh* SkeletalMesh = NULL;
	UAnimSequence* AnimationAsset = Cast<UAnimSequence>( Asset );
	UVertexAnimation* VertexAnimation = Cast<UVertexAnimation>( Asset );

	if( AnimationAsset != NULL )
	{
		// base it on preview skeletal mesh, just to have something
		SkeletalMesh = AnimationAsset->GetSkeleton()? AnimationAsset->GetSkeleton()->GetPreviewMesh() : NULL;
	}
	else if( VertexAnimation != NULL )
	{
		// base it on preview skeletal mesh, just to have something
		SkeletalMesh = VertexAnimation->BaseSkelMesh;
	}

	// Check to see if it's actually a DestructibleMesh, in which case we won't use this factory
	if( SkeletalMesh != NULL && SkeletalMesh->IsA(UDestructibleMesh::StaticClass()) )
	{
		SkeletalMesh = NULL;
	}

	check( SkeletalMesh != NULL );
	return SkeletalMesh;
}

void UActorFactoryAnimationAsset::PostSpawnActor( UObject* Asset, AActor* NewActor )
{
	Super::PostSpawnActor( Asset, NewActor );
	UAnimationAsset* AnimationAsset = Cast<UAnimationAsset>(Asset);
	UVertexAnimation* VertexAnimation = Cast<UVertexAnimation>(Asset);

	ASkeletalMeshActor* NewSMActor = CastChecked<ASkeletalMeshActor>(NewActor);
	USkeletalMeshComponent* NewSASComponent = (NewSMActor->SkeletalMeshComponent);

	if( NewSASComponent )
	{
		if( AnimationAsset )
		{
			NewSASComponent->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
			NewSASComponent->AnimationData.AnimToPlay = AnimationAsset;
			// set runtime data
			NewSASComponent->SetAnimation(AnimationAsset);
		}
		else if( VertexAnimation )
		{
			NewSASComponent->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
			NewSASComponent->AnimationData.VertexAnimToPlay = VertexAnimation;

			// set runtime data
			NewSASComponent->SetVertexAnimation(VertexAnimation);
		}
	}
}

void UActorFactoryAnimationAsset::PostCreateBlueprint( UObject* Asset,  AActor* CDO )
{
	Super::PostCreateBlueprint( Asset, CDO );
	UAnimationAsset* AnimationAsset = Cast<UAnimationAsset>(Asset);
	UVertexAnimation* VertexAnimation = Cast<UVertexAnimation>(Asset);

	ASkeletalMeshActor* SkeletalMeshActor = CastChecked<ASkeletalMeshActor>( CDO );
	USkeletalMeshComponent* SkeletalComponent = ( SkeletalMeshActor->SkeletalMeshComponent );
	if (AnimationAsset)
	{
		SkeletalComponent->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
		SkeletalComponent->SetAnimation(AnimationAsset);
	}
	else if (VertexAnimation)
	{
		SkeletalComponent->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
		SkeletalComponent->SetVertexAnimation(VertexAnimation);
	}
}


/*-----------------------------------------------------------------------------
UActorFactorySkeletalMesh
-----------------------------------------------------------------------------*/
UActorFactorySkeletalMesh::UActorFactorySkeletalMesh(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{ 
	DisplayName = LOCTEXT("SkeletalMeshDisplayName", "Skeletal Mesh");
	NewActorClass = ASkeletalMeshActor::StaticClass();
}

bool UActorFactorySkeletalMesh::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{	
	if ( !AssetData.IsValid() || 
		( !AssetData.GetClass()->IsChildOf( USkeletalMesh::StaticClass() ) && 
		  !AssetData.GetClass()->IsChildOf( UAnimBlueprint::StaticClass() ) && 
		  !AssetData.GetClass()->IsChildOf( USkeleton::StaticClass() ) ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoAnimSeq", "A valid anim sequence must be specified.");
		return false;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	FAssetData SkeletalMeshData;

	if ( AssetData.GetClass()->IsChildOf( USkeletalMesh::StaticClass() ) )
	{
		SkeletalMeshData = AssetData;
	}

	if ( !SkeletalMeshData.IsValid() && AssetData.GetClass()->IsChildOf( UAnimBlueprint::StaticClass() ) )
	{
		const FString* TargetSkeletonPath = AssetData.TagsAndValues.Find( TEXT("TargetSkeleton") );
		if ( TargetSkeletonPath == NULL || TargetSkeletonPath->IsEmpty() )
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoTargetSkeleton", "UAnimBlueprints must have a valid Target Skeleton.");
			return false;
		}

		FAssetData TargetSkeleton = AssetRegistry.GetAssetByObjectPath( **TargetSkeletonPath );
		if ( !TargetSkeleton.IsValid() )
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoTargetSkeleton", "UAnimBlueprints must have a valid Target Skeleton.");
			return false;
		}

		const FString* SkeletalMeshPath = TargetSkeleton.TagsAndValues.Find( TEXT("PreviewSkeletalMesh") );
		if ( SkeletalMeshPath == NULL || SkeletalMeshPath->IsEmpty() )
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoPreviewSkeletalMesh", "The Target Skeleton of the UAnimBlueprint must have a valid Preview Skeletal Mesh.");
			return false;
		}

		SkeletalMeshData = AssetRegistry.GetAssetByObjectPath( **SkeletalMeshPath );
	}

	if ( !SkeletalMeshData.IsValid() && AssetData.GetClass()->IsChildOf( USkeleton::StaticClass() ) )
	{
		const FString* PreviewSkeletalMeshPath = AssetData.TagsAndValues.Find( TEXT("PreviewSkeletalMesh") );
		if ( PreviewSkeletalMeshPath == NULL || PreviewSkeletalMeshPath->IsEmpty() )
		{
			OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoPreviewSkeletalMesh", "The Target Skeleton of the UAnimBlueprint must have a valid Preview Skeletal Mesh.");
			return false;
		}

		SkeletalMeshData = AssetRegistry.GetAssetByObjectPath( **PreviewSkeletalMeshPath );
	}

	if ( !SkeletalMeshData.IsValid() )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoSkeletalMeshAss", "No valid skeletal mesh was found associated with the animation sequence.");
		return false;
	}

	// Check to see if it's actually a DestructibleMesh, in which case we won't use this factory
	if ( SkeletalMeshData.GetClass()->IsChildOf( UDestructibleMesh::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoDestructibleMesh", "The animation sequence must not have a DestructibleMesh associated with it.");
		return false;
	}

	return true;
}

USkeletalMesh* UActorFactorySkeletalMesh::GetSkeletalMeshFromAsset( UObject* Asset ) const
{
	USkeletalMesh*SkeletalMesh = Cast<USkeletalMesh>( Asset );
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>( Asset );
	USkeleton* Skeleton = Cast<USkeleton>( Asset );

	if( SkeletalMesh == NULL && AnimBlueprint != NULL && AnimBlueprint->TargetSkeleton )
	{
		// base it on preview skeletal mesh, just to have something
		SkeletalMesh = AnimBlueprint->TargetSkeleton->GetPreviewMesh();
	}

	if( SkeletalMesh == NULL && Skeleton != NULL )
	{
		SkeletalMesh = Skeleton->GetPreviewMesh();
	}

	check( SkeletalMesh != NULL );
	return SkeletalMesh;
}

void UActorFactorySkeletalMesh::PostSpawnActor( UObject* Asset, AActor* NewActor )
{
	USkeletalMesh* SkeletalMesh = GetSkeletalMeshFromAsset(Asset);
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>( Asset );
	ASkeletalMeshActor* NewSMActor = CastChecked<ASkeletalMeshActor>(NewActor);

	GEditor->SetActorLabelUnique(NewActor, SkeletalMesh->GetName());

	// Term Component
	NewSMActor->SkeletalMeshComponent->UnregisterComponent();

	// Change properties
	NewSMActor->SkeletalMeshComponent->SkeletalMesh = SkeletalMesh;
	if (NewSMActor->GetWorld()->IsGameWorld())
	{
		NewSMActor->ReplicatedMesh = SkeletalMesh;
	}

	// Init Component
	NewSMActor->SkeletalMeshComponent->RegisterComponent();
	if( AnimBlueprint )
	{
		NewSMActor->SkeletalMeshComponent->SetAnimClass(AnimBlueprint->GeneratedClass);
	}
}

void UActorFactorySkeletalMesh::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	USkeletalMesh* SkeletalMesh = GetSkeletalMeshFromAsset( Asset );
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>( Asset );

	ASkeletalMeshActor* SkeletalMeshActor = CastChecked<ASkeletalMeshActor>( CDO );
	SkeletalMeshActor->SkeletalMeshComponent->SkeletalMesh = SkeletalMesh;
	SkeletalMeshActor->SkeletalMeshComponent->AnimBlueprintGeneratedClass = AnimBlueprint ? Cast<UAnimBlueprintGeneratedClass>(AnimBlueprint->GeneratedClass) : NULL;
}


/*-----------------------------------------------------------------------------
UActorFactoryCameraActor
-----------------------------------------------------------------------------*/
UActorFactoryCameraActor::UActorFactoryCameraActor(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("CameraDisplayName", "Camera");
	NewActorClass = ACameraActor::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryAmbientSound
-----------------------------------------------------------------------------*/
UActorFactoryAmbientSound::UActorFactoryAmbientSound(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("AmbientSoundDisplayName", "Ambient Sound");
	NewActorClass = AAmbientSound::StaticClass();
}

bool UActorFactoryAmbientSound::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	//We allow creating AAmbientSounds without an existing sound asset
	if ( UActorFactory::CanCreateActorFrom( AssetData, OutErrorMsg ) )
	{
		return true;
	}

	if ( AssetData.IsValid() && !AssetData.GetClass()->IsChildOf( USoundBase::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoSoundAsset", "A valid sound asset must be specified.");
		return false;
	}

	return true;
}

void UActorFactoryAmbientSound::PostSpawnActor( UObject* Asset, AActor* NewActor)
{
	USoundBase* AmbientSound = Cast<USoundBase>( Asset );

	if ( AmbientSound != NULL )
	{
		AAmbientSound* NewSound = CastChecked<AAmbientSound>( NewActor );
		GEditor->SetActorLabelUnique(NewSound, AmbientSound->GetName());
		NewSound->AudioComponent->SetSound(AmbientSound);
	}
}

UObject* UActorFactoryAmbientSound::GetAssetFromActorInstance(AActor* Instance)
{
	check(Instance->IsA(NewActorClass));
	AAmbientSound* SoundActor = CastChecked<AAmbientSound>(Instance);

	check(SoundActor->AudioComponent);
	return SoundActor->AudioComponent->Sound;
}

void UActorFactoryAmbientSound::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	USoundBase* AmbientSound = Cast<USoundBase>( Asset );

	if ( AmbientSound != NULL )
	{
		AAmbientSound* NewSound = CastChecked<AAmbientSound>( CDO );
		NewSound->AudioComponent->SetSound(AmbientSound);
	}
}

/*-----------------------------------------------------------------------------
UActorFactoryClass
-----------------------------------------------------------------------------*/
UActorFactoryClass::UActorFactoryClass(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("ClassDisplayName", "Class");
}

bool UActorFactoryClass::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( AssetData.IsValid() && AssetData.GetClass()->IsChildOf( UClass::StaticClass() ) )
	{
		UClass* ActualClass = Cast<UClass>(AssetData.GetAsset());
		if ( (NULL != ActualClass) && ActualClass->IsChildOf(AActor::StaticClass()) )
		{
			return true;
		}
	}

	OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoClass", "The specified Blueprint must be Actor based.");
	return false;
}

AActor* UActorFactoryClass::GetDefaultActor( const FAssetData& AssetData )
{
	if ( AssetData.IsValid() && AssetData.GetClass()->IsChildOf( UClass::StaticClass() ) )
	{
		UClass* ActualClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *AssetData.ObjectPath.ToString(), NULL, LOAD_NoWarn, NULL));
			
		//Cast<UClass>(AssetData.GetAsset());
		if ( (NULL != ActualClass) && ActualClass->IsChildOf(AActor::StaticClass()) )
		{
			return ActualClass->GetDefaultObject<AActor>();
		}
	}

	return NULL;
}

bool UActorFactoryClass::PreSpawnActor( UObject* Asset, FVector& InOutLocation, FRotator& InOutRotation, bool bRotationWasSupplied)
{
	UClass* ActualClass = Cast<UClass>(Asset);

	if ( (NULL != ActualClass) && ActualClass->IsChildOf(AActor::StaticClass()) )
	{
		return true;
	}

	return false;
}

void UActorFactoryClass::PostSpawnActor( UObject* Asset, AActor* NewActor)
{
	UClass* ActualClass = CastChecked<UClass>(Asset);
	GEditor->SetActorLabelUnique(NewActor, ActualClass->GetName());
}

AActor* UActorFactoryClass::SpawnActor( UObject* Asset, ULevel* InLevel, const FVector& Location, const FRotator& Rotation, EObjectFlags ObjectFlags, const FName& Name )
{
	UClass* ActualClass = Cast<UClass>(Asset);

	if ( (NULL != ActualClass) && ActualClass->IsChildOf(AActor::StaticClass()) )
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.OverrideLevel = InLevel;
		SpawnInfo.ObjectFlags = ObjectFlags;
		SpawnInfo.Name = Name;
		return InLevel->OwningWorld->SpawnActor( ActualClass, &Location, &Rotation, SpawnInfo );
	}

	return NULL;
}


/*-----------------------------------------------------------------------------
UActorFactoryBlueprint
-----------------------------------------------------------------------------*/
UActorFactoryBlueprint::UActorFactoryBlueprint(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("BlueprintDisplayName", "Blueprint");
}

bool UActorFactoryBlueprint::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( !AssetData.IsValid() || !AssetData.GetClass()->IsChildOf( UBlueprint::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoBlueprint", "No Blueprint was specified, or the specified Blueprint needs to be compiled.");
		return false;
	}

	const FString* ParentClassPath = AssetData.TagsAndValues.Find( TEXT("ParentClass") );
	if ( ParentClassPath == NULL || ParentClassPath->IsEmpty() )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoBlueprint", "No Blueprint was specified, or the specified Blueprint needs to be compiled.");
		return false;
	}

	UClass* ParentClass = FindObject<UClass>(NULL, **ParentClassPath);

	bool bIsActorBased = false;
	if ( ParentClass != NULL )
	{
		// The parent class is loaded. Make sure it is derived from AActor
		bIsActorBased = ParentClass->IsChildOf(AActor::StaticClass());
	}
	else
	{
		// The parent class does not exist or is not loaded.
		// Ask the asset registry for the ancestors of this class to see if it is an unloaded blueprint generated class.
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		const FString ObjectPath = FPackageName::ExportTextPathToObjectPath(*ParentClassPath);
		const FName ParentClassPathFName = FName( *FPackageName::ObjectPathToObjectName(ObjectPath) );
		TArray<FName> AncestorClassNames;
		AssetRegistry.GetAncestorClassNames(ParentClassPathFName, AncestorClassNames);

		bIsActorBased = AncestorClassNames.Contains(AActor::StaticClass()->GetFName());
	}

	if ( !bIsActorBased )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoBlueprint", "The specified Blueprint must be Actor based.");
		return false;
	}

	return true;
}

AActor* UActorFactoryBlueprint::GetDefaultActor( const FAssetData& AssetData )
{
	if ( !AssetData.IsValid() || !AssetData.GetClass()->IsChildOf( UBlueprint::StaticClass() ) )
	{
		return NULL;
	}

	const FString* GeneratedClassPath = AssetData.TagsAndValues.Find("GeneratedClass");
	if ( GeneratedClassPath == NULL || GeneratedClassPath->IsEmpty() )
	{
		return NULL;
	}

	UClass* GeneratedClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, **GeneratedClassPath, NULL, LOAD_NoWarn, NULL));

	if ( GeneratedClass == NULL )
	{
		return NULL;
	}

	return GeneratedClass->GetDefaultObject<AActor>();
}

bool UActorFactoryBlueprint::PreSpawnActor( UObject* Asset, FVector& InOutLocation, FRotator& InOutRotation, bool bRotationWasSupplied)
{
	UBlueprint* Blueprint = CastChecked<UBlueprint>(Asset);

	// Invalid if there is no generated class, or this is not actor based
	if (Blueprint == NULL || Blueprint->GeneratedClass == NULL || !FBlueprintEditorUtils::IsActorBased(Blueprint))
	{
		return false;
	}

	return true;
}

void UActorFactoryBlueprint::PostSpawnActor( UObject* Asset, AActor* NewActor)
{
	UBlueprint* Blueprint = CastChecked<UBlueprint>(Asset);
	GEditor->SetActorLabelUnique(NewActor, Blueprint->GetName());
}



/*-----------------------------------------------------------------------------
UActorFactoryMatineeActor
-----------------------------------------------------------------------------*/
UActorFactoryMatineeActor::UActorFactoryMatineeActor(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("MatineeDisplayName", "Matinee");
	NewActorClass = AMatineeActor::StaticClass();
}

bool UActorFactoryMatineeActor::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	//We allow creating AMatineeActors without an existing asset
	if ( UActorFactory::CanCreateActorFrom( AssetData, OutErrorMsg ) )
	{
		return true;
	}

	if ( AssetData.IsValid() && !AssetData.GetClass()->IsChildOf( UInterpData::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoInterpData", "A valid InterpData must be specified.");
		return false;
	}

	return true;
}

void UActorFactoryMatineeActor::PostSpawnActor( UObject* Asset, AActor* NewActor )
{
	UInterpData* MatineeData = Cast<UInterpData>( Asset );
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( NewActor );

	if( MatineeData )
	{
		GEditor->SetActorLabelUnique(NewActor, MatineeData->GetName());
		MatineeActor->MatineeData = MatineeData;
	}
	else
	{
		// if MatineeData isn't set yet, create default one
		UInterpData * NewMatineeData = ConstructObject<UInterpData>(UInterpData::StaticClass(), NewActor);
		MatineeActor->MatineeData = NewMatineeData;
	}
}

void UActorFactoryMatineeActor::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	UInterpData* MatineeData = Cast<UInterpData>( Asset );
	AMatineeActor* MatineeActor = CastChecked<AMatineeActor>( CDO );

	// @todo sequencer: Don't ever need or want InterpData for Sequencer.  We will probably get rid of this after old Matinee goes away.
	// also note the PostSpawnActor() code above creates an UInterpData and puts it in the actor's outermost package.  Definitely do not
	// want that for Sequencer.
	MatineeActor->MatineeData = MatineeData;
}


/*-----------------------------------------------------------------------------
UActorFactoryDirectionalLight
-----------------------------------------------------------------------------*/
UActorFactoryDirectionalLight::UActorFactoryDirectionalLight(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("DirectionalLightDisplayName", "Directional Light");
	NewActorClass = ADirectionalLight::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactorySpotLight
-----------------------------------------------------------------------------*/
UActorFactorySpotLight::UActorFactorySpotLight(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("SpotLightDisplayName", "Spot Light");
	NewActorClass = ASpotLight::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryPointLight
-----------------------------------------------------------------------------*/
UActorFactoryPointLight::UActorFactoryPointLight(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("PointLightDisplayName", "Point Light");
	NewActorClass = APointLight::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactorySkyLight
-----------------------------------------------------------------------------*/
UActorFactorySkyLight::UActorFactorySkyLight( const class FPostConstructInitializeProperties& PCIP )
: Super( PCIP )
{
	DisplayName = LOCTEXT( "SkyLightDisplayName", "Sky Light" );
	NewActorClass = ASkyLight::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactorySphereReflectionCapture
-----------------------------------------------------------------------------*/
UActorFactorySphereReflectionCapture::UActorFactorySphereReflectionCapture(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("ReflectionCaptureSphereDisplayName", "Sphere Reflection Capture");
	NewActorClass = ASphereReflectionCapture::StaticClass();
	SpawnPositionOffset = FVector(0, 0, 200);
}

/*-----------------------------------------------------------------------------
UActorFactoryBoxReflectionCapture
-----------------------------------------------------------------------------*/
UActorFactoryBoxReflectionCapture::UActorFactoryBoxReflectionCapture(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("ReflectionCaptureBoxDisplayName", "Box Reflection Capture");
	NewActorClass = ABoxReflectionCapture::StaticClass();
	SpawnPositionOffset = FVector(0, 0, 200);
}

/*-----------------------------------------------------------------------------
UActorFactoryPlaneReflectionCapture
-----------------------------------------------------------------------------*/
UActorFactoryPlaneReflectionCapture::UActorFactoryPlaneReflectionCapture(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("ReflectionCapturePlaneDisplayName", "Plane Reflection Capture");
	NewActorClass = APlaneReflectionCapture::StaticClass();
	SpawnPositionOffset = FVector(0, 0, 200);
}

/*-----------------------------------------------------------------------------
UActorFactoryAtmosphericFog
-----------------------------------------------------------------------------*/
UActorFactoryAtmosphericFog::UActorFactoryAtmosphericFog(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("AtmosphericFogDisplayName", "Atmospheric Fog");
	NewActorClass = AAtmosphericFog::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryExponentialHeightFog
-----------------------------------------------------------------------------*/
UActorFactoryExponentialHeightFog::UActorFactoryExponentialHeightFog(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("ExponentialHeightFogDisplayName", "Exponential Height Fog");
	NewActorClass = AExponentialHeightFog::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryInteractiveFoliage
-----------------------------------------------------------------------------*/
UActorFactoryInteractiveFoliage::UActorFactoryInteractiveFoliage(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("InteractiveFoliageDisplayName", "Interactive Foliage");
	NewActorClass = AInteractiveFoliageActor::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryTriggerBox
-----------------------------------------------------------------------------*/
UActorFactoryTriggerBox::UActorFactoryTriggerBox(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("TriggerBoxDisplayName", "Box Trigger");
	NewActorClass = ATriggerBox::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryTriggerCapsule
-----------------------------------------------------------------------------*/
UActorFactoryTriggerCapsule::UActorFactoryTriggerCapsule(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("TriggerCapsuleDisplayName", "Capsule Trigger");
	NewActorClass = ATriggerCapsule::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryTriggerSphere
-----------------------------------------------------------------------------*/
UActorFactoryTriggerSphere::UActorFactoryTriggerSphere(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("TriggerSphereDisplayName", "Sphere Trigger");
	NewActorClass = ATriggerSphere::StaticClass();
}

/*-----------------------------------------------------------------------------
UActorFactoryDestructible
-----------------------------------------------------------------------------*/
UActorFactoryDestructible::UActorFactoryDestructible(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("DestructibleDisplayName", "Destructible");
	NewActorClass = ADestructibleActor::StaticClass();
}

bool UActorFactoryDestructible::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( !AssetData.IsValid() || !AssetData.GetClass()->IsChildOf( UDestructibleMesh::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoDestructibleMeshSpecified", "No destructible mesh was specified.");
		return false;
	}

	return true;
}

void UActorFactoryDestructible::PostSpawnActor( UObject* Asset, AActor* NewActor )
{
	UDestructibleMesh* DestructibleMesh = CastChecked<UDestructibleMesh>( Asset );
	ADestructibleActor* NewDestructibleActor = CastChecked<ADestructibleActor>(NewActor);

	GEditor->SetActorLabelUnique(NewActor, DestructibleMesh->GetName());

	// Term Component
	NewDestructibleActor->DestructibleComponent->UnregisterComponent();

	// Change properties
	NewDestructibleActor->DestructibleComponent->SetSkeletalMesh( DestructibleMesh );

	// Init Component
	NewDestructibleActor->DestructibleComponent->RegisterComponent();
}

UObject* UActorFactoryDestructible::GetAssetFromActorInstance(AActor* Instance)
{
	check(Instance->IsA(NewActorClass));
	ADestructibleActor* DA = CastChecked<ADestructibleActor>(Instance);

	check(DA->DestructibleComponent);
	return DA->DestructibleComponent->SkeletalMesh;
}

void UActorFactoryDestructible::PostCreateBlueprint( UObject* Asset, AActor* CDO )
{
	UDestructibleMesh* DestructibleMesh = CastChecked<UDestructibleMesh>( Asset );
	ADestructibleActor* DestructibleActor = CastChecked<ADestructibleActor>( CDO );

	DestructibleActor->DestructibleComponent->SetSkeletalMesh( DestructibleMesh );
}


/*-----------------------------------------------------------------------------
UActorFactoryVectorField
-----------------------------------------------------------------------------*/
UActorFactoryVectorFieldVolume::UActorFactoryVectorFieldVolume(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DisplayName = LOCTEXT("VectorFieldVolumeDisplayName", "Vector Field Volume");
	NewActorClass = AVectorFieldVolume::StaticClass();
}

bool UActorFactoryVectorFieldVolume::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( !AssetData.IsValid() || !AssetData.GetClass()->IsChildOf( UVectorField::StaticClass() ) )
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoVectorField", "No vector field was specified.");
		return false;
	}

	return true;
}

void UActorFactoryVectorFieldVolume::PostSpawnActor( UObject* Asset, AActor* NewActor )
{
	UVectorField* VectorField = CastChecked<UVectorField>(Asset);
	AVectorFieldVolume* VectorFieldVolumeActor = CastChecked<AVectorFieldVolume>(NewActor);

	if ( VectorFieldVolumeActor && VectorFieldVolumeActor->VectorFieldComponent )
	{
		VectorFieldVolumeActor->VectorFieldComponent->VectorField = VectorField;
		VectorFieldVolumeActor->PostEditChange();
	}
}

/*-----------------------------------------------------------------------------
CreateBrushForVolumeActor
-----------------------------------------------------------------------------*/
// Helper function for the volume actor factories, not sure where it should live
void CreateBrushForVolumeActor( AVolume* NewActor, UBrushBuilder* BrushBuilder )
{
	if ( NewActor != NULL )
	{
		// this code builds a brush for the new actor
		NewActor->PreEditChange(NULL);

		NewActor->PolyFlags = 0;
		NewActor->Brush = new( NewActor, NAME_None, RF_Transactional )UModel(FPostConstructInitializeProperties(), NULL, true );
		NewActor->Brush->Polys = new( NewActor->Brush, NAME_None, RF_Transactional )UPolys(FPostConstructInitializeProperties());
		NewActor->BrushComponent->Brush = NewActor->Brush;
		if(BrushBuilder != nullptr)
		{
			NewActor->BrushBuilder = DuplicateObject<UBrushBuilder>(BrushBuilder, NewActor);
		}

		BrushBuilder->Build( NewActor->GetWorld(), NewActor );

		FBSPOps::csgPrepMovingBrush( NewActor );

		// Set the texture on all polys to NULL.  This stops invisible textures
		// dependencies from being formed on volumes.
		if ( NewActor->Brush )
		{
			for ( int32 poly = 0 ; poly < NewActor->Brush->Polys->Element.Num() ; ++poly )
			{
				FPoly* Poly = &(NewActor->Brush->Polys->Element[poly]);
				Poly->Material = NULL;
			}
		}

		NewActor->PostEditChange();
	}
}

/*-----------------------------------------------------------------------------
UActorFactoryBoxVolume
-----------------------------------------------------------------------------*/
UActorFactoryBoxVolume::UActorFactoryBoxVolume( const class FPostConstructInitializeProperties& PCIP )
: Super(PCIP)
{
	DisplayName = LOCTEXT( "BoxVolumeDisplayName", "Box Volume" );
	NewActorClass = AVolume::StaticClass();
}

bool UActorFactoryBoxVolume::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( UActorFactory::CanCreateActorFrom( AssetData, OutErrorMsg ) )
	{
		return true;
	}

	if ( AssetData.IsValid() && !AssetData.GetClass()->IsChildOf( AVolume::StaticClass() ) )
	{
		return false;
	}

	return true;
}

void UActorFactoryBoxVolume::PostSpawnActor( UObject* Asset, AActor* NewActor )
{
	AVolume* VolumeActor = CastChecked<AVolume>(NewActor);
	if ( VolumeActor != NULL )
	{
		UCubeBuilder* Builder = ConstructObject<UCubeBuilder>( UCubeBuilder::StaticClass() );
		CreateBrushForVolumeActor( VolumeActor, Builder );
	}
}

/*-----------------------------------------------------------------------------
UActorFactorySphereVolume
-----------------------------------------------------------------------------*/
UActorFactorySphereVolume::UActorFactorySphereVolume( const class FPostConstructInitializeProperties& PCIP )
: Super(PCIP)
{
	DisplayName = LOCTEXT( "SphereVolumeDisplayName", "Sphere Volume" );
	NewActorClass = AVolume::StaticClass();
}

bool UActorFactorySphereVolume::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( UActorFactory::CanCreateActorFrom( AssetData, OutErrorMsg ) )
	{
		return true;
	}

	if ( AssetData.IsValid() && !AssetData.GetClass()->IsChildOf( AVolume::StaticClass() ) )
	{
		return false;
	}

	return true;
}

void UActorFactorySphereVolume::PostSpawnActor( UObject* Asset, AActor* NewActor )
{
	AVolume* VolumeActor = CastChecked<AVolume>(NewActor);
	if ( VolumeActor != NULL )
	{
		UTetrahedronBuilder* Builder = ConstructObject<UTetrahedronBuilder>( UTetrahedronBuilder::StaticClass() );
		Builder->SphereExtrapolation = 2;
		Builder->Radius = 192.0f;
		CreateBrushForVolumeActor( VolumeActor, Builder );
	}
}

/*-----------------------------------------------------------------------------
UActorFactoryCylinderVolume
-----------------------------------------------------------------------------*/
UActorFactoryCylinderVolume::UActorFactoryCylinderVolume( const class FPostConstructInitializeProperties& PCIP )
: Super(PCIP)
{
	DisplayName = LOCTEXT( "CylinderVolumeDisplayName", "Cylinder Volume" );
	NewActorClass = AVolume::StaticClass();
}
bool UActorFactoryCylinderVolume::CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg )
{
	if ( UActorFactory::CanCreateActorFrom( AssetData, OutErrorMsg ) )
	{
		return true;
	}

	if ( AssetData.IsValid() && !AssetData.GetClass()->IsChildOf( AVolume::StaticClass() ) )
	{
		return false;
	}

	return true;

}
void UActorFactoryCylinderVolume::PostSpawnActor( UObject* Asset, AActor* NewActor )
{
	AVolume* VolumeActor = CastChecked<AVolume>(NewActor);
	if ( VolumeActor != NULL )
	{
		UCylinderBuilder* Builder = ConstructObject<UCylinderBuilder>( UCylinderBuilder::StaticClass() );
		Builder->OuterRadius = 128.0f;
		CreateBrushForVolumeActor( VolumeActor, Builder );
	}
}

#undef LOCTEXT_NAMESPACE
