// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.



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
	TSharedPtr< FUICommandInfo > SetShowVertices;

	// Collision Menu Commands
	TSharedPtr< FUICommandInfo > CreateDOP10X;
	TSharedPtr< FUICommandInfo > CreateDOP10Y;
	TSharedPtr< FUICommandInfo > CreateDOP10Z;
	TSharedPtr< FUICommandInfo > CreateDOP18;
	TSharedPtr< FUICommandInfo > CreateDOP26;
	TSharedPtr< FUICommandInfo > CreateBoxCollision;
	TSharedPtr< FUICommandInfo > CreateSphereCollision;
	TSharedPtr< FUICommandInfo > CreateSphylCollision;
	TSharedPtr< FUICommandInfo > CreateAutoConvexCollision;
	TSharedPtr< FUICommandInfo > RemoveCollision;
	TSharedPtr< FUICommandInfo > ConvertBoxesToConvex;
	TSharedPtr< FUICommandInfo > CopyCollisionFromSelectedMesh;

	// Mesh Menu Commands
	TSharedPtr< FUICommandInfo > FindSource;

	TSharedPtr< FUICommandInfo > ChangeMesh;

	TSharedPtr< FUICommandInfo > SaveGeneratedLODs;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

public:
};
