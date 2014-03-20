// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	VectorFieldVisualization.cpp: Visualization of vector fields.
==============================================================================*/

#include "EnginePrivate.h"
#include "VectorFieldVisualization.h"
#include "VectorField.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "GlobalShader.h"
#include "SceneManagement.h"
#include "FXSystem.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FVectorFieldVisualizationParameters,TEXT("VectorFieldVis"));

/*------------------------------------------------------------------------------
	Vertex factory for visualizing vector fields.
------------------------------------------------------------------------------*/

/**
 * Shader parameters for the vector field visualization vertex factory.
 */
class FVectorFieldVisualizationVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:
	virtual void Bind( const FShaderParameterMap& ParameterMap ) OVERRIDE
	{
		VectorFieldTexture.Bind(ParameterMap, TEXT("VectorFieldTexture"));
		VectorFieldTextureSampler.Bind(ParameterMap, TEXT("VectorFieldTextureSampler"));
	}

	virtual void Serialize(FArchive& Ar) OVERRIDE
	{
		Ar << VectorFieldTexture;
		Ar << VectorFieldTextureSampler;
	}

	virtual void SetMesh(FShader* Shader,const FVertexFactory* VertexFactory,const FSceneView& View,const FMeshBatchElement& BatchElement,uint32 DataFlags) const OVERRIDE;

	virtual uint32 GetSize() const { return sizeof(*this); }

private:

	/** The vector field texture parameter. */
	FShaderResourceParameter VectorFieldTexture;
	FShaderResourceParameter VectorFieldTextureSampler;
};

/**
 * Vertex declaration for visualizing vector fields.
 */
class FVectorFieldVisualizationVertexDeclaration : public FRenderResource
{
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0,0,VET_Float4,0));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};
TGlobalResource<FVectorFieldVisualizationVertexDeclaration> GVectorFieldVisualizationVertexDeclaration;

/**
 * A dummy vertex buffer to bind when visualizing vector fields. This prevents
 * some D3D debug warnings about zero-element input layouts but is not strictly
 * required.
 */
class FDummyVertexBuffer : public FVertexBuffer
{
public:

	virtual void InitRHI()
	{
		VertexBufferRHI = RHICreateVertexBuffer(sizeof(FVector4) * 2, NULL, BUF_Static);
		FVector4* DummyContents = (FVector4*)RHILockVertexBuffer(VertexBufferRHI,0,sizeof(FVector4)*2,RLM_WriteOnly);
		DummyContents[0] = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		DummyContents[1] = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		RHIUnlockVertexBuffer(VertexBufferRHI);
	}
};
TGlobalResource<FDummyVertexBuffer> GDummyVertexBuffer;

/**
 * Constructs render resources for this vertex factory.
 */
void FVectorFieldVisualizationVertexFactory::InitRHI()
{
	FVertexStream Stream;

	// No streams should currently exist.
	check( Streams.Num() == 0 );

	// Stream 0: Global particle texture coordinate buffer.
	Stream.VertexBuffer = &GDummyVertexBuffer;
	Stream.Stride = sizeof(FVector4);
	Stream.Offset = 0;
	Streams.Add(Stream);

	// Set the declaration.
	check( IsValidRef(GVectorFieldVisualizationVertexDeclaration.VertexDeclarationRHI) );
	SetDeclaration( GVectorFieldVisualizationVertexDeclaration.VertexDeclarationRHI );
}

/**
 * Release render resources for this vertex factory.
 */
void FVectorFieldVisualizationVertexFactory::ReleaseRHI()
{
	UniformBuffer.SafeRelease();
	VectorFieldTextureRHI = FTexture3DRHIParamRef();
	FVertexFactory::ReleaseRHI();
}

/**
 * Should we cache the material's shadertype on this platform with this vertex factory? 
 */
bool FVectorFieldVisualizationVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return Material->IsSpecialEngineMaterial() && SupportsGPUParticles(Platform);
}

/**
 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
 */
void FVectorFieldVisualizationVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	FVertexFactory::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
}

/**
 * Construct shader parameters for this type of vertex factory.
 */
FVertexFactoryShaderParameters* FVectorFieldVisualizationVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FVectorFieldVisualizationVertexFactoryShaderParameters() : NULL;
}

/**
 * Set parameters for this vertex factory instance.
 */
void FVectorFieldVisualizationVertexFactory::SetParameters(
	const FVectorFieldVisualizationParameters& InUniformParameters,
	FTexture3DRHIParamRef InVectorFieldTextureRHI )
{
	UniformBuffer = FVectorFieldVisualizationBufferRef::CreateUniformBufferImmediate(InUniformParameters, UniformBuffer_SingleUse);
	VectorFieldTextureRHI = InVectorFieldTextureRHI;
}

/**
 * Set vertex factory shader parameters.
 */
void FVectorFieldVisualizationVertexFactoryShaderParameters::SetMesh(FShader* Shader,const FVertexFactory* InVertexFactory,const FSceneView& View,const FMeshBatchElement& BatchElement,uint32 DataFlags) const
{
	FVectorFieldVisualizationVertexFactory* VertexFactory = (FVectorFieldVisualizationVertexFactory*)InVertexFactory;
	FVertexShaderRHIParamRef VertexShaderRHI = Shader->GetVertexShader();
	FSamplerStateRHIParamRef SamplerStatePoint = TStaticSamplerState<SF_Point>::GetRHI();
	SetUniformBufferParameter(VertexShaderRHI, Shader->GetUniformBufferParameter<FVectorFieldVisualizationParameters>(), VertexFactory->UniformBuffer);
	SetTextureParameter(VertexShaderRHI, VectorFieldTexture, VectorFieldTextureSampler, SamplerStatePoint, VertexFactory->VectorFieldTextureRHI);
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FVectorFieldVisualizationVertexFactory,"VectorFieldVisualizationVertexFactory",true,false,true,false,false);

