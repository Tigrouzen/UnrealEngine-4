// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/BaseToolkit.h"
#include "SCompoundWidget.h"


namespace EMeshPaintColorSet
{
	enum Type
	{
		/**
		 * Paint Color.
		 */
		PaintColor,

		/**
		 * Erase Color.
		 */
		EraseColor
	};
}

namespace EMeshPaintWriteColorChannels
{
	enum Type
	{
		Red,
		Green,
		Blue,
		Alpha
	};
}

/**
 * Mode Toolkit for the Mesh Paint Mode
 */
class FMeshPaintToolKit : public FModeToolkit
{
public:
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) OVERRIDE;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) OVERRIDE;

	/** Initializes the geometry mode toolkit */
	virtual void Init(const TSharedPtr< class IToolkitHost >& InitToolkitHost);

	/** IToolkit interface */
	virtual FName GetToolkitFName() const OVERRIDE;
	virtual FText GetBaseToolkitName() const OVERRIDE;
	virtual class FEdMode* GetEditorMode() const OVERRIDE;
	virtual TSharedPtr<class SWidget> GetInlineContent() const OVERRIDE;

private:
	/** Geometry tools widget */
	TSharedPtr<class SMeshPaint> MeshPaintWidgets;
};

/**
 * Slate widgets for the Mesh Paint Mode
 */
class SMeshPaint : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SMeshPaint ){}
	SLATE_END_ARGS()

	/** SCompoundWidget functions */
	void Construct(const FArguments& InArgs, TSharedRef<FMeshPaintToolKit> InParentToolkit);

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) OVERRIDE;

