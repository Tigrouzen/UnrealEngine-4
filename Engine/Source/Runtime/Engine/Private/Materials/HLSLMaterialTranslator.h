// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
/*=============================================================================
	HLSLMaterialTranslator.h: Translates material expressions into HLSL code.
=============================================================================*/

#pragma once

#if WITH_EDITORONLY_DATA

#include "EnginePrivate.h"
#include "MaterialCompiler.h"
#include "MaterialUniformExpressions.h"
#include "ParameterCollection.h"

/** @return the number of components in a vector type. */
uint32 GetNumComponents(EMaterialValueType Type)
{
	switch(Type)
	{
		case MCT_Float:
		case MCT_Float1: return 1;
		case MCT_Float2: return 2;
		case MCT_Float3: return 3;
		case MCT_Float4: return 4;
		default: return 0;
	}
}

/** @return the vector type containing a given number of components. */
EMaterialValueType GetVectorType(uint32 NumComponents)
{
	switch(NumComponents)
	{
		case 1: return MCT_Float;
		case 2: return MCT_Float2;
		case 3: return MCT_Float3;
		case 4: return MCT_Float4;
		default: return MCT_Unknown;
	};
}

struct FShaderCodeChunk
{
	/** 
	 * Definition string of the code chunk. 
	 * If !bInline && !UniformExpression || UniformExpression->IsConstant(), this is the definition of a local variable named by SymbolName.
	 * Otherwise if bInline || (UniformExpression && UniformExpression->IsConstant()), this is a code expression that needs to be inlined.
	 */
	FString Definition;
	/** 
	 * Name of the local variable used to reference this code chunk. 
	 * If bInline || UniformExpression, there will be no symbol name and Definition should be used directly instead.
	 */
	FString SymbolName;
	/** Reference to a uniform expression, if this code chunk has one. */
	TRefCountPtr<FMaterialUniformExpression> UniformExpression;
	EMaterialValueType Type;
	/** Whether the code chunk should be inlined or not.  If true, SymbolName is empty and Definition contains the code to inline. */
	bool bInline;

	/** Ctor for creating a new code chunk with no associated uniform expression. */
	FShaderCodeChunk(const TCHAR* InDefinition,const FString& InSymbolName,EMaterialValueType InType,bool bInInline):
		Definition(InDefinition),
		SymbolName(InSymbolName),
		UniformExpression(NULL),
		Type(InType),
		bInline(bInInline)
	{}

	/** Ctor for creating a new code chunk with a uniform expression. */
	FShaderCodeChunk(FMaterialUniformExpression* InUniformExpression,const TCHAR* InDefinition,EMaterialValueType InType):
		Definition(InDefinition),
		UniformExpression(InUniformExpression),
		Type(InType),
		bInline(false)
	{}
};

// to avoid limits with the Printf parameter count, for more readability and type safety
class FLazyPrintf
{
public:
	// constructor
	FLazyPrintf(const TCHAR* InputWithPercentS)
		: CurrentInputPos(InputWithPercentS)
	{
		// to avoid reallocations
		CurrentState.Empty(50 * 1024);
	}

	FString GetResultString()
	{
		// internal error more %s than %s in MaterialTemplate.usf
		check(!ProcessUntilPercentS());

		// copy all remaining input data
		CurrentState += CurrentInputPos;

		return CurrentState;
	}

	// %s
	void PushParam(const TCHAR* Data)
	{
		if(ProcessUntilPercentS())
		{
			CurrentState += Data;
		}
		else
		{
			// internal error, more ReplacePercentS() calls than %s in MaterialTemplate.usf
			check(0);
		}
	}

private:

	// @param Pattern e.g. TEXT("%s")
	bool ProcessUntilPercentS()
	{
		const TCHAR* Found = FCString::Strstr(CurrentInputPos, TEXT("%s"));

		if(Found == 0)
		{
			return false;
		}

		// copy from input until %s
		while(CurrentInputPos < Found)
		{
			// can cause reallocations we could avoid
			CurrentState += *CurrentInputPos++;
		}

		// jump over %s
		CurrentInputPos += 2;

		return true;
	}

	const TCHAR* CurrentInputPos;
	FString CurrentState;
};

class FHLSLMaterialTranslator : public FMaterialCompiler
{
protected:

	/** The shader frequency of the current material property being compiled. */
	EShaderFrequency ShaderFrequency;
	/** The current material property being compiled.  This affects the behavior of all compiler functions except GetFixedParameterCode. */
	EMaterialProperty MaterialProperty;
	/** Material being compiled.  Only transient compilation output like error information can be stored on the FMaterial. */
	FMaterial* Material;
	/** Compilation output which will be stored in the DDC. */
	FMaterialCompilationOutput& MaterialCompilationOutput;
	FStaticParameterSet StaticParameters;
	EShaderPlatform Platform;
	/** Quality level being compiled for. */
	EMaterialQualityLevel::Type QualityLevel;

	/** Feature level being compiled for. */
	ERHIFeatureLevel::Type FeatureLevel;

	/** Code chunk definitions corresponding to each of the material inputs, only initialized after Translate has been called. */
	FString TranslatedCodeChunkDefinitions[CompiledMP_MAX];

	/** Code chunks corresponding to each of the material inputs, only initialized after Translate has been called. */
	FString TranslatedCodeChunks[CompiledMP_MAX];

	/** Line number of the #line in MaterialTemplate.usf */
	int32 MaterialTemplateLineNumber;

	/** Stores the resource declarations */
	FString ResourcesString;
	
	/** Contents of the MaterialTemplate.usf file */
	FString MaterialTemplate;

	// Array of code chunks per material property
	TArray<FShaderCodeChunk> CodeChunks[MP_MAX][SF_NumFrequencies];

	// Uniform expressions used across all material properties
	TArray<FShaderCodeChunk> UniformExpressions;

	// Stack that tracks compiler state specific to each function being compiled
	TArray<FMaterialFunctionCompileState> FunctionStack;

	/** Parameter collections referenced by this material.  The position in this array is used as an index on the shader parameter. */
	TArray<UMaterialParameterCollection*> ParameterCollections;

	// Index of the next symbol to create
	int32 NextSymbolIndex;

	/** Any custom expression function implementations */
	TArray<FString> CustomExpressionImplementations;

	/** Whether the translation succeeded. */
	uint32 bSuccess : 1;
	/** Whether the compute shader material inputs were compiled. */
	uint32 bCompileForComputeShader : 1;
	/** Whether the compiled material uses scene depth. */
	uint32 bUsesSceneDepth : 1;
	/** true if the material needs particle position. */
	uint32 bNeedsParticlePosition : 1;
	/** true if the material needs particle velocity. */
	uint32 bNeedsParticleVelocity : 1;
	/** true of the material uses a particle dynamic parameter */
	uint32 bNeedsParticleDynamicParameter : 1;
	/** true if the material needs particle relative time. */
	uint32 bNeedsParticleTime : 1;
	/** true if the material uses particle motion blur. */
	uint32 bUsesParticleMotionBlur : 1;
	/** true if the material uses spherical particle opacity. */
	uint32 bUsesSphericalParticleOpacity : 1;
	/** true if the material uses particle sub uvs. */
	uint32 bUsesParticleSubUVs : 1;
	/** Boolean indicating using LightmapUvs */
	uint32 bUsesLightmapUVs : 1;
	/** true if needs SpeedTree code */
	uint32 bUsesSpeedTree : 1;
	/** Boolean indicating the material uses worldspace position without shader offsets applied */
	uint32 bNeedsWorldPositionExcludingShaderOffsets : 1;
	/** true if the material needs particle size. */
	uint32 bNeedsParticleSize : 1;
	/** true if any scene texture expressions are reading from post process inputs */
	uint32 bNeedsSceneTexturePostProcessInputs : 1;
	/** true if any atmospheric fog expressions are used */
	uint32 bUsesAtmosphericFog : 1;
	/** true if the material reads vertex color in the pixel shader. */
	uint32 bUsesVertexColor : 1;
	/** true if the material reads particle color in the pixel shader. */
	uint32 bUsesParticleColor : 1;
	uint32 bUsesTransformVector : 1;
	/** Tracks the number of texture coordinates used by this material. */
	uint32 NumUserTexCoords;
	/** Tracks the number of texture coordinates used by the vertex shader in this material. */
	uint32 NumUserVertexTexCoords;

public: 

	FHLSLMaterialTranslator(FMaterial* InMaterial,
		FMaterialCompilationOutput& InMaterialCompilationOutput,
		const FStaticParameterSet& InStaticParameters,
		EShaderPlatform InPlatform,
		EMaterialQualityLevel::Type InQualityLevel,
		ERHIFeatureLevel::Type InFeatureLevel)
	:	ShaderFrequency(SF_Pixel)
	,	MaterialProperty(MP_EmissiveColor)
	,	Material(InMaterial)
	,	MaterialCompilationOutput(InMaterialCompilationOutput)
	,	StaticParameters(InStaticParameters)
	,	Platform(InPlatform)
	,	QualityLevel(InQualityLevel)
	,	FeatureLevel(InFeatureLevel)
	,	MaterialTemplateLineNumber(INDEX_NONE)
	,	NextSymbolIndex(INDEX_NONE)
	,	bSuccess(false)
	,	bCompileForComputeShader(false)
	,	bUsesSceneDepth(false)
	,	bNeedsParticlePosition(false)
	,	bNeedsParticleVelocity(false)
	,	bNeedsParticleDynamicParameter(false)
	,	bNeedsParticleTime(false)
	,	bUsesParticleMotionBlur(false)
	,	bUsesSphericalParticleOpacity(false)
	,   bUsesParticleSubUVs(false)
	,	bUsesLightmapUVs(false)
	,	bUsesSpeedTree(false)
	,	bNeedsWorldPositionExcludingShaderOffsets(false)
	,	bNeedsParticleSize(false)
	,	bNeedsSceneTexturePostProcessInputs(false)
	,	bUsesVertexColor(false)
	,	bUsesParticleColor(false)
	,	bUsesTransformVector(false)
	,	NumUserTexCoords(0)
	,	NumUserVertexTexCoords(0)
	{}
 
	bool Translate()
	{
		STAT(double HLSLTranslateTime = 0);
		{
			SCOPE_SECONDS_COUNTER(HLSLTranslateTime);
			bSuccess = true;

			// WARNING: No compile outputs should be stored on the UMaterial / FMaterial / FMaterialResource, unless they are transient editor-only data (like error expressions)
			// Compile outputs that need to be saved must be stored in MaterialCompilationOutput, which will be saved to the DDC.

			Material->CompileErrors.Empty();
			Material->ErrorExpressions.Empty();

			MaterialCompilationOutput.bUsesSceneColor = false;
			MaterialCompilationOutput.bNeedsSceneTextures = false;
			MaterialCompilationOutput.bUsesEyeAdaptation = false;

			// Add a state item for the root level
			FunctionStack.Add(FMaterialFunctionCompileState(NULL));

			bCompileForComputeShader = Material->IsLightFunction();

			// Generate code
			int32 Chunk[CompiledMP_MAX];

			Chunk[MP_Normal]						= ForceCast(Material->CompileProperty(MP_Normal                ,GetMaterialPropertyShaderFrequency(MP_Normal),this),MCT_Float3);
			Chunk[MP_EmissiveColor]					= ForceCast(Material->CompileProperty(MP_EmissiveColor         ,GetMaterialPropertyShaderFrequency(MP_EmissiveColor),this),MCT_Float3);
			Chunk[MP_DiffuseColor]					= ForceCast(Material->CompileProperty(MP_DiffuseColor          ,GetMaterialPropertyShaderFrequency(MP_DiffuseColor),this),MCT_Float3);
			Chunk[MP_SpecularColor]					= ForceCast(Material->CompileProperty(MP_SpecularColor         ,GetMaterialPropertyShaderFrequency(MP_SpecularColor),this),MCT_Float3);
			Chunk[MP_BaseColor]						= ForceCast(Material->CompileProperty(MP_BaseColor             ,GetMaterialPropertyShaderFrequency(MP_BaseColor),this),MCT_Float3);
			Chunk[MP_Metallic]						= ForceCast(Material->CompileProperty(MP_Metallic              ,GetMaterialPropertyShaderFrequency(MP_Metallic),this),MCT_Float1);
			Chunk[MP_Specular]						= ForceCast(Material->CompileProperty(MP_Specular              ,GetMaterialPropertyShaderFrequency(MP_Specular),this),MCT_Float1);
			Chunk[MP_Roughness]						= ForceCast(Material->CompileProperty(MP_Roughness             ,GetMaterialPropertyShaderFrequency(MP_Roughness),this),MCT_Float1);
			Chunk[MP_Opacity]						= ForceCast(Material->CompileProperty(MP_Opacity               ,GetMaterialPropertyShaderFrequency(MP_Opacity),this),MCT_Float1);
			Chunk[MP_OpacityMask]					= ForceCast(Material->CompileProperty(MP_OpacityMask           ,GetMaterialPropertyShaderFrequency(MP_OpacityMask),this),MCT_Float1);
			Chunk[MP_WorldPositionOffset]			= ForceCast(Material->CompileProperty(MP_WorldPositionOffset   ,GetMaterialPropertyShaderFrequency(MP_WorldPositionOffset),this),MCT_Float3);
			if (FeatureLevel >= ERHIFeatureLevel::SM5)
			{
				Chunk[MP_WorldDisplacement]			= ForceCast(Material->CompileProperty(MP_WorldDisplacement     ,GetMaterialPropertyShaderFrequency(MP_WorldDisplacement),this),MCT_Float3);
			}
			else
			{
				SetMaterialProperty(MP_WorldDisplacement, GetMaterialPropertyShaderFrequency(MP_WorldDisplacement));
				Chunk[MP_WorldDisplacement]			= ForceCast(Constant3(0.0f,0.0f,0.0f),MCT_Float3);
			}
			Chunk[MP_TessellationMultiplier]		= ForceCast(Material->CompileProperty(MP_TessellationMultiplier,GetMaterialPropertyShaderFrequency(MP_TessellationMultiplier),this),MCT_Float1);
			Chunk[MP_SubsurfaceColor]				= ForceCast(Material->CompileProperty(MP_SubsurfaceColor       ,GetMaterialPropertyShaderFrequency(MP_SubsurfaceColor),this),MCT_Float3);
			Chunk[MP_AmbientOcclusion]				= ForceCast(Material->CompileProperty(MP_AmbientOcclusion      ,GetMaterialPropertyShaderFrequency(MP_AmbientOcclusion),this),MCT_Float1);
			Chunk[MP_Refraction]					= ForceCast(Material->CompileProperty(MP_Refraction            ,GetMaterialPropertyShaderFrequency(MP_Refraction),this),MCT_Float2);

			if (bCompileForComputeShader)
			{
				Chunk[CompiledMP_EmissiveColorCS]		= ForceCast(Material->CompileProperty(MP_EmissiveColor     ,SF_Compute,this),MCT_Float3);
			}

			for (uint32 CustomUVIndex = MP_CustomizedUVs0; CustomUVIndex <= MP_CustomizedUVs7; CustomUVIndex++)
			{
				// Only compile custom UV inputs for UV channels requested by the pixel shader inputs
				// Any unconnected inputs will have a texcoord generated for them in Material->CompileProperty, which will pass through the vertex (uncustomized) texture coordinates
				// Note: this is using NumUserTexCoords, which is set by translating all the pixel properties above
				if (CustomUVIndex - MP_CustomizedUVs0 < NumUserTexCoords)
				{
					Chunk[CustomUVIndex] = ForceCast(Material->CompileProperty((EMaterialProperty)CustomUVIndex, GetMaterialPropertyShaderFrequency((EMaterialProperty)CustomUVIndex), this), MCT_Float2);
				}
				else
				{
					Chunk[CustomUVIndex] = INDEX_NONE;
				}
			}

			check(FunctionStack.Num() == 1);

			if (Material->GetBlendMode() == BLEND_Modulate && Material->GetLightingModel() != MLM_Unlit && !Material->IsUsedWithDeferredDecal())
			{
				Errorf(TEXT("Dynamically lit translucency is not supported for BLEND_Modulate materials."));
			}

			if (Material->GetMaterialDomain() == MD_Surface)
			{
				if (Material->GetBlendMode() == BLEND_Modulate && Material->IsSeparateTranslucencyEnabled())
				{
					Errorf(TEXT("Separate translucency with BLEND_Modulate is not supported. Consider using BLEND_Translucent with black emissive"));
				}
			}

			// Don't allow opaque and masked materials to scene depth as the results are undefined
			if (bUsesSceneDepth && Material->GetMaterialDomain() != MD_PostProcess && !IsTranslucentBlendMode(Material->GetBlendMode()))
			{
				Errorf(TEXT("Only transparent or postprocess materials can read from scene depth."));
			}

			if (MaterialCompilationOutput.bUsesSceneColor && Material->GetMaterialDomain() != MD_PostProcess && !IsTranslucentBlendMode(Material->GetBlendMode()))
			{
				Errorf(TEXT("Only transparent or postprocess materials can read from scene color."));
			}

			if (Material->IsLightFunction() && Material->GetBlendMode() != BLEND_Opaque)
			{
				Errorf(TEXT("Light function materials must be opaque."));
			}

			if (Material->IsLightFunction() && Material->GetLightingModel() != MLM_Unlit)
			{
				Errorf(TEXT("Light function materials must use unlit."));
			}

			if (Material->GetMaterialDomain() == MD_PostProcess && Material->GetLightingModel() != MLM_Unlit)
			{
				Errorf(TEXT("Post process materials must use unlit."));
			}

			if (MaterialCompilationOutput.bNeedsSceneTextures)
			{
				if (Material->GetMaterialDomain() != MD_PostProcess)
				{
					if (Material->GetBlendMode() == BLEND_Opaque || Material->GetBlendMode() == BLEND_Masked)
					{
						// In opaque pass, none the the textures are available
						Errorf(TEXT("SceneTexture expressions cannot be used in opaque materials"));
					}
					else if (bNeedsSceneTexturePostProcessInputs)
					{
						Errorf(TEXT("SceneTexture expressions cannot use post process inputs in non post process domain materials"));
					}
				}
			}

			ResourcesString = TEXT("");
			MaterialCompilationOutput.UniformExpressionSet.GetResourcesString(Platform,ResourcesString);

			// Output the implementation for any custom expressions we will call below.
			for(int32 ExpressionIndex = 0;ExpressionIndex < CustomExpressionImplementations.Num();ExpressionIndex++)
			{
				ResourcesString += CustomExpressionImplementations[ExpressionIndex] + "\r\n\r\n";
			}

			for(uint32 PropertyId = 0; PropertyId < MP_MAX; ++PropertyId)
			{
				if(PropertyId == MP_MaterialAttributes )
				{
					continue;
				}

				GetFixedParameterCode(
					Chunk[PropertyId], 
					(EMaterialProperty)PropertyId, GetMaterialPropertyShaderFrequency((EMaterialProperty)PropertyId),
					TranslatedCodeChunkDefinitions[PropertyId],
					TranslatedCodeChunks[PropertyId]);
			}

			if (bCompileForComputeShader)
			{
				for(uint32 PropertyId = MP_MAX; PropertyId < CompiledMP_MAX; ++PropertyId)
				{
					GetFixedParameterCode(Chunk[PropertyId], MP_EmissiveColor, SF_Compute, TranslatedCodeChunkDefinitions[PropertyId], TranslatedCodeChunks[PropertyId]);
				}
			}

			LoadShaderSourceFileChecked(TEXT("MaterialTemplate"), MaterialTemplate);

			// Find the string index of the '#line' statement in MaterialTemplate.usf
			const int32 LineIndex = MaterialTemplate.Find(TEXT("#line"), ESearchCase::CaseSensitive);
			check(LineIndex != INDEX_NONE);

			// Count line endings before the '#line' statement
			MaterialTemplateLineNumber = INDEX_NONE;
			int32 StartPosition = LineIndex + 1;
			do 
			{
				MaterialTemplateLineNumber++;
				// Using \n instead of LINE_TERMINATOR as not all of the lines are terminated consistently
				// Subtract one from the last found line ending index to make sure we skip over it
				StartPosition = MaterialTemplate.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, StartPosition - 1);
			} 
			while (StartPosition != INDEX_NONE);
			check(MaterialTemplateLineNumber != INDEX_NONE);
			// At this point MaterialTemplateLineNumber is one less than the line number of the '#line' statement
			// For some reason we have to add 2 more to the #line value to get correct error line numbers from D3DXCompileShader
			MaterialTemplateLineNumber += 3;

			MaterialCompilationOutput.UniformExpressionSet.SetParameterCollections(ParameterCollections);

			// Create the material uniform buffer struct.
			MaterialCompilationOutput.UniformExpressionSet.CreateBufferStruct();
		}
		INC_FLOAT_STAT_BY(STAT_ShaderCompiling_HLSLTranslation,(float)HLSLTranslateTime);

