// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "DistributionFloat.generated.h"

/** Type-safe floating point distribution. */
#if !CPP      //noexport struct
USTRUCT(noexport)
struct FFloatDistribution
{
	UPROPERTY()
	FDistributionLookupTable Table;

};
#endif

USTRUCT()
struct FRawDistributionFloat : public FRawDistribution
{
	GENERATED_USTRUCT_BODY()

private:
	UPROPERTY()
	float MinValue;

	UPROPERTY()
	float MaxValue;

public:
	UPROPERTY(EditAnywhere, export, noclear, Category=RawDistributionFloat)
	class UDistributionFloat* Distribution;


	FRawDistributionFloat()
		: MinValue(0)
		, MaxValue(0)
		, Distribution(NULL)
	{
	}


	#if WITH_EDITOR
		/**`
		 * Initialize a raw distribution from the original Unreal distribution
		 */
		void Initialize();
	#endif
			 
		/**
		 * Gets a pointer to the raw distribution if you can just call FRawDistribution::GetValue1 on it, otherwise NULL 
		 */
		const FRawDistribution* GetFastRawDistribution();

		/**
		 * Get the value at the specified F
		 */
		ENGINE_API float GetValue(float F=0.0f, UObject* Data=NULL, class FRandomStream* InRandomStream = NULL);

		/**
		 * Get the min and max values
		 */
		void GetOutRange(float& MinOut, float& MaxOut);

		/**
		 * Is this distribution a uniform type? (ie, does it have two values per entry?)
		 */
		inline bool IsUniform() { return LookupTable.SubEntryStride != 0; }
	
};

UCLASS(abstract, customconstructor,MinimalAPI)
class UDistributionFloat : public UDistribution
{
	GENERATED_UCLASS_BODY()

	/** Can this variable be baked out to a FRawDistribution? Should be true 99% of the time*/
	UPROPERTY(EditAnywhere, Category=Baked)
	uint32 bCanBeBaked:1;

	/** Set internally when the distribution is updated so that that FRawDistribution can know to update itself*/
	uint32 bIsDirty:1;

	/** Script-accessible way to query a float distribution */
	virtual float GetFloatValue(float F = 0);


	UDistributionFloat(const class FPostConstructInitializeProperties& PCIP)
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
	 *  Returns the lock axes flag used at runtime to swizzle random stream values. Not used for distributions derived from UDistributionFloat.
	 */
	virtual uint8 GetLockFlag() const { return 0; }

	/**
	 * Fill out an array of floats and return the number of elements in the entry
	 *
	 * @param Time The time to evaluate the distribution
	 * @param Values An array of values to be filled out, guaranteed to be big enough for 4 values
	 * @return The number of elements (values) set in the array
	 */
	virtual uint32 InitializeRawEntry(float Time, float* Values) const;

	/** @todo document */
	virtual float GetValue( float F = 0.f, UObject* Data = NULL, class FRandomStream* InRandomStream = NULL ) const;

	// Begin FCurveEdInterface Interface
	virtual void GetInRange(float& MinIn, float& MaxIn) const OVERRIDE;
	virtual void GetOutRange(float& MinOut, float& MaxOut) const OVERRIDE;
	// End FCurveEdInterface Interface
	
	/** @return true of this distribution can be baked into a FRawDistribution lookup table, otherwise false */
	virtual bool CanBeBaked() const 
	{
		return true; 
	}

	/**
	 * Returns the number of values in the distribution. 1 for float.
	 */
	int32 GetValueCount() const
	{
		return 1;
	}

	/** Begin UObject interface */
#if	WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif	// WITH_EDITOR
	virtual bool NeedsLoadForClient() const OVERRIDE;
	virtual bool NeedsLoadForServer() const OVERRIDE;
	/** End UObject interface */
};

