// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "BlueprintEditor.h"
#include "BlueprintEditorCommands.h"
#include "SSCSEditorViewport.h"
#include "SCSEditorViewportClient.h"
#include "SSCSEditor.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "Editor/UnrealEd/Public/SViewportToolBar.h"
#include "Editor/UnrealEd/Public/STransformViewportToolbar.h"
#include "EditorViewportCommands.h"
#include "SEditorViewportToolBarMenu.h"

/*-----------------------------------------------------------------------------
   SSCSEditorViewportToolBar
-----------------------------------------------------------------------------*/

class SSCSEditorViewportToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS( SSCSEditorViewportToolBar ){}
		SLATE_ARGUMENT(TWeakPtr<SSCSEditorViewport>, EditorViewport)
	SLATE_END_ARGS()

	/** Constructs this widget with the given parameters */
	void Construct(const FArguments& InArgs)
	{
		EditorViewport = InArgs._EditorViewport;

		static const FName DefaultForegroundName("DefaultForeground");

		this->ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.ColorAndOpacity(this, &SViewportToolBar::OnGetColorAndOpacity)
			.ForegroundColor(FEditorStyle::GetSlateColor(DefaultForegroundName))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 2.0f)
				[
					SNew(SEditorViewportToolbarMenu)
					.ParentToolBar(SharedThis(this))
					.Cursor(EMouseCursor::Default)
					.Image("EditorViewportToolBar.MenuDropdown")
					.OnGetMenuContent(this, &SSCSEditorViewportToolBar::GeneratePreviewMenu)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 2.0f)
				[
					SNew( SEditorViewportToolbarMenu )
					.ParentToolBar( SharedThis( this ) )
					.Cursor( EMouseCursor::Default )
					.Label(this, &SSCSEditorViewportToolBar::GetCameraMenuLabel)
					.LabelIcon(this, &SSCSEditorViewportToolBar::GetCameraMenuLabelIcon)
					.OnGetMenuContent(this, &SSCSEditorViewportToolBar::GenerateCameraMenu)
				]
				+ SHorizontalBox::Slot()
				.Padding( 3.0f, 1.0f )
				.HAlign( HAlign_Right )
				[
					SNew(STransformViewportToolBar)
					.Viewport(EditorViewport.Pin().ToSharedRef())
					.CommandList(EditorViewport.Pin()->GetCommandList())
				]
			]
		];

		SViewportToolBar::Construct(SViewportToolBar::FArguments());
	}

	/** Creates the preview menu */
	TSharedRef<SWidget> GeneratePreviewMenu() const
	{
		TSharedPtr<const FUICommandList> CommandList = EditorViewport.IsValid()? EditorViewport.Pin()->GetCommandList(): NULL;

		const bool bInShouldCloseWindowAfterMenuSelection = true;

		FMenuBuilder PreviewOptionsMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);
		{
			PreviewOptionsMenuBuilder.BeginSection("BlueprintEditorPreviewOptions", NSLOCTEXT("BlueprintEditor", "PreviewOptionsMenuHeader", "Preview Viewport Options"));
			{
				PreviewOptionsMenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().ResetCamera);
				PreviewOptionsMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().ToggleRealTime);
				PreviewOptionsMenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().ShowFloor);
				PreviewOptionsMenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().ShowGrid);
			}
			PreviewOptionsMenuBuilder.EndSection();
		}

		return PreviewOptionsMenuBuilder.MakeWidget();
	}

	FText GetCameraMenuLabel() const
	{
		FText Label = NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_Default", "Camera");

		if(EditorViewport.IsValid())
		{
			switch(EditorViewport.Pin()->GetViewportClient()->GetViewportType())
			{
			case LVT_Perspective:
				Label = NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_Perspective", "Perspective");
				break;

			case LVT_OrthoXY:
				Label = NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_Top", "Top");
				break;

			case LVT_OrthoYZ:
				Label = NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_Side", "Side");
				break;

			case LVT_OrthoXZ:
				Label = NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_Front", "Front");
				break;

			case LVT_OrthoFreelook:
				Label = NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_OrthoFreelook", "Ortho");
				break;
			}
		}

		return Label;
	}

	const FSlateBrush* GetCameraMenuLabelIcon() const
	{
		FName Icon = NAME_None;

		if(EditorViewport.IsValid())
		{
			switch(EditorViewport.Pin()->GetViewportClient()->GetViewportType())
			{
			case LVT_Perspective:
				Icon = FName("EditorViewport.Perspective");
				break;

			case LVT_OrthoXY:
				Icon = FName("EditorViewport.Top");
				break;

			case LVT_OrthoYZ:
				Icon = FName( "EditorViewport.Side");
				break;

			case LVT_OrthoXZ:
				Icon = FName("EditorViewport.Front");
				break;
			}
		}

		return FEditorStyle::GetBrush(Icon);
	}

	TSharedRef<SWidget> GenerateCameraMenu() const
	{
		TSharedPtr<const FUICommandList> CommandList = EditorViewport.IsValid()? EditorViewport.Pin()->GetCommandList(): nullptr;

		const bool bInShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder CameraMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);

		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Perspective);

		CameraMenuBuilder.BeginSection("LevelViewportCameraType_Ortho", NSLOCTEXT("BlueprintEditor", "CameraTypeHeader_Ortho", "Orthographic"));
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Top);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Side);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Front);
		CameraMenuBuilder.EndSection();

		return CameraMenuBuilder.MakeWidget();
	}

