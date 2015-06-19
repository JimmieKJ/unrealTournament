// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DebugCameraController.generated.h"


/**
* Camera controller that allows you to fly around a level mostly unrestricted by normal movement rules.
*
* To turn it on, please press Alt+C or both (left and right) analogs on XBox pad,
* or use the "ToggleDebugCamera" console command. Check the debug camera bindings
* in DefaultPawn.cpp for the camera controls.
*/
UCLASS(config=Game, hidedropdown)
class ENGINE_API ADebugCameraController
	: public APlayerController
{
	GENERATED_UCLASS_BODY()

	/** Whether to show information about the selected actor on the debug camera HUD. */
	UPROPERTY(globalconfig)
	uint32 bShowSelectedInfo:1;

	/** @todo document */
	UPROPERTY()
	uint32 bIsFrozenRendering:1;

	/** @todo document */
	UPROPERTY()
	class UDrawFrustumComponent* DrawFrustum;
	
	/** @todo document */
	UFUNCTION(exec)
	virtual void ShowDebugSelectedInfo();

	/** Selects the object the camera is aiming at. */
	void SelectTargetedObject();

	/** Called when the user pressed the deselect key, just before the selected actor is cleared. */
	void Unselect();

	/** @todo document */
	void IncreaseCameraSpeed();

	/** @todo document */
	void DecreaseCameraSpeed();

	/** @todo document */
	void IncreaseFOV();

	/** @todo document */
	void DecreaseFOV();

	/** @todo document */
	void ToggleDisplay();

	/**
	 * function called from key bindings command to save information about
	 * turning on/off FreezeRendering command.
	 */
	virtual void ToggleFreezeRendering();

public:

	/** @todo document */
	class AActor* SelectedActor;

	/** @todo document */
	class UPrimitiveComponent* SelectedComponent;

	/** @todo document */
	class APlayerController* OriginalControllerRef;

	/** @todo document */
	class UPlayer* OriginalPlayer;

	// Allows control over the speed of the spectator pawn. This scales the speed based on the InitialMaxSpeed.
	float SpeedScale;
	
	// Initial max speed of the spectator pawn when we start possession.
	float InitialMaxSpeed;

	// Initial acceleration of the spectator pawn when we start possession.
	float InitialAccel;

	// Initial deceleration of the spectator pawn when we start possession.
	float InitialDecel;

protected:

	// Adjusts movement speed limits based on SpeedScale.
	virtual void ApplySpeedScale();
	virtual void SetupInputComponent() override;

public:

	/** Function called on activation debug camera controller */
	void OnActivate(class APlayerController* OriginalPC);

	/** Function called on deactivation debug camera controller */
	void OnDeactivate(class APlayerController* RestoredPC);

	/**
	 * Builds a list of components that are hidden based upon gameplay
	 *
	 * @param ViewLocation the view point to hide/unhide from
	 * @param HiddenComponents the list to add to/remove from
	 */
	virtual void UpdateHiddenComponents(const FVector& ViewLocation,TSet<FPrimitiveComponentId>& HiddenComponents) override;

public:

	// APlayerController Interface

	virtual void PostInitializeComponents() override;
	virtual FString ConsoleCommand(const FString& Command, bool bWriteToLog = true) override;
	virtual void AddCheats(bool bForce) override;
	virtual void EndSpectatingState() override;
	/** Custom spawn to spawn a default SpectatorPawn, to use as a spectator and initialize it. By default it is spawned at the PC's current location and rotation. */
	virtual ASpectatorPawn* SpawnSpectatorPawn() override;

protected:

	/**
	 * Called when an actor has been selected with the primary key (e.g. left mouse button).
	 * @param Hit	Info struct for the selection point.
	 */
	virtual void Select( FHitResult const& Hit );

	virtual void SetSpectatorPawn(class ASpectatorPawn* NewSpectatorPawn) override;

private:

	/** The normalized screen location when a drag starts */
	FVector2D LastTouchDragLocation;

	void OnTouchBegin(ETouchIndex::Type FingerIndex, FVector Location);
	void OnTouchEnd(ETouchIndex::Type FingerIndex, FVector Location);
	void OnFingerMove(ETouchIndex::Type FingerIndex, FVector Location);
};
