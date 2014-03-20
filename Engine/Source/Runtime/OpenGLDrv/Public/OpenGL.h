// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGL.h: Public OpenGL base definitions for non-common functionality
=============================================================================*/
#pragma once
	

/** OpenGL Logging. */
OPENGLDRV_API DECLARE_LOG_CATEGORY_EXTERN(LogOpenGL,Log,VeryVerbose);

#define UGL_REQUIRED_VOID			{ UE_LOG(LogOpenGL,Fatal,TEXT("%s is not supported."), ANSI_TO_TCHAR(__FUNCTION__)); }
#define UGL_REQUIRED(ReturnValue)	{ UE_LOG(LogOpenGL,Fatal,TEXT("%s is not supported."), ANSI_TO_TCHAR(__FUNCTION__)); return (ReturnValue); }
#define UGL_OPTIONAL_VOID			{ }
#define UGL_OPTIONAL(ReturnValue)	{ return (ReturnValue); }

struct FPlatformOpenGLDevice;
struct FPlatformOpenGLContext;


#define UGL_SUPPORTS_PIXELBUFFERS		1
#define UGL_SUPPORTS_UNIFORMBUFFERS		1

#ifndef OPENGL_ES2
#define OPENGL_ES2	0
#endif
#ifndef OPENGL_ES3
#define OPENGL_ES3	0
#endif
#ifndef OPENGL_GL3
#define OPENGL_GL3	0
#endif


class FOpenGLBase
{
public:
	enum EResourceLockMode
	{
		RLM_ReadWrite,
		RLM_ReadOnly,
		RLM_WriteOnly,
		RLM_WriteOnlyUnsynchronized,
	};

	enum EQueryMode
	{
		QM_Result,
		QM_ResultAvailable,
	};

	enum EFenceResult
	{
		FR_AlreadySignaled,
		FR_TimeoutExpired,
		FR_ConditionSatisfied,
		FR_WaitFailed,
	};

	static void ProcessQueryGLInt();
	static void ProcessExtensions(const FString& ExtensionsString);

