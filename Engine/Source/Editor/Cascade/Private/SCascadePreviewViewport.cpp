// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CascadeModule.h"
#include "Cascade.h"
#include "PreviewScene.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "CascadePreviewViewportClient.h"
#include "SCascadePreviewToolbar.h"
#include "SCascadePreviewViewport.h"
#include "SDockTab.h"


SCascadePreviewViewport::~SCascadePreviewViewport()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->Viewport = NULL;
	}
}

void SCascadePreviewViewport::Construct(const FArguments& InArgs)
{
	CascadePtr = InArgs._Cascade;

	SEditorViewport::Construct( SEditorViewport::FArguments() );
}

void SCascadePreviewViewport::RefreshViewport()
{
	SceneViewport->Invalidate();
}

bool SCascadePreviewViewport::IsVisible() const
{
	return ViewportWidget.IsValid() && (!ParentTab.IsValid() || ParentTab.Pin()->IsForeground());
}

EVisibility SCascadePreviewViewport::GetWidgetVisibility() const
{
	return IsVisible()? EVisibility::Visible: EVisibility::Collapsed;
}

TSharedPtr<FSceneViewport> SCascadePreviewViewport::GetViewport() const
{
	return SceneViewport;
}

TSharedPtr<FCascadeEdPreviewViewportClient> SCascadePreviewViewport::GetViewportClient() const
{
	return ViewportClient;
}

TSharedPtr<SViewport> SCascadePreviewViewport::GetViewportWidget() const
{
	return ViewportWidget;
}

TSharedRef<FEditorViewportClient> SCascadePreviewViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FCascadeEdPreviewViewportClient(CascadePtr, SharedThis(this)));

	ViewportClient->bSetListenerPosition = false;

	ViewportClient->SetRealtime(true);
	ViewportClient->VisibilityDelegate.BindSP(this, &SCascadePreviewViewport::IsVisible);

	return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SCascadePreviewViewport::MakeViewportToolbar()
{
	return
	SNew(SCascadePreviewViewportToolBar)
		.CascadePtr(CascadePtr)
		.Visibility(this, &SCascadePreviewViewport::GetWidgetVisibility)
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());
}


void SCascadePreviewViewport::OnFocusViewportToSelection()
{
	UParticleSystemComponent* Component = CascadePtr.Pin()->GetParticleSystemComponent();

	if( Component )
	{
		ViewportClient->FocusViewportOnBox( Component->Bounds.GetBox() );
	}
}