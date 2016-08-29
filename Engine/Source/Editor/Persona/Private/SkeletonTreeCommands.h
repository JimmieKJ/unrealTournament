// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#ifndef __SkeletonTreeCommands_h_
#define __SkeletonTreeCommands_h_

/**
 * Class containing commands for skeleton tree actions
 */
class FSkeletonTreeCommands : public TCommands<FSkeletonTreeCommands>
{
public:
	FSkeletonTreeCommands() 
		: TCommands<FSkeletonTreeCommands>
		(
			TEXT("SkeletonTree"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "SkelTree", "Skeleton Tree"), // Localized context name for displaying
			NAME_None, // Parent context name.  
			FEditorStyle::GetStyleSetName() // Icon Style Set
		)
	{

	}

	/** Initialize commands */
	virtual void RegisterCommands() override;

	/** Show all bones in the tree */
	TSharedPtr< FUICommandInfo > ShowAllBones;

	/** Show only bones that are present in the current mesh */
	TSharedPtr< FUICommandInfo > ShowMeshBones;

	/** Show only bones that are present in the current LOD */
	TSharedPtr< FUICommandInfo > ShowLODBones;

	/** Show only bones that have weighted vertices */
	TSharedPtr< FUICommandInfo > ShowWeightedBones;

	/** Hide all bones */
	TSharedPtr< FUICommandInfo > HideBones;

	/** Add a socket to the skeleton */
	TSharedPtr< FUICommandInfo > AddSocket;

	/** Customize a socket for a mesh */
	TSharedPtr< FUICommandInfo > CreateMeshSocket;

	/** Remove customization for a socket (actually just deletes the mesh socket) */
	TSharedPtr< FUICommandInfo > RemoveMeshSocket;

	/** Promotes a mesh-only socket to the skeleton */
	TSharedPtr< FUICommandInfo > PromoteSocketToSkeleton;

	/** Delete selected rows (deletes any sockets or assets selected in the tree */
	TSharedPtr< FUICommandInfo > DeleteSelectedRows;

	/** Show active sockets */
	TSharedPtr< FUICommandInfo > ShowActiveSockets;

	/** Show skeletal mesh sockets */
	TSharedPtr< FUICommandInfo > ShowMeshSockets;

	/** Show skeleton sockets */
	TSharedPtr< FUICommandInfo > ShowSkeletonSockets;

	/** Show all sockets */
	TSharedPtr< FUICommandInfo > ShowAllSockets;

	/** Hide all sockets */
	TSharedPtr< FUICommandInfo > HideSockets;

	/** Copy bone names */
	TSharedPtr< FUICommandInfo > CopyBoneNames;

	/** Reset bone transforms */
	TSharedPtr< FUICommandInfo > ResetBoneTransforms;

	/** Copy sockets to clipboard */
	TSharedPtr< FUICommandInfo > CopySockets;

	/** Paste sockets from clipboard */
	TSharedPtr< FUICommandInfo > PasteSockets;

	/** Paste sockets from clipboard */
	TSharedPtr< FUICommandInfo > PasteSocketsToSelectedBone;
};

#endif // __SkeletonTreeCommands_h_