/*------------------------------------------------------------------------------
	Drawing interface.
------------------------------------------------------------------------------*/

/**
 * Draw the bounds for a vector field instance.
 * @param PDI - The primitive drawing interface with which to draw.
 * @param View - The view in which to draw.
 * @param VectorFieldInstance - The vector field instance to draw.
 */
void DrawVectorFieldBounds(
	FPrimitiveDrawInterface* PDI,
	const FSceneView* View,
	FVectorFieldInstance* VectorFieldInstance )
{
	FVector Corners[8];
	FVectorFieldResource* Resource = VectorFieldInstance->Resource;
	const FVector HalfVoxelOffset(
		0.5f / Resource->SizeX, 0.5f / Resource->SizeY, 0.5f / Resource->SizeZ);
	const FVector LocalMin = -HalfVoxelOffset;
	const FVector LocalMax = FVector(1.0f) + HalfVoxelOffset;
	const FMatrix& VolumeToWorld = VectorFieldInstance->VolumeToWorld;
	const FLinearColor LineColor(1.0f,0.5f,0.0f,1.0f);
	const ESceneDepthPriorityGroup LineDPG = SDPG_World;

	// Compute all eight corners of the volume.
	Corners[0] = VolumeToWorld.TransformPosition(FVector(LocalMin.X, LocalMin.Y, LocalMin.Z));
	Corners[1] = VolumeToWorld.TransformPosition(FVector(LocalMax.X, LocalMin.Y, LocalMin.Z));
	Corners[2] = VolumeToWorld.TransformPosition(FVector(LocalMax.X, LocalMax.Y, LocalMin.Z));
	Corners[3] = VolumeToWorld.TransformPosition(FVector(LocalMin.X, LocalMax.Y, LocalMin.Z));
	Corners[4] = VolumeToWorld.TransformPosition(FVector(LocalMin.X, LocalMin.Y, LocalMax.Z));
	Corners[5] = VolumeToWorld.TransformPosition(FVector(LocalMax.X, LocalMin.Y, LocalMax.Z));
	Corners[6] = VolumeToWorld.TransformPosition(FVector(LocalMax.X, LocalMax.Y, LocalMax.Z));
	Corners[7] = VolumeToWorld.TransformPosition(FVector(LocalMin.X, LocalMax.Y, LocalMax.Z));

	// Draw the lines that form the box.
	for ( int32 Index = 0; Index < 4; ++Index )
	{
		const int32 NextIndex = (Index + 1) & 0x3;
		// Bottom face.
		PDI->DrawLine(Corners[Index], Corners[NextIndex], LineColor, LineDPG);
		// Top face.
		PDI->DrawLine(Corners[Index+4], Corners[NextIndex+4], LineColor, LineDPG);
		// Side faces.
		PDI->DrawLine(Corners[Index], Corners[Index+4], LineColor, LineDPG);
	}
}

/**
 * Draw the vector field for a vector field instance.
 * @param PDI - The primitive drawing interface with which to draw.
 * @param View - The view in which to draw.
 * @param VertexFactory - The vertex factory with which to draw.
 * @param VectorFieldInstance - The vector field instance to draw.
 */
void DrawVectorField(
	FPrimitiveDrawInterface* PDI,
	const FSceneView* View,
	FVectorFieldVisualizationVertexFactory* VertexFactory,
	FVectorFieldInstance* VectorFieldInstance )
{
	FVectorFieldResource* Resource = VectorFieldInstance->Resource;

	if (Resource && IsValidRef(Resource->VolumeTextureRHI))
	{
		// Set up parameters.
		FVectorFieldVisualizationParameters UniformParameters;
		UniformParameters.VolumeToWorld = VectorFieldInstance->VolumeToWorld;
		UniformParameters.VolumeToWorldNoScale = VectorFieldInstance->VolumeToWorldNoScale;
		UniformParameters.VoxelSize = FVector( 1.0f / Resource->SizeX, 1.0f / Resource->SizeY, 1.0f / Resource->SizeZ );
		UniformParameters.Scale = VectorFieldInstance->Intensity * Resource->Intensity;
		VertexFactory->SetParameters(UniformParameters, Resource->VolumeTextureRHI);

		// Create a mesh batch for the visualization.
		FMeshBatch MeshBatch;
		MeshBatch.CastShadow = false;
		MeshBatch.bUseAsOccluder = false;
		MeshBatch.VertexFactory = VertexFactory;
		MeshBatch.MaterialRenderProxy = GEngine->LevelColorationUnlitMaterial->GetRenderProxy(false);
		MeshBatch.Type = PT_LineList;

		// A single mesh element.
		FMeshBatchElement& MeshElement = MeshBatch.Elements[0];
		MeshElement.NumPrimitives = 1;
		MeshElement.NumInstances = Resource->SizeX * Resource->SizeY * Resource->SizeZ;
		MeshElement.FirstIndex = 0;
		MeshElement.MinVertexIndex = 0;
		MeshElement.MaxVertexIndex = 1;
		MeshElement.PrimitiveUniformBufferResource = &GIdentityPrimitiveUniformBuffer;

		// Draw!
		PDI->DrawMesh(MeshBatch);
	}
}
