// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "VREditorBaseUserWidget.h"
#include "VREditorRadialMenuItem.generated.h"

DECLARE_MULTICAST_DELEGATE(FVRQuickMenuItemPressedCallback)

/**
 * A simple quick-access menu for VR editing, with frequently-used features
 */
UCLASS( Blueprintable )
class UVREditorRadialMenuItem : public UVREditorBaseUserWidget
{
	GENERATED_BODY()

public:
	/** Default constructor that sets up CDO properties */
	UVREditorRadialMenuItem( const FObjectInitializer& ObjectInitializer );

	/** Update UI visuals */
	UFUNCTION(BlueprintNativeEvent, meta = (DisplayName = "On Enter Hover"))
	void OnEnterHover();

	/** Update UI visuals */
	UFUNCTION(BlueprintNativeEvent, meta = (DisplayName = "On Leave Hover"))
	void OnLeaveHover();

	/** Broadcasts the PressedDelegate */
	UFUNCTION(BlueprintNativeEvent, meta = (DisplayName = "On Pressed"))
	void OnPressed();

	/** Changes the text for this widget */
	UFUNCTION(BlueprintNativeEvent, meta = (DisplayName = "Set Label "))
	void SetLabel( const FString& Text );

	/** Gets the angle from the pivot */
	UFUNCTION( BlueprintCallable, meta = (DisplayName = "Get Angle "), Category = "RadialMenu" )
	float GetAngle() const;
	
	/** Broadcast when pressed */
	FVRQuickMenuItemPressedCallback OnPressedDelegate;
};