// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "DistributionVectorParameterBase.generated.h"

UCLASS(abstract, collapsecategories, hidecategories=Object, editinlinenew)
class UDistributionVectorParameterBase : public UDistributionVectorConstant
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=DistributionVectorParameterBase)
	FName ParameterName;

	UPROPERTY(EditAnywhere, Category=DistributionVectorParameterBase)
	FVector MinInput;

	UPROPERTY(EditAnywhere, Category=DistributionVectorParameterBase)
	FVector MaxInput;

	UPROPERTY(EditAnywhere, Category=DistributionVectorParameterBase)
	FVector MinOutput;

	UPROPERTY(EditAnywhere, Category=DistributionVectorParameterBase)
	FVector MaxOutput;

	UPROPERTY(EditAnywhere, Category=DistributionVectorParameterBase)
	TEnumAsByte<enum DistributionParamMode> ParamModes[3];


	//Begin UDistributionVector Interface
	virtual FVector GetValue(float F = 0.f, UObject* Data = NULL, int32 Extreme = 0, class FRandomStream* InRandomStream = NULL) const OVERRIDE;
	virtual bool CanBeBaked() const OVERRIDE { return false; }
	//End UDistributionVector Interface
	
	/** todo document */
	virtual bool GetParamValue(UObject* Data, FName ParamName, FVector& OutVector) const { return false; }

	/**
	 * Return whether or not this distribution can be baked into a FRawDistribution lookup table
	 */
};



