// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PhATPrivatePCH.h"
#include "PhAT.h"
#include "Editor/UnrealEd/Public/SViewportToolBar.h"
#include "Editor/UnrealEd/Public/STransformViewportToolbar.h"
#include "Editor/UnrealEd/Public/SEditorViewportToolBarMenu.h"
#include "Editor/UnrealEd/Public/SEditorViewportViewMenu.h"
#include "PhATActions.h"
#include "SPhATPreviewToolbar.h"
#include "Editor/UnrealEd/Public/SEditorViewport.h"
#include "SPhATPreviewViewport.h"
#include "PhATPreviewViewportClient.h"

#define LOCTEXT_NAMESPACE "PhatViewportToolBar"


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
			.Padding(2.0f, 2.0f)
			[
				SNew(SEditorViewportToolbarMenu)
				.ParentToolBar(SharedThis(this))
				.Cursor(EMouseCursor::Default)
				.Image("EditorViewportToolBar.MenuDropdown")
				.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("EditorViewportToolBar.MenuDropdown")))
				.OnGetMenuContent(this, &SPhATPreviewViewportToolBar::GenerateOptionsMenu)
			]

			// Camera Type (Perspective/Top/etc...)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 2.0f)
			[
				SNew(SEditorViewportToolbarMenu)
				.ParentToolBar(SharedThis(this))
				.Label(this, &SPhATPreviewViewportToolBar::GetCameraMenuLabel)
				.LabelIcon(this, &SPhATPreviewViewportToolBar::GetCameraMenuLabelIcon)
				.OnGetMenuContent(this, &SPhATPreviewViewportToolBar::GeneratePerspectiveMenu)
			]

			// View menu (lit, unlit, etc...)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 2.0f)
			[
				SNew(SEditorViewportViewMenu, InRealViewport.ToSharedRef(), SharedThis(this))
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 2.0f)
			[
				SNew(SEditorViewportToolbarMenu)
				.ParentToolBar(SharedThis(this))
				.Cursor(EMouseCursor::Default)
				.Label(NSLOCTEXT("PhAT", "ShowMenuTitle_Default", "Show"))
				.OnGetMenuContent(this, &SPhATPreviewViewportToolBar::GenerateShowMenu) 
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

FText SPhATPreviewViewportToolBar::GetCameraMenuLabel() const
{
	FText Label = LOCTEXT("Viewport_Default", "Camera");
	TSharedPtr<SPhATPreviewViewport> EditorViewport = PhATPtr.Pin()->GetPreviewViewportWidget();
	if (EditorViewport.IsValid())
	{
		switch (EditorViewport->GetViewportClient()->GetViewportType())
		{
		case LVT_Perspective:
			Label = LOCTEXT("CameraMenuTitle_Perspective", "Perspective");
			break;

		case LVT_OrthoXY:
			Label = LOCTEXT("CameraMenuTitle_Top", "Top");
			break;

		case LVT_OrthoYZ:
			Label = LOCTEXT("CameraMenuTitle_Left", "Left");
			break;

		case LVT_OrthoXZ:
			Label = LOCTEXT("CameraMenuTitle_Front", "Front");
			break;

		case LVT_OrthoNegativeXY:
			Label = LOCTEXT("CameraMenuTitle_Bottom", "Bottom");
			break;

		case LVT_OrthoNegativeYZ:
			Label = LOCTEXT("CameraMenuTitle_Right", "Right");
			break;

		case LVT_OrthoNegativeXZ:
			Label = LOCTEXT("CameraMenuTitle_Back", "Back");
			break;
		case LVT_OrthoFreelook:
			break;
		}
	}

	return Label;
}

const FSlateBrush* SPhATPreviewViewportToolBar::GetCameraMenuLabelIcon() const
{
	FName Icon = NAME_None;
	TSharedPtr<SPhATPreviewViewport> EditorViewport = PhATPtr.Pin()->GetPreviewViewportWidget();
	if (EditorViewport.IsValid())
	{
		switch (EditorViewport->GetViewportClient()->GetViewportType())
		{
		case LVT_Perspective:
			Icon = FName("EditorViewport.Perspective");
			break;

		case LVT_OrthoXY:
			Icon = FName("EditorViewport.Top");
			break;

		case LVT_OrthoYZ:
			Icon = FName("EditorViewport.Left");
			break;

		case LVT_OrthoXZ:
			Icon = FName("EditorViewport.Front");
			break;

		case LVT_OrthoNegativeXY:
			Icon = FName("EditorViewport.Bottom");
			break;

		case LVT_OrthoNegativeYZ:
			Icon = FName("EditorViewport.Right");
			break;

		case LVT_OrthoNegativeXZ:
			Icon = FName("EditorViewport.Back");
			break;
		case LVT_OrthoFreelook:
			break;
		}
	}

	return FEditorStyle::GetBrush(Icon);
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

TSharedRef<SWidget> SPhATPreviewViewportToolBar::GenerateShowMenu() const
{
	const FPhATCommands& Actions = FPhATCommands::Get();
		
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, PhATPtr.Pin()->GetToolkitCommands());
	{
		ShowMenuBuilder.AddMenuEntry(Actions.ShowSkeleton);
		ShowMenuBuilder.AddMenuEntry(Actions.DrawGroundBox);
		ShowMenuBuilder.AddMenuEntry(Actions.InstanceProperties);
		ShowMenuBuilder.AddMenuEntry(Actions.ShowKinematicBodies);
		ShowMenuBuilder.AddMenuEntry(Actions.ToggleGraphicsHierarchy);
		ShowMenuBuilder.AddMenuEntry(Actions.ToggleBoneInfuences);
		ShowMenuBuilder.AddMenuEntry(Actions.ToggleMassProperties);
	}

	return ShowMenuBuilder.MakeWidget();	
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

TSharedRef<SWidget> SPhATPreviewViewportToolBar::GenerateOptionsMenu() const
{

	const bool bIsPerspective = PhATPtr.Pin()->GetPreviewViewportWidget()->GetViewportClient()->IsPerspective();
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder OptionsMenuBuilder(bInShouldCloseWindowAfterMenuSelection, PhATPtr.Pin()->GetToolkitCommands());
	{
		OptionsMenuBuilder.BeginSection("LevelViewportViewportOptions", LOCTEXT("OptionsMenuHeader", "Viewport Options"));
		{
			if (bIsPerspective)
			{
				OptionsMenuBuilder.AddWidget(GenerateFOVMenu(), LOCTEXT("FOVAngle", "Field of View (H)"));
			}
		}
		OptionsMenuBuilder.EndSection();		
	}

	return OptionsMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SPhATPreviewViewportToolBar::GenerateFOVMenu() const
{
	const float FOVMin = 5.f;
	const float FOVMax = 170.f;

	return
		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.WidthOverride(100.0f)
			[
				SNew(SSpinBox<float>)
				.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.MinValue(FOVMin)
				.MaxValue(FOVMax)
				.Value(this, &SPhATPreviewViewportToolBar::OnGetFOVValue)
				.OnValueChanged(this, &SPhATPreviewViewportToolBar::OnFOVValueChanged)
			]
		];
}

float SPhATPreviewViewportToolBar::OnGetFOVValue() const
{
	return PhATPtr.Pin()->GetPreviewViewportWidget()->GetViewportClient()->ViewFOV;
}

void SPhATPreviewViewportToolBar::OnFOVValueChanged(float NewValue)
{
	bool bUpdateStoredFOV = true;
	FPhATEdPreviewViewportClient* ViewportClient = PhATPtr.Pin()->GetPreviewViewportWidget()->GetViewportClient().Get();
	ViewportClient->FOVAngle = NewValue;
	ViewportClient->ViewFOV = NewValue;
	ViewportClient->Invalidate();
}


#undef LOCTEXT_NAMESPACE
