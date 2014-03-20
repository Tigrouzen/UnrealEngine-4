// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.



#pragma once

/**
 * Unreal StaticMesh editor actions
 */
class FStaticMeshEditorCommands : public TCommands<FStaticMeshEditorCommands>
{

public:
	FStaticMeshEditorCommands() : TCommands<FStaticMeshEditorCommands>
	(
		"StaticMeshEditor", // Context name for fast lookup
		NSLOCTEXT("Contexts", "StaticMeshEditor", "StaticMesh Editor"), // Localized context name for displaying
		"EditorViewport",  // Parent
		FEditorStyle::GetStyleSetName() // Icon Style Set
	)
	{
	}
	
	/**
	 * StaticMesh Editor Commands
	 */
	
	/**  */
	TSharedPtr< FUICommandInfo > SetShowWireframe;
	TSharedPtr< FUICommandInfo > SetShowVertexColor;
	TSharedPtr< FUICommandInfo > SetDrawUVs;
	TSharedPtr< FUICommandInfo > SetShowGrid;
	TSharedPtr< FUICommandInfo > SetShowBounds;
	TSharedPtr< FUICommandInfo > SetShowCollision;
	TSharedPtr< FUICommandInfo > ResetCamera;
	TSharedPtr< FUICommandInfo > SetShowSockets;
	TSharedPtr< FUICommandInfo > SetDrawAdditionalData;

	// View Menu Commands
	TSharedPtr< FUICommandInfo > SetShowNormals;
	TSharedPtr< FUICommandInfo > SetShowTangents;
	TSharedPtr< FUICommandInfo > SetShowBinormals;
	TSharedPtr< FUICommandInfo > SetShowPivot;

	// Collision Menu Commands
	TSharedPtr< FUICommandInfo > CreateDOP6;
	TSharedPtr< FUICommandInfo > CreateDOP10X;
	TSharedPtr< FUICommandInfo > CreateDOP10Y;
	TSharedPtr< FUICommandInfo > CreateDOP10Z;
	TSharedPtr< FUICommandInfo > CreateDOP18;
	TSharedPtr< FUICommandInfo > CreateDOP26;
	TSharedPtr< FUICommandInfo > CreateSphereCollision;
	TSharedPtr< FUICommandInfo > CreateAutoConvexCollision;
	TSharedPtr< FUICommandInfo > RemoveCollision;
	TSharedPtr< FUICommandInfo > ConvertBoxesToConvex;
	TSharedPtr< FUICommandInfo > CopyCollisionFromSelectedMesh;

	// Mesh Menu Commands
	TSharedPtr< FUICommandInfo > FindSource;

	TSharedPtr< FUICommandInfo > GenerateUniqueUVs;

	TSharedPtr< FUICommandInfo > ChangeMesh;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() OVERRIDE;

public:
};