	static FORCEINLINE bool SupportsMapBuffer()							{ return true; }
	static FORCEINLINE bool SupportsDepthTexture()						{ return true; }
	static FORCEINLINE bool SupportsDrawBuffers()						{ return true; }
	static FORCEINLINE bool SupportsPixelBuffers()						{ return true; }
	static FORCEINLINE bool SupportsUniformBuffers()					{ return true; }
	static FORCEINLINE bool SupportsStructuredBuffers()					{ return true; }
	static FORCEINLINE bool SupportsTimestampQueries()					{ return true; }
	static FORCEINLINE bool SupportsDisjointTimeQueries()				{ return false; } // @todo: if enabled, causes crash on PC and massive slowdown on Mac
	static FORCEINLINE bool SupportsOcclusionQueries()					{ return true; }
	static FORCEINLINE bool SupportsExactOcclusionQueries()				{ return true; }
	static FORCEINLINE bool SupportsBlitFramebuffer()					{ return true; }
	static FORCEINLINE bool SupportsDepthStencilReadSurface()			{ return true; }
	static FORCEINLINE bool SupportsFloatReadSurface()					{ return true; }
	static FORCEINLINE bool SupportsMultipleRenderTargets()				{ return true; }
	static FORCEINLINE bool SupportsMultisampledTextures()				{ return true; }
	static FORCEINLINE bool SupportsFences()							{ return true; }
	static FORCEINLINE bool SupportsPolygonMode()						{ return true; }
	static FORCEINLINE bool SupportsSamplerObjects()					{ return true; }
	static FORCEINLINE bool SupportsTexture3D()							{ return true; }
	static FORCEINLINE bool SupportsTextureLODBias()					{ return true; }
	static FORCEINLINE bool SupportsTextureCompare()					{ return true; }
	static FORCEINLINE bool SupportsTextureBaseLevel()					{ return true; }
	static FORCEINLINE bool SupportsTextureMaxLevel()					{ return true; }
	static FORCEINLINE bool SupportsInstancing()						{ return true; }
	static FORCEINLINE bool SupportsVertexAttribInteger()				{ return true; }
	static FORCEINLINE bool SupportsVertexAttribShort()					{ return true; }
	static FORCEINLINE bool SupportsVertexAttribByte()					{ return true; }
	static FORCEINLINE bool SupportsVertexAttribDouble()				{ return true; }
	static FORCEINLINE bool SupportsVertexArrayObjects()				{ return false; }
	static FORCEINLINE bool SupportsDrawIndexOffset()					{ return true; }
	static FORCEINLINE bool SupportsResourceView()						{ return true; }
	static FORCEINLINE bool SupportsCopyBuffer()						{ return true; }
	static FORCEINLINE bool SupportsDiscardFrameBuffer()				{ return false; }
	static FORCEINLINE bool SupportsIndexedExtensions()					{ return true; }
	static FORCEINLINE bool SupportsVertexHalfFloat()					{ return true; }
	static FORCEINLINE bool SupportsTextureFloat()						{ return true; }
	static FORCEINLINE bool SupportsTextureHalfFloat()					{ return true; }
	static FORCEINLINE bool SupportsColorBufferHalfFloat()				{ return true; }
	static FORCEINLINE bool SupportsGSRenderTargetLayerSwitchingToMips() { return true; }
	static FORCEINLINE bool SupportsShaderFramebufferFetch()			{ return false; }
	static FORCEINLINE bool SupportsVertexArrayBGRA()					{ return true; }
	static FORCEINLINE bool SupportsBGRA8888()							{ return true; }
	static FORCEINLINE bool SupportsSRGB()								{ return true; }
	static FORCEINLINE bool SupportsRGBA8()								{ return true; }
	static FORCEINLINE bool SupportsDXT()								{ return true; }
	static FORCEINLINE bool SupportsPVRTC()								{ return false; }
	static FORCEINLINE bool SupportsATITC()								{ return false; }
	static FORCEINLINE bool SupportsASTC()								{ return bSupportsASTC; }
	static FORCEINLINE bool SupportsETC1()								{ return false; }
	static FORCEINLINE bool SupportsETC2()								{ return false; }
	static FORCEINLINE bool SupportsCombinedDepthStencilAttachment()	{ return true; }
	static FORCEINLINE bool SupportsFastBufferData()					{ return true; }
	static FORCEINLINE bool SupportsCopyImage()							{ return bSupportsCopyImage; }
	static FORCEINLINE bool SupportsCopyTextureLevels()					{ return false; }
	static FORCEINLINE bool SupportsTextureFilterAnisotropic()			{ return false; }
	static FORCEINLINE bool SupportsPackedDepthStencil()				{ return true; }
	static FORCEINLINE bool SupportsTextureCubeLodEXT()					{ return true; }
	static FORCEINLINE bool SupportsShaderTextureLod()					{ return false; }
	static FORCEINLINE bool SupportsSeparateAlphaBlend()				{ return false; }
	static FORCEINLINE bool SupportsTessellation()						{ return false; }
	static FORCEINLINE bool SupportsComputeShaders()					{ return false; }
	static FORCEINLINE bool SupportsTextureView()						{ return false; }
	static FORCEINLINE bool SupportsSeamlessCubeMap()					{ return false; }
	static FORCEINLINE bool HasSamplerRestrictions()					{ return false; }
	static FORCEINLINE bool HasHardwareHiddenSurfaceRemoval()			{ return false; }

	static FORCEINLINE GLenum GetDepthFormat()							{ return GL_DEPTH_COMPONENT16; }

	static FORCEINLINE GLint GetMaxTextureImageUnits()			{ check(MaxTextureImageUnits != -1); return MaxTextureImageUnits; }
	static FORCEINLINE GLint GetMaxVertexTextureImageUnits()	{ check(MaxVertexTextureImageUnits != -1); return MaxVertexTextureImageUnits; }
	static FORCEINLINE GLint GetMaxGeometryTextureImageUnits()	{ check(MaxGeometryTextureImageUnits != -1); return MaxGeometryTextureImageUnits; }
	static FORCEINLINE GLint GetMaxHullTextureImageUnits()		{ check(MaxHullTextureImageUnits != -1); return MaxHullTextureImageUnits; }
	static FORCEINLINE GLint GetMaxDomainTextureImageUnits()	{ check(MaxDomainTextureImageUnits != -1); return MaxDomainTextureImageUnits; }
	static FORCEINLINE GLint GetMaxComputeTextureImageUnits()	{ return 0; }
	static FORCEINLINE GLint GetMaxCombinedTextureImageUnits()	{ check(MaxCombinedTextureImageUnits != -1); return MaxCombinedTextureImageUnits; }

	static FORCEINLINE GLint GetFirstPixelTextureUnit()			{ return 0; }
	static FORCEINLINE GLint GetFirstVertexTextureUnit()		{ return GetFirstPixelTextureUnit() + GetMaxTextureImageUnits(); }
	static FORCEINLINE GLint GetFirstGeometryTextureUnit()		{ return GetFirstVertexTextureUnit() + GetMaxVertexTextureImageUnits(); }
	static FORCEINLINE GLint GetFirstHullTextureUnit()			{ return GetFirstGeometryTextureUnit() + GetMaxGeometryTextureImageUnits(); }
	static FORCEINLINE GLint GetFirstDomainTextureUnit()		{ return GetFirstHullTextureUnit() + GetMaxHullTextureImageUnits(); }

