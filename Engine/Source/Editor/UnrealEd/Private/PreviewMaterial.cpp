// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Editor/MaterialEditor/Public/MaterialEditorModule.h"

/**
 * Class for rendering the material on the preview mesh in the Material Editor
 */
class FPreviewMaterial : public FMaterialResource, public FMaterialRenderProxy
{
public:
	FPreviewMaterial()
	:	FMaterialResource()
	{
	}
	
	~FPreviewMaterial()
	{
		BeginReleaseResource(this);
		FlushRenderingCommands();
	}

	/**
	 * Should the shader for this material with the given platform, shader type and vertex 
	 * factory type combination be compiled
	 *
	 * @param Platform		The platform currently being compiled for
	 * @param ShaderType	Which shader is being compiled
	 * @param VertexFactory	Which vertex factory is being compiled (can be NULL)
	 *
	 * @return true if the shader should be compiled
	 */
	virtual bool ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const
	{
		// only generate the needed shaders (which should be very restrictive for fast recompiling during editing)
		// @todo: Add a FindShaderType by fname or something

		// Always allow HitProxy shaders.
		if (FCString::Stristr(ShaderType->GetName(), TEXT("HitProxy")))
		{
			return true;
		}

		// we only need local vertex factory for the preview static mesh
		if (VertexFactoryType != FindVertexFactoryType(FName(TEXT("FLocalVertexFactory"), FNAME_Find)))
		{
			//cache for gpu skinned vertex factory if the material allows it
			//this way we can have a preview skeletal mesh
			if (!IsUsedWithSkeletalMesh() ||
				(VertexFactoryType != FindVertexFactoryType(FName(TEXT("TGPUSkinVertexFactoryfalse"), FNAME_Find)) &&
				VertexFactoryType != FindVertexFactoryType(FName(TEXT("TGPUSkinVertexFactorytrue"), FNAME_Find))))
			{
				return false;
			}
		}

		// look for any of the needed type
		bool bShaderTypeMatches = false;

		// For FMaterialResource::GetRepresentativeInstructionCounts
		if (FCString::Stristr(ShaderType->GetName(), TEXT("BasePassPSTDistanceFieldShadowsAndLightMapPolicyHQ")))
		{
			bShaderTypeMatches = true;
		}
		else if (FCString::Stristr(ShaderType->GetName(), TEXT("BasePassPSFNoLightMapPolicy")))
		{
			bShaderTypeMatches = true;
		}
		else if (FCString::Stristr(ShaderType->GetName(), TEXT("BasePassVSFCachedVolumeIndirectLightingPolicy"))
			|| FCString::Stristr(ShaderType->GetName(), TEXT("BasePassPSFCachedVolumeIndirectLightingPolicy"))
			|| FCString::Stristr(ShaderType->GetName(), TEXT("BasePassHSFCachedVolumeIndirectLightingPolicy"))
			|| FCString::Stristr(ShaderType->GetName(), TEXT("BasePassDSFCachedVolumeIndirectLightingPolicy")))
		{
			bShaderTypeMatches = true;
		}
		else if (FCString::Stristr(ShaderType->GetName(), TEXT("BasePassPSFSelfShadowedTranslucencyPolicy")))
		{
			bShaderTypeMatches = true;
		}
		// Pick tessellation shader based on material settings
		else if(FCString::Stristr(ShaderType->GetName(), TEXT("BasePassVSFNoLightMapPolicy")) ||
			FCString::Stristr(ShaderType->GetName(), TEXT("BasePassHSFNoLightMapPolicy")) ||
			FCString::Stristr(ShaderType->GetName(), TEXT("BasePassDSFNoLightMapPolicy")))
		{
			bShaderTypeMatches = true;
		}
		else if (FCString::Stristr(ShaderType->GetName(), TEXT("DepthOnly")))
		{
			bShaderTypeMatches = true;
		}
		else if (FCString::Stristr(ShaderType->GetName(), TEXT("ShadowDepth")))
		{
			bShaderTypeMatches = true;
		}
		else if (FCString::Stristr(ShaderType->GetName(), TEXT("TDistortion")))
		{
			bShaderTypeMatches = true;
		}
		else if (FCString::Stristr(ShaderType->GetName(), TEXT("TBasePassForForwardShading")))
		{
			bShaderTypeMatches = true;
		}

		return bShaderTypeMatches;
	}

