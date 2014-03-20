// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalRenderGPUSkin.cpp: GPU skinned skeletal mesh rendering code.

	This code contains embedded portions of source code from dqconv.c Conversion routines between (regular quaternion, translation) and dual quaternion, Version 1.0.0, Copyright ?2006-2007 University of Dublin, Trinity College, All Rights Reserved, which have been altered from their original version.

	The following terms apply to dqconv.c Conversion routines between (regular quaternion, translation) and dual quaternion, Version 1.0.0:

	This software is provided 'as-is', without any express or implied warranty.  In no event will the author(s) be held liable for any damages arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software. If you use this software
	in a product, an acknowledgment in the product documentation would be
	appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.
=============================================================================*/

#include "EnginePrivate.h"
#include "GPUSkinVertexFactory.h"
#include "SkeletalRenderGPUSkin.h"
#include "SkeletalRenderCPUSkin.h"
#include "GPUSkinCache.h"

DEFINE_LOG_CATEGORY_STATIC(LogSkeletalGPUSkinMesh, Warning, All);

// 0/1
#define UPDATE_PER_BONE_DATA_ONLY_FOR_OBJECT_BEEN_VISIBLE 1

DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Update"),STAT_MorphVertexBuffer_Update,STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Init"),STAT_MorphVertexBuffer_Init,STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Apply Delta"),STAT_MorphVertexBuffer_ApplyDelta,STATGROUP_MorphTarget);


FPreviousPerBoneMotionBlur GPrevPerBoneMotionBlur;

/*-----------------------------------------------------------------------------
FMorphVertexBuffer
-----------------------------------------------------------------------------*/

/** 
* Initialize the dynamic RHI for this rendering resource 
*/
void FMorphVertexBuffer::InitDynamicRHI()
{
	// LOD of the skel mesh is used to find number of vertices in buffer
	FStaticLODModel& LodModel = SkelMeshResource->LODModels[LODIdx];

	// Create the buffer rendering resource
	uint32 Size = LodModel.NumVertices * sizeof(FMorphGPUSkinVertex);
	VertexBufferRHI = RHICreateVertexBuffer(Size,NULL,BUF_Dynamic);

	// Lock the buffer.
	FMorphGPUSkinVertex* Buffer = (FMorphGPUSkinVertex*) RHILockVertexBuffer(VertexBufferRHI,0,Size,RLM_WriteOnly);

	// zero all deltas (NOTE: DeltaTangentZ is FPackedNormal, so we can't just FMemory::Memzero)
	for (uint32 VertIndex=0; VertIndex < LodModel.NumVertices; ++VertIndex)
	{
		Buffer[VertIndex].DeltaPosition = FVector::ZeroVector;
		Buffer[VertIndex].DeltaTangentZ = FVector::ZeroVector;
	}

	// Unlock the buffer.
	RHIUnlockVertexBuffer(VertexBufferRHI);
	
	// hasn't been updated yet
	bHasBeenUpdated = false;
}

/** 
* Release the dynamic RHI for this rendering resource 
*/
void FMorphVertexBuffer::ReleaseDynamicRHI()
{
	VertexBufferRHI.SafeRelease();
}

/*-----------------------------------------------------------------------------
FSkeletalMeshObjectGPUSkin
-----------------------------------------------------------------------------*/

FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectGPUSkin(USkinnedMeshComponent* InMeshComponent, FSkeletalMeshResource* InSkeletalMeshResource) 
:	FSkeletalMeshObject(InMeshComponent, InSkeletalMeshResource)
,	DynamicData(NULL)
,	bMorphResourcesInitialized(false)
{
	// create LODs to match the base mesh
	LODs.Empty(SkeletalMeshResource->LODModels.Num());
	for( int32 LODIndex=0;LODIndex < SkeletalMeshResource->LODModels.Num();LODIndex++ )
	{
		new(LODs) FSkeletalMeshObjectLOD(SkeletalMeshResource,LODIndex);
	}

	InitResources();
}


FSkeletalMeshObjectGPUSkin::~FSkeletalMeshObjectGPUSkin()
{
	delete DynamicData;
}


void FSkeletalMeshObjectGPUSkin::InitResources()
{
	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo[LODIndex];
		SkelLOD.InitResources(MeshLODInfo);
	}
}

void FSkeletalMeshObjectGPUSkin::ReleaseResources()
{
	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		SkelLOD.ReleaseResources();
	}
	// also release morph resources
	ReleaseMorphResources();
}

void FSkeletalMeshObjectGPUSkin::InitMorphResources(bool bInUsePerBoneMotionBlur)
{
	if( bMorphResourcesInitialized )
	{
		// release first if already initialized
		ReleaseMorphResources();
	}

	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		// init any morph vertex buffers for each LOD
		const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo[LODIndex];
		SkelLOD.InitMorphResources(MeshLODInfo,bInUsePerBoneMotionBlur);
	}
	bMorphResourcesInitialized = true;
}

void FSkeletalMeshObjectGPUSkin::ReleaseMorphResources()
{
	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		// release morph vertex buffers and factories if they were created
		SkelLOD.ReleaseMorphResources();
	}
	bMorphResourcesInitialized = false;
}


void FSkeletalMeshObjectGPUSkin::Update(int32 LODIndex,USkinnedMeshComponent* InMeshComponent,const TArray<FActiveVertexAnim>& ActiveVertexAnims)
{
	// make sure morph data has been initialized for each LOD
	if( !bMorphResourcesInitialized && ActiveVertexAnims.Num() > 0 )
	{
		// initialized on-the-fly in order to avoid creating extra vertex streams for each skel mesh instance
		InitMorphResources(InMeshComponent->bPerBoneMotionBlur);		
	}

	// create the new dynamic data for use by the rendering thread
	// this data is only deleted when another update is sent
	FDynamicSkelMeshObjectDataGPUSkin* NewDynamicData = new FDynamicSkelMeshObjectDataGPUSkin(InMeshComponent,SkeletalMeshResource,LODIndex,ActiveVertexAnims);


	// queue a call to update this data
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		SkelMeshObjectUpdateDataCommand,
		FSkeletalMeshObject*, MeshObject, this,
		FDynamicSkelMeshObjectData*, NewDynamicData, NewDynamicData,
	{
		FScopeCycleCounter Context(MeshObject->GetStatId());
		MeshObject->UpdateDynamicData_RenderThread(NewDynamicData);
	}
	);

	if( GIsEditor )
	{
		// this does not need thread-safe update
		ProgressiveDrawingFraction = InMeshComponent->ProgressiveDrawingFraction;
		CustomSortAlternateIndexMode = (ECustomSortAlternateIndexMode)InMeshComponent->CustomSortAlternateIndexMode;
	}
}

