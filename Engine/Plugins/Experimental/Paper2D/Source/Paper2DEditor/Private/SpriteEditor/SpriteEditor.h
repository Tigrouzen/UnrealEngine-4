// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/AssetEditorManager.h"

//////////////////////////////////////////////////////////////////////////
// FSpriteEditor

class FSpriteEditor : public FAssetEditorToolkit, public FGCObject
{
public:
	// IToolkit interface
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) OVERRIDE;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) OVERRIDE;
	// End of IToolkit interface

	// FAssetEditorToolkit
	virtual FName GetToolkitFName() const OVERRIDE;
	virtual FText GetBaseToolkitName() const OVERRIDE;
	virtual FText GetToolkitName() const OVERRIDE;
	virtual FLinearColor GetWorldCentricTabColorScale() const OVERRIDE;
	virtual FString GetWorldCentricTabPrefix() const OVERRIDE;
	// End of FAssetEditorToolkit

	// FSerializableObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) OVERRIDE;
	// End of FSerializableObject interface

	// Get the source texture for the current sprite being edited
	UTexture2D* GetSourceTexture() const;
public:
	void InitSpriteEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UPaperSprite* InitSprite);

	UPaperSprite* GetSpriteBeingEdited() const { return SpriteBeingEdited; }
	void SetSpriteBeingEdited(UPaperSprite* NewSprite);
protected:
	UPaperSprite* SpriteBeingEdited;
	TSharedPtr<class SSpriteEditorViewport> ViewportPtr;

protected:
	void BindCommands();
	void ExtendMenu();
	void ExtendToolbar();

	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SpriteList(const FSpawnTabArgs& Args);
};