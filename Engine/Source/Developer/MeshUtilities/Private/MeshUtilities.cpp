// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "MeshUtilitiesPrivate.h"
#include "SkeletalMeshTypes.h"
#include "Landscape/LandscapeRender.h"
#include "MeshBuild.h"
#include "TessellationRendering.h"
#include "NVTriStrip.h"
#include "forsythtriangleorderoptimizer.h"
#include "ThirdParty/nvtesslib/inc/nvtess.h"
#include "SkeletalMeshTools.h"
#include "Landscape/LandscapeDataAccess.h"
#include "ImageUtils.h"
#include "MaterialExportUtils.h"

/*------------------------------------------------------------------------------
	MeshUtilities module.
------------------------------------------------------------------------------*/

// The version string is a GUID. If you make a change to mesh utilities that
// causes meshes to be rebuilt you MUST generate a new GUID and replace this
// string with it.

#define MESH_UTILITIES_VER TEXT("359a029847e84730ba516769d0f19427")

DEFINE_LOG_CATEGORY_STATIC(LogMeshUtilities,Verbose,All);

class FMeshUtilities : public IMeshUtilities
{
public:
	/** Default constructor. */
	FMeshUtilities()
		: MeshReduction(NULL)
		, MeshMerging(NULL)
	{
	}

private:
	/** Cached pointer to the mesh reduction interface. */
	IMeshReduction* MeshReduction;
	/** Cached pointer to the mesh merging interface. */
	IMeshMerging* MeshMerging;
	/** Cached version string. */
	FString VersionString;
	/** True if Simplygon is being used for mesh reduction. */
	bool bUsingSimplygon;
	/** True if NvTriStrip is being used for tri order optimization. */
	bool bUsingNvTriStrip;
	// IMeshUtilities interface.
	virtual const FString& GetVersionString() const OVERRIDE
	{
		return VersionString;
	}
	virtual bool BuildStaticMesh(
		FStaticMeshRenderData& OutRenderData,
		TArray<FStaticMeshSourceModel>& SourceModels,
		const FStaticMeshLODGroup& LODGroup
		) OVERRIDE;

	virtual bool BuildSkeletalMesh( FStaticLODModel& LODModel, const FReferenceSkeleton& RefSkeleton, const TArray<FVertInfluence>& Influences, const TArray<FMeshWedge>& Wedges, const TArray<FMeshFace>& Faces, const TArray<FVector>& Points, const TArray<int32>& PointToOriginalMap, bool bKeepOverlappingVertices = false, bool bComputeNormals = true, bool bComputeTangents = true );

	virtual bool GenerateUVs(
		FRawMesh& RawMesh,
		uint32 TexCoordIndex,
		float MinChartSpacingPercent,
		float BorderSpacingPercent,
		bool bUseMaxStretch,
		const TArray< int32 >* InFalseEdgeIndices,
		uint32& MaxCharts,
		float& MaxDesiredStretch,
		FText& OutError
		) OVERRIDE;

	virtual bool LayoutUVs(FRawMesh& RawMesh, uint32 TextureResolution, uint32 TexCoordIndex, FText& OutError) OVERRIDE;
	virtual IMeshReduction* GetMeshReductionInterface() OVERRIDE;
	virtual IMeshMerging* GetMeshMergingInterface() OVERRIDE;
	virtual void CacheOptimizeIndexBuffer(TArray<uint16>& Indices) OVERRIDE;
	virtual void CacheOptimizeIndexBuffer(TArray<uint32>& Indices) OVERRIDE;
	void CacheOptimizeVertexAndIndexBuffer(TArray<FStaticMeshBuildVertex>& Vertices,TArray<TArray<uint32> >& PerSectionIndices,TArray<int32>& WedgeMap);
	
	virtual void BuildSkeletalAdjacencyIndexBuffer(
		const TArray<FSoftSkinVertex>& VertexBuffer,
		const uint32 TexCoordCount,
		const TArray<uint32>& Indices,
		TArray<uint32>& OutPnAenIndices
		) OVERRIDE;

	virtual void CalcBoneVertInfos(USkeletalMesh* SkeletalMesh, TArray<FBoneVertInfo>& Infos, bool bOnlyDominant) OVERRIDE;

	/**
	 * Builds a renderable skeletal mesh LOD model. Note that the array of chunks
	 * will be destroyed during this process!
	 * @param LODModel				Upon return contains a renderable skeletal mesh LOD model.
	 * @param RefSkeleton			The reference skeleton associated with the model.
	 * @param Chunks				Skinned mesh chunks from which to build the renderable model.
	 * @param PointToOriginalMap	Maps a vertex's RawPointIdx to its index at import time.
	 */
	void BuildSkeletalModelFromChunks(FStaticLODModel& LODModel,const FReferenceSkeleton& RefSkeleton,TArray<FSkinnedMeshChunk*>& Chunks,const TArray<int32>& PointToOriginalMap);

	// IModuleInterface interface.
	virtual void StartupModule() OVERRIDE;
	virtual void ShutdownModule() OVERRIDE;

	virtual void MergeActors(
		const TArray<AActor*>& SourceActors, 
		const FString& PackageName,
		TArray<UObject*>& 
		OutAssetsToSync, 
		FVector& OutMergedActorLocation) const OVERRIDE;
	
	virtual void CreateProxyMesh( 
		const TArray<AActor*>& Actors, 
		const struct FMeshProxySettings& InProxySettings, 
		const FString& ProxyPackageName,
		TArray<UObject*>& OutAssetsToSync,
		FVector& OutProxyLocation
		) OVERRIDE;
	
	bool ConstructRawMesh(
		UStaticMeshComponent* MeshComponent, 
		FRawMesh& OutRawMesh, 
		TArray<UMaterialInterface*>& OutUniqueMaterials,
		TArray<int32>& OutGlobalMaterialIndices
		) const;
};

IMPLEMENT_MODULE(FMeshUtilities, MeshUtilities);

/*------------------------------------------------------------------------------
	NVTriStrip for cache optimizing index buffers.
------------------------------------------------------------------------------*/

namespace NvTriStrip
{
	/**
	 * Converts 16 bit indices to 32 bit prior to passing them into the real GenerateStrips util method
	 */
	void GenerateStrips(
		const uint8* Indices,
		bool Is32Bit,
		const uint32 NumIndices,
		PrimitiveGroup** PrimGroups,
		uint32* NumGroups
		)
	{
		if (Is32Bit)
		{
			GenerateStrips((uint32*)Indices, NumIndices, PrimGroups, NumGroups);
		}
		else
		{
			// convert to 32 bit
			uint32 Idx;
			TArray<uint32> NewIndices;
			NewIndices.AddUninitialized(NumIndices);
			for (Idx = 0; Idx < NumIndices; ++Idx)
			{
				NewIndices[Idx] = ((uint16*)Indices)[Idx];
			}
			GenerateStrips(NewIndices.GetData(), NumIndices, PrimGroups, NumGroups);
		}

	}

	/**
	 * Orders a triangle list for better vertex cache coherency.
	 *
	 * *** WARNING: This is safe to call for multiple threads IF AND ONLY IF all
	 * threads call SetListsOnly(true) and SetCacheSize(CACHESIZE_GEFORCE3). If
	 * NvTriStrip is ever used with different settings the library will need
	 * some modifications to be thread-safe. ***
	 */
	template<typename IndexDataType, typename Allocator>
	void CacheOptimizeIndexBuffer(TArray<IndexDataType,Allocator>& Indices)
	{
		checkAtCompileTime(sizeof(IndexDataType) == 2 || sizeof(IndexDataType) == 4, IndicesMustBeShortOrInt);

		PrimitiveGroup*	PrimitiveGroups = NULL;
		uint32			NumPrimitiveGroups = 0;
		bool Is32Bit = sizeof(IndexDataType) == 4;

		SetListsOnly(true);
		SetCacheSize(CACHESIZE_GEFORCE3);

		GenerateStrips((uint8*)Indices.GetTypedData(),Is32Bit,Indices.Num(),&PrimitiveGroups,&NumPrimitiveGroups);

		Indices.Empty();
		Indices.AddUninitialized(PrimitiveGroups->numIndices);
	
		if( Is32Bit )
		{
			FMemory::Memcpy(Indices.GetTypedData(),PrimitiveGroups->indices,Indices.Num() * sizeof(IndexDataType));
		}
		else
		{
			for( uint32 I = 0; I < PrimitiveGroups->numIndices; ++I )
			{
				Indices[I] = (uint16)PrimitiveGroups->indices[I];
			}
		}

		delete [] PrimitiveGroups;
	}
}

/*------------------------------------------------------------------------------
	Forsyth algorithm for cache optimizing index buffers.
------------------------------------------------------------------------------*/

namespace Forsyth
{
	/**
	 * Converts 16 bit indices to 32 bit prior to passing them into the real OptimizeFaces util method
	 */
	void OptimizeFaces(
		const uint8* Indices,
		bool Is32Bit,
		const uint32 NumIndices,
		uint32 NumVertices,
		uint32* OutIndices,
		uint16 CacheSize
		)
	{
		if (Is32Bit)
		{
			OptimizeFaces((uint32*)Indices, NumIndices, NumVertices, OutIndices, CacheSize);
		}
		else
		{
			// convert to 32 bit
			uint32 Idx;
			TArray<uint32> NewIndices;
			NewIndices.AddUninitialized(NumIndices);
			for (Idx = 0; Idx < NumIndices; ++Idx)
			{
				NewIndices[Idx] = ((uint16*)Indices)[Idx];
			}
			OptimizeFaces(NewIndices.GetData(),NumIndices, NumVertices, OutIndices, CacheSize);
		}

	}

	/**
	 * Orders a triangle list for better vertex cache coherency.
	 */
	template<typename IndexDataType, typename Allocator>
	void CacheOptimizeIndexBuffer(TArray<IndexDataType,Allocator>& Indices)
	{
		checkAtCompileTime(sizeof(IndexDataType) == 2 || sizeof(IndexDataType) == 4, IndicesMustBeShortOrInt);
		bool Is32Bit = sizeof(IndexDataType) == 4;

		// Count the number of vertices
		uint32 NumVertices = 0;
		for(int32 Index = 0; Index < Indices.Num(); ++Index)
		{
			if(Indices[Index] > NumVertices)
			{
				NumVertices = Indices[Index];
			}
		}
		NumVertices += 1;

		TArray<uint32> OptimizedIndices;
		OptimizedIndices.AddUninitialized(Indices.Num());
		uint16 CacheSize = 32;
		OptimizeFaces((uint8*)Indices.GetData(),Is32Bit,Indices.Num(),NumVertices, OptimizedIndices.GetData(), CacheSize);

		if( Is32Bit )
		{
			FMemory::Memcpy(Indices.GetTypedData(),OptimizedIndices.GetTypedData(),Indices.Num() * sizeof(IndexDataType));
		}
		else
		{
			for( int32 I = 0; I < OptimizedIndices.Num(); ++I )
			{
				Indices[I] = (uint16)OptimizedIndices[I];
			}
		}
	}
}

void FMeshUtilities::CacheOptimizeIndexBuffer(TArray<uint16>& Indices)
{
	if(bUsingNvTriStrip)
	{
		NvTriStrip::CacheOptimizeIndexBuffer(Indices);
	}
	else
	{
		Forsyth::CacheOptimizeIndexBuffer(Indices);
	}
}

void FMeshUtilities::CacheOptimizeIndexBuffer(TArray<uint32>& Indices)
{
	if(bUsingNvTriStrip)
	{
		NvTriStrip::CacheOptimizeIndexBuffer(Indices);
	}
	else
	{
		Forsyth::CacheOptimizeIndexBuffer(Indices);
	}
}

/*------------------------------------------------------------------------------
	NVTessLib for computing adjacency used for tessellation.
------------------------------------------------------------------------------*/

/**
 * Provides static mesh render data to the NVIDIA tessellation library.
 */
class FStaticMeshNvRenderBuffer : public nv::RenderBuffer
{
public:

	/** Construct from static mesh render buffers. */
	FStaticMeshNvRenderBuffer(
		const FPositionVertexBuffer& InPositionVertexBuffer,
		const FStaticMeshVertexBuffer& InVertexBuffer,
		const TArray<uint32>& Indices )
		: PositionVertexBuffer( InPositionVertexBuffer )
		, VertexBuffer( InVertexBuffer )
	{
		check( PositionVertexBuffer.GetNumVertices() == VertexBuffer.GetNumVertices() );
		mIb = new nv::IndexBuffer( (void*)Indices.GetTypedData(), nv::IBT_U32, Indices.Num(), false );
	}

	/** Retrieve the position and first texture coordinate of the specified index. */
	virtual nv::Vertex getVertex( unsigned int Index ) const
	{
		nv::Vertex Vertex;

		check( Index < PositionVertexBuffer.GetNumVertices() );

		const FVector& Position = PositionVertexBuffer.VertexPosition( Index );
		Vertex.pos.x = Position.X;
		Vertex.pos.y = Position.Y;
		Vertex.pos.z = Position.Z;

		if( VertexBuffer.GetNumTexCoords() )
		{
			const FVector2D UV = VertexBuffer.GetVertexUV( Index, 0 );
			Vertex.uv.x = UV.X;
			Vertex.uv.y = UV.Y;
		}
		else
		{
			Vertex.uv.x = 0.0f;
			Vertex.uv.y = 0.0f;
		}

		return Vertex;
	}

private:

	/** The position vertex buffer for the static mesh. */
	const FPositionVertexBuffer& PositionVertexBuffer;

	/** The vertex buffer for the static mesh. */
	const FStaticMeshVertexBuffer& VertexBuffer;

	/** Copying is forbidden. */
	FStaticMeshNvRenderBuffer( const FStaticMeshNvRenderBuffer& ); 
	FStaticMeshNvRenderBuffer& operator=( const FStaticMeshNvRenderBuffer& );
};

/**
 * Provides skeletal mesh render data to the NVIDIA tessellation library.
 */
class FSkeletalMeshNvRenderBuffer : public nv::RenderBuffer
{
public:

	/** Construct from static mesh render buffers. */
	FSkeletalMeshNvRenderBuffer( 
		const TArray<FSoftSkinVertex>& InVertexBuffer,
		const uint32 InTexCoordCount,
		const TArray<uint32>& Indices )
		: VertexBuffer( InVertexBuffer )
		, TexCoordCount( InTexCoordCount )
	{
		mIb = new nv::IndexBuffer( (void*)Indices.GetTypedData(), nv::IBT_U32, Indices.Num(), false );
	}

	/** Retrieve the position and first texture coordinate of the specified index. */
	virtual nv::Vertex getVertex( unsigned int Index ) const
	{
		nv::Vertex Vertex;

		check( Index < (unsigned int)VertexBuffer.Num() );

		const FSoftSkinVertex& SrcVertex = VertexBuffer[ Index ];

		Vertex.pos.x = SrcVertex.Position.X;
		Vertex.pos.y = SrcVertex.Position.Y;
		Vertex.pos.z = SrcVertex.Position.Z;

		if( TexCoordCount > 0 )
		{
			Vertex.uv.x = SrcVertex.UVs[0].X;
			Vertex.uv.y = SrcVertex.UVs[0].Y;
		}
		else
		{
			Vertex.uv.x = 0.0f;
			Vertex.uv.y = 0.0f;
		}

		return Vertex;
	}

private:
	/** The vertex buffer for the skeletal mesh. */
	const TArray<FSoftSkinVertex>& VertexBuffer;
	const uint32 TexCoordCount;

	/** Copying is forbidden. */
	FSkeletalMeshNvRenderBuffer(const FSkeletalMeshNvRenderBuffer&); 
	FSkeletalMeshNvRenderBuffer& operator=(const FSkeletalMeshNvRenderBuffer&);
};

static void BuildStaticAdjacencyIndexBuffer(
	const FPositionVertexBuffer& PositionVertexBuffer,
	const FStaticMeshVertexBuffer& VertexBuffer,
	const TArray<uint32>& Indices,
	TArray<uint32>& OutPnAenIndices
	)
{
	if ( Indices.Num() )
	{
		FStaticMeshNvRenderBuffer StaticMeshRenderBuffer( PositionVertexBuffer, VertexBuffer, Indices );
		nv::IndexBuffer* PnAENIndexBuffer = nv::tess::buildTessellationBuffer( &StaticMeshRenderBuffer, nv::DBM_PnAenDominantCorner, true );
		check( PnAENIndexBuffer );
		const int32 IndexCount = (int32)PnAENIndexBuffer->getLength();
		OutPnAenIndices.Empty( IndexCount );
		OutPnAenIndices.AddUninitialized( IndexCount );
		for ( int32 Index = 0; Index < IndexCount; ++Index )
		{
			OutPnAenIndices[ Index ] = (*PnAENIndexBuffer)[Index];
		}
		delete PnAENIndexBuffer;
	}
	else
	{
		OutPnAenIndices.Empty();
	}
}