void FSkeletalMeshObjectGPUSkin::UpdateDynamicData_RenderThread(FDynamicSkelMeshObjectData* InDynamicData)
{
	SCOPE_CYCLE_COUNTER(STAT_GPUSkinUpdateRTTime);
	bool bMorphNeedsUpdate=false;
	// figure out if the morphing vertex buffer needs to be updated. compare old vs new active morphs
	bMorphNeedsUpdate = DynamicData ? (DynamicData->LODIndex != ((FDynamicSkelMeshObjectDataGPUSkin*)InDynamicData)->LODIndex ||
		!DynamicData->ActiveVertexAnimsEqual(((FDynamicSkelMeshObjectDataGPUSkin*)InDynamicData)->ActiveVertexAnims))
		: true;

	// we should be done with the old data at this point
	delete DynamicData;
	// update with new data
	DynamicData = (FDynamicSkelMeshObjectDataGPUSkin*)InDynamicData;
	checkSlow(DynamicData);

	FSkeletalMeshObjectLOD& LOD = LODs[DynamicData->LODIndex];
	const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo[DynamicData->LODIndex];

	// if hasn't been updated, force update again
	bMorphNeedsUpdate = LOD.MorphVertexBuffer.bHasBeenUpdated? bMorphNeedsUpdate:true;

	const FStaticLODModel& LODModel = SkeletalMeshResource->LODModels[DynamicData->LODIndex];
	const TArray<FSkelMeshChunk>& Chunks = GetRenderChunks(DynamicData->LODIndex);

	// use correct vertex factories based on alternate weights usage
	FVertexFactoryData& VertexFactoryData = LOD.GPUSkinVertexFactories;

	bool DataPresent = false;

	if(DynamicData->NumWeightedActiveVertexAnims > 0) 
	{
		DataPresent = true;
		checkSlow((VertexFactoryData.MorphVertexFactories.Num() == Chunks.Num()));
	}
	else
	{
//		checkSlow(VertexFactoryData.MorphVertexFactories.Num() == 0);
		DataPresent = VertexFactoryData.VertexFactories.Num() > 0;
	}

	if(DataPresent)
	{
		bool bGPUSkinCacheEnabled = GEnableGPUSkinCache && (GRHIFeatureLevel >= ERHIFeatureLevel::SM5);
		for( int32 ChunkIdx=0; ChunkIdx < Chunks.Num(); ChunkIdx++ )
		{
			const FSkelMeshChunk& Chunk = Chunks[ChunkIdx];

			bool bClothFactory = (DynamicData->ClothSimulUpdateData.Num() > 0) && Chunk.HasApexClothData();

			if (GRHIFeatureLevel < ERHIFeatureLevel::SM3)
			{
				bClothFactory = false;
			}

			FGPUBaseSkinVertexFactory::ShaderDataType& ShaderData = 
				bClothFactory ? VertexFactoryData.ClothVertexFactories[ChunkIdx]->GetVertexFactory()->GetShaderData() :
				(DynamicData->NumWeightedActiveVertexAnims > 0 ? VertexFactoryData.MorphVertexFactories[ChunkIdx].GetShaderData() : VertexFactoryData.VertexFactories[ChunkIdx].GetShaderData());

			TArray<FBoneSkinning>& ChunkMatrices = ShaderData.BoneMatrices;

			// update bone matrix shader data for the vertex factory of each chunk
			ChunkMatrices.Reset(); // remove all elts but leave allocated

			const int32 NumBones = Chunk.BoneMap.Num();
			ChunkMatrices.Reserve( NumBones ); // we are going to keep adding data to this for each bone

			const int32 NumToAdd = NumBones - ChunkMatrices.Num();
			ChunkMatrices.AddUninitialized( NumToAdd );

			//FSkinMatrix3x4 is sizeof() == 48
			// CACHE_LINE_SIZE (128) / 48 = 2.6
			//  sizeof(FMatrix) == 64
			// CACHE_LINE_SIZE (128) / 64 = 2
			const int32 PreFetchStride = 2; // FPlatformMisc::Prefetch stride
    
			TArray<FMatrix>& ReferenceToLocalMatrices = DynamicData->ReferenceToLocal;
			const int32 NumReferenceToLocal = ReferenceToLocalMatrices.Num();
			for( int32 BoneIdx=0; BoneIdx < NumBones; BoneIdx++ )
			{
				FPlatformMisc::Prefetch( ChunkMatrices.GetTypedData() + BoneIdx + PreFetchStride ); 
				FPlatformMisc::Prefetch( ChunkMatrices.GetTypedData() + BoneIdx + PreFetchStride, CACHE_LINE_SIZE ); 
				FPlatformMisc::Prefetch( ReferenceToLocalMatrices.GetTypedData() + BoneIdx + PreFetchStride );
				FPlatformMisc::Prefetch( ReferenceToLocalMatrices.GetTypedData() + BoneIdx + PreFetchStride, CACHE_LINE_SIZE );
    
				FBoneSkinning& BoneMat = ChunkMatrices[BoneIdx];
				const FBoneIndexType RefToLocalIdx = Chunk.BoneMap[BoneIdx];
				const FMatrix& RefToLocal = ReferenceToLocalMatrices[RefToLocalIdx];
				RefToLocal.To3x4MatrixTranspose( (float*)BoneMat.M );
			}

			// Create a uniform buffer from the bone transforms.
			ShaderData.UpdateBoneData();

			// Try to use the GPU skinning cache if possible
			if (bGPUSkinCacheEnabled && ChunkIdx < MAX_GPUSKINCACHE_CHUNKS_PER_LOD && !bClothFactory && Chunk.MaxBoneInfluences > 0 && DynamicData->NumWeightedActiveVertexAnims <= 0)
			{
				int32 Key = GGPUSkinCache.StartCacheMesh(GPUSkinCacheKeys[ChunkIdx], &VertexFactoryData.VertexFactories[ChunkIdx], &VertexFactoryData.PassthroughVertexFactories[ChunkIdx], Chunk, this, Chunk.HasExtraBoneInfluences());
				if(Key >= 0)
				{
					GPUSkinCacheKeys[ChunkIdx] = (int16)Key;
				}
			}

#if WITH_APEX_CLOTHING
			// Update uniform buffer for APEX cloth simulation mesh positions and normals
			if( bClothFactory )
			{				
				FGPUBaseSkinAPEXClothVertexFactory::ClothShaderType& ClothShaderData = VertexFactoryData.ClothVertexFactories[ChunkIdx]->GetClothShaderData();
				int16 ActorIdx = Chunk.CorrespondClothAssetIndex;
				if( DynamicData->ClothSimulUpdateData.IsValidIndex(ActorIdx) )
				{
					ClothShaderData.UpdateClothSimulData( DynamicData->ClothSimulUpdateData[ActorIdx].ClothSimulPositions, DynamicData->ClothSimulUpdateData[ActorIdx].ClothSimulNormals);
				}
			}
#endif // WITH_APEX_CLOTHING
		}
	}

	// only update if the morph data changed and there are weighted morph targets
	if( bMorphNeedsUpdate &&
		DynamicData->NumWeightedActiveVertexAnims > 0 )
	{
		// update the morph data for the lod
		LOD.UpdateMorphVertexBuffer( DynamicData->ActiveVertexAnims );
	}
}

