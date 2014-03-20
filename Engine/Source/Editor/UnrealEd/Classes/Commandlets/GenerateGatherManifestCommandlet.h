// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "GenerateGatherManifestCommandlet.generated.h"

/**
 *	UGenerateGatherManifestCommandlet: Generates a localisation manifest; generally used as a gather step.
 */
UCLASS()
class UGenerateGatherManifestCommandlet : public UGatherTextCommandletBase
{
    GENERATED_UCLASS_BODY()
#if CPP || UE_BUILD_DOCS
public:
	// Begin UCommandlet Interface
	virtual int32 Main( const FString& Params ) OVERRIDE;

	bool WriteManifest( const TSharedPtr<FInternationalizationManifest>& InManifest, const FString& OutputFilePath );
	// End UCommandlet Interface
#endif
};