	static FORCEINLINE GLint GetFirstComputeTextureUnit()		{ return 0; }
	static FORCEINLINE GLint GetFirstComputeUAVUnit()			{ return 0; }

	static FORCEINLINE GLint GetMaxPixelUniformComponents()		{ check(MaxPixelUniformComponents != -1); return MaxPixelUniformComponents; }
	static FORCEINLINE GLint GetMaxVertexUniformComponents()	{ check(MaxVertexUniformComponents != -1); return MaxVertexUniformComponents; }
	static FORCEINLINE GLint GetMaxGeometryUniformComponents()	{ check(MaxGeometryUniformComponents != -1); return MaxGeometryUniformComponents; }
	static FORCEINLINE GLint GetMaxHullUniformComponents()		{ check(MaxHullUniformComponents != -1); return MaxHullUniformComponents; }
	static FORCEINLINE GLint GetMaxDomainUniformComponents()	{ check(MaxDomainUniformComponents != -1); return MaxDomainUniformComponents; }
	static FORCEINLINE GLint GetMaxComputeUniformComponents()	{ return 0; }

	static FORCEINLINE bool IsDebugContent()					{ return false; }
	static FORCEINLINE void InitDebugContext()					{ }


	// Silently ignored if not implemented:
	static FORCEINLINE void QueryTimestampCounter(GLuint QueryID) UGL_OPTIONAL_VOID
	static FORCEINLINE void BeginQuery(GLenum QueryType, GLuint QueryId) UGL_OPTIONAL_VOID
	static FORCEINLINE void EndQuery(GLenum QueryType) UGL_OPTIONAL_VOID
	static FORCEINLINE void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, uint64 *OutResult) UGL_OPTIONAL_VOID
	static FORCEINLINE void BindFragDataLocation(GLuint Program, GLuint Color, const GLchar *Name) UGL_OPTIONAL_VOID
	static FORCEINLINE void ReadBuffer(GLenum Mode) UGL_OPTIONAL_VOID
	static FORCEINLINE void DrawBuffer(GLenum Mode) UGL_OPTIONAL_VOID
	static FORCEINLINE void DeleteSync(UGLsync Sync) UGL_OPTIONAL_VOID
	static FORCEINLINE UGLsync FenceSync(GLenum Condition, GLbitfield Flags) UGL_OPTIONAL(0)
	static FORCEINLINE bool IsSync(UGLsync Sync) UGL_OPTIONAL(false)
	static FORCEINLINE EFenceResult ClientWaitSync(UGLsync Sync, GLbitfield Flags, GLuint64 Timeout) UGL_OPTIONAL(FR_WaitFailed)
	static FORCEINLINE void GenSamplers(GLsizei Count, GLuint *Samplers) UGL_OPTIONAL_VOID
	static FORCEINLINE void DeleteSamplers(GLsizei Count, GLuint *Samplers) UGL_OPTIONAL_VOID
	static FORCEINLINE void SetSamplerParameter(GLuint Sampler, GLenum Parameter, GLint Value) UGL_OPTIONAL_VOID
	static FORCEINLINE void BindSampler(GLuint Unit, GLuint Sampler) UGL_OPTIONAL_VOID
	static FORCEINLINE void PolygonMode(GLenum Face, GLenum Mode) UGL_OPTIONAL_VOID
	static FORCEINLINE void VertexAttribDivisor(GLuint Index, GLuint Divisor) UGL_OPTIONAL_VOID
	static FORCEINLINE void PushGroupMarker(const ANSICHAR* Name) UGL_OPTIONAL_VOID
	static FORCEINLINE void PopGroupMarker() UGL_OPTIONAL_VOID
	static FORCEINLINE void LabelObject(GLenum Type, GLuint Object, const ANSICHAR* Name) UGL_OPTIONAL_VOID
	static FORCEINLINE GLsizei GetLabelObject(GLenum Type, GLuint Object, GLsizei BufferSize, ANSICHAR* OutName) UGL_OPTIONAL(0)
	static FORCEINLINE void DiscardFramebufferEXT(GLenum Target, GLsizei NumAttachments, const GLenum* Attachments) UGL_OPTIONAL_VOID
	static FORCEINLINE void CopyTextureLevels(GLuint destinationTexture, GLuint sourceTexture, GLint sourceBaseLevel, GLsizei sourceLevelCount) UGL_OPTIONAL_VOID

	// Will assert at run-time if not implemented:
	static FORCEINLINE void* MapBufferRange(GLenum Type, uint32 InOffset, uint32 InSize, EResourceLockMode LockMode) UGL_REQUIRED(NULL)
	static FORCEINLINE void UnmapBufferRange(GLenum Type, uint32 InOffset, uint32 InSize) UGL_REQUIRED_VOID
	static FORCEINLINE void UnmapBuffer(GLenum Type) UGL_REQUIRED_VOID
	static FORCEINLINE void GenQueries(GLsizei NumQueries, GLuint* QueryIDs) UGL_REQUIRED_VOID
	static FORCEINLINE void DeleteQueries(GLsizei NumQueries, const GLuint* QueryIDs) UGL_REQUIRED_VOID
	static FORCEINLINE void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint *OutResult) UGL_REQUIRED_VOID
	static FORCEINLINE void BindBufferBase(GLenum Target, GLuint Index, GLuint Buffer) UGL_REQUIRED_VOID
	static FORCEINLINE GLuint GetUniformBlockIndex(GLuint Program, const GLchar *UniformBlockName) UGL_REQUIRED(-1)
	static FORCEINLINE void UniformBlockBinding(GLuint Program, GLuint UniformBlockIndex, GLuint UniformBlockBinding) UGL_REQUIRED_VOID
	static FORCEINLINE void Uniform4uiv(GLint Location, GLsizei Count, const GLuint* Value) UGL_REQUIRED_VOID
	static FORCEINLINE void TexParameter(GLenum Target, GLenum Parameter, GLint Value) UGL_REQUIRED_VOID
	static FORCEINLINE void FramebufferTexture(GLenum Target, GLenum Attachment, GLuint Texture, GLint Level) UGL_REQUIRED_VOID
	static FORCEINLINE void FramebufferTexture2D(GLenum Target, GLenum Attachment, GLenum TexTarget, GLuint Texture, GLint Level)
	{
		glFramebufferTexture2D(Target, Attachment, TexTarget, Texture, Level);
	}
	static FORCEINLINE void FramebufferTexture3D(GLenum Target, GLenum Attachment, GLenum TexTarget, GLuint Texture, GLint Level, GLint ZOffset) UGL_REQUIRED_VOID
	static FORCEINLINE void FramebufferTextureLayer(GLenum Target, GLenum Attachment, GLuint Texture, GLint Level, GLint Layer) UGL_REQUIRED_VOID
	static FORCEINLINE void FramebufferRenderbuffer(GLenum Target, GLenum Attachment, GLenum RenderBufferTarget, GLuint RenderBuffer)
	{
		glFramebufferRenderbuffer(Target, Attachment, RenderBufferTarget, RenderBuffer);
	}
	static FORCEINLINE void BlitFramebuffer(GLint SrcX0, GLint SrcY0, GLint SrcX1, GLint SrcY1, GLint DstX0, GLint DstY0, GLint DstX1, GLint DstY1, GLbitfield Mask, GLenum Filter) UGL_REQUIRED_VOID
	static FORCEINLINE void DrawBuffers(GLsizei NumBuffers, const GLenum *Buffers) UGL_REQUIRED_VOID
	static FORCEINLINE void DepthRange(GLdouble Near, GLdouble Far) UGL_REQUIRED_VOID
	static FORCEINLINE void EnableIndexed(GLenum Parameter, GLuint Index) UGL_REQUIRED_VOID
	static FORCEINLINE void DisableIndexed(GLenum Parameter, GLuint Index) UGL_REQUIRED_VOID
	static FORCEINLINE void ColorMaskIndexed(GLuint Index, GLboolean Red, GLboolean Green, GLboolean Blue, GLboolean Alpha) UGL_REQUIRED_VOID
	static FORCEINLINE void VertexAttribPointer(GLuint Index, GLint Size, GLenum Type, GLboolean Normalized, GLsizei Stride, const GLvoid* Pointer) UGL_REQUIRED_VOID
	static FORCEINLINE void VertexAttribIPointer(GLuint Index, GLint Size, GLenum Type, GLsizei Stride, const GLvoid* Pointer) UGL_REQUIRED_VOID
	static FORCEINLINE void VertexAttrib4Nsv(GLuint AttributeIndex, const GLshort* Values) UGL_REQUIRED_VOID
	static FORCEINLINE void VertexAttrib4sv(GLuint AttributeIndex, const GLshort* Values) UGL_REQUIRED_VOID
	static FORCEINLINE void VertexAttribI4sv(GLuint AttributeIndex, const GLshort* Values) UGL_REQUIRED_VOID
	static FORCEINLINE void VertexAttribI4usv(GLuint AttributeIndex, const GLushort* Values) UGL_REQUIRED_VOID
	static FORCEINLINE void VertexAttrib4Nubv(GLuint AttributeIndex, const GLubyte* Values) UGL_REQUIRED_VOID
	static FORCEINLINE void VertexAttrib4ubv(GLuint AttributeIndex, const GLubyte* Values) UGL_REQUIRED_VOID
	static FORCEINLINE void VertexAttribI4ubv(GLuint AttributeIndex, const GLubyte* Values) UGL_REQUIRED_VOID
	static FORCEINLINE void VertexAttrib4Nbv(GLuint AttributeIndex, const GLbyte* Values) UGL_REQUIRED_VOID
	static FORCEINLINE void VertexAttrib4bv(GLuint AttributeIndex, const GLbyte* Values) UGL_REQUIRED_VOID
	static FORCEINLINE void VertexAttribI4bv(GLuint AttributeIndex, const GLbyte* Values) UGL_REQUIRED_VOID
	static FORCEINLINE void VertexAttrib4dv(GLuint AttributeIndex, const GLdouble* Values) UGL_REQUIRED_VOID
	static FORCEINLINE void VertexAttribI4iv(GLuint AttributeIndex, const GLint* Values) UGL_REQUIRED_VOID
	static FORCEINLINE void VertexAttribI4uiv(GLuint AttributeIndex, const GLuint* Values) UGL_REQUIRED_VOID
	static FORCEINLINE void DrawArraysInstanced(GLenum Mode, GLint First, GLsizei Count, GLsizei InstanceCount) UGL_REQUIRED_VOID
	static FORCEINLINE void DrawElementsInstanced(GLenum Mode, GLsizei Count, GLenum Type, const GLvoid* Indices, GLsizei InstanceCount) UGL_REQUIRED_VOID
	static FORCEINLINE void DrawRangeElements(GLenum Mode, GLuint Start, GLuint End, GLsizei Count, GLenum Type, const GLvoid* Indices) UGL_REQUIRED_VOID
	static FORCEINLINE void ClearBufferfv(GLenum Buffer, GLint DrawBufferIndex, const GLfloat* Value) UGL_REQUIRED_VOID
	static FORCEINLINE void ClearBufferfi(GLenum Buffer, GLint DrawBufferIndex, GLfloat Depth, GLint Stencil) UGL_REQUIRED_VOID
	static FORCEINLINE void ClearBufferiv(GLenum Buffer, GLint DrawBufferIndex, const GLint* Value) UGL_REQUIRED_VOID
	static FORCEINLINE void ClearDepth(GLdouble Depth) UGL_REQUIRED_VOID
	static FORCEINLINE void TexImage3D(GLenum Target, GLint Level, GLint InternalFormat, GLsizei Width, GLsizei Height, GLsizei Depth, GLint Border, GLenum Format, GLenum Type, const GLvoid* PixelData) UGL_REQUIRED_VOID
	static FORCEINLINE void CompressedTexImage3D(GLenum Target, GLint Level, GLenum InternalFormat, GLsizei Width, GLsizei Height, GLsizei Depth, GLint Border, GLsizei ImageSize, const GLvoid* PixelData) UGL_REQUIRED_VOID
	static FORCEINLINE void TexImage2DMultisample(GLenum Target, GLsizei Samples, GLint InternalFormat, GLsizei Width, GLsizei Height, GLboolean FixedSampleLocations) UGL_REQUIRED_VOID
	static FORCEINLINE void TexBuffer(GLenum Target, GLenum InternalFormat, GLuint Buffer) UGL_REQUIRED_VOID
	static FORCEINLINE void TexSubImage3D(GLenum Target, GLint Level, GLint XOffset, GLint YOffset, GLint ZOffset, GLsizei Width, GLsizei Height, GLsizei Depth, GLenum Format, GLenum Type, const GLvoid* PixelData) UGL_REQUIRED_VOID
	static FORCEINLINE void	CopyTexSubImage3D(GLenum Target, GLint Level, GLint XOffset, GLint YOffset, GLint ZOffset, GLint X, GLint Y, GLsizei Width, GLsizei Height) UGL_REQUIRED_VOID
	static FORCEINLINE void GetCompressedTexImage(GLenum Target, GLint Level, GLvoid* OutImageData) UGL_REQUIRED_VOID
	static FORCEINLINE void GetTexImage(GLenum Target, GLint Level, GLenum Format, GLenum Type, GLvoid* OutPixelData) UGL_REQUIRED_VOID
	static FORCEINLINE void CopyBufferSubData(GLenum ReadTarget, GLenum WriteTarget, GLintptr ReadOffset, GLintptr WriteOffset, GLsizeiptr Size) UGL_REQUIRED_VOID
	static FORCEINLINE const ANSICHAR* GetStringIndexed(GLenum Name, GLuint Index) UGL_REQUIRED(NULL)
	static FORCEINLINE GLuint GetMajorVersion() UGL_REQUIRED(0)
	static FORCEINLINE GLuint GetMinorVersion() UGL_REQUIRED(0)
	static FORCEINLINE ERHIFeatureLevel::Type GetFeatureLevel() UGL_REQUIRED(ERHIFeatureLevel::SM4)
	static FORCEINLINE EShaderPlatform GetShaderPlatform() UGL_REQUIRED(SP_OPENGL_SM4)
	static FORCEINLINE FString GetAdapterName() UGL_REQUIRED(TEXT(""))
	static FORCEINLINE void BlendFuncSeparatei(GLuint Buf, GLenum SrcRGB, GLenum DstRGB, GLenum SrcAlpha, GLenum DstAlpha) UGL_REQUIRED_VOID
	static FORCEINLINE void BlendEquationSeparatei(GLuint Buf, GLenum ModeRGB, GLenum ModeAlpha) UGL_REQUIRED_VOID
	static FORCEINLINE void BlendFunci(GLuint Buf, GLenum Src, GLenum Dst) UGL_REQUIRED_VOID
	static FORCEINLINE void BlendEquationi(GLuint Buf, GLenum Mode) UGL_REQUIRED_VOID
	static FORCEINLINE void PatchParameteri(GLenum Pname, GLint Value) UGL_REQUIRED_VOID
	static FORCEINLINE void BindImageTexture(GLuint Unit, GLuint Texture, GLint Level, GLboolean Layered, GLint Layer, GLenum Access, GLenum Format) UGL_REQUIRED_VOID
	static FORCEINLINE void DispatchCompute(GLuint NumGroupsX, GLuint NumGroupsY, GLuint NumGroupsZ) UGL_REQUIRED_VOID
	static FORCEINLINE void MemoryBarrier(GLbitfield Barriers) UGL_REQUIRED_VOID
	static FORCEINLINE bool TexStorage2D(GLenum Target, GLint Levels, GLint InternalFormat, GLsizei Width, GLsizei Height, GLenum Format, GLenum Type, uint32 Flags) UGL_OPTIONAL(false)
	static FORCEINLINE void TexStorage3D(GLenum Target, GLint Levels, GLint InternalFormat, GLsizei Width, GLsizei Height, GLsizei Depth, GLenum Format, GLenum Type) UGL_REQUIRED_VOID
	static FORCEINLINE void CompressedTexSubImage3D(GLenum Target, GLint Level, GLint XOffset, GLint YOffset, GLint ZOffset, GLsizei Width, GLsizei Height, GLsizei Depth, GLenum Format, GLsizei ImageSize, const GLvoid* PixelData) UGL_REQUIRED_VOID
	static FORCEINLINE void CopyImageSubData(GLuint SrcName, GLenum SrcTarget, GLint SrcLevel, GLint SrcX, GLint SrcY, GLint SrcZ, GLuint DstName, GLenum DstTarget, GLint DstLevel, GLint DstX, GLint DstY, GLint DstZ, GLsizei Width, GLsizei Height, GLsizei Depth) UGL_REQUIRED_VOID

	static FPlatformOpenGLDevice*	CreateDevice() UGL_REQUIRED(NULL)
	static FPlatformOpenGLContext*	CreateContext( FPlatformOpenGLDevice* Device, void* WindowHandle ) UGL_REQUIRED(NULL)

	static FORCEINLINE void BufferSubData(GLenum Target, GLintptr Offset, GLsizeiptr Size, const GLvoid* Data)
	{
		glBufferSubData(Target, Offset, Size, Data);
	}

	static FORCEINLINE void CheckFrameBuffer() 
	{
#if UE_BUILD_DEBUG 
		GLenum CompleteResult = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (CompleteResult != GL_FRAMEBUFFER_COMPLETE)
		{
				UE_LOG(LogRHI, Fatal,TEXT("Framebuffer not complete. Status = 0x%x"), CompleteResult);
		}
#endif 
	}