void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::UpdateMorphVertexBuffer( const TArray<FActiveVertexAnim>& ActiveVertexAnims )
{
	SCOPE_CYCLE_COUNTER( STAT_MorphVertexBuffer_Update );
	// static variables to initialize vertex buffer, FPackedNormal can't be initialized as 0, so preset arrays to init them
	static FMorphGPUSkinVertex ZeroVertex(	FVector::ZeroVector, FPackedNormal::ZeroNormal	);
	static TArray<FMorphGPUSkinVertex> ZeroVertexArray;

	if( IsValidRef(MorphVertexBuffer.VertexBufferRHI) )
	{
		// LOD of the skel mesh is used to find number of vertices in buffer
		FStaticLODModel& LodModel = SkelMeshResource->LODModels[LODIndex];
		uint32 Size = LodModel.NumVertices * sizeof(FMorphGPUSkinVertex);

#if !WITH_EDITORONLY_DATA
		// Lock the buffer.
		FMorphGPUSkinVertex* Buffer = (FMorphGPUSkinVertex*)RHILockVertexBuffer(MorphVertexBuffer.VertexBufferRHI,0,Size,RLM_WriteOnly);
#else
		FMorphGPUSkinVertex* Buffer = (FMorphGPUSkinVertex*)FMemory::Malloc(Size);
#endif

		{
			SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_Init);
			int32 MaxZeroVertCount = 2048;	//32K of extra memory
			if (ZeroVertexArray.Num() == 0)
			{
				ZeroVertexArray.Init(ZeroVertex, MaxZeroVertCount);
			}
			// zero all deltas (NOTE: DeltaTangentZ is FPackedNormal, so we can't just FMemory::Memzero)
			uint32 VertIndex =0;
			for (; VertIndex + (ZeroVertexArray.Num()-1) < LodModel.NumVertices; VertIndex += ZeroVertexArray.Num())
			{
				FMemory::Memcpy(&Buffer[VertIndex], ZeroVertexArray.GetData(), sizeof(FMorphGPUSkinVertex)*ZeroVertexArray.Num());
			}
			if ( VertIndex < LodModel.NumVertices )
			{
				FMemory::Memcpy(&Buffer[VertIndex], ZeroVertexArray.GetData(), sizeof(FMorphGPUSkinVertex)*(LodModel.NumVertices-VertIndex));
			}
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_ApplyDelta);

			// iterate over all active vertex anims and accumulate their vertex deltas
			for( int32 AnimIdx=0; AnimIdx < ActiveVertexAnims.Num(); AnimIdx++ )
			{
				const FActiveVertexAnim& VertAnim = ActiveVertexAnims[AnimIdx];
				checkSlow(VertAnim.VertAnim != NULL);
				checkSlow(VertAnim.VertAnim->HasDataForLOD(LODIndex));
				checkSlow(VertAnim.Weight >= MinVertexAnimBlendWeight && VertAnim.Weight <= MaxVertexAnimBlendWeight);

				// Allocate temp state
				FVertexAnimEvalStateBase* AnimState = VertAnim.VertAnim->InitEval();

				// Get deltas
				int32 NumDeltas;
				FVertexAnimDelta* Deltas = VertAnim.VertAnim->GetDeltasAtTime(VertAnim.Time, LODIndex, AnimState, NumDeltas);

				float ClampedMorphWeight = FMath::Min(VertAnim.Weight,1.0f);

#if !WITH_EDITORONLY_DATA
				VectorRegister MorphTangentZDelta, TangentZDelta;
#endif
				// iterate over the vertices that this lod model has changed
				for( int32 MorphVertIdx=0; MorphVertIdx < NumDeltas; MorphVertIdx++ )
				{
					const FVertexAnimDelta& MorphVertex = Deltas[MorphVertIdx];
					check(MorphVertex.SourceIdx < LodModel.NumVertices);
					FMorphGPUSkinVertex& DestVertex = Buffer[MorphVertex.SourceIdx];

					if (AnimIdx == 0)
					{
						//if the first morph, use direct assignment and do not blend with what is there (zeros)
						DestVertex.DeltaPosition =  MorphVertex.PositionDelta * VertAnim.Weight;

#if !WITH_EDITORONLY_DATA
						DestVertex.DeltaTangentZ.Vector.Packed = MorphVertex.TangentZDelta.Vector.Packed;
#endif
					}
					else
					{
						DestVertex.DeltaPosition += MorphVertex.PositionDelta * VertAnim.Weight;
#if !WITH_EDITORONLY_DATA
						// vectorized method of below function to avoid humongous LHS
						MorphTangentZDelta = Unpack3(&MorphVertex.TangentZDelta.Vector.Packed);
						TangentZDelta = Unpack3(&DestVertex.DeltaTangentZ.Vector.Packed);
						TangentZDelta = VectorMultiplyAdd(MorphTangentZDelta, VectorLoadFloat1(&ClampedMorphWeight), TangentZDelta);
						Pack3(TangentZDelta, &DestVertex.DeltaTangentZ.Vector.Packed);
#endif
					}
#if WITH_EDITORONLY_DATA
					DestVertex.DeltaTangentZ = (FVector(DestVertex.DeltaTangentZ) + FVector(MorphVertex.TangentZDelta) * ClampedMorphWeight);	
#endif
				}

				VertAnim.VertAnim->TermEval(AnimState);
			}
		}

#if WITH_EDITORONLY_DATA
		// Lock the real buffer.
		FMorphGPUSkinVertex* ActualBuffer = (FMorphGPUSkinVertex*)RHILockVertexBuffer(MorphVertexBuffer.VertexBufferRHI,0,Size,RLM_WriteOnly);
		FMemory::Memcpy(ActualBuffer,Buffer,Size);
		FMemory::Free(Buffer);
#endif
		// Unlock the buffer.
		RHIUnlockVertexBuffer(MorphVertexBuffer.VertexBufferRHI);
		// set update flag
		MorphVertexBuffer.bHasBeenUpdated = true;
	}
}

