// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "VREditorBaseUserWidget.h"
#include "VREditorRadialMenu.generated.h"

class UVREditorRadialMenuItem;

/**
 * A simple radial menu for VR editing, with frequently-used features
 */
UCLASS( Blueprintable )
class UVREditorRadialMenu : public UVREditorBaseUserWidget
{
	GENERATED_BODY()

public:
	/** Default constructor that sets up CDO properties */
	UVREditorRadialMenu( const FObjectInitializer& ObjectInitializer );

	/** Update the state of the widget with the trackpad location */
	void Update( const class UVREditorInteractor* Interactor );

	/** To bind the buttons from UMG */
	UFUNCTION(BlueprintCallable, Category = "RadialItems")
	void SetButtons( UVREditorRadialMenuItem* InitTopItem, UVREditorRadialMenuItem* InitTopRightItem, UVREditorRadialMenuItem* InitRightItem, UVREditorRadialMenuItem* InitBottomRightItem,
		UVREditorRadialMenuItem* InitBottomItem, UVREditorRadialMenuItem* InitLeftBottomItem, UVREditorRadialMenuItem* InitLeftItem, UVREditorRadialMenuItem* InitTopLeftItem );

	/** Resets the current item to not hovering */
	void ResetItem();

	/** Press the current item */
	void SelectCurrentItem();

	/** If the users has his trackpad or analog stick inside the radius where buttons will not be selected */
	bool IsInMenuRadius() const;

	/** Current position of the trackpad to render line in UMG */
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "RadialMenu")
	FVector2D TrackpadPosition;

	/** Current angle of the trackpad position (-180 to 180) to visualize in UMG */
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "RadialMenu" )
	float TrackpadAngle;

private:
	/** All the buttons in the radial menu */
	UPROPERTY()
	TArray<UVREditorRadialMenuItem*> MenuItems;

	/** The current widget currently selected */
	UPROPERTY()
	UVREditorRadialMenuItem* CurrentItem;

	/** The previous item that was selected */
	UPROPERTY()
	UVREditorRadialMenuItem* PreviousItem;

	/** Updates the label of the gizmo type item */
	void UpdateGizmoTypeLabel();

	/** Temporary functions for delegates of the buttons */
	void OnGizmoCycle();
	void OnDuplicateButtonClicked();
	void OnDeleteButtonClicked();
	void OnUndoButtonClicked();
	void OnRedoButtonClicked();
	void OnCopyButtonClicked();
	void OnPasteButtonClicked();
	void OnSnapActorsToGroundClicked();
};