protected:
	static GLint MaxTextureImageUnits;
	static GLint MaxCombinedTextureImageUnits;
	static GLint MaxVertexTextureImageUnits;
	static GLint MaxGeometryTextureImageUnits;
	static GLint MaxHullTextureImageUnits;
	static GLint MaxDomainTextureImageUnits;
	static GLint MaxVertexUniformComponents;
	static GLint MaxPixelUniformComponents;
	static GLint MaxGeometryUniformComponents;
	static GLint MaxHullUniformComponents;
	static GLint MaxDomainUniformComponents;

	/** GL_KHR_texture_compression_astc_ldr */
	static bool bSupportsASTC;

	/** GL_ARB_copy_image */
	static bool bSupportsCopyImage;

	/** GL_ARB_seamless_cube_map */
	static bool bSupportsSeamlessCubemap;
};

/** Unreal tokens that maps to different OpenGL tokens by platform. */
#define UGL_DRAW_FRAMEBUFFER	GL_DRAW_FRAMEBUFFER
#define UGL_READ_FRAMEBUFFER	GL_READ_FRAMEBUFFER
#define UGL_ABGR8				GL_UNSIGNED_INT_8_8_8_8_REV
#define UGL_ANY_SAMPLES_PASSED	GL_ANY_SAMPLES_PASSED
#define UGL_SAMPLES_PASSED		GL_SAMPLES_PASSED
#define UGL_TIME_ELAPSED		GL_TIME_ELAPSED
#define UGL_CLAMP_TO_BORDER		GL_CLAMP_TO_BORDER