const FVertexFactory* FSkeletalMeshObjectGPUSkin::GetVertexFactory(int32 LODIndex,int32 ChunkIdx) const
{
	checkSlow( LODs.IsValidIndex(LODIndex) );
	checkSlow( DynamicData );

	const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo[LODIndex];
	const FSkeletalMeshObjectLOD& LOD = LODs[LODIndex];

	// cloth simulation is updated & if this ChunkIdx is for ClothVertexFactory
	if ( DynamicData->ClothSimulUpdateData.Num() > 0 
		&& LOD.GPUSkinVertexFactories.ClothVertexFactories.IsValidIndex(ChunkIdx)  
		&& LOD.GPUSkinVertexFactories.ClothVertexFactories[ChunkIdx] != NULL )
	{
		return LOD.GPUSkinVertexFactories.ClothVertexFactories[ChunkIdx]->GetVertexFactory();
	}
	// use the morph enabled vertex factory if any active morphs are set
	else if( DynamicData->NumWeightedActiveVertexAnims > 0 )
	{
		return &LOD.GPUSkinVertexFactories.MorphVertexFactories[ChunkIdx];
	}
	// use the default gpu skin vertex factory
	else
	{
		// If the GPU skinning cache was used, return the passthrough vertex factory
		if (GGPUSkinCache.IsElementProcessed(GPUSkinCacheKeys[ChunkIdx]))
		{
			return &LOD.GPUSkinVertexFactories.PassthroughVertexFactories[ChunkIdx];
		}
		else
		{
			return &LOD.GPUSkinVertexFactories.VertexFactories[ChunkIdx];
		}
	}
}

/** 
 * Initialize the stream components common to all GPU skin vertex factory types 
 *
 * @param VertexFactoryData - context for setting the vertex factory stream components. commited later
 * @param VertexBuffers - vertex buffers which contains the data and also stride info
 * @param bUseInstancedVertexWeights - use instanced influence weights instead of default weights
 */
template<class VertexFactoryType>
void InitGPUSkinVertexFactoryComponents(typename VertexFactoryType::DataType* VertexFactoryData, 
										const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& VertexBuffers)
{
	typedef TGPUSkinVertexBase<VertexFactoryType::HasExtraBoneInfluences> BaseVertexType;

	// tangents
	VertexFactoryData->TangentBasisComponents[0] = FVertexStreamComponent(
		VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(BaseVertexType,TangentX),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_PackedNormal);
	VertexFactoryData->TangentBasisComponents[1] = FVertexStreamComponent(
		VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(BaseVertexType,TangentZ),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_PackedNormal);
	
	// bone indices
	VertexFactoryData->BoneIndices = FVertexStreamComponent(
		VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(BaseVertexType,InfluenceBones),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_UByte4);
	// bone weights
	VertexFactoryData->BoneWeights = FVertexStreamComponent(
		VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(BaseVertexType,InfluenceWeights),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_UByte4N);
	if (VertexFactoryType::HasExtraBoneInfluences)
	{
		// Extra streams for bone indices & weights
		VertexFactoryData->ExtraBoneIndices = FVertexStreamComponent(
			VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(BaseVertexType,InfluenceBones) + 4,VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_UByte4);
		VertexFactoryData->ExtraBoneWeights = FVertexStreamComponent(
			VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(BaseVertexType,InfluenceWeights) + 4,VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_UByte4N);
	}

	// uvs
	if( !VertexBuffers.VertexBufferGPUSkin->GetUseFullPrecisionUVs() )
	{
		typedef TGPUSkinVertexFloat16Uvs<MAX_TEXCOORDS, VertexFactoryType::HasExtraBoneInfluences> VertexType;
		VertexFactoryData->PositionComponent = FVertexStreamComponent(
			VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(VertexType,Position),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Float3);

		// Add a texture coordinate for each texture coordinate set we have
		for( uint32 UVIndex = 0; UVIndex < VertexBuffers.VertexBufferGPUSkin->GetNumTexCoords(); ++UVIndex )
		{
			VertexFactoryData->TextureCoordinates.Add(FVertexStreamComponent(
				VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(VertexType,UVs) + sizeof(FVector2DHalf) * UVIndex, VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Half2));
		}
	}
	else
	{	
		typedef TGPUSkinVertexFloat32Uvs<MAX_TEXCOORDS, VertexFactoryType::HasExtraBoneInfluences> VertexType;
		VertexFactoryData->PositionComponent = FVertexStreamComponent(
			VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(VertexType,Position),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Float3);

		// Add a texture coordinate for each texture coordinate set we have
		for( uint32 UVIndex = 0; UVIndex < VertexBuffers.VertexBufferGPUSkin->GetNumTexCoords(); ++UVIndex )
		{
			VertexFactoryData->TextureCoordinates.Add(FVertexStreamComponent(
				VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(VertexType,UVs) + sizeof(FVector2D) * UVIndex, VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Float2));
		}
	}

	// Color data may be NULL
	if( VertexBuffers.ColorVertexBuffer != NULL && 
		VertexBuffers.ColorVertexBuffer->IsInitialized() )
	{
		// Color
		VertexFactoryData->ColorComponent = FVertexStreamComponent(
			VertexBuffers.ColorVertexBuffer,STRUCT_OFFSET(FGPUSkinVertexColor,VertexColor),VertexBuffers.ColorVertexBuffer->GetStride(),VET_Color);
	}
}

/** 
 * Initialize the stream components common to all GPU skin vertex factory types 
 *
 * @param VertexFactoryData - context for setting the vertex factory stream components. commited later
 * @param VertexBuffers - vertex buffers which contains the data and also stride info
 * @param bUseInstancedVertexWeights - use instanced influence weights instead of default weights
 */
template<class VertexFactoryType>
void InitMorphVertexFactoryComponents(typename VertexFactoryType::DataType* VertexFactoryData, 
										const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& VertexBuffers)
{
	// delta positions
	VertexFactoryData->DeltaPositionComponent = FVertexStreamComponent(
		VertexBuffers.MorphVertexBuffer,STRUCT_OFFSET(FMorphGPUSkinVertex,DeltaPosition),sizeof(FMorphGPUSkinVertex),VET_Float3);
	// delta normals
	VertexFactoryData->DeltaTangentZComponent = FVertexStreamComponent(
		VertexBuffers.MorphVertexBuffer,STRUCT_OFFSET(FMorphGPUSkinVertex,DeltaTangentZ),sizeof(FMorphGPUSkinVertex),VET_PackedNormal);
}

