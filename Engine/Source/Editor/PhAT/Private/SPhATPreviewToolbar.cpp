// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PhATModule.h"
#include "PhAT.h"
#include "Editor/UnrealEd/Public/SViewportToolBar.h"
#include "Editor/UnrealEd/Public/STransformViewportToolbar.h"
#include "Editor/UnrealEd/Public/SEditorViewportToolBarMenu.h"
#include "Editor/UnrealEd/Public/SEditorViewportViewMenu.h"
#include "PhATActions.h"
#include "SPhATPreviewToolbar.h"
#include "Editor/UnrealEd//Public/SEditorViewport.h"


void SPhATPreviewViewportToolBar::Construct(const FArguments& InArgs, TSharedPtr<class SEditorViewport> InRealViewport)
{
	PhATPtr = InArgs._PhATPtr;
	
	static const FName DefaultForegroundName("DefaultForeground");

	this->ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("NoBorder"))
		// Color and opacity is changed based on whether or not the mouse cursor is hovering over the toolbar area
		.ColorAndOpacity(this, &SViewportToolBar::OnGetColorAndOpacity)
		.ForegroundColor( FEditorStyle::GetSlateColor(DefaultForegroundName) )
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 2.0f)
			[
				SNew(SEditorViewportToolbarMenu)
				.ParentToolBar(SharedThis(this))
				.Cursor(EMouseCursor::Default)
				.Label(NSLOCTEXT("PhAT", "CameraMenuTitle_Default", "Camera"))
				.OnGetMenuContent(this, &SPhATPreviewViewportToolBar::GeneratePerspectiveMenu)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 2.0f)
			[
				SNew(SEditorViewportToolbarMenu)
				.ParentToolBar(SharedThis(this))
				.Cursor(EMouseCursor::Default)
				.Label(NSLOCTEXT("PhAT", "ViewMenuTitle_Default", "View"))
				.OnGetMenuContent(this, &SPhATPreviewViewportToolBar::GenerateViewMenu) 
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 2.0f)
			[
				SNew(SEditorViewportToolbarMenu)
				.ParentToolBar(SharedThis(this))
				.Cursor(EMouseCursor::Default)
				.Label(NSLOCTEXT("PhAT", "ModesMenuTitle_Default", "Modes"))
				.OnGetMenuContent(this, &SPhATPreviewViewportToolBar::GenerateModesMenu) 
			]

			+ SHorizontalBox::Slot()
			.Padding(3.0f, 1.0f)
			.HAlign(HAlign_Right)
			[
				SNew(STransformViewportToolBar)
				.Viewport(InRealViewport)
				.CommandList(InRealViewport->GetCommandList())
			]
		]
	];

	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}