/** OpenGL extensions */

// http://www.opengl.org/registry/specs/EXT/texture_compression_s3tc.txt
#if !defined(GL_EXT_texture_compression_s3tc)
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT			0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT		0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT		0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT		0x83F3
#endif

// http://www.khronos.org/registry/gles/extensions/IMG/IMG_texture_compression_pvrtc.txt
#if !defined(GL_IMG_texture_compression_pvrtc)
#define GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG		0x8C00
#define GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG		0x8C01
#define GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG		0x8C02
#define GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG		0x8C03
#endif

// http://www.khronos.org/registry/gles/extensions/AMD/AMD_compressed_ATC_texture.txt
#if !defined(GL_ATI_texture_compression_atitc)
#define GL_ATC_RGB_AMD							0x8C92
#define GL_ATC_RGBA_EXPLICIT_ALPHA_AMD			0x8C93
#define GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD		0x87EE
#endif

#if !defined(GL_OES_compressed_ETC1_RGB8_texture)
#define GL_ETC1_RGB8_OES                        0x8D64
#endif

// http://www.opengl.org/registry/specs/EXT/texture_sRGB.txt
#if !defined(GL_EXT_texture_sRGB)
#define GL_SRGB_EXT                       0x8C40
#define GL_SRGB8_EXT                      0x8C41
#define GL_SRGB_ALPHA_EXT                 0x8C42
#define GL_SRGB8_ALPHA8_EXT               0x8C43
#define GL_SLUMINANCE_ALPHA_EXT           0x8C44
#define GL_SLUMINANCE8_ALPHA8_EXT         0x8C45
#define GL_SLUMINANCE_EXT                 0x8C46
#define GL_SLUMINANCE8_EXT                0x8C47
#define GL_COMPRESSED_SRGB_EXT            0x8C48
#define GL_COMPRESSED_SRGB_ALPHA_EXT      0x8C49
#define GL_COMPRESSED_SLUMINANCE_EXT      0x8C4A
#define GL_COMPRESSED_SLUMINANCE_ALPHA_EXT 0x8C4B
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT  0x8C4C
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif

// http://www.opengl.org/registry/specs/ARB/texture_compression_rgtc.txt
#if !defined(GL_ARB_texture_compression_rgtc)
#define GL_COMPRESSED_RED_RGTC1           0x8DBB
#define GL_COMPRESSED_SIGNED_RED_RGTC1    0x8DBC
#define GL_COMPRESSED_RG_RGTC2            0x8DBD
#define GL_COMPRESSED_SIGNED_RG_RGTC2     0x8DBE
#endif

// http://www.khronos.org/registry/gles/extensions/AMD/AMD_compressed_ATC_texture.txt
#if !defined(GL_ATI_texture_compression_atitc)
#define GL_ATC_RGB_AMD							0x8C92
#define GL_ATC_RGBA_EXPLICIT_ALPHA_AMD			0x8C93
#define GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD		0x87EE
#endif

/* http://www.khronos.org/registry/gles/extensions/NV/NV_sRGB_formats.txt */
#if !defined(GL_NV_sRGB_formats)
#define GL_SLUMINANCE_NV                                        0x8C46
#define GL_SLUMINANCE_ALPHA_NV                                  0x8C44
#define GL_SRGB8_NV                                             0x8C41
#define GL_SLUMINANCE8_NV                                       0x8C47
#define GL_SLUMINANCE8_ALPHA8_NV                                0x8C45
#define GL_COMPRESSED_SRGB_S3TC_DXT1_NV                         0x8C4C
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_NV                   0x8C4D
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV                   0x8C4E
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV                   0x8C4F
#define GL_ETC1_SRGB8_NV                                        0x88EE
#endif

