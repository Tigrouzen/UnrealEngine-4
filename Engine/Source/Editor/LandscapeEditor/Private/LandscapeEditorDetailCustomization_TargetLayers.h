// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LandscapeEditorDetailCustomization_Base.h"


/**
 * Slate widgets customizer for the target layers list in the Landscape Editor
 */

class FLandscapeEditorDetailCustomization_TargetLayers : public FLandscapeEditorDetailCustomization_Base
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) OVERRIDE;

protected:
	static bool ShouldShowTargetLayers();
};

class FLandscapeEditorCustomNodeBuilder_TargetLayers : public IDetailCustomNodeBuilder
{
public:
	FLandscapeEditorCustomNodeBuilder_TargetLayers(TSharedRef<FAssetThumbnailPool> ThumbnailPool);
	~FLandscapeEditorCustomNodeBuilder_TargetLayers();

	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRegenerateChildren ) OVERRIDE;
	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) OVERRIDE;
	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) OVERRIDE;
	virtual void Tick( float DeltaTime ) OVERRIDE {}
	virtual bool RequiresTick() const OVERRIDE { return false; }
	virtual bool InitiallyCollapsed() const OVERRIDE { return false; }
	virtual FName GetName() const OVERRIDE { return "TargetLayers"; }

protected:
	TSharedRef<FAssetThumbnailPool> ThumbnailPool;

	static class FEdModeLandscape* GetEditorMode();

	void GenerateRow(IDetailChildrenBuilder& ChildrenBuilder, const TSharedRef<FLandscapeTargetListInfo> Target);

	static bool GetTargetLayerIsSelected(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnTargetSelectionChanged(const TSharedRef<FLandscapeTargetListInfo> Target);
	static TSharedPtr<SWidget> OnTargetLayerContextMenuOpening(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnExportLayer(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnImportLayer(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnReimportLayer(const TSharedRef<FLandscapeTargetListInfo> Target);
	static bool ShouldFilterLayerInfo(const class FAssetData& AssetData, FName LayerName);
	static void OnTargetLayerSetObject(const UObject* Object, const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetTargetLayerInfoSelectorVisibility(const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetTargetLayerCreateVisibility(const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetTargetLayerMakePublicVisibility(const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetTargetLayerDeleteVisibility(const TSharedRef<FLandscapeTargetListInfo> Target);
	static TSharedRef<SWidget> OnGetTargetLayerCreateMenu(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnTargetLayerCreateClicked(const TSharedRef<FLandscapeTargetListInfo> Target, bool bNoWeightBlend);
	static FReply OnTargetLayerMakePublicClicked(const TSharedRef<FLandscapeTargetListInfo> Target);
	static FReply OnTargetLayerDeleteClicked(const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetDebugModeColorChannelVisibility(const TSharedRef<FLandscapeTargetListInfo> Target);
	static ESlateCheckBoxState::Type DebugModeColorChannelIsChecked(const TSharedRef<FLandscapeTargetListInfo> Target, int32 Channel);
	static void OnDebugModeColorChannelChanged(ESlateCheckBoxState::Type NewCheckedState, const TSharedRef<FLandscapeTargetListInfo> Target, int32 Channel);
};

class SLandscapeEditorSelectableBorder : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SLandscapeEditorSelectableBorder)
		: _Content()
		, _HAlign(HAlign_Fill)
		, _VAlign(VAlign_Fill)
		, _Padding(FMargin(2.0f))
	{ }
		SLATE_DEFAULT_SLOT(FArguments, Content)

		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_ATTRIBUTE(FMargin, Padding)

		SLATE_EVENT(FOnContextMenuOpening, OnContextMenuOpening)
		SLATE_EVENT(FSimpleDelegate, OnSelected)
		SLATE_ATTRIBUTE(bool, IsSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	const FSlateBrush* GetBorder() const;

protected:
	FOnContextMenuOpening OnContextMenuOpening;
	FSimpleDelegate OnSelected;
	TAttribute<bool> IsSelected;
};