// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Texture mode module
 */
class FTextureAlignModeModule : public IModuleInterface
{
private:
	TSharedPtr<class FEdModeTexture> EdModeTexture;
public:
	/**
	 * Called right after the module's DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule() OVERRIDE;

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule() OVERRIDE;};

/**
 * Allows texture alignment on BSP surfaces via the widget.
 */
class FEdModeTexture : public FEdMode
{
public:
	FEdModeTexture();
	virtual ~FEdModeTexture();

	/** Stores the coordinate system that was active when the mode was entered so it can restore it later. */
	ECoordSystem SaveCoordSystem;

	virtual void Enter();
	virtual void Exit();
	virtual FVector GetWidgetLocation() const;
	virtual bool ShouldDrawWidget() const;
	virtual bool GetCustomDrawingCoordinateSystem( FMatrix& InMatrix, void* InData );
	virtual bool GetCustomInputCoordinateSystem( FMatrix& InMatrix, void* InData );
	virtual EAxisList::Type GetWidgetAxisToDraw( FWidget::EWidgetMode InWidgetMode ) const;
	virtual bool StartTracking(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport);
	virtual bool EndTracking(FLevelEditorViewportClient* InViewportClient, FViewport* InViewport);
	virtual bool AllowWidgetMove() { return false; }

protected:
	/** The current transaction. */
	FScopedTransaction*		ScopedTransaction;
	/* The world that brush that we started tracking with belongs to. Cleared when tracking ends. */
	UWorld*		TrackingWorld;
};

/**
 * FModeTool_Texture
 */
class FModeTool_Texture : public FModeTool
{
public:
	FModeTool_Texture();

	virtual bool InputDelta(FLevelEditorViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);

	// override these to allow this tool to keep track of the users dragging during a single drag event
	virtual bool StartModify()	{ PreviousInputDrag = FVector::ZeroVector; return true; }
	virtual bool EndModify()	{ return true; }

private:
	FVector PreviousInputDrag;
};