protected:

	/** Gets a reference to the mesh paint editor mode if it is active */
	FEdMode* GetEditorMode() const;

	/** Returns the visibility state of vertices only property controls */
	EVisibility GetResourceTypeVerticesVisibility() const;

	/** Returns the visibility state of textures only property controls */
	EVisibility GetResourceTypeTexturesVisibility() const;	

	/** Returns the visibility state of the properties control */
	EVisibility GetVertexPaintModeVisibility(bool bColorPanel) const;

	/** Returns the visibility state of the button that pushes inst vertex colors to source mesh */
	EVisibility GetPushInstanceVertexColorsToMeshButtonVisibility() const;

	/** Returns the visibility state of the button that imports vertex colors from TGA */
	EVisibility GetImportVertexColorsVisibility() const;

	/** Returns the value for the brush radius setting */
	TOptional<float> GetBrushRadius() const;

	/** Returns the value for the brush strength setting */
	TOptional<float> GetBrushStrength() const;

	/** Returns the value for the brush fall off setting */
	TOptional<float> GetBrushFalloffAmount() const;

	/** Returns the value for the brush flow setting */
	TOptional<float> GetFlowAmount() const;

	/** Returns the value for the paint weight index */
	int32 GetPaintWeightIndex() const;

	/** Returns the value for the erase weight index */
	int32 GetEraseWeightIndex() const;

	/** Returns a string representing the memory used selected actors vertex color data */
	FString GetInstanceVertexColorsText() const;

	/** Called when the Mesh Paint Mode radio button selection is changed */
	void OntMeshPaintResourceChanged(EMeshPaintResource::Type InPaintResource);

	/** Called when the vertex paint target radio button selection is changed */
	void OntVertexPaintTargetChanged(EMeshVertexPaintTarget::Type InPaintTarget);

	/** Called when the vertex paint mode radio button selection is changed */
	void OnVertexPaintModeChanged(EMeshPaintMode::Type InPaintMode);

	/** Called when the color view radio button selection is changed */
	void OnVertexPaintColorViewModeChanged(EMeshPaintColorViewMode::Type InColorViewMode);

	/** Called when the color/erase radio button selection is changed */
	void OnVertexPaintColorSetChanged(EMeshPaintColorSet::Type InPaintColorSet);

	/** Called when the brush radius numeric entry box value is changed */
	void OnBrushRadiusChanged(float InBrushRadius);

	/** Called when the brush strength numeric entry box value is changed */
	void OnBrushStrengthChanged(float InBrushStrength);

	/** Called when the brush fall off numeric entry box value is changed */
	void OnBrushFalloffAmountChanged(float InBrushFalloffAmount);

	/** Called when the brush flow numeric entry box value is changed */
	void OnFlowAmountChanged(float InFlowAmount);

	/** Called to get the checked state of the ignore back-facing checkbox */
	ESlateCheckBoxState::Type IsIgnoreBackfaceChecked() const;

	/** Called when the ignore back face checkbox value is changed */
	void OnIgnoreBackfaceChanged(ESlateCheckBoxState::Type InCheckState);

	/** Called to get the checked state of the seam painting checkbox */
	ESlateCheckBoxState::Type IsSeamPaintingChecked() const;

	/** Called when the seam painting checkbox value is changed */
	void OnSeamPaintingChanged(ESlateCheckBoxState::Type InCheckState);
	
	/** Called to get the checked state of the enable flow checkbox */
	ESlateCheckBoxState::Type IsEnableFlowChecked() const;

	/** Called when the enable flow checkbox value is changed */
	void OnEnableFlowChanged(ESlateCheckBoxState::Type InCheckState);

	/** Called when the erase weight is modified */
	void OnEraseWeightChanged(int32 InWeightIndex);

	/** Called when the paint weight is modified */
	void OnPaintWeightChanged(int32 InWeightIndex);

	/** Called to get the checked state of the write channel check boxes */
	ESlateCheckBoxState::Type IsWriteColorChannelChecked(EMeshPaintWriteColorChannels::Type CheckBoxInfo) const;

	/** Called when the state of one of the write channel check boxes is modified */
	void OnWriteColorChannelChanged(ESlateCheckBoxState::Type InNewValue, EMeshPaintWriteColorChannels::Type CheckBoxInfo);

	/** Called when the fill button is clicked */
	FReply FillInstanceVertexColorsButtonClicked();

	/** Called when the push instance vertex colors to source mesh button is clicked */
	FReply PushInstanceVertexColorsToMeshButtonClicked();

	/** Called when the import button is clicked */
	FReply ImportVertexColorsFromTGAButtonClicked();

	/** Called when the save button is clicked */
	FReply SaveVertexPaintPackageButtonClicked();

	/** Called when the find in content browser button is clicked */
	FReply FindVertexPaintMeshInContentBrowserButtonClicked();

	/** Called when find in content browser button is clicked in texture paint*/
	FReply FindTextureInContentBrowserButtonClicked();

	/** Called when save texture button is clicked to save to package */
	FReply SaveTextureButtonClicked();

	/** Called when commit texture button is clicked to write changes to texture */
	FReply CommitTextureChangesButtonClicked();	

	/** Called when the New Texture button is clicked */
	FReply NewTextureButtonClicked();

	/** Called when the New Texture button is clicked */
	FReply DuplicateTextureButtonClicked();

	/** Called when the remove button is clicked */
	FReply RemoveInstanceVertexColorsButtonClicked();

	/** Called when the fix button is clicked */
	FReply FixInstanceVertexColorsButtonClicked();

	/** Called when the copy button is clicked */
	FReply CopyInstanceVertexColorsButtonClicked();

	/** Called when the paste button is clicked */
	FReply PasteInstanceVertexColorsButtonClicked();

	/** Called when the swap weight button is clicked */
	FReply SwapPaintAndEraseWeightButtonClicked();

	/** Returns the enabled state of the PushInstanceVertexColorsToMesh button */
	bool IsPushInstanceVertexColorsToMeshButtonEnabled() const;

	/** Returns the enabled state of the save button */
	bool IsSaveVertexPaintPackageButtonEnabled() const;

	/** Checks to see if the current vertex paint target is set to ComponentInstance */
	bool IsVertexPaintTargetComponentInstance() const;

	/** Commit all painted textures */
	void CommitPaintChanges();

	/**  Returns true if the texture has been edited */ 
	bool IsSelectedTextureDirty() const;

	/** Returns true if there are texture changes to be committed */
	bool AreThereChangesToCommit() const;

	/** Returns true if the selected texture is valid */
	bool IsSelectedTextureValid() const;

	/** Checks to see if the current selection has vertex colors */
	bool HasInstanceVertexColors() const;

	/** Checks to see if the current selection requires vertex color fixup */
	bool RequiresInstanceVertexColorsFixup() const;

	/** Checks to see if the current selection has instanced color data that can be copied */
	bool CanCopyToColourBufferCopy() const;

	/** Checks to see if there are colors in the copy buffer */
	bool CanPasteFromColourBufferCopy() const;

	/** Checks to see if the texture can be duplicated */
	bool CanCreateInstanceMaterialAndTexture() const;

	/** returns a blend weight menu widget */
	TSharedRef<SWidget> GetTotalWeightCountMenu();

	/** returns a UV channel menu widget */
	TSharedRef<SWidget> GetUVChannels();

	FString GetCurrentUVChannel() const;

	/** returns a list of texture target menu widgets */
	TSharedRef<SWidget> GetTextureTargets();

	/** returns a widget containing texture target name, details and image */
	TSharedRef<SWidget> GetTextureTargetWidget( UTexture2D* TextureData );

	/** returns a string containing currently selected texture */
	FString GetCurrentTextureTargetText( UTexture2D* TextureData, int index ) const;

	/** returns a image containing currently selected texture image */
	const FSlateBrush* GetCurrentTextureTargetImage( UTexture2D* TextureData ) const;

	/** Called when a blend weight menu entry is selected */
	void OnChangeTotalWeightCount(int32 InCount);

	/** Called when a blend weight menu entry is selected */
	void OnChangeUVChannel( int32 Channel);

	/** Called when a blend weight menu entry is selected */
	FReply OnChangeTextureTarget(TWeakObjectPtr<UTexture2D> TextureData);

	/** Returns the blend weight menu entry selection */
	FText GetTotalWeightCountSelection() const;

	/** Returns the UI string for the blend weight menu entry */
	FText GetTotalWeightCountText(int32 InCount) const;
		
	/** Saves the packages associated with passed in objects */
	bool SavePackagesForObjects( TArray<UObject*>& InObjects );

protected:

	/** Holds the current radio button choice for which color will be set when using the color picker */
	EMeshPaintColorSet::Type PaintColorSet;

	/** Determines if the options to import colors from TGA should be shown */
	bool bShowImportOptions;

private:
	/** Pointer to the MeshPaint edit mode. */
	class FEdModeMeshPaint* MeshPaintEditMode;
};
