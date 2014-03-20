// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "DEditorStaticComponentMaskParameterValue.generated.h"

USTRUCT()
struct UNREALED_API FDComponentMaskParameter
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=DComponentMaskParameter)
	uint32 R:1;

	UPROPERTY(EditAnywhere, Category=DComponentMaskParameter)
	uint32 G:1;

	UPROPERTY(EditAnywhere, Category=DComponentMaskParameter)
	uint32 B:1;

	UPROPERTY(EditAnywhere, Category=DComponentMaskParameter)
	uint32 A:1;



		/** Constructor */
		FDComponentMaskParameter(bool InR, bool InG, bool InB, bool InA) :
			R(InR),
			G(InG),
			B(InB),
			A(InA)
		{
		};
		FDComponentMaskParameter() :
			R(false),
			G(false),
			B(false),
			A(false)
		{
		};
	
};

UCLASS(hidecategories=Object, dependson=UUnrealEdTypes, collapsecategories)
class UNREALED_API UDEditorStaticComponentMaskParameterValue : public UDEditorParameterValue
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=DEditorStaticComponentMaskParameterValue)
	struct FDComponentMaskParameter ParameterValue;

};