TSharedRef<SWidget> SPhATPreviewViewportToolBar::GeneratePerspectiveMenu() const
{
	const FPhATCommands& Actions = FPhATCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder PerspectiveMenuBuilder(bInShouldCloseWindowAfterMenuSelection, PhATPtr.Pin()->GetToolkitCommands());
	{
		PerspectiveMenuBuilder.AddMenuEntry(Actions.PerspectiveView);
		PerspectiveMenuBuilder.AddMenuEntry(Actions.TopView);
		PerspectiveMenuBuilder.AddMenuEntry(Actions.BottomView);
		PerspectiveMenuBuilder.AddMenuEntry(Actions.LeftView);
		PerspectiveMenuBuilder.AddMenuEntry(Actions.RightView);
		PerspectiveMenuBuilder.AddMenuEntry(Actions.FrontView);
		PerspectiveMenuBuilder.AddMenuEntry(Actions.BackView);
	}

	return PerspectiveMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SPhATPreviewViewportToolBar::GenerateViewMenu() const
{
	const FPhATCommands& Actions = FPhATCommands::Get();
		
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ViewMenuBuilder(bInShouldCloseWindowAfterMenuSelection, PhATPtr.Pin()->GetToolkitCommands());
	{
		ViewMenuBuilder.AddMenuEntry(Actions.ShowSkeleton);
		ViewMenuBuilder.AddMenuEntry(Actions.DrawGroundBox);
		ViewMenuBuilder.AddMenuEntry(Actions.InstanceProperties);
		ViewMenuBuilder.AddMenuEntry(Actions.ShowKinematicBodies);
		ViewMenuBuilder.AddMenuEntry(Actions.ToggleGraphicsHierarchy);
		ViewMenuBuilder.AddMenuEntry(Actions.ToggleBoneInfuences);
		ViewMenuBuilder.AddMenuEntry(Actions.ToggleMassProperties);
	}

	return ViewMenuBuilder.MakeWidget();	
}

TSharedRef<SWidget> SPhATPreviewViewportToolBar::GenerateModesMenu() const
{
	const FPhATCommands& Actions = FPhATCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ModesMenuBuilder(bInShouldCloseWindowAfterMenuSelection, PhATPtr.Pin()->GetToolkitCommands());
	{
		// Mesh, collision and constraint rendering modes
		{
			struct Local
			{
				static void BuildMeshRenderModeMenu(FMenuBuilder& Menu)
				{
					const FPhATCommands& Commands = FPhATCommands::Get();

					Menu.BeginSection("PhATRenderingMode", NSLOCTEXT("PhAT", "MeshRenderModeHeader", "Mesh Rendering Mode"));
					{
						Menu.AddMenuEntry(Commands.MeshRenderingMode_Solid);
						Menu.AddMenuEntry(Commands.MeshRenderingMode_Wireframe);
						Menu.AddMenuEntry(Commands.MeshRenderingMode_None);
					}
					Menu.EndSection();
				}

				static void BuildCollisionRenderModeMenu(FMenuBuilder& Menu)
				{
					const FPhATCommands& Commands = FPhATCommands::Get();

					Menu.BeginSection("PhATCollisionMode", NSLOCTEXT("PhAT", "CollisionRenderModeHeader", "Collision Rendering Mode"));
					{
						Menu.AddMenuEntry(Commands.CollisionRenderingMode_Solid);
						Menu.AddMenuEntry(Commands.CollisionRenderingMode_Wireframe);
						Menu.AddMenuEntry(Commands.CollisionRenderingMode_None);
					}
					Menu.EndSection();
				}

				static void BuildConstraintRenderModeMenu(FMenuBuilder& Menu)
				{
					const FPhATCommands& Commands = FPhATCommands::Get();

					Menu.BeginSection("PhATConstraintMode", NSLOCTEXT("PhAT", "ConstraintRenderModeHeader", "Constraint Rendering Mode"));
					{
						Menu.AddMenuEntry(Commands.ConstraintRenderingMode_None);
						Menu.AddMenuEntry(Commands.ConstraintRenderingMode_AllPositions);
						Menu.AddMenuEntry(Commands.ConstraintRenderingMode_AllLimits);
					}
					Menu.EndSection();
				}
			};

			ModesMenuBuilder.BeginSection("PhATSubMenus");
			{
				ModesMenuBuilder.AddSubMenu(NSLOCTEXT("PhAT", "MeshRenderModeSubMenu", "MeshRender Mode"), FText::GetEmpty(), FNewMenuDelegate::CreateStatic(&Local::BuildMeshRenderModeMenu));
				ModesMenuBuilder.AddSubMenu(NSLOCTEXT("PhAT", "CollisionRenderModeSubMenu", "CollisionRender Mode"), FText::GetEmpty(), FNewMenuDelegate::CreateStatic(&Local::BuildCollisionRenderModeMenu));
				ModesMenuBuilder.AddSubMenu(NSLOCTEXT("PhAT", "ConstraintRenderModeSubMenu", "ConstraintRender Mode"), FText::GetEmpty(), FNewMenuDelegate::CreateStatic(&Local::BuildConstraintRenderModeMenu));
			}
			ModesMenuBuilder.EndSection();
		}
	}

	return ModesMenuBuilder.MakeWidget();	
}