void FMeshUtilities::BuildSkeletalAdjacencyIndexBuffer(
	const TArray<FSoftSkinVertex>& VertexBuffer,
	const uint32 TexCoordCount,
	const TArray<uint32>& Indices,
	TArray<uint32>& OutPnAenIndices
	)
{
	if ( Indices.Num() )
	{
		FSkeletalMeshNvRenderBuffer SkeletalMeshRenderBuffer( VertexBuffer, TexCoordCount, Indices );
		nv::IndexBuffer* PnAENIndexBuffer = nv::tess::buildTessellationBuffer( &SkeletalMeshRenderBuffer, nv::DBM_PnAenDominantCorner, true );
		check( PnAENIndexBuffer );
		const int32 IndexCount = (int32)PnAENIndexBuffer->getLength();
		OutPnAenIndices.Empty( IndexCount );
		OutPnAenIndices.AddUninitialized( IndexCount );
		for ( int32 Index = 0; Index < IndexCount; ++Index )
		{
			OutPnAenIndices[ Index ] = (*PnAENIndexBuffer)[Index];
		}
		delete PnAENIndexBuffer;
	}
	else
	{
		OutPnAenIndices.Empty();
	}
}

void FMeshUtilities::CalcBoneVertInfos(USkeletalMesh* SkeletalMesh, TArray<FBoneVertInfo>& Infos, bool bOnlyDominant)
{
	SkeletalMeshTools::CalcBoneVertInfos(SkeletalMesh,Infos,bOnlyDominant);
}

/**
 * Builds a renderable skeletal mesh LOD model. Note that the array of chunks
 * will be destroyed during this process!
 * @param LODModel				Upon return contains a renderable skeletal mesh LOD model.
 * @param RefSkeleton			The reference skeleton associated with the model.
 * @param Chunks				Skinned mesh chunks from which to build the renderable model.
 * @param PointToOriginalMap	Maps a vertex's RawPointIdx to its index at import time.
 */
void FMeshUtilities::BuildSkeletalModelFromChunks(FStaticLODModel& LODModel,const FReferenceSkeleton& RefSkeleton,TArray<FSkinnedMeshChunk*>& Chunks,const TArray<int32>& PointToOriginalMap)
{
#if WITH_EDITORONLY_DATA
	// Clear out any data currently held in the LOD model.
	LODModel.Sections.Empty();
	LODModel.Chunks.Empty();
	LODModel.NumVertices = 0;
	if (LODModel.MultiSizeIndexContainer.IsIndexBufferValid())
	{
		LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Empty();
	}

	// Setup the section and chunk arrays on the model.
	for (int32 ChunkIndex = 0; ChunkIndex < Chunks.Num(); ++ChunkIndex)
	{
		FSkinnedMeshChunk* SrcChunk = Chunks[ChunkIndex];

		FSkelMeshSection& Section = *new(LODModel.Sections) FSkelMeshSection();
		Section.MaterialIndex = SrcChunk->MaterialIndex;
		Section.ChunkIndex = ChunkIndex;

		FSkelMeshChunk& Chunk = *new(LODModel.Chunks) FSkelMeshChunk();
		Exchange(Chunk.BoneMap,SrcChunk->BoneMap);

		// Update the active bone indices on the LOD model.
		for (int32 BoneIndex = 0; BoneIndex < Chunk.BoneMap.Num(); ++BoneIndex)
		{
			LODModel.ActiveBoneIndices.AddUnique(Chunk.BoneMap[BoneIndex]);
		}
	}

	// Reset 'final vertex to import vertex' map info
	LODModel.MeshToImportVertexMap.Empty();
	LODModel.MaxImportVertex = 0;

	// Keep track of index mapping to chunk vertex offsets
	TArray< TArray<uint32> > VertexIndexRemap;
	VertexIndexRemap.Empty(LODModel.Sections.Num());
	// Pack the chunk vertices into a single vertex buffer.
	TArray<uint32> RawPointIndices;	
	LODModel.NumVertices = 0;

	int32 PrevMaterialIndex = -1;
	int32 CurrentChunkBaseVertexIndex = -1; 	// base vertex index for all chunks of the same material
	int32 CurrentChunkVertexCount = -1; 		// total vertex count for all chunks of the same material
	int32 CurrentVertexIndex = 0; 			// current vertex index added to the index buffer for all chunks of the same material

	// rearrange the vert order to minimize the data fetched by the GPU
	for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
	{
		if (IsInGameThread())
		{
			GWarn->StatusUpdate(SectionIndex, LODModel.Sections.Num(), NSLOCTEXT("UnrealEd", "ProcessingSections", "Processing Sections"));
		}

		FSkinnedMeshChunk* SrcChunk = Chunks[SectionIndex];
		FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
		TArray<FSoftSkinBuildVertex>& ChunkVertices = SrcChunk->Vertices;
		TArray<uint32>& ChunkIndices = SrcChunk->Indices;

		// Reorder the section index buffer for better vertex cache efficiency.
		CacheOptimizeIndexBuffer(ChunkIndices);

		// Calculate the number of triangles in the section.  Note that CacheOptimize may change the number of triangles in the index buffer!
		Section.NumTriangles = ChunkIndices.Num() / 3;
		TArray<FSoftSkinBuildVertex> OriginalVertices;
		Exchange(ChunkVertices,OriginalVertices);
		ChunkVertices.AddUninitialized(OriginalVertices.Num());

		TArray<int32> IndexCache;
		IndexCache.AddUninitialized(ChunkVertices.Num());
		FMemory::Memset(IndexCache.GetData(), INDEX_NONE, IndexCache.Num() * IndexCache.GetTypeSize());
		int32 NextAvailableIndex = 0;
		// Go through the indices and assign them new values that are coherent where possible
		for (int32 Index = 0; Index < ChunkIndices.Num(); Index++)
		{
			const int32 OriginalIndex = ChunkIndices[Index];
			const int32 CachedIndex = IndexCache[OriginalIndex];

			if (CachedIndex == INDEX_NONE)
			{
				// No new index has been allocated for this existing index, assign a new one
				ChunkIndices[Index] = NextAvailableIndex;
				// Mark what this index has been assigned to
				IndexCache[OriginalIndex] = NextAvailableIndex;
				NextAvailableIndex++;
			}
			else
			{
				// Reuse an existing index assignment
				ChunkIndices[Index] = CachedIndex;
			}
			// Reorder the vertices based on the new index assignment
			ChunkVertices[ChunkIndices[Index]] = OriginalVertices[OriginalIndex];
		}
	}

	// Build the arrays of rigid and soft vertices on the model's chunks.
	for(int32 SectionIndex = 0;SectionIndex < LODModel.Sections.Num();SectionIndex++)
	{
		FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
		int32 ChunkIndex = Section.ChunkIndex;
		FSkelMeshChunk& Chunk = LODModel.Chunks[ChunkIndex]; 
		TArray<FSoftSkinBuildVertex>& ChunkVertices = Chunks[ChunkIndex]->Vertices;

		if( IsInGameThread() )
		{
			// Only update status if in the game thread.  When importing morph targets, this function can run in another thread
			GWarn->StatusUpdate( ChunkIndex, LODModel.Chunks.Num(), NSLOCTEXT("UnrealEd", "ProcessingChunks", "Processing Chunks") );
		}

		CurrentVertexIndex = 0;
		CurrentChunkVertexCount = 0;
		PrevMaterialIndex = Section.MaterialIndex;

		// Calculate the offset to this chunk's vertices in the vertex buffer.
		Chunk.BaseVertexIndex = CurrentChunkBaseVertexIndex = LODModel.NumVertices;

		// Update the size of the vertex buffer.
		LODModel.NumVertices += ChunkVertices.Num();

		// Separate the section's vertices into rigid and soft vertices.
		TArray<uint32>& ChunkVertexIndexRemap = *new(VertexIndexRemap) TArray<uint32>();
		ChunkVertexIndexRemap.AddUninitialized(ChunkVertices.Num());
		for(int32 VertexIndex = 0;VertexIndex < ChunkVertices.Num();VertexIndex++)
		{
			const FSoftSkinBuildVertex& SoftVertex = ChunkVertices[VertexIndex];
			if(SoftVertex.InfluenceWeights[1] == 0)
			{
				FRigidSkinVertex RigidVertex;
				RigidVertex.Position = SoftVertex.Position;
				RigidVertex.TangentX = SoftVertex.TangentX;
				RigidVertex.TangentY = SoftVertex.TangentY;
				RigidVertex.TangentZ = SoftVertex.TangentZ;
				FMemory::Memcpy( RigidVertex.UVs, SoftVertex.UVs, sizeof(FVector2D)*MAX_TEXCOORDS );
				RigidVertex.Color = SoftVertex.Color;
				RigidVertex.Bone = SoftVertex.InfluenceBones[0];
				Chunk.RigidVertices.Add(RigidVertex);
				ChunkVertexIndexRemap[VertexIndex] = (uint32)(Chunk.BaseVertexIndex + CurrentVertexIndex);
				CurrentVertexIndex++;
				// add the index to the original wedge point source of this vertex
				RawPointIndices.Add( SoftVertex.PointWedgeIdx );
				// Also remember import index
				const int32 RawVertIndex = PointToOriginalMap[SoftVertex.PointWedgeIdx];
				LODModel.MeshToImportVertexMap.Add(RawVertIndex);
				LODModel.MaxImportVertex = FMath::Max<float>(LODModel.MaxImportVertex, RawVertIndex);
			}
		}
		for(int32 VertexIndex = 0;VertexIndex < ChunkVertices.Num();VertexIndex++)
		{
			const FSoftSkinBuildVertex& SoftVertex = ChunkVertices[VertexIndex];
			if(SoftVertex.InfluenceWeights[1] > 0)
			{
				FSoftSkinVertex NewVertex;
				NewVertex.Position = SoftVertex.Position;
				NewVertex.TangentX = SoftVertex.TangentX;
				NewVertex.TangentY = SoftVertex.TangentY;
				NewVertex.TangentZ = SoftVertex.TangentZ;
				FMemory::Memcpy( NewVertex.UVs, SoftVertex.UVs, sizeof(FVector2D)*MAX_TEXCOORDS );
				NewVertex.Color = SoftVertex.Color;
				for (int32 i = 0; i < MAX_TOTAL_INFLUENCES; ++i)
				{
					NewVertex.InfluenceBones[i] = SoftVertex.InfluenceBones[i];
					NewVertex.InfluenceWeights[i] = SoftVertex.InfluenceWeights[i];
				}
				Chunk.SoftVertices.Add(NewVertex);
				ChunkVertexIndexRemap[VertexIndex] = (uint32)(Chunk.BaseVertexIndex + CurrentVertexIndex);
				CurrentVertexIndex++;
				// add the index to the original wedge point source of this vertex
				RawPointIndices.Add( SoftVertex.PointWedgeIdx );
				// Also remember import index
				const int32 RawVertIndex = PointToOriginalMap[SoftVertex.PointWedgeIdx];
				LODModel.MeshToImportVertexMap.Add(RawVertIndex);
				LODModel.MaxImportVertex = FMath::Max<float>(LODModel.MaxImportVertex, RawVertIndex);
			}
		}

		// update total num of verts added
		Chunk.NumRigidVertices = Chunk.RigidVertices.Num();
		Chunk.NumSoftVertices = Chunk.SoftVertices.Num();

		// update max bone influences
		Chunk.CalcMaxBoneInfluences();

		// Log info about the chunk.
		UE_LOG(LogSkeletalMesh, Log, TEXT("Chunk %u: %u rigid vertices, %u soft vertices, %u active bones"),
			ChunkIndex,
			Chunk.RigidVertices.Num(),
			Chunk.SoftVertices.Num(),
			Chunk.BoneMap.Num()
			);
	}

	// Copy raw point indices to LOD model.
	LODModel.RawPointIndices.RemoveBulkData();
	if( RawPointIndices.Num() )
	{
		LODModel.RawPointIndices.Lock(LOCK_READ_WRITE);
		void* Dest = LODModel.RawPointIndices.Realloc( RawPointIndices.Num() );
		FMemory::Memcpy( Dest, RawPointIndices.GetData(), LODModel.RawPointIndices.GetBulkDataSize() );
		LODModel.RawPointIndices.Unlock();
	}

#if DISALLOW_32BIT_INDICES
	LODModel.MultiSizeIndexContainer.CreateIndexBuffer(sizeof(uint16));
#else
	LODModel.MultiSizeIndexContainer.CreateIndexBuffer((LODModel.NumVertices < MAX_uint16)? sizeof(uint16): sizeof(uint32));
#endif

	// Finish building the sections.
	for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
	{
		FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

		const TArray<uint32>& SectionIndices = Chunks[SectionIndex]->Indices;
		FRawStaticIndexBuffer16or32Interface* IndexBuffer = LODModel.MultiSizeIndexContainer.GetIndexBuffer();
		Section.BaseIndex = IndexBuffer->Num();
		const int32 NumIndices = SectionIndices.Num();
		const TArray<uint32>& SectionVertexIndexRemap = VertexIndexRemap[Section.ChunkIndex];
		for (int32 Index = 0; Index < NumIndices; Index++)
		{
			uint32 VertexIndex = SectionVertexIndexRemap[SectionIndices[Index]];
			IndexBuffer->AddItem(VertexIndex);
		}
	}

	// Free the skinned mesh chunks which are no longer needed.
	for (int32 i = 0; i < Chunks.Num(); ++i)
	{
		delete Chunks[i];
		Chunks[i] = NULL;
	}
	Chunks.Empty();

	// Build the adjacency index buffer used for tessellation.
	{
		TArray<FSoftSkinVertex> Vertices;
		LODModel.GetVertices( Vertices );

		FMultiSizeIndexContainerData IndexData;
		LODModel.MultiSizeIndexContainer.GetIndexBufferData( IndexData );

		FMultiSizeIndexContainerData AdjacencyIndexData;
		AdjacencyIndexData.DataTypeSize = IndexData.DataTypeSize;

		BuildSkeletalAdjacencyIndexBuffer( Vertices, LODModel.NumTexCoords, IndexData.Indices, AdjacencyIndexData.Indices );
		LODModel.AdjacencyMultiSizeIndexContainer.RebuildIndexBuffer( AdjacencyIndexData );
	}

	// Compute the required bones for this model.
	USkeletalMesh::CalculateRequiredBones(LODModel,RefSkeleton,NULL);
#endif // #if WITH_EDITORONLY_DATA
}

/*------------------------------------------------------------------------------
	Common functionality.
------------------------------------------------------------------------------*/

/** Helper struct for building acceleration structures. */
struct FIndexAndZ
{
	float Z;
	int32 Index;

	/** Default constructor. */
	FIndexAndZ() {}

	/** Initialization constructor. */
	FIndexAndZ(int32 InIndex, FVector V)
	{
		Z = 0.30f * V.X + 0.33f * V.Y + 0.37f * V.Z;
		Index = InIndex;
	}
};

/** Sorting function for vertex Z/index pairs. */
struct FCompareIndexAndZ
{
	FORCEINLINE bool operator()(FIndexAndZ const& A, FIndexAndZ const& B) const { return A.Z < B.Z; }
};

static int32 ComputeNumTexCoords(FRawMesh const& RawMesh, int32 MaxSupportedTexCoords)
{
	int32 NumWedges = RawMesh.WedgeIndices.Num();
	int32 NumTexCoords = 0;
	for (int32 TexCoordIndex = 0; TexCoordIndex < MAX_MESH_TEXTURE_COORDS; ++TexCoordIndex)
	{
		if (RawMesh.WedgeTexCoords[TexCoordIndex].Num() != NumWedges)
		{
			break;
		}
		NumTexCoords++;
	}
	return FMath::Min(NumTexCoords, MaxSupportedTexCoords);
}

/**
 * Returns true if the specified points are about equal
 */
inline bool PointsEqual(const FVector& V1,const FVector& V2, float ComparisonThreshold)
{
	if (FMath::Abs(V1.X - V2.X) > ComparisonThreshold
		|| FMath::Abs(V1.Y - V2.Y) > ComparisonThreshold
		|| FMath::Abs(V1.Z - V2.Z) > ComparisonThreshold)
	{
		return false;
	}
	return true;
}

inline bool UVsEqual(const FVector2D& UV1,const FVector2D& UV2)
{
	if(FMath::Abs(UV1.X - UV2.X) > (1.0f / 1024.0f))
		return false;

	if(FMath::Abs(UV1.Y - UV2.Y) > (1.0f / 1024.0f))
		return false;

	return true;
}

static inline FVector GetPositionForWedge(FRawMesh const& Mesh, int32 WedgeIndex)
{
	int32 VertexIndex = Mesh.WedgeIndices[WedgeIndex];
	return Mesh.VertexPositions[VertexIndex];
}

struct FMeshEdge
{
	int32	Vertices[2];
	int32	Faces[2];
};

