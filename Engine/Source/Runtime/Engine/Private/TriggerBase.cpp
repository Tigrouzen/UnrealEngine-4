// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

ATriggerBase::ATriggerBase(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> TriggerTextureObject;
		FName ID_Triggers;
		FText NAME_Triggers;
		FConstructorStatics()
			: TriggerTextureObject(TEXT("/Engine/EditorResources/S_Trigger"))
			, ID_Triggers(TEXT("Triggers"))
			, NAME_Triggers(NSLOCTEXT( "SpriteCategory", "Triggers", "Triggers" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bHidden = true;
	bCanBeDamaged = false;

	// ATriggerBase is requesting UShapeComponent which is abstract, however it is responsibility
	// of a derived class to override this type with PCIP.SetDefaultSubobjectClass.
	CollisionComponent = PCIP.CreateAbstractDefaultSubobject<UShapeComponent>(this, TEXT("CollisionComp"));
	if (CollisionComponent)
	{
		RootComponent = CollisionComponent;
		CollisionComponent->bHiddenInGame = false;
	}

	SpriteComponent = PCIP.CreateDefaultSubobject<UBillboardComponent>(this, TEXT("Sprite"));
	if (SpriteComponent)
	{
		SpriteComponent->Sprite = ConstructorStatics.TriggerTextureObject.Get();
		SpriteComponent->bHiddenInGame = false;
		SpriteComponent->AlwaysLoadOnClient = false;
		SpriteComponent->AlwaysLoadOnServer = false;
#if WITH_EDITORONLY_DATA
		SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Triggers;
		SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Triggers;
#endif
	}
}
