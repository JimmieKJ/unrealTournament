// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VREditorModule.h"
#include "VREditorRadialMenuItem.h"

UVREditorRadialMenuItem::UVREditorRadialMenuItem( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{

}

float UVREditorRadialMenuItem::GetAngle() const
{
	return FRotator::NormalizeAxis( FMath::RadiansToDegrees(FMath::Atan2( RenderTransformPivot.X - RenderTransform.Translation.X,  RenderTransformPivot.X - RenderTransform.Translation.Y ) ) );
}

void UVREditorRadialMenuItem::OnEnterHover_Implementation()
{

}

void UVREditorRadialMenuItem::OnLeaveHover_Implementation()
{
	
}

void UVREditorRadialMenuItem::OnPressed_Implementation()
{
	OnPressedDelegate.Broadcast();
}

void UVREditorRadialMenuItem::SetLabel_Implementation(const FString& Text)
{

}