// http://www.opengl.org/registry/specs/KHR/texture_compression_astc_ldr.txt
#if !defined(GL_KHR_texture_compression_astc_ldr)
#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR            0x93B0
#define GL_COMPRESSED_RGBA_ASTC_5x4_KHR            0x93B1
#define GL_COMPRESSED_RGBA_ASTC_5x5_KHR            0x93B2
#define GL_COMPRESSED_RGBA_ASTC_6x5_KHR            0x93B3
#define GL_COMPRESSED_RGBA_ASTC_6x6_KHR            0x93B4
#define GL_COMPRESSED_RGBA_ASTC_8x5_KHR            0x93B5
#define GL_COMPRESSED_RGBA_ASTC_8x6_KHR            0x93B6
#define GL_COMPRESSED_RGBA_ASTC_8x8_KHR            0x93B7
#define GL_COMPRESSED_RGBA_ASTC_10x5_KHR           0x93B8
#define GL_COMPRESSED_RGBA_ASTC_10x6_KHR           0x93B9
#define GL_COMPRESSED_RGBA_ASTC_10x8_KHR           0x93BA
#define GL_COMPRESSED_RGBA_ASTC_10x10_KHR          0x93BB
#define GL_COMPRESSED_RGBA_ASTC_12x10_KHR          0x93BC
#define GL_COMPRESSED_RGBA_ASTC_12x12_KHR          0x93BD

