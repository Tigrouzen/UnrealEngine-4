// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDocumentation.h"

#define LOCTEXT_NAMESPACE "PersonaMode"

/////////////////////////////////////////////////////

struct FPersonaTabs
{
	// Tab constants

	// Selection Details
	static const FName MorphTargetsID;
	static const FName SkeletonTreeViewID;
	// Skeleton Pose manager
	static const FName RetargetSourceManagerID;
	// Skeleton/Sockets
	// Anim Blueprint Params
	// Explorer
	// Blueprint Defaults
	static const FName AnimBlueprintDefaultsEditorID;
	// Anim Document
	static const FName ScrubberID;
	// Toolbar
	static const FName PreviewViewportID;
	static const FName AssetBrowserID;
	static const FName MirrorSetupID;
	static const FName AnimBlueprintDebugHistoryID;
	static const FName AnimAssetPropertiesID;
	static const FName MeshAssetPropertiesID;
	static const FName PreviewManagerID;
	static const FName SkeletonAnimNotifiesID;

	// Blueprint Document

	// Inherited from blueprint editor
	/*
	static const FName Toolbar;
	static const FName Explorer;
	static const FName VariableList;
	static const FName Inspector;
	etc...
	*/

private:
	FPersonaTabs() {}
};

/////////////////////////////////////////////////////

// This is the list of IDs for persona modes
struct FPersonaModes
{
	// Mode constants
	static const FName SkeletonDisplayMode;
	static const FName MeshEditMode;
	static const FName PhysicsEditMode;
	static const FName AnimationEditMode;
	static const FName AnimBlueprintEditMode;
	static FText GetLocalizedMode( const FName InMode )
	{
		static TMap< FName, FText > LocModes;

		if (LocModes.Num() == 0)
		{
			LocModes.Add( SkeletonDisplayMode, NSLOCTEXT("PersonaModes", "SkeletonDisplayMode", "Skeleton") );
			LocModes.Add( MeshEditMode, NSLOCTEXT("PersonaModes", "MeshEditMode", "Mesh") );
			LocModes.Add( PhysicsEditMode, NSLOCTEXT("PersonaModes", "PhysicsEditMode", "Physics") );
			LocModes.Add( AnimationEditMode, NSLOCTEXT("PersonaModes", "AnimationEditMode", "Animation") );
			LocModes.Add( AnimBlueprintEditMode, NSLOCTEXT("PersonaModes", "AnimBlueprintEditMode", "Graph") );
		}

		check( InMode != NAME_None );
		const FText* OutDesc = LocModes.Find( InMode );
		check( OutDesc );
		return *OutDesc;
	}
private:
	FPersonaModes() {}
};

/////////////////////////////////////////////////////
// FPersonaModeSharedData

struct FPersonaModeSharedData 
{
	FPersonaModeSharedData();

	// camera setup
	FVector				ViewLocation;
	FRotator			ViewRotation;
	float				OrthoZoom;
	
	// orbit setup
	FVector				OrbitZoom;
	FVector				LookAtLocation;
	bool				bCameraLock;
	bool				bCameraFollow;

	// show flags
	bool				bShowReferencePose;
	bool				bShowBones;
	bool				bShowBoneNames;
	bool				bShowSockets;
	bool				bShowBound;

	// viewport setup
	int32				ViewportType;
	int32				PlaybackSpeedMode;
	int32				LocalAxesMode;

	// Playback state
	FSingleAnimationPlayData PlaybackData;
};

/////////////////////////////////////////////////////
// FPersonaAppMode

class FPersonaAppMode : public FApplicationMode
{
protected:
	FPersonaAppMode(TSharedPtr<class FPersona> InPersona, FName InModeName);

public:
	// FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) OVERRIDE;
	// End of FApplicationMode interface

protected:
	TWeakPtr<class FPersona> MyPersona;

	// Set of spawnable tabs in persona mode (@TODO: Multiple lists!)
	FWorkflowAllowedTabSet PersonaTabFactories;
};

/////////////////////////////////////////////////////
// FSkeletonTreeSummoner

struct FSkeletonTreeSummoner : public FWorkflowTabFactory
{
public:
	FSkeletonTreeSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);
	
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const OVERRIDE;

	// Create a tooltip widget for the tab
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const OVERRIDE
	{
		return  IDocumentation::Get()->CreateToolTip(LOCTEXT("SkeletonTreeTooltip", "The Skeleton Tree tab lets you see and select bones (and sockets) in the skeleton hierarchy."), NULL, TEXT("Shared/Editors/Persona"), TEXT("SkeletonTree_Window"));
	}
};

/////////////////////////////////////////////////////
// FMorphTargetTabSummoner

struct FMorphTargetTabSummoner : public FWorkflowTabFactory
{
public:
	FMorphTargetTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const OVERRIDE;

	// Create a tooltip widget for the tab
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const OVERRIDE
	{
		return  IDocumentation::Get()->CreateToolTip(LOCTEXT("MorphTargetTooltip", "The Morph Target tab lets you preview any morph targets (aka blend shapes) available for the current mesh."), NULL, TEXT("Shared/Editors/Persona"), TEXT("MorphTarget_Window"));
	}
};


/////////////////////////////////////////////////////
// FAnimationAssetBrowserSummoner

struct FAnimationAssetBrowserSummoner : public FWorkflowTabFactory
{
	FAnimationAssetBrowserSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const OVERRIDE;

	// Create a tooltip widget for the tab
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const OVERRIDE
	{
		return  IDocumentation::Get()->CreateToolTip(LOCTEXT("AnimAssetBrowserTooltip", "The Asset Browser lets you browse all animation-related assets (animations, blend spaces etc)."), NULL, TEXT("Shared/Editors/Persona"), TEXT("AssetBrowser_Window"));
	}
};

/////////////////////////////////////////////////////
// FPreviewViewportSummoner

struct FPreviewViewportSummoner : public FWorkflowTabFactory
{
	FPreviewViewportSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const OVERRIDE;
};

/////////////////////////////////////////////////////
// FRetargetSourceManagerTabSummoner

struct FRetargetSourceManagerTabSummoner : public FWorkflowTabFactory
{
public:
	FRetargetSourceManagerTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const OVERRIDE;

	// Create a tooltip widget for the tab
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const OVERRIDE
	{
		return  IDocumentation::Get()->CreateToolTip(LOCTEXT("RetargetSourceTooltip", "In this panel, you can manage retarget sources for different body types"), NULL, TEXT("Shared/Editors/Persona"), TEXT("RetargetSource_Window"));
	}
};

/////////////////////////////////////////////////////
// FAnimBlueprintDefaultsEditorSummoner

namespace EAnimBlueprintEditorMode
{
	enum Type
	{
		PreviewMode,
		DefaultsMode
	};
}

struct FAnimBlueprintDefaultsEditorSummoner : public FWorkflowTabFactory
{
public:
	FAnimBlueprintDefaultsEditorSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const OVERRIDE;

	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const OVERRIDE;

private:
	/** Delegates to customize tab look based on selected mode */
	EVisibility IsEditorVisible(EAnimBlueprintEditorMode::Type Mode) const;
	ESlateCheckBoxState::Type IsChecked(EAnimBlueprintEditorMode::Type Mode) const;
	const FSlateBrush* GetBorderBrushByMode(EAnimBlueprintEditorMode::Type Mode) const;

	/** Handle changing of editor mode */
	void OnCheckedChanged(ESlateCheckBoxState::Type NewType, EAnimBlueprintEditorMode::Type Mode);
	EAnimBlueprintEditorMode::Type CurrentMode;
};
#undef LOCTEXT_NAMESPACE
