// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "VREditorWidgetComponent.h"

UVREditorWidgetComponent::UVREditorWidgetComponent(const FObjectInitializer& ObjectInitializer)
	: Super( ObjectInitializer )
{
	bIsHovering = false;
	DrawingPolicy = EVREditorWidgetDrawingPolicy::Always;
	bHasEverDrawn = false;
}

bool UVREditorWidgetComponent::ShouldDrawWidget() const
{
	if ( DrawingPolicy == EVREditorWidgetDrawingPolicy::Always ||
		(DrawingPolicy == EVREditorWidgetDrawingPolicy::Hovering && bIsHovering) ||
		!bHasEverDrawn )
	{
		return Super::ShouldDrawWidget();
	}

	return false;
}

void UVREditorWidgetComponent::DrawWidgetToRenderTarget(float DeltaTime)
{
	bHasEverDrawn = true;

	Super::DrawWidgetToRenderTarget(DeltaTime);
}
