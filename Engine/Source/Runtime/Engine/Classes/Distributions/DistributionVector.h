// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "DistributionVector.generated.h"

UENUM()
enum EDistributionVectorLockFlags
{
	EDVLF_None,
	EDVLF_XY,
	EDVLF_XZ,
	EDVLF_YZ,
	EDVLF_XYZ,
	EDVLF_MAX,
};

UENUM()
enum EDistributionVectorMirrorFlags
{
	EDVMF_Same,
	EDVMF_Different,
	EDVMF_Mirror,
	EDVMF_MAX,
};

/** Type-safe vector distribution. */
#if !CPP      //noexport struct
USTRUCT(noexport)
struct FVectorDistribution
{
	UPROPERTY()
	FDistributionLookupTable Table;

};
#endif

/** Type-safe 4-vector distribution. */
#if !CPP      //noexport struct
USTRUCT(noexport)
struct FVector4Distribution
{
	UPROPERTY()
	FDistributionLookupTable Table;

};
#endif

USTRUCT()
struct FRawDistributionVector : public FRawDistribution
{
	GENERATED_USTRUCT_BODY()

private:
	UPROPERTY()
	float MinValue;

	UPROPERTY()
	float MaxValue;

public:
	UPROPERTY(EditAnywhere, export, noclear, Category=RawDistributionVector)
	class UDistributionVector* Distribution;


	FRawDistributionVector()
		: MinValue(0)
		, MaxValue(0)
		, Distribution(NULL)
	{
	}


	#if WITH_EDITOR
		/**
		 * Initialize a raw distribution from the original Unreal distribution
		 */
		void Initialize();
	#endif

		/**
		 * Gets a pointer to the raw distribution if you can just call FRawDistribution::GetValue3 on it, otherwise NULL 
		 */
		const FRawDistribution *GetFastRawDistribution();

		/**
		 * Get the value at the specified F
		 */
		ENGINE_API FVector GetValue(float F=0.0f, UObject* Data=NULL, int32 LastExtreme=0, class FRandomStream* InRandomStream = NULL);

		/**
		 * Get the min and max values
		 */
		void GetOutRange(float& MinOut, float& MaxOut);

		/**
		 * Is this distribution a uniform type? (ie, does it have two values per entry?)
		 */
		inline bool IsUniform() { return LookupTable.SubEntryStride != 0; }
	
};

UCLASS(abstract, customconstructor)
class ENGINE_API UDistributionVector : public UDistribution
{
	GENERATED_UCLASS_BODY()

	/** Can this variable be baked out to a FRawDistribution? Should be true 99% of the time*/
	UPROPERTY(EditAnywhere, Category=Baked)
	uint32 bCanBeBaked:1;

	/** Set internally when the distribution is updated so that that FRawDistribution can know to update itself*/
	UPROPERTY()
	uint32 bIsDirty:1;

	/** Script-accessible way to query a FVector distribution */
	virtual FVector GetVectorValue(float F = 0);


	UDistributionVector(const class FPostConstructInitializeProperties& PCIP)
	:	Super(PCIP)
	,   bCanBeBaked(true)
	,   bIsDirty(true) // make sure the FRawDistribution is initialized
	{
	}

	//@todo.CONSOLE: Currently, consoles need this? At least until we have some sort of cooking/packaging step!
	/**
	 * Return the operation used at runtime to calculate the final value
	 */
	virtual ERawDistributionOperation GetOperation() const { return RDO_None; }

	/**
	 * Returns the lock axes flag used at runtime to swizzle random stream values. 
	 */
	virtual uint8 GetLockFlag() const { return 0; }
	
	/**
	 * Fill out an array of vectors and return the number of elements in the entry
	 *
	 * @param Time The time to evaluate the distribution
	 * @param Values An array of values to be filled out, guaranteed to be big enough for 2 vectors
	 * @return The number of elements (values) set in the array
	 */
	virtual uint32 InitializeRawEntry(float Time, float* Values) const;

	virtual FVector	GetValue( float F = 0.f, UObject* Data = NULL, int32 LastExtreme = 0, class FRandomStream* InRandomStream = NULL ) const;

	// Begin FCurveEdInterface Interface
	virtual void GetInRange(float& MinIn, float& MaxIn) const;
	virtual void GetOutRange(float& MinOut, float& MaxOut) const;
	virtual	void GetRange(FVector& OutMin, FVector& OutMax) const;
	// Begin FCurveEdInterface Interface

	/** @return true of this distribution can be baked into a FRawDistribution lookup table, otherwise false */
	virtual bool CanBeBaked() const 
	{
		return true; 
	}

	/**
	 * Returns the number of values in the distribution. 3 for vector.
	 */
	int32 GetValueCount() const
	{
		return 3;
	}

	/** Begin UObject interface */
#if	WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	virtual bool NeedsLoadForClient() const OVERRIDE;
	virtual bool NeedsLoadForServer() const OVERRIDE;
	/** End UObject interface */

};

