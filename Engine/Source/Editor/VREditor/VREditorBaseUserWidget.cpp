// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VREditorModule.h"
#include "VREditorBaseUserWidget.h"
#include "VREditorFloatingUI.h"

UVREditorBaseUserWidget::UVREditorBaseUserWidget( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer ),
	  Owner( nullptr )
{
}


void UVREditorBaseUserWidget::SetOwner( class AVREditorFloatingUI* NewOwner )
{
	Owner = NewOwner;
}

