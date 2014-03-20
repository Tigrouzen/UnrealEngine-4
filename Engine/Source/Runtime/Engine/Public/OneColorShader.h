// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Engine.h"
#include "GlobalShader.h"
#include "ShaderParameters.h"

/**
 * Vertex shader for rendering a single, constant color.
 */
class FOneColorVS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOneColorVS,Global,ENGINE_API);
public:
	FOneColorVS( )	{ }
	FOneColorVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader( Initializer )
	{
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}
};

/**
 * Pixel shader for rendering a single, constant color.
 */
class FOneColorPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOneColorPS,Global,ENGINE_API);
public:
	FOneColorPS( )	{ }
	FOneColorPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader( Initializer )
	{
		ColorParameter.Bind( Initializer.ParameterMap, TEXT("DrawColorMRT"), SPF_Mandatory);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ColorParameter;
		return bShaderHasOutdatedParameters;
	}
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	/** The parameter to use for setting the draw Color. */
	FShaderParameter ColorParameter;
};

/**
 * Pixel shader for rendering a single, constant color to MRTs.
 */
template<int32 NumOutputs>
class TOneColorPixelShaderMRT : public FOneColorPS
{
	DECLARE_EXPORTED_SHADER_TYPE(TOneColorPixelShaderMRT,Global,ENGINE_API);
public:
	TOneColorPixelShaderMRT( )	{ }
	TOneColorPixelShaderMRT(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FOneColorPS( Initializer )
	{
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		if( NumOutputs > 1 )
		{
			return IsFeatureLevelSupported( Platform, ERHIFeatureLevel::SM4 );
		}
		return true;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FOneColorPS::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NUM_OUTPUTS"), NumOutputs);
	}
};

/**
 * Compute shader for writing values
 */
class FFillTextureCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FFillTextureCS,Global,ENGINE_API);
public:
	FFillTextureCS( )	{ }
	FFillTextureCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader( Initializer )
	{
		FillValue.Bind( Initializer.ParameterMap, TEXT("FillValue"), SPF_Mandatory);
		Params0.Bind( Initializer.ParameterMap, TEXT("Params0"), SPF_Mandatory);
		Params1.Bind( Initializer.ParameterMap, TEXT("Params1"), SPF_Mandatory);
		Params2.Bind( Initializer.ParameterMap, TEXT("Params2"), SPF_Optional);
		FillTexture.Bind( Initializer.ParameterMap, TEXT("FillTexture"), SPF_Mandatory);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << FillValue << Params0 << Params1 << Params2 << FillTexture;
		return bShaderHasOutdatedParameters;
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	FShaderParameter FillValue;
	FShaderParameter Params0;	// Texture Width,Height (.xy); Use Exclude Rect 1 : 0 (.z)
	FShaderParameter Params1;	// Include X0,Y0 (.xy) - X1,Y1 (.zw)
	FShaderParameter Params2;	// ExcludeRect X0,Y0 (.xy) - X1,Y1 (.zw)
	FShaderResourceParameter FillTexture;
};
