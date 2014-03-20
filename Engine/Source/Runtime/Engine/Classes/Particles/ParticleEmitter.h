// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// ParticleEmitter
// The base class for any particle emitter objects.
//=============================================================================

#pragma once
#include "ParticleEmitter.generated.h"

//=============================================================================
//	Burst emissions
//=============================================================================
UENUM()
enum EParticleBurstMethod
{
	EPBM_Instant,
	EPBM_Interpolated,
	EPBM_MAX,
};

//=============================================================================
//	SubUV-related
//=============================================================================
UENUM()
enum EParticleSubUVInterpMethod
{
	PSUVIM_None,
	PSUVIM_Linear,
	PSUVIM_Linear_Blend,
	PSUVIM_Random,
	PSUVIM_Random_Blend,
	PSUVIM_MAX,
};

//=============================================================================
//	Cascade-related
//=============================================================================
UENUM()
enum EEmitterRenderMode
{
	ERM_Normal,
	ERM_Point,
	ERM_Cross,
	ERM_None,
	ERM_MAX,
};

USTRUCT()
struct FParticleBurst
{
	GENERATED_USTRUCT_BODY()

	/** The number of particles to burst */
	UPROPERTY(EditAnywhere, Category=ParticleBurst)
	int32 Count;

	/** If >= 0, use as a range [CountLow..Count] */
	UPROPERTY(EditAnywhere, Category=ParticleBurst)
	int32 CountLow;

	/** The time at which to burst them (0..1: emitter lifetime) */
	UPROPERTY(EditAnywhere, Category=ParticleBurst)
	float Time;



		FParticleBurst()
		: Count(0)
		, CountLow(-1)		// Disabled by default...
		, Time(0.0f)
		{
		}
	
};

UCLASS(HeaderGroup=Particle, dependson=UParticleLODLevel, hidecategories=Object, editinlinenew, abstract, MinimalAPI)
class UParticleEmitter : public UObject
{
	GENERATED_UCLASS_BODY()

	//=============================================================================
	//	General variables
	//=============================================================================
	/** The name of the emitter. */
	UPROPERTY(EditAnywhere, Category=Particle)
	FName EmitterName;

	UPROPERTY(transient)
	int32 SubUVDataOffset;

	/**
	 *	How to render the emitter particles. Can be one of the following:
	 *		ERM_Normal	- As the intended sprite/mesh
	 *		ERM_Point	- As a 2x2 pixel block with no scaling and the color set in EmitterEditorColor
	 *		ERM_Cross	- As a cross of lines, scaled to the size of the particle in EmitterEditorColor
	 *		ERM_None	- Do not render
	 */
	UPROPERTY(EditAnywhere, Category=Cascade)
	TEnumAsByte<enum EEmitterRenderMode> EmitterRenderMode;

#if WITH_EDITORONLY_DATA
	/**
	 *	The color of the emitter in the curve editor and debug rendering modes.
	 */
	UPROPERTY(EditAnywhere, Category=Cascade)
	FColor EmitterEditorColor;

#endif // WITH_EDITORONLY_DATA
	//=============================================================================
	//	'Private' data - not required by the editor
	//=============================================================================
	UPROPERTY(instanced)
	TArray<class UParticleLODLevel*> LODLevels;

	UPROPERTY()
	uint32 ConvertedModules:1;

	UPROPERTY()
	int32 PeakActiveParticles;

	//=============================================================================
	//	Performance/LOD Data
	//=============================================================================
	
	/**
	 *	Initial allocation count - overrides calculated peak count if > 0
	 */
	UPROPERTY(EditAnywhere, Category=Particle)
	int32 InitialAllocationCount;

	/** 
	 * Scales the spawn rate of this emitter when the engine is running in medium or low detail mode.
	 * This can be used to optimize particle draw cost in splitscreen.
	 * A value of 0 effectively disables this emitter outside of high detail mode,
	 * And this does not affect spawn per unit, unless the value is 0.
	 */
	UPROPERTY(EditAnywhere, Category=Particle)
	float MediumDetailSpawnRateScale;

	/** If detail mode is >= system detail mode, primitive won't be rendered. */
	UPROPERTY(EditAnywhere, Category=Particle)
	TEnumAsByte<enum EDetailMode> DetailMode;

#if WITH_EDITORONLY_DATA
	/** This value indicates the emitter should be drawn 'collapsed' in Cascade */
	UPROPERTY(EditAnywhere, Category=Cascade)
	uint32 bCollapsed:1;
#endif // WITH_EDITORONLY_DATA

	/** If true, then show only this emitter in the editor */
	UPROPERTY(transient)
	uint32 bIsSoloing:1;

	/** 
	 *	If true, then this emitter was 'cooked out' by the cooker. 
	 *	This means it was completely disabled, but to preserve any
	 *	indexing schemes, it is left in place.
	 */
	UPROPERTY()
	uint32 bCookedOut:1;


	// Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	virtual void PostLoad() OVERRIDE;
	// End UObject Interface

	// @todo document
	virtual FParticleEmitterInstance* CreateInstance(UParticleSystemComponent* InComponent);

	// Sets up this emitter with sensible defaults so we can see some particles as soon as its created.
	virtual void SetToSensibleDefaults() {}

	// @todo document
	virtual void UpdateModuleLists();

	// @todo document
	ENGINE_API void SetEmitterName(FName Name);

	// @todo document
	ENGINE_API FName& GetEmitterName();

	// @todo document
	virtual	void						SetLODCount(int32 LODCount);

	// For Cascade
	// @todo document
	void	AddEmitterCurvesToEditor(UInterpCurveEdSetup* EdSetup);

	// @todo document
	ENGINE_API void	RemoveEmitterCurvesFromEditor(UInterpCurveEdSetup* EdSetup);

	// @todo document
	ENGINE_API void	ChangeEditorColor(FColor& Color, UInterpCurveEdSetup* EdSetup);

	// @todo document
	void	AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp);

	/** CreateLODLevel
	 *	Creates the given LOD level.
	 *	Intended for editor-time usage.
	 *	Assumes that the given LODLevel will be in the [0..100] range.
	 *	
	 *	@return The index of the created LOD level
	 */
	ENGINE_API int32		CreateLODLevel(int32 LODLevel, bool bGenerateModuleData = true);

	/** IsLODLevelValid
	 *	Returns true if the given LODLevel is one of the array entries.
	 *	Intended for editor-time usage.
	 *	Assumes that the given LODLevel will be in the [0..(NumLODLevels - 1)] range.
	 *	
	 *	@return false if the requested LODLevel is not valid.
	 *			true if the requested LODLevel is valid.
	 */
	ENGINE_API bool	IsLODLevelValid(int32 LODLevel);

	/** GetCurrentLODLevel
	*	Returns the currently set LODLevel. Intended for game-time usage.
	*	Assumes that the given LODLevel will be in the [0..# LOD levels] range.
	*	
	*	@return NULL if the requested LODLevel is not valid.
	*			The pointer to the requested UParticleLODLevel if valid.
	*/
	FORCEINLINE UParticleLODLevel* GetCurrentLODLevel(FParticleEmitterInstance* Instance)
	{
		if (!FPlatformProperties::HasEditorOnlyData())
		{
			return Instance->CurrentLODLevel;
		}
		else
		{
			// for the game (where we care about perf) we don't branch
			if (Instance->GetWorld()->IsGameWorld() )
			{
				return Instance->CurrentLODLevel;
			}
			else
			{
				EditorUpdateCurrentLOD( Instance );
				return Instance->CurrentLODLevel;
			}
		}
	}

	/**
	 * This will update the LOD of the particle in the editor.
	 *
	 * @see GetCurrentLODLevel(FParticleEmitterInstance* Instance)
	 */
	ENGINE_API void EditorUpdateCurrentLOD(FParticleEmitterInstance* Instance);

	/** GetLODLevel
	 *	Returns the given LODLevel. Intended for game-time usage.
	 *	Assumes that the given LODLevel will be in the [0..# LOD levels] range.
	 *	
	 *	@param	LODLevel - the requested LOD level in the range [0..# LOD levels].
	 *
	 *	@return NULL if the requested LODLevel is not valid.
	 *			The pointer to the requested UParticleLODLevel if valid.
	 */
	ENGINE_API UParticleLODLevel*	GetLODLevel(int32 LODLevel);
	
	/**
	 *	Autogenerate the lowest LOD level...
	 *
	 *	@param	bDuplicateHighest	If true, make the level an exact copy of the highest
	 *
	 *	@return	bool				true if successful, false if not.
	 */
	virtual bool		AutogenerateLowestLODLevel(bool bDuplicateHighest = false);
	
	/**
	 *	CalculateMaxActiveParticleCount
	 *	Determine the maximum active particles that could occur with this emitter.
	 *	This is to avoid reallocation during the life of the emitter.
	 *
	 *	@return	true	if the number was determined
	 *			false	if the number could not be determined
	 */
	virtual bool		CalculateMaxActiveParticleCount();

	/**
	 *	Retrieve the parameters associated with this particle system.
	 *
	 *	@param	ParticleSysParamList	The list of FParticleSysParams used in the system
	 *	@param	ParticleParameterList	The list of ParticleParameter distributions used in the system
	 */
	void GetParametersUtilized(TArray<FString>& ParticleSysParamList,
							   TArray<FString>& ParticleParameterList);

	/**
	 * Builds data needed for simulation by the emitter from all modules.
	 */
	void Build();
};