/** 
 * Initialize the stream components common to all GPU skin vertex factory types 
 *
 * @param VertexFactoryData - context for setting the vertex factory stream components. commited later
 * @param VertexBuffers - vertex buffers which contains the data and also stride info
 * @param bUseInstancedVertexWeights - use instanced influence weights instead of default weights
 */
template<class VertexFactoryType>
void InitAPEXClothVertexFactoryComponents(typename VertexFactoryType::DataType* VertexFactoryData, 
										const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& VertexBuffers)
{
	// barycentric coord for positions
	VertexFactoryData->CoordPositionComponent = FVertexStreamComponent(
		VertexBuffers.APEXClothVertexBuffer,STRUCT_OFFSET(FApexClothPhysToRenderVertData,PositionBaryCoordsAndDist),sizeof(FApexClothPhysToRenderVertData),VET_Float4);
	// barycentric coord for normals
	VertexFactoryData->CoordNormalComponent = FVertexStreamComponent(
		VertexBuffers.APEXClothVertexBuffer,STRUCT_OFFSET(FApexClothPhysToRenderVertData,NormalBaryCoordsAndDist),sizeof(FApexClothPhysToRenderVertData),VET_Float4);
	// barycentric coord for tangents
	VertexFactoryData->CoordTangentComponent = FVertexStreamComponent(
		VertexBuffers.APEXClothVertexBuffer,STRUCT_OFFSET(FApexClothPhysToRenderVertData,TangentBaryCoordsAndDist),sizeof(FApexClothPhysToRenderVertData),VET_Float4);
	// indices for reference physics mesh vertices
	VertexFactoryData->SimulIndicesComponent = FVertexStreamComponent(
		VertexBuffers.APEXClothVertexBuffer,STRUCT_OFFSET(FApexClothPhysToRenderVertData,SimulMeshVertIndices),sizeof(FApexClothPhysToRenderVertData),VET_Short4);
}

/** 
 * Handles transferring data between game/render threads when initializing vertex factory components 
 */
template <class VertexFactoryType>
class TDynamicUpdateVertexFactoryData
{
public:
	TDynamicUpdateVertexFactoryData(
		VertexFactoryType* InVertexFactory,
		const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers)
		:	VertexFactory(InVertexFactory)
		,	VertexBuffers(InVertexBuffers)
	{}
	VertexFactoryType* VertexFactory;
	const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers VertexBuffers;
	
};

ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_DECLARE_TEMPLATE(
InitGPUSkinVertexFactory,VertexFactoryType,
TDynamicUpdateVertexFactoryData<VertexFactoryType>,VertexUpdateData,VertexUpdateData,
{
	typename VertexFactoryType::DataType Data;
	InitGPUSkinVertexFactoryComponents<VertexFactoryType>(&Data,VertexUpdateData.VertexBuffers);
	VertexUpdateData.VertexFactory->SetData(Data);
	VertexUpdateData.VertexFactory->GetShaderData().MeshOrigin = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshOrigin();
	VertexUpdateData.VertexFactory->GetShaderData().MeshExtension = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshExtension();
});

/**
 * Creates a vertex factory entry for the given type and initialize it on the render thread
 */
template <class VertexFactoryTypeBase, class VertexFactoryType>
static void CreateVertexFactory(TIndirectArray<VertexFactoryTypeBase>& VertexFactories,
						 const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers,
						 TArray<FBoneSkinning>& InBoneMatrices
						 )
{
	auto* VertexFactory = new VertexFactoryType(InBoneMatrices);
	VertexFactories.Add(VertexFactory);

	// Setup the update data for enqueue
	TDynamicUpdateVertexFactoryData<VertexFactoryType> VertexUpdateData(VertexFactory,InVertexBuffers);

	// update vertex factory components and sync it
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_CREATE_TEMPLATE(
	InitGPUSkinVertexFactory,VertexFactoryType,
	TDynamicUpdateVertexFactoryData<VertexFactoryType>,VertexUpdateData);

	// init rendering resource	
	BeginInitResource(VertexFactory);
}

ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_DECLARE_TEMPLATE(
InitGPUSkinVertexFactoryMorph,VertexFactoryType,
TDynamicUpdateVertexFactoryData<VertexFactoryType>,VertexUpdateData,VertexUpdateData,
{
	typename VertexFactoryType::DataType Data;
	InitGPUSkinVertexFactoryComponents<VertexFactoryType>(&Data,VertexUpdateData.VertexBuffers);
	InitMorphVertexFactoryComponents<VertexFactoryType>(&Data,VertexUpdateData.VertexBuffers);
	VertexUpdateData.VertexFactory->SetData(Data);
	VertexUpdateData.VertexFactory->GetShaderData().MeshOrigin = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshOrigin();
	VertexUpdateData.VertexFactory->GetShaderData().MeshExtension = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshExtension();
});

/**
 * Creates a vertex factory entry for the given type and initialize it on the render thread
 */
template <class VertexFactoryTypeBase, class VertexFactoryType>
static void CreateVertexFactoryMorph(TIndirectArray<VertexFactoryTypeBase>& VertexFactories,
						 const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers,
						 TArray<FBoneSkinning>& InBoneMatrices
						 )

{
	auto* VertexFactory = new VertexFactoryType(InBoneMatrices);
	VertexFactories.Add(VertexFactory);
					 	
	// Setup the update data for enqueue
	TDynamicUpdateVertexFactoryData<VertexFactoryType> VertexUpdateData(VertexFactory, InVertexBuffers);

	// update vertex factory components and sync it
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_CREATE_TEMPLATE(
	InitGPUSkinVertexFactoryMorph,VertexFactoryType,
	TDynamicUpdateVertexFactoryData<VertexFactoryType>,VertexUpdateData);

	// init rendering resource	
	BeginInitResource(VertexFactory);
}

// APEX cloth
ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_DECLARE_TEMPLATE(
InitGPUSkinAPEXClothVertexFactory,VertexFactoryType,
TDynamicUpdateVertexFactoryData<VertexFactoryType>,VertexUpdateData,VertexUpdateData,
{
	typename VertexFactoryType::DataType Data;
	InitGPUSkinVertexFactoryComponents<VertexFactoryType>(&Data,VertexUpdateData.VertexBuffers);
	InitAPEXClothVertexFactoryComponents<VertexFactoryType>(&Data,VertexUpdateData.VertexBuffers);
	VertexUpdateData.VertexFactory->SetData(Data);
	VertexUpdateData.VertexFactory->GetShaderData().MeshOrigin = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshOrigin();
	VertexUpdateData.VertexFactory->GetShaderData().MeshExtension = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshExtension();
});

/**
 * Creates a vertex factory entry for the given type and initialize it on the render thread
 */
