// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WidgetLayoutLibrary.generated.h"

UCLASS()
class UMG_API UWidgetLayoutLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Gets the projected world to screen position for a player, then converts it into a widget
	 * position, which takes into account any quality scaling.
	 * @param PlayerController The player controller to project the position in the world to their screen.
	 * @param WorldLocation The world location to project from.
	 * @param ScreenPosition The position in the viewport with quality scale removed and DPI scale remove.
	 * @return true if the position projects onto the screen.
	 */
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="Viewport")
	static bool ProjectWorldLocationToWidgetPosition(APlayerController* PlayerController, FVector WorldLocation, FVector2D& ScreenPosition);

	/**
	 * Convert a World Space 3D position into a 2D Widget Screen Space position, with distance from the camera the Z component.  This
	 * takes into account any quality scaling as well.
	 *
	 * @return true if the world coordinate was successfully projected to the screen.
	 */
	static bool ProjectWorldLocationToWidgetPositionWithDistance(APlayerController* PlayerController, FVector WorldLocation, FVector& ScreenPosition);

	/**
	 * Gets the current DPI Scale being applied to the viewport and all the Widgets.
	 */
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="Viewport", meta=( WorldContext="WorldContextObject" ))
	static float GetViewportScale(UObject* WorldContextObject);

	/**
	 * Gets the size of the game viewport.
	 */
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="Viewport", meta=( WorldContext="WorldContextObject" ))
	static FVector2D GetViewportSize(UObject* WorldContextObject);

	/**
	 * Gets the mouse position of the player controller, scaled by the DPI.  If you're trying to go from raw mouse screenspace coordinates
	 * to fullscreen widget space, you'll need to transform the mouse into DPI Scaled space.  This function performs that scaling.
	 *
	 * MousePositionScaledByDPI = MousePosition * (1 / ViewportScale).
	 */
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="Viewport")
	static bool GetMousePositionScaledByDPI(APlayerController* Player, float& LocationX, float& LocationY);

	/**
	* Gets the slot object on the child widget as a Border Slot, allowing you to manipulate layout information.
	* @param Widget The child widget of a border panel.
	*/
	UFUNCTION(BlueprintPure, Category = "Slot")
	static class UBorderSlot* SlotAsBorderSlot(UWidget* Widget);

	/**
	 * Gets the slot object on the child widget as a Canvas Slot, allowing you to manipulate layout information.
	 * @param Widget The child widget of a canvas panel.
	 */
	UFUNCTION(BlueprintPure, Category="Slot")
	static UCanvasPanelSlot* SlotAsCanvasSlot(UWidget* Widget);

	/**
	 * Gets the slot object on the child widget as a Grid Slot, allowing you to manipulate layout information.
	 * @param Widget The child widget of a grid panel.
	 */
	UFUNCTION(BlueprintPure, Category="Slot")
	static UGridSlot* SlotAsGridSlot(UWidget* Widget);

	/**
	 * Gets the slot object on the child widget as a Horizontal Box Slot, allowing you to manipulate its information.
	 * @param Widget The child widget of a Horizontal Box.
	 */
	UFUNCTION(BlueprintPure, Category="Slot")
	static UHorizontalBoxSlot* SlotAsHorizontalBoxSlot(UWidget* Widget);

	/**
	 * Gets the slot object on the child widget as a Overlay Slot, allowing you to manipulate layout information.
	 * @param Widget The child widget of a overlay panel.
	 */
	UFUNCTION(BlueprintPure, Category="Slot")
	static UOverlaySlot* SlotAsOverlaySlot(UWidget* Widget);

	/**
	 * Gets the slot object on the child widget as a Uniform Grid Slot, allowing you to manipulate layout information.
	 * @param Widget The child widget of a uniform grid panel.
	 */
	UFUNCTION(BlueprintPure, Category="Slot")
	static UUniformGridSlot* SlotAsUniformGridSlot(UWidget* Widget);

	/**
	 * Gets the slot object on the child widget as a Vertical Box Slot, allowing you to manipulate its information.
	 * @param Widget The child widget of a Vertical Box.
	 */
	UFUNCTION(BlueprintPure, Category = "Slot")
	static UVerticalBoxSlot* SlotAsVerticalBoxSlot(UWidget* Widget);

	/**
	 * Removes all widgets from the viewport.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Viewport", meta=( WorldContext="WorldContextObject" ))
	static void RemoveAllWidgets(UObject* WorldContextObject);
};