	/**
	 * Should shaders compiled for this material be saved to disk?
	 */
	virtual bool IsPersistent() const { return false; }

	// FMaterialRenderProxy interface
	virtual const FMaterial* GetMaterial(ERHIFeatureLevel::Type FeatureLevel) const
	{
		if(GetRenderingThreadShaderMap())
		{
			return this;
		}
		else
		{
			return UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false)->GetMaterial(FeatureLevel);
		}
	}

	virtual bool GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		return Material->GetRenderProxy(0)->GetVectorValue(ParameterName, OutValue, Context);
	}

	virtual bool GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const
	{
		return Material->GetRenderProxy(0)->GetScalarValue(ParameterName, OutValue, Context);
	}

	virtual bool GetTextureValue(const FName ParameterName,const UTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		return Material->GetRenderProxy(0)->GetTextureValue(ParameterName,OutValue,Context);
	}
};

/** Implementation of Preview Material functions*/
UPreviewMaterial::UPreviewMaterial(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

FMaterialResource* UPreviewMaterial::AllocateResource()
{
	return new FPreviewMaterial();
}

UMaterialEditorInstanceConstant::UMaterialEditorInstanceConstant(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UMaterialEditorInstanceConstant::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (SourceInstance)
	{
		UProperty* PropertyThatChanged = PropertyChangedEvent.Property;

		// we will unregister and register components to update materials so we have to notify NavigationSystem that this is "fake" operation and we don't have to update NavMesh
		for (auto It=GEditor->GetWorldContexts().CreateConstIterator(); It; ++It)
		{
			if (UWorld* World = It->World())
			{
				if (World->GetNavigationSystem() != NULL)
					World->GetNavigationSystem()->BeginFakeComponentChanges();
			}
		}

		if(PropertyThatChanged && PropertyThatChanged->GetName()==TEXT("Parent") )
		{
			UpdateSourceInstanceParent();

			FGlobalComponentReregisterContext RecreateComponents;
			// Fully update static parameters before re-registering the scene's components
			SetSourceInstance(SourceInstance);
		}

		CopyToSourceInstance();

		// Tell our source instance to update itself so the preview updates.
		SourceInstance->PostEditChangeProperty(PropertyChangedEvent);

		//components are updated and navigation system can work normally
		for (auto It=GEditor->GetWorldContexts().CreateConstIterator(); It; ++It)
		{
			if (UWorld* World = It->World())
			{
				if (World->GetNavigationSystem() != NULL)
					World->GetNavigationSystem()->EndFakeComponentChanges();
			}
		}
	}
}

void  UMaterialEditorInstanceConstant::AssignParameterToGroup(UMaterial* ParentMaterial, UDEditorParameterValue * ParameterValue)
{
	check(ParentMaterial);
	check(ParameterValue);

	FName ParameterGroupName;
	ParentMaterial->GetGroupName(ParameterValue->ParameterName, ParameterGroupName);

	if (ParameterGroupName == TEXT("") || ParameterGroupName == TEXT("None"))
	{
		if (bUseOldStyleMICEditorGroups == true)
		{
			if (Cast<UDEditorVectorParameterValue>( ParameterValue))
			{
				ParameterGroupName = TEXT("Vector Parameter Values");
			}
			else if (Cast<UDEditorTextureParameterValue>( ParameterValue))
			{
				ParameterGroupName = TEXT("Texture Parameter Values");
			}
			else if (Cast<UDEditorScalarParameterValue>( ParameterValue))
			{
				ParameterGroupName = TEXT("Scalar Parameter Values");
			}
			else if (Cast<UDEditorStaticSwitchParameterValue>( ParameterValue))
			{
				ParameterGroupName = TEXT("Static Switch Parameter Values");
			}
			else if (Cast<UDEditorStaticComponentMaskParameterValue>( ParameterValue))
			{
				ParameterGroupName = TEXT("Static Component Mask Parameter Values");
			}
			else if (Cast<UDEditorFontParameterValue>( ParameterValue))
			{
				ParameterGroupName = TEXT("Font Parameter Values");
			}
			else
			{
				ParameterGroupName = TEXT("None");
			}
		}
		else
		{
			ParameterGroupName = TEXT("None");
		}

	}

	FEditorParameterGroup& CurrentGroup = GetParameterGroup(ParameterGroupName);		
	ParameterValue->SetFlags( RF_Transactional );
	CurrentGroup.Parameters.Add(ParameterValue);
}

FEditorParameterGroup&  UMaterialEditorInstanceConstant::GetParameterGroup(FName & ParameterGroup)
{	
	if (ParameterGroup == TEXT(""))
	{
		ParameterGroup = TEXT("None");
	}
	for (int32 i = 0; i < ParameterGroups.Num(); i ++)
	{
		FEditorParameterGroup& Group= ParameterGroups[i];
		if (Group.GroupName == ParameterGroup)
		{
			return Group;
		}
	}
	int32 ind = ParameterGroups.AddZeroed(1);
	FEditorParameterGroup& Group= ParameterGroups[ind];
	Group.GroupName = ParameterGroup;
	return Group;
}

void UMaterialEditorInstanceConstant::RegenerateArrays()
{
	VisibleExpressions.Empty();
	ParameterGroups.Empty();
	if(Parent)
	{	
		// Only operate on base materials
		UMaterial* ParentMaterial = Parent->GetMaterial();
		SourceInstance->UpdateParameterNames();	// Update any parameter names that may have changed.

		// Loop through all types of parameters for this material and add them to the parameter arrays.
		TArray<FName> ParameterNames;
		TArray<FGuid> Guids;
		ParentMaterial->GetAllVectorParameterNames(ParameterNames, Guids);

		// Vector Parameters.

		for(int32 ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			UDEditorVectorParameterValue & ParameterValue = *(ConstructObject<UDEditorVectorParameterValue>(UDEditorVectorParameterValue::StaticClass()));
			FName ParameterName = ParameterNames[ParameterIdx];
			FLinearColor Value;

			ParameterValue.bOverride = false;
			ParameterValue.ParameterName = ParameterName;
			ParameterValue.ExpressionId = Guids[ParameterIdx];

			if(SourceInstance->GetVectorParameterValue(ParameterName, Value))
			{
				ParameterValue.ParameterValue = Value;
			}

			// @todo: This is kind of slow, maybe store these in a map for lookup?
			// See if this keyname exists in the source instance.
			for(int32 VectorParameterIdx=0; VectorParameterIdx<SourceInstance->VectorParameterValues.Num(); VectorParameterIdx++)
			{
				FVectorParameterValue& SourceParam = SourceInstance->VectorParameterValues[VectorParameterIdx];
				if(ParameterName==SourceParam.ParameterName)
				{
					ParameterValue.bOverride = true;
					ParameterValue.ParameterValue = SourceParam.ParameterValue;
				}
			}
			AssignParameterToGroup(ParentMaterial, Cast<UDEditorParameterValue>(&ParameterValue));
		}
		// Scalar Parameters.
		ParentMaterial->GetAllScalarParameterNames(ParameterNames, Guids);
		for(int32 ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{			
			UDEditorScalarParameterValue & ParameterValue = *(ConstructObject<UDEditorScalarParameterValue>(UDEditorScalarParameterValue::StaticClass()));
			FName ParameterName = ParameterNames[ParameterIdx];
			float Value;

			ParameterValue.bOverride = false;
			ParameterValue.ParameterName = ParameterName;
			ParameterValue.ExpressionId = Guids[ParameterIdx];

			if(SourceInstance->GetScalarParameterValue(ParameterName, Value))
			{
				ParameterValue.ParameterValue = Value;
			}


			// @todo: This is kind of slow, maybe store these in a map for lookup?
			// See if this keyname exists in the source instance.
			for(int32 ScalarParameterIdx=0; ScalarParameterIdx<SourceInstance->ScalarParameterValues.Num(); ScalarParameterIdx++)
			{
				FScalarParameterValue& SourceParam = SourceInstance->ScalarParameterValues[ScalarParameterIdx];
				if(ParameterName==SourceParam.ParameterName)
				{
					ParameterValue.bOverride = true;
					ParameterValue.ParameterValue = SourceParam.ParameterValue;
				}
			}
			AssignParameterToGroup(ParentMaterial, Cast<UDEditorParameterValue>(&ParameterValue));
		}

		// Texture Parameters.
		ParentMaterial->GetAllTextureParameterNames(ParameterNames, Guids);
		for(int32 ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{			
			UDEditorTextureParameterValue& ParameterValue = *(ConstructObject<UDEditorTextureParameterValue>(UDEditorTextureParameterValue::StaticClass()));
			FName ParameterName = ParameterNames[ParameterIdx];
			UTexture* Value;

			ParameterValue.bOverride = false;
			ParameterValue.ParameterName = ParameterName;
			ParameterValue.ExpressionId = Guids[ParameterIdx];

			if(SourceInstance->GetTextureParameterValue(ParameterName, Value))
			{
				ParameterValue.ParameterValue = Value;
			}


			// @todo: This is kind of slow, maybe store these in a map for lookup?
			// See if this keyname exists in the source instance.
			for(int32 TextureParameterIdx=0; TextureParameterIdx<SourceInstance->TextureParameterValues.Num(); TextureParameterIdx++)
			{
				FTextureParameterValue& SourceParam = SourceInstance->TextureParameterValues[TextureParameterIdx];
				if(ParameterName==SourceParam.ParameterName)
				{
					ParameterValue.bOverride = true;
					ParameterValue.ParameterValue = SourceParam.ParameterValue;
				}
			}
			AssignParameterToGroup(ParentMaterial, Cast<UDEditorParameterValue>(&ParameterValue));
		}

		// Font Parameters.
		ParentMaterial->GetAllFontParameterNames(ParameterNames, Guids);
		for(int32 ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			UDEditorFontParameterValue& ParameterValue = *(ConstructObject<UDEditorFontParameterValue>(UDEditorFontParameterValue::StaticClass()));
			FName ParameterName = ParameterNames[ParameterIdx];
			UFont* FontValue;
			int32 FontPage;

			ParameterValue.bOverride = false;
			ParameterValue.ParameterName = ParameterName;
			ParameterValue.ExpressionId = Guids[ParameterIdx];

			if(SourceInstance->GetFontParameterValue(ParameterName, FontValue,FontPage))
			{
				ParameterValue.FontValue = FontValue;
				ParameterValue.FontPage = FontPage;
			}


			// @todo: This is kind of slow, maybe store these in a map for lookup?
			// See if this keyname exists in the source instance.
			for(int32 FontParameterIdx=0; FontParameterIdx<SourceInstance->FontParameterValues.Num(); FontParameterIdx++)
			{
				FFontParameterValue& SourceParam = SourceInstance->FontParameterValues[FontParameterIdx];
				if(ParameterName==SourceParam.ParameterName)
				{
					ParameterValue.bOverride = true;
					ParameterValue.FontValue = SourceParam.FontValue;
					ParameterValue.FontPage = SourceParam.FontPage;
				}
			}
			AssignParameterToGroup(ParentMaterial, Cast<UDEditorParameterValue>(&ParameterValue));
		}

		// Get all static parameters from the source instance.  This will handle inheriting parent values.
		FStaticParameterSet SourceStaticParameters;
		SourceInstance->GetStaticParameterValues(SourceStaticParameters);

		// Copy Static Switch Parameters
		for(int32 ParameterIdx=0; ParameterIdx<SourceStaticParameters.StaticSwitchParameters.Num(); ParameterIdx++)
		{			
			FStaticSwitchParameter StaticSwitchParameterValue = FStaticSwitchParameter(SourceStaticParameters.StaticSwitchParameters[ParameterIdx]);
			UDEditorStaticSwitchParameterValue& ParameterValue = *(ConstructObject<UDEditorStaticSwitchParameterValue>(UDEditorStaticSwitchParameterValue::StaticClass()));
			ParameterValue.ParameterValue =StaticSwitchParameterValue.Value;
			ParameterValue.bOverride =StaticSwitchParameterValue.bOverride;
			ParameterValue.ParameterName =StaticSwitchParameterValue.ParameterName;
			ParameterValue.ExpressionId= StaticSwitchParameterValue.ExpressionGUID;

			AssignParameterToGroup(ParentMaterial, Cast<UDEditorParameterValue>(&ParameterValue));
		}

		// Copy Static Component Mask Parameters

		for(int32 ParameterIdx=0; ParameterIdx<SourceStaticParameters.StaticComponentMaskParameters.Num(); ParameterIdx++)
		{
			FStaticComponentMaskParameter StaticComponentMaskParameterValue = FStaticComponentMaskParameter(SourceStaticParameters.StaticComponentMaskParameters[ParameterIdx]);
			UDEditorStaticComponentMaskParameterValue& ParameterValue = *(ConstructObject<UDEditorStaticComponentMaskParameterValue>(UDEditorStaticComponentMaskParameterValue::StaticClass()));
			ParameterValue.ParameterValue.R = StaticComponentMaskParameterValue.R;
			ParameterValue.ParameterValue.G = StaticComponentMaskParameterValue.G;
			ParameterValue.ParameterValue.B = StaticComponentMaskParameterValue.B;
			ParameterValue.ParameterValue.A = StaticComponentMaskParameterValue.A;
			ParameterValue.bOverride =StaticComponentMaskParameterValue.bOverride;
			ParameterValue.ParameterName =StaticComponentMaskParameterValue.ParameterName;
			ParameterValue.ExpressionId= StaticComponentMaskParameterValue.ExpressionGUID;

			AssignParameterToGroup(ParentMaterial, Cast<UDEditorParameterValue>(&ParameterValue));
		}

		IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>( "MaterialEditor" );
		MaterialEditorModule->GetVisibleMaterialParameters(ParentMaterial, SourceInstance, VisibleExpressions);
	}
	// sort contents of groups
	for(int32 ParameterIdx=0; ParameterIdx<ParameterGroups.Num(); ParameterIdx++)
	{
		FEditorParameterGroup & ParamGroup = ParameterGroups[ParameterIdx];
		struct FCompareUDEditorParameterValueByParameterName
		{
			FORCEINLINE bool operator()(const UDEditorParameterValue& A, const UDEditorParameterValue& B) const
			{
				FString AName = A.ParameterName.ToString().ToLower();
				FString BName = B.ParameterName.ToString().ToLower();
				return AName < BName;
			}
		};
		ParamGroup.Parameters.Sort( FCompareUDEditorParameterValueByParameterName() );
	}	
	
	// sort groups itself pushing defaults to end
	struct FCompareFEditorParameterGroupByName
	{
		FORCEINLINE bool operator()(const FEditorParameterGroup& A, const FEditorParameterGroup& B) const
		{
			FString AName = A.GroupName.ToString().ToLower();
			FString BName = B.GroupName.ToString().ToLower();
			if (AName == TEXT("none"))
			{
				return false;
			}
			if (BName == TEXT("none"))
			{
				return false;
			}
			return AName < BName;
		}
	};
	ParameterGroups.Sort( FCompareFEditorParameterGroupByName() );
	TArray<struct FEditorParameterGroup> ParameterDefaultGroups;
	for(int32 ParameterIdx=0; ParameterIdx<ParameterGroups.Num(); ParameterIdx++)
	{
		FEditorParameterGroup & ParamGroup = ParameterGroups[ParameterIdx];
		if (bUseOldStyleMICEditorGroups == false)
		{			
			if (ParamGroup.GroupName ==  TEXT("None"))
			{
				ParameterDefaultGroups.Add(ParamGroup);
				ParameterGroups.RemoveAt(ParameterIdx);
				break;
			}
		}
		else
		{
			if (ParamGroup.GroupName ==  TEXT("Vector Parameter Values") || 
				ParamGroup.GroupName ==  TEXT("Scalar Parameter Values") ||
				ParamGroup.GroupName ==  TEXT("Texture Parameter Values") ||
				ParamGroup.GroupName ==  TEXT("Static Switch Parameter Values") ||
				ParamGroup.GroupName ==  TEXT("Static Component Mask Parameter Values") ||
				ParamGroup.GroupName ==  TEXT("Font Parameter Values"))
			{
				ParameterDefaultGroups.Add(ParamGroup);
				ParameterGroups.RemoveAt(ParameterIdx);
			}

		}
	}
	if (ParameterDefaultGroups.Num() >0)
	{
		ParameterGroups.Append(ParameterDefaultGroups);
	}

}

void UMaterialEditorInstanceConstant::CopyToSourceInstance()
{
	if(SourceInstance->IsTemplate(RF_ClassDefaultObject) == false )
	{
		SourceInstance->MarkPackageDirty();
		SourceInstance->ClearParameterValuesEditorOnly();

		// Scalar Parameters
		for(int32 GroupIdx=0; GroupIdx<ParameterGroups.Num(); GroupIdx++)
		{
			FEditorParameterGroup & Group = ParameterGroups[GroupIdx];
			for(int32 ParameterIdx=0; ParameterIdx<Group.Parameters.Num(); ParameterIdx++)
			{
				if (Group.Parameters[ParameterIdx] == NULL)
				{
					continue;
				}
				UDEditorScalarParameterValue * ScalarParameterValue = Cast<UDEditorScalarParameterValue>(Group.Parameters[ParameterIdx]);
				if (ScalarParameterValue)
				{
					if(ScalarParameterValue->bOverride)
					{
						SourceInstance->SetScalarParameterValueEditorOnly(ScalarParameterValue->ParameterName, ScalarParameterValue->ParameterValue);
						continue;
					}
				}
				UDEditorFontParameterValue * FontParameterValue = Cast<UDEditorFontParameterValue>(Group.Parameters[ParameterIdx]);
				if (FontParameterValue)
				{
					if(FontParameterValue->bOverride)
					{
						SourceInstance->SetFontParameterValueEditorOnly(FontParameterValue->ParameterName,FontParameterValue->FontValue,FontParameterValue->FontPage);
						continue;
					}
				}

				UDEditorTextureParameterValue * TextureParameterValue = Cast<UDEditorTextureParameterValue>(Group.Parameters[ParameterIdx]);
				if (TextureParameterValue)
				{
					if(TextureParameterValue->bOverride)
					{
						SourceInstance->SetTextureParameterValueEditorOnly(TextureParameterValue->ParameterName, TextureParameterValue->ParameterValue);
						continue;
					}
				}
				UDEditorVectorParameterValue * VectorParameterValue = Cast<UDEditorVectorParameterValue>(Group.Parameters[ParameterIdx]);
				if (VectorParameterValue)
				{
					if(VectorParameterValue->bOverride)
					{
						SourceInstance->SetVectorParameterValueEditorOnly(VectorParameterValue->ParameterName, VectorParameterValue->ParameterValue);
						continue;
					}
				}

			}
		}

		//Create the source instance base overrides too. The source instance owns this from this point on.
		if( !SourceInstance->BasePropertyOverrides )
		{
			SourceInstance->BasePropertyOverrides = new FMaterialInstanceBasePropertyOverrides();
			SourceInstance->BasePropertyOverrides->Init(*SourceInstance);
		}
		//Check for changes to see if we need to force a recompile
		bool bForceRecompile = SourceInstance->BasePropertyOverrides->Update(BasePropertyOverrides);
		bForceRecompile |= SourceInstance->bOverrideBaseProperties != bOverrideBaseProperties;
		SourceInstance->bOverrideBaseProperties = bOverrideBaseProperties;
		
		FStaticParameterSet NewStaticParameters;
		BuildStaticParametersForSourceInstance(NewStaticParameters);
		SourceInstance->UpdateStaticPermutation(NewStaticParameters,bForceRecompile);

		// Copy phys material back to source instance
		SourceInstance->PhysMaterial = PhysMaterial;

		// Copy the Lightmass settings...
		SourceInstance->SetOverrideCastShadowAsMasked(LightmassSettings.CastShadowAsMasked.bOverride);
		SourceInstance->SetCastShadowAsMasked(LightmassSettings.CastShadowAsMasked.ParameterValue);
		SourceInstance->SetOverrideEmissiveBoost(LightmassSettings.EmissiveBoost.bOverride);
		SourceInstance->SetEmissiveBoost(LightmassSettings.EmissiveBoost.ParameterValue);
		SourceInstance->SetOverrideDiffuseBoost(LightmassSettings.DiffuseBoost.bOverride);
		SourceInstance->SetDiffuseBoost(LightmassSettings.DiffuseBoost.ParameterValue);
		SourceInstance->SetOverrideExportResolutionScale(LightmassSettings.ExportResolutionScale.bOverride);
		SourceInstance->SetExportResolutionScale(LightmassSettings.ExportResolutionScale.ParameterValue);
		SourceInstance->SetOverrideDistanceFieldPenumbraScale(LightmassSettings.DistanceFieldPenumbraScale.bOverride);
		SourceInstance->SetDistanceFieldPenumbraScale(LightmassSettings.DistanceFieldPenumbraScale.ParameterValue);

		// Copy Refraction bias setting
		SourceInstance->SetScalarParameterValueEditorOnly(TEXT("RefractionDepthBias"), RefractionDepthBias);

		// Update object references and parameter names.
		SourceInstance->UpdateParameterNames();
		VisibleExpressions.Empty();
		
		// force refresh of visibility of properties
		if( Parent )
		{
			UMaterial* ParentMaterial = Parent->GetMaterial();
			IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>( "MaterialEditor" );
			MaterialEditorModule->GetVisibleMaterialParameters(ParentMaterial, SourceInstance, VisibleExpressions);
		}
	}
}

void UMaterialEditorInstanceConstant::BuildStaticParametersForSourceInstance(FStaticParameterSet& OutStaticParameters)
{
	for(int32 GroupIdx=0; GroupIdx<ParameterGroups.Num(); GroupIdx++)
	{
		FEditorParameterGroup& Group = ParameterGroups[GroupIdx];

		for(int32 ParameterIdx=0; ParameterIdx<Group.Parameters.Num(); ParameterIdx++)
		{
			if (Group.Parameters[ParameterIdx] == NULL)
			{
				continue;
			}
			// static switch

			UDEditorStaticSwitchParameterValue* StaticSwitchParameterValue = Cast<UDEditorStaticSwitchParameterValue>(Group.Parameters[ParameterIdx]);
			if (StaticSwitchParameterValue)
			{
				bool SwitchValue = StaticSwitchParameterValue->ParameterValue;
				FGuid ExpressionIdValue = StaticSwitchParameterValue->ExpressionId;

				if (StaticSwitchParameterValue->bOverride)
				{
					FStaticSwitchParameter * NewParameter = 
						new(OutStaticParameters.StaticSwitchParameters) FStaticSwitchParameter(StaticSwitchParameterValue->ParameterName, SwitchValue, StaticSwitchParameterValue->bOverride, ExpressionIdValue);
				}
			}

			// static component mask

			UDEditorStaticComponentMaskParameterValue* StaticComponentMaskParameterValue = Cast<UDEditorStaticComponentMaskParameterValue>(Group.Parameters[ParameterIdx]);
			if (StaticComponentMaskParameterValue)
			{
				bool MaskR = StaticComponentMaskParameterValue->ParameterValue.R;
				bool MaskG = StaticComponentMaskParameterValue->ParameterValue.G;
				bool MaskB = StaticComponentMaskParameterValue->ParameterValue.B;
				bool MaskA = StaticComponentMaskParameterValue->ParameterValue.A;
				FGuid ExpressionIdValue = StaticComponentMaskParameterValue->ExpressionId;

				if (StaticComponentMaskParameterValue->bOverride)
				{
					FStaticComponentMaskParameter* NewParameter = new(OutStaticParameters.StaticComponentMaskParameters) 
						FStaticComponentMaskParameter(StaticComponentMaskParameterValue->ParameterName, MaskR, MaskG, MaskB, MaskA, StaticComponentMaskParameterValue->bOverride, ExpressionIdValue);\
				}
			}
		}
	}
}


void UMaterialEditorInstanceConstant::SetSourceInstance(UMaterialInstanceConstant* MaterialInterface)
{
	check(MaterialInterface);
	SourceInstance = MaterialInterface;
	Parent = SourceInstance->Parent;
	PhysMaterial = SourceInstance->PhysMaterial;

	//Init the base property overrides.
	BasePropertyOverrides.Init(*SourceInstance);
	bOverrideBaseProperties = SourceInstance->bOverrideBaseProperties;
	if( SourceInstance->BasePropertyOverrides )
	{
		BasePropertyOverrides = *SourceInstance->BasePropertyOverrides;
	}

	// Copy the Lightmass settings...
	LightmassSettings.CastShadowAsMasked.bOverride = SourceInstance->GetOverrideCastShadowAsMasked();
	LightmassSettings.CastShadowAsMasked.ParameterValue = SourceInstance->GetCastShadowAsMasked();
	LightmassSettings.EmissiveBoost.bOverride = SourceInstance->GetOverrideEmissiveBoost();
	LightmassSettings.EmissiveBoost.ParameterValue = SourceInstance->GetEmissiveBoost();
	LightmassSettings.DiffuseBoost.bOverride = SourceInstance->GetOverrideDiffuseBoost();
	LightmassSettings.DiffuseBoost.ParameterValue = SourceInstance->GetDiffuseBoost();
	LightmassSettings.ExportResolutionScale.bOverride = SourceInstance->GetOverrideExportResolutionScale();
	LightmassSettings.ExportResolutionScale.ParameterValue = SourceInstance->GetExportResolutionScale();
	LightmassSettings.DistanceFieldPenumbraScale.bOverride = SourceInstance->GetOverrideDistanceFieldPenumbraScale();
	LightmassSettings.DistanceFieldPenumbraScale.ParameterValue = SourceInstance->GetDistanceFieldPenumbraScale();

	//Copy refraction settings
	SourceInstance->GetRefractionSettings(RefractionDepthBias);

	RegenerateArrays();

	//propagate changes to the base material so the instance will be updated if it has a static permutation resource
	FStaticParameterSet NewStaticParameters;
	BuildStaticParametersForSourceInstance(NewStaticParameters);
	SourceInstance->UpdateStaticPermutation(NewStaticParameters);
}

void UMaterialEditorInstanceConstant::UpdateSourceInstanceParent()
{
	// If the parent was changed to the source instance, set it to NULL
	if( Parent == SourceInstance )
	{
		Parent = NULL;
	}

	SourceInstance->SetParentEditorOnly( Parent );
}

#if WITH_EDITOR
void UMaterialEditorInstanceConstant::PostEditUndo()
{
	UpdateSourceInstanceParent();

	Super::PostEditUndo();
}
#endif

UMaterialEditorMeshComponent::UMaterialEditorMeshComponent(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}
