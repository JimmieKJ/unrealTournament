// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "EditorLiveStreamingModule.h"
#include "EditorLiveStreamingSettings.h"


UEditorLiveStreamingSettings::UEditorLiveStreamingSettings( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}


void UEditorLiveStreamingSettings::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent )
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;

	const FName Name = PropertyThatChanged ? PropertyThatChanged->GetFName() : NAME_None;
// 	if( Name == ??? )
// 	{
//		...
//	}

	this->SaveConfig();
}