		return bSuccess;
	}

	void GetMaterialEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		if (bNeedsParticlePosition || Material->ShouldGenerateSphericalParticleNormals() || bUsesSphericalParticleOpacity)
		{
			OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_POSITION"), 1);
		}

		if (bNeedsParticleVelocity)
		{
			OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_VELOCITY"), 1);
		}

		if (bNeedsParticleDynamicParameter)
		{
			OutEnvironment.SetDefine(TEXT("USE_DYNAMIC_PARAMETERS"), 1);
		}

		if (bNeedsParticleTime)
		{
			OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_TIME"), 1);
		}

		if (bUsesParticleMotionBlur)
		{
			OutEnvironment.SetDefine(TEXT("USES_PARTICLE_MOTION_BLUR"), 1);
		}

		if (bUsesSphericalParticleOpacity)
		{
			OutEnvironment.SetDefine(TEXT("SPHERICAL_PARTICLE_OPACITY"), TEXT("1"));
		}

		if (bUsesParticleSubUVs)
		{
			OutEnvironment.SetDefine(TEXT("USE_PARTICLE_SUBUVS"), TEXT("1"));
		}

		if (bUsesLightmapUVs)
		{
			OutEnvironment.SetDefine(TEXT("LIGHTMAP_UV_ACCESS"),TEXT("1"));
		}

		if (bUsesSpeedTree)
		{
			OutEnvironment.SetDefine(TEXT("USES_SPEEDTREE"),TEXT("1"));
		}

		if (bNeedsWorldPositionExcludingShaderOffsets)
		{
			OutEnvironment.SetDefine(TEXT("NEEDS_WORLD_POSITION_EXCLUDING_SHADER_OFFSETS"), TEXT("1"));
		}

		if( bNeedsParticleSize )
		{
			OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_SIZE"), TEXT("1"));
		}

		if( MaterialCompilationOutput.bNeedsSceneTextures )
		{
			OutEnvironment.SetDefine(TEXT("NEEDS_SCENE_TEXTURES"), TEXT("1"));
		}
		if( MaterialCompilationOutput.bUsesEyeAdaptation )
		{
			OutEnvironment.SetDefine(TEXT("USES_EYE_ADAPTATION"), TEXT("1"));
		}
		OutEnvironment.SetDefine(TEXT("MATERIAL_ATMOSPHERIC_FOG"), bUsesAtmosphericFog ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("INTERPOLATE_VERTEX_COLOR"), bUsesVertexColor ? TEXT("1") : TEXT("0")); 
		OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_COLOR"), bUsesParticleColor ? TEXT("1") : TEXT("0")); 
		OutEnvironment.SetDefine(TEXT("USES_TRANSFORM_VECTOR"), bUsesTransformVector ? TEXT("1") : TEXT("0")); 

		OutEnvironment.SetDefine(TEXT("ENABLE_TRANSLUCENCY_VERTEX_FOG"), Material->UseTranslucencyVertexFog() ? TEXT("1") : TEXT("0"));

		for (int32 CollectionIndex = 0; CollectionIndex < ParameterCollections.Num(); CollectionIndex++)
		{
			// Add uniform buffer declarations for any parameter collections referenced
			const FString CollectionName = FString::Printf(TEXT("MaterialCollection%u"), CollectionIndex);
			FShaderUniformBufferParameter::ModifyCompilationEnvironment(*CollectionName,ParameterCollections[CollectionIndex]->GetUniformBufferStruct(),Platform,OutEnvironment);
		}
	}

	FString GetMaterialShaderCode()
	{	
		// use "MaterialTemplate.usf" to create the functions to get data (e.g. material attributes) and code (e.g. material expressions to create specular color) from C++
		FLazyPrintf LazyPrintf(*MaterialTemplate);

		LazyPrintf.PushParam(*FString::Printf(TEXT("%u"),NumUserVertexTexCoords));
		LazyPrintf.PushParam(*FString::Printf(TEXT("%u"),NumUserTexCoords));
		LazyPrintf.PushParam(*ResourcesString);
		LazyPrintf.PushParam(*(TranslatedCodeChunkDefinitions[MP_Normal] + TEXT("	return ") + TranslatedCodeChunks[MP_Normal] + TEXT(";")));
		LazyPrintf.PushParam(*(TranslatedCodeChunkDefinitions[MP_EmissiveColor] + TEXT("	return ") + TranslatedCodeChunks[MP_EmissiveColor] + TEXT(";")));
		LazyPrintf.PushParam(bCompileForComputeShader ? *(TranslatedCodeChunkDefinitions[CompiledMP_EmissiveColorCS] + TEXT("	return ") + TranslatedCodeChunks[CompiledMP_EmissiveColorCS] + TEXT(";")) : TEXT("return 0"));
		LazyPrintf.PushParam(*(TranslatedCodeChunkDefinitions[MP_DiffuseColor] + TEXT("	return ") + TranslatedCodeChunks[MP_DiffuseColor] + TEXT(";")));
		LazyPrintf.PushParam(*(TranslatedCodeChunkDefinitions[MP_SpecularColor] + TEXT("	return ") + TranslatedCodeChunks[MP_SpecularColor] + TEXT(";")));
		LazyPrintf.PushParam(*(TranslatedCodeChunkDefinitions[MP_BaseColor] + TEXT("	return ") + TranslatedCodeChunks[MP_BaseColor] + TEXT(";")));
		LazyPrintf.PushParam(*(TranslatedCodeChunkDefinitions[MP_Metallic] + TEXT("	return ") + TranslatedCodeChunks[MP_Metallic] + TEXT(";")));
		LazyPrintf.PushParam(*(TranslatedCodeChunkDefinitions[MP_Specular] + TEXT("	return ") + TranslatedCodeChunks[MP_Specular] + TEXT(";")));
		LazyPrintf.PushParam(*(TranslatedCodeChunkDefinitions[MP_Roughness] + TEXT("	return ") + TranslatedCodeChunks[MP_Roughness] + TEXT(";")));
		LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"),Material->GetTranslucencyDirectionalLightingIntensity()));
		
		LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"),Material->GetTranslucentShadowDensityScale()));
		LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"),Material->GetTranslucentSelfShadowDensityScale()));
		LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"),Material->GetTranslucentSelfShadowSecondDensityScale()));
		LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"),Material->GetTranslucentSelfShadowSecondOpacity()));
		LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"),Material->GetTranslucentBackscatteringExponent()));
		LazyPrintf.PushParam(*FString::Printf(TEXT("return MaterialFloat3(%.5f, %.5f, %.5f)"),
			Material->GetTranslucentMultipleScatteringExtinction().R,
			Material->GetTranslucentMultipleScatteringExtinction().G,
			Material->GetTranslucentMultipleScatteringExtinction().B));
		LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"),Material->GetOpacityMaskClipValue()));
		LazyPrintf.PushParam(*(TranslatedCodeChunkDefinitions[MP_Opacity] + TEXT("	return ") + TranslatedCodeChunks[MP_Opacity] + TEXT(";")));
		LazyPrintf.PushParam(*(TranslatedCodeChunkDefinitions[MP_OpacityMask] + TEXT("	return ") + TranslatedCodeChunks[MP_OpacityMask] + TEXT(";")));
		/*
		LazyPrintf.PushParam(*FString::Printf(TEXT("%s return %s"),
			*GetDefinitions(MP_OpacityMask, GetMaterialPropertyShaderFrequency(MP_OpacityMask)),
			*CodeChunk[MP_OpacityMask]));
		*/
		LazyPrintf.PushParam(*(TranslatedCodeChunkDefinitions[MP_WorldPositionOffset] + TEXT("	return ") + TranslatedCodeChunks[MP_WorldPositionOffset] + TEXT(";")));
		LazyPrintf.PushParam(*(TranslatedCodeChunkDefinitions[MP_WorldDisplacement] + TEXT("	return ") + TranslatedCodeChunks[MP_WorldDisplacement] + TEXT(";")));
		LazyPrintf.PushParam(*(TranslatedCodeChunkDefinitions[MP_TessellationMultiplier] + TEXT("	return ") + TranslatedCodeChunks[MP_TessellationMultiplier] + TEXT(";")));
		LazyPrintf.PushParam(*(TranslatedCodeChunkDefinitions[MP_SubsurfaceColor] + TEXT("	return ") + TranslatedCodeChunks[MP_SubsurfaceColor] + TEXT(";")));
		LazyPrintf.PushParam(*(TranslatedCodeChunkDefinitions[MP_AmbientOcclusion] + TEXT("	return ") + TranslatedCodeChunks[MP_AmbientOcclusion] + TEXT(";")));
		LazyPrintf.PushParam(*(TranslatedCodeChunkDefinitions[MP_Refraction] + TEXT("	return ") + TranslatedCodeChunks[MP_Refraction] + TEXT(";")));

		FString CustomUVAssignments;

		for (uint32 CustomUVIndex = 0; CustomUVIndex < NumUserTexCoords; CustomUVIndex++)
		{
			CustomUVAssignments += FString::Printf(TEXT("%s	OutTexCoords[%u] = %s;")LINE_TERMINATOR, *TranslatedCodeChunkDefinitions[MP_CustomizedUVs0 + CustomUVIndex], CustomUVIndex, *TranslatedCodeChunks[MP_CustomizedUVs0 + CustomUVIndex]);
		}

		LazyPrintf.PushParam(*CustomUVAssignments);
	
		LazyPrintf.PushParam(*FString::Printf(TEXT("%u"),MaterialTemplateLineNumber));

		return LazyPrintf.GetResultString();
	}

