
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLES2.cpp: OpenGL ES2 implementation.
=============================================================================*/

#if !PLATFORM_DESKTOP

#include "OpenGLDrvPrivate.h"

#if OPENGL_ES2

/** GL_OES_vertex_array_object */
bool FOpenGLES2::bSupportsVertexArrayObjects = false;

/** GL_OES_mapbuffer */
bool FOpenGLES2::bSupportsMapBuffer = false;

/** GL_OES_depth_texture */
bool FOpenGLES2::bSupportsDepthTexture = false;

/** GL_ARB_occlusion_query2, GL_EXT_occlusion_query_boolean */
bool FOpenGLES2::bSupportsOcclusionQueries = false;

/** GL_EXT_disjoint_timer_query */
bool FOpenGLES2::bSupportsDisjointTimeQueries = false;


#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarDisjointTimerQueries(
	TEXT("r.DisjointTimerQueries"),
	0,
	TEXT("If set to 1, allows GPU time to be measured (e.g. STAT UNIT). It defaults to 0 because some devices supports it but very slowly."),
	ECVF_RenderThreadSafe);
#endif


/** GL_OES_rgb8_rgba8 */
bool FOpenGLES2::bSupportsRGBA8 = false;

/** GL_APPLE_texture_format_BGRA8888 */
bool FOpenGLES2::bSupportsBGRA8888 = false;

/** GL_EXT_discard_framebuffer */
bool FOpenGLES2::bSupportsDiscardFrameBuffer = false;

/** GL_OES_vertex_half_float */
bool FOpenGLES2::bSupportsVertexHalfFloat = false;

/** GL_OES_texture_float */
bool FOpenGLES2::bSupportsTextureFloat = false;

/** GL_OES_texture_half_float */
bool FOpenGLES2::bSupportsTextureHalfFloat = false;

/** GL_EXT_color_buffer_half_float */
bool FOpenGLES2::bSupportsColorBufferHalfFloat = false;

/** GL_EXT_shader_framebuffer_fetch */
bool FOpenGLES2::bSupportsShaderFramebufferFetch = false;

/** GL_EXT_sRGB */
bool FOpenGLES2::bSupportsSGRB = false;

/** GL_NV_texture_compression_s3tc, GL_EXT_texture_compression_s3tc */
bool FOpenGLES2::bSupportsDXT = false;

/** GL_IMG_texture_compression_pvrtc */
bool FOpenGLES2::bSupportsPVRTC = false;

/** GL_ATI_texture_compression_atitc, GL_AMD_compressed_ATC_texture */
bool FOpenGLES2::bSupportsATITC = false;

/** GL_OES_compressed_ETC1_RGB8_texture */
bool FOpenGLES2::bSupportsETC1 = false;

/** OpenGL ES 3.0 profile */
bool FOpenGLES2::bSupportsETC2 = false;

/** GL_FRAGMENT_SHADER, GL_LOW_FLOAT */
int FOpenGLES2::ShaderLowPrecision = 0;

/** GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT */
int FOpenGLES2::ShaderMediumPrecision = 0;

/** GL_FRAGMENT_SHADER, GL_HIGH_FLOAT */
int FOpenGLES2::ShaderHighPrecision = 0;

/** GL_NV_framebuffer_blit */
bool FOpenGLES2::bSupportsNVFrameBufferBlit = false;

/** GL_EXT_texture_filter_anisotropic */
bool FOpenGLES2::bSupportsTextureFilterAnisotropic = false;

/** GL_OES_packed_depth_stencil */
bool FOpenGLES2::bSupportsPackedDepthStencil = false;

/** textureCubeLodEXT */
bool FOpenGLES2::bSupportsTextureCubeLodEXT = true;

/** GL_EXT_shader_texture_lod */
bool FOpenGLES2::bSupportsShaderTextureLod = false;

/** GL_APPLE_copy_texture_levels */
bool FOpenGLES2::bSupportsCopyTextureLevels = false;

/** GL_EXT_texture_storage */
bool FOpenGLES2::bSupportsTextureStorageEXT = false;


bool FOpenGLES2::SupportsDisjointTimeQueries()
{
	bool bAllowDisjointTimerQueries = false;
#if !UE_BUILD_SHIPPING
	bAllowDisjointTimerQueries = (CVarDisjointTimerQueries.GetValueOnRenderThread() == 1);
#endif
	return bSupportsDisjointTimeQueries && bAllowDisjointTimerQueries;
}

void FOpenGLES2::ProcessQueryGLInt()
{
#ifndef __clang__
#define LOG_AND_GET_GL_INT(IntEnum,Default,Dest) if (IntEnum) {glGetIntegerv(IntEnum, &Dest);} else {Dest = Default;} UE_LOG(LogRHI, Log, TEXT("  ") ## TEXT(#IntEnum) ## TEXT(": %d"), Dest)
#else
#define LOG_AND_GET_GL_INT(IntEnum,Default,Dest) if (IntEnum) {glGetIntegerv(IntEnum, &Dest);} else {Dest = Default;} UE_LOG(LogRHI, Log, TEXT("  " #IntEnum ": %d"), Dest)
#endif
	LOG_AND_GET_GL_INT(GL_MAX_VERTEX_UNIFORM_VECTORS, 0, MaxVertexUniformComponents);
	LOG_AND_GET_GL_INT(GL_MAX_FRAGMENT_UNIFORM_VECTORS, 0, MaxPixelUniformComponents);
	MaxVertexUniformComponents *= 4;
	MaxPixelUniformComponents *= 4;
	MaxGeometryUniformComponents = 0;
#undef LOG_AND_GET_GL_INT

	MaxGeometryTextureImageUnits = 0;
	MaxHullTextureImageUnits = 0;
	MaxDomainTextureImageUnits = 0;
}