#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR    0x93D0
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR    0x93D1
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR    0x93D2
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR    0x93D3
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR    0x93D4
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR    0x93D5
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR    0x93D6
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR    0x93D7
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR   0x93D8
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR   0x93D9
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR   0x93DA
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR  0x93DB
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR  0x93DC
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR  0x93DD
#endif

#if !defined(GL_TESS_EVALUATION_SHADER)
#define GL_TESS_EVALUATION_SHADER					0x8E87
#endif
#if !defined(GL_TESS_CONTROL_SHADER)
#define GL_TESS_CONTROL_SHADER						0x8E88
#endif
#if !defined(GL_PATCHES)
#define GL_PATCHES									0x000E
#endif
#if !defined(GL_PATCH_VERTICES)
#define GL_PATCH_VERTICES							0x8E72
#endif
#if !defined(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS)
#define GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS			0x8C29
#endif
#if !defined(GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS)
#define GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS		0x8E81
#endif
#if !defined(GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS)
#define GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS	0x8E82
#endif
#if !defined(GL_READ_WRITE)
#define GL_READ_WRITE								0x88BA
#endif
#if !defined(GL_ALL_BARRIER_BITS)
#define GL_ALL_BARRIER_BITS							0xFFFFFFFF
#endif
#if !defined(GL_TEXTURE_CUBE_MAP_ARRAY)
#define GL_TEXTURE_CUBE_MAP_ARRAY					0x9009
#endif
#if !defined(GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER)
#define GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER		0x84F0
#endif
#if !defined(GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_EVALUATION_SHADER)
#define GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_EVALUATION_SHADER	0x84F1
#endif
#if !defined(GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER)
#define GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER			0x90EC
#endif
#ifndef GL_ARB_seamless_cube_map
#define GL_TEXTURE_CUBE_MAP_SEAMLESS				0x884F
#endif
#ifndef GL_TIME_ELAPSED_EXT
#define GL_TIME_ELAPSED_EXT							0x88BF
#endif
#ifndef GL_TIMESTAMP_EXT
#define GL_TIMESTAMP_EXT							0x8E28
#endif

#if PLATFORM_HTML5
// WebGL has this in core spec: http://www.khronos.org/registry/webgl/specs/latest/1.0/#6.6
#ifndef GL_DEPTH_STENCIL_ATTACHMENT
#define GL_DEPTH_STENCIL_ATTACHMENT					0x821A
#endif
#endif

#ifndef GL_GPU_DISJOINT_EXT
#define GL_GPU_DISJOINT_EXT							0x8FBB
#endif