protected:

	// GetParameterCode
	virtual FString GetParameterCode(int32 Index, const TCHAR* Default = 0)
	{
		if(Index == INDEX_NONE && Default)
		{
			return Default;
		}

		checkf(Index >= 0 && Index < CodeChunks[MaterialProperty][ShaderFrequency].Num(), TEXT("Index %d/%d, Platform=%d"), Index, CodeChunks[MaterialProperty][ShaderFrequency].Num(), Platform);
		const FShaderCodeChunk& CodeChunk = CodeChunks[MaterialProperty][ShaderFrequency][Index];
		if(CodeChunk.UniformExpression && CodeChunk.UniformExpression->IsConstant() || CodeChunk.bInline)
		{
			// Constant uniform expressions and code chunks which are marked to be inlined are accessed via Definition
			return CodeChunk.Definition;
		}
		else
		{
			if (CodeChunk.UniformExpression)
			{
				// If the code chunk has a uniform expression, create a new code chunk to access it
				const int32 AccessedIndex = AccessUniformExpression(Index);
				const FShaderCodeChunk& AccessedCodeChunk = CodeChunks[MaterialProperty][ShaderFrequency][AccessedIndex];
				if(AccessedCodeChunk.bInline)
				{
					// Handle the accessed code chunk being inlined
					return AccessedCodeChunk.Definition;
				}
				// Return the symbol used to reference this code chunk
				check(AccessedCodeChunk.SymbolName.Len() > 0);
				return AccessedCodeChunk.SymbolName;
			}
			
			// Return the symbol used to reference this code chunk
			check(CodeChunk.SymbolName.Len() > 0);
			return CodeChunk.SymbolName;
		}
	}

	/** Creates a string of all definitions needed for the given material input. */
	FString GetDefinitions(EMaterialProperty InProperty, EShaderFrequency InFrequency) const
	{
		FString Definitions;
		for (int32 ChunkIndex = 0; ChunkIndex < CodeChunks[InProperty][InFrequency].Num(); ChunkIndex++)
		{
			const FShaderCodeChunk& CodeChunk = CodeChunks[InProperty][InFrequency][ChunkIndex];
			// Uniform expressions (both constant and variable) and inline expressions don't have definitions.
			if (!CodeChunk.UniformExpression && !CodeChunk.bInline)
			{
				Definitions += CodeChunk.Definition;
			}
		}
		return Definitions;
	}

	// GetFixedParameterCode
	virtual void GetFixedParameterCode(int32 Index, EMaterialProperty InProperty, EShaderFrequency InFrequency, FString& OutDefinitions, FString& OutValue)
	{
		if(Index != INDEX_NONE)
		{
			checkf(Index >= 0 && Index < CodeChunks[InProperty][InFrequency].Num(), TEXT("Index out of range %d/%d [%s]"), Index, CodeChunks[InProperty][InFrequency].Num(), *this->Material->GetFriendlyName());
			check(!CodeChunks[InProperty][InFrequency][Index].UniformExpression || CodeChunks[InProperty][InFrequency][Index].UniformExpression->IsConstant());
			if (CodeChunks[InProperty][InFrequency][Index].UniformExpression && CodeChunks[InProperty][InFrequency][Index].UniformExpression->IsConstant())
			{
					// Handle a constant uniform expression being the only code chunk hooked up to a material input
					const FShaderCodeChunk& CodeChunk = CodeChunks[InProperty][InFrequency][Index];
				OutValue = CodeChunk.Definition;
				}
				else
				{
					const FShaderCodeChunk& CodeChunk = CodeChunks[InProperty][InFrequency][Index];
					// Combine the definition lines and the return statement
					check(CodeChunk.bInline || CodeChunk.SymbolName.Len() > 0);
				OutDefinitions = GetDefinitions(InProperty, InFrequency);
				OutValue = CodeChunk.bInline ? CodeChunk.Definition : CodeChunk.SymbolName;
			}
		}
		else
		{
			OutValue = TEXT("0");
		}
	}

	/** Used to get a user friendly type from EMaterialValueType */
	const TCHAR* DescribeType(EMaterialValueType Type) const
	{
		switch(Type)
		{
		case MCT_Float1:		return TEXT("float");
		case MCT_Float2:		return TEXT("float2");
		case MCT_Float3:		return TEXT("float3");
		case MCT_Float4:		return TEXT("float4");
		case MCT_Float:			return TEXT("float");
		case MCT_Texture2D:		return TEXT("texture2D");
		case MCT_TextureCube:	return TEXT("textureCube");
		case MCT_StaticBool:	return TEXT("static bool");
		case MCT_MaterialAttributes:	return TEXT("MaterialAttributes");
		default:				return TEXT("unknown");
		};
	}

	/** Used to get an HLSL type from EMaterialValueType */
	const TCHAR* HLSLTypeString(EMaterialValueType Type) const
	{
		switch(Type)
		{
		case MCT_Float1:		return TEXT("MaterialFloat");
		case MCT_Float2:		return TEXT("MaterialFloat2");
		case MCT_Float3:		return TEXT("MaterialFloat3");
		case MCT_Float4:		return TEXT("MaterialFloat4");
		case MCT_Float:			return TEXT("MaterialFloat");
		case MCT_Texture2D:		return TEXT("texture2D");
		case MCT_TextureCube:	return TEXT("textureCube");
		case MCT_StaticBool:	return TEXT("static bool");
		case MCT_MaterialAttributes:	return TEXT("MaterialAttributes");
		default:				return TEXT("unknown");
		};
	}

	int32 NonPixelShaderExpressionError()
	{
		return Errorf(TEXT("Invalid node used in vertex/hull/domain shader input!"));
	}

	int32 ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::Type RequiredFeatureLevel)
	{
		if (FeatureLevel < RequiredFeatureLevel)
		{
			FString FeatureLevelName;
			GetFeatureLevelName(FeatureLevel, FeatureLevelName);
			return Errorf(TEXT("Node not supported in feature level %s"), *FeatureLevelName);
		}

		return 0;
	}

	int32 NonVertexShaderExpressionError()
	{
		return Errorf(TEXT("Invalid node used in pixel/hull/domain shader input!"));
	}

	int32 NonVertexOrPixelShaderExpressionError()
	{
		return Errorf(TEXT("Invalid node used in hull/domain shader input!"));
	}

	/** Creates a unique symbol name and adds it to the symbol list. */
	FString CreateSymbolName(const TCHAR* SymbolNameHint)
	{
		NextSymbolIndex++;
		return FString(SymbolNameHint) + FString::FromInt(NextSymbolIndex);
	}

	/** Adds an already formatted inline or referenced code chunk */
	int32 AddCodeChunkInner(const TCHAR* FormattedCode,EMaterialValueType Type,bool bInlined)
	{
		if (Type == MCT_Unknown)
		{
			return INDEX_NONE;
		}

		if (bInlined)
		{
			const int32 CodeIndex = CodeChunks[MaterialProperty][ShaderFrequency].Num();
			// Adding an inline code chunk, the definition will be the code to inline
			new(CodeChunks[MaterialProperty][ShaderFrequency]) FShaderCodeChunk(FormattedCode,TEXT(""),Type,true);
			return CodeIndex;
		}
		// Can only create temporaries for float and material attribute types.
		else if (Type & (MCT_Float))
		{
			const int32 CodeIndex = CodeChunks[MaterialProperty][ShaderFrequency].Num();
			// Allocate a local variable name
			const FString SymbolName = CreateSymbolName(TEXT("Local"));
			// Construct the definition string which stores the result in a temporary and adds a newline for readability
			const FString LocalVariableDefinition = FString("	") + HLSLTypeString(Type) + TEXT(" ") + SymbolName + TEXT(" = ") + FormattedCode + TEXT(";") + LINE_TERMINATOR;
			// Adding a code chunk that creates a local variable
			new(CodeChunks[MaterialProperty][ShaderFrequency]) FShaderCodeChunk(*LocalVariableDefinition,SymbolName,Type,false);
			return CodeIndex;
		}
		else
		{
			if (Type == MCT_MaterialAttributes)
			{
				return Errorf(TEXT("Operation not supported on Material Attributes"));
			}

			if (Type & MCT_Texture)
			{
				return Errorf(TEXT("Operation not supported on a Texture"));
			}

			if (Type == MCT_StaticBool)
			{
				return Errorf(TEXT("Operation not supported on a Static Bool"));
			}
			
			return INDEX_NONE;
		}
	}

	/** 
	 * Constructs the formatted code chunk and creates a new local variable definition from it. 
	 * This should be used over AddInlinedCodeChunk when the code chunk adds actual instructions, and especially when calling a function.
	 * Creating local variables instead of inlining simplifies the generated code and reduces redundant expression chains,
	 * Making compiles faster and enabling the shader optimizer to do a better job.
	 */
	int32 AddCodeChunk(EMaterialValueType Type,const TCHAR* Format,...)
	{
		int32	BufferSize		= 256;
		TCHAR*	FormattedCode	= NULL;
		int32	Result			= -1;

		while(Result == -1)
		{
			FormattedCode = (TCHAR*) FMemory::Realloc( FormattedCode, BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT( FormattedCode, BufferSize, BufferSize-1, Format, Format, Result );
			BufferSize *= 2;
		};
		FormattedCode[Result] = 0;

		const int32 CodeIndex = AddCodeChunkInner(FormattedCode,Type,false);
		FMemory::Free(FormattedCode);

		return CodeIndex;
	}

	/** 
	 * Constructs the formatted code chunk and creates an inlined code chunk from it. 
	 * This should be used instead of AddCodeChunk when the code chunk does not add any actual shader instructions, for example a component mask.
	 */
	int32 AddInlinedCodeChunk(EMaterialValueType Type,const TCHAR* Format,...)
	{
		int32	BufferSize		= 256;
		TCHAR*	FormattedCode	= NULL;
		int32	Result			= -1;

		while(Result == -1)
		{
			FormattedCode = (TCHAR*) FMemory::Realloc( FormattedCode, BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT( FormattedCode, BufferSize, BufferSize-1, Format, Format, Result );
			BufferSize *= 2;
		};
		FormattedCode[Result] = 0;
		const int32 CodeIndex = AddCodeChunkInner(FormattedCode,Type,true);
		FMemory::Free(FormattedCode);

		return CodeIndex;
	}

	// AddUniformExpression - Adds an input to the Code array and returns its index.
	int32 AddUniformExpression(FMaterialUniformExpression* UniformExpression,EMaterialValueType Type,const TCHAR* Format,...)
	{
		if (Type == MCT_Unknown)
		{
			return INDEX_NONE;
		}

		check(UniformExpression);

		// Only a texture uniform expression can have MCT_Texture type
		if ((Type & MCT_Texture) && !UniformExpression->GetTextureUniformExpression())
		{
			return Errorf(TEXT("Operation not supported on a Texture"));
		}

		if (Type == MCT_StaticBool)
		{
			return Errorf(TEXT("Operation not supported on a Static Bool"));
		}
		
		if (Type == MCT_MaterialAttributes)
		{
			return Errorf(TEXT("Operation not supported on a MaterialAttributes"));
		}

		bool bFoundExistingExpression = false;
		// Search for an existing code chunk with the same uniform expression in the array of all uniform expressions used by this material.
		for (int32 ExpressionIndex = 0; ExpressionIndex < UniformExpressions.Num() && !bFoundExistingExpression; ExpressionIndex++)
		{
			FMaterialUniformExpression* TestExpression = UniformExpressions[ExpressionIndex].UniformExpression;
			check(TestExpression);
			if(TestExpression->IsIdentical(UniformExpression))
			{
				bFoundExistingExpression = true;
				// This code chunk has an identical uniform expression to the new expression, reuse it.
				// This allows multiple material properties to share uniform expressions because AccessUniformExpression uses AddUniqueItem when adding uniform expressions.
				check(Type == UniformExpressions[ExpressionIndex].Type);
				// Search for an existing code chunk with the same uniform expression in the array of code chunks for this material property.
				for(int32 ChunkIndex = 0;ChunkIndex < CodeChunks[MaterialProperty][ShaderFrequency].Num();ChunkIndex++)
				{
					FMaterialUniformExpression* OtherExpression = CodeChunks[MaterialProperty][ShaderFrequency][ChunkIndex].UniformExpression;
					if(OtherExpression && OtherExpression->IsIdentical(UniformExpression))
					{
						delete UniformExpression;
						// Reuse the entry in CodeChunks[MaterialProperty][ShaderFrequency]
						return ChunkIndex;
					}
				}
				delete UniformExpression;
				// Use the existing uniform expression from a different material property,
				// And continue so that a code chunk using the uniform expression will be generated for this material property.
				UniformExpression = TestExpression;
				break;
			}
		}

		int32		BufferSize		= 256;
		TCHAR*	FormattedCode	= NULL;
		int32		Result			= -1;

		while(Result == -1)
		{
			FormattedCode = (TCHAR*) FMemory::Realloc( FormattedCode, BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT( FormattedCode, BufferSize, BufferSize-1, Format, Format, Result );
			BufferSize *= 2;
		};
		FormattedCode[Result] = 0;

		const int32 ReturnIndex = CodeChunks[MaterialProperty][ShaderFrequency].Num();
		// Create a new code chunk for the uniform expression
		new(CodeChunks[MaterialProperty][ShaderFrequency]) FShaderCodeChunk(UniformExpression,FormattedCode,Type);

		if (!bFoundExistingExpression)
		{
			// Add an entry to the material-wide list of uniform expressions
			new(UniformExpressions) FShaderCodeChunk(UniformExpression,FormattedCode,Type);
		}

		FMemory::Free(FormattedCode);
		return ReturnIndex;
	}

	// AccessUniformExpression - Adds code to access the value of a uniform expression to the Code array and returns its index.
	int32 AccessUniformExpression(int32 Index)
	{
		check(Index >= 0 && Index < CodeChunks[MaterialProperty][ShaderFrequency].Num());
		const FShaderCodeChunk&	CodeChunk = CodeChunks[MaterialProperty][ShaderFrequency][Index];
		check(CodeChunk.UniformExpression && !CodeChunk.UniformExpression->IsConstant());

		FMaterialUniformExpressionTexture* TextureUniformExpression = CodeChunk.UniformExpression->GetTextureUniformExpression();
		// Any code chunk can have a texture uniform expression (eg FMaterialUniformExpressionFlipBookTextureParameter),
		// But a texture code chunk must have a texture uniform expression
		check(!(CodeChunk.Type & MCT_Texture) || TextureUniformExpression);

		TCHAR FormattedCode[MAX_SPRINTF]=TEXT("");
		if(CodeChunk.Type == MCT_Float)
		{
			const static TCHAR IndexToMask[] = {'x', 'y', 'z', 'w'};
			const int32 ScalarInputIndex = MaterialCompilationOutput.UniformExpressionSet.UniformScalarExpressions.AddUnique(CodeChunk.UniformExpression);
			// Update the above FMemory::Malloc if this FCString::Sprintf grows in size, e.g. %s, ...
			FCString::Sprintf(FormattedCode, TEXT("Material.ScalarExpressions[%u].%c"), ScalarInputIndex / 4, IndexToMask[ScalarInputIndex % 4]);
		}
		else if(CodeChunk.Type & MCT_Float)
		{
			const int32 VectorInputIndex = MaterialCompilationOutput.UniformExpressionSet.UniformVectorExpressions.AddUnique(CodeChunk.UniformExpression);
			const TCHAR* Mask;
			switch(CodeChunk.Type)
			{
			case MCT_Float:
			case MCT_Float1: Mask = TEXT(".r"); break;
			case MCT_Float2: Mask = TEXT(".rg"); break;
			case MCT_Float3: Mask = TEXT(".rgb"); break;
			default: Mask = TEXT(""); break;
			};

			FCString::Sprintf(FormattedCode, TEXT("Material.VectorExpressions[%u]%s"), VectorInputIndex, Mask);
		}
		else if(CodeChunk.Type & MCT_Texture)
		{
			int32 TextureInputIndex = INDEX_NONE;
			const TCHAR* BaseName = TEXT("");
			switch(CodeChunk.Type)
			{
			case MCT_Texture2D:
				TextureInputIndex = MaterialCompilationOutput.UniformExpressionSet.Uniform2DTextureExpressions.AddUnique(TextureUniformExpression);
				BaseName = TEXT("Texture2D");
				break;
			case MCT_TextureCube:
				TextureInputIndex = MaterialCompilationOutput.UniformExpressionSet.UniformCubeTextureExpressions.AddUnique(TextureUniformExpression);
				BaseName = TEXT("TextureCube");
				break;
			default: UE_LOG(LogMaterial, Fatal,TEXT("Unrecognized texture material value type: %u"),(int32)CodeChunk.Type);
			};
			FCString::Sprintf(FormattedCode, TEXT("Material%s_%u"), BaseName, TextureInputIndex);
		}
		else
		{
			UE_LOG(LogMaterial, Fatal,TEXT("User input of unknown type: %s"),DescribeType(CodeChunk.Type));
		}

		return AddInlinedCodeChunk(CodeChunks[MaterialProperty][ShaderFrequency][Index].Type,FormattedCode);
	}

	// CoerceParameter
	FString CoerceParameter(int32 Index,EMaterialValueType DestType)
	{
		check(Index >= 0 && Index < CodeChunks[MaterialProperty][ShaderFrequency].Num());
		const FShaderCodeChunk&	CodeChunk = CodeChunks[MaterialProperty][ShaderFrequency][Index];
		if( CodeChunk.Type == DestType )
		{
			return GetParameterCode(Index);
		}
		else
		{
			if( (CodeChunk.Type & DestType) && (CodeChunk.Type & MCT_Float) )
			{
				switch( DestType )
				{
				case MCT_Float1:
					return FString::Printf( TEXT("MaterialFloat(%s)"), *GetParameterCode(Index) );
				case MCT_Float2:
					return FString::Printf( TEXT("MaterialFloat2(%s,%s)"), *GetParameterCode(Index), *GetParameterCode(Index) );
				case MCT_Float3:
					return FString::Printf( TEXT("MaterialFloat3(%s,%s,%s)"), *GetParameterCode(Index), *GetParameterCode(Index), *GetParameterCode(Index) );
				case MCT_Float4:
					return FString::Printf( TEXT("MaterialFloat4(%s,%s,%s,%s)"), *GetParameterCode(Index), *GetParameterCode(Index), *GetParameterCode(Index), *GetParameterCode(Index) );
				default: 
					return FString::Printf( TEXT("%s"), *GetParameterCode(Index) );
				}
			}
			else
			{
				Errorf(TEXT("Coercion failed: %s: %s -> %s"),*CodeChunk.Definition,DescribeType(CodeChunk.Type),DescribeType(DestType));
				return TEXT("");
			}
		}
	}

	// GetParameterType
	EMaterialValueType GetParameterType(int32 Index) const
	{
		check(Index >= 0 && Index < CodeChunks[MaterialProperty][ShaderFrequency].Num());
		return CodeChunks[MaterialProperty][ShaderFrequency][Index].Type;
	}

	// GetParameterUniformExpression
	FMaterialUniformExpression* GetParameterUniformExpression(int32 Index) const
	{
		check(Index >= 0 && Index < CodeChunks[MaterialProperty][ShaderFrequency].Num());
		return CodeChunks[MaterialProperty][ShaderFrequency][Index].UniformExpression;
	}

	// GetArithmeticResultType
	EMaterialValueType GetArithmeticResultType(EMaterialValueType TypeA,EMaterialValueType TypeB)
	{
		if(!(TypeA & MCT_Float) || !(TypeB & MCT_Float))
		{
			Errorf(TEXT("Attempting to perform arithmetic on non-numeric types: %s %s"),DescribeType(TypeA),DescribeType(TypeB));
			return MCT_Unknown;
		}

		if(TypeA == TypeB)
		{
			return TypeA;
		}
		else if(TypeA & TypeB)
		{
			if(TypeA == MCT_Float)
			{
				return TypeB;
			}
			else
			{
				check(TypeB == MCT_Float);
				return TypeA;
			}
		}
		else
		{
			Errorf(TEXT("Arithmetic between types %s and %s are undefined"),DescribeType(TypeA),DescribeType(TypeB));
			return MCT_Unknown;
		}
	}

	EMaterialValueType GetArithmeticResultType(int32 A,int32 B)
	{
		check(A >= 0 && A < CodeChunks[MaterialProperty][ShaderFrequency].Num());
		check(B >= 0 && B < CodeChunks[MaterialProperty][ShaderFrequency].Num());

		EMaterialValueType	TypeA = CodeChunks[MaterialProperty][ShaderFrequency][A].Type,
			TypeB = CodeChunks[MaterialProperty][ShaderFrequency][B].Type;

		return GetArithmeticResultType(TypeA,TypeB);
	}

	// FMaterialCompiler interface.

	/** 
	 * Sets the current material property being compiled.  
	 * This affects the internal state of the compiler and the results of all functions except GetFixedParameterCode.
	 */
	virtual void SetMaterialProperty(EMaterialProperty InProperty, EShaderFrequency InShaderFrequency)
	{
		MaterialProperty = InProperty;
		ShaderFrequency = InShaderFrequency;
	}

	virtual int32 Error(const TCHAR* Text)
	{
		FString ErrorString;

		if (FunctionStack.Num() > 1)
		{
			// If we are inside a function, add that to the error message.  
			// Only add the function call node to ErrorExpressions, since we can't add a reference to the expressions inside the function as they are private objects.
			// Add the first function node on the stack because that's the one visible in the material being compiled, the rest are all nested functions.
			UMaterialExpressionMaterialFunctionCall* ErrorFunction = FunctionStack[1].FunctionCall;
			Material->ErrorExpressions.Add(ErrorFunction);
			ErrorString = FString(TEXT("Function ")) + ErrorFunction->MaterialFunction->GetName() + TEXT(": ");
		}

		if (FunctionStack.Last().ExpressionStack.Num() > 0)
		{
			UMaterialExpression* ErrorExpression = FunctionStack.Last().ExpressionStack.Last().Expression;
			check(ErrorExpression);

			if (ErrorExpression->GetClass() != UMaterialExpressionMaterialFunctionCall::StaticClass()
				&& ErrorExpression->GetClass() != UMaterialExpressionFunctionInput::StaticClass()
				&& ErrorExpression->GetClass() != UMaterialExpressionFunctionOutput::StaticClass())
			{
				// Add the expression currently being compiled to ErrorExpressions so we can draw it differently
				Material->ErrorExpressions.Add(ErrorExpression);

				const int32 ChopCount = FCString::Strlen(TEXT("MaterialExpression"));
				const FString ErrorClassName = ErrorExpression->GetClass()->GetName();

				// Add the node type to the error message
				ErrorString += FString(TEXT("(Node ")) + ErrorClassName.Right(ErrorClassName.Len() - ChopCount) + TEXT(") ");
			}
		}
			
		ErrorString += Text;

		//add the error string to the material's CompileErrors array
		Material->CompileErrors.AddUnique(ErrorString);
		bSuccess = false;
		
		return INDEX_NONE;
	}

	virtual int32 CallExpression(FMaterialExpressionKey ExpressionKey,FMaterialCompiler* Compiler) OVERRIDE
	{
		// Check if this expression has already been translated.
		int32* ExistingCodeIndex = FunctionStack.Last().ExpressionCodeMap[MaterialProperty][ShaderFrequency].Find(ExpressionKey);
		if(ExistingCodeIndex)
		{
			return *ExistingCodeIndex;
		}
		else
		{
			// Disallow reentrance.
			if(FunctionStack.Last().ExpressionStack.Find(ExpressionKey) != INDEX_NONE)
			{
				return Error(TEXT("Reentrant expression"));
			}

			// The first time this expression is called, translate it.
			FunctionStack.Last().ExpressionStack.Add(ExpressionKey);
			const int32 FunctionDepth = FunctionStack.Num();

			int32 Result = ExpressionKey.Expression->Compile(Compiler, ExpressionKey.OutputIndex, ExpressionKey.MultiplexIndex);

			FMaterialExpressionKey PoppedExpressionKey = FunctionStack.Last().ExpressionStack.Pop();

			// Verify state integrity
			check(PoppedExpressionKey == ExpressionKey);
			check(FunctionDepth == FunctionStack.Num());

			// Cache the translation.
			FunctionStack.Last().ExpressionCodeMap[MaterialProperty][ShaderFrequency].Add(ExpressionKey,Result);

			return Result;
		}
	}

	virtual EMaterialValueType GetType(int32 Code) OVERRIDE
	{
		if(Code != INDEX_NONE)
		{
			return GetParameterType(Code);
		}
		else
		{
			return MCT_Unknown;
		}
	}

	virtual EMaterialQualityLevel::Type GetQualityLevel() OVERRIDE
	{
		return QualityLevel;
	}

	virtual ERHIFeatureLevel::Type GetFeatureLevel() OVERRIDE
	{
		return FeatureLevel;
	}

	virtual float GetRefractionDepthBiasValue() OVERRIDE
	{
		return Material->GetRefractionDepthBiasValue();
	}

	/** 
	 * Casts the passed in code to DestType, or generates a compile error if the cast is not valid. 
	 * This will truncate a type (float4 -> float3) but not add components (float2 -> float3), however a float1 can be cast to any float type by replication. 
	 */
	virtual int32 ValidCast(int32 Code,EMaterialValueType DestType) OVERRIDE
	{
		if(Code == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(Code) && !GetParameterUniformExpression(Code)->IsConstant())
		{
			return ValidCast(AccessUniformExpression(Code),DestType);
		}

		EMaterialValueType SourceType = GetParameterType(Code);

		int32 CompiledResult = INDEX_NONE;
		if (SourceType & DestType)
		{
			CompiledResult = Code;
		}
		else if((SourceType & MCT_Float) && (DestType & MCT_Float))
		{
			const uint32 NumSourceComponents = GetNumComponents(SourceType);
			const uint32 NumDestComponents = GetNumComponents(DestType);

			if(NumSourceComponents > NumDestComponents) // Use a mask to select the first NumDestComponents components from the source.
			{
				const TCHAR*	Mask;
				switch(NumDestComponents)
				{
				case 1: Mask = TEXT(".r"); break;
				case 2: Mask = TEXT(".rg"); break;
				case 3: Mask = TEXT(".rgb"); break;
				default: UE_LOG(LogMaterial, Fatal,TEXT("Should never get here!")); return INDEX_NONE;
				};

				return AddInlinedCodeChunk(DestType,TEXT("%s%s"),*GetParameterCode(Code),Mask);
			}
			else if(NumSourceComponents < NumDestComponents) // Pad the source vector up to NumDestComponents.
			{
				// Only allow replication when the Source is a Float1
				if (NumSourceComponents == 1)
				{
					const uint32 NumPadComponents = NumDestComponents - NumSourceComponents;
					FString CommaParameterCodeString = FString::Printf(TEXT(",%s"), *GetParameterCode(Code));

					CompiledResult = AddInlinedCodeChunk(
						DestType,
						TEXT("%s(%s%s%s%s)"),
						HLSLTypeString(DestType),
						*GetParameterCode(Code),
						NumPadComponents >= 1 ? *CommaParameterCodeString : TEXT(""),
						NumPadComponents >= 2 ? *CommaParameterCodeString : TEXT(""),
						NumPadComponents >= 3 ? *CommaParameterCodeString : TEXT("")
						);
				}
				else
				{
					CompiledResult = Errorf(TEXT("Cannot cast from %s to %s."), DescribeType(SourceType), DescribeType(DestType));
				}
			}
			else
			{
				CompiledResult = Code;
			}
		}
		else
		{
			//We can feed any type into a material attributes socket as we're really just passing them through.
			if( DestType == MCT_MaterialAttributes )
			{
				CompiledResult = Code;
			}
			else
			{
				CompiledResult = Errorf(TEXT("Cannot cast from %s to %s."), DescribeType(SourceType), DescribeType(DestType));
			}
		}

		return CompiledResult;
	}

	virtual int32 ForceCast(int32 Code,EMaterialValueType DestType,bool bExactMatch=false,bool bReplicateValue=false) OVERRIDE
	{
		if(Code == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(Code) && !GetParameterUniformExpression(Code)->IsConstant())
		{
			return ForceCast(AccessUniformExpression(Code),DestType,bExactMatch,bReplicateValue);
		}

		EMaterialValueType	SourceType = GetParameterType(Code);

		if (bExactMatch ? (SourceType == DestType) : (SourceType & DestType))
		{
			return Code;
		}
		else if((SourceType & MCT_Float) && (DestType & MCT_Float))
		{
			const uint32 NumSourceComponents = GetNumComponents(SourceType);
			const uint32 NumDestComponents = GetNumComponents(DestType);

			if(NumSourceComponents > NumDestComponents) // Use a mask to select the first NumDestComponents components from the source.
			{
				const TCHAR*	Mask;
				switch(NumDestComponents)
				{
				case 1: Mask = TEXT(".r"); break;
				case 2: Mask = TEXT(".rg"); break;
				case 3: Mask = TEXT(".rgb"); break;
				default: UE_LOG(LogMaterial, Fatal,TEXT("Should never get here!")); return INDEX_NONE;
				};

				return AddInlinedCodeChunk(DestType,TEXT("%s%s"),*GetParameterCode(Code),Mask);
			}
			else if(NumSourceComponents < NumDestComponents) // Pad the source vector up to NumDestComponents.
			{
				// Only allow replication when the Source is a Float1
				if (NumSourceComponents != 1)
				{
					bReplicateValue = false;
				}

				const uint32 NumPadComponents = NumDestComponents - NumSourceComponents;
				FString CommaParameterCodeString = FString::Printf(TEXT(",%s"), *GetParameterCode(Code));

				return AddInlinedCodeChunk(
					DestType,
					TEXT("%s(%s%s%s%s)"),
					HLSLTypeString(DestType),
					*GetParameterCode(Code),
					NumPadComponents >= 1 ? (bReplicateValue ? *CommaParameterCodeString : TEXT(",0")) : TEXT(""),
					NumPadComponents >= 2 ? (bReplicateValue ? *CommaParameterCodeString : TEXT(",0")) : TEXT(""),
					NumPadComponents >= 3 ? (bReplicateValue ? *CommaParameterCodeString : TEXT(",0")) : TEXT("")
					);
			}
			else
			{
				return Code;
			}
		}
		else
		{
			return Errorf(TEXT("Cannot force a cast between non-numeric types."));
		}
	}

	/** Pushes a function onto the compiler's function stack, which indicates that compilation is entering a function. */
	virtual void PushFunction(const FMaterialFunctionCompileState& FunctionState) OVERRIDE
	{
		FunctionStack.Push(FunctionState);
	}	

	/** Pops a function from the compiler's function stack, which indicates that compilation is leaving a function. */
	virtual FMaterialFunctionCompileState PopFunction() OVERRIDE
	{
		return FunctionStack.Pop();
	}

	virtual int32 AccessCollectionParameter(UMaterialParameterCollection* ParameterCollection, int32 ParameterIndex, int32 ComponentIndex) OVERRIDE
	{
		if (!ParameterCollection || ParameterIndex == -1)
		{
			return INDEX_NONE;
		}

		int32 CollectionIndex = ParameterCollections.Find(ParameterCollection);

		if (CollectionIndex == INDEX_NONE)
		{
			if (ParameterCollections.Num() >= MaxNumParameterCollectionsPerMaterial)
			{
				return Error(TEXT("Material references too many MaterialParameterCollections!  A material may only reference 2 different collections."));
			}

			ParameterCollections.Add(ParameterCollection);
			CollectionIndex = ParameterCollections.Num() - 1;
		}

		int32 VectorChunk = AddCodeChunk(MCT_Float4,TEXT("MaterialCollection%u.Vectors[%u]"),CollectionIndex,ParameterIndex);

		return ComponentMask(VectorChunk, 
			ComponentIndex == -1 ? true : ComponentIndex % 4 == 0,
			ComponentIndex == -1 ? true : ComponentIndex % 4 == 1,
			ComponentIndex == -1 ? true : ComponentIndex % 4 == 2,
			ComponentIndex == -1 ? true : ComponentIndex % 4 == 3);
	}

	virtual int32 VectorParameter(FName ParameterName,const FLinearColor& DefaultValue) OVERRIDE
	{
		return AddUniformExpression(new FMaterialUniformExpressionVectorParameter(ParameterName,DefaultValue),MCT_Float4,TEXT(""));
	}

	virtual int32 ScalarParameter(FName ParameterName,float DefaultValue) OVERRIDE
	{
		return AddUniformExpression(new FMaterialUniformExpressionScalarParameter(ParameterName,DefaultValue),MCT_Float,TEXT(""));
	}

	virtual int32 Constant(float X) OVERRIDE
	{
		return AddUniformExpression(new FMaterialUniformExpressionConstant(FLinearColor(X,X,X,X),MCT_Float),MCT_Float,TEXT("%0.8f"),X);
	}

	virtual int32 Constant2(float X,float Y) OVERRIDE
	{
		return AddUniformExpression(new FMaterialUniformExpressionConstant(FLinearColor(X,Y,0,0),MCT_Float2),MCT_Float2,TEXT("MaterialFloat2(%0.8f,%0.8f)"),X,Y);
	}

	virtual int32 Constant3(float X,float Y,float Z) OVERRIDE
	{
		return AddUniformExpression(new FMaterialUniformExpressionConstant(FLinearColor(X,Y,Z,0),MCT_Float3),MCT_Float3,TEXT("MaterialFloat3(%0.8f,%0.8f,%0.8f)"),X,Y,Z);
	}

	virtual int32 Constant4(float X,float Y,float Z,float W) OVERRIDE
	{
		return AddUniformExpression(new FMaterialUniformExpressionConstant(FLinearColor(X,Y,Z,W),MCT_Float4),MCT_Float4,TEXT("MaterialFloat4(%0.8f,%0.8f,%0.8f,%0.8f)"),X,Y,Z,W);
	}

	virtual int32 GameTime() OVERRIDE
	{
		return AddInlinedCodeChunk(MCT_Float, TEXT("View.GameTime"));
	}

	virtual int32 RealTime() OVERRIDE
	{
		return AddInlinedCodeChunk(MCT_Float, TEXT("View.RealTime"));
	}

	virtual int32 PeriodicHint(int32 PeriodicCode) OVERRIDE
	{
		if(PeriodicCode == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(PeriodicCode))
		{
			return AddUniformExpression(new FMaterialUniformExpressionPeriodic(GetParameterUniformExpression(PeriodicCode)),GetParameterType(PeriodicCode),TEXT("%s"),*GetParameterCode(PeriodicCode));
		}
		else
		{
			return PeriodicCode;
		}
	}

	virtual int32 Sine(int32 X) OVERRIDE
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionSine(GetParameterUniformExpression(X),0),MCT_Float,TEXT("sin(%s)"),*CoerceParameter(X,MCT_Float));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),TEXT("sin(%s)"),*GetParameterCode(X));
		}
	}

	virtual int32 Cosine(int32 X) OVERRIDE
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionSine(GetParameterUniformExpression(X),1),MCT_Float,TEXT("cos(%s)"),*CoerceParameter(X,MCT_Float));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),TEXT("cos(%s)"),*GetParameterCode(X));
		}
	}

	virtual int32 Floor(int32 X) OVERRIDE
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFloor(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("floor(%s)"),*GetParameterCode(X));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),TEXT("floor(%s)"),*GetParameterCode(X));
		}
	}

	virtual int32 Ceil(int32 X) OVERRIDE
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionCeil(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("ceil(%s)"),*GetParameterCode(X));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),TEXT("ceil(%s)"),*GetParameterCode(X));
		}
	}

	virtual int32 Frac(int32 X) OVERRIDE
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFrac(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("frac(%s)"),*GetParameterCode(X));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),TEXT("frac(%s)"),*GetParameterCode(X));
		}
	}

	virtual int32 Fmod(int32 A, int32 B) OVERRIDE
	{
		if ((A == INDEX_NONE) || (B == INDEX_NONE))
		{
			return INDEX_NONE;
		}

		if (GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFmod(GetParameterUniformExpression(A),GetParameterUniformExpression(B)),
				GetParameterType(A),TEXT("fmod(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
		else
		{
			return AddCodeChunk(GetParameterType(A),
				TEXT("fmod(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
	}

	/**
	* Creates the new shader code chunk needed for the Abs expression
	*
	* @param	X - Index to the FMaterialCompiler::CodeChunk entry for the input expression
	* @return	Index to the new FMaterialCompiler::CodeChunk entry for this expression
	*/	
	virtual int32 Abs( int32 X ) OVERRIDE
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		// get the user input struct for the input expression
		FMaterialUniformExpression* pInputParam = GetParameterUniformExpression(X);
		if( pInputParam )
		{
			FMaterialUniformExpressionAbs* pUniformExpression = new FMaterialUniformExpressionAbs( pInputParam );
			return AddUniformExpression( pUniformExpression, GetParameterType(X), TEXT("abs(%s)"), *GetParameterCode(X) );
		}
		else
		{
			return AddCodeChunk( GetParameterType(X), TEXT("abs(%s)"), *GetParameterCode(X) );
		}
	}

	virtual int32 ReflectionVector() OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Domain)
		{
			return NonPixelShaderExpressionError();
		}

		return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.ReflectionVector"));
	}

	virtual int32 ReflectionAboutCustomWorldNormal(int32 CustomWorldNormal, int32 bNormalizeCustomWorldNormal) OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Domain)
		{
			return NonPixelShaderExpressionError();
		}

		if (CustomWorldNormal == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		const TCHAR* ShouldNormalize = (!!bNormalizeCustomWorldNormal) ? TEXT("true") : TEXT("false");

		return AddCodeChunk(MCT_Float3,TEXT("ReflectionAboutCustomWorldNormal(Parameters, %s, %s)"), *GetParameterCode(CustomWorldNormal), ShouldNormalize);
	}

	virtual int32 CameraVector() OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Domain)
		{
			return NonPixelShaderExpressionError();
		}
		return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.CameraVector"));
	}

	virtual int32 CameraWorldPosition() OVERRIDE
	{
		return AddInlinedCodeChunk(MCT_Float3,TEXT("View.ViewOrigin.xyz"));
	}

	virtual int32 LightVector() OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}

		if (!Material->IsLightFunction() && !Material->IsUsedWithDeferredDecal())
		{
			return Errorf(TEXT("LightVector can only be used in LightFunction or DeferredDecal materials"));
		}

		return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.LightVector"));
	}

	virtual int32 ScreenPosition() OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}

		return AddCodeChunk(MCT_Float2,TEXT("ScreenAlignedPosition(Parameters.ScreenPosition).xy"));		
	}

	virtual int32 ViewSize() OVERRIDE
	{
		return AddCodeChunk(MCT_Float2,TEXT("View.ViewSizeAndSceneTexelSize.xy"));
	}

	virtual int32 SceneTexelSize() OVERRIDE
	{
		return AddCodeChunk(MCT_Float2,TEXT("View.ViewSizeAndSceneTexelSize.zw"));
	}

	virtual int32 ParticleMacroUV() OVERRIDE 
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}

		return AddCodeChunk(MCT_Float2,TEXT("GetParticleMacroUV(Parameters)"));
	}

	virtual int32 ParticleSubUV(int32 TextureIndex, EMaterialSamplerType SamplerType, bool bBlend) OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}

		if (TextureIndex == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		int32 ParticleSubUV;
		const FString TexCoordCode = TEXT("Parameters.Particle.SubUVCoords[%u].xy");
		const int32 TexCoord1 = AddCodeChunk(MCT_Float2,*TexCoordCode,0);

		if(bBlend)
		{
			// Out	 = linear interpolate... using 2 sub-images of the texture
			// A	 = RGB sample texture with Parameters.Particle.SubUVCoords[0]
			// B	 = RGB sample texture with Parameters.Particle.SubUVCoords[1]
			// Alpha = Parameters.Particle.SubUVLerp

			const int32 TexCoord2 = AddCodeChunk( MCT_Float2,*TexCoordCode,1);
			const int32 SubImageLerp = AddCodeChunk(MCT_Float, TEXT("Parameters.Particle.SubUVLerp"));

			const int32 TexSampleA = TextureSample(TextureIndex, TexCoord1, SamplerType );
			const int32 TexSampleB = TextureSample(TextureIndex, TexCoord2, SamplerType );
			ParticleSubUV = Lerp( TexSampleA,TexSampleB, SubImageLerp);
		} 
		else
		{
			ParticleSubUV = TextureSample(TextureIndex, TexCoord1, SamplerType );
		}
	
		bUsesParticleSubUVs = true;
		return ParticleSubUV;
	}

	virtual int32 ParticleColor() OVERRIDE
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bUsesParticleColor |= (ShaderFrequency != SF_Vertex);
		return AddInlinedCodeChunk(MCT_Float4,TEXT("Parameters.Particle.Color"));	
	}

	virtual int32 ParticlePosition() OVERRIDE
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bNeedsParticlePosition = true;
		return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.Particle.PositionAndSize.xyz"));	
	}

	virtual int32 ParticleRadius() OVERRIDE
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bNeedsParticlePosition = true;
		return AddInlinedCodeChunk(MCT_Float,TEXT("max(Parameters.Particle.PositionAndSize.w, .001f)"));	
	}

	virtual int32 SphericalParticleOpacity(int32 Density) OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}

		if (Density == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		bNeedsParticlePosition = true;
		bUsesSphericalParticleOpacity = true;
		return AddCodeChunk(MCT_Float, TEXT("GetSphericalParticleOpacity(Parameters,%s)"), *GetParameterCode(Density));
	}

	virtual int32 ParticleRelativeTime() OVERRIDE
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bNeedsParticleTime = true;
		return AddInlinedCodeChunk(MCT_Float,TEXT("Parameters.Particle.RelativeTime"));
	}

	virtual int32 ParticleMotionBlurFade() OVERRIDE
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bUsesParticleMotionBlur = true;
		return AddInlinedCodeChunk(MCT_Float,TEXT("Parameters.Particle.MotionBlurFade"));
	}

	virtual int32 ParticleDirection() OVERRIDE
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bNeedsParticleVelocity = true;
		return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.Particle.Velocity.xyz"));
	}

	virtual int32 ParticleSpeed() OVERRIDE
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bNeedsParticleVelocity = true;
		return AddInlinedCodeChunk(MCT_Float,TEXT("Parameters.Particle.Velocity.w"));
	}

	virtual int32 ParticleSize() OVERRIDE
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		bNeedsParticleSize = true;
		return AddInlinedCodeChunk(MCT_Float2,TEXT("Parameters.Particle.Size"));
	}

	virtual int32 WorldPosition(EWorldPositionIncludedOffsets WorldPositionIncludedOffsets) OVERRIDE
	{
		if (ShaderFrequency == SF_Pixel)
		{
			// If this material has no expressions for world position offset or world displacement, the non-offset world position will
			// be exactly the same as the offset one, so there is no point bringing in the extra code.
			// Also, we can't access the full offset world position in anything other than the pixel shader, because it won't have
			// been calculated yet
			bool bNonOffsetWorldPositionAvailable = Material->MaterialModifiesMeshPosition() && ShaderFrequency == SF_Pixel;

			switch (WorldPositionIncludedOffsets)
			{
			case WPT_Default:
				{
					return AddInlinedCodeChunk(MCT_Float3, TEXT("Parameters.WorldPosition"));
				}

			case WPT_ExcludeAllShaderOffsets:
				{
					bNeedsWorldPositionExcludingShaderOffsets = true;
					return AddInlinedCodeChunk(MCT_Float3, TEXT("Parameters.WorldPosition_NoOffsets"));
				}

			case WPT_CameraRelative:
				{
					return AddInlinedCodeChunk(MCT_Float3, TEXT("Parameters.WorldPosition_CamRelative"));
				}

			case WPT_CameraRelativeNoOffsets:
				{
					bNeedsWorldPositionExcludingShaderOffsets = true;
					return AddInlinedCodeChunk(MCT_Float3, TEXT("Parameters.WorldPosition_NoOffsets_CamRelative"));
				}

			default:
				{
					Errorf(TEXT("Encountered unknown world position type '%d'"), WorldPositionIncludedOffsets);
					return INDEX_NONE;
				}
			}
		}
		else
		{
			// If not in a pixel shader, only the normal WorldPosition is available
			return AddInlinedCodeChunk(MCT_Float3, TEXT("Parameters.WorldPosition"));
		}
	}

	virtual int32 ObjectWorldPosition() OVERRIDE
	{
		return AddInlinedCodeChunk(MCT_Float3,TEXT("GetObjectWorldPosition(Parameters)"));		
	}

	virtual int32 ObjectRadius() OVERRIDE
	{
		return AddInlinedCodeChunk(MCT_Float,TEXT("Primitive.ObjectWorldPositionAndRadius.w"));		
	}

	virtual int32 ObjectBounds() OVERRIDE
	{
		return AddInlinedCodeChunk(MCT_Float3,TEXT("Primitive.ObjectBounds.xyz"));		
	}

	virtual int32 DistanceCullFade() OVERRIDE
	{
		return AddInlinedCodeChunk(MCT_Float,TEXT("GetDistanceCullFade()"));		
	}

	virtual int32 ActorWorldPosition() OVERRIDE
	{
		return AddInlinedCodeChunk(MCT_Float3,TEXT("Primitive.ActorWorldPosition"));		
	}

	virtual int32 If(int32 A,int32 B,int32 AGreaterThanB,int32 AEqualsB,int32 ALessThanB,int32 ThresholdArg) OVERRIDE
	{
		if(A == INDEX_NONE || B == INDEX_NONE || AGreaterThanB == INDEX_NONE || ALessThanB == INDEX_NONE || ThresholdArg == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (AEqualsB != INDEX_NONE)
		{
			EMaterialValueType ResultType = GetArithmeticResultType(GetParameterType(AGreaterThanB),GetArithmeticResultType(AEqualsB,ALessThanB));

			int32 CoercedAGreaterThanB = ForceCast(AGreaterThanB,ResultType);
			int32 CoercedAEqualsB = ForceCast(AEqualsB,ResultType);
			int32 CoercedALessThanB = ForceCast(ALessThanB,ResultType);

			if(CoercedAGreaterThanB == INDEX_NONE || CoercedAEqualsB == INDEX_NONE || CoercedALessThanB == INDEX_NONE)
			{
				return INDEX_NONE;
			}

			return AddCodeChunk(
				ResultType,
				TEXT("((abs(%s - %s) > %s) ? (%s >= %s ? %s : %s) : %s)"),
				*GetParameterCode(A),
				*GetParameterCode(B),
				*GetParameterCode(ThresholdArg),
				*GetParameterCode(A),
				*GetParameterCode(B),
				*GetParameterCode(CoercedAGreaterThanB),
				*GetParameterCode(CoercedALessThanB),
				*GetParameterCode(CoercedAEqualsB)
				);
		}
		else
		{
			EMaterialValueType ResultType = GetArithmeticResultType(AGreaterThanB,ALessThanB);

			int32 CoercedAGreaterThanB = ForceCast(AGreaterThanB,ResultType);
			int32 CoercedALessThanB = ForceCast(ALessThanB,ResultType);

			if(CoercedAGreaterThanB == INDEX_NONE || CoercedALessThanB == INDEX_NONE)
			{
				return INDEX_NONE;
			}

			return AddCodeChunk(
				ResultType,
				TEXT("((%s >= %s) ? %s : %s)"),
				*GetParameterCode(A),
				*GetParameterCode(B),
				*GetParameterCode(CoercedAGreaterThanB),
				*GetParameterCode(CoercedALessThanB)
				);
		}
	}

	virtual int32 TextureCoordinate(uint32 CoordinateIndex, bool UnMirrorU, bool UnMirrorV) OVERRIDE
	{
		const uint32 MaxNumCoordinates = FeatureLevel == ERHIFeatureLevel::ES2 ? 3 : 8;

		if (CoordinateIndex >= MaxNumCoordinates)
		{
			return Errorf(TEXT("Only %u texture coordinate sets can be used by this feature level, currently using %u"), MaxNumCoordinates, CoordinateIndex + 1);
		}

		if (ShaderFrequency == SF_Vertex)
		{
			NumUserVertexTexCoords = FMath::Max(CoordinateIndex + 1, NumUserVertexTexCoords);
		}
		else
		{
			NumUserTexCoords = FMath::Max(CoordinateIndex + 1, NumUserTexCoords);
		}

		FString	SampleCode;
		if ( UnMirrorU && UnMirrorV )
		{
			SampleCode = TEXT("UnMirrorUV(Parameters.TexCoords[%u].xy, Parameters)");
		}
		else if ( UnMirrorU )
		{
			SampleCode = TEXT("UnMirrorU(Parameters.TexCoords[%u].xy, Parameters)");
		}
		else if ( UnMirrorV )
		{
			SampleCode = TEXT("UnMirrorV(Parameters.TexCoords[%u].xy, Parameters)");
		}
		else
		{
			SampleCode = TEXT("Parameters.TexCoords[%u].xy");
		}

		// Note: inlining is important so that on ES2 devices, where half precision is used in the pixel shader, 
		// The UV does not get assigned to a half temporary in cases where the texture sample is done directly from interpolated UVs
		return AddInlinedCodeChunk(
				MCT_Float2,
				*SampleCode,
				CoordinateIndex
				);
		}

	virtual int32 TextureSample(int32 TextureIndex,int32 CoordinateIndex,EMaterialSamplerType SamplerType,int32 MipValueIndex=INDEX_NONE,ETextureMipValueMode MipValueMode=TMVM_None) OVERRIDE
	{
		if(TextureIndex == INDEX_NONE || CoordinateIndex == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (ShaderFrequency != SF_Pixel
			&& ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM4) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType TextureType = GetParameterType(TextureIndex);

		if(TextureType != MCT_Texture2D && TextureType != MCT_TextureCube)
		{
			Errorf(TEXT("Sampling unknown texture type: %s"),DescribeType(TextureType));
			return INDEX_NONE;
		}

		if(ShaderFrequency != SF_Pixel && MipValueMode == TMVM_MipBias)
		{
			Errorf(TEXT("MipBias is only supported in the pixel shader"));
			return INDEX_NONE;
		}

		FString MipValueCode =
			(MipValueIndex != INDEX_NONE && MipValueMode != TMVM_None)
			? CoerceParameter(MipValueIndex, MCT_Float1)
			: TEXT("0.0f");

		// if we are not in the PS we need a mip level
		if(ShaderFrequency != SF_Pixel)
		{
			MipValueMode = TMVM_MipLevel;
		}

		FString SampleCode = 
			(TextureType == MCT_TextureCube)
			? TEXT("TextureCubeSample")
			: TEXT("Texture2DSample");
	
		if(MipValueMode == TMVM_None)
		{
			SampleCode += TEXT("(%s,%sSampler,%s)");
		}
		else if(MipValueMode == TMVM_MipLevel)
		{
			// Mobile: Sampling of a particular level depends on an extension; iOS does have it by default but
			// there's a driver as of 7.0.2 that will cause a GPU hang if used with an Aniso > 1 sampler, so show an error for now
			if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM3) == INDEX_NONE)
			{
				Errorf(TEXT("Sampling for a specific mip-level is not supported for mobile"));
				return INDEX_NONE;
			}

			SampleCode += TEXT("Level(%s,%sSampler,%s,%s)");
		}
		else if(MipValueMode == TMVM_MipBias)
		{
			SampleCode += TEXT("Bias(%s,%sSampler,%s,%s)");
		}

		switch( SamplerType )
		{
			case SAMPLERTYPE_Color:
				SampleCode = FString::Printf( TEXT("ProcessMaterialColorTextureLookup(%s)"), *SampleCode );
				break;
			
			case SAMPLERTYPE_Alpha:
				// Sampling a single channel texture in D3D9 gives: (G,G,G)
				// Sampling a single channel texture in D3D11 gives: (G,0,0)
				// This replication reproduces the D3D9 behavior in all cases.
				SampleCode = FString::Printf( TEXT("(%s).rrrr"), *SampleCode );
				break;
			
			case SAMPLERTYPE_Grayscale:
				// Sampling a greyscale texture in D3D9 gives: (G,G,G)
				// Sampling a greyscale texture in D3D11 gives: (G,0,0)
				// This replication reproduces the D3D9 behavior in all cases.
				SampleCode = FString::Printf( TEXT("ProcessMaterialGreyscaleTextureLookup((%s).r).rrrr"), *SampleCode );
				break;

			case SAMPLERTYPE_Normal:
				// Normal maps need to be unpacked in the pixel shader.
				SampleCode = FString::Printf( TEXT("UnpackNormalMap(%s)"), *SampleCode );
				break;
		}

		FString TextureName =
			(TextureType == MCT_TextureCube)
			? CoerceParameter(TextureIndex, MCT_TextureCube)
			: CoerceParameter(TextureIndex, MCT_Texture2D);

		FString UVs =
			(TextureType == MCT_TextureCube)
			? CoerceParameter(CoordinateIndex, MCT_Float3)
			: CoerceParameter(CoordinateIndex, MCT_Float2);

		return AddCodeChunk(
			MCT_Float4,
			*SampleCode,
			*TextureName,
			*TextureName,
			*UVs,
			*MipValueCode
			);
	}

	virtual int32 PixelDepth() OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}
		return AddInlinedCodeChunk(MCT_Float, TEXT("Parameters.ScreenPosition.w"));		
	}

	/** Calculate screen aligned UV coodinates from an offset fraction or texture coordinate */
	int32 GetScreenAlignedUV(int32 Offset, int32 UV, bool bUseOffset)
	{
		if(bUseOffset)
		{
			return AddCodeChunk(MCT_Float2, TEXT("CalcScreenUVFromOffsetFraction(Parameters.ScreenPosition, %s)"), *GetParameterCode(Offset));
		}
		else
		{
			FString DefaultScreenAligned(TEXT("MaterialFloat2(ScreenAlignedPosition(Parameters.ScreenPosition).xy)"));
			FString CodeString = (UV != INDEX_NONE) ? CoerceParameter(UV,MCT_Float2) : DefaultScreenAligned;
			return AddInlinedCodeChunk(MCT_Float2, *CodeString );
		}
	}

	virtual int32 SceneDepth(int32 Offset, int32 UV, bool bUseOffset) OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}

		if (Offset == INDEX_NONE && bUseOffset)
		{
			return INDEX_NONE;
		}

		bUsesSceneDepth = true;

		FString	UserDepthCode(TEXT("CalcSceneDepth(%s)"));
		int32 TexCoordCode = GetScreenAlignedUV(Offset, UV, bUseOffset);
		// add the code string
		return AddCodeChunk(
			MCT_Float,
			*UserDepthCode,
			*GetParameterCode(TexCoordCode)
			);
	}
	
	// @param SceneTextureId of type ESceneTextureId e.g. PPI_SubsurfaceColor
	virtual int32 SceneTextureLookup(int32 UV, uint32 InSceneTextureId) OVERRIDE
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM3) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (ShaderFrequency != SF_Pixel)
		{
			// we can relax this later if needed
			return NonPixelShaderExpressionError();
		}
		
		MaterialCompilationOutput.bNeedsSceneTextures = true;
		bNeedsSceneTexturePostProcessInputs = ((InSceneTextureId >= PPI_PostProcessInput0 && InSceneTextureId <= PPI_PostProcessInput6) || InSceneTextureId == PPI_SceneColor);

		ESceneTextureId SceneTextureId = (ESceneTextureId)InSceneTextureId;

		FString DefaultScreenAligned(TEXT("MaterialFloat2(ScreenAlignedPosition(Parameters.ScreenPosition).xy)"));
		FString TexCoordCode((UV != INDEX_NONE) ? CoerceParameter(UV, MCT_Float2) : DefaultScreenAligned);

		return AddCodeChunk(
			MCT_Float4,
			TEXT("SceneTextureLookup(%s, %d)"),
			*TexCoordCode, (int)SceneTextureId
			);
	}

	// @param SceneTextureId of type ESceneTextureId e.g. PPI_SubsurfaceColor
	virtual int32 SceneTextureSize(uint32 InSceneTextureId, bool bInvert) OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel)
		{
			// we can relax this later if needed
			return NonPixelShaderExpressionError();
		}

		MaterialCompilationOutput.bNeedsSceneTextures = true;

		ESceneTextureId SceneTextureId = (ESceneTextureId)InSceneTextureId;

		if(SceneTextureId >= PPI_PostProcessInput0 && SceneTextureId <= PPI_PostProcessInput6)
		{
			int Index = SceneTextureId - PPI_PostProcessInput0;

			if(bInvert)
			{
				return AddCodeChunk(MCT_Float2, TEXT("PostprocessInput%dSize.zw"), Index);
			}
			else
			{
				return AddCodeChunk(MCT_Float2, TEXT("PostprocessInput%dSize.xy"), Index);
			}
		}
		else
		{
			// BufferSize
			if(bInvert)
			{
				return Div(Constant(1.0f), AddCodeChunk(MCT_Float2, TEXT("View.RenderTargetSize")));
			}
			else
			{
				return AddCodeChunk(MCT_Float2, TEXT("View.RenderTargetSize"));
			}
		}
	}

	// @param SceneTextureId of type ESceneTextureId e.g. PPI_SubsurfaceColor
	virtual int32 SceneTextureMin(uint32 InSceneTextureId) OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel)
		{
			// we can relax this later if needed
			return NonPixelShaderExpressionError();
		}

		MaterialCompilationOutput.bNeedsSceneTextures = true;

		ESceneTextureId SceneTextureId = (ESceneTextureId)InSceneTextureId;

		if(SceneTextureId >= PPI_PostProcessInput0 && SceneTextureId <= PPI_PostProcessInput6)
		{
			int Index = SceneTextureId - PPI_PostProcessInput0;

			return AddCodeChunk(MCT_Float2,TEXT("PostprocessInput%dMinMax.xy"), Index);
		}
		else
		{			
			return AddCodeChunk(MCT_Float2,TEXT("View.SceneTextureMinMax.xy"));
		}
	}

	virtual int32 SceneTextureMax(uint32 InSceneTextureId) OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel)
		{
			// we can relax this later if needed
			return NonPixelShaderExpressionError();
		}

		MaterialCompilationOutput.bNeedsSceneTextures = true;

		ESceneTextureId SceneTextureId = (ESceneTextureId)InSceneTextureId;

		if(SceneTextureId >= PPI_PostProcessInput0 && SceneTextureId <= PPI_PostProcessInput6)
		{
			int Index = SceneTextureId - PPI_PostProcessInput0;

			return AddCodeChunk(MCT_Float2,TEXT("PostprocessInput%dMinMax.zw"), Index);
		}
		else
		{			
			return AddCodeChunk(MCT_Float2,TEXT("View.SceneTextureMinMax.zw"));
		}
	}

	virtual int32 SceneColor(int32 Offset, int32 UV, bool bUseOffset) OVERRIDE
	{
		if (Offset == INDEX_NONE && bUseOffset)
		{
			return INDEX_NONE;
		}

		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}

		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM3) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		MaterialCompilationOutput.bUsesSceneColor = true;

		int32 ScreenUVCode = GetScreenAlignedUV(Offset, UV, bUseOffset);
		return AddCodeChunk(
			MCT_Float3,
			TEXT("DecodeSceneColorForMaterialNode(%s)"),
			*GetParameterCode(ScreenUVCode)
			);
	}

	virtual int32 Texture(UTexture* InTexture) OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel
			&& ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM4) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType ShaderType = InTexture->GetMaterialType();
		const int32 TextureReferenceIndex = Material->GetReferencedTextures().Find(InTexture);
		checkf(TextureReferenceIndex != INDEX_NONE, TEXT("Material expression called Compiler->Texture() without implementing UMaterialExpression::GetReferencedTexture properly"));
		return AddUniformExpression(new FMaterialUniformExpressionTexture(TextureReferenceIndex),ShaderType,TEXT(""));
	}

	virtual int32 TextureParameter(FName ParameterName,UTexture* DefaultValue) OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel
			&& ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM4) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType ShaderType = DefaultValue->GetMaterialType();
		const int32 TextureReferenceIndex = Material->GetReferencedTextures().Find(DefaultValue);
		checkf(TextureReferenceIndex != INDEX_NONE, TEXT("Material expression called Compiler->TextureParameter() without implementing UMaterialExpression::GetReferencedTexture properly"));
		return AddUniformExpression(new FMaterialUniformExpressionTextureParameter(ParameterName,TextureReferenceIndex),ShaderType,TEXT(""));
	}

	virtual int32 StaticBool(bool bValue) OVERRIDE
	{
		return AddInlinedCodeChunk(MCT_StaticBool,(bValue ? TEXT("true") : TEXT("false")));
	}

	virtual int32 StaticBoolParameter(FName ParameterName,bool bDefaultValue) OVERRIDE
	{
		// Look up the value we are compiling with for this static parameter.
		bool bValue = bDefaultValue;
		for(int32 ParameterIndex = 0;ParameterIndex < StaticParameters.StaticSwitchParameters.Num();++ParameterIndex)
		{
			const FStaticSwitchParameter& Parameter = StaticParameters.StaticSwitchParameters[ParameterIndex];
			if(Parameter.ParameterName == ParameterName)
			{
				bValue = Parameter.Value;
				break;
			}
		}

		return StaticBool(bValue);
	}
	
	virtual int32 StaticComponentMask(int32 Vector,FName ParameterName,bool bDefaultR,bool bDefaultG,bool bDefaultB,bool bDefaultA) OVERRIDE
	{
		// Look up the value we are compiling with for this static parameter.
		bool bValueR = bDefaultR;
		bool bValueG = bDefaultG;
		bool bValueB = bDefaultB;
		bool bValueA = bDefaultA;
		for(int32 ParameterIndex = 0;ParameterIndex < StaticParameters.StaticComponentMaskParameters.Num();++ParameterIndex)
		{
			const FStaticComponentMaskParameter& Parameter = StaticParameters.StaticComponentMaskParameters[ParameterIndex];
			if(Parameter.ParameterName == ParameterName)
			{
				bValueR = Parameter.R;
				bValueG = Parameter.G;
				bValueB = Parameter.B;
				bValueA = Parameter.A;
				break;
			}
		}

		return ComponentMask(Vector,bValueR,bValueG,bValueB,bValueA);
	}

	virtual bool GetStaticBoolValue(int32 BoolIndex, bool& bSucceeded) OVERRIDE
	{
		bSucceeded = true;
		if (BoolIndex == INDEX_NONE)
		{
			bSucceeded = false;
			return false;
		}

		if (GetParameterType(BoolIndex) != MCT_StaticBool)
		{
			Errorf(TEXT("Failed to cast %s input to static bool type"), DescribeType(GetParameterType(BoolIndex)));
			bSucceeded = false;
			return false;
		}

		if (GetParameterCode(BoolIndex).Contains(TEXT("true")))
		{
			return true;
		}
		return false;
	}

	virtual int32 StaticTerrainLayerWeight(FName ParameterName,int32 Default) OVERRIDE
	{
		// Look up the weight-map index for this static parameter.
		int32 WeightmapIndex = INDEX_NONE;
		bool bFoundParameter = false;
		for(int32 ParameterIndex = 0;ParameterIndex < StaticParameters.TerrainLayerWeightParameters.Num();++ParameterIndex)
		{
			const FStaticTerrainLayerWeightParameter& Parameter = StaticParameters.TerrainLayerWeightParameters[ParameterIndex];
			if(Parameter.ParameterName == ParameterName)
			{
				WeightmapIndex = Parameter.WeightmapIndex;
				bFoundParameter = true;
				break;
			}
		}

		if(!bFoundParameter)
		{
			return Default;
		}
		else if(WeightmapIndex == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		else if (GetFeatureLevel() != ERHIFeatureLevel::ES2)
		{
			FString WeightmapName = FString::Printf(TEXT("Weightmap%d"),WeightmapIndex);
			int32 TextureCodeIndex = TextureParameter(FName(*WeightmapName),  GEngine->WeightMapPlaceholderTexture);
			int32 WeightmapCode = TextureSample(TextureCodeIndex, TextureCoordinate(3, false, false), SAMPLERTYPE_Masks);
			FString LayerMaskName = FString::Printf(TEXT("LayerMask_%s"),*ParameterName.ToString());
			return Dot(WeightmapCode,VectorParameter(FName(*LayerMaskName), FLinearColor(1.f,0.f,0.f,0.f)));
		}
		else
		{
			int32 WeightmapCode = AddInlinedCodeChunk(MCT_Float4, TEXT("Parameters.LayerWeights"));
			FString LayerMaskName = FString::Printf(TEXT("LayerMask_%s"),*ParameterName.ToString());
			return Dot(WeightmapCode,VectorParameter(FName(*LayerMaskName), FLinearColor(1.f,0.f,0.f,0.f)));
		}
	}

	virtual int32 VertexColor() OVERRIDE
	{
		bUsesVertexColor |= (ShaderFrequency != SF_Vertex);
		return AddInlinedCodeChunk(MCT_Float4,TEXT("Parameters.VertexColor"));
	}

	virtual int32 Add(int32 A,int32 B) OVERRIDE
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A),GetParameterUniformExpression(B),FMO_Add),GetArithmeticResultType(A,B),TEXT("(%s + %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
		else
		{
			return AddCodeChunk(GetArithmeticResultType(A,B),TEXT("(%s + %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
	}

	virtual int32 Sub(int32 A,int32 B) OVERRIDE
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A),GetParameterUniformExpression(B),FMO_Sub),GetArithmeticResultType(A,B),TEXT("(%s - %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
		else
		{
			return AddCodeChunk(GetArithmeticResultType(A,B),TEXT("(%s - %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
	}

	virtual int32 Mul(int32 A,int32 B) OVERRIDE
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A),GetParameterUniformExpression(B),FMO_Mul),GetArithmeticResultType(A,B),TEXT("(%s * %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
		else
		{
			return AddCodeChunk(GetArithmeticResultType(A,B),TEXT("(%s * %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
	}

	virtual int32 Div(int32 A,int32 B) OVERRIDE
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A),GetParameterUniformExpression(B),FMO_Div),GetArithmeticResultType(A,B),TEXT("(%s / %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
		else
		{
			return AddCodeChunk(GetArithmeticResultType(A,B),TEXT("(%s / %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
	}

	virtual int32 Dot(int32 A,int32 B) OVERRIDE
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		FMaterialUniformExpression* ExpressionA = GetParameterUniformExpression(A);
		FMaterialUniformExpression* ExpressionB = GetParameterUniformExpression(B);

		EMaterialValueType TypeA = GetParameterType(A);
		EMaterialValueType TypeB = GetParameterType(B);
		if(ExpressionA && ExpressionB)
		{
			if (TypeA == MCT_Float && TypeB == MCT_Float)
			{
				return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(ExpressionA,ExpressionB,FMO_Mul),MCT_Float,TEXT("mul(%s,%s)"),*GetParameterCode(A),*GetParameterCode(B));
			}
			else
			{
				if (TypeA == TypeB)
				{
					return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(ExpressionA,ExpressionB,FMO_Dot),MCT_Float,TEXT("dot(%s,%s)"),*GetParameterCode(A),*GetParameterCode(B));
				}
				else
				{
					// Promote scalar (or truncate the bigger type)
					if (TypeA == MCT_Float || (TypeB != MCT_Float && GetNumComponents(TypeA) > GetNumComponents(TypeB)))
					{
						return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(ExpressionA,ExpressionB,FMO_Dot),MCT_Float,TEXT("dot(%s,%s)"),*CoerceParameter(A, TypeB),*GetParameterCode(B));
					}
					else
					{
						return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(ExpressionA,ExpressionB,FMO_Dot),MCT_Float,TEXT("dot(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B, TypeA));
					}
				}
			}
		}
		else
		{
			// Promote scalar (or truncate the bigger type)
			if (TypeA == MCT_Float || (TypeB != MCT_Float && GetNumComponents(TypeA) > GetNumComponents(TypeB)))
			{
				return AddCodeChunk(MCT_Float,TEXT("dot(%s, %s)"), *CoerceParameter(A, TypeB), *GetParameterCode(B));
			}
			else
			{
				return AddCodeChunk(MCT_Float,TEXT("dot(%s, %s)"), *GetParameterCode(A), *CoerceParameter(B, TypeA));
			}
		}
	}

	virtual int32 Cross(int32 A,int32 B) OVERRIDE
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		return AddCodeChunk(MCT_Float3,TEXT("cross(%s,%s)"),*CoerceParameter(A,MCT_Float3),*CoerceParameter(B,MCT_Float3));
	}

	virtual int32 Power(int32 Base,int32 Exponent) OVERRIDE
	{
		if(Base == INDEX_NONE || Exponent == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		FString ExponentCode = CoerceParameter(Exponent,MCT_Float);
		if (CodeChunks[MaterialProperty][ShaderFrequency][Exponent].UniformExpression && CodeChunks[MaterialProperty][ShaderFrequency][Exponent].UniformExpression->IsConstant())
		{
			//chop off the parenthesis
			FString NumericPortion = ExponentCode.Mid(1, ExponentCode.Len() - 2);
			float ExponentValue = FCString::Atof(*NumericPortion); 
			//check if the power was 1.0f to work around a xenon HLSL compiler bug in the Feb XDK
			//which incorrectly optimizes pow(x, 1.0f) as if it were pow(x, 0.0f).
			if (fabs(ExponentValue - 1.0f) < (float)KINDA_SMALL_NUMBER)
			{
				return Base;
			}
		}

		// use ClampedPow so artist are prevented to cause NAN creeping into the math
		return AddCodeChunk(GetParameterType(Base),TEXT("ClampedPow(%s,%s)"),*GetParameterCode(Base),*ExponentCode);
	}

	virtual int32 SquareRoot(int32 X) OVERRIDE
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionSquareRoot(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("sqrt(%s)"),*GetParameterCode(X));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),TEXT("sqrt(%s)"),*GetParameterCode(X));
		}
	}

	virtual int32 Length(int32 X) OVERRIDE
	{
		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X))
		{
			return AddUniformExpression(new FMaterialUniformExpressionLength(GetParameterUniformExpression(X)),MCT_Float,TEXT("length(%s)"),*GetParameterCode(X));
		}
		else
		{
			return AddCodeChunk(MCT_Float,TEXT("length(%s)"),*GetParameterCode(X));
		}
	}

	virtual int32 Lerp(int32 X,int32 Y,int32 A) OVERRIDE
	{
		if(X == INDEX_NONE || Y == INDEX_NONE || A == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType ResultType = GetArithmeticResultType(X,Y);
		EMaterialValueType AlphaType = ResultType == CodeChunks[MaterialProperty][ShaderFrequency][A].Type ? ResultType : MCT_Float1;
		return AddCodeChunk(ResultType,TEXT("lerp(%s,%s,%s)"),*CoerceParameter(X,ResultType),*CoerceParameter(Y,ResultType),*CoerceParameter(A,AlphaType));
	}

	virtual int32 Min(int32 A,int32 B) OVERRIDE
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionMin(GetParameterUniformExpression(A),GetParameterUniformExpression(B)),GetParameterType(A),TEXT("min(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
		else
		{
			return AddCodeChunk(GetParameterType(A),TEXT("min(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
	}

	virtual int32 Max(int32 A,int32 B) OVERRIDE
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionMax(GetParameterUniformExpression(A),GetParameterUniformExpression(B)),GetParameterType(A),TEXT("max(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
		else
		{
			return AddCodeChunk(GetParameterType(A),TEXT("max(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		}
	}

	virtual int32 Clamp(int32 X,int32 A,int32 B) OVERRIDE
	{
		if(X == INDEX_NONE || A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(GetParameterUniformExpression(X) && GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionClamp(GetParameterUniformExpression(X),GetParameterUniformExpression(A),GetParameterUniformExpression(B)),GetParameterType(X),TEXT("min(max(%s,%s),%s)"),*GetParameterCode(X),*CoerceParameter(A,GetParameterType(X)),*CoerceParameter(B,GetParameterType(X)));
		}
		else
		{
			return AddCodeChunk(GetParameterType(X),TEXT("min(max(%s,%s),%s)"),*GetParameterCode(X),*CoerceParameter(A,GetParameterType(X)),*CoerceParameter(B,GetParameterType(X)));
		}
	}

	virtual int32 ComponentMask(int32 Vector,bool R,bool G,bool B,bool A) OVERRIDE
	{
		if(Vector == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType	VectorType = GetParameterType(Vector);

		if(	A && (VectorType & MCT_Float) < MCT_Float4 ||
			B && (VectorType & MCT_Float) < MCT_Float3 ||
			G && (VectorType & MCT_Float) < MCT_Float2 ||
			R && (VectorType & MCT_Float) < MCT_Float1)
		{
			return Errorf(TEXT("Not enough components in (%s: %s) for component mask %u%u%u%u"),*GetParameterCode(Vector),DescribeType(GetParameterType(Vector)),R,G,B,A);
		}

		EMaterialValueType	ResultType;
		switch((R ? 1 : 0) + (G ? 1 : 0) + (B ? 1 : 0) + (A ? 1 : 0))
		{
		case 1: ResultType = MCT_Float; break;
		case 2: ResultType = MCT_Float2; break;
		case 3: ResultType = MCT_Float3; break;
		case 4: ResultType = MCT_Float4; break;
		default: 
			return Errorf(TEXT("Couldn't determine result type of component mask %u%u%u%u"),R,G,B,A);
		};

		return AddInlinedCodeChunk(
			ResultType,
			TEXT("%s.%s%s%s%s"),
			*GetParameterCode(Vector),
			R ? TEXT("r") : TEXT(""),
			// If VectorType is set to MCT_Float which means it could be any of the float types, assume it is a float1
			G ? (VectorType == MCT_Float ? TEXT("r") : TEXT("g")) : TEXT(""),
			B ? (VectorType == MCT_Float ? TEXT("r") : TEXT("b")) : TEXT(""),
			A ? (VectorType == MCT_Float ? TEXT("r") : TEXT("a")) : TEXT("")
			);
	}

	virtual int32 AppendVector(int32 A,int32 B) OVERRIDE
	{
		if(A == INDEX_NONE || B == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		int32 NumResultComponents = GetNumComponents(GetParameterType(A)) + GetNumComponents(GetParameterType(B));
		EMaterialValueType	ResultType = GetVectorType(NumResultComponents);

		if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
		{
			return AddUniformExpression(new FMaterialUniformExpressionAppendVector(GetParameterUniformExpression(A),GetParameterUniformExpression(B),GetNumComponents(GetParameterType(A))),ResultType,TEXT("MaterialFloat%u(%s,%s)"),NumResultComponents,*GetParameterCode(A),*GetParameterCode(B));
		}
		else
		{
			return AddInlinedCodeChunk(ResultType,TEXT("MaterialFloat%u(%s,%s)"),NumResultComponents,*GetParameterCode(A),*GetParameterCode(B));
		}
	}

	/**
	* Generate shader code for transforming a vector
	*/
	virtual int32 TransformVector(uint8 SourceCoordType,uint8 DestCoordType,int32 A) OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Domain && ShaderFrequency != SF_Vertex)
		{
			return NonPixelShaderExpressionError();
		}

		const EMaterialVectorCoordTransformSource SourceCoordinateSpace = (EMaterialVectorCoordTransformSource)SourceCoordType;
		const EMaterialVectorCoordTransform DestinationCoordinateSpace = (EMaterialVectorCoordTransform)DestCoordType;

		// Construct float3(0,0,x) out of the input if it is a scalar
		// This way artists can plug in a scalar and it will be treated as height, or a vector displacement
		if(A != INDEX_NONE && (GetType(A) & MCT_Float1) && SourceCoordinateSpace == TRANSFORMSOURCE_Tangent)
		{
			A = AppendVector(Constant2(0, 0), A);
		}

		int32 Domain = Material->GetMaterialDomain();
		if( (Domain != MD_Surface && Domain != MD_DeferredDecal) && 
			(SourceCoordinateSpace == TRANSFORMSOURCE_Tangent || SourceCoordinateSpace == TRANSFORMSOURCE_Local || DestCoordType == TRANSFORM_Tangent || DestCoordType == TRANSFORM_Tangent) )
		{
			return Errorf(TEXT("Local and tangent transforms are only supported in the Surface and Deferred Decal material domains!"));
		}

		if (ShaderFrequency != SF_Vertex)
		{
			bUsesTransformVector = true;
		}

		int32 Result = INDEX_NONE;
		if(A != INDEX_NONE)
		{
			int32 NumInputComponents = GetNumComponents(GetParameterType(A));
			// only allow float3/float4 transforms
			if( NumInputComponents < 3 )
			{
				Result = Errorf(TEXT("input must be a vector (%s: %s) or a scalar (if source is Tangent)"),*GetParameterCode(A),DescribeType(GetParameterType(A)));
			}
			else if ((SourceCoordinateSpace == TRANSFORMSOURCE_World && DestinationCoordinateSpace == TRANSFORM_World)
				|| (SourceCoordinateSpace == TRANSFORMSOURCE_Local && DestinationCoordinateSpace == TRANSFORM_Local)
				|| (SourceCoordinateSpace == TRANSFORMSOURCE_Tangent && DestinationCoordinateSpace == TRANSFORM_Tangent)
				|| (SourceCoordinateSpace == TRANSFORMSOURCE_View && DestinationCoordinateSpace == TRANSFORM_View))
			{
				// Pass through
				Result = A;
			}
			else 
			{
				// code string to transform the input vector
				FString CodeStr;
				if (SourceCoordinateSpace == TRANSFORMSOURCE_Tangent)
				{
					switch( DestinationCoordinateSpace )
					{
					case TRANSFORM_Local:
						// transform from tangent to local space
						if( ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Vertex )
						{
							return Errorf(TEXT("Local space in only supported for vertex or pixel shader!"));
						}
						CodeStr = FString(TEXT("TransformTangentVectorToLocal(Parameters,%s)"));
						break;
					case TRANSFORM_World:
						// transform from tangent to world space
						if(ShaderFrequency == SF_Domain)
						{
							// domain shader uses a prescale value to preserve scaling factor on WorldTransform	when sampling a displacement map
							CodeStr = FString(TEXT("TransformTangentVectorToWorld_PreScaled(Parameters,%s)"));
						}
						else
						{
							CodeStr = FString(TEXT("TransformTangentVectorToWorld(Parameters.TangentToWorld,%s)"));
						}
						break;
					case TRANSFORM_View:
						// transform from tangent to view space
						if( ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Vertex )
						{
							return Errorf(TEXT("View space in only supported for vertex or pixel shader!"));
						}
						CodeStr = FString(TEXT("TransformTangentVectorToView(Parameters,%s)"));
						break;
					default:
						UE_LOG(LogMaterial, Fatal, TEXT("Invalid DestCoordType. See EMaterialVectorCoordTransform") );
					}
				}
				else if (SourceCoordinateSpace == TRANSFORMSOURCE_Local)
				{
					if( ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Vertex )
					{
						return Errorf(TEXT("Local space in only supported for vertex or pixel shader!"));
					}

					switch( DestinationCoordinateSpace )
					{
					case TRANSFORM_Tangent:
						CodeStr = FString(TEXT("TransformLocalVectorToTangent(Parameters,%s)"));
						break;
					case TRANSFORM_World:
						CodeStr = FString(TEXT("TransformLocalVectorToWorld(Parameters,%s)"));
						break;
					case TRANSFORM_View:
						CodeStr = FString(TEXT("TransformLocalVectorToView(%s)"));
						break;
					default:
						UE_LOG(LogMaterial, Fatal, TEXT("Invalid DestCoordType. See EMaterialVectorCoordTransform") );
					}
				}
				else if (SourceCoordinateSpace == TRANSFORMSOURCE_World)
				{
					switch( DestinationCoordinateSpace )
					{
					case TRANSFORM_Tangent:
						CodeStr = FString(TEXT("TransformWorldVectorToTangent(Parameters.TangentToWorld,%s)"));
						break;
					case TRANSFORM_Local:
						if( ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Vertex )
						{
							return Errorf(TEXT("Local space in only supported for vertex or pixel shader!"));
						}
						CodeStr = FString(TEXT("TransformWorldVectorToLocal(%s)"));
						break;
					case TRANSFORM_View:
						if( ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Vertex )
						{
							return Errorf(TEXT("View space in only supported for vertex or pixel shader!"));
						}
						CodeStr = FString(TEXT("TransformWorldVectorToView(%s)"));
						break;
					default:
						UE_LOG(LogMaterial, Fatal, TEXT("Invalid DestCoordType. See EMaterialVectorCoordTransform") );
					}
				}
				else if (SourceCoordinateSpace == TRANSFORMSOURCE_View)
				{
					switch( DestinationCoordinateSpace )
					{
					case TRANSFORM_Tangent:
						CodeStr = FString(TEXT("TransformWorldVectorToTangent(Parameters.TangentToWorld,TransformViewVectorToWorld(%s))"));
						break;
					case TRANSFORM_Local:
						if( ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Vertex )
						{
							return Errorf(TEXT("Local space in only supported for vertex or pixel shader!"));
						}
						CodeStr = FString(TEXT("TransformViewVectorToLocal(%s)"));
						break;
					case TRANSFORM_World:
						if( ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Vertex )
						{
							return Errorf(TEXT("View space in only supported for vertex or pixel shader!"));
						}
						CodeStr = FString(TEXT("TransformViewVectorToWorld(%s)"));
						break;
					default:
						UE_LOG(LogMaterial, Fatal, TEXT("Invalid DestCoordType. See EMaterialVectorCoordTransform") );
					}
				}
				else
				{
					UE_LOG(LogMaterial, Fatal, TEXT("Invalid SourceCoordType. See EMaterialVectorCoordTransformSource") );
				}

				// we are only transforming vectors (not points) so only return a float3
				Result = AddCodeChunk(
					MCT_Float3,
					*CodeStr,
					*CoerceParameter(A,MCT_Float3)
					);
			}
		}
		return Result; 
	}

	/**
	* Generate shader code for transforming a position
	*
	* @param	CoordType - type of transform to apply. see EMaterialExpressionTransformPosition 
	* @param	A - index for input vector parameter's code
	*/
	virtual int32 TransformPosition(uint8 SourceCoordType, uint8 DestCoordType, int32 A) OVERRIDE
	{
		const EMaterialPositionTransformSource SourceCoordinateSpace = (EMaterialPositionTransformSource)SourceCoordType;
		const EMaterialPositionTransformSource DestinationCoordinateSpace = (EMaterialPositionTransformSource)DestCoordType;

		int32 Result = INDEX_NONE;

		if (SourceCoordinateSpace == DestinationCoordinateSpace)
		{
			Result = A;
		}
		else if (A != INDEX_NONE)
		{
			// code string to transform the input vector
			FString CodeStr;

			if (SourceCoordinateSpace == TRANSFORMPOSSOURCE_Local)
			{
				CodeStr = FString(TEXT("TransformLocalPositionToWorld(Parameters,%s)"));
			}
			else if (SourceCoordinateSpace == TRANSFORMPOSSOURCE_World)
			{
				CodeStr = FString(TEXT("TransformWorldPositionToLocal(%s)"));
			}
				
			Result = AddCodeChunk(
				MCT_Float3,
				*CodeStr,
				*CoerceParameter(A,MCT_Float3)
				);
		}
		return Result; 
	}

	virtual int32 DynamicParameter() OVERRIDE
	{
		if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonVertexOrPixelShaderExpressionError();
		}

		bNeedsParticleDynamicParameter = true;

		return AddInlinedCodeChunk(
			MCT_Float4,
			TEXT("Parameters.Particle.DynamicParameter")
			);
	}

	virtual int32 LightmapUVs() OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}

		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM3) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		bUsesLightmapUVs = true;

		int32 ResultIdx = INDEX_NONE;
		FString CodeChunk = FString::Printf(TEXT("GetLightmapUVs(Parameters)"));
		ResultIdx = AddCodeChunk(
			MCT_Float2,
			*CodeChunk
			);
		return ResultIdx;
	}

	virtual int32 LightmassReplace(int32 Realtime, int32 Lightmass) OVERRIDE { return Realtime; }

	virtual int32 GIReplace(int32 Direct, int32 StaticIndirect, int32 DynamicIndirect) OVERRIDE 
	{ 
		if(Direct == INDEX_NONE || DynamicIndirect == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType ResultType = GetArithmeticResultType(Direct, DynamicIndirect);

		return AddCodeChunk(ResultType,TEXT("(GetGIReplaceState() ? (%s) : (%s))"), *GetParameterCode(DynamicIndirect), *GetParameterCode(Direct));
	}

	virtual int32 ObjectOrientation() OVERRIDE
	{ 
		return AddInlinedCodeChunk(MCT_Float3,TEXT("Primitive.ObjectOrientation.xyz"));	
	}

	virtual int32 RotateAboutAxis(int32 NormalizedRotationAxisAndAngleIndex, int32 PositionOnAxisIndex, int32 PositionIndex) OVERRIDE
	{
		if (NormalizedRotationAxisAndAngleIndex == INDEX_NONE
			|| PositionOnAxisIndex == INDEX_NONE
			|| PositionIndex == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		else
		{
			return AddCodeChunk(
				MCT_Float3,
				TEXT("RotateAboutAxis(%s,%s,%s)"),
				*CoerceParameter(NormalizedRotationAxisAndAngleIndex,MCT_Float4),
				*CoerceParameter(PositionOnAxisIndex,MCT_Float3),
				*CoerceParameter(PositionIndex,MCT_Float3)
				);	
		}
	}

	virtual int32 TwoSidedSign() OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}
		return AddInlinedCodeChunk(MCT_Float,TEXT("Parameters.TwoSidedSign"));	
	}

	virtual int32 VertexNormal() OVERRIDE
	{
		return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.TangentToWorld[2]"));	
	}

	virtual int32 PixelNormalWS() OVERRIDE
	{
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
		{
			return NonPixelShaderExpressionError();
		}
		if(MaterialProperty == MP_Normal)
		{
			return Errorf(TEXT("Invalid node PixelNormalWS used for Normal input."));
		}
		return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.WorldNormal"));	
	}

	virtual int32 DDX( int32 X ) OVERRIDE
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM3) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (ShaderFrequency == SF_Compute)
		{
			// running a material in a compute shader pass (e.g. when using SVOGI)
			return AddInlinedCodeChunk(MCT_Float, TEXT("0"));	
		}

		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}

		return AddCodeChunk(GetParameterType(X),TEXT("ddx(%s)"),*GetParameterCode(X));
	}

	virtual int32 DDY( int32 X ) OVERRIDE
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM3) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(X == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (ShaderFrequency == SF_Compute)
		{
			// running a material in a compute shader pass
			return AddInlinedCodeChunk(MCT_Float, TEXT("0"));	
		}
		if (ShaderFrequency != SF_Pixel)
		{
			return NonPixelShaderExpressionError();
		}

		return AddCodeChunk(GetParameterType(X),TEXT("ddy(%s)"),*GetParameterCode(X));
	}

	virtual int32 AntialiasedTextureMask(int32 Tex, int32 UV, float Threshold, uint8 Channel) OVERRIDE
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM3) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (Tex == INDEX_NONE || UV == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		int32 ThresholdConst = Constant(Threshold);
		int32 ChannelConst = Constant(Channel);
		FString TextureName = CoerceParameter(Tex, GetParameterType(Tex));

		return AddCodeChunk(MCT_Float, 
			TEXT("AntialiasedTextureMask(%s,%sSampler,%s,%s,%s)"), 
			*GetParameterCode(Tex),
			*TextureName,
			*GetParameterCode(UV),
			*GetParameterCode(ThresholdConst),
			*GetParameterCode(ChannelConst));
	}

	virtual int32 DepthOfFieldFunction(int32 Depth, int32 FunctionValueIndex) OVERRIDE
	{
		if (ShaderFrequency == SF_Hull)
		{
			return Errorf(TEXT("Invalid node DepthOfFieldFunction used in hull shader input!"));
		}

		if (Depth == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		return AddCodeChunk(MCT_Float, 
			TEXT("MaterialExpressionDepthOfFieldFunction(%s, %d)"), 
			*GetParameterCode(Depth), FunctionValueIndex);
	}

	virtual int32 Noise(int32 Position, float Scale, int32 Quality, uint8 NoiseFunction, bool bTurbulence, int32 Levels, float OutputMin, float OutputMax, float LevelScale, int32 FilterWidth) OVERRIDE
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM3) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if(Position == INDEX_NONE || FilterWidth == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		// to limit performance problems due to values outside reasonable range
		Levels = FMath::Clamp(Levels, 1, 10);

		int32 ScaleConst = Constant(Scale);
		int32 QualityConst = Constant(Quality);
		int32 NoiseFunctionConst = Constant(NoiseFunction);
		int32 TurbulenceConst = Constant(bTurbulence);
		int32 LevelsConst = Constant(Levels);
		int32 OutputMinConst = Constant(OutputMin);
		int32 OutputMaxConst = Constant(OutputMax);
		int32 LevelScaleConst = Constant(LevelScale);

		return AddCodeChunk(MCT_Float, 
			TEXT("MaterialExpressionNoise(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)"), 
			*GetParameterCode(Position),
			*GetParameterCode(ScaleConst),
			*GetParameterCode(QualityConst),
			*GetParameterCode(NoiseFunctionConst),
			*GetParameterCode(TurbulenceConst),
			*GetParameterCode(LevelsConst),
			*GetParameterCode(OutputMinConst),
			*GetParameterCode(OutputMaxConst),
			*GetParameterCode(LevelScaleConst),
			*GetParameterCode(FilterWidth));
	}

	virtual int32 BlackBody( int32 Temp ) OVERRIDE
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM3) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if( Temp == INDEX_NONE )
		{
			return INDEX_NONE;
		}

		return AddCodeChunk( MCT_Float3, TEXT("MaterialExpressionBlackBody(%s)"), *GetParameterCode(Temp) );
	}

	virtual int32 AtmosphericFogColor( int32 WorldPosition ) OVERRIDE
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM3) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		bUsesAtmosphericFog = true;
		if( WorldPosition == INDEX_NONE )
		{
			return AddCodeChunk( MCT_Float4, TEXT("MaterialExpressionAtmosphericFog(Parameters, Parameters.WorldPosition)"));
		}
		else
		{
			return AddCodeChunk( MCT_Float4, TEXT("MaterialExpressionAtmosphericFog(Parameters, %s)"), *GetParameterCode(WorldPosition) );
		}
	}

	virtual int32 CustomExpression( class UMaterialExpressionCustom* Custom, TArray<int32>& CompiledInputs ) OVERRIDE
	{
		int32 ResultIdx = INDEX_NONE;

		FString OutputTypeString;
		EMaterialValueType OutputType;
		switch( Custom->OutputType )
		{
		case CMOT_Float2:
			OutputType = MCT_Float2;
			OutputTypeString = TEXT("MaterialFloat2");
			break;
		case CMOT_Float3:
			OutputType = MCT_Float3;
			OutputTypeString = TEXT("MaterialFloat3");
			break;
		case CMOT_Float4:
			OutputType = MCT_Float4;
			OutputTypeString = TEXT("MaterialFloat4");
			break;
		default:
			OutputType = MCT_Float;
			OutputTypeString = TEXT("MaterialFloat");
			break;
		}

		// Declare implementation function
		FString InputParamDecl;
		check( Custom->Inputs.Num() == CompiledInputs.Num() );
		for( int32 i = 0; i < Custom->Inputs.Num(); i++ )
		{
			// skip over unnamed inputs
			if( Custom->Inputs[i].InputName.Len()==0 )
			{
				continue;
			}
			InputParamDecl += TEXT(",");
			switch(GetParameterType(CompiledInputs[i]))
			{
			case MCT_Float:
			case MCT_Float1:
				InputParamDecl += TEXT("MaterialFloat ");
				break;
			case MCT_Float2:
				InputParamDecl += TEXT("MaterialFloat2 ");
				break;
			case MCT_Float3:
				InputParamDecl += TEXT("MaterialFloat3 ");
				break;
			case MCT_Float4:
				InputParamDecl += TEXT("MaterialFloat4 ");
				break;
			case MCT_Texture2D:
				InputParamDecl += TEXT("sampler2D ");
				break;
			default:
				return Errorf(TEXT("Bad type %s for %s input %s"),DescribeType(GetParameterType(CompiledInputs[i])), *Custom->Description, *Custom->Inputs[i].InputName);
				break;
			}
			InputParamDecl += Custom->Inputs[i].InputName;
		}
		int32 CustomExpressionIndex = CustomExpressionImplementations.Num();
		FString Code = Custom->Code;
		if( !Code.Contains(TEXT("return")) )
		{
			Code = FString(TEXT("return "))+Code+TEXT(";");
		}
		Code.ReplaceInline(TEXT("\n"),TEXT("\r\n"));
		FString ImplementationCode = FString::Printf(TEXT("%s CustomExpression%d(FMaterial%sParameters Parameters%s)\r\n{\r\n%s\r\n}\r\n"), *OutputTypeString, CustomExpressionIndex, ShaderFrequency==SF_Vertex?TEXT("Vertex"):TEXT("Pixel"), *InputParamDecl, *Code);
		CustomExpressionImplementations.Add( ImplementationCode );

		// Add call to implementation function
		FString CodeChunk = FString::Printf(TEXT("CustomExpression%d(Parameters"), CustomExpressionIndex);
		for( int32 i = 0; i < CompiledInputs.Num(); i++ )
		{
			// skip over unnamed inputs
			if( Custom->Inputs[i].InputName.Len()==0 )
			{
				continue;
			}
			CodeChunk += TEXT(",");
			CodeChunk += *GetParameterCode(CompiledInputs[i]);
		}
		CodeChunk += TEXT(")");

		ResultIdx = AddCodeChunk(
			OutputType,
			*CodeChunk
			);
		return ResultIdx;
	}

	/**
	 * Adds code to return a random value shared by all geometry for any given instanced static mesh
	 *
	 * @return	Code index
	 */
	virtual int32 PerInstanceRandom() OVERRIDE
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM3) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Vertex)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		else
		{
			return AddInlinedCodeChunk(MCT_Float, TEXT("GetPerInstanceRandom(Parameters)"));
		}
	}

	/**
	 * Returns a mask that either enables or disables selection on a per-instance basis when instancing
	 *
	 * @return	Code index
	 */
	virtual int32 PerInstanceFadeAmount() OVERRIDE
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM3) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Vertex)
		{
			return NonVertexOrPixelShaderExpressionError();
		}
		else
		{
			return AddInlinedCodeChunk(MCT_Float, TEXT("GetPerInstanceFadeAmount(Parameters)"));
		}
	}

	virtual int32 SpeedTree(ESpeedTreeGeometryType GeometryType, ESpeedTreeWindType WindType, ESpeedTreeLODType LODType, float BillboardThreshold) OVERRIDE 
	{ 
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM3) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if (ShaderFrequency != SF_Vertex)
		{
			return NonVertexShaderExpressionError();
		}
		else
		{
			bUsesSpeedTree = true;

			NumUserVertexTexCoords = FMath::Max<uint32>(NumUserVertexTexCoords, 6);

			return AddCodeChunk(MCT_Float3, TEXT("GetSpeedTreeVertexOffset(Parameters, %d, %d, %d, %g)"), GeometryType, WindType, LODType, BillboardThreshold);
		}
	}

	/**
	 * Adds code for texture coordinate offset to localize large UV
	 *
	 * @return	Code index
	 */
	virtual int32 TextureCoordinateOffset() OVERRIDE
	{
		if (FeatureLevel == ERHIFeatureLevel::ES2 && ShaderFrequency == SF_Vertex)
		{
			return AddInlinedCodeChunk(MCT_Float2, TEXT("Parameters.TexCoordOffset"));
		}
		else
		{
			return Constant(0.f);
		}
	}

	/**Experimental access to the EyeAdaptation RT for Post Process materials. Can be one frame behind depending on the value of BlendableLocation. */
	virtual int32 EyeAdaptation() OVERRIDE
	{
		if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM5) == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		if( ShaderFrequency != SF_Pixel )
		{
			NonPixelShaderExpressionError();
		}

		MaterialCompilationOutput.bUsesEyeAdaptation = true;

		return AddInlinedCodeChunk(MCT_Float, TEXT("EyeAdaptationLookup()"));
	}
};

#endif