/**
 * This helper class builds the edge list for a mesh. It uses a hash of vertex
 * positions to edges sharing that vertex to remove the n^2 searching of all
 * previously added edges. This class is templatized so it can be used with
 * either static mesh or skeletal mesh vertices
 */
template <class VertexClass> class TEdgeBuilder
{
protected:
	/**
	 * The list of indices to build the edge data from
	 */
	const TArray<uint32>& Indices;
	/**
	 * The array of verts for vertex position comparison
	 */
	const TArray<VertexClass>& Vertices;
	/**
	 * The array of edges to create
	 */
	TArray<FMeshEdge>& Edges;
	/**
	 * List of edges that start with a given vertex
	 */
	TMultiMap<FVector,FMeshEdge*> VertexToEdgeList;

	/**
	 * This function determines whether a given edge matches or not. It must be
	 * provided by derived classes since they have the specific information that
	 * this class doesn't know about (vertex info, influences, etc)
	 *
	 * @param Index1 The first index of the edge being checked
	 * @param Index2 The second index of the edge
	 * @param OtherEdge The edge to compare. Was found via the map
	 *
	 * @return true if the edge is a match, false otherwise
	 */
	virtual bool DoesEdgeMatch(int32 Index1,int32 Index2,FMeshEdge* OtherEdge) = 0;

	/**
	 * Searches the list of edges to see if this one matches an existing and
	 * returns a pointer to it if it does
	 *
	 * @param Index1 the first index to check for
	 * @param Index2 the second index to check for
	 *
	 * @return NULL if no edge was found, otherwise the edge that was found
	 */
	inline FMeshEdge* FindOppositeEdge(int32 Index1,int32 Index2)
	{
		FMeshEdge* Edge = NULL;
		TArray<FMeshEdge*> EdgeList;
		// Search the hash for a corresponding vertex
		VertexToEdgeList.MultiFind(Vertices[Index2].Position,EdgeList);
		// Now search through the array for a match or not
		for (int32 EdgeIndex = 0; EdgeIndex < EdgeList.Num() && Edge == NULL;
			EdgeIndex++)
		{
			FMeshEdge* OtherEdge = EdgeList[EdgeIndex];
			// See if this edge matches the passed in edge
			if (OtherEdge != NULL && DoesEdgeMatch(Index1,Index2,OtherEdge))
			{
				// We have a match
				Edge = OtherEdge;
			}
		}
		return Edge;
	}

	/**
	 * Updates an existing edge if found or adds the new edge to the list
	 *
	 * @param Index1 the first index in the edge
	 * @param Index2 the second index in the edge
	 * @param Triangle the triangle that this edge was found in
	 */
	inline void AddEdge(int32 Index1,int32 Index2,int32 Triangle)
	{
		// If this edge matches another then just fill the other triangle
		// otherwise add it
		FMeshEdge* OtherEdge = FindOppositeEdge(Index1,Index2);
		if (OtherEdge == NULL)
		{
			// Add a new edge to the array
			int32 EdgeIndex = Edges.AddZeroed();
			Edges[EdgeIndex].Vertices[0] = Index1;
			Edges[EdgeIndex].Vertices[1] = Index2;
			Edges[EdgeIndex].Faces[0] = Triangle;
			Edges[EdgeIndex].Faces[1] = -1;
			// Also add this edge to the hash for faster searches
			// NOTE: This relies on the array never being realloced!
			VertexToEdgeList.Add(Vertices[Index1].Position,&Edges[EdgeIndex]);
		}
		else
		{
			OtherEdge->Faces[1] = Triangle;
		}
	}

public:
	/**
	 * Initializes the values for the code that will build the mesh edge list
	 */
	TEdgeBuilder(const TArray<uint32>& InIndices,
		const TArray<VertexClass>& InVertices,
		TArray<FMeshEdge>& OutEdges) :
		Indices(InIndices), Vertices(InVertices), Edges(OutEdges)
	{
		// Presize the array so that there are no extra copies being done
		// when adding edges to it
		Edges.Empty(Indices.Num());
	}

	/**
	* Virtual dtor
	*/
	virtual ~TEdgeBuilder(){}


	/**
	 * Uses a hash of indices to edge lists so that it can avoid the n^2 search
	 * through the full edge list
	 */
	void FindEdges(void)
	{
		// @todo Handle something other than trilists when building edges
		int32 TriangleCount = Indices.Num() / 3;
		int32 EdgeCount = 0;
		// Work through all triangles building the edges
		for (int32 Triangle = 0; Triangle < TriangleCount; Triangle++)
		{
			// Determine the starting index
			int32 TriangleIndex = Triangle * 3;
			// Get the indices for the triangle
			int32 Index1 = Indices[TriangleIndex];
			int32 Index2 = Indices[TriangleIndex + 1];
			int32 Index3 = Indices[TriangleIndex + 2];
			// Add the first to second edge
			AddEdge(Index1,Index2,Triangle);
			// Now add the second to third
			AddEdge(Index2,Index3,Triangle);
			// Add the third to first edge
			AddEdge(Index3,Index1,Triangle);
		}
	}
};

/**
 * This is the static mesh specific version for finding edges
 */
class FStaticMeshEdgeBuilder : public TEdgeBuilder<FStaticMeshBuildVertex>
{
public:
	/**
	 * Constructor that passes all work to the parent class
	 */
	FStaticMeshEdgeBuilder(const TArray<uint32>& InIndices,
		const TArray<FStaticMeshBuildVertex>& InVertices,
		TArray<FMeshEdge>& OutEdges) :
		TEdgeBuilder<FStaticMeshBuildVertex>(InIndices,InVertices,OutEdges)
	{
	}

	/**
	 * This function determines whether a given edge matches or not for a static mesh
	 *
	 * @param Index1 The first index of the edge being checked
	 * @param Index2 The second index of the edge
	 * @param OtherEdge The edge to compare. Was found via the map
	 *
	 * @return true if the edge is a match, false otherwise
	 */
	bool DoesEdgeMatch(int32 Index1,int32 Index2,FMeshEdge* OtherEdge)
	{
		return Vertices[OtherEdge->Vertices[1]].Position == Vertices[Index1].Position &&
			OtherEdge->Faces[1] == -1;
	}
};

static void ComputeTriangleTangents(
	TArray<FVector>& TriangleTangentX,
	TArray<FVector>& TriangleTangentY,
	TArray<FVector>& TriangleTangentZ,
	FRawMesh const& RawMesh,
	float ComparisonThreshold
	)
{
	int32 NumTriangles = RawMesh.WedgeIndices.Num() / 3;
	TriangleTangentX.Empty(NumTriangles);
	TriangleTangentY.Empty(NumTriangles);
	TriangleTangentZ.Empty(NumTriangles);

	for (int32 TriangleIndex = 0;TriangleIndex < NumTriangles; TriangleIndex++)
	{
		int32 UVIndex = 0;

		FVector P[3];
		for (int32 i = 0; i < 3; ++i)
		{
			P[i] = GetPositionForWedge(RawMesh, TriangleIndex * 3 + i);
		}

		const FVector Normal = ((P[1] - P[2])^(P[0] - P[2])).SafeNormal(ComparisonThreshold);
		FMatrix	ParameterToLocal(
			FPlane(P[1].X - P[0].X, P[1].Y - P[0].Y, P[1].Z - P[0].Z, 0),
			FPlane(P[2].X - P[0].X, P[2].Y - P[0].Y, P[2].Z - P[0].Z, 0),
			FPlane(P[0].X,          P[0].Y,          P[0].Z,          0),
			FPlane(0,               0,               0,				  1)
			);

		FVector2D T1 = RawMesh.WedgeTexCoords[UVIndex][TriangleIndex * 3 + 0];
		FVector2D T2 = RawMesh.WedgeTexCoords[UVIndex][TriangleIndex * 3 + 1];
		FVector2D T3 = RawMesh.WedgeTexCoords[UVIndex][TriangleIndex * 3 + 2];
		FMatrix ParameterToTexture(
			FPlane(	T2.X - T1.X,	T2.Y - T1.Y,	0,	0	),
			FPlane(	T3.X - T1.X,	T3.Y - T1.Y,	0,	0	),
			FPlane(	T1.X,			T1.Y,			1,	0	),
			FPlane(	0,				0,				0,	1	)
			);

		// Use InverseSlow to catch singular matrices.  InverseSafe can miss this sometimes.
		const FMatrix TextureToLocal = ParameterToTexture.InverseSlow() * ParameterToLocal;

		TriangleTangentX.Add(TextureToLocal.TransformVector(FVector(1,0,0)).SafeNormal());
		TriangleTangentY.Add(TextureToLocal.TransformVector(FVector(0,1,0)).SafeNormal());
		TriangleTangentZ.Add(Normal);

		FVector::CreateOrthonormalBasis(
			TriangleTangentX[TriangleIndex],
			TriangleTangentY[TriangleIndex],
			TriangleTangentZ[TriangleIndex]
			);
	}

	check(TriangleTangentX.Num() == NumTriangles);
	check(TriangleTangentY.Num() == NumTriangles);
	check(TriangleTangentZ.Num() == NumTriangles);
}

/**
 * Create a table that maps the corner of each face to its overlapping corners.
 * @param OutOverlappingCorners - Maps a corner index to the indices of all overlapping corners.
 * @param RawMesh - The mesh for which to compute overlapping corners.
 */
static void FindOverlappingCorners(
	TMultiMap<int32,int32>& OutOverlappingCorners,
	FRawMesh const& RawMesh,
	float ComparisonThreshold
	)
{
	int32 NumWedges = RawMesh.WedgeIndices.Num();

	// Create a list of vertex Z/index pairs
	TArray<FIndexAndZ> VertIndexAndZ;
	VertIndexAndZ.Empty(NumWedges);
	for(int32 WedgeIndex = 0; WedgeIndex < NumWedges; WedgeIndex++)
	{
		new(VertIndexAndZ) FIndexAndZ(WedgeIndex, GetPositionForWedge(RawMesh, WedgeIndex));
	}

	// Sort the vertices by z value
	VertIndexAndZ.Sort(FCompareIndexAndZ());

	// Search for duplicates, quickly!
	for (int32 i = 0; i < VertIndexAndZ.Num(); i++)
	{
		// only need to search forward, since we add pairs both ways
		for (int32 j = i + 1; j < VertIndexAndZ.Num(); j++)
		{
			if (FMath::Abs(VertIndexAndZ[j].Z - VertIndexAndZ[i].Z) > THRESH_POINTS_ARE_SAME * 4.01f)
				break; // can't be any more dups

			FVector PositionA = GetPositionForWedge(RawMesh, VertIndexAndZ[i].Index);
			FVector PositionB = GetPositionForWedge(RawMesh, VertIndexAndZ[j].Index);

			if (PointsEqual(PositionA, PositionB, ComparisonThreshold))					
			{
				OutOverlappingCorners.Add(VertIndexAndZ[i].Index,VertIndexAndZ[j].Index);
				OutOverlappingCorners.Add(VertIndexAndZ[j].Index,VertIndexAndZ[i].Index);
			}
		}
	}
}

namespace ETangentOptions
{
	enum Type
	{
		None = 0,
		BlendOverlappingNormals = 0x1,
		IgnoreDegenerateTriangles = 0x2,
	};
};

/**
	* Smoothing group interpretation helper structure.
	*/
struct FFanFace
{
	int32 FaceIndex;
	int32 LinkedVertexIndex;
	bool bFilled;
	bool bBlendTangents;
	bool bBlendNormals;
};

