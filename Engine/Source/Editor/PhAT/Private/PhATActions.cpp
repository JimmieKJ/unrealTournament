// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PhATModule.h"
#include "PhATActions.h"
#include "PhAT.h"

#define LOCTEXT_NAMESPACE ""

void FPhATCommands::RegisterCommands()
{
	UI_COMMAND(ChangeDefaultMesh, "Change Mesh", "Change Default SkeletalMesh", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ResetEntireAsset, "Reset Asset", "Opens Body Creation Settings And Replaces Asset Using The New Settings", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ResetBoneCollision, "Reset", "Reset Bone Collision", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ApplyPhysicalMaterial, "Apply PhysMat", "Applies Currently Selected Physical Material In Content Browser To All Bodies", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(EditingMode_Body, "Body Mode", "Body Editing Mode", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(EditingMode_Constraint, "Constraint Mode", "Constraint Editing Mode", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(CopyProperties, "Copy Properties", "Copy Properties: Copy Properties Of Currently Selected Object To Next Selected Object", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::C));
	UI_COMMAND(PasteProperties, "Paste Properties", "Paste Properties: Copy Properties Of Currently Selected Object To Next Selected Object", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::V));
	UI_COMMAND(InstanceProperties, "Instance Properties", "Instance Properties: Displays Instance Properties When In Body Editing Mode", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::I));
	UI_COMMAND(RepeatLastSimulation, "Simulate", "Previews Physics Simulation", EUserInterfaceActionType::RadioButton, FInputChord(EKeys::Enter));
	UI_COMMAND(SimulationNormal, "Real Simulation", "Previews Normal Physics Simulation", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SimulationNoGravity, "No Gravity Simulation", "Run Physics Simulation without gravity. Use this to debug issues with your ragdoll. If the setup is correct, the asset should not move!", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ToggleSelectedSimulation, "Selected Simulation", "Run Physics Simulation on selected objects. Use this to tune  specific parts of your ragdoll.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(MeshRenderingMode_Solid, "Solid", "Solid Mesh Rendering Mode", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(MeshRenderingMode_Wireframe, "Wireframe", "Wireframe Mesh Rendering Mode", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(MeshRenderingMode_None, "None", "No Mesh Rendering Mode", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(CollisionRenderingMode_Solid, "Solid", "Solid Collision Rendering Mode", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(CollisionRenderingMode_Wireframe, "Wireframe", "Wireframe Collision Rendering Mode", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(CollisionRenderingMode_None, "None", "No Collision Rendering Mode", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ConstraintRenderingMode_None, "None", "No Constraint Rendering Mode", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ConstraintRenderingMode_AllPositions, "All Positions", "All Positions Constraint Rendering Mode", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ConstraintRenderingMode_AllLimits, "All Limits", "All Limits Constraint Rendering Mode", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ShowKinematicBodies, "Kinematic Bodies", "Displays Kinematic Bodies In Red", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(DrawGroundBox, "Ground Box", "Displays Floor Grid", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleGraphicsHierarchy, "Hierarchy", "Show Graphical Hierarchy Of Joints In Preview Viewport", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleBoneInfuences, "Bone Influences", "Displays Vertices Weighted To Currently Selected Bone or Body", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleMassProperties, "Mass Properties", "Show Mass Properties For Bodies When Simulation Is Enabled", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(DisableCollision, "Collision Off", "Disable Collision Of Currently Selected Body With Next Selected Body", EUserInterfaceActionType::Button, FInputChord(EKeys::RightBracket));
	UI_COMMAND(EnableCollision, "Collision On", "Enable Collision Of Currently Selected Body With Next Selected Body", EUserInterfaceActionType::Button, FInputChord(EKeys::LeftBracket));
	UI_COMMAND(WeldToBody, "Weld", "Weld Body: Weld Currently Selected Bodies", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddNewBody, "New Body", "Add New Body To Selected Bone.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddSphere, "Add Sphere", "Add Sphere To Selected Bone", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddSphyl, "Add Sphyl", "Add Sphyl To Selected Bone", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddBox, "Add Box", "Add Box To Selected Bone", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DeletePrimitive, "Delete", "Delete Selected Primitive", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DuplicatePrimitive, "Duplicate", "Duplicate Selected Primitive", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ResetConstraint, "Reset", "Reset Constraint", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SnapConstraint, "Snap", "Snap Constraint To Bone", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ConvertToBallAndSocket, "To B&S", "Convert Selected Constraint To Ball-And-Socket", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ConvertToHinge, "To Hinge", "Convert Selected Constraint To Hinge", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ConvertToPrismatic, "To Prismatic", "Convert Selected Constraint To Prismatic", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ConvertToSkeletal, "To Skeletal", "Convert Selected Constraint To Skeletal", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DeleteConstraint, "Delete", "Delete Selected Constraint", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(PlayAnimation, "Play", "Play Animation", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(RecordAnimation, "Record", "Record Animation", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ShowSkeleton, "Skeleton", "Show Skeleton", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(MakeBodyKinematic, "Kinematic", "Make Body Kinematic", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(MakeBodySimulated, "Simulated", "Make Body Simulated", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(MakeBodyDefault, "Default", "Reset This Body To Default", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(KinematicAllBodiesBelow, "Set All Bodies Below To Kinematic", "Set All Bodies Below To Kinematic", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SimulatedAllBodiesBelow, "Set All Bodies Below To Simulated", "Set All Bodies Below To Simulated", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(MakeAllBodiesBelowDefault, "Reset All Bodies Below To Default", "Reset All Bodies Below To Default", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DeleteBody, "Delete", "Delete Selected Body", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DeleteAllBodiesBelow, "Delete All Bodies Below", "Delete All Bodies Below", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ToggleMotor, "Toggle Motor", "Toggle Motor", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(EnableMotorsBelow, "Enable Motors Below", "Enable Motors Below", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DisableMotorsBelow, "Disable Motors Below", "Disable Motors Below", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SelectAllObjects, "Select All Objects", "Select All Objects", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::A));
	UI_COMMAND(HierarchyFilterAll, "All Bones", "Show Entire Hierarchy", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(HierarchyFilterBodies, "Bones With Bodies", "Filter Bones With Bodies", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(PerspectiveView, "Perspective", "Perspective", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(LeftView, "Left", "Orthographic view from left", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(TopView, "Top", "Orthographic view from top", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(FrontView, "Front", "Orthographic view from front", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RightView, "Right", "Orthographic view from right", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(BottomView, "Bottom", "Orthographic view from bottom", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(BackView, "Back", "Orthographic view from back", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(Mirror, "Mirror", "Finds the body on the other side and duplicates constraint and body", EUserInterfaceActionType::Button, FInputChord(EKeys::M));


	UI_COMMAND(SelectionLock, "Lock Selection", "", EUserInterfaceActionType::Button, FInputChord(EKeys::X));

	/** As two commands cannot have the same key; this command wraps both 
	  * DeletePrimitive and DeleteConstraint so the user can delete whatever is selected 
	  */
	UI_COMMAND(DeleteSelected, "Delete selected primitive or constraint", "", EUserInterfaceActionType::Button, FInputChord(EKeys::Platform_Delete));
	UI_COMMAND(CycleConstraintOrientation, "Cycle selected constraint orientation", "", EUserInterfaceActionType::Button, FInputChord(EKeys::Q));
	UI_COMMAND(CycleConstraintActive, "Cycle active constraint", "", EUserInterfaceActionType::Button, FInputChord(EKeys::Four));
	UI_COMMAND(ToggleSwing1, "Toggle Swing1 Constraint", "", EUserInterfaceActionType::Button, FInputChord(EKeys::One));
	UI_COMMAND(ToggleSwing2, "Toggle Swing2 Constraint", "", EUserInterfaceActionType::Button, FInputChord(EKeys::Three));
	UI_COMMAND(ToggleTwist, "Toggle Twist Constraint", "", EUserInterfaceActionType::Button, FInputChord(EKeys::Two));
	UI_COMMAND(FocusOnSelection, "Focus the viewport on the current selection", "", EUserInterfaceActionType::Button, FInputChord(EKeys::F));
}

#undef LOCTEXT_NAMESPACE