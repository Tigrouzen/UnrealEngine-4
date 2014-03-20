// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "ShaderFormatOpenGL.h"
#include "Core.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "TargetPlatform.h"   
#include "hlslcc.h"
#include "ShaderCore.h"

static FName NAME_GLSL_150(TEXT("GLSL_150")); 
static FName NAME_GLSL_430(TEXT("GLSL_430"));
static FName NAME_GLSL_ES2(TEXT("GLSL_ES2"));
static FName NAME_GLSL_ES2_WEBGL(TEXT("GLSL_ES2_WEBGL"));
static FName NAME_GLSL_150_ES2(TEXT("GLSL_150_ES2"));
static FName NAME_GLSL_ES2_IOS(TEXT("GLSL_ES2_IOS"));
 
class FShaderFormatGLSL : public IShaderFormat
{
	enum 
	{
		/** Version for shader format, this becomes part of the DDC key. */
		UE_SHADER_GLSL_150_VER = 21,
		UE_SHADER_GLSL_430_VER = 51,
		UE_SHADER_GLSL_ES2_VER = 10,
		UE_SHADER_GLSL_150ES2_VER = 11,
		UE_SHADER_GLSL_ES2_VER_WEBGL  = 11,
		UE_SHADER_GLSL_ES2_IOS_VER  = 1,
	}; 

	void CheckFormat(FName Format) const
	{
		check(	Format == NAME_GLSL_150 ||  
				Format == NAME_GLSL_430 || 
				Format == NAME_GLSL_ES2 || 
				Format == NAME_GLSL_150_ES2 ||
                Format == NAME_GLSL_ES2_WEBGL ||
				Format == NAME_GLSL_ES2_IOS
			);
	}

public:
	virtual uint16 GetVersion(FName Format) const OVERRIDE
	{
		CheckFormat(Format);
		uint32 GLSLVersion = 0;
		if (Format == NAME_GLSL_150)
		{
			GLSLVersion  = UE_SHADER_GLSL_150_VER;
		}
		else if (Format == NAME_GLSL_430)
		{
			GLSLVersion = UE_SHADER_GLSL_430_VER;
		}
		else if (Format == NAME_GLSL_ES2)
		{
			GLSLVersion = UE_SHADER_GLSL_ES2_VER;
		}
		else if (Format == NAME_GLSL_150_ES2)
		{
			GLSLVersion = UE_SHADER_GLSL_150ES2_VER;
		}
		else if (Format == NAME_GLSL_ES2_WEBGL)
		{
			GLSLVersion = UE_SHADER_GLSL_ES2_VER_WEBGL;
		}
		else if (Format == NAME_GLSL_ES2_IOS)
		{
			GLSLVersion = UE_SHADER_GLSL_ES2_IOS_VER;
		}
		else
		{
			check(0);
		}
		const uint8 HLSLCCVersion = ((HLSLCC_VersionMajor & 0x0f) << 4) | (HLSLCC_VersionMinor & 0x0f);
		const uint16 Version = ((HLSLCCVersion & 0xff) << 8) | (GLSLVersion & 0xff);
		return Version;
	}
	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const
	{
		OutFormats.Add(NAME_GLSL_150);
		OutFormats.Add(NAME_GLSL_430);
		OutFormats.Add(NAME_GLSL_ES2);
		OutFormats.Add(NAME_GLSL_ES2_WEBGL);
		OutFormats.Add(NAME_GLSL_150_ES2);
		OutFormats.Add(NAME_GLSL_ES2_IOS);
	}
	virtual void CompileShader(FName Format, const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output,const FString& WorkingDirectory) const
	{
		CheckFormat(Format);

		if (Format == NAME_GLSL_150)
		{
			CompileShader_Windows_OGL(Input, Output, WorkingDirectory, GLSL_150);
		}
		else if (Format == NAME_GLSL_430)
		{
			CompileShader_Windows_OGL(Input, Output, WorkingDirectory, GLSL_430);
		}
		else if (Format == NAME_GLSL_ES2)
		{
			CompileShader_Windows_OGL(Input, Output, WorkingDirectory, GLSL_ES2);

			if (Input.DumpDebugInfoPath != TEXT("") && IFileManager::Get().DirectoryExists(*Input.DumpDebugInfoPath))
			{
				FShaderCompilerInput ES2Input = Input;
				ES2Input.DumpDebugInfoPath = ES2Input.DumpDebugInfoPath.Replace(TEXT("GLSL_150_ES2"), TEXT("GLSL_ES2"), ESearchCase::CaseSensitive);
				if (!IFileManager::Get().DirectoryExists(*ES2Input.DumpDebugInfoPath))
				{
					verifyf(IFileManager::Get().MakeDirectory(*ES2Input.DumpDebugInfoPath, true), TEXT("Failed to create directory for shader debug info '%s'"), *ES2Input.DumpDebugInfoPath);
				}

				FShaderCompilerOutput ES2Output;
				CompileShader_Windows_OGL(ES2Input, ES2Output, WorkingDirectory, GLSL_ES2);
			}

		}
		else if (Format == NAME_GLSL_ES2_WEBGL )
		{
			 CompileShader_Windows_OGL(Input, Output, WorkingDirectory, GLSL_ES2_WEBGL);
		}
		else if (Format == NAME_GLSL_ES2_IOS )
		{
			CompileShader_Windows_OGL(Input, Output, WorkingDirectory, GLSL_ES2_IOS);
		}
		else if (Format == NAME_GLSL_150_ES2)
		{
			CompileShader_Windows_OGL(Input, Output, WorkingDirectory, GLSL_150_ES2);

			if (Input.DumpDebugInfoPath != TEXT("") && IFileManager::Get().DirectoryExists(*Input.DumpDebugInfoPath))
			{
				FShaderCompilerInput ES2Input = Input;
				ES2Input.DumpDebugInfoPath = ES2Input.DumpDebugInfoPath.Replace(TEXT("GLSL_150_ES2"), TEXT("GLSL_ES2_150"), ESearchCase::CaseSensitive);
				if (!IFileManager::Get().DirectoryExists(*ES2Input.DumpDebugInfoPath))
				{
					verifyf(IFileManager::Get().MakeDirectory(*ES2Input.DumpDebugInfoPath, true), TEXT("Failed to create directory for shader debug info '%s'"), *ES2Input.DumpDebugInfoPath);
				}

				FShaderCompilerOutput ES2Output;
				CompileShader_Windows_OGL(ES2Input, ES2Output, WorkingDirectory, GLSL_ES2);
			}
		}
		else
		{
			check(0);
		}
	}
};

/**
 * Module for OpenGL shaders
 */

static IShaderFormat* Singleton = NULL;

class FShaderFormatOpenGLModule : public IShaderFormatModule
{
public:
	virtual ~FShaderFormatOpenGLModule()
	{
		delete Singleton;
		Singleton = NULL;
	}
	virtual IShaderFormat* GetShaderFormat()
	{
		if (!Singleton)
		{
			Singleton = new FShaderFormatGLSL();
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE( FShaderFormatOpenGLModule, ShaderFormatOpenGL);