static void ComputeTangents(
	FRawMesh& RawMesh,
	TMultiMap<int32,int32> const& OverlappingCorners,
	uint32 TangentOptions
	)
{
	bool bBlendOverlappingNormals = (TangentOptions & ETangentOptions::BlendOverlappingNormals) != 0;
	bool bIgnoreDegenerateTriangles = (TangentOptions & ETangentOptions::IgnoreDegenerateTriangles) != 0;
	float ComparisonThreshold = bIgnoreDegenerateTriangles ? THRESH_POINTS_ARE_SAME : 0.0f;

	// Compute per-triangle tangents.
	TArray<FVector> TriangleTangentX;
	TArray<FVector> TriangleTangentY;
	TArray<FVector> TriangleTangentZ;

	ComputeTriangleTangents(
		TriangleTangentX,
		TriangleTangentY,
		TriangleTangentZ,
		RawMesh,
		bIgnoreDegenerateTriangles ? SMALL_NUMBER : 0.0f
		);

	// Declare these out here to avoid reallocations.
	TArray<FFanFace> RelevantFacesForCorner[3];
	TArray<int32> AdjacentFaces;
	TArray<int32> DupVerts;

	int32 NumWedges = RawMesh.WedgeIndices.Num();
	int32 NumFaces = NumWedges / 3;

	// Allocate storage for tangents if none were provided.
	if (RawMesh.WedgeTangentX.Num() != NumWedges)
	{
		RawMesh.WedgeTangentX.Empty(NumWedges);
		RawMesh.WedgeTangentX.AddZeroed(NumWedges);
	}
	if (RawMesh.WedgeTangentY.Num() != NumWedges)
	{
		RawMesh.WedgeTangentY.Empty(NumWedges);
		RawMesh.WedgeTangentY.AddZeroed(NumWedges);
	}
	if (RawMesh.WedgeTangentZ.Num() != NumWedges)
	{
		RawMesh.WedgeTangentZ.Empty(NumWedges);
		RawMesh.WedgeTangentZ.AddZeroed(NumWedges);
	}

	for (int32 FaceIndex = 0; FaceIndex < NumFaces; FaceIndex++)
	{
		int32 WedgeOffset = FaceIndex * 3;
		FVector CornerPositions[3];
		FVector CornerTangentX[3];
		FVector CornerTangentY[3];
		FVector CornerTangentZ[3];

		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			CornerTangentX[CornerIndex] = FVector::ZeroVector;
			CornerTangentY[CornerIndex] = FVector::ZeroVector;
			CornerTangentZ[CornerIndex] = FVector::ZeroVector;
			CornerPositions[CornerIndex] = GetPositionForWedge(RawMesh, WedgeOffset + CornerIndex);
			RelevantFacesForCorner[CornerIndex].Reset();
		}

		// Don't process degenerate triangles.
		if (PointsEqual(CornerPositions[0],CornerPositions[1], ComparisonThreshold)
			|| PointsEqual(CornerPositions[0],CornerPositions[2], ComparisonThreshold)
			|| PointsEqual(CornerPositions[1],CornerPositions[2], ComparisonThreshold))
		{
			continue;
		}

		// No need to process triangles if tangents already exist.
		bool bCornerHasTangents[3] = {0};
		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			bCornerHasTangents[CornerIndex] = !RawMesh.WedgeTangentX[WedgeOffset + CornerIndex].IsZero()
				&& !RawMesh.WedgeTangentY[WedgeOffset + CornerIndex].IsZero()
				&& !RawMesh.WedgeTangentZ[WedgeOffset + CornerIndex].IsZero();
		}
		if (bCornerHasTangents[0] && bCornerHasTangents[1] && bCornerHasTangents[2])
		{
			continue;
		}

		// Calculate smooth vertex normals.
		float Determinant = FVector::Triple(
			TriangleTangentX[FaceIndex],
			TriangleTangentY[FaceIndex],
			TriangleTangentZ[FaceIndex]
			);

		// Start building a list of faces adjacent to this face.
		AdjacentFaces.Reset();
		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			int32 ThisCornerIndex = WedgeOffset + CornerIndex;
			DupVerts.Reset();
			OverlappingCorners.MultiFind(ThisCornerIndex,DupVerts);
			DupVerts.Add(ThisCornerIndex); // I am a "dup" of myself
			for (int32 k = 0; k < DupVerts.Num(); k++)
			{
				AdjacentFaces.AddUnique(DupVerts[k] / 3);
			}
		}

		// We need to sort these here because the criteria for point equality is
		// exact, so we must ensure the exact same order for all dups.
		AdjacentFaces.Sort();

		// Process adjacent faces
		for (int32 AdjacentFaceIndex = 0; AdjacentFaceIndex < AdjacentFaces.Num(); AdjacentFaceIndex++)
		{
			int32 OtherFaceIndex = AdjacentFaces[AdjacentFaceIndex];
			for (int32 OurCornerIndex = 0; OurCornerIndex < 3; OurCornerIndex++)
			{
				if (bCornerHasTangents[OurCornerIndex])
					continue;

				FFanFace NewFanFace;
				int32 CommonIndexCount = 0;

				// Check for vertices in common.
				if (FaceIndex == OtherFaceIndex)
				{
					CommonIndexCount = 3;		
					NewFanFace.LinkedVertexIndex = OurCornerIndex;
				}
				else
				{
					// Check matching vertices against main vertex .
					for (int32 OtherCornerIndex = 0; OtherCornerIndex < 3; OtherCornerIndex++)
					{
						if (PointsEqual(
								CornerPositions[OurCornerIndex],
								GetPositionForWedge(RawMesh, OtherFaceIndex * 3 + OtherCornerIndex),
								ComparisonThreshold
								))
						{
							CommonIndexCount++;
							NewFanFace.LinkedVertexIndex = OtherCornerIndex;
						}
					}
				}

				// Add if connected by at least one point. Smoothing matches are considered later.
				if (CommonIndexCount > 0)
				{ 					
					NewFanFace.FaceIndex = OtherFaceIndex;
					NewFanFace.bFilled = (OtherFaceIndex == FaceIndex); // Starter face for smoothing floodfill.
					NewFanFace.bBlendTangents = NewFanFace.bFilled;
					NewFanFace.bBlendNormals = NewFanFace.bFilled;
					RelevantFacesForCorner[OurCornerIndex].Add(NewFanFace);
				}
			}
		}

		// Find true relevance of faces for a vertex normal by traversing
		// smoothing-group-compatible connected triangle fans around common vertices.
		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			if (bCornerHasTangents[CornerIndex])
				continue;

			int32 NewConnections;
			do
			{
				NewConnections = 0;
				for (int32 OtherFaceIdx=0; OtherFaceIdx < RelevantFacesForCorner[CornerIndex].Num(); OtherFaceIdx++)
				{
					FFanFace& OtherFace = RelevantFacesForCorner[CornerIndex][OtherFaceIdx];
					// The vertex' own face is initially the only face with bFilled == true.
					if (OtherFace.bFilled)
					{				
						for (int32 NextFaceIndex = 0; NextFaceIndex < RelevantFacesForCorner[CornerIndex].Num(); NextFaceIndex++)
						{
							FFanFace& NextFace = RelevantFacesForCorner[CornerIndex][NextFaceIndex];
							if (!NextFace.bFilled) // && !NextFace.bBlendTangents)
							{
								if ((NextFaceIndex != OtherFaceIdx)
									&& (RawMesh.FaceSmoothingMasks[NextFace.FaceIndex] & RawMesh.FaceSmoothingMasks[OtherFace.FaceIndex]))
								{				
									int32 CommonVertices = 0;
									int32 CommonTangentVertices = 0;
									int32 CommonNormalVertices = 0;
									for (int32 OtherCornerIndex = 0; OtherCornerIndex < 3; OtherCornerIndex++)
									{											
										for (int32 NextCornerIndex = 0; NextCornerIndex < 3; NextCornerIndex++)
										{
											int32 NextVertexIndex = RawMesh.WedgeIndices[NextFace.FaceIndex * 3 + NextCornerIndex];
											int32 OtherVertexIndex = RawMesh.WedgeIndices[OtherFace.FaceIndex * 3 + OtherCornerIndex];
											if (PointsEqual(
													RawMesh.VertexPositions[NextVertexIndex],
													RawMesh.VertexPositions[OtherVertexIndex],
													ComparisonThreshold))
											{
												CommonVertices++;
												if (UVsEqual(
														RawMesh.WedgeTexCoords[0][NextFace.FaceIndex * 3 + NextCornerIndex],
														RawMesh.WedgeTexCoords[0][OtherFace.FaceIndex * 3 + OtherCornerIndex]))
												{
													CommonTangentVertices++;
												}
												if (bBlendOverlappingNormals
													|| NextVertexIndex == OtherVertexIndex)
												{
													CommonNormalVertices++;
												}
											}
										}										
									}
									// Flood fill faces with more than one common vertices which must be touching edges.
									if (CommonVertices > 1)
									{
										NextFace.bFilled = true;
										NextFace.bBlendNormals = (CommonNormalVertices > 1);
										NewConnections++;

										// Only blend tangents if there is no UV seam along the edge with this face.
										if (OtherFace.bBlendTangents && CommonTangentVertices > 1)
										{
											float OtherDeterminant = FVector::Triple(
												TriangleTangentX[NextFace.FaceIndex],
												TriangleTangentY[NextFace.FaceIndex],
												TriangleTangentZ[NextFace.FaceIndex]
												);
											if ((Determinant * OtherDeterminant) > 0.0f)
											{
												NextFace.bBlendTangents = true;
											}
										}
									}								
								}
							}
						}
					}
				}
			}
			while (NewConnections > 0);
		}
        

		// Vertex normal construction.
		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			if (bCornerHasTangents[CornerIndex])
			{
				CornerTangentX[CornerIndex] = RawMesh.WedgeTangentX[WedgeOffset + CornerIndex];
				CornerTangentY[CornerIndex] = RawMesh.WedgeTangentY[WedgeOffset + CornerIndex];
				CornerTangentZ[CornerIndex] = RawMesh.WedgeTangentZ[WedgeOffset + CornerIndex];
			}
			else
			{
				for (int32 RelevantFaceIdx = 0; RelevantFaceIdx < RelevantFacesForCorner[CornerIndex].Num(); RelevantFaceIdx++)
				{
					FFanFace const& RelevantFace = RelevantFacesForCorner[CornerIndex][RelevantFaceIdx];
					if (RelevantFace.bFilled)
					{
						int32 OtherFaceIndex = RelevantFace.FaceIndex;
						if (RelevantFace.bBlendTangents)
						{
							CornerTangentX[CornerIndex] += TriangleTangentX[OtherFaceIndex];
							CornerTangentY[CornerIndex] += TriangleTangentY[OtherFaceIndex];
						}
						if (RelevantFace.bBlendNormals)
						{
							CornerTangentZ[CornerIndex] += TriangleTangentZ[OtherFaceIndex];
						}
					}
				}
				if (!RawMesh.WedgeTangentX[WedgeOffset + CornerIndex].IsZero())
				{
					CornerTangentX[CornerIndex] = RawMesh.WedgeTangentX[WedgeOffset + CornerIndex];
				}
				if (!RawMesh.WedgeTangentY[WedgeOffset + CornerIndex].IsZero())
				{
					CornerTangentY[CornerIndex] = RawMesh.WedgeTangentY[WedgeOffset + CornerIndex];
				}
				if (!RawMesh.WedgeTangentZ[WedgeOffset + CornerIndex].IsZero())
				{
					CornerTangentZ[CornerIndex] = RawMesh.WedgeTangentZ[WedgeOffset + CornerIndex];
				}
			}
		}

		// Normalization.
		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			CornerTangentX[CornerIndex].Normalize();
			CornerTangentY[CornerIndex].Normalize();
			CornerTangentZ[CornerIndex].Normalize();

			// Gram-Schmidt orthogonalization
			CornerTangentY[CornerIndex] -= CornerTangentX[CornerIndex] * (CornerTangentX[CornerIndex] | CornerTangentY[CornerIndex]);
			CornerTangentY[CornerIndex].Normalize();

			CornerTangentX[CornerIndex] -= CornerTangentZ[CornerIndex] * (CornerTangentZ[CornerIndex] | CornerTangentX[CornerIndex]);
			CornerTangentX[CornerIndex].Normalize();
			CornerTangentY[CornerIndex] -= CornerTangentZ[CornerIndex] * (CornerTangentZ[CornerIndex] | CornerTangentY[CornerIndex]);
			CornerTangentY[CornerIndex].Normalize();
		}

		// Copy back to the mesh.
		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			RawMesh.WedgeTangentX[WedgeOffset + CornerIndex] = CornerTangentX[CornerIndex];
			RawMesh.WedgeTangentY[WedgeOffset + CornerIndex] = CornerTangentY[CornerIndex];
			RawMesh.WedgeTangentZ[WedgeOffset + CornerIndex] = CornerTangentZ[CornerIndex];
		}
	}

	check(RawMesh.WedgeTangentX.Num() == NumWedges);
	check(RawMesh.WedgeTangentY.Num() == NumWedges);
	check(RawMesh.WedgeTangentZ.Num() == NumWedges);
}

static void ComputeStreamingTextureFactors(
	float* OutStreamingTextureFactors,
	float* OutMaxStreamingTextureFactor,
	const FRawMesh& Mesh
	)
{
	int32 NumTexCoords = ComputeNumTexCoords(Mesh, MAX_STATIC_TEXCOORDS);
	int32 NumFaces = Mesh.WedgeIndices.Num() / 3;
	TArray<float> TexelRatios[MAX_STATIC_TEXCOORDS];
	float MaxStreamingTextureFactor = 0.0f;
	for (int32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
	{
		int32 Wedge0 = FaceIndex * 3 + 0;
		int32 Wedge1 = FaceIndex * 3 + 1;
		int32 Wedge2 = FaceIndex * 3 + 2;

		const FVector& Pos0 = Mesh.GetWedgePosition(Wedge0);
		const FVector& Pos1 = Mesh.GetWedgePosition(Wedge1);
		const FVector& Pos2 = Mesh.GetWedgePosition(Wedge2);
		float	L1	= (Pos0 - Pos1).Size(),
				L2	= (Pos0 - Pos2).Size();

		for(int32 UVIndex = 0;UVIndex < NumTexCoords;UVIndex++)
		{
			FVector2D UV0 = Mesh.WedgeTexCoords[UVIndex][Wedge0];
			FVector2D UV1 = Mesh.WedgeTexCoords[UVIndex][Wedge1];
			FVector2D UV2 = Mesh.WedgeTexCoords[UVIndex][Wedge2];

			float	T1	= (UV0 - UV1).Size(),
					T2	= (UV0 - UV2).Size();

			if( FMath::Abs(T1 * T2) > FMath::Square(SMALL_NUMBER) )
			{
				const float TexelRatio = FMath::Max( L1 / T1, L2 / T2 );
				TexelRatios[UVIndex].Add( TexelRatio );

				// Update max texel ratio
				if( TexelRatio > MaxStreamingTextureFactor )
				{
					MaxStreamingTextureFactor = TexelRatio;
				}
			}
		}
	}

	for(int32 UVIndex = 0;UVIndex < MAX_STATIC_TEXCOORDS;UVIndex++)
	{
		OutStreamingTextureFactors[UVIndex] = 0.0f;
		if( TexelRatios[UVIndex].Num() )
		{
			// Disregard upper 75% of texel ratios.
			// This is to ignore backfacing surfaces or other non-visible surfaces that tend to map a small number of texels to a large surface.
			TexelRatios[UVIndex].Sort( TGreater<float>() );
			float TexelRatio = TexelRatios[UVIndex][ FMath::Trunc(TexelRatios[UVIndex].Num() * 0.75f) ];
			OutStreamingTextureFactors[UVIndex] = TexelRatio;
		}
	}
	*OutMaxStreamingTextureFactor = MaxStreamingTextureFactor;
}

static void BuildDepthOnlyIndexBuffer(
	TArray<uint32>& OutDepthIndices,
	const TArray<FStaticMeshBuildVertex>& InVertices,
	const TArray<uint32>& InIndices,
	const TArray<FStaticMeshSection>& InSections
	)
{
	int32 NumVertices = InVertices.Num();
	if (InIndices.Num() <= 0 || NumVertices <= 0)
	{
		OutDepthIndices.Empty();
		return;
	}

	// Create a mapping of index -> first overlapping index to accelerate the construction of the shadow index buffer.
	TArray<FIndexAndZ> VertIndexAndZ;
	VertIndexAndZ.Empty(NumVertices);
	for(int32 VertIndex = 0; VertIndex < NumVertices; VertIndex++)
	{
		new(VertIndexAndZ) FIndexAndZ(VertIndex, InVertices[VertIndex].Position);
	}
	VertIndexAndZ.Sort(FCompareIndexAndZ());

	// Setup the index map. 0xFFFFFFFF == not set.
	TArray<uint32> IndexMap;
	IndexMap.AddUninitialized(NumVertices);
	FMemory::Memset(IndexMap.GetData(),0xFF,NumVertices * sizeof(uint32));

	// Search for duplicates, quickly!
	for (int32 i = 0; i < VertIndexAndZ.Num(); i++)
	{
		uint32 SrcIndex = VertIndexAndZ[i].Index;
		float Z = VertIndexAndZ[i].Z;
		IndexMap[SrcIndex] = FMath::Min(IndexMap[SrcIndex], SrcIndex);

		// Search forward since we add pairs both ways.
		for (int32 j = i + 1; j < VertIndexAndZ.Num(); j++)
		{
			if (FMath::Abs(VertIndexAndZ[j].Z - Z) > THRESH_POINTS_ARE_SAME * 4.01f)
				break; // can't be any more dups

			uint32 OtherIndex = VertIndexAndZ[j].Index;
			if (PointsEqual(InVertices[SrcIndex].Position,InVertices[OtherIndex].Position,/*bUseEpsilonCompare=*/ true))					
			{
				IndexMap[SrcIndex] = FMath::Min(IndexMap[SrcIndex], OtherIndex);
				IndexMap[OtherIndex] = FMath::Min(IndexMap[OtherIndex], SrcIndex);
			}
		}
	}

	// Build the depth-only index buffer by remapping all indices to the first overlapping
	// vertex in the vertex buffer.
	OutDepthIndices.Empty();
	for (int32 SectionIndex = 0; SectionIndex < InSections.Num(); ++SectionIndex)
	{
		const FStaticMeshSection& Section = InSections[SectionIndex];
		int32 FirstIndex = Section.FirstIndex;
		int32 LastIndex = FirstIndex + Section.NumTriangles * 3;
		for (int32 SrcIndex = FirstIndex; SrcIndex < LastIndex; ++SrcIndex)
		{
			uint32 VertIndex = InIndices[SrcIndex];
			OutDepthIndices.Add(IndexMap[VertIndex]);
		}
	}
}

static float GetComparisonThreshold(FMeshBuildSettings const& BuildSettings)
{
	return BuildSettings.bRemoveDegenerates ? THRESH_POINTS_ARE_SAME : 0.0f;
}

/*------------------------------------------------------------------------------
	Static mesh building.
------------------------------------------------------------------------------*/

static FStaticMeshBuildVertex BuildStaticMeshVertex(FRawMesh const& RawMesh, int32 WedgeIndex, float BuildScale)
{
	FStaticMeshBuildVertex Vertex;
	Vertex.Position = GetPositionForWedge(RawMesh, WedgeIndex) * BuildScale;

	Vertex.TangentX = RawMesh.WedgeTangentX[WedgeIndex];
	Vertex.TangentY = RawMesh.WedgeTangentY[WedgeIndex];
	Vertex.TangentZ = RawMesh.WedgeTangentZ[WedgeIndex];

	if (RawMesh.WedgeColors.IsValidIndex(WedgeIndex))
	{
		Vertex.Color = RawMesh.WedgeColors[WedgeIndex];
	}
	else
	{
		Vertex.Color = FColor::White;
	}

	int32 NumTexCoords = FMath::Min<int32>(MAX_MESH_TEXTURE_COORDS, MAX_STATIC_TEXCOORDS);
	for (int32 i = 0; i < NumTexCoords; ++i)
	{
		if (RawMesh.WedgeTexCoords[i].IsValidIndex(WedgeIndex))
		{
			Vertex.UVs[i] = RawMesh.WedgeTexCoords[i][WedgeIndex];
		}
		else
		{
			Vertex.UVs[i] = FVector2D(0.0f,0.0f);
		}
	}
	return Vertex;
}

static bool AreVerticesEqual(
	FStaticMeshBuildVertex const& A,
	FStaticMeshBuildVertex const& B,
	float ComparisonThreshold
	)
{
	if (!PointsEqual(A.Position, B.Position, ComparisonThreshold)
		|| !NormalsEqual(A.TangentX, B.TangentX)
		|| !NormalsEqual(A.TangentY, B.TangentY)
		|| !NormalsEqual(A.TangentZ, B.TangentZ)
		|| A.Color != B.Color)
	{
		return false;
	}

	// UVs
	for (int32 UVIndex = 0; UVIndex < MAX_STATIC_TEXCOORDS; UVIndex++)
	{
		if (!UVsEqual(A.UVs[UVIndex], B.UVs[UVIndex]))
		{
			return false;
		}
	}

	return true;
}

static void BuildStaticMeshVertexAndIndexBuffers(
	TArray<FStaticMeshBuildVertex>& OutVertices,
	TArray<TArray<uint32> >& OutPerSectionIndices,
	TArray<int32>& OutWedgeMap,
	const FRawMesh& RawMesh,
	const TMultiMap<int32,int32>& OverlappingCorners,
	float ComparisonThreshold,
	float BuildScale
	)
{
	TMap<int32,int32> FinalVerts;
	TArray<int32> DupVerts;
	int32 NumFaces = RawMesh.WedgeIndices.Num() / 3;

	// Process each face, build vertex buffer and per-section index buffers.
	for(int32 FaceIndex = 0; FaceIndex < NumFaces; FaceIndex++)
	{
		int32 VertexIndices[3];
		FVector CornerPositions[3];

		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			CornerPositions[CornerIndex] = GetPositionForWedge(RawMesh, FaceIndex * 3 + CornerIndex);
		}

		// Don't process degenerate triangles.
		if (PointsEqual(CornerPositions[0],CornerPositions[1], ComparisonThreshold)
			|| PointsEqual(CornerPositions[0],CornerPositions[2], ComparisonThreshold)
			|| PointsEqual(CornerPositions[1],CornerPositions[2], ComparisonThreshold))
		{
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				OutWedgeMap.Add(INDEX_NONE);
			}
			continue;
		}

		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			int32 WedgeIndex = FaceIndex * 3 + CornerIndex;
			FStaticMeshBuildVertex ThisVertex = BuildStaticMeshVertex(RawMesh, WedgeIndex, BuildScale);

			DupVerts.Reset();
			OverlappingCorners.MultiFind(WedgeIndex,DupVerts);
			DupVerts.Sort();

			int32 Index = INDEX_NONE;
			for (int32 k = 0; k < DupVerts.Num(); k++)
			{
				if (DupVerts[k] >= WedgeIndex)
				{
					// the verts beyond me haven't been placed yet, so these duplicates are not relevant
					break;
				}

				int32 *Location = FinalVerts.Find(DupVerts[k]);
				if (Location != NULL
					&& AreVerticesEqual(ThisVertex, OutVertices[*Location], ComparisonThreshold))
				{
					Index = *Location;
					break;
				}
			}
			if (Index == INDEX_NONE)
			{
				Index = OutVertices.Add(ThisVertex);
				FinalVerts.Add(WedgeIndex,Index);
			}
			VertexIndices[CornerIndex] = Index;
		}

		// Reject degenerate triangles.
		if (VertexIndices[0] == VertexIndices[1]
			|| VertexIndices[1] == VertexIndices[2]
			|| VertexIndices[0] == VertexIndices[2])
		{
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				OutWedgeMap.Add(INDEX_NONE);
			}
			continue;
		}

		// Put the indices in the material index buffer.
		int32 SectionIndex = FMath::Clamp(RawMesh.FaceMaterialIndices[FaceIndex], 0, OutPerSectionIndices.Num());
		TArray<uint32>& SectionIndices = OutPerSectionIndices[SectionIndex];
		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			SectionIndices.Add(VertexIndices[CornerIndex]);
			OutWedgeMap.Add(VertexIndices[CornerIndex]);
		}
	}
}

