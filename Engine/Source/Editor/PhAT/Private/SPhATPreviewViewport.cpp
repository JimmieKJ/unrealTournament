// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SPhATPreviewViewport.h"
#include "PhAT.h"
#include "PhATPreviewViewportClient.h"
#include "Widgets/SViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SceneViewport.h"
#include "SPhATPreviewToolbar.h"
#include "Widgets/Docking/SDockTab.h"


SPhATPreviewViewport::~SPhATPreviewViewport()
{
	if (EditorViewportClient.IsValid())
	{
		EditorViewportClient->Viewport->Destroy();
		EditorViewportClient->Viewport = NULL;
	}
}

void SPhATPreviewViewport::SetViewportType(ELevelViewportType ViewType)
{
	EditorViewportClient->SetViewportType(ViewType);
}

void SPhATPreviewViewport::RotateViewportType()
{
	EditorViewportClient->RotateViewportType();
}

void SPhATPreviewViewport::Construct(const FArguments& InArgs)
{
	PhATPtr = InArgs._PhAT;

	SEditorViewport::Construct(SEditorViewport::FArguments());
	ViewportWidget->SetEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());
}

void SPhATPreviewViewport::RefreshViewport()
{
	SceneViewport->Invalidate();
	SceneViewport->InvalidateDisplay();
}

bool SPhATPreviewViewport::IsVisible() const
{
	return ViewportWidget.IsValid() && (!ParentTab.IsValid() || ParentTab.Pin()->IsForeground());
}

TSharedPtr<FSceneViewport> SPhATPreviewViewport::GetViewport() const
{
	return SceneViewport;
}

TSharedPtr<FPhATEdPreviewViewportClient> SPhATPreviewViewport::GetViewportClient() const
{
	return EditorViewportClient;
}

TSharedPtr<SViewport> SPhATPreviewViewport::GetViewportWidget() const
{
	return ViewportWidget;
}

void SPhATPreviewViewport::OnFocusViewportToSelection()
{
	if(FPhAT* Phat = PhATPtr.Pin().Get())
	{
		Phat->OnFocusSelection();
	}
}

TSharedRef<FEditorViewportClient> SPhATPreviewViewport::MakeEditorViewportClient()
{
	EditorViewportClient = MakeShareable(new FPhATEdPreviewViewportClient(PhATPtr, PhATPtr.Pin()->GetSharedData(), SharedThis(this)));

	EditorViewportClient->bSetListenerPosition = false;

	EditorViewportClient->SetRealtime(!FPhAT::IsPIERunning());
	EditorViewportClient->VisibilityDelegate.BindSP(this, &SPhATPreviewViewport::IsVisible);

	return EditorViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SPhATPreviewViewport::MakeViewportToolbar()
{
	return SNew(SPhATPreviewViewportToolBar, SharedThis(this))
			.PhATPtr(PhATPtr)
			.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());
}
