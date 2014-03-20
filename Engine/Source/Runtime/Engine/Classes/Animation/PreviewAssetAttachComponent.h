// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/** 
 * This is a component used by USkeleton and USkeletalMesh 
 * to deal with attaching assets to sockets or bones
 *
 */

#pragma once

#include "PreviewAssetAttachComponent.generated.h"

/** Preview items that are attached to the skeleton **/
USTRUCT()
struct FPreviewAttachedObjectPair
{
	GENERATED_USTRUCT_BODY()

	/** The name of the attach point of the Object (for example a bone or socket name) */
	UPROPERTY()
	FName AttachedTo;

	/** the object to be attached */
	UPROPERTY()
	UObject* Object;
};

// Iterators
typedef TIndexedContainerIterator<      TArray<FPreviewAttachedObjectPair>,       FPreviewAttachedObjectPair, int32> TIterator;
typedef TIndexedContainerIterator<const TArray<FPreviewAttachedObjectPair>, const FPreviewAttachedObjectPair, int32> TConstIterator;

/** Component which deals with attaching assets */
USTRUCT()
struct FPreviewAssetAttachContainer
{
	GENERATED_USTRUCT_BODY()

public:
	/**
	 * Adds the given Name/Object to the PreviewAttachedObjects list. Allows us to recreate the attached objects later
	 *
	 * @param	AttachObject		The object that is being attached
	 * @param	AttachPointName		The place where the object is attached (bone or socket)
	 */
	ENGINE_API void AddAttachedObject( UObject* AttachObject, FName AttachPointName );

	/**
	 * Remove the given object from the attached list
	 *
	 * @param	ObjectToRemove		The object that is being removed
	 */
	ENGINE_API void RemoveAttachedObject( UObject* ObjectToRemove, FName AttachName );

	/**
	 * Grab the asset (if any) attached at the place given
	 *
	 * @param	AttachName		The name of the attach point (Bone name or socket name) to look for an asset
	 * @return					UObject or NULL if nothing is attached at that location.
	 */
	ENGINE_API UObject* GetAttachedObjectByAttachName( FName AttachName ) const;

	/**
	 * Clears all the preview attached objects
	 */
	ENGINE_API void ClearAllAttachedObjects();

	/**
	 * Returns the number of attached objects
	 */
	ENGINE_API int32 Num() const;

	/** 
	 * operator [] - pipes through to AttachedObjects
	 */
	ENGINE_API FPreviewAttachedObjectPair& operator[]( int32 i );

	/**
	 * Const iterator creator for AttachedObjects
	 */
	ENGINE_API TConstIterator CreateConstIterator() const;

	/**
	 * Iterator creator for AttachedObjects
	 */
	ENGINE_API TIterator CreateIterator();

	/** 
	 * RemoveAtSwap passthrough
	 */
	ENGINE_API void RemoveAtSwap( int32 Index, int32 Count = 1, bool bAllowShrinking = true );

private:
	UPROPERTY()
	TArray<FPreviewAttachedObjectPair> AttachedObjects;
};