void FOpenGLES2::ProcessExtensions( const FString& ExtensionsString )
{
	ProcessQueryGLInt();
	FOpenGLBase::ProcessExtensions(ExtensionsString);

	bSupportsMapBuffer = ExtensionsString.Contains(TEXT("GL_OES_mapbuffer"));
	bSupportsDepthTexture = ExtensionsString.Contains(TEXT("GL_OES_depth_texture"));
	bSupportsOcclusionQueries = ExtensionsString.Contains(TEXT("GL_ARB_occlusion_query2")) || ExtensionsString.Contains(TEXT("GL_EXT_occlusion_query_boolean"));
	bSupportsDisjointTimeQueries = ExtensionsString.Contains(TEXT("GL_EXT_disjoint_timer_query"));
	bSupportsRGBA8 = ExtensionsString.Contains(TEXT("GL_OES_rgb8_rgba8"));
	bSupportsBGRA8888 = ExtensionsString.Contains(TEXT("GL_APPLE_texture_format_BGRA8888")) || ExtensionsString.Contains(TEXT("GL_IMG_texture_format_BGRA8888")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_format_BGRA8888"));
	bSupportsVertexHalfFloat = ExtensionsString.Contains(TEXT("GL_OES_vertex_half_float"));
	bSupportsTextureFloat = ExtensionsString.Contains(TEXT("GL_OES_texture_float"));
	bSupportsTextureHalfFloat = ExtensionsString.Contains(TEXT("GL_OES_texture_half_float"));
	bSupportsSGRB = ExtensionsString.Contains(TEXT("GL_EXT_sRGB"));
	bSupportsColorBufferHalfFloat = ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_half_float"));
	bSupportsShaderFramebufferFetch = ExtensionsString.Contains(TEXT("GL_EXT_shader_framebuffer_fetch")) || ExtensionsString.Contains(TEXT("GL_NV_shader_framebuffer_fetch"));
	// @todo ios7: SRGB support does not work with our texture format setup (ES2 docs indicate that internalFormat and format must match, but they don't at all with sRGB enabled)
	//             One possible solution us to use GLFormat.InternalFormat[bSRGB] instead of GLFormat.Format
	bSupportsSGRB = false;//ExtensionsString.Contains(TEXT("GL_EXT_sRGB"));
	bSupportsDXT = ExtensionsString.Contains(TEXT("GL_NV_texture_compression_s3tc")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_compression_s3tc"));
	bSupportsPVRTC = ExtensionsString.Contains(TEXT("GL_IMG_texture_compression_pvrtc")) ;
	bSupportsATITC = ExtensionsString.Contains(TEXT("GL_ATI_texture_compression_atitc")) || ExtensionsString.Contains(TEXT("GL_AMD_compressed_ATC_texture"));
	bSupportsETC1 = ExtensionsString.Contains(TEXT("GL_OES_compressed_ETC1_RGB8_texture"));
	bSupportsVertexArrayObjects = ExtensionsString.Contains(TEXT("GL_OES_vertex_array_object")) ;
	bSupportsDiscardFrameBuffer = ExtensionsString.Contains(TEXT("GL_EXT_discard_framebuffer"));
	bSupportsNVFrameBufferBlit = ExtensionsString.Contains(TEXT("GL_NV_framebuffer_blit"));
	bSupportsTextureFilterAnisotropic = ExtensionsString.Contains(TEXT("GL_EXT_texture_filter_anisotropic"));
	bSupportsPackedDepthStencil = ExtensionsString.Contains(TEXT("GL_OES_packed_depth_stencil"));
	bSupportsShaderTextureLod = ExtensionsString.Contains(TEXT("GL_EXT_shader_texture_lod"));
	bSupportsTextureStorageEXT = ExtensionsString.Contains(TEXT("GL_EXT_texture_storage"));
	bSupportsCopyTextureLevels = bSupportsTextureStorageEXT && ExtensionsString.Contains(TEXT("GL_APPLE_copy_texture_levels"));

	// Report shader precision
	int Range[2];
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_LOW_FLOAT, Range, &ShaderLowPrecision);
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT, Range, &ShaderMediumPrecision);
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, Range, &ShaderHighPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader lowp precision: %d"), ShaderLowPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader mediump precision: %d"), ShaderMediumPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader highp precision: %d"), ShaderHighPrecision);

	if ( FPlatformMisc::IsDebuggerPresent() && UE_BUILD_DEBUG )
	{
		// Enable GL debug markers if we're running in Xcode
		extern int32 GEmitMeshDrawEvent;
		GEmitMeshDrawEvent = 1;
		GEmitDrawEvents = true;
	}
}

#endif

#endif //desktop