void FMeshUtilities::CacheOptimizeVertexAndIndexBuffer(
	TArray<FStaticMeshBuildVertex>& Vertices,
	TArray<TArray<uint32> >& PerSectionIndices,
	TArray<int32>& WedgeMap
	)
{
	// Copy the vertices since we will be reordering them
	TArray<FStaticMeshBuildVertex> OriginalVertices = Vertices;

	// Initialize a cache that stores which indices have been assigned
	TArray<int32> IndexCache;
	IndexCache.AddUninitialized(Vertices.Num());
	FMemory::Memset(IndexCache.GetData(), INDEX_NONE, IndexCache.Num() * IndexCache.GetTypeSize());
	int32 NextAvailableIndex = 0;

	// Iterate through the section index buffers, 
	// Optimizing index order for the post transform cache (minimizes the number of vertices transformed), 
	// And vertex order for the pre transform cache (minimizes the amount of vertex data fetched by the GPU).
	for (int32 SectionIndex = 0; SectionIndex < PerSectionIndices.Num(); SectionIndex++)
	{
		TArray<uint32>& Indices = PerSectionIndices[SectionIndex];

		if (Indices.Num())
		{
			// Optimize the index buffer for the post transform cache with.
			CacheOptimizeIndexBuffer(Indices);

			// Copy the index buffer since we will be reordering it
			TArray<uint32> OriginalIndices = Indices;

			// Go through the indices and assign them new values that are coherent where possible
			for (int32 Index = 0; Index < Indices.Num(); Index++)
			{
				const int32 CachedIndex = IndexCache[OriginalIndices[Index]];

				if (CachedIndex == INDEX_NONE)
				{
					// No new index has been allocated for this existing index, assign a new one
					Indices[Index] = NextAvailableIndex;
					// Mark what this index has been assigned to
					IndexCache[OriginalIndices[Index]] = NextAvailableIndex;
					NextAvailableIndex++;
				}
				else
				{
					// Reuse an existing index assignment
					Indices[Index] = CachedIndex;
				}
				// Reorder the vertices based on the new index assignment
				Vertices[Indices[Index]] = OriginalVertices[OriginalIndices[Index]];
			}
		}
	}

	for (int32 i = 0; i < WedgeMap.Num(); i++)
	{
		int32 MappedIndex = WedgeMap[i];
		if (MappedIndex != INDEX_NONE)
		{
			WedgeMap[i] = IndexCache[MappedIndex];
		}
	}
}

static void ApplyScaling( FRawMesh& Mesh, float BuildScale )
{
	const int32 NumFaces = Mesh.WedgeIndices.Num() / 3;

	for(int32 FaceIndex = 0; FaceIndex < NumFaces; FaceIndex++)
	{
		for (int32 TriVertexIndex = 0; TriVertexIndex < 3; TriVertexIndex++)
		{
			const int32 WedgeIndex = FaceIndex * 3 + TriVertexIndex;
			int32 VertexIndex = Mesh.WedgeIndices[WedgeIndex];
			Mesh.VertexPositions[VertexIndex] *= BuildScale;
		}
	}

}

bool FMeshUtilities::BuildStaticMesh(
	FStaticMeshRenderData& OutRenderData,
	TArray<FStaticMeshSourceModel>& SourceModels,
	const FStaticMeshLODGroup& LODGroup
	)
{
	TIndirectArray<FRawMesh> LODMeshes;
	TIndirectArray<TMultiMap<int32,int32> > LODOverlappingCorners;
	float LODMaxDeviation[MAX_STATIC_MESH_LODS];
	FMeshBuildSettings LODBuildSettings[MAX_STATIC_MESH_LODS];

	// Gather source meshes for each LOD.
	for (int32 LODIndex = 0; LODIndex < SourceModels.Num(); ++LODIndex)
	{
		FStaticMeshSourceModel& SrcModel = SourceModels[LODIndex];
		FRawMesh& RawMesh = *new(LODMeshes) FRawMesh;
		TMultiMap<int32,int32>& OverlappingCorners = *new(LODOverlappingCorners) TMultiMap<int32,int32>;

		if (!SrcModel.RawMeshBulkData->IsEmpty())
		{
			SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);
			// Make sure the raw mesh is not irreparably malformed.
			if (!RawMesh.IsValidOrFixable())
			{
				UE_LOG(LogMeshUtilities,Error,TEXT("Raw mesh is corrupt for LOD%d."), LODIndex);
				return false;
			}
			LODBuildSettings[LODIndex] = SrcModel.BuildSettings;

			float ComparisonThreshold = GetComparisonThreshold(LODBuildSettings[LODIndex]);
			int32 NumWedges = RawMesh.WedgeIndices.Num();

			// Find overlapping corners to accelerate adjacency.
			FindOverlappingCorners(OverlappingCorners, RawMesh, ComparisonThreshold);

			// Figure out if we should recompute normals and tangents.
			bool bRecomputeNormals = SrcModel.BuildSettings.bRecomputeNormals || RawMesh.WedgeTangentZ.Num() == 0;
			bool bRecomputeTangents = SrcModel.BuildSettings.bRecomputeTangents || RawMesh.WedgeTangentX.Num() == 0 || RawMesh.WedgeTangentY.Num() == 0;

			// Dump normals and tangents if we are recomputing them.
			if (bRecomputeTangents)
			{
				RawMesh.WedgeTangentX.Empty(NumWedges);
				RawMesh.WedgeTangentX.AddZeroed(NumWedges);
				RawMesh.WedgeTangentY.Empty(NumWedges);
				RawMesh.WedgeTangentY.AddZeroed(NumWedges);
			}
			if (bRecomputeNormals)
			{
				RawMesh.WedgeTangentZ.Empty(NumWedges);
				RawMesh.WedgeTangentZ.AddZeroed(NumWedges);
			}

			// Compute any missing tangents.
			{
				// Static meshes always blend normals of overlapping corners.
				uint32 TangentOptions = ETangentOptions::BlendOverlappingNormals;
				if (SrcModel.BuildSettings.bRemoveDegenerates)
				{
					// If removing degenerate triangles, ignore them when computing tangents.
					TangentOptions |= ETangentOptions::IgnoreDegenerateTriangles;
				}
				ComputeTangents(RawMesh, OverlappingCorners, TangentOptions);
			}

			// At this point the mesh will have valid tangents.
			check(RawMesh.WedgeTangentX.Num() == NumWedges);
			check(RawMesh.WedgeTangentY.Num() == NumWedges);
			check(RawMesh.WedgeTangentZ.Num() == NumWedges);
		}
		else if (LODIndex > 0 && MeshReduction)
		{
			// If a raw mesh is not explicitly provided, use the raw mesh of the
			// next highest LOD.
			RawMesh = LODMeshes[LODIndex-1];
			OverlappingCorners = LODOverlappingCorners[LODIndex-1];
			LODBuildSettings[LODIndex] = LODBuildSettings[LODIndex-1];
		}
	}
	check(LODMeshes.Num() == SourceModels.Num());
	check(LODOverlappingCorners.Num() == SourceModels.Num());

	// Bail if there is no raw mesh data from which to build a renderable mesh.
	if (LODMeshes.Num() == 0 || LODMeshes[0].WedgeIndices.Num() == 0)
	{
		return false;
	}

	// Reduce each LOD mesh according to its reduction settings.
	OutRenderData.bReducedBySimplygon = false;
	int32 NumValidLODs = 0;
	for (int32 LODIndex = 0; LODIndex < SourceModels.Num(); ++LODIndex)
	{
		const FStaticMeshSourceModel& SrcModel = SourceModels[LODIndex];
		FMeshReductionSettings ReductionSettings = LODGroup.GetSettings(SrcModel.ReductionSettings, LODIndex);
		LODMaxDeviation[NumValidLODs] = 0.0f;
		if (LODIndex != NumValidLODs)
		{
			LODBuildSettings[NumValidLODs] = LODBuildSettings[LODIndex];
			LODOverlappingCorners[NumValidLODs] = LODOverlappingCorners[LODIndex];
		}

		if (MeshReduction && (ReductionSettings.PercentTriangles < 1.0f || ReductionSettings.MaxDeviation > 0.0f))
		{
			FRawMesh InMesh = LODMeshes[ReductionSettings.BaseLODModel];
			FRawMesh& DestMesh = LODMeshes[NumValidLODs];
			TMultiMap<int32,int32>& DestOverlappingCorners = LODOverlappingCorners[NumValidLODs];

			MeshReduction->Reduce(DestMesh, LODMaxDeviation[NumValidLODs], InMesh, ReductionSettings);
			if (DestMesh.WedgeIndices.Num() > 0 && !DestMesh.IsValid())
			{
				UE_LOG(LogMeshUtilities,Error,TEXT("Mesh reduction produced a corrupt mesh for LOD%d"),LODIndex);
				return false;
			}
			OutRenderData.bReducedBySimplygon = bUsingSimplygon;

			// Recompute adjacency information.
			DestOverlappingCorners.Reset();
			float ComparisonThreshold = GetComparisonThreshold(LODBuildSettings[NumValidLODs]);
			FindOverlappingCorners(DestOverlappingCorners, DestMesh, ComparisonThreshold);
		}

		if (LODMeshes[NumValidLODs].WedgeIndices.Num() > 0)
		{
			NumValidLODs++;
		}
	}

	if (NumValidLODs < 1)
	{
		return false;
	}

	// Generate per-LOD rendering data.
	OutRenderData.AllocateLODResources(NumValidLODs);
	for (int32 LODIndex = 0; LODIndex < NumValidLODs; ++LODIndex)
	{
		FStaticMeshLODResources& LODModel = OutRenderData.LODResources[LODIndex];
		FRawMesh& RawMesh = LODMeshes[LODIndex];
		LODModel.MaxDeviation = LODMaxDeviation[LODIndex];

		TArray<FStaticMeshBuildVertex> Vertices;
		TArray<TArray<uint32> > PerSectionIndices;

		// Find out how many sections are in the mesh.
		int32 MaxMaterialIndex = -1;
		for (int32 FaceIndex = 0; FaceIndex < RawMesh.FaceMaterialIndices.Num(); FaceIndex++)
		{
			MaxMaterialIndex = FMath::Max<int32>(RawMesh.FaceMaterialIndices[FaceIndex],MaxMaterialIndex);
		}
		MaxMaterialIndex = FMath::Min(MaxMaterialIndex, 64);
		while (MaxMaterialIndex >= LODModel.Sections.Num())
		{
			FStaticMeshSection* Section = new(LODModel.Sections) FStaticMeshSection();
			Section->MaterialIndex = LODModel.Sections.Num() - 1;
			new(PerSectionIndices) TArray<uint32>;
		}

		// Build and cache optimize vertex and index buffers.
		{
			// TODO_STATICMESH: The wedge map is only valid for LODIndex 0 if no reduction has been performed.
			// We can compute an approximate one instead for other LODs.
			TArray<int32> TempWedgeMap;
			TArray<int32>& WedgeMap = (LODIndex == 0 && SourceModels[0].ReductionSettings.PercentTriangles >= 1.0f) ? OutRenderData.WedgeMap : TempWedgeMap;
			float ComparisonThreshold = GetComparisonThreshold(LODBuildSettings[LODIndex]);
			BuildStaticMeshVertexAndIndexBuffers(Vertices, PerSectionIndices, WedgeMap, RawMesh, LODOverlappingCorners[LODIndex], ComparisonThreshold, LODBuildSettings[LODIndex].BuildScale );
			check(WedgeMap.Num() == RawMesh.WedgeIndices.Num());
			CacheOptimizeVertexAndIndexBuffer(Vertices, PerSectionIndices, WedgeMap);
			check(WedgeMap.Num() == RawMesh.WedgeIndices.Num());
		}

		// Initialize the vertex buffer.
		int32 NumTexCoords = ComputeNumTexCoords(RawMesh, MAX_STATIC_TEXCOORDS);
		LODModel.VertexBuffer.SetUseFullPrecisionUVs(LODBuildSettings[LODIndex].bUseFullPrecisionUVs);	
		LODModel.VertexBuffer.Init(Vertices,NumTexCoords);
		LODModel.PositionVertexBuffer.Init(Vertices);
		LODModel.ColorVertexBuffer.Init(Vertices);

		// Concatenate the per-section index buffers.
		TArray<uint32> CombinedIndices;
		bool bNeeds32BitIndices = false;
		for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
		{
			FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
			TArray<uint32> const& SectionIndices = PerSectionIndices[SectionIndex];
			Section.FirstIndex = 0;
			Section.NumTriangles = 0;
			Section.MinVertexIndex = 0;
			Section.MaxVertexIndex = 0;

			if (SectionIndices.Num())
			{
				Section.FirstIndex = CombinedIndices.Num();
				Section.NumTriangles = SectionIndices.Num() / 3;

				CombinedIndices.AddUninitialized(SectionIndices.Num());
				uint32* DestPtr = &CombinedIndices[Section.FirstIndex];
				uint32 const* SrcPtr = SectionIndices.GetTypedData();

				Section.MinVertexIndex = *SrcPtr;
				Section.MaxVertexIndex = *SrcPtr;

				for (int32 Index = 0; Index < SectionIndices.Num(); Index++)
				{
					uint32 VertIndex = *SrcPtr++;

					bNeeds32BitIndices |= (VertIndex > MAX_uint16);
					Section.MinVertexIndex = FMath::Min<uint32>(VertIndex,Section.MinVertexIndex);
					Section.MaxVertexIndex = FMath::Max<uint32>(VertIndex,Section.MaxVertexIndex);
					*DestPtr++ = VertIndex;
				}
			}
		}
		LODModel.IndexBuffer.SetIndices(CombinedIndices, bNeeds32BitIndices ? EIndexBufferStride::Force32Bit : EIndexBufferStride::Force16Bit);

		if (LODIndex == 0)
		{
			ComputeStreamingTextureFactors(
				OutRenderData.StreamingTextureFactors,
				&OutRenderData.MaxStreamingTextureFactor,
				RawMesh
				);
		}

		// Build the depth-only index buffer.
		{
			TArray<uint32> DepthOnlyIndices;
			BuildDepthOnlyIndexBuffer(
				DepthOnlyIndices,
				Vertices,
				CombinedIndices,
				LODModel.Sections
				);
			CacheOptimizeIndexBuffer(DepthOnlyIndices);
			LODModel.DepthOnlyIndexBuffer.SetIndices(DepthOnlyIndices, bNeeds32BitIndices ? EIndexBufferStride::Force32Bit : EIndexBufferStride::Force16Bit);
		}

		// Build a list of wireframe edges in the static mesh.
		{
			TArray<FMeshEdge> Edges;
			TArray<uint32> WireframeIndices;

			FStaticMeshEdgeBuilder(CombinedIndices,Vertices,Edges).FindEdges();
			WireframeIndices.Empty(2 * Edges.Num());
			for(int32 EdgeIndex = 0;EdgeIndex < Edges.Num();EdgeIndex++)
			{
				FMeshEdge&	Edge = Edges[EdgeIndex];
				WireframeIndices.Add(Edge.Vertices[0]);
				WireframeIndices.Add(Edge.Vertices[1]);
			}
			LODModel.WireframeIndexBuffer.SetIndices(WireframeIndices, bNeeds32BitIndices ? EIndexBufferStride::Force32Bit : EIndexBufferStride::Force16Bit);
		}

		// Build the adjacency index buffer used for tessellation.
		{
			TArray<uint32> AdjacencyIndices;

			BuildStaticAdjacencyIndexBuffer(
				LODModel.PositionVertexBuffer,
				LODModel.VertexBuffer,
				CombinedIndices,
				AdjacencyIndices
				);
			LODModel.AdjacencyIndexBuffer.SetIndices(AdjacencyIndices, bNeeds32BitIndices ? EIndexBufferStride::Force32Bit : EIndexBufferStride::Force16Bit);
		}
	}

	// Copy the original material indices to fixup meshes before compacting of materials was done.
	if (NumValidLODs > 0)
	{
		OutRenderData.MaterialIndexToImportIndex = LODMeshes[0].MaterialIndexToImportIndex;
	}

	// Calculate the bounding box.
	FBox BoundingBox(0);
	FPositionVertexBuffer& BasePositionVertexBuffer = OutRenderData.LODResources[0].PositionVertexBuffer;
	for (uint32 VertexIndex = 0; VertexIndex < BasePositionVertexBuffer.GetNumVertices(); VertexIndex++)
	{
		BoundingBox += BasePositionVertexBuffer.VertexPosition(VertexIndex);
	}
	BoundingBox.GetCenterAndExtents(OutRenderData.Bounds.Origin,OutRenderData.Bounds.BoxExtent);

	// Calculate the bounding sphere, using the center of the bounding box as the origin.
	OutRenderData.Bounds.SphereRadius = 0.0f;
	for (uint32 VertexIndex = 0; VertexIndex < BasePositionVertexBuffer.GetNumVertices(); VertexIndex++)
	{
		OutRenderData.Bounds.SphereRadius = FMath::Max(
			(BasePositionVertexBuffer.VertexPosition(VertexIndex) - OutRenderData.Bounds.Origin).Size(),
			OutRenderData.Bounds.SphereRadius
			);
	}

	return true;
}


