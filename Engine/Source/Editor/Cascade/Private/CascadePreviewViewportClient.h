// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once


class FCascade;
class SCascadePreviewViewport;


/*-----------------------------------------------------------------------------
   FCascadeViewportClient
-----------------------------------------------------------------------------*/

class FCascadeEdPreviewViewportClient : public FEditorViewportClient
{
public:
	/** Constructor */
	FCascadeEdPreviewViewportClient(TWeakPtr<FCascade> InCascade, TWeakPtr<SCascadePreviewViewport> InCascadeViewport);
	~FCascadeEdPreviewViewportClient();

	/** FEditorViewportClient interface */
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas) OVERRIDE;
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) OVERRIDE;
	virtual bool InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed = 1.0f, bool bGamepad = false) OVERRIDE;
	virtual bool InputAxis(FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples = 1, bool bGamepad = false) OVERRIDE;
	virtual FSceneInterface* GetScene() const OVERRIDE;
	virtual FLinearColor GetBackgroundColor() const OVERRIDE;
	virtual bool ShouldOrbitCamera() const OVERRIDE;
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) OVERRIDE;
	virtual bool CanCycleWidgetMode() const OVERRIDE;

	/** Sets the position and orientation of the preview camera */
	void SetPreviewCamera(const FRotator& NewPreviewAngle, float NewPreviewDistance);

	/** Update the memory information of the particle system */
	void UpdateMemoryInformation();

	/** Generates a new thumbnail image for the content browser */
	void CreateThumbnail();

	/** Draw flag types */
	enum EDrawElements
	{
		ParticleCounts = 0x001,
		ParticleEvents = 0x002,
		ParticleTimes = 0x004,
		ParticleMemory = 0x008,
		VectorFields = 0x010,
		Bounds = 0x020,
		WireSphere = 0x040,
		OriginAxis = 0x080,
		Orbit = 0x100
	};

	/** Accessors */
	FPreviewScene& GetPreviewScene();
	bool GetDrawElement(EDrawElements Element) const;
	void ToggleDrawElement(EDrawElements Element);
	FColor GetPreviewBackgroundColor() const;
	UStaticMeshComponent* GetFloorComponent();
	FEditorCommonDrawHelper& GetDrawHelper();
	float& GetWireSphereRadius();

private:
	/** Pointer back to the ParticleSystem editor tool that owns us */
	TWeakPtr<FCascade> CascadePtr;

	/** Pointer back to the ParticleSystem viewport control that owns us */
	TWeakPtr<SCascadePreviewViewport> CascadeViewportPtr;
	
	/** Preview mesh */
	UStaticMeshComponent* FloorComponent;

	/** Camera potition/rotation */
	FRotator PreviewAngle;
	float PreviewDistance;

	/** If true, will take screenshot for thumbnail on next draw call */
	float bCaptureScreenShot;

	/** User input state info */
	FVector WorldManipulateDir;
	FVector LocalManipulateDir;
	float DragX;
	float DragY;
	EAxisList::Type WidgetAxis;
	EWidgetMovementMode WidgetMM;
	bool bManipulatingVectorField;

	/** Draw flags (see EDrawElements) */
	int32 DrawFlags;

	/** Radius of the wireframe sphere */
	float WireSphereRadius;

	/** Veiwport background color */
	FColor BackgroundColor;
	
	/** The scene used for the viewport. Owned externally */
	FPreviewScene CascadePreviewScene;

	/** The size of the ParticleSystem via FArchive memory counting */
	int32 ParticleSystemRootSize;
	/** The size the particle modules take for the system */
	int32 ParticleModuleMemSize;
	/** The size of the ParticleSystemComponent via FArchive memory counting */
	int32 PSysCompRootSize;
	/** The size of the ParticleSystemComponent resource size */
	int32 PSysCompResourceSize;

	/** Draw info index for vector fields */
	const int32 VectorFieldHitproxyInfo;

	/** Speed multiplier used when moving the scene light around */
	const float LightRotSpeed;
};