template <class VertexFactoryTypeBase, class VertexFactoryType>
static void CreateVertexFactoryCloth(TArray<VertexFactoryTypeBase*>& VertexFactories,
						 const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers,
						 TArray<FBoneSkinning>& InBoneMatrices
						 )

{
	VertexFactoryType* VertexFactory = new VertexFactoryType(InBoneMatrices);
	VertexFactories.Add(VertexFactory);
					 	
	// Setup the update data for enqueue
	TDynamicUpdateVertexFactoryData<VertexFactoryType> VertexUpdateData(VertexFactory, InVertexBuffers);

	// update vertex factory components and sync it
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_CREATE_TEMPLATE(
	InitGPUSkinAPEXClothVertexFactory,VertexFactoryType,
	TDynamicUpdateVertexFactoryData<VertexFactoryType>,VertexUpdateData);

	// init rendering resource	
	BeginInitResource(VertexFactory);
}

/**
 * Determine the current vertex buffers valid for the current LOD
 *
 * @param OutVertexBuffers output vertex buffers
 */
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::GetVertexBuffers(
	FVertexFactoryBuffers& OutVertexBuffers,
	FStaticLODModel& LODModel,
	const FSkelMeshObjectLODInfo& MeshLODInfo)
{
	OutVertexBuffers.VertexBufferGPUSkin = &LODModel.VertexBufferGPUSkin;
	OutVertexBuffers.ColorVertexBuffer = &LODModel.ColorVertexBuffer;
	OutVertexBuffers.MorphVertexBuffer = &MorphVertexBuffer;
	OutVertexBuffers.APEXClothVertexBuffer = &LODModel.APEXClothVertexBuffer;
}

/**
 * Init one array of matrices for each chunk (shared across vertex factory types)
 *
 * @param Chunks - relevant chunk information (either original or from swapped influence)
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitPerChunkBoneMatrices(const TArray<FSkelMeshChunk>& Chunks)
{
	checkSlow(!IsInActualRenderingThread());

	// one array of matrices for each chunk (shared across vertex factory types)
	if (PerChunkBoneMatricesArray.Num() != Chunks.Num())
	{
		PerChunkBoneMatricesArray.Empty(Chunks.Num());
		PerChunkBoneMatricesArray.AddZeroed(Chunks.Num());
	}
}

/** 
 * Init vertex factory resources for this LOD 
 *
 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
 * @param Chunks - relevant chunk information (either original or from swapped influence)
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitVertexFactories(
	const FVertexFactoryBuffers& VertexBuffers, 
	const TArray<FSkelMeshChunk>& Chunks)
{
	// one array of matrices for each chunk (shared across vertex factory types)
	InitPerChunkBoneMatrices(Chunks);

	// first clear existing factories (resources assumed to have been released already)
	// then [re]create the factories

	VertexFactories.Empty(Chunks.Num());
	{
		for(int32 FactoryIdx = 0; FactoryIdx < Chunks.Num(); ++FactoryIdx)
		{
			if (VertexBuffers.VertexBufferGPUSkin->HasExtraBoneInfluences())
			{
				CreateVertexFactory< FGPUBaseSkinVertexFactory, TGPUSkinVertexFactory<true> >(VertexFactories,VertexBuffers,PerChunkBoneMatricesArray[FactoryIdx]);
				CreateVertexFactory< FGPUBaseSkinVertexFactory, TGPUSkinPassthroughVertexFactory<true> >(PassthroughVertexFactories,VertexBuffers,PerChunkBoneMatricesArray[FactoryIdx]);
			}
			else
			{
				CreateVertexFactory< FGPUBaseSkinVertexFactory, TGPUSkinVertexFactory<false> >(VertexFactories,VertexBuffers,PerChunkBoneMatricesArray[FactoryIdx]);
				CreateVertexFactory< FGPUBaseSkinVertexFactory, TGPUSkinPassthroughVertexFactory<false> >(PassthroughVertexFactories,VertexBuffers,PerChunkBoneMatricesArray[FactoryIdx]);
			}
		}
	}
}

/** 
 * Release vertex factory resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::ReleaseVertexFactories()
{
	// Default factories
	for( int32 FactoryIdx=0; FactoryIdx < VertexFactories.Num(); FactoryIdx++)
	{
		BeginReleaseResource(&VertexFactories[FactoryIdx]);
	}

	for (int32 FactoryIdx = 0; FactoryIdx < PassthroughVertexFactories.Num(); FactoryIdx++)
	{
		BeginReleaseResource(&PassthroughVertexFactories[FactoryIdx]);
	}
}

void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitMorphVertexFactories(
	const FVertexFactoryBuffers& VertexBuffers, 
	const TArray<FSkelMeshChunk>& Chunks,
	bool bInUsePerBoneMotionBlur)
{
	// one array of matrices for each chunk (shared across vertex factory types)
	InitPerChunkBoneMatrices(Chunks);
	// clear existing factories (resources assumed to have been released already)
	MorphVertexFactories.Empty(Chunks.Num());
	for( int32 FactoryIdx=0; FactoryIdx < Chunks.Num(); FactoryIdx++ )
	{
		if (VertexBuffers.VertexBufferGPUSkin->HasExtraBoneInfluences())
		{
			CreateVertexFactoryMorph<FGPUBaseSkinVertexFactory, TGPUSkinMorphVertexFactory<true> >(MorphVertexFactories,VertexBuffers,PerChunkBoneMatricesArray[FactoryIdx]);
		}
		else
		{
			CreateVertexFactoryMorph<FGPUBaseSkinVertexFactory, TGPUSkinMorphVertexFactory<false> >(MorphVertexFactories,VertexBuffers,PerChunkBoneMatricesArray[FactoryIdx]);
		}
	}
}

/** 
 * Release morph vertex factory resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::ReleaseMorphVertexFactories()
{
	// Default morph factories
	for( int32 FactoryIdx=0; FactoryIdx < MorphVertexFactories.Num(); FactoryIdx++ )
	{
		auto& MorphVertexFactory = MorphVertexFactories[FactoryIdx];
		BeginReleaseResource(&MorphVertexFactory);
	}
}

void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitAPEXClothVertexFactories(
	const FVertexFactoryBuffers& VertexBuffers, 
	const TArray<FSkelMeshChunk>& Chunks)
{
	// one array of matrices for each chunk (shared across vertex factory types)
	InitPerChunkBoneMatrices(Chunks);

	// clear existing factories (resources assumed to have been released already)
	ClothVertexFactories.Empty(Chunks.Num());
	for( int32 FactoryIdx=0; FactoryIdx < Chunks.Num(); FactoryIdx++ )
	{
		if(Chunks[FactoryIdx].HasApexClothData() && GRHIFeatureLevel >= ERHIFeatureLevel::SM3)
		{
			if (VertexBuffers.VertexBufferGPUSkin->HasExtraBoneInfluences())
			{
				CreateVertexFactoryCloth<FGPUBaseSkinAPEXClothVertexFactory, TGPUSkinAPEXClothVertexFactory<true> >(ClothVertexFactories,VertexBuffers,PerChunkBoneMatricesArray[FactoryIdx]);
			}
			else
			{
				CreateVertexFactoryCloth<FGPUBaseSkinAPEXClothVertexFactory, TGPUSkinAPEXClothVertexFactory<false> >(ClothVertexFactories,VertexBuffers,PerChunkBoneMatricesArray[FactoryIdx]);
			}
		}
		else
		{
			ClothVertexFactories.Add(NULL);
		}
	}
}

/** 
 * Release APEX cloth vertex factory resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::ReleaseAPEXClothVertexFactories()
{
	// Default APEX cloth factories
	for( int32 FactoryIdx=0; FactoryIdx < ClothVertexFactories.Num(); FactoryIdx++ )
	{
		auto* ClothVertexFactory = ClothVertexFactories[FactoryIdx];
		if (ClothVertexFactory)
		{
			BeginReleaseResource(ClothVertexFactory->GetVertexFactory());
		}
	}
}
/** 
 * Init rendering resources for this LOD 
 * @param bUseLocalVertexFactory - use non-gpu skinned factory when rendering in ref pose
 * @param MeshLODInfo - information about the state of the bone influence swapping
 * @param Chunks - relevant chunk information (either original or from swapped influence)
 */
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::InitResources(const FSkelMeshObjectLODInfo& MeshLODInfo)
{
	check(SkelMeshResource);
	check(SkelMeshResource->LODModels.IsValidIndex(LODIndex));

	// vertex buffer for each lod has already been created when skelmesh was loaded
	FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIndex];
	
	// Vertex buffers available for the LOD
	FVertexFactoryBuffers VertexBuffers;
	GetVertexBuffers(VertexBuffers,LODModel,MeshLODInfo);

	// init gpu skin factories
	GPUSkinVertexFactories.InitVertexFactories(VertexBuffers,LODModel.Chunks);
	if ( LODModel.HasApexClothData() )
	{
		GPUSkinVertexFactories.InitAPEXClothVertexFactories(VertexBuffers,LODModel.Chunks);
	}
}

