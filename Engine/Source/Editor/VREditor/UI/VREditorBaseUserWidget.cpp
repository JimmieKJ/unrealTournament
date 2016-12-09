// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

