// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VREditorTeleporter.generated.h"

/**
* VR Editor teleport manager
*/
UCLASS()
class UVREditorTeleporter : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Initializes the teleport system */
	void Init( class IVREditorMode* InMode );

	/** Shuts down the teleport system */
	void Shutdown();

	/** Ticks the current teleport */
	void Tick( const float DeltaTime );

	/** Returns true if we're actively teleporting */
	bool IsTeleporting() const
	{
		return bIsTeleporting;
	}

	/** Start teleporting, does a ray trace with the hand passed and calculates the locations for lerp movement in Teleport */
	void StartTeleport( class UViewportInteractor* Interactor );

private:

	/** Called when the user presses a button on their motion controller device */
	void OnInteractorAction( class FEditorViewportClient& ViewportClient, UViewportInteractor* Interactor,
		const struct FViewportActionKeyInput& Action, bool& bOutIsInputCaptured, bool& bWasHandled );

	/** Actually teleport to a position */
	void Teleport( const float DeltaTime );

	//
	// Fields
	//

	/** Owning mode */
	IVREditorMode* VRMode;

	/** If we are currently teleporting */
	bool bIsTeleporting;

	/** The current lerp of the teleport between the TeleportStartLocation and the TeleportGoalLocation */
	float TeleportLerpAlpha;

	/** Set on the current Roomspace location in the world in StartTeleport before doing the actual teleporting */
	FVector TeleportStartLocation;

	/** The calculated goal location in StartTeleport to move the Roomspace to */
	FVector TeleportGoalLocation;

	/** So we can teleport on a light press and make sure a second teleport by a fullpress doesn't happen */
	bool bJustTeleported;

	/** Teleport sound */
	UPROPERTY()
	class USoundCue* TeleportSound;

};