/** 
 * Release rendering resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::ReleaseResources()
{	
	// Release gpu skin vertex factories
	GPUSkinVertexFactories.ReleaseVertexFactories();

	// Release APEX cloth vertex factory
	GPUSkinVertexFactories.ReleaseAPEXClothVertexFactories();
}

void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::InitMorphResources(const FSkelMeshObjectLODInfo& MeshLODInfo, bool bInUsePerBoneMotionBlur)
{
	check(SkelMeshResource);
	check(SkelMeshResource->LODModels.IsValidIndex(LODIndex));

	// vertex buffer for each lod has already been created when skelmesh was loaded
	FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIndex];

	// init the delta vertex buffer for this LOD
	BeginInitResource(&MorphVertexBuffer);

	// Vertex buffers available for the LOD
	FVertexFactoryBuffers VertexBuffers;
	GetVertexBuffers(VertexBuffers,LODModel,MeshLODInfo);
	// init morph skin factories
	GPUSkinVertexFactories.InitMorphVertexFactories(VertexBuffers,LODModel.Chunks,bInUsePerBoneMotionBlur);
}

/** 
* Release rendering resources for the morph stream of this LOD
*/
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::ReleaseMorphResources()
{
	// Release morph vertex factories
	GPUSkinVertexFactories.ReleaseMorphVertexFactories();
	// release the delta vertex buffer
	BeginReleaseResource(&MorphVertexBuffer);
}


TArray<FTransform>* FSkeletalMeshObjectGPUSkin::GetSpaceBases() const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(DynamicData)
	{
		return &(DynamicData->MeshSpaceBases);
	}
	else
#endif
	{
		return NULL;
	}
}

const FTwoVectors& FSkeletalMeshObjectGPUSkin::GetCustomLeftRightVectors(int32 SectionIndex) const
{
	if( DynamicData && DynamicData->CustomLeftRightVectors.IsValidIndex(SectionIndex) )
	{
		return DynamicData->CustomLeftRightVectors[SectionIndex];
	}
	else
	{
		static FTwoVectors Bad( FVector::ZeroVector, FVector(1.f,0.f,0.f) );
		return Bad;
	}
}

/*-----------------------------------------------------------------------------
FDynamicSkelMeshObjectDataGPUSkin
-----------------------------------------------------------------------------*/

FDynamicSkelMeshObjectDataGPUSkin::FDynamicSkelMeshObjectDataGPUSkin(
	USkinnedMeshComponent* InMeshComponent,
	FSkeletalMeshResource* InSkeletalMeshResource,
	int32 InLODIndex,
	const TArray<FActiveVertexAnim>& InActiveVertexAnims
	)
	:	LODIndex(InLODIndex)
	,	ActiveVertexAnims(InActiveVertexAnims)
	,	NumWeightedActiveVertexAnims(0)
{
	// update ReferenceToLocal
	UpdateRefToLocalMatrices( ReferenceToLocal, InMeshComponent, InSkeletalMeshResource, LODIndex );

	UpdateCustomLeftRightVectors( CustomLeftRightVectors, InMeshComponent, InSkeletalMeshResource, LODIndex );


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	MeshSpaceBases = InMeshComponent->SpaceBases;
#endif

	// find number of morphs that are currently weighted and will affect the mesh
	for( int32 AnimIdx=ActiveVertexAnims.Num()-1; AnimIdx >= 0; AnimIdx-- )
	{
		const FActiveVertexAnim& Anim = ActiveVertexAnims[AnimIdx];
		if( Anim.VertAnim != NULL && 
			Anim.Weight >= MinVertexAnimBlendWeight &&
			Anim.Weight <= MaxVertexAnimBlendWeight &&
			Anim.VertAnim->HasDataForLOD(LODIndex) ) 
		{
			NumWeightedActiveVertexAnims++;
		}
		else
		{
			ActiveVertexAnims.RemoveAt(AnimIdx);
		}
	}

	// Update the clothing simulation mesh positions and normals
	UpdateClothSimulationData(InMeshComponent);
}

