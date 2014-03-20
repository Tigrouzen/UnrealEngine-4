// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/** Factory which allows import of an NxApexDestructibleAsset */

#pragma once
#include "DestructibleMeshFactory.generated.h"

UCLASS(hideCategories=Object, MinimalAPI)
class UDestructibleMeshFactory : public UFactory
{
    GENERATED_UCLASS_BODY()
	// Begin UFactory Interface
	virtual FText GetDisplayName() const OVERRIDE;
#if WITH_APEX
	virtual UObject* FactoryCreateBinary( UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn ) OVERRIDE;
#endif // WITH_APEX
	// Begin UFactory Interface	
};


