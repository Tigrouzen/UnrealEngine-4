// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IEnvironmentQueryEditor.h"
#include "Toolkits/AssetEditorToolkit.h"


/** Viewer/editor for a DataTable */
class FEnvironmentQueryEditor : public IEnvironmentQueryEditor, public FEditorUndoClient
{

public:

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) OVERRIDE;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) OVERRIDE;

	void InitEnvironmentQueryEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UEnvQuery* Script );
	FGraphPanelSelectionSet GetSelectedNodes() const;

	FEnvironmentQueryEditor();
	virtual ~FEnvironmentQueryEditor();

	// Begin IToolkit interface
	virtual FName GetToolkitFName() const OVERRIDE;
	virtual FText GetBaseToolkitName() const OVERRIDE;
	virtual FString GetWorldCentricTabPrefix() const OVERRIDE;
	virtual FLinearColor GetWorldCentricTabColorScale() const OVERRIDE;
	// End IToolkit interface

	// Begin FEditorUndoClient Interface
	virtual void	PostUndo(bool bSuccess) OVERRIDE;
	virtual void	PostRedo(bool bSuccess) OVERRIDE;
	// End of FEditorUndoClient

	// Delegates
	void SelectAllNodes();
	bool CanSelectAllNodes() const;
	void DeleteSelectedNodes();
	bool CanDeleteNodes() const;
	void DeleteSelectedDuplicatableNodes();
	void CutSelectedNodes();
	bool CanCutNodes() const;
	void CopySelectedNodes();
	bool CanCopyNodes() const;
	void PasteNodes();
	void PasteNodesHere(const FVector2D& Location);
	bool CanPasteNodes() const;
	void DuplicateNodes();
	bool CanDuplicateNodes() const;

	void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);

protected:
	/** Called when "Save" is clicked for this asset */
	virtual void SaveAsset_Execute() OVERRIDE;

private:
	/** Create widget for graph editing */
	TSharedRef<class SGraphEditor> CreateGraphEditorWidget(UEdGraph* InGraph);

	/** Creates all internal widgets for the tabs to point at */
	void CreateInternalWidgets();

	/** Called when the selection changes in the GraphEditor */
	void OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection);

	/** Spawns the tab with the update graph inside */
	TSharedRef<SDockTab> SpawnTab_UpdateGraph(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Properties(const FSpawnTabArgs& Args);
private:

	/* Query being edited */
	UEnvQuery* Query;

	/** */
	TWeakPtr<SGraphEditor> UpdateGraphEdPtr;

	/** Property View */
	TSharedPtr<class IDetailsView> DetailsView;

	/** The command list for this editor */
	TSharedPtr<FUICommandList> GraphEditorCommands;

	/**	Graph editor tab */
	static const FName EQSUpdateGraphTabId;
	static const FName EQSPropertiesTabId;
};
