// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"
#include "SkeletonTreeCommands.h"

void FSkeletonTreeCommands::RegisterCommands()
{
	UI_COMMAND( ShowAllBones, "Show All Bones", "Show every bone in the skeleton", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( ShowMeshBones, "Show Mesh Bones", "Show bones that are used in the mesh", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( ShowWeightedBones, "Show Weighted Bones", "Show bones that have vertices weighted to them", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( HideBones, "Hide Bones", "Hides all bones (sockets and attached assets will still be listed)", EUserInterfaceActionType::RadioButton, FInputGesture() );

	UI_COMMAND( CopyBoneNames, "Copy Selected Bone Names", "Copy selected bone names to clipboard", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( ResetBoneTransforms, "Reset Selected Bone Transforms", "Reset the transforms of the selected bones", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( CopySockets, "Copy Selected Sockets", "Copy selected sockets to clipboard", EUserInterfaceActionType::Button, FInputGesture( EModifierKey::Control, EKeys::C ) );
	UI_COMMAND( PasteSockets, "Paste Selected Sockets", "Paste selected sockets from clipboard", EUserInterfaceActionType::Button, FInputGesture( EModifierKey::Control, EKeys::V ) );

	UI_COMMAND( AddSocket, "Add Socket", "Add a socket to this bone in the skeleton (disabled when socket filter is set to \"Mesh Sockets\" or \"Sockets Hidden\" mode)", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( CreateMeshSocket, "Create Mesh Socket", "Duplicate this socket from skeleton to the current mesh and modify the socket data for it", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( RemoveMeshSocket, "Remove Mesh Socket", "Remove duplicated version of this socket for the current mesh (reverts to the socket in the skeleton)", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( PromoteSocketToSkeleton, "Promote Socket To Skeleton", "Makes this socket available for all meshes that use the same skeleton (copies the socket from this mesh to the skeleton)", EUserInterfaceActionType::Button, FInputGesture() );

	UI_COMMAND(DeleteSelectedRows, "Delete", "Delete all selected sockets and attached meshes in the tree", EUserInterfaceActionType::Button, FInputGesture(EKeys::Platform_Delete));

	UI_COMMAND( ShowActiveSockets, "Show Active Sockets", "Show mesh and skeleton sockets, hiding the skeleton sockets that have a customized mesh socket", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( ShowAllSockets, "Show All Sockets", "Show all sockets that are in the mesh and skeleton", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( ShowMeshSockets, "Show Mesh Sockets", "Show sockets that are in the mesh only", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( ShowSkeletonSockets, "Show Skeleton Sockets", "Show sockets that are in the skeleton only", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( HideSockets, "Hide Sockets", "Show no sockets", EUserInterfaceActionType::RadioButton, FInputGesture() );
}
