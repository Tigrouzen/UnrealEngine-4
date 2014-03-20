// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ChunkedArray.h: Chunked array definition.
=============================================================================*/

#pragma once

#include "Array.h"

/** An array that uses multiple allocations to avoid allocation failure due to fragmentation. */
template<typename ElementType, uint32 TargetBytesPerChunk = 16384 >
class TChunkedArray
{
public:

	/** Initialization constructor. */
	TChunkedArray(int32 InNumElements = 0):
		NumElements(InNumElements)
	{
		// Compute the number of chunks needed.
		const int32 NumChunks = (NumElements + NumElementsPerChunk - 1) / NumElementsPerChunk;

		// Allocate the chunks.
		Chunks.Empty(NumChunks);
		for(int32 ChunkIndex = 0;ChunkIndex < NumChunks;ChunkIndex++)
		{
			new(Chunks) FChunk;
		}
	}

#if PLATFORM_COMPILER_HAS_RVALUE_REFERENCES

private:
	template <typename ArrayType>
	FORCEINLINE static typename TEnableIf<TContainerTraits<ArrayType>::MoveWillEmptyContainer>::Type MoveOrCopy(ArrayType& ToArray, ArrayType& FromArray)
	{
		ToArray.Chunks      = (ChunksType&&)FromArray.Chunks;
		ToArray.NumElements = FromArray.NumElements;
		FromArray.NumElements = 0;
	}

	template <typename ArrayType>
	FORCEINLINE static typename TEnableIf<!TContainerTraits<ArrayType>::MoveWillEmptyContainer>::Type MoveOrCopy(ArrayType& ToArray, ArrayType& FromArray)
	{
		ToArray = FromArray;
	}

public:
	TChunkedArray(TChunkedArray&& Other)
	{
		MoveOrCopy(*this, Other);
	}

	TChunkedArray& operator=(TChunkedArray&& Other)
	{
		if (this != &Other)
		{
			MoveOrCopy(*this, Other);
		}

		return *this;
	}

#endif

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS

	TChunkedArray(const TChunkedArray&) = default;
	TChunkedArray& operator=(const TChunkedArray&) = default;

#else

	FORCEINLINE TChunkedArray(const TChunkedArray& Other)
		: Chunks     (Other.Chunks)
		, NumElements(Other.NumElements)
	{
	}

	FORCEINLINE TChunkedArray& operator=(const TChunkedArray& Other)
	{
		Chunks      = Other.Chunks;
		NumElements = Other.NumElements;

		return *this;
	}

#endif

	// Accessors.
	ElementType& operator()(int32 ElementIndex)
	{
		const uint32 ChunkIndex = ElementIndex / NumElementsPerChunk;
		const uint32 ChunkElementIndex = ElementIndex % NumElementsPerChunk;
		return Chunks[ChunkIndex].Elements[ChunkElementIndex];
	}
	const ElementType& operator()(int32 ElementIndex) const
	{
		const int32 ChunkIndex = ElementIndex / NumElementsPerChunk;
		const int32 ChunkElementIndex = ElementIndex % NumElementsPerChunk;
		return Chunks[ChunkIndex].Elements[ChunkElementIndex];
	}
	ElementType& operator[](int32 ElementIndex)
	{
		const uint32 ChunkIndex = ElementIndex / NumElementsPerChunk;
		const uint32 ChunkElementIndex = ElementIndex % NumElementsPerChunk;
		return Chunks[ChunkIndex].Elements[ChunkElementIndex];
	}
	const ElementType& operator[](int32 ElementIndex) const
	{
		const int32 ChunkIndex = ElementIndex / NumElementsPerChunk;
		const int32 ChunkElementIndex = ElementIndex % NumElementsPerChunk;
		return Chunks[ChunkIndex].Elements[ChunkElementIndex];
	}
	int32 Num() const 
	{ 
		return NumElements; 
	}

	uint32 GetAllocatedSize( void ) const
	{
		return Chunks.GetAllocatedSize();
	}

	/**
	 * Adds a new item to the end of the chunked array.
	 *
	 * @param Item	The item to add
	 * @return		Index to the new item
	 */
	int32 AddElement( const ElementType& Item )
	{
		new(*this) ElementType(Item);
		return this->NumElements - 1;
	}

	int32 Add( int32 Count=1 )
	{
		check(Count>=0);
		checkSlow(NumElements>=0);

		const int32 OldNum = NumElements;
		for (int32 i = 0; i < Count; i++)
		{
			if (NumElements % NumElementsPerChunk == 0)
			{
				new(Chunks) FChunk;
			}
			NumElements++;
		}
		return OldNum;
	}

	void Empty( int32 Slack=0 ) 
	{
		Chunks.Empty(Slack % NumElementsPerChunk + 1);
		NumElements = 0;
	}

	void Shrink()
	{
		Chunks.Shrink();
	}

private:
	enum { NumElementsPerChunk = TargetBytesPerChunk / sizeof(ElementType) };

	/** A chunk of the array's elements. */
	struct FChunk
	{
		/** The elements in the chunk. */
		ElementType Elements[NumElementsPerChunk];
	};

	/** The chunks of the array's elements. */
	typedef TIndirectArray<FChunk> ChunksType;
	ChunksType Chunks;

	/** The number of elements in the array. */
	int32 NumElements;
};

template <typename ElementType, uint32 TargetBytesPerChunk>
struct TContainerTraits<TChunkedArray<ElementType, TargetBytesPerChunk> > : public TContainerTraitsBase<TChunkedArray<ElementType, TargetBytesPerChunk> >
{
	enum { MoveWillEmptyContainer =
		PLATFORM_COMPILER_HAS_RVALUE_REFERENCES &&
		TContainerTraits<typename TChunkedArray<ElementType, TargetBytesPerChunk>::ChunksType>::MoveWillEmptyContainer };
};

template <typename T,uint32 TargetBytesPerChunk> void* operator new( size_t Size, TChunkedArray<T,TargetBytesPerChunk>& ChunkedArray )
{
	check(Size == sizeof(T));
	const int32 Index = ChunkedArray.Add(1);
	return &ChunkedArray(Index);
}