bool FMeshUtilities::BuildSkeletalMesh( FStaticLODModel& LODModel, const FReferenceSkeleton& RefSkeleton, const TArray<FVertInfluence>& Influences, const TArray<FMeshWedge>& Wedges, const TArray<FMeshFace>& Faces, const TArray<FVector>& Points, const TArray<int32>& PointToOriginalMap, bool bKeepOverlappingVertices, bool bComputeNormals, bool bComputeTangents )
{
#if WITH_EDITORONLY_DATA
	bool bTooManyVerts = false;

	check(PointToOriginalMap.Num() == Points.Num());

	// Calculate face tangent vectors.
	TArray<FVector>	FaceTangentX;
	TArray<FVector>	FaceTangentY;
	FaceTangentX.AddUninitialized(Faces.Num());
	FaceTangentY.AddUninitialized(Faces.Num());

	if( bComputeNormals || bComputeTangents )
	{
		for(int32 FaceIndex = 0;FaceIndex < Faces.Num();FaceIndex++)
		{
			FVector	P1 = Points[Wedges[Faces[FaceIndex].iWedge[0]].iVertex],
				P2 = Points[Wedges[Faces[FaceIndex].iWedge[1]].iVertex],
				P3 = Points[Wedges[Faces[FaceIndex].iWedge[2]].iVertex];
			FVector	TriangleNormal = FPlane(P3,P2,P1);
			FMatrix	ParameterToLocal(
				FPlane(	P2.X - P1.X,	P2.Y - P1.Y,	P2.Z - P1.Z,	0	),
				FPlane(	P3.X - P1.X,	P3.Y - P1.Y,	P3.Z - P1.Z,	0	),
				FPlane(	P1.X,			P1.Y,			P1.Z,			0	),
				FPlane(	0,				0,				0,				1	)
				);

			float	U1 = Wedges[Faces[FaceIndex].iWedge[0]].UVs[0].X,
				U2 = Wedges[Faces[FaceIndex].iWedge[1]].UVs[0].X,
				U3 = Wedges[Faces[FaceIndex].iWedge[2]].UVs[0].X,
				V1 = Wedges[Faces[FaceIndex].iWedge[0]].UVs[0].Y,
				V2 = Wedges[Faces[FaceIndex].iWedge[1]].UVs[0].Y,
				V3 = Wedges[Faces[FaceIndex].iWedge[2]].UVs[0].Y;

			FMatrix	ParameterToTexture(
				FPlane(	U2 - U1,	V2 - V1,	0,	0	),
				FPlane(	U3 - U1,	V3 - V1,	0,	0	),
				FPlane(	U1,			V1,			1,	0	),
				FPlane(	0,			0,			0,	1	)
				);

			FMatrix	TextureToLocal = ParameterToTexture.InverseSlow() * ParameterToLocal;
			FVector	TangentX = TextureToLocal.TransformVector(FVector(1,0,0)).SafeNormal(),
				TangentY = TextureToLocal.TransformVector(FVector(0,1,0)).SafeNormal(),
				TangentZ;

			TangentX = TangentX - TriangleNormal * (TangentX | TriangleNormal);
			TangentY = TangentY - TriangleNormal * (TangentY | TriangleNormal);

			FaceTangentX[FaceIndex] = TangentX.SafeNormal();
			FaceTangentY[FaceIndex] = TangentY.SafeNormal();
		}
	}

	TArray<int32>	WedgeInfluenceIndices;

	// Find wedge influences.
	TMap<uint32,uint32> VertexIndexToInfluenceIndexMap;

	for(uint32 LookIdx = 0;LookIdx < (uint32)Influences.Num();LookIdx++)
	{
		// Order matters do not allow the map to overwrite an existing value.
		if( !VertexIndexToInfluenceIndexMap.Find( Influences[LookIdx].VertIndex) )
		{
			VertexIndexToInfluenceIndexMap.Add( Influences[LookIdx].VertIndex, LookIdx );
		}
	}

	for(int32 WedgeIndex = 0;WedgeIndex < Wedges.Num();WedgeIndex++)
	{
		uint32* InfluenceIndex = VertexIndexToInfluenceIndexMap.Find( Wedges[WedgeIndex].iVertex );

		check( InfluenceIndex );

		WedgeInfluenceIndices.Add( *InfluenceIndex );
	}

 	check(Wedges.Num() == WedgeInfluenceIndices.Num());

	// Calculate smooth wedge tangent vectors.

	if( IsInGameThread() )
	{
		// Only update status if in the game thread.  When importing morph targets, this function can run in another thread
		GWarn->BeginSlowTask( NSLOCTEXT("UnrealEd", "ProcessingSkeletalTriangles", "Processing Mesh Triangles"), true );
	}


	// To accelerate generation of adjacency, we'll create a table that maps each vertex index
	// to its overlapping vertices, and a table that maps a vertex to the its influenced faces
	TMultiMap<int32,int32> Vert2Duplicates;
	TMultiMap<int32,int32> Vert2Faces;
	{
		// Create a list of vertex Z/index pairs
		TArray<FSkeletalMeshVertIndexAndZ> VertIndexAndZ;
		VertIndexAndZ.Empty(Points.Num());
		for (int32 i = 0; i < Points.Num(); i++)
		{
			FSkeletalMeshVertIndexAndZ iandz;
			iandz.Index = i;
			iandz.Z = Points[i].Z;
			VertIndexAndZ.Add(iandz);
		}

		// Sorting function for vertex Z/index pairs
		struct FCompareFSkeletalMeshVertIndexAndZ
		{
			FORCEINLINE bool operator()( const FSkeletalMeshVertIndexAndZ& A, const FSkeletalMeshVertIndexAndZ& B ) const
			{
				return A.Z < B.Z;
			}
		};

		// Sort the vertices by z value
		VertIndexAndZ.Sort( FCompareFSkeletalMeshVertIndexAndZ() );

		// Search for duplicates, quickly!
		for (int32 i = 0; i < VertIndexAndZ.Num(); i++)
		{
			// only need to search forward, since we add pairs both ways
			for (int32 j = i + 1; j < VertIndexAndZ.Num(); j++)
			{
				if (FMath::Abs(VertIndexAndZ[j].Z - VertIndexAndZ[i].Z) > THRESH_POINTS_ARE_SAME )
				{
					// our list is sorted, so there can't be any more dupes
					break;
				}

				// check to see if the points are really overlapping
				if(PointsEqual(
					Points[VertIndexAndZ[i].Index],
					Points[VertIndexAndZ[j].Index] ))					
				{
					Vert2Duplicates.Add(VertIndexAndZ[i].Index,VertIndexAndZ[j].Index);
					Vert2Duplicates.Add(VertIndexAndZ[j].Index,VertIndexAndZ[i].Index);
				}
			}
		}

		// we are done with this
		VertIndexAndZ.Empty();

		// now create a map from vert indices to faces
		for(int32 FaceIndex = 0;FaceIndex < Faces.Num();FaceIndex++)
		{
			const FMeshFace&	Face = Faces[FaceIndex];
			for(int32 VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			{
				Vert2Faces.AddUnique(Wedges[Face.iWedge[VertexIndex]].iVertex,FaceIndex);
			}
		}
	}

	TArray<FSkinnedMeshChunk*> Chunks;
	TArray<int32> AdjacentFaces;
	TArray<int32> DupVerts;
	TArray<int32> DupFaces;

	for(int32 FaceIndex = 0;FaceIndex < Faces.Num();FaceIndex++)
	{
		// Only update the status progress bar if we are in the gamethread and every thousand faces. 
		// Updating status is extremely slow
		if( IsInGameThread() && FaceIndex % 5000 == 0 )
		{
			// Only update status if in the game thread.  When importing morph targets, this function can run in another thread
			GWarn->StatusUpdate( FaceIndex, Faces.Num(), NSLOCTEXT("UnrealEd", "ProcessingSkeletalTriangles", "Processing Mesh Triangles") );
		}

		const FMeshFace&	Face = Faces[FaceIndex];

		FVector	VertexTangentX[3],
				VertexTangentY[3],
				VertexTangentZ[3];

		if( bComputeNormals || bComputeTangents )
		{
			for(int32 VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			{
				VertexTangentX[VertexIndex] = FVector::ZeroVector;
				VertexTangentY[VertexIndex] = FVector::ZeroVector;
				VertexTangentZ[VertexIndex] = FVector::ZeroVector;
			}

			FVector	TriangleNormal = FPlane(
				Points[Wedges[Face.iWedge[2]].iVertex],
				Points[Wedges[Face.iWedge[1]].iVertex],
				Points[Wedges[Face.iWedge[0]].iVertex]
			);
			float	Determinant = FVector::Triple(FaceTangentX[FaceIndex],FaceTangentY[FaceIndex],TriangleNormal);

			// Start building a list of faces adjacent to this triangle
			AdjacentFaces.Reset();
			for(int32 VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			{
				int32 vert = Wedges[Face.iWedge[VertexIndex]].iVertex;
				DupVerts.Reset();
				Vert2Duplicates.MultiFind(vert,DupVerts);
				DupVerts.Add(vert); // I am a "dupe" of myself
				for (int32 k = 0; k < DupVerts.Num(); k++)
				{
					DupFaces.Reset();
					Vert2Faces.MultiFind(DupVerts[k],DupFaces);
					for (int32 l = 0; l < DupFaces.Num(); l++)
					{
						AdjacentFaces.AddUnique(DupFaces[l]);
					}
				}
			}

			// Process adjacent faces
			for(int32 AdjacentFaceIndex = 0;AdjacentFaceIndex < AdjacentFaces.Num();AdjacentFaceIndex++)
			{
				int32 OtherFaceIndex = AdjacentFaces[AdjacentFaceIndex];
				const FMeshFace&	OtherFace = Faces[OtherFaceIndex];
				FVector		OtherTriangleNormal = FPlane(
					Points[Wedges[OtherFace.iWedge[2]].iVertex],
					Points[Wedges[OtherFace.iWedge[1]].iVertex],
					Points[Wedges[OtherFace.iWedge[0]].iVertex]
				);
				float		OtherFaceDeterminant = FVector::Triple(FaceTangentX[OtherFaceIndex],FaceTangentY[OtherFaceIndex],OtherTriangleNormal);

				for(int32 VertexIndex = 0;VertexIndex < 3;VertexIndex++)
				{
					for(int32 OtherVertexIndex = 0;OtherVertexIndex < 3;OtherVertexIndex++)
					{
						if(PointsEqual(
							Points[Wedges[OtherFace.iWedge[OtherVertexIndex]].iVertex],
							Points[Wedges[Face.iWedge[VertexIndex]].iVertex]
						))					
						{
							if(Determinant * OtherFaceDeterminant > 0.0f && SkeletalMeshTools::SkeletalMesh_UVsEqual(Wedges[OtherFace.iWedge[OtherVertexIndex]],Wedges[Face.iWedge[VertexIndex]]))
							{
								VertexTangentX[VertexIndex] += FaceTangentX[OtherFaceIndex];
								VertexTangentY[VertexIndex] += FaceTangentY[OtherFaceIndex];
							}

							// Only contribute 'normal' if the vertices are truly one and the same to obey hard "smoothing" edges baked into 
							// the mesh by vertex duplication
							if( Wedges[OtherFace.iWedge[OtherVertexIndex]].iVertex == Wedges[Face.iWedge[VertexIndex]].iVertex ) 
							{
								VertexTangentZ[VertexIndex] += OtherTriangleNormal;
							}
						}
					}
				}
			}
		}

		// Find a chunk which matches this triangle.
		FSkinnedMeshChunk* Chunk = NULL;
		for (int32 i = 0; i < Chunks.Num(); ++i)
		{
			if (Chunks[i]->MaterialIndex == Face.MeshMaterialIndex)
			{
				Chunk = Chunks[i];
				break;
			}
		}
		if (Chunk == NULL)
		{
			Chunk = new FSkinnedMeshChunk();
			Chunk->MaterialIndex = Face.MeshMaterialIndex;
			Chunk->OriginalSectionIndex = Chunks.Num();
			Chunks.Add(Chunk);
		}

		uint32 TriangleIndices[3];

		for(int32 VertexIndex = 0;VertexIndex < 3;VertexIndex++)
		{
			FSoftSkinBuildVertex	Vertex;

			Vertex.Position = Points[Wedges[Face.iWedge[VertexIndex]].iVertex];

			FVector TangentX,TangentY,TangentZ;

			if( bComputeNormals || bComputeTangents )
			{
				TangentX = VertexTangentX[VertexIndex].SafeNormal();
				TangentY = VertexTangentY[VertexIndex].SafeNormal();

				if( bComputeNormals )
				{
					TangentZ = VertexTangentZ[VertexIndex].SafeNormal();
				}
				else
				{
					TangentZ = Face.TangentZ[VertexIndex];
				}

				TangentY -= TangentX * (TangentX | TangentY);
				TangentY.Normalize();

				TangentX -= TangentZ * (TangentZ | TangentX);
				TangentY -= TangentZ * (TangentZ | TangentY);

				TangentX.Normalize();
				TangentY.Normalize();
			}
			else
			{
				TangentX = Face.TangentX[VertexIndex];
				TangentY = Face.TangentY[VertexIndex];
				TangentZ = Face.TangentZ[VertexIndex];

				// Normalize overridden tangents.  Its possible for them to import un-normalized.
				TangentX.Normalize();
				TangentY.Normalize();
				TangentZ.Normalize();
			}

			Vertex.TangentX = TangentX;
			Vertex.TangentY = TangentY;
			Vertex.TangentZ = TangentZ;

			FMemory::Memcpy( Vertex.UVs,  Wedges[Face.iWedge[VertexIndex]].UVs, sizeof(FVector2D)*MAX_TEXCOORDS);	
			Vertex.Color = Wedges[Face.iWedge[VertexIndex]].Color;

			{
				// Count the influences.

				int32 InfIdx = WedgeInfluenceIndices[Face.iWedge[VertexIndex]];
				int32 LookIdx = InfIdx;

				uint32 InfluenceCount = 0;
				while( Influences.IsValidIndex(LookIdx) && (Influences[LookIdx].VertIndex == Wedges[Face.iWedge[VertexIndex]].iVertex) )
				{			
					InfluenceCount++;
					LookIdx++;
				}
				InfluenceCount = FMath::Min<uint32>(InfluenceCount,MAX_TOTAL_INFLUENCES);

				// Setup the vertex influences.

				Vertex.InfluenceBones[0] = 0;
				Vertex.InfluenceWeights[0] = 255;
				for(uint32 i = 1;i < MAX_TOTAL_INFLUENCES;i++)
				{
					Vertex.InfluenceBones[i] = 0;
					Vertex.InfluenceWeights[i] = 0;
				}

				uint32	TotalInfluenceWeight = 0;
				for(uint32 i = 0;i < InfluenceCount;i++)
				{
					FBoneIndexType BoneIndex = (FBoneIndexType)Influences[InfIdx+i].BoneIndex;
					if( BoneIndex >= RefSkeleton.GetNum() )
						continue;

					Vertex.InfluenceBones[i] = BoneIndex;
					Vertex.InfluenceWeights[i] = (uint8)(Influences[InfIdx+i].Weight * 255.0f);
					TotalInfluenceWeight += Vertex.InfluenceWeights[i];
				}
				Vertex.InfluenceWeights[0] += 255 - TotalInfluenceWeight;
			}

			// Add the vertex as well as its original index in the points array
			Vertex.PointWedgeIdx = Wedges[Face.iWedge[VertexIndex]].iVertex;

			int32	V = SkeletalMeshTools::AddSkinVertex(Chunk->Vertices,Vertex,bKeepOverlappingVertices);


			// set the index entry for the newly added vertex
			// check(V >= 0 && V <= MAX_uint16);
#if DISALLOW_32BIT_INDICES
			if (V > MAX_uint16)
			{
				bTooManyVerts = true;
			}
			TriangleIndices[VertexIndex] = (uint16)V;
#else
			// TArray internally has int32 for capacity, so no need to test for uint32 as it's larger than int32
			TriangleIndices[VertexIndex] = (uint32)V;
#endif
		}

		if(TriangleIndices[0] != TriangleIndices[1] && TriangleIndices[0] != TriangleIndices[2] && TriangleIndices[1] != TriangleIndices[2])
		{
			for(uint32 VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			{
				Chunk->Indices.Add(TriangleIndices[VertexIndex]);
			}
		}
	}

	// Chunk vertices to satisfy the requested limit.
	static const auto MaxBonesVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("Compat.MAX_GPUSKIN_BONES"));
	const int32 MaxGPUSkinBones = MaxBonesVar->GetValueOnAnyThread();
	SkeletalMeshTools::ChunkSkinnedVertices(Chunks,MaxGPUSkinBones);

	// Build the skeletal model from chunks.
	BuildSkeletalModelFromChunks(LODModel,RefSkeleton,Chunks,PointToOriginalMap);

	if( IsInGameThread() )
	{
		// Only update status if in the game thread.  When importing morph targets, this function can run in another thread
		GWarn->EndSlowTask();
	}

	// Only show these warnings if in the game thread.  When importing morph targets, this function can run in another thread and these warnings dont prevent the mesh from importing
	if( IsInGameThread() )
	{
		bool bHasBadSections = false;
		for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
		{
			FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
			bHasBadSections |= (Section.NumTriangles == 0);

			// Log info about the section.
			UE_LOG(LogSkeletalMesh, Log, TEXT("Section %u: Material=%u, Chunk=%u, %u triangles"),
				SectionIndex,
				Section.MaterialIndex,
				Section.ChunkIndex,
				Section.NumTriangles
				);
		}
		if( bHasBadSections )
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_SkeletalMeshHasBadSections", "Input mesh has a section with no triangles.  This mesh may not render properly.") );
		}

		if (bTooManyVerts)
		{
			UE_LOG(LogSkeletalMesh, Log, TEXT("Input mesh has too many vertices.  The generated mesh will be corrupt!"));
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_SkeletalMeshTooManyVertices", "Input mesh has too many vertices.  The generated mesh will be corrupt!  Consider adding extra materials to split up the source mesh into smaller chunks."));
		}
	}

	return true;
#else
	UE_LOG(LogSkeletalMesh, Fatal,TEXT("Cannot call FSkeletalMeshTools::CreateSkinningStreams on a console!"));
	return false;
#endif
}

void FMeshUtilities::CreateProxyMesh( 
	const TArray<AActor*>& SourceActors, 
	const struct FMeshProxySettings& InProxySettings,
	const FString& ProxyPackageName,
	TArray<UObject*>& OutAssetsToSync,
	FVector& OutProxyLocation)
{
	if (MeshMerging == NULL)
	{
		UE_LOG(LogMeshUtilities, Log, TEXT("No automatic mesh merging module available"));
		return;
	}

	TArray<ALandscapeProxy*>		LandscapesToMerge;
	TArray<UStaticMeshComponent*>	ComponentsToMerge;
	
	//Collect components of the corresponding actor
	for (AActor* Actor : SourceActors)
	{
		ALandscapeProxy* LandscapeActor = Cast<ALandscapeProxy>(Actor);
		if (LandscapeActor)
		{
			LandscapesToMerge.Add(LandscapeActor);
		}
		else
		{
			TArray<UStaticMeshComponent*> Components;
			Actor->GetComponents<UStaticMeshComponent>(Components);
			ComponentsToMerge.Append(Components);
		}
	}
	
	// Convert collected static mesh components and landscapes into raw meshes and flatten materials
	TArray<FRawMesh>								RawMeshes;
	TArray<MaterialExportUtils::FFlattenMaterial>	UniqueMaterials;
	TMap<int32, TArray<int32>>						MaterialMap;
	FBox											ProxyBounds(0);
	
	RawMeshes.Empty(ComponentsToMerge.Num() + LandscapesToMerge.Num());
	UniqueMaterials.Empty(ComponentsToMerge.Num() + LandscapesToMerge.Num());
	
	// Convert static mesh components
	TArray<UMaterialInterface*> StaticMeshMaterials;
	for (UStaticMeshComponent* MeshComponent : ComponentsToMerge)
	{
		TArray<int32> RawMeshMaterialMap;
		int32 RawMeshId = RawMeshes.Add(FRawMesh());
		
		if (ConstructRawMesh(MeshComponent, RawMeshes[RawMeshId], StaticMeshMaterials, RawMeshMaterialMap))
		{
			MaterialMap.Add(RawMeshId, RawMeshMaterialMap);
			//Store the bounds for each component
			ProxyBounds+= MeshComponent->Bounds.GetBox();
		}
		else
		{
			RawMeshes.RemoveAt(RawMeshId);
		}
	}

	// Convert materials into flatten materials
	for (UMaterialInterface* Material : StaticMeshMaterials)
	{
		UniqueMaterials.Add(MaterialExportUtils::FFlattenMaterial());
		MaterialExportUtils::ExportMaterial(Material, UniqueMaterials.Last());
	}
	
	// Convert landscapes
	for (ALandscapeProxy* Landscape : LandscapesToMerge)
	{
		TArray<int32> RawMeshMaterialMap;
		int32 RawMeshId = RawMeshes.Add(FRawMesh());

		if (Landscape->ExportToRawMesh(RawMeshes[RawMeshId]))
		{
			// Landscape has one unique material
			int32 MatIdx = UniqueMaterials.Add(MaterialExportUtils::FFlattenMaterial());
			RawMeshMaterialMap.Add(MatIdx);
			MaterialMap.Add(RawMeshId, RawMeshMaterialMap);
			// This is texture resolution for a landscape, probably need to be calculated using landscape size
			UniqueMaterials.Last().DiffuseSize = FIntPoint(1024, 1024);
			MaterialExportUtils::ExportMaterial(Landscape, UniqueMaterials.Last());
			
			//Store the bounds for each component
			ProxyBounds+= Landscape->GetComponentsBoundingBox(true);
		}
		else
		{
			RawMeshes.RemoveAt(RawMeshId);
		}
	}

	if (RawMeshes.Num() == 0)
	{
		return;
	}
	
	//For each raw mesh, re-map the material indices according to the MaterialMap
	for (int32 RawMeshIndex = 0; RawMeshIndex < RawMeshes.Num(); ++RawMeshIndex)
	{
		FRawMesh& RawMesh = RawMeshes[RawMeshIndex];
		const TArray<int32>& Map = *MaterialMap.Find(RawMeshIndex);
		int32 NumFaceMaterials = RawMesh.FaceMaterialIndices.Num();

		for (int32 FaceMaterialIndex = 0; FaceMaterialIndex < NumFaceMaterials; ++FaceMaterialIndex)
		{
			int32 LocalMaterialIndex = RawMesh.FaceMaterialIndices[FaceMaterialIndex];
			int32 GlobalIndex = Map[LocalMaterialIndex];

			//Assign the new material index to the raw mesh
			RawMesh.FaceMaterialIndices[FaceMaterialIndex] = GlobalIndex;
		}
	}

	//
	// Build proxy mesh
	//
	FRawMesh								ProxyRawMesh;
	MaterialExportUtils::FFlattenMaterial	ProxyFlattenMaterial;
	
	MeshMerging->BuildProxy(RawMeshes, UniqueMaterials, InProxySettings, ProxyRawMesh, ProxyFlattenMaterial);

	//Transform the proxy mesh
	OutProxyLocation = ProxyBounds.GetCenter();
	for(FVector& Vertex : ProxyRawMesh.VertexPositions)
	{
		Vertex-= OutProxyLocation;
	}
	
	//
	//Create New Package for the Proxy mesh
	//
	FString PackageName = ProxyPackageName;
	FString AssetName = TEXT("ProxyMesh");
	if (PackageName.IsEmpty())
	{
		// Proxy package will be stored in Content folder
		PackageName = FPaths::GameContentDir() + AssetName;
	}
	else
	{
		AssetName = FPackageName::GetShortName(PackageName);
	}

	UPackage* Package = CreatePackage(NULL, *PackageName);

	check(Package);
	Package->FullyLoad();
	Package->Modify();

	// Construct proxy material
	UMaterial* ProxyMaterial = MaterialExportUtils::CreateMaterial(ProxyFlattenMaterial, Package, AssetName, RF_Public|RF_Standalone);
	
	// Construct proxy static mesh
	FName StaticMeshName = MakeUniqueObjectName(Package, UStaticMesh::StaticClass(), *AssetName);
	UStaticMesh* StaticMesh = new(Package, StaticMeshName, RF_Public|RF_Standalone) UStaticMesh(FPostConstructInitializeProperties());
	StaticMesh->InitResources();
	{
		FString OutputPath = StaticMesh->GetPathName();

		// make sure it has a new lighting guid
		StaticMesh->LightingGuid = FGuid::NewGuid();

		// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoordindex exists for all LODs, etc).
		StaticMesh->LightMapResolution = 32;
		StaticMesh->LightMapCoordinateIndex = 1;

		FStaticMeshSourceModel* SrcModel = new (StaticMesh->SourceModels) FStaticMeshSourceModel();
		/*Don't allow the engine to recalculate normals*/
		SrcModel->BuildSettings.bRecomputeNormals = false;
		SrcModel->BuildSettings.bRecomputeTangents = false;
		SrcModel->BuildSettings.bRemoveDegenerates = false;
		SrcModel->BuildSettings.bUseFullPrecisionUVs = false;
		SrcModel->RawMeshBulkData->SaveRawMesh(ProxyRawMesh);

		//Assign the proxy material to the static mesh
		StaticMesh->Materials.Add(ProxyMaterial);

		StaticMesh->Build();
		StaticMesh->PostEditChange();
	}

	OutAssetsToSync.Add(ProxyMaterial);
	OutAssetsToSync.Add(StaticMesh);

#if 0 // dump flattened materials as texture assets
	for (const auto& FlatMat : UniqueMaterials)
	{
		if (FlatMat.DiffuseSamples.Num() > 1)
		{
			FString DiffuseTextureName = MakeUniqueObjectName(Package, UTexture2D::StaticClass(), TEXT("FlattenMaterial_Diffuse")).ToString();
			FCreateTexture2DParameters TexParams;
			TexParams.bUseAlpha = false;
			TexParams.CompressionSettings = TC_Default;
			TexParams.bDeferCompression = false;
			TexParams.bSRGB = false;

			UTexture2D* DiffuseTexture = FImageUtils::CreateTexture2D(
				FlatMat.DiffuseSize.X, 
				FlatMat.DiffuseSize.Y,
				FlatMat.DiffuseSamples,
				Package,
				DiffuseTextureName,
				RF_Public|RF_Standalone, 
				TexParams);

			OutAssetsToSync.Add(DiffuseTexture);
		}
	}
#endif
}

bool FMeshUtilities::ConstructRawMesh(
	UStaticMeshComponent* InMeshComponent, 
	FRawMesh& OutRawMesh, 
	TArray<UMaterialInterface*>& OutUniqueMaterials,
	TArray<int32>& OutGlobalMaterialIndices) const
{
	UStaticMesh* SrcMesh = InMeshComponent->StaticMesh;
	if (SrcMesh == NULL)
	{
		UE_LOG(LogMeshUtilities, Warning, TEXT("No static mesh actor found in component %s."), *InMeshComponent->GetName());
		return false;
	}
	
	if (SrcMesh->SourceModels.Num() < 1)
	{
		UE_LOG(LogMeshUtilities, Warning, TEXT("No base render mesh found for %s."), *SrcMesh->GetName());
		return false;
	}

	//Always access the base mesh
	FStaticMeshSourceModel& SrcModel = SrcMesh->SourceModels[0];
	if (SrcModel.RawMeshBulkData->IsEmpty())
	{
		UE_LOG(LogMeshUtilities, Error, TEXT("Base render mesh has no imported raw mesh data %s."), *SrcMesh->GetName());
		return false;
	}

	SrcModel.RawMeshBulkData->LoadRawMesh(OutRawMesh);

	// Make sure the raw mesh is not irreparably malformed.
	if (!OutRawMesh.IsValidOrFixable())
	{
		UE_LOG(LogMeshUtilities, Error, TEXT("Raw mesh (%s) is corrupt for LOD%d."), *SrcMesh->GetName(), 1);
		return false;
	}

	//Transform the raw mesh to world space
	FTransform CtoM = InMeshComponent->ComponentToWorld;
	FMatrix InvTransCToM = CtoM.ToMatrixWithScale().Inverse().GetTransposed();

	for (FVector& Vertex : OutRawMesh.VertexPositions)
	{
		Vertex = CtoM.TransformFVector4(Vertex);
	}

	int32 NumWedges = OutRawMesh.WedgeIndices.Num();
	/* Always Recalculate normals, tangents and bitangents */
	OutRawMesh.WedgeTangentZ.Empty(NumWedges);
	OutRawMesh.WedgeTangentZ.AddZeroed(NumWedges);
	OutRawMesh.WedgeTangentX.Empty(NumWedges);
	OutRawMesh.WedgeTangentX.AddZeroed(NumWedges);
	OutRawMesh.WedgeTangentY.Empty(NumWedges);
	OutRawMesh.WedgeTangentY.AddZeroed(NumWedges);

	TMultiMap<int32,int32> OverlappingCorners;
	FindOverlappingCorners(OverlappingCorners, OutRawMesh, 0.1f);
	ComputeTangents(OutRawMesh, OverlappingCorners, ETangentOptions::BlendOverlappingNormals);

	for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
	{
		OutRawMesh.WedgeTangentX[WedgeIndex] = InvTransCToM.TransformVector(OutRawMesh.WedgeTangentX[WedgeIndex]).SafeNormal();
		OutRawMesh.WedgeTangentY[WedgeIndex] = InvTransCToM.TransformVector(OutRawMesh.WedgeTangentY[WedgeIndex]).SafeNormal();
		OutRawMesh.WedgeTangentZ[WedgeIndex] = InvTransCToM.TransformVector(OutRawMesh.WedgeTangentZ[WedgeIndex]).SafeNormal();
	}

	//Need to store the unique material indices in order to re-map the material indices in each rawmesh
	//Only using the base mesh
	for (const FStaticMeshSection& Section : SrcMesh->RenderData->LODResources[0].Sections) 
	{
		// Add material and store the material ID
		UMaterialInterface* MaterialToAdd = InMeshComponent->GetMaterial(Section.MaterialIndex);
		if (MaterialToAdd != NULL)
		{
			OutGlobalMaterialIndices.Add(OutUniqueMaterials.AddUnique(MaterialToAdd));
		}
		else
		{
			OutGlobalMaterialIndices.Add(INDEX_NONE);
		}
	}
			
	return true;
}



/*------------------------------------------------------------------------------
	Mesh merging 
------------------------------------------------------------------------------*/

//
// Helper class for generating square atlas for square lightmaps
//
class FLightmapPacker
{
public:
	/**
	 * @return lightmap rect in a generated atlas, invalid Rect otherwise
	 */
	FIntRect GetPackedLightmapRect(int32 Idx) const
	{
		if (PackedLigthmapSlots.IsValidIndex(Idx))
		{
			uint32 X0 = PackedLigthmapSlots[Idx]->X;
			uint32 Y0 = PackedLigthmapSlots[Idx]->Y;
			uint32 X1 = X0 + PackedLigthmapSlots[Idx]->Width;
			uint32 Y1 = Y0 + PackedLigthmapSlots[Idx]->Height;
			return FIntRect(X0, Y0, X1, Y1);
		}
		return FIntRect();
	}

	/**
	 * @return Atlas resolution, 0 - in case atlas was not created
	 */
	uint32 GetAtlasResolution() const
	{
		return PackedLightmapAtlas ? PackedLightmapAtlas->GetWidth() : 0;
	}

	/**
	 *  Attempts to pack provided square lightmaps into single atlas                                                                   
	 */ 
	bool Pack(const TArray<uint32>& LigthmapsList)
	{
		// Calculate total ligtmaps area and sort lightmaps list by resolution
		TArray<TPair<int32, uint32>> SortedLightmaps;
		float TotalArea = 0;

		for (int32 i = 0; i < LigthmapsList.Num(); ++i)
		{
			uint32 LightmapRes = LigthmapsList[i];
			TotalArea+= FMath::Square(LightmapRes);
			SortedLightmaps.Add(TPairInitializer<int32, uint32>(i, LightmapRes));
		}
		//
		SortedLightmaps.Sort([](TPair<int32, uint32> L, TPair<int32, uint32> R) { return R.Value < L.Value; });
		
		// Try to pack, increasing atlas resolution with each step
		uint32 PackedSize = FMath::RoundUpToPowerOfTwo(FMath::Round(FMath::Sqrt(TotalArea)));
		for (int32 i = 0; i < 10; ++i) // 2 iterations should be enough >.<
		{
			PackedLightmapAtlas = new FLightmapAtlas(PackedSize);
			PackedLigthmapSlots.SetNum(LigthmapsList.Num());

			for (TPair<int32, uint32> SortedLightmap : SortedLightmaps)
			{
				const FAtlasedTextureSlot* Slot = PackedLightmapAtlas->AddLightmap(SortedLightmap.Value);
				if (Slot == nullptr)
				{
					PackedLigthmapSlots.Empty();
					PackedLightmapAtlas.Reset();
					break;
				}
			
				PackedLigthmapSlots[SortedLightmap.Key] = Slot;
			}

			if (PackedLigthmapSlots.Num() == LigthmapsList.Num())
			{
				return true;
			}

			PackedSize = FMath::RoundUpToPowerOfTwo(PackedSize + 1);
		}
		
		return false;
	}

private:
	struct FLightmapAtlas : public FSlateTextureAtlas
	{
		FLightmapAtlas(uint32 InWidth)
			: FSlateTextureAtlas(InWidth, InWidth, 0, 0)
		{}

		const FAtlasedTextureSlot* AddLightmap(uint32 InWidth)
		{
			return FindSlotForTexture(InWidth, InWidth);
		}

		virtual void ConditionalUpdateTexture() OVERRIDE {};
	};

private:
	TScopedPointer<FLightmapAtlas>			PackedLightmapAtlas;
	TArray<const FAtlasedTextureSlot*>		PackedLigthmapSlots;
};

void FMeshUtilities::MergeActors(const TArray<AActor*>& SourceActors, const FString& InPackageName, TArray<UObject*>& OutAssetsToSync, FVector& OutMergedActorLocation) const
{
	TArray<UStaticMeshComponent*> ComponentsToMerge;
	
	// Collect static mesh components
	for (AActor* Actor : SourceActors)
	{
		TArray<UStaticMeshComponent*> Components;
		Actor->GetComponents<UStaticMeshComponent>(Components);
		ComponentsToMerge.Append(Components);
	}
	
	struct FRawMeshExt
	{
		FRawMeshExt() 
			: LightMapCoordinateIndex(1)
			, LightMapRes(32)
		{}
		
		FRawMesh	Mesh;
		int32		LightMapCoordinateIndex;
		int32		LightMapRes;
		FString		AssetPackageName;
		FVector		Pivot;	
	};
	
	TArray<UMaterialInterface*>						UniqueMaterials;
	TMap<int32, TArray<int32>>						MaterialMap;
	TArray<FRawMeshExt>								SourceMeshes;
	bool											bWithVertexColors = false;
	
	// Convert collected static mesh components into raw meshes
	SourceMeshes.Empty(ComponentsToMerge.Num());

	for (UStaticMeshComponent* MeshComponent : ComponentsToMerge)
	{
		TArray<int32> MeshMaterialMap;
		int32 MeshId = SourceMeshes.Add(FRawMeshExt());

		if (ConstructRawMesh(MeshComponent, SourceMeshes[MeshId].Mesh, UniqueMaterials, MeshMaterialMap))
		{
			MaterialMap.Add(MeshId, MeshMaterialMap);

			// Store mesh lightmap info
			FIntPoint ActorLightMapRes; 
			MeshComponent->GetLightMapResolution(ActorLightMapRes.X, ActorLightMapRes.Y);
			SourceMeshes[MeshId].LightMapRes = ActorLightMapRes.X;
			SourceMeshes[MeshId].LightMapCoordinateIndex = MeshComponent->StaticMesh->LightMapCoordinateIndex;
			
			// Store component location
			SourceMeshes[MeshId].Pivot = MeshComponent->ComponentToWorld.GetLocation();
			
			// Source mesh asset package name
			SourceMeshes[MeshId].AssetPackageName = MeshComponent->StaticMesh->GetOutermost()->GetName();

			bWithVertexColors|= (SourceMeshes[MeshId].Mesh.WedgeColors.Num() != 0);
		}
		else
		{
			SourceMeshes.RemoveAt(MeshId);
		}
	}

	if (SourceMeshes.Num() == 0)
	{
		return;	
	}

	//For each raw mesh, re-map the material indices according to the MaterialMap
	for (int32 MeshIndex = 0; MeshIndex < SourceMeshes.Num(); ++MeshIndex)
	{
		FRawMesh& RawMesh = SourceMeshes[MeshIndex].Mesh;
		const TArray<int32>& Map = *MaterialMap.Find(MeshIndex);
		for (int32& FaceMaterialIndex : RawMesh.FaceMaterialIndices)
		{
			//Assign the new material index to the raw mesh
			FaceMaterialIndex = Map[FaceMaterialIndex];
		}
	}

	FRawMeshExt MergedMesh;

	// Pack lightmaps
	static const uint32 MaxLightmapRes = 2048;
	float MergedLightmapScale = 1.f;
	TArray<uint32> LightmapResList;
	for (const FRawMeshExt& SourceMesh : SourceMeshes)
	{
		LightmapResList.Add(SourceMesh.LightMapRes);
	}
							
	FLightmapPacker LightmapPacker;	
	LightmapPacker.Pack(LightmapResList);
	MergedMesh.LightMapRes = LightmapPacker.GetAtlasResolution();
	if (MergedMesh.LightMapRes > MaxLightmapRes)
	{
		MergedLightmapScale = MaxLightmapRes/(float)MergedMesh.LightMapRes;
		MergedMesh.LightMapRes = MaxLightmapRes;
	}

	// Use first mesh for naming and pivot
	MergedMesh.AssetPackageName = SourceMeshes[0].AssetPackageName;
	MergedMesh.Pivot = SourceMeshes[0].Pivot;
		
	// Merge meshes into single mesh
	for (int32 SourceMeshIdx = 0; SourceMeshIdx < SourceMeshes.Num(); ++SourceMeshIdx)
	{
		// Merge vertex data from source mesh list into single mesh
		FRawMesh& TargetRawMesh = MergedMesh.Mesh;
		const FRawMesh& SourceRawMesh = SourceMeshes[SourceMeshIdx].Mesh;
						
		TargetRawMesh.FaceSmoothingMasks.Append(SourceRawMesh.FaceSmoothingMasks);
		TargetRawMesh.FaceMaterialIndices.Append(SourceRawMesh.FaceMaterialIndices);

		int32 IndicesOffset = TargetRawMesh.VertexPositions.Num();

		for (int32 Index : SourceRawMesh.WedgeIndices)
		{
			TargetRawMesh.WedgeIndices.Add(Index + IndicesOffset);
		}

		for (FVector VertexPos : SourceRawMesh.VertexPositions)
		{
			MergedMesh.Pivot = FVector::ZeroVector; // bake vertices in world space. TODO: optional
			TargetRawMesh.VertexPositions.Add(VertexPos - MergedMesh.Pivot);
		}
						
		TargetRawMesh.WedgeTangentX.Append(SourceRawMesh.WedgeTangentX);
		TargetRawMesh.WedgeTangentY.Append(SourceRawMesh.WedgeTangentY);
		TargetRawMesh.WedgeTangentZ.Append(SourceRawMesh.WedgeTangentZ);
		
		// Deal with vertex colors
		// Some meshes may have it, in this case merged mesh will be forced to have vertex colors as well
		if (bWithVertexColors)
		{
			if (SourceRawMesh.WedgeColors.Num())
			{
				TargetRawMesh.WedgeColors.Append(SourceRawMesh.WedgeColors);
			}
			else
			{
				// In case this source mesh does not have vertex colors, fill target with 0xFF
				int32 ColorsOffset = TargetRawMesh.WedgeColors.Num();
				int32 ColorsNum = SourceRawMesh.WedgeIndices.Num();
				TargetRawMesh.WedgeColors.AddUninitialized(ColorsNum);
				FMemory::Memset(&TargetRawMesh.WedgeColors[ColorsOffset], 0xFF, ColorsNum*TargetRawMesh.WedgeColors.GetTypeSize());
			}
		}
		
		// Only first UV channel will be used 
		TargetRawMesh.WedgeTexCoords[0].Append(SourceRawMesh.WedgeTexCoords[0]);
		
		// Transform lightmap UVs
		if (MergedMesh.LightMapRes)
		{
			FIntRect	PackedLightmapRect = LightmapPacker.GetPackedLightmapRect(SourceMeshIdx);
			FVector2D	UVOffset = FVector2D(PackedLightmapRect.Min) * MergedLightmapScale / MergedMesh.LightMapRes;

			for (FVector2D LightMapUV : SourceRawMesh.WedgeTexCoords[SourceMeshes[SourceMeshIdx].LightMapCoordinateIndex])
			{
				float UVScale = SourceMeshes[SourceMeshIdx].LightMapRes*MergedLightmapScale/MergedMesh.LightMapRes;
				TargetRawMesh.WedgeTexCoords[MergedMesh.LightMapCoordinateIndex].Add(LightMapUV * UVScale + UVOffset);
			}
		}
	}
	
	//
	//Create merged mesh asset
	//
	{
		FString AssetName;
		FString PackageName;
		if (InPackageName.IsEmpty())
		{
			AssetName = FString(TEXT("MergedMesh_")) + FPackageName::GetShortName(MergedMesh.AssetPackageName);
			PackageName = FPackageName::GetLongPackagePath(MergedMesh.AssetPackageName) + AssetName;
		}
		else
		{
			AssetName = FPackageName::GetShortName(InPackageName);
			PackageName = InPackageName;
		}
		
		FName UniquePackageName = MakeUniqueObjectName(NULL, UPackage::StaticClass(), *PackageName);

		UPackage* Package = CreatePackage(NULL, *UniquePackageName.ToString());
		check(Package);
		Package->FullyLoad();
		Package->Modify();

		FName StaticMeshName = MakeUniqueObjectName(Package, UStaticMesh::StaticClass(), *AssetName);
		UStaticMesh* StaticMesh = new(Package, StaticMeshName, RF_Public|RF_Standalone) UStaticMesh(FPostConstructInitializeProperties());
		StaticMesh->InitResources();
		
		FString OutputPath = StaticMesh->GetPathName();

		// make sure it has a new lighting guid
		StaticMesh->LightingGuid = FGuid::NewGuid();

		// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoordindex exists for all LODs, etc).
		StaticMesh->LightMapResolution = MergedMesh.LightMapRes;
		StaticMesh->LightMapCoordinateIndex = MergedMesh.LightMapCoordinateIndex;

		FStaticMeshSourceModel* SrcModel = new (StaticMesh->SourceModels) FStaticMeshSourceModel();
		/*Don't allow the engine to recalculate normals*/
		SrcModel->BuildSettings.bRecomputeNormals = false;
		SrcModel->BuildSettings.bRecomputeTangents = false;
		SrcModel->BuildSettings.bRemoveDegenerates = false;
		SrcModel->BuildSettings.bUseFullPrecisionUVs = false;
		SrcModel->RawMeshBulkData->SaveRawMesh(MergedMesh.Mesh);

		// Assign materials
		for (UMaterialInterface* Material : UniqueMaterials)
		{
			StaticMesh->Materials.Add(Material);
		}
		
		StaticMesh->Build();
		StaticMesh->PostEditChange();

		OutAssetsToSync.Add(StaticMesh);
		
		//
		OutMergedActorLocation = MergedMesh.Pivot;
	}
}
	

/*------------------------------------------------------------------------------
	UV Generation
------------------------------------------------------------------------------*/

#if PLATFORM_WINDOWS
#include "Windows/D3D9MeshUtils.h"
#endif // #if PLATFORM_WINDOWS

bool FMeshUtilities::GenerateUVs(
	FRawMesh& RawMesh,
	uint32 TexCoordIndex,
	float MinChartSpacingPercent,
	float BorderSpacingPercent,
	bool bUseMaxStretch,
	const TArray< int32 >* InFalseEdgeIndices,
	uint32& MaxCharts,
	float& MaxDesiredStretch,
	FText& OutError
	)
{
#if PLATFORM_WINDOWS
	FD3D9MeshUtilities D3DMeshUtils;
	return D3DMeshUtils.GenerateUVs(RawMesh, TexCoordIndex, MinChartSpacingPercent, BorderSpacingPercent, bUseMaxStretch, InFalseEdgeIndices, MaxCharts, MaxDesiredStretch, OutError);
#else
	return false;
#endif // #if PLATFORM_WINDOWS

}

bool FMeshUtilities::LayoutUVs(FRawMesh& RawMesh, uint32 TextureResolution, uint32 TexCoordIndex, FText& OutError)
{
#if PLATFORM_WINDOWS
	FD3D9MeshUtilities D3DMeshUtils;
	return D3DMeshUtils.LayoutUVs(RawMesh, TextureResolution, TexCoordIndex, OutError);
#else
	return false;
#endif // #if PLATFORM_WINDOWS
}

/*------------------------------------------------------------------------------
	Mesh reduction.
------------------------------------------------------------------------------*/

IMeshReduction* FMeshUtilities::GetMeshReductionInterface()
{
	return MeshReduction;
}

/*------------------------------------------------------------------------------
	Mesh merging.
------------------------------------------------------------------------------*/
IMeshMerging* FMeshUtilities::GetMeshMergingInterface()
{
	return MeshMerging;
}

/*------------------------------------------------------------------------------
	Module initialization / teardown.
------------------------------------------------------------------------------*/

void FMeshUtilities::StartupModule()
{
	check(MeshReduction == NULL);
	check(MeshMerging == NULL);

	// Look for a mesh reduction module.
	{
		TArray<FName> ModuleNames;
		FModuleManager::Get().FindModules(TEXT("*MeshReduction"), ModuleNames);

		if (ModuleNames.Num())
		{
			for (int32 Index = 0; Index < ModuleNames.Num(); Index++)
			{
				IMeshReductionModule& MeshReductionModule = FModuleManager::LoadModuleChecked<IMeshReductionModule>(ModuleNames[Index]);
								
				// Look for MeshReduction interface
				if (MeshReduction == NULL)
				{
					MeshReduction = MeshReductionModule.GetMeshReductionInterface();
					if (MeshReduction)
					{
						UE_LOG(LogMeshUtilities,Log,TEXT("Using %s for automatic mesh reduction"),*ModuleNames[Index].ToString());
					}
				}

				// Look for MeshMerging interface
				if (MeshMerging == NULL)
				{
					MeshMerging = MeshReductionModule.GetMeshMergingInterface();
					if (MeshMerging)
					{
						UE_LOG(LogMeshUtilities,Log,TEXT("Using %s for automatic mesh merging"),*ModuleNames[Index].ToString());
					}
				}

				// Break early if both interfaces were found
				if (MeshReduction && MeshMerging)
				{
					break;
				}
			}
		}

		if (!MeshReduction)
		{
			UE_LOG(LogMeshUtilities,Log,TEXT("No automatic mesh reduction module available"));
		}

		if (!MeshMerging)
		{
			UE_LOG(LogMeshUtilities,Log,TEXT("No automatic mesh merging module available"));
		}
	}

	TConsoleVariableData<int32>* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.TriangleOrderOptimization"));
	bUsingNvTriStrip = (CVar->GetValueOnGameThread() == 0);

	// Construct and cache the version string for the mesh utilities module.
	VersionString = FString::Printf(
		TEXT("%s%s%s"),
		MESH_UTILITIES_VER,
		MeshReduction ? *MeshReduction->GetVersionString() : TEXT(""),
		bUsingNvTriStrip ? TEXT("_NvTriStrip") : TEXT("")
		);
	bUsingSimplygon = VersionString.Contains(TEXT("Simplygon"));
}

void FMeshUtilities::ShutdownModule()
{
	MeshReduction = NULL;
	MeshMerging = NULL;
	VersionString.Empty();
}
