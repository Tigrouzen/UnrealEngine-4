// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "ShaderFormatOpenGL.h"
#include "Core.h"

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
	#include "Windows/PreWindowsApi.h"
	#include <objbase.h>
	#include <assert.h>
	#include <stdio.h>
	#include "Windows/PostWindowsApi.h"
	#include "Windows/MinWindows.h"
#include "HideWindowsPlatformTypes.h"
#endif
#include "ShaderPreprocessor.h"
#include "hlslcc.h"
#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
	#include <GL/glcorearb.h>
	#include <GL/glext.h>
	#include <GL/wglext.h>
#include "HideWindowsPlatformTypes.h"
#elif PLATFORM_MAC
	#include <OpenGL/OpenGL.h>
	#include <OpenGL/gl3.h>
	#include <OpenGL/gl3ext.h>
	#ifndef GL_COMPUTE_SHADER
	#define GL_COMPUTE_SHADER 0x91B9
	#endif
	#ifndef GL_TESS_EVALUATION_SHADER
	#define GL_TESS_EVALUATION_SHADER 0x8E87
	#endif 
	#ifndef GL_TESS_CONTROL_SHADER
	#define GL_TESS_CONTROL_SHADER 0x8E88
	#endif
#endif
#include "OpenGLUtil.h"
#include "OpenGLShaderResources.h"

DEFINE_LOG_CATEGORY_STATIC(LogOpenGLShaderCompiler, Log, All); 



#define ENABLE_IMAGINATION_COMPILER		1

#define MAX_SAMPLERS_PER_SHADER_GLSL_150 16
#define MAX_SAMPLERS_PER_SHADER_GLSL_430 32

  
static FORCEINLINE bool IsES2Platform(GLSLVersion Version)
{
	return (Version == GLSL_ES2 || Version == GLSL_150_ES2 || Version == GLSL_ES2_WEBGL || Version == GLSL_ES2_IOS); 
}

static FORCEINLINE bool IsPCES2Platform(GLSLVersion Version)
{
	return (Version == GLSL_150_ES2);
}

/*------------------------------------------------------------------------------
	Shader compiling.
------------------------------------------------------------------------------*/

#if PLATFORM_WINDOWS
/** List all OpenGL entry points needed for shader compilation. */
#define ENUM_GL_ENTRYPOINTS(EnumMacro) \
	EnumMacro(PFNGLCOMPILESHADERPROC,glCompileShader) \
	EnumMacro(PFNGLCREATESHADERPROC,glCreateShader) \
	EnumMacro(PFNGLDELETESHADERPROC,glDeleteShader) \
	EnumMacro(PFNGLGETSHADERIVPROC,glGetShaderiv) \
	EnumMacro(PFNGLGETSHADERINFOLOGPROC,glGetShaderInfoLog) \
	EnumMacro(PFNGLSHADERSOURCEPROC,glShaderSource) \
	EnumMacro(PFNGLDELETEBUFFERSPROC,glDeleteBuffers)

/** Define all GL functions. */
#define DEFINE_GL_ENTRYPOINTS(Type,Func) static Type Func = NULL;
ENUM_GL_ENTRYPOINTS(DEFINE_GL_ENTRYPOINTS);

/** This function is handled separately because it is used to get a real context. */
static PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;

/** Platform specific OpenGL context. */
struct FPlatformOpenGLContext
{
	HWND WindowHandle;
	HDC DeviceContext;
	HGLRC OpenGLContext;
};

/**
 * A dummy wndproc.
 */
static LRESULT CALLBACK PlatformDummyGLWndproc(HWND hWnd, uint32 Message, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hWnd, Message, wParam, lParam);
}

/**
 * Initialize a pixel format descriptor for the given window handle.
 */
static void PlatformInitPixelFormatForDevice(HDC DeviceContext)
{
	// Pixel format descriptor for the context.
	PIXELFORMATDESCRIPTOR PixelFormatDesc;
	FMemory::MemZero(PixelFormatDesc);
	PixelFormatDesc.nSize		= sizeof(PIXELFORMATDESCRIPTOR);
	PixelFormatDesc.nVersion	= 1;
	PixelFormatDesc.dwFlags		= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	PixelFormatDesc.iPixelType	= PFD_TYPE_RGBA;
	PixelFormatDesc.cColorBits	= 32;
	PixelFormatDesc.cDepthBits	= 0;
	PixelFormatDesc.cStencilBits	= 0;
	PixelFormatDesc.iLayerType	= PFD_MAIN_PLANE;

	// Set the pixel format and create the context.
	int32 PixelFormat = ChoosePixelFormat(DeviceContext, &PixelFormatDesc);
	if (!PixelFormat || !SetPixelFormat(DeviceContext, PixelFormat, &PixelFormatDesc))
	{
		UE_LOG(LogOpenGLShaderCompiler, Fatal,TEXT("Failed to set pixel format for device context."));
	}
}

/**
 * Create a dummy window used to construct OpenGL contexts.
 */
static void PlatformCreateDummyGLWindow(FPlatformOpenGLContext* OutContext)
{
	const TCHAR* WindowClassName = TEXT("DummyGLToolsWindow");

	// Register a dummy window class.
	static bool bInitializedWindowClass = false;
	if (!bInitializedWindowClass)
	{
		WNDCLASS wc;

		bInitializedWindowClass = true;
		FMemory::MemZero(wc);
		wc.style = CS_OWNDC;
		wc.lpfnWndProc = PlatformDummyGLWndproc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = NULL;
		wc.hIcon = NULL;
		wc.hCursor = NULL;
		wc.hbrBackground = (HBRUSH)(COLOR_MENUTEXT);
		wc.lpszMenuName = NULL;
		wc.lpszClassName = WindowClassName;
		ATOM ClassAtom = ::RegisterClass(&wc);
		check(ClassAtom);
	}

	// Create a dummy window.
	OutContext->WindowHandle = CreateWindowEx(
		WS_EX_WINDOWEDGE,
		WindowClassName,
		NULL,
		WS_POPUP,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, NULL, NULL);
	check(OutContext->WindowHandle);

	// Get the device context.
	OutContext->DeviceContext = GetDC(OutContext->WindowHandle);
	check(OutContext->DeviceContext);
	PlatformInitPixelFormatForDevice(OutContext->DeviceContext);
}

/**
 * Create a core profile OpenGL context.
 */
static void PlatformCreateOpenGLContextCore(FPlatformOpenGLContext* OutContext, int MajorVersion, int MinorVersion, HGLRC InParentContext)
{
	check(wglCreateContextAttribsARB);
	check(OutContext);
	check(OutContext->DeviceContext);

	int AttribList[] =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB, MajorVersion,
		WGL_CONTEXT_MINOR_VERSION_ARB, MinorVersion,
		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB | WGL_CONTEXT_DEBUG_BIT_ARB,
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		0
	};

	OutContext->OpenGLContext = wglCreateContextAttribsARB(OutContext->DeviceContext, InParentContext, AttribList);
	check(OutContext->OpenGLContext);
}

/**
 * Make the context current.
 */
static void PlatformMakeGLContextCurrent(FPlatformOpenGLContext* Context)
{
	check(Context && Context->OpenGLContext && Context->DeviceContext);
	wglMakeCurrent(Context->DeviceContext, Context->OpenGLContext);
}

/**
 * Initialize an OpenGL context so that shaders can be compiled.
 */