private:
	/** Reference to the parent viewport */
	TWeakPtr<SSCSEditorViewport> EditorViewport;
};


/*-----------------------------------------------------------------------------
   SSCSEditorViewport
-----------------------------------------------------------------------------*/

void SSCSEditorViewport::Construct(const FArguments& InArgs)
{
	// Initialize
	bPreviewNeedsUpdating = false;
	bResetCameraOnNextPreviewUpdate = false;

	// Save off the Blueprint editor reference, we'll need this later
	BlueprintEditorPtr = InArgs._BlueprintEditor;

	SEditorViewport::Construct( SEditorViewport::FArguments() );

	// Refresh the preview scene
	RequestRefresh(true);
}

SSCSEditorViewport::~SSCSEditorViewport()
{
	if(ViewportClient.IsValid())
	{
		// Reset this to ensure it's no longer in use after destruction
		ViewportClient->Viewport = NULL;
	}
}



bool SSCSEditorViewport::IsVisible() const
{
	// We consider the viewport to be visible if the reference is valid
	return ViewportWidget.IsValid() && SEditorViewport::IsVisible();
}

EVisibility SSCSEditorViewport::GetWidgetVisibility() const
{
	return IsVisible()? EVisibility::Visible: EVisibility::Collapsed;
}

TSharedRef<FEditorViewportClient> SSCSEditorViewport::MakeEditorViewportClient()
{
	// Construct a new viewport client instance.
	ViewportClient = MakeShareable(new FSCSEditorViewportClient(BlueprintEditorPtr, PreviewScene));
	ViewportClient->SetRealtime(true);
	ViewportClient->bSetListenerPosition = false;
	ViewportClient->VisibilityDelegate.BindSP(this, &SSCSEditorViewport::IsVisible);

	return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SSCSEditorViewport::MakeViewportToolbar()
{
	return 
		SNew(SSCSEditorViewportToolBar)
		.EditorViewport(SharedThis(this))
		.Visibility(this, &SSCSEditorViewport::GetWidgetVisibility)
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());
}


void SSCSEditorViewport::BindCommands()
{
	const FBlueprintEditorCommands& Commands = FBlueprintEditorCommands::Get();

	GetCommandList()->Append( BlueprintEditorPtr.Pin()->GetToolkitCommands() );
	GetCommandList()->Append( BlueprintEditorPtr.Pin()->GetSCSEditor()->CommandList.ToSharedRef() );
	SEditorViewport::BindCommands();

	BlueprintEditorPtr.Pin()->GetToolkitCommands()->MapAction(
		FBlueprintEditorCommands::Get().EnableSimulation,
		FExecuteAction::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::ToggleIsSimulateEnabled),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::GetIsSimulateEnabled));

	// Toggle camera lock on/off
	CommandList->MapAction(
		FBlueprintEditorCommands::Get().ResetCamera,
		FExecuteAction::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::ResetCamera) );

	CommandList->MapAction(
		FBlueprintEditorCommands::Get().ShowFloor,
		FExecuteAction::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::ToggleShowFloor),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::GetShowFloor));

	CommandList->MapAction(
		FBlueprintEditorCommands::Get().ShowGrid,
		FExecuteAction::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::ToggleShowGrid),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::GetShowGrid));
}

void SSCSEditorViewport::Invalidate()
{
	ViewportClient->Invalidate();
}

void SSCSEditorViewport::EnablePreview(bool bEnable)
{
	if(bEnable)
	{
		// Restore the previously-saved realtime setting
		ViewportClient->RestoreRealtime();
	}
	else
	{
		// Disable and store the current realtime setting. This will bypass real-time rendering in the preview viewport (see UEditorEngine::UpdateSingleViewportClient).
		ViewportClient->SetRealtime(false, true);
	}
}

void SSCSEditorViewport::RequestRefresh(bool bResetCamera, bool bRefreshNow)
{
	if(bRefreshNow)
	{
		if(ViewportClient.IsValid())
		{
			ViewportClient->InvalidatePreview(bResetCamera);
		}
	}
	else
	{
		// Defer the update until the next tick. This way we don't accidentally spawn the preview actor in the middle of a transaction, for example.
		bPreviewNeedsUpdating = true;
		if(bResetCamera)
		{
			bResetCameraOnNextPreviewUpdate = true;
		}
	}
}

void SSCSEditorViewport::OnComponentSelectionChanged()
{
	// When the component selection changes, make sure to invalidate hit proxies to sync with the current selection
	SceneViewport->InvalidateHitProxy();
}

void SSCSEditorViewport::OnFocusViewportToSelection()
{
	ViewportClient->FocusViewportToSelection();
}

bool SSCSEditorViewport::GetIsSimulateEnabled()
{
	return ViewportClient->GetIsSimulateEnabled();
}

void SSCSEditorViewport::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SEditorViewport::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// If the preview scene is no longer valid (i.e. all actors have destroyed themselves), then attempt to recreate the scene. This way we can "loop" certain "finite" Blueprints that might destroy themselves.
	if(ViewportClient.IsValid() && (bPreviewNeedsUpdating || !ViewportClient->IsPreviewSceneValid()))
	{
		ViewportClient->InvalidatePreview(bResetCameraOnNextPreviewUpdate);

		// Reset for next update
		bPreviewNeedsUpdating = false;
		bResetCameraOnNextPreviewUpdate = false;
	}
}

AActor* SSCSEditorViewport::GetPreviewActor() const
{
	return ViewportClient->GetPreviewActor();
}
