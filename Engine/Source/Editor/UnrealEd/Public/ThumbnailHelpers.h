// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "PreviewScene.h"

class FSceneViewFamily;

class UNREALED_API FThumbnailPreviewScene : public FPreviewScene
{
public:
	/** Constructor */
	FThumbnailPreviewScene();

	/** Allocates then adds an FSceneView to the ViewFamily. */
	void GetView(FSceneViewFamily* ViewFamily, int32 X, int32 Y, uint32 SizeX, uint32 SizeY) const;

protected:
	/** Helper function to get the bounds offset to display an asset */
	float GetBoundsZOffset(const FBoxSphereBounds& Bounds) const;

	/**
	  * Gets parameters to create a view matrix to be used by GetView(). Implemented in children classes.
	  * @param InFOVDegrees  The FOV used to display the thumbnail. Often used to calculate the output parameters.
	  * @param OutOrigin	 The origin of the orbit view. Typically the center of the bounds of the target object.
	  * @param OutOrbitPitch The pitch of the orbit cam around the object.
	  * @param OutOrbitYaw	 The yaw of the orbit cam around the object.
	  * @param OutOrbitZoom  The camera distance from the object.
	  */
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const = 0;
};

class UNREALED_API FParticleSystemThumbnailScene : public FThumbnailPreviewScene
{
public:
	/** Constructor/Destructor */
	FParticleSystemThumbnailScene();
	virtual ~FParticleSystemThumbnailScene();

	/** Sets the particle system to use in the next GetView() */
	void SetParticleSystem(class UParticleSystem* ParticleSystem);

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const OVERRIDE;

protected:
	/** The particle system component used to display all particle system thumbnails */
	class UParticleSystemComponent* PartComponent;

	/** The FXSystem used to render all thumbnail particle systems */
	class FFXSystemInterface* ThumbnailFXSystem;
};

class UNREALED_API FMaterialThumbnailScene : public FThumbnailPreviewScene
{
public:	
	/** Constructor */
	FMaterialThumbnailScene();

	/** Sets the material to use in the next GetView() */
	void SetMaterialInterface(class UMaterialInterface* InMaterial);

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const OVERRIDE;

protected:
	/** The static mesh actor used to display all material thumbnails */
	AStaticMeshActor* PreviewActor;
};

class UNREALED_API FSkeletalMeshThumbnailScene : public FThumbnailPreviewScene
{
public:
	/** Constructor */
	FSkeletalMeshThumbnailScene();

	/** Sets the skeletal mesh to use in the next GetView() */
	void SetSkeletalMesh(class USkeletalMesh* InSkeletalMesh);

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const OVERRIDE;

private:
	/** The skeletal mesh actor used to display all skeletal mesh thumbnails */
	class ASkeletalMeshActor* PreviewActor;
};

class UNREALED_API FStaticMeshThumbnailScene : public FThumbnailPreviewScene
{
public:
	/** Constructor */
	FStaticMeshThumbnailScene();

	/** Sets the static mesh to use in the next GetView() */
	void SetStaticMesh(class UStaticMesh* StaticMesh);

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const OVERRIDE;

private:
	/** The static mesh actor used to display all static mesh thumbnails */
	AStaticMeshActor* PreviewActor;
};

class UActorComponent;

class UNREALED_API FBlueprintThumbnailScene : public FThumbnailPreviewScene
{
public:
	/** Constructor/Destructor */
	FBlueprintThumbnailScene();
	~FBlueprintThumbnailScene();

	/** Sets the static mesh to use in the next GetView() */
	void SetBlueprint(class UBlueprint* Blueprint);

	/** Returns true if this component can be visualized */
	bool IsValidComponentForVisualization(UActorComponent* Component) const;

	/** Refreshes components for the specified blueprint */
	void BlueprintChanged(class UBlueprint* Blueprint);

	void AddReferencedObjects( FReferenceCollector& Collector ) OVERRIDE;

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const OVERRIDE;

	/** Returns a duplicate of the specified component whose outer is the transient package. If the component can not be created, a placeholder component is made in its place. */
	UActorComponent* CreateComponentInstanceFromTemplate(UActorComponent* ComponentTemplate) const;

	/** Creates instances of template components found in a blueprint's simple construction script */
	void InstanceComponents(USCS_Node* CurrentNode, USceneComponent* ParentComponent, const TMap<UActorComponent*, UActorComponent*>& NativeInstanceMap, TArray<UActorComponent*>& OutComponents);

	/** Get/Release for the component pool */
	TArray<UPrimitiveComponent*> GetPooledVisualizableComponents(UBlueprint* Blueprint);

	/** Handler for when garbage collection occurs. Used to clear the ComponentsPool */
	void OnPreGarbageCollect();

	/** Removes all references to components in the components pool and empties all lists. This prepares all loaded components for GC. */
	void ClearComponentsPool();

private:
	/** The blueprint that is currently being rendered. NULL when not rendering. */
	UBlueprint* CurrentBlueprint;

	/** Instances of the visualizable components found in the blueprint. This array only has elements while the blueprint is being rendered.  */
	TArray<UPrimitiveComponent*> VisualizableBlueprintComponents;

	/** A map of Blueprint to "pooled component list".
	  * This will be populated with components that were created by this preview scene.
	  * Objects in this map are not persistent and will be garbage collected at GC time.
	  */
	TMap< TWeakObjectPtr<UBlueprint>, TArray< TWeakObjectPtr<UActorComponent> > > AllComponentsPool;
	TMap< TWeakObjectPtr<UBlueprint>, TArray< TWeakObjectPtr<UPrimitiveComponent> > > VisualizableComponentsPool;
};