static void PlatformInitOpenGL(void*& ContextPtr, void*& PrevContextPtr, int InMajorVersion, int InMinorVersion)
{
	static FPlatformOpenGLContext ShaderCompileContext = {0};

	ContextPtr = (void*)wglGetCurrentDC();
	PrevContextPtr = (void*)wglGetCurrentContext();

	if (ShaderCompileContext.OpenGLContext == NULL && InMajorVersion && InMinorVersion)
	{
		PlatformCreateDummyGLWindow(&ShaderCompileContext);

		// Disable warning C4191: 'type cast' : unsafe conversion from 'PROC' to 'XXX' while getting GL entry points.
		#pragma warning(push)
		#pragma warning(disable:4191)

		if (wglCreateContextAttribsARB == NULL)
		{
			// Create a dummy context so that wglCreateContextAttribsARB can be initialized.
			ShaderCompileContext.OpenGLContext = wglCreateContext(ShaderCompileContext.DeviceContext);
			check(ShaderCompileContext.OpenGLContext);
			PlatformMakeGLContextCurrent(&ShaderCompileContext);
			wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
			check(wglCreateContextAttribsARB);
			wglDeleteContext(ShaderCompileContext.OpenGLContext);
		}

		// Create a context so that remaining GL function pointers can be initialized.
		PlatformCreateOpenGLContextCore(&ShaderCompileContext, InMajorVersion, InMinorVersion, /*InParentContext=*/ NULL);
		check(ShaderCompileContext.OpenGLContext);
		PlatformMakeGLContextCurrent(&ShaderCompileContext);

		if (glCreateShader == NULL)
		{
			// Initialize all entry points.
			#define GET_GL_ENTRYPOINTS(Type,Func) Func = (Type)wglGetProcAddress(#Func);
			ENUM_GL_ENTRYPOINTS(GET_GL_ENTRYPOINTS);

			// Check that all of the entry points have been initialized.
			bool bFoundAllEntryPoints = true;
			#define CHECK_GL_ENTRYPOINTS(Type,Func) if (Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(LogOpenGLShaderCompiler, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }
			ENUM_GL_ENTRYPOINTS(CHECK_GL_ENTRYPOINTS);
			checkf(bFoundAllEntryPoints, TEXT("Failed to find all OpenGL entry points."));
		}

		// Restore warning C4191.
		#pragma warning(pop)
	}
	PlatformMakeGLContextCurrent(&ShaderCompileContext);
}
static void PlatformReleaseOpenGL(void* ContextPtr, void* PrevContextPtr)
{
	wglMakeCurrent((HDC)ContextPtr, (HGLRC)PrevContextPtr);
}
#elif PLATFORM_MAC
static void PlatformInitOpenGL(void*& ContextPtr, void*& PrevContextPtr, int InMajorVersion, int InMinorVersion)
{
	check(InMajorVersion > 3 || (InMajorVersion == 3 && InMinorVersion >= 2));

	CGLPixelFormatAttribute AttribList[] =
	{
		kCGLPFANoRecovery,
		kCGLPFAAccelerated,
		kCGLPFAOpenGLProfile,
		(CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,
		(CGLPixelFormatAttribute)0
	};

	CGLPixelFormatObj PixelFormat;
	GLint NumFormats = 0;
	CGLError Error = CGLChoosePixelFormat(AttribList, &PixelFormat, &NumFormats);
	check(Error == kCGLNoError);

	CGLContextObj ShaderCompileContext;
	Error = CGLCreateContext(PixelFormat, NULL, &ShaderCompileContext);
	check(Error == kCGLNoError);

	Error = CGLDestroyPixelFormat(PixelFormat);
	check(Error == kCGLNoError);

	PrevContextPtr = (void*)CGLGetCurrentContext();

	Error = CGLSetCurrentContext(ShaderCompileContext);
	check(Error == kCGLNoError);

	ContextPtr = (void*)ShaderCompileContext;
}
static void PlatformReleaseOpenGL(void* ContextPtr, void* PrevContextPtr)
{
	CGLContextObj ShaderCompileContext = (CGLContextObj)ContextPtr;
	CGLContextObj PreviousShaderCompileContext = (CGLContextObj)PrevContextPtr;
	CGLError Error;

	Error = CGLSetCurrentContext(PreviousShaderCompileContext);
	check(Error == kCGLNoError);

	Error = CGLDestroyContext(ShaderCompileContext);
	check(Error == kCGLNoError);
}
#endif

/** Map shader frequency -> GL shader type. */
GLenum GLFrequencyTable[] =
{
	GL_VERTEX_SHADER,	// SF_Vertex
	GL_TESS_CONTROL_SHADER,	 // SF_Hull
	GL_TESS_EVALUATION_SHADER, // SF_Domain
	GL_FRAGMENT_SHADER, // SF_Pixel
	GL_GEOMETRY_SHADER,	// SF_Geometry
	GL_COMPUTE_SHADER,  // SF_Compute
	
};

/** Map shader frequency -> string for messages. */
const TCHAR* GLFrequencyStringTable[] =
{
	TEXT("Vertex"),
	TEXT("Hull"),
	TEXT("Domain"),
	TEXT("Pixel"),
	TEXT("Geometry"),
	TEXT("Compute")
};

/** Compile time check to verify that the GL mapping tables are up-to-date. */
checkAtCompileTime(SF_NumFrequencies == ARRAY_COUNT(GLFrequencyTable), SF_NumFrequencesChanged_PleaseUpdateTables);
checkAtCompileTime(ARRAY_COUNT(GLFrequencyTable) == ARRAY_COUNT(GLFrequencyStringTable), FrequencyTableSizeMismatch);

/**
 * Parse a GLSL error.
 * @param OutErrors - Storage for shader compiler errors.
 * @param InLine - A single line from the compile error log.
 */
void ParseGlslError(TArray<FShaderCompilerError>& OutErrors, const FString& InLine)
{
	const TCHAR* ErrorPrefix = TEXT("error: 0:");
	const TCHAR* p = *InLine;
	if (FCString::Strnicmp(p, ErrorPrefix, 9) == 0)
	{
		FString ErrorMsg;
		int32 LineNumber = 0;
		p += FCString::Strlen(ErrorPrefix);

		// Skip to a number, take that to be the line number.
		while (*p && *p < TEXT('0') && *p > TEXT('9')) { p++; }
		while (*p && *p >= TEXT('0') && *p <= TEXT('9'))
		{
			LineNumber = 10 * LineNumber + (*p++ - TEXT('0'));
		}

		// Skip to the next alphanumeric value, treat that as the error message.
		while (*p && !FChar::IsAlnum(*p)) { p++; }
		ErrorMsg = p;

		// Generate a compiler error.
		if (ErrorMsg.Len() > 0)
		{
			// Note that no mapping exists from the GLSL source to the original
			// HLSL source.
			FShaderCompilerError* CompilerError = new(OutErrors) FShaderCompilerError;
			CompilerError->StrippedErrorMessage = FString::Printf(
				TEXT("driver compile error(%d): %s"),
				LineNumber,
				*ErrorMsg
				);
		}
	}
}

static FString ParseIdentifier(const ANSICHAR* &Str)
{
	FString Result;

	while ((*Str >= 'A' && *Str <= 'Z')
		|| (*Str >= 'a' && *Str <= 'z')
		|| (*Str >= '0' && *Str <= '9')
		|| *Str == '_')
	{
		Result += *Str;
		++Str;
	}

	return Result;
}

static bool Match(const ANSICHAR* &Str, ANSICHAR Char)
{
	if (*Str == Char)
	{
		++Str;
		return true;
	}

	return false;
}

static uint32 ParseNumber(const ANSICHAR* &Str)
{
	uint32 Num = 0;
	while (*Str && *Str >= '0' && *Str <= '9')
	{
		Num = Num * 10 + *Str++ - '0';
	}
	return Num;
}

/**
 * Construct the final microcode from the compiled and verified shader source.
 * @param ShaderOutput - Where to store the microcode and parameter map.
 * @param InShaderSource - GLSL source with input/output signature.
 * @param SourceLen - The length of the GLSL source code.
 */
static void BuildShaderOutput(
	FShaderCompilerOutput& ShaderOutput,
	const ANSICHAR* InShaderSource,
	int32 SourceLen,
	GLSLVersion Version
	)
{
	FOpenGLCodeHeader Header = {0};
	const ANSICHAR* ShaderSource = InShaderSource;
	FShaderParameterMap& ParameterMap = ShaderOutput.ParameterMap;
	EShaderFrequency Frequency = (EShaderFrequency)ShaderOutput.Target.Frequency;

	// Write out the magic markers.
	Header.GlslMarker = 0x474c534c;
	switch (Frequency)
	{
	case SF_Vertex:
		Header.FrequencyMarker = 0x5653;
		break;
	case SF_Pixel:
		Header.FrequencyMarker = 0x5053;
		break;
	case SF_Geometry:
		Header.FrequencyMarker = 0x4753;
		break;
	case SF_Hull:
		Header.FrequencyMarker = 0x4853;
		break;
	case SF_Domain:
		Header.FrequencyMarker = 0x4453;
		break;
	case SF_Compute:
		Header.FrequencyMarker = 0x4353;
		break;
	default:
		UE_LOG(LogOpenGLShaderCompiler, Fatal, TEXT("Invalid shader frequency: %d"), (int32)Frequency);
	}

	#define DEF_PREFIX_STR(Str) \
		const ANSICHAR* Str##Prefix = "// @" #Str ": "; \
		const int32 Str##PrefixLen = FCStringAnsi::Strlen(Str##Prefix)
	DEF_PREFIX_STR(Inputs);
	DEF_PREFIX_STR(Outputs);
	DEF_PREFIX_STR(UniformBlocks);
	DEF_PREFIX_STR(Uniforms);
	DEF_PREFIX_STR(PackedGlobals);
	DEF_PREFIX_STR(PackedUB);
	DEF_PREFIX_STR(PackedUBCopies);
	DEF_PREFIX_STR(PackedUBGlobalCopies);
	DEF_PREFIX_STR(Samplers);
	DEF_PREFIX_STR(UAVs);
	DEF_PREFIX_STR(SamplerStates);
	#undef DEF_PREFIX_STR

	// Skip any comments that come before the signature.
	while (	FCStringAnsi::Strncmp(ShaderSource, "//", 2) == 0 &&
			FCStringAnsi::Strncmp(ShaderSource, "// @", 4) != 0 )
	{
		while (*ShaderSource && *ShaderSource++ != '\n') {}
	}

	// HLSLCC first prints the list of inputs.
	if (FCStringAnsi::Strncmp(ShaderSource, InputsPrefix, InputsPrefixLen) == 0)
	{
		ShaderSource += InputsPrefixLen;

		// Only inputs for vertex shaders must be tracked.
		if (Frequency == SF_Vertex)
		{
			const ANSICHAR* AttributePrefix = "in_ATTRIBUTE";
			const int32 AttributePrefixLen = FCStringAnsi::Strlen(AttributePrefix);
			while (*ShaderSource && *ShaderSource != '\n')
			{
				// Skip the type.
				while (*ShaderSource && *ShaderSource++ != ':') {}
				
				// Only process attributes.
				if (FCStringAnsi::Strncmp(ShaderSource, AttributePrefix, AttributePrefixLen) == 0)
				{
					ShaderSource += AttributePrefixLen;
					uint8 AttributeIndex = ParseNumber(ShaderSource);
					Header.Bindings.InOutMask |= (1 << AttributeIndex);
				}

				// Skip to the next.
				while (*ShaderSource && *ShaderSource != ',' && *ShaderSource != '\n')
				{
					ShaderSource++;
				}

				if (Match(ShaderSource, '\n'))
				{
					break;
				}

				verify(Match(ShaderSource, ','));
			}
		}
		else
		{
			// Skip to the next line.
			while (*ShaderSource && *ShaderSource++ != '\n') {}
		}
	}

	// Then the list of outputs.
	if (FCStringAnsi::Strncmp(ShaderSource, OutputsPrefix, OutputsPrefixLen) == 0)
	{
		ShaderSource += OutputsPrefixLen;

		// Only outputs for pixel shaders must be tracked.
		if (Frequency == SF_Pixel)
		{
			const ANSICHAR* TargetPrefix = "out_Target";
			const int32 TargetPrefixLen = FCStringAnsi::Strlen(TargetPrefix);

			while (*ShaderSource && *ShaderSource != '\n')
			{
				// Skip the type.
				while (*ShaderSource && *ShaderSource++ != ':') {}

				// Handle targets.
				if (FCStringAnsi::Strncmp(ShaderSource, TargetPrefix, TargetPrefixLen) == 0)
				{
					ShaderSource += TargetPrefixLen;
					uint8 TargetIndex = ParseNumber(ShaderSource);
					Header.Bindings.InOutMask |= (1 << TargetIndex);
				}
				// Handle depth writes.
				else if (FCStringAnsi::Strcmp(ShaderSource, "gl_FragDepth") == 0)
				{
					Header.Bindings.InOutMask |= 0x8000;
				}

				// Skip to the next.
				while (*ShaderSource && *ShaderSource != ',' && *ShaderSource != '\n')
				{
					ShaderSource++;
				}

				if (Match(ShaderSource, '\n'))
				{
					break;
				}

				verify(Match(ShaderSource, ','));
			}
		}
		else
		{
			// Skip to the next line.
			while (*ShaderSource && *ShaderSource++ != '\n') {}
		}
	}

	// Then 'normal' uniform buffers.
	if (FCStringAnsi::Strncmp(ShaderSource, UniformBlocksPrefix, UniformBlocksPrefixLen) == 0)
	{
		ShaderSource += UniformBlocksPrefixLen;

		while (*ShaderSource && *ShaderSource != '\n')
		{
			FString BufferName = ParseIdentifier(ShaderSource);
			verify(BufferName.Len() > 0);
			verify(Match(ShaderSource, '('));
			uint16 UBIndex = ParseNumber(ShaderSource);
			check(UBIndex == Header.Bindings.NumUniformBuffers);
			verify(Match(ShaderSource, ')'));
			ParameterMap.AddParameterAllocation(*BufferName, Header.Bindings.NumUniformBuffers++, 0, 0);

			// Skip the comma.
			if (Match(ShaderSource, '\n'))
			{
				break;
			}

			verify(Match(ShaderSource, ','));
		}

		Match(ShaderSource, '\n');
	}

	// Then uniforms.
	const uint16 BytesPerComponent = 4;
/*
	uint16 PackedUniformSize[OGL_NUM_PACKED_UNIFORM_ARRAYS] = {0};
	FMemory::Memzero(&PackedUniformSize, sizeof(PackedUniformSize));
*/
	if (FCStringAnsi::Strncmp(ShaderSource, UniformsPrefix, UniformsPrefixLen) == 0)
	{
		// @todo-mobile: Will we ever need to support this code path?
		check(0);
/*
		ShaderSource += UniformsPrefixLen;

		while (*ShaderSource && *ShaderSource != '\n')
		{
			uint16 ArrayIndex = 0;
			uint16 Offset = 0;
			uint16 NumComponents = 0;

			FString ParameterName = ParseIdentifier(ShaderSource);
			verify(ParameterName.Len() > 0);
			verify(Match(ShaderSource, '('));
			ArrayIndex = ParseNumber(ShaderSource);
			verify(Match(ShaderSource, ':'));
			Offset = ParseNumber(ShaderSource);
			verify(Match(ShaderSource, ':'));
			NumComponents = ParseNumber(ShaderSource);
			verify(Match(ShaderSource, ')'));

			ParameterMap.AddParameterAllocation(
				*ParameterName,
				ArrayIndex,
				Offset * BytesPerComponent,
				NumComponents * BytesPerComponent
				);

			if (ArrayIndex < OGL_NUM_PACKED_UNIFORM_ARRAYS)
			{
				PackedUniformSize[ArrayIndex] = FMath::Max<uint16>(
					PackedUniformSize[ArrayIndex],
					BytesPerComponent * (Offset + NumComponents)
					);
			}

			// Skip the comma.
			if (Match(ShaderSource, '\n'))
			{
				break;
			}

			verify(Match(ShaderSource, ','));
		}

		Match(ShaderSource, '\n');
*/
	}

	// Packed global uniforms
	TMap<ANSICHAR, uint16> PackedGlobalArraySize;
	if (FCStringAnsi::Strncmp(ShaderSource, PackedGlobalsPrefix, PackedGlobalsPrefixLen) == 0)
	{
		ShaderSource += PackedGlobalsPrefixLen;
		while (*ShaderSource && *ShaderSource != '\n')
		{
			ANSICHAR ArrayIndex = 0;
			uint16 Offset = 0;
			uint16 NumComponents = 0;

			FString ParameterName = ParseIdentifier(ShaderSource);
			verify(ParameterName.Len() > 0);
			verify(Match(ShaderSource, '('));
			ArrayIndex = *ShaderSource++;
			verify(Match(ShaderSource, ':'));
			Offset = ParseNumber(ShaderSource);
			verify(Match(ShaderSource, ','));
			NumComponents = ParseNumber(ShaderSource);
			verify(Match(ShaderSource, ')'));

			ParameterMap.AddParameterAllocation(
				*ParameterName,
				ArrayIndex,
				Offset * BytesPerComponent,
				NumComponents * BytesPerComponent
				);

			uint16& Size = PackedGlobalArraySize.FindOrAdd(ArrayIndex);
			Size = FMath::Max<uint16>(BytesPerComponent * (Offset + NumComponents), Size);

			if (Match(ShaderSource, '\n'))
			{
				break;
			}

			// Skip the comma.
			verify(Match(ShaderSource, ','));
		}

		Match(ShaderSource, '\n');
	}

	// Packed Uniform Buffers
	TMap<int, TMap<ANSICHAR, uint16> > PackedUniformBuffersSize;
	while (FCStringAnsi::Strncmp(ShaderSource, PackedUBPrefix, PackedUBPrefixLen) == 0)
	{
		ShaderSource += PackedUBPrefixLen;
		FString BufferName = ParseIdentifier(ShaderSource);
		verify(BufferName.Len() > 0);
		verify(Match(ShaderSource, '('));
		uint16 BufferIndex = ParseNumber(ShaderSource);
		check(BufferIndex == Header.Bindings.NumUniformBuffers);
		verify(Match(ShaderSource, ')'));
		ParameterMap.AddParameterAllocation(*BufferName, Header.Bindings.NumUniformBuffers++, 0, 0);

		verify(Match(ShaderSource, ':'));
		Match(ShaderSource, ' ');
		while (*ShaderSource && *ShaderSource != '\n')
		{
			FString ParameterName = ParseIdentifier(ShaderSource);
			verify(ParameterName.Len() > 0);
			verify(Match(ShaderSource, '('));
			ParseNumber(ShaderSource);
			verify(Match(ShaderSource, ','));
			ParseNumber(ShaderSource);
			verify(Match(ShaderSource, ')'));

			if (Match(ShaderSource, '\n'))
			{
				break;
			}

			verify(Match(ShaderSource, ','));
		}
	}

	// Packed Uniform Buffers copy lists & setup sizes for each UB/Precision entry
	enum EFlattenUBState
	{
		Unknown,
		GroupedUBs,
		FlattenedUBs,
	};
	EFlattenUBState UBState = Unknown;
	if (FCStringAnsi::Strncmp(ShaderSource, PackedUBCopiesPrefix, PackedUBCopiesPrefixLen) == 0)
	{
		ShaderSource += PackedUBCopiesPrefixLen;
		while (*ShaderSource && *ShaderSource != '\n')
		{
			FOpenGLUniformBufferCopyInfo CopyInfo;

			CopyInfo.SourceUBIndex = ParseNumber(ShaderSource);
			verify(Match(ShaderSource, ':'));

			CopyInfo.SourceOffsetInFloats = ParseNumber(ShaderSource);
			verify(Match(ShaderSource, '-'));

			CopyInfo.DestUBIndex = ParseNumber(ShaderSource);
			verify(Match(ShaderSource, ':'));

			CopyInfo.DestUBTypeName = *ShaderSource++;
			CopyInfo.DestUBTypeIndex = GLPackedTypeNameToTypeIndex(CopyInfo.DestUBTypeName);
			verify(Match(ShaderSource, ':'));

			CopyInfo.DestOffsetInFloats = ParseNumber(ShaderSource);
			verify(Match(ShaderSource, ':'));

			CopyInfo.SizeInFloats = ParseNumber(ShaderSource);

			Header.UniformBuffersCopyInfo.Add(CopyInfo);

			auto& UniformBufferSize = PackedUniformBuffersSize.FindOrAdd(CopyInfo.DestUBIndex);
			uint16& Size = UniformBufferSize.FindOrAdd(CopyInfo.DestUBTypeName);
			Size = FMath::Max<uint16>(BytesPerComponent * (CopyInfo.DestOffsetInFloats + CopyInfo.SizeInFloats), Size);

			if (Match(ShaderSource, '\n'))
			{
				break;
			}

			verify(Match(ShaderSource, ','));
		}

		check(UBState == Unknown);
		UBState = GroupedUBs;
	}

	if (FCStringAnsi::Strncmp(ShaderSource, PackedUBGlobalCopiesPrefix, PackedUBGlobalCopiesPrefixLen) == 0)
	{
		ShaderSource += PackedUBGlobalCopiesPrefixLen;
		while (*ShaderSource && *ShaderSource != '\n')
		{
			FOpenGLUniformBufferCopyInfo CopyInfo;

			CopyInfo.SourceUBIndex = ParseNumber(ShaderSource);
			verify(Match(ShaderSource, ':'));

			CopyInfo.SourceOffsetInFloats = ParseNumber(ShaderSource);
			verify(Match(ShaderSource, '-'));

			CopyInfo.DestUBIndex = 0;

			CopyInfo.DestUBTypeName = *ShaderSource++;
			CopyInfo.DestUBTypeIndex = GLPackedTypeNameToTypeIndex(CopyInfo.DestUBTypeName);
			verify(Match(ShaderSource, ':'));

			CopyInfo.DestOffsetInFloats = ParseNumber(ShaderSource);
			verify(Match(ShaderSource, ':'));

			CopyInfo.SizeInFloats = ParseNumber(ShaderSource);

			Header.UniformBuffersCopyInfo.Add(CopyInfo);

			uint16& Size = PackedGlobalArraySize.FindOrAdd(CopyInfo.DestUBTypeName);
			Size = FMath::Max<uint16>(BytesPerComponent * (CopyInfo.DestOffsetInFloats + CopyInfo.SizeInFloats), Size);

			if (Match(ShaderSource, '\n'))
			{
				break;
			}

			verify(Match(ShaderSource, ','));
		}

		check(UBState == Unknown);
		UBState = FlattenedUBs;
	}

	Header.Bindings.bFlattenUB = (UBState == FlattenedUBs);

	// Setup Packed Array info
	Header.Bindings.PackedGlobalArrays.Reserve(PackedGlobalArraySize.Num());
	for (auto Iterator = PackedGlobalArraySize.CreateIterator(); Iterator; ++Iterator)
	{
		ANSICHAR TypeName = Iterator.Key();
		uint16 Size = Iterator.Value();
		Size = (Size + 0xf) & (~0xf);
		FOpenGLPackedArrayInfo Info;
		Info.Size = Size;
		Info.TypeName = TypeName;
		Info.TypeIndex = GLPackedTypeNameToTypeIndex(TypeName);
		Header.Bindings.PackedGlobalArrays.Add(Info);
	}

	// Setup Packed Uniform Buffers info
	Header.Bindings.PackedUniformBuffers.Reserve(PackedUniformBuffersSize.Num());
	for (auto Iterator = PackedUniformBuffersSize.CreateIterator(); Iterator; ++Iterator)
	{
		int BufferIndex = Iterator.Key();
		auto& ArraySizes = Iterator.Value();
		TArray<FOpenGLPackedArrayInfo> InfoArray;
		InfoArray.Reserve(ArraySizes.Num());
		for (auto IterSizes = ArraySizes.CreateIterator(); IterSizes; ++IterSizes)
		{
			ANSICHAR TypeName = IterSizes.Key();
			uint16 Size = IterSizes.Value();
			Size = (Size + 0xf) & (~0xf);
			FOpenGLPackedArrayInfo Info;
			Info.Size = Size;
			Info.TypeName = TypeName;
			Info.TypeIndex = GLPackedTypeNameToTypeIndex(TypeName);
			InfoArray.Add(Info);
		}

		Header.Bindings.PackedUniformBuffers.Add(InfoArray);
	}

	// Then samplers.
	if (FCStringAnsi::Strncmp(ShaderSource, SamplersPrefix, SamplersPrefixLen) == 0)
	{
		ShaderSource += SamplersPrefixLen;

		while (*ShaderSource && *ShaderSource != '\n')
		{
			uint16 Offset = 0;
			uint16 NumSamplers = 0;

			FString ParameterName = ParseIdentifier(ShaderSource);
			verify(ParameterName.Len() > 0);
			verify(Match(ShaderSource, '('));
			Offset = ParseNumber(ShaderSource);
			verify(Match(ShaderSource, ':'));
			NumSamplers = ParseNumber(ShaderSource);
			ParameterMap.AddParameterAllocation(
				*ParameterName,
				0,
				Offset,
				NumSamplers
				);

			Header.Bindings.NumSamplers = FMath::Max<uint8>(
				Header.Bindings.NumSamplers,
				Offset + NumSamplers
				);

			if (Match(ShaderSource, '['))
			{
				// Sampler States
				do
				{
					FString SamplerState = ParseIdentifier(ShaderSource);
					checkSlow(SamplerState.Len() != 0);
					ParameterMap.AddParameterAllocation(
						*SamplerState,
						0,
						Offset,
						NumSamplers
						);
				}
				while (Match(ShaderSource, ','));
				verify(Match(ShaderSource, ']'));
			}

			verify(Match(ShaderSource, ')'));

			if (Match(ShaderSource, '\n'))
			{
				break;
			}

			// Skip the comma.
			verify(Match(ShaderSource, ','));
		}
	}	

	// Then UAVs (images in GLSL)
	if (FCStringAnsi::Strncmp(ShaderSource, UAVsPrefix, UAVsPrefixLen) == 0)
	{
		ShaderSource += UAVsPrefixLen;

		while (*ShaderSource && *ShaderSource != '\n')
		{
			uint16 Offset = 0;
			uint16 NumUAVs = 0;

			FString ParameterName = ParseIdentifier(ShaderSource);
			verify(ParameterName.Len() > 0);
			verify(Match(ShaderSource, '('));
			Offset = ParseNumber(ShaderSource);
			verify(Match(ShaderSource, ':'));
			NumUAVs = ParseNumber(ShaderSource);

			ParameterMap.AddParameterAllocation(
				*ParameterName,
				0,
				Offset,
				NumUAVs
				);

			Header.Bindings.NumUAVs = FMath::Max<uint8>(
				Header.Bindings.NumUAVs,
				Offset + NumUAVs
				);

			verify(Match(ShaderSource, ')'));

			if (Match(ShaderSource, '\n'))
			{
				break;
			}

			// Skip the comma.
			verify(Match(ShaderSource, ','));
		}
	}

	const int32 MaxSamplers = GetFeatureLevelMaxTextureSamplers(GetMaxSupportedFeatureLevel((EShaderPlatform)ShaderOutput.Target.Platform));

	if (Header.Bindings.NumSamplers > MaxSamplers)
	{
		ShaderOutput.bSucceeded = false;
		FShaderCompilerError* NewError = new(ShaderOutput.Errors) FShaderCompilerError();
		NewError->StrippedErrorMessage =
			FString::Printf(TEXT("shader uses %d samplers exceeding the limit of %d"),
				Header.Bindings.NumSamplers, MaxSamplers);
	}
	else
	{
		// Write out the header and shader source code.
		FMemoryWriter Ar(ShaderOutput.Code, true);
		Ar << Header;
		Ar.Serialize((void*)ShaderSource, SourceLen + 1 - (ShaderSource - InShaderSource));
		
		ShaderOutput.NumInstructions = 0;
		ShaderOutput.NumTextureSamplers = Header.Bindings.NumSamplers;
		ShaderOutput.bSucceeded = true;
	}
}

static void OpenGLVersionFromGLSLVersion(GLSLVersion InVersion, int& OutMajorVersion, int& OutMinorVersion)
{
	switch(InVersion)
	{
		case GLSL_150:
			OutMajorVersion = 3;
			OutMinorVersion = 2;
			break;
		case GLSL_430:
			OutMajorVersion = 4;
			OutMinorVersion = 3;
			break;
		case GLSL_150_ES2:
			OutMajorVersion = 3;
			OutMinorVersion = 2;
			break;
		case GLSL_ES2_IOS:
		case GLSL_ES2_WEBGL: 
		case GLSL_ES2:
			OutMajorVersion = 0;
			OutMinorVersion = 0;
			break;
		default:
			// Invalid enum
			check(0);
			OutMajorVersion = 0;
			OutMinorVersion = 0;
			break;
	}
}

static const TCHAR* GetGLSLES2CompilerExecutable(bool bNDACompiler)
{
	// Unfortunately no env var is set to handle install path
	return (bNDACompiler 
		? TEXT("C:\\Imagination\\PowerVR\\GraphicsSDK\\Compilers\\OGLES\\Windows_x86_32\\glslcompiler_sgx543_nda.exe")
		: TEXT("C:\\Imagination\\PowerVR\\GraphicsSDK\\Compilers\\OGLES\\Windows_x86_32\\glslcompiler_sgx543.exe"));
}

static FString CreateGLSLES2CompilerArguments(const FString& ShaderFile, const FString& OutputFile, EHlslShaderFrequency Frequency, bool bNDACompiler)
{
	const TCHAR* FrequencySwitch = TEXT("");
	switch (Frequency)
	{
	case HLSLCC_PixelShader:
		FrequencySwitch = TEXT(" -f");
		break;

	case HLSLCC_VertexShader:
		FrequencySwitch = TEXT(" -v");
		break;

	default:
		return TEXT("");
	}

	FString Arguments = FString::Printf(TEXT("%s %s %s -profile -perfsim"), *FPaths::GetCleanFilename(ShaderFile), *FPaths::GetCleanFilename(OutputFile), FrequencySwitch);

	if (bNDACompiler)
	{
		Arguments += " -disasm";
	}
	
	return Arguments;
}

static FString CreateCommandLineGLSLES2(const FString& ShaderFile, const FString& OutputFile, GLSLVersion Version, EHlslShaderFrequency Frequency, bool bNDACompiler) 
{
	if (Version != GLSL_ES2 && Version != GLSL_ES2_WEBGL && Version != GLSL_ES2_IOS)
	{
		return TEXT("");
	}

	FString CmdLine = FString(GetGLSLES2CompilerExecutable(bNDACompiler)) + TEXT(" ") + CreateGLSLES2CompilerArguments(ShaderFile, OutputFile, Frequency, bNDACompiler);
	CmdLine += FString(LINE_TERMINATOR) + TEXT("pause");
	return CmdLine;
}

/** Precompile a glsl shader for ES2. */
static void PrecompileGLSLES2(FShaderCompilerOutput& ShaderOutput, const FShaderCompilerInput& ShaderInput, const ANSICHAR* ShaderSource, EHlslShaderFrequency Frequency)
{
	const TCHAR* CompilerExecutableName = GetGLSLES2CompilerExecutable(false);
	const int32 SourceLen = FCStringAnsi::Strlen(ShaderSource);
	const bool bCompilerExecutableExists = FPaths::FileExists(CompilerExecutableName);

	// Using the debug info path to write out the files to disk for the PVR shader compiler
	if (ShaderInput.DumpDebugInfoPath != TEXT("") && bCompilerExecutableExists)
	{
		const FString GLSLSourceFile = (ShaderInput.DumpDebugInfoPath / TEXT("GLSLSource.txt"));
		bool bSavedSuccessfully = false;

		{
			FArchive* Ar = IFileManager::Get().CreateFileWriter(*GLSLSourceFile, FILEWRITE_EvenIfReadOnly);

			// Save the ansi file to disk so it can be used as input to the PVR shader compiler
			if (Ar)
			{
				bSavedSuccessfully = true;

				// @todo: Patch the code so that textureCubeLodEXT gets converted to textureCubeLod to workaround PowerVR issues
				const ANSICHAR* VersionString = FCStringAnsi::Strfind(ShaderSource, "#version 100");
				check(VersionString);
				VersionString += 12;	// strlen("# version 100");
				Ar->Serialize((void*)ShaderSource, (VersionString - ShaderSource) * sizeof(ANSICHAR));
				const char* PVRWorkaround = "\n#ifndef textureCubeLodEXT\n#define textureCubeLodEXT textureCubeLod\n#endif\n";
				Ar->Serialize((void*)PVRWorkaround, FCStringAnsi::Strlen(PVRWorkaround));
				Ar->Serialize((void*)VersionString, (SourceLen - (VersionString - ShaderSource)) * sizeof(ANSICHAR));
				delete Ar;
			}
		}

		if (bSavedSuccessfully && ENABLE_IMAGINATION_COMPILER)
		{
			const FString Arguments = CreateGLSLES2CompilerArguments(GLSLSourceFile, TEXT("ASM.txt"), Frequency, false);

			FString StdOut;
			FString StdErr;
			int32 ReturnCode = 0;

			// Run the PowerVR shader compiler and wait for completion
			FPlatformProcess::ExecProcess(GetGLSLES2CompilerExecutable(false), *Arguments, &ReturnCode, &StdOut, &StdErr);

			if (ReturnCode >= 0)
			{
				ShaderOutput.bSucceeded = true;
				ShaderOutput.Target = ShaderInput.Target;

				BuildShaderOutput(ShaderOutput, ShaderSource, SourceLen, GLSL_ES2);

				// Parse the cycle count
				const int32 CycleCountStringLength = FPlatformString::Strlen(TEXT("Cycle count: "));
				const int32 CycleCountIndex = StdOut.Find(TEXT("Cycle count: "));

				if (CycleCountIndex != INDEX_NONE && CycleCountIndex + CycleCountStringLength < StdOut.Len())
				{
					const int32 CycleCountEndIndex = StdOut.Find(TEXT("\n"), ESearchCase::IgnoreCase, ESearchDir::FromStart, CycleCountIndex + CycleCountStringLength);

					if (CycleCountEndIndex != INDEX_NONE)
					{
						const FString InstructionSubstring = StdOut.Mid(CycleCountIndex + CycleCountStringLength, CycleCountEndIndex - (CycleCountIndex + CycleCountStringLength));
						ShaderOutput.NumInstructions = FCString::Atoi(*InstructionSubstring);
					}
				}
			}
			else
			{
				ShaderOutput.bSucceeded = false;

				FShaderCompilerError* NewError = new(ShaderOutput.Errors) FShaderCompilerError();
				// Print the name of the generated glsl file so we can open it with a double click in the VS.Net output window
				NewError->StrippedErrorMessage = FString::Printf(TEXT("%s \nPVR SDK glsl compiler for SGX543: %s"), *GLSLSourceFile, *StdOut);
			}
		}
		else
		{
			ShaderOutput.bSucceeded = true;
			ShaderOutput.Target = ShaderInput.Target;

			BuildShaderOutput(ShaderOutput, ShaderSource, SourceLen, GLSL_ES2);
		}
	}
	else
	{
		ShaderOutput.bSucceeded = true;
		ShaderOutput.Target = ShaderInput.Target;

		BuildShaderOutput(ShaderOutput, ShaderSource, SourceLen, GLSL_ES2);
	}
}

/**
 * Precompile a GLSL shader.
 * @param ShaderOutput - The precompiled shader.
 * @param ShaderInput - The shader input.
 * @param InPreprocessedShader - The preprocessed source code.
 */
static void PrecompileShader(FShaderCompilerOutput& ShaderOutput, const FShaderCompilerInput& ShaderInput, const ANSICHAR* ShaderSource, GLSLVersion Version, EHlslShaderFrequency Frequency)
{
	check(ShaderInput.Target.Frequency < SF_NumFrequencies);

	// Lookup the GL shader type.
	GLenum GLFrequency = GLFrequencyTable[ShaderInput.Target.Frequency];
	if (GLFrequency == GL_NONE)
	{
		ShaderOutput.bSucceeded = false;
		FShaderCompilerError* NewError = new(ShaderOutput.Errors) FShaderCompilerError();
		NewError->StrippedErrorMessage = FString::Printf(TEXT("%s shaders not supported for use in OpenGL."), GLFrequencyStringTable[ShaderInput.Target.Frequency]);
		return;
	}

	if (Version == GLSL_ES2 || Version == GLSL_ES2_WEBGL || Version == GLSL_ES2_IOS) 
	{
		PrecompileGLSLES2(ShaderOutput, ShaderInput, ShaderSource, Frequency);
	}
	else
	{
		// Create the shader with the preprocessed source code.
		void* ContextPtr;
		void* PrevContextPtr;
		int MajorVersion = 0;
		int MinorVersion = 0;
		OpenGLVersionFromGLSLVersion(Version, MajorVersion, MinorVersion);
		PlatformInitOpenGL(ContextPtr, PrevContextPtr, MajorVersion, MinorVersion);

		GLint SourceLen = FCStringAnsi::Strlen(ShaderSource);
		GLuint Shader = glCreateShader(GLFrequency);
		{
			const GLchar* SourcePtr = ShaderSource;
			glShaderSource(Shader, 1, &SourcePtr, &SourceLen);
		}

		// Compile and get results.
		glCompileShader(Shader);
		{
			GLint CompileStatus;
			glGetShaderiv(Shader, GL_COMPILE_STATUS, &CompileStatus);
			if (CompileStatus == GL_TRUE)
			{
				ShaderOutput.Target = ShaderInput.Target;
				BuildShaderOutput(
					ShaderOutput,
					ShaderSource,
					(int32)SourceLen,
					Version
					);
			}
			else
			{
				GLint LogLength;
				glGetShaderiv(Shader, GL_INFO_LOG_LENGTH, &LogLength);
				if (LogLength > 1)
				{
					TArray<ANSICHAR> RawCompileLog;
					FString CompileLog;
					TArray<FString> LogLines;

					RawCompileLog.Empty(LogLength);
					RawCompileLog.AddZeroed(LogLength);
					glGetShaderInfoLog(Shader, LogLength, /*OutLength=*/ NULL, RawCompileLog.GetTypedData());
					CompileLog = ANSI_TO_TCHAR(RawCompileLog.GetTypedData());
					CompileLog.ParseIntoArray(&LogLines, TEXT("\n"), true);

					for (int32 Line = 0; Line < LogLines.Num(); ++Line)
					{
						ParseGlslError(ShaderOutput.Errors, LogLines[Line]);
					}

					if (ShaderOutput.Errors.Num() == 0)
					{
						FShaderCompilerError* NewError = new(ShaderOutput.Errors) FShaderCompilerError();
						NewError->StrippedErrorMessage = FString::Printf(
							TEXT("GLSL source:\n%sGL compile log: %s\n"),
							ANSI_TO_TCHAR(ShaderSource),
							ANSI_TO_TCHAR(RawCompileLog.GetTypedData())
							);
					}
				}
				else
				{
					FShaderCompilerError* NewError = new(ShaderOutput.Errors) FShaderCompilerError();
					NewError->StrippedErrorMessage = TEXT("Shader compile failed without errors.");
				}

				ShaderOutput.bSucceeded = false;
			}
		}
		glDeleteShader(Shader);
		PlatformReleaseOpenGL(ContextPtr, PrevContextPtr);
	}
}

/**
 * Parse an error emitted by the HLSL cross-compiler.
 * @param OutErrors - Array into which compiler errors may be added.
 * @param InLine - A line from the compile log.
 */
static void ParseHlslccError(TArray<FShaderCompilerError>& OutErrors, const FString& InLine)
{
	const TCHAR* p = *InLine;
	FShaderCompilerError* Error = new(OutErrors) FShaderCompilerError();

	// Copy the filename.
	while (*p && *p != TEXT('(')) { Error->ErrorFile += (*p++); }
	Error->ErrorFile = GetRelativeShaderFilename(Error->ErrorFile);
	p++;

	// Parse the line number.
	int32 LineNumber = 0;
	while (*p && *p >= TEXT('0') && *p <= TEXT('9'))
	{
		LineNumber = 10 * LineNumber + (*p++ - TEXT('0'));
	}
	Error->ErrorLineString = *FString::Printf(TEXT("%d"), LineNumber);

	// Skip to the warning message.
	while (*p && (*p == TEXT(')') || *p == TEXT(':') || *p == TEXT(' ') || *p == TEXT('\t'))) { p++; }
	Error->StrippedErrorMessage = p;
}

/*------------------------------------------------------------------------------
	External interface.
------------------------------------------------------------------------------*/

static FString CreateCommandLineHLSLCC( const FString& ShaderFile, const FString& OutputFile, const FString& EntryPoint, EHlslShaderFrequency Frequency, GLSLVersion Version, uint32 CCFlags ) 
{
	const TCHAR* FrequencySwitch = TEXT("");
	switch (Frequency)
	{
		case HLSLCC_PixelShader:
			FrequencySwitch = TEXT(" -ps");
			break;

		case HLSLCC_VertexShader:
			FrequencySwitch = TEXT(" -vs");
			break;

		case HLSLCC_HullShader:
			FrequencySwitch = TEXT(" -hs");
			break;

		case HLSLCC_DomainShader:
			FrequencySwitch = TEXT(" -ds");
			break;
		case HLSLCC_ComputeShader:
			FrequencySwitch = TEXT(" -cs");
			break;

		case HLSLCC_GeometryShader:
			FrequencySwitch = TEXT(" -gs");
			break;

		default:
			check(0);
	}

	const TCHAR* VersionSwitch = TEXT("");
	switch (Version)
	{
		case GLSL_150:
			VersionSwitch = TEXT(" -gl3");
			break;

		case GLSL_150_ES2:
			VersionSwitch = TEXT(" -gl3 -flattenub -flattenubstruct");
			break;

		case GLSL_430:
			VersionSwitch = TEXT(" -gl4");
			break;

		case GLSL_ES2:
		case GLSL_ES2_WEBGL:
			VersionSwitch = TEXT(" -es2");
			break;

		case GLSL_ES2_IOS:
			VersionSwitch = TEXT(" -ios -es2");
			break;

		default:
			return TEXT("");
	}

	const TCHAR* ApplyCSE = (CCFlags & HLSLCC_ApplyCommonSubexpressionElimination) != 0 ? TEXT("-cse") : TEXT("");
	FString CmdLine = FPaths::RootDir() / FString::Printf(TEXT("Engine\\Source\\ThirdParty\\hlslcc\\hlslcc\\bin\\Win64\\VS2010\\hlslcc_64.exe %s -o=%s %s -entry=%s %s %s"), *ShaderFile, *OutputFile, FrequencySwitch, *EntryPoint, VersionSwitch, ApplyCSE);
	CmdLine += "\npause";
	return CmdLine;
}

/**
 * Compile a shader for OpenGL on Windows.
 * @param Input - The input shader code and environment.
 * @param Output - Contains shader compilation results upon return.
 */
void CompileShader_Windows_OGL(const FShaderCompilerInput& Input,FShaderCompilerOutput& Output,const FString& WorkingDirectory, GLSLVersion Version, bool bCompileMicrocode)
{
	FString PreprocessedShader;
	FShaderCompilerDefinitions AdditionalDefines;
	EHlslCompileTarget HlslCompilerTarget = HLSLCC_InvalidTarget;
	EShaderBackEnd BackEnd = SBE_Regular;
	switch (Version)
	{
		case GLSL_430:
			AdditionalDefines.SetDefine(TEXT("COMPILER_GLSL"), 1);
			AdditionalDefines.SetDefine(TEXT("GL4_PROFILE"), 1);
			HlslCompilerTarget = HLSLCC_Glsl430;
			break;

		case GLSL_150:
			AdditionalDefines.SetDefine(TEXT("COMPILER_GLSL"), 1);
			AdditionalDefines.SetDefine(TEXT("GL3_PROFILE"), 1);
			HlslCompilerTarget = HLSLCC_Glsl150;
			break;

		case GLSL_ES2_WEBGL:
			AdditionalDefines.SetDefine(TEXT("WEBGL"), 1);
			AdditionalDefines.SetDefine(TEXT("COMPILER_GLSL_ES2"), 1);
			AdditionalDefines.SetDefine(TEXT("ES2_PROFILE"), 1);
			HlslCompilerTarget = HLSLCC_GlslEs2;
			AdditionalDefines.SetDefine(TEXT("row_major"), TEXT(""));
			break; 

		case GLSL_ES2_IOS:
			AdditionalDefines.SetDefine(TEXT("IOS"), 1);
			AdditionalDefines.SetDefine(TEXT("COMPILER_GLSL_ES2"), 1);
			AdditionalDefines.SetDefine(TEXT("ES2_PROFILE"), 1);
			HlslCompilerTarget = HLSLCC_GlslEs2;
			AdditionalDefines.SetDefine(TEXT("row_major"), TEXT(""));
			BackEnd = SBE_IOS;
			break; 

		case GLSL_ES2:
			AdditionalDefines.SetDefine(TEXT("COMPILER_GLSL_ES2"), 1);
			AdditionalDefines.SetDefine(TEXT("ES2_PROFILE"), 1);
			HlslCompilerTarget = HLSLCC_GlslEs2;
			AdditionalDefines.SetDefine(TEXT("row_major"), TEXT(""));
			break; 

		case GLSL_150_ES2:
			AdditionalDefines.SetDefine(TEXT("COMPILER_GLSL"), 1);
			AdditionalDefines.SetDefine(TEXT("ES2_PROFILE"), 1);
			HlslCompilerTarget = HLSLCC_Glsl150;
			AdditionalDefines.SetDefine(TEXT("row_major"), TEXT(""));
			break;

		default:
			check(0);
	}
	
	const bool bDumpDebugInfo = (Input.DumpDebugInfoPath != TEXT("") && IFileManager::Get().DirectoryExists(*Input.DumpDebugInfoPath));

	AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)1);
	if (PreprocessShader(PreprocessedShader, Output, Input, AdditionalDefines))
	{
		char* GlslShaderSource = NULL;
		char* ErrorLog = NULL;

		const EHlslShaderFrequency FrequencyTable[] =
		{
			HLSLCC_VertexShader,
			Version == GLSL_430 ? HLSLCC_HullShader : HLSLCC_InvalidFrequency,
			Version == GLSL_430 ? HLSLCC_DomainShader : HLSLCC_InvalidFrequency,
			HLSLCC_PixelShader,
			IsES2Platform(Version) ? HLSLCC_InvalidFrequency : HLSLCC_GeometryShader,
			Version == GLSL_430 ? HLSLCC_ComputeShader : HLSLCC_InvalidFrequency
		};

		const EHlslShaderFrequency Frequency = FrequencyTable[Input.Target.Frequency];
		if (Frequency == HLSLCC_InvalidFrequency)
		{
			Output.bSucceeded = false;
			FShaderCompilerError* NewError = new(Output.Errors) FShaderCompilerError();
			NewError->StrippedErrorMessage = FString::Printf(
				TEXT("%s shaders not supported for use in OpenGL."),
				GLFrequencyStringTable[Input.Target.Frequency]
				);
			return;
		}

		// Write out the preprocessed file and a batch file to compile it if requested (DumpDebugInfoPath is valid)
		if (bDumpDebugInfo)
		{
			FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*(Input.DumpDebugInfoPath / Input.SourceFilename + TEXT(".usf")));
			if (FileWriter)
			{
				auto AnsiSourceFile = StringCast<ANSICHAR>(*PreprocessedShader);
				FileWriter->Serialize((ANSICHAR*)AnsiSourceFile.Get(), AnsiSourceFile.Length());
				FileWriter->Close();
				delete FileWriter;
			}
		}

		uint32 CCFlags = HLSLCC_NoPreprocess | HLSLCC_PackUniforms | HLSLCC_DX11ClipSpace;
		if (IsES2Platform(Version))
		{
			CCFlags |= HLSLCC_FlattenUniformBuffers | HLSLCC_FlattenUniformBufferStructures;
			// Currently only enabled for ES2, as there are still features to implement for SM4+ (atomics, global store, UAVs, etc)
			if (!IsPCES2Platform(Version))
			{
				CCFlags |= HLSLCC_ApplyCommonSubexpressionElimination;
			}
		}

		if (bDumpDebugInfo)
		{
			const FString GLSLFile = (Input.DumpDebugInfoPath / TEXT("Output.glsl"));
			const FString USFFile = (Input.DumpDebugInfoPath / Input.SourceFilename) + TEXT(".usf");
			const FString CCBatchFileContents = CreateCommandLineHLSLCC(USFFile, GLSLFile, *Input.EntryPointName, Frequency, Version, CCFlags);
			if (!CCBatchFileContents.IsEmpty())
			{
				FFileHelper::SaveStringToFile(CCBatchFileContents, *(Input.DumpDebugInfoPath / TEXT("CrossCompile.bat")));
			}
		}

		int32 Result = HlslCrossCompile(
			TCHAR_TO_ANSI(*Input.SourceFilename),
			TCHAR_TO_ANSI(*PreprocessedShader),
			TCHAR_TO_ANSI(*Input.EntryPointName),
			Frequency,
			BackEnd,
			CCFlags,
			HlslCompilerTarget,
			&GlslShaderSource,
			&ErrorLog
			);

		if (Result != 0)
		{
			if (bDumpDebugInfo)
			{
				const FString GLSLFile = (Input.DumpDebugInfoPath / TEXT("Output.glsl"));
				const FString GLBatchFileContents = CreateCommandLineGLSLES2(GLSLFile, (Input.DumpDebugInfoPath / TEXT("Output.asm")), Version, Frequency, false);
				if (!GLBatchFileContents.IsEmpty())
				{
					FFileHelper::SaveStringToFile(GLBatchFileContents, *(Input.DumpDebugInfoPath / TEXT("GLSLCompile.bat")));
				}

				const FString NDABatchFileContents = CreateCommandLineGLSLES2(GLSLFile, (Input.DumpDebugInfoPath / TEXT("Output.asm")), Version, Frequency, true);
				if (!NDABatchFileContents.IsEmpty())
				{
					FFileHelper::SaveStringToFile(NDABatchFileContents, *(Input.DumpDebugInfoPath / TEXT("NDAGLSLCompile.bat")));
				}

				int32 GlslSourceLen = FCStringAnsi::Strlen(GlslShaderSource);
				if (GlslSourceLen > 0)
				{
					FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*(Input.DumpDebugInfoPath / Input.SourceFilename + TEXT(".glsl")));
					if (FileWriter)
					{
						FileWriter->Serialize(GlslShaderSource,GlslSourceLen+1);
						FileWriter->Close();
						delete FileWriter;
					}
				}
			}

			if (bCompileMicrocode)
			{
				PrecompileShader(Output, Input, GlslShaderSource, Version, Frequency);
				if (Output.bSucceeded == false)
				{
#if DUMP_HLSCLCC_SHADERS
					DumpFile.SetFilename( *DumpGLSL);
					for(int i = 0; i < Output.Errors.Num(); ++i)
					{
						DumpFile.Logf(TEXT("%s"), *Output.Errors[i].GetErrorString());
					}
					DumpFile.Flush();
#endif
				}
			}
			else
			{
				int32 SourceLen = FCStringAnsi::Strlen(GlslShaderSource);
				Output.Target = Input.Target;
				BuildShaderOutput(Output, GlslShaderSource, SourceLen, Version);
			}
		}
		else
		{
			FString Tmp = ANSI_TO_TCHAR(ErrorLog);
			TArray<FString> ErrorLines;
			Tmp.ParseIntoArray(&ErrorLines, TEXT("\n"), true);
			for (int32 LineIndex = 0; LineIndex < ErrorLines.Num(); ++LineIndex)
			{
				const FString& Line = ErrorLines[LineIndex];
				ParseHlslccError(Output.Errors, Line);
			}
		}

		if (GlslShaderSource)
		{
			free(GlslShaderSource);
		}
		if (ErrorLog)
		{
			free(ErrorLog);
		}
	}
}