bool FDynamicSkelMeshObjectDataGPUSkin::ActiveVertexAnimsEqual( const TArray<FActiveVertexAnim>& CompareActiveVertexAnims )
{
	bool Result=true;
	if( CompareActiveVertexAnims.Num() == ActiveVertexAnims.Num() )
	{
		const float WeightThreshold = 0.001f;
		const float TimeThreshold = 0.001f;
		for( int32 MorphIdx=0; MorphIdx < ActiveVertexAnims.Num(); MorphIdx++ )
		{
			const FActiveVertexAnim& VertAnim = ActiveVertexAnims[MorphIdx];
			const FActiveVertexAnim& CompVertAnim = CompareActiveVertexAnims[MorphIdx];

			if( VertAnim.VertAnim != CompVertAnim.VertAnim ||
				FMath::Abs(VertAnim.Weight - CompVertAnim.Weight) >= WeightThreshold ||
				FMath::Abs(VertAnim.Time - CompVertAnim.Time) >= TimeThreshold )
			{
				Result=false;
				break;
			}
		}
	}
	else
	{
		Result = false;
	}
	return Result;
}

bool FDynamicSkelMeshObjectDataGPUSkin::UpdateClothSimulationData(USkinnedMeshComponent* InMeshComponent)
{
	USkeletalMeshComponent * SkelMeshComponent = Cast<USkeletalMeshComponent>(InMeshComponent);
	if(SkelMeshComponent)
	{
		SkelMeshComponent->GetUpdateClothSimulationData(ClothSimulUpdateData);
		return true;
	}
	return false;
}
/*-----------------------------------------------------------------------------
FPreviousPerBoneMotionBlur
-----------------------------------------------------------------------------*/

FPreviousPerBoneMotionBlur::FPreviousPerBoneMotionBlur()
	:BufferIndex(0), LockedData(0), LockedTexelPosition(0), LockedTexelCount(0), bWarningBufferSizeExceeded(false)
{
}


void FPreviousPerBoneMotionBlur::InitResources()
{
	if (GRHIFeatureLevel >= ERHIFeatureLevel::SM4)
	{
		for(uint32 i = 0; i < PER_BONE_BUFFER_COUNT; ++i)
		{
			PerChunkBoneMatricesTexture[i].InitResource();	
		}	
	}
}

void FPreviousPerBoneMotionBlur::ReleaseResources()
{
	checkSlow(IsInRenderingThread());

	for(int32 i = 0; i < PER_BONE_BUFFER_COUNT; ++i)
	{
		PerChunkBoneMatricesTexture[i].ReleaseResource();
	}
}

void FPreviousPerBoneMotionBlur::RestoreForPausedMotionBlur()
{
	if(BufferIndex == 0)
	{
		BufferIndex = PER_BONE_BUFFER_COUNT - 1;
	}
	else
	{
		--BufferIndex;
	}
}

uint32 FPreviousPerBoneMotionBlur::GetSizeX() const
{
	return PerChunkBoneMatricesTexture[0].GetSizeX();
}

bool FPreviousPerBoneMotionBlur::IsLocked() const
{
	return LockedData != 0;
}

void FPreviousPerBoneMotionBlur::InitIfNeeded()
{
	if(!PerChunkBoneMatricesTexture[0].IsInitialized())
	{
		InitResources();
	}
}
void FPreviousPerBoneMotionBlur::LockData()
{
	checkSlow(!LockedData);
	checkSlow(IsInRenderingThread());

	InitIfNeeded();

	FBoneDataVertexBuffer& WriteTexture = PerChunkBoneMatricesTexture[GetWriteBufferIndex()];

	if(WriteTexture.IsValid())
	{
		LockedData = WriteTexture.LockData();
		LockedTexelPosition = 0;
		LockedTexelCount = WriteTexture.GetSizeX();
	}
}

uint32 FPreviousPerBoneMotionBlur::AppendData(FBoneSkinning *DataStart, uint32 BoneCount)
{
	checkSlow(LockedData);
	checkSlow(DataStart);
	checkSlow(BoneCount);

	uint32 TexelCount = BoneCount * sizeof(FBoneSkinning) / sizeof(float) / 4;

	uint32 OldLockedTexelPosition = LockedTexelPosition;
	
	LockedTexelPosition += TexelCount;

	if(LockedTexelPosition <= LockedTexelCount)
	{
		FMemory::Memcpy(&LockedData[OldLockedTexelPosition * 4], DataStart, BoneCount * sizeof(FBoneSkinning));

		return OldLockedTexelPosition;
	}
	else
	{
		// Not enough space in the texture, we should increase the texture size. The new bigger size 
		// can be found in LockedTexelPosition. This is currently not done - so we might not see motion blur
		// skinning on all objects.
		bWarningBufferSizeExceeded = true;
		return 0xffffffff;
	}
}

void FPreviousPerBoneMotionBlur::UnlockData(bool bAdvance)
{
	if(IsLocked())
	{
		LockedTexelPosition = 0;
		LockedTexelCount = 0;
		LockedData = 0;

		PerChunkBoneMatricesTexture[GetWriteBufferIndex()].UnlockData();

		if(bAdvance) 
		{
			AdvanceBufferIndex();
		}
	}
	
	{
		static int LogSpawmPrevent = 0;

		if(bWarningBufferSizeExceeded)
		{
			bWarningBufferSizeExceeded = false;

			if((LogSpawmPrevent % 16) == 0)
			{
				UE_LOG(LogSkeletalGPUSkinMesh, Warning, TEXT("Exceeded buffer for per bone motionblur for skinned mesh veclocity rendering. Artifacts can occur. Change Content, increase buffer size or change to use FGlobalDynamicVertexBuffer."));
			}
			++LogSpawmPrevent;
		}
		else
		{
			LogSpawmPrevent = 0;
		}
	}
}

FBoneDataVertexBuffer* FPreviousPerBoneMotionBlur::GetReadData()
{
	return &PerChunkBoneMatricesTexture[GetReadBufferIndex()];
}

uint32 FPreviousPerBoneMotionBlur::GetReadBufferIndex() const
{
	return BufferIndex;
}

uint32 FPreviousPerBoneMotionBlur::GetWriteBufferIndex() const
{
	uint32 ret = BufferIndex + 1;

	if(ret >= PER_BONE_BUFFER_COUNT)
	{
		ret = 0;
	}
	return ret;
}

void FPreviousPerBoneMotionBlur::AdvanceBufferIndex()
{
	++BufferIndex;

	if(BufferIndex >= PER_BONE_BUFFER_COUNT)
	{
		BufferIndex = 0;
	}
}

/** 
 *	Function to free up the resources in GPrevPerBoneMotionBlur
 *	Should only be called at application exit
 */
ENGINE_API void MotionBlur_Free()
{
	ENQUEUE_UNIQUE_RENDER_COMMAND(
		MotionBlurFree,
	{
		GPrevPerBoneMotionBlur.ReleaseResources();
	}
	);		
}
