// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EditorWidgetsPrivatePCH.h"
#include "ModuleManager.h"
#include "STransportControl.h"

IMPLEMENT_MODULE( FEditorWidgetsModule, EditorWidgets );

const FName FEditorWidgetsModule::EditorWidgetsAppIdentifier( TEXT( "EditorWidgetsApp" ) );

void FEditorWidgetsModule::StartupModule()
{
}

void FEditorWidgetsModule::ShutdownModule()
{
}

TSharedRef<IObjectNameEditableTextBox> FEditorWidgetsModule::CreateObjectNameEditableTextBox(const TArray<TWeakObjectPtr<UObject>>& Objects)
{
	TSharedRef<SObjectNameEditableTextBox> Widget = SNew(SObjectNameEditableTextBox).Objects(Objects);
	return Widget;
}

TSharedRef<SWidget> FEditorWidgetsModule::CreateAssetDiscoveryIndicator(EAssetDiscoveryIndicatorScaleMode::Type ScaleMode, FMargin Padding, bool bFadeIn)
{
	return SNew(SAssetDiscoveryIndicator)
		.ScaleMode(ScaleMode)
		.Padding(Padding)
		.FadeIn(bFadeIn);
}

TSharedRef<ITransportControl> FEditorWidgetsModule::CreateTransportControl(const FTransportControlArgs& Args)
{
	return SNew(STransportControl)
		.TransportArgs(Args);
}
