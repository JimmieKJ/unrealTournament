// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTHUD_TeamDM.h"

#include "UTHUD_Showdown.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTHUD_Showdown : public AUTHUD_TeamDM
{
	GENERATED_UCLASS_BODY()

	/** icon for player starts on the minimap (rotated BG that indicates direction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
	FCanvasIcon PlayerStartBGIcon;

	/** icon for player starts on the minimap (foreground) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
	FCanvasIcon PlayerStartIcon;

	/** drawn over selected player starts on the minimap */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
	UTexture2D* SelectedSpawnTexture;

	/** Sound played when player chooses spawn location. */
	UPROPERTY()
		USoundBase* SpawnSelectSound;

	/** Other player completed selection. */
	UPROPERTY()
		USoundBase* OtherSelectSound;

	/** Actor whose icon the mouse pointer is hovering over last time we checked */
	UPROPERTY(BlueprintReadOnly)
	AActor* LastHoveredActor;

	/** most recent time LastHoveredActor changed - NOTE: This is in RealTimeSeconds! */
	UPROPERTY(BlueprintReadOnly)
	float LastHoveredActorChangeTime;

	/** background for help text over map */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
	FCanvasIcon SpawnHelpTextBG;

	/** time we started flashing the help text because the player is running out of selection time */
	float SpawnTextWarningTime;

	/** True if haven't yet played on deck notification for this round. */
	bool bNeedOnDeckNotify;

	UPROPERTY()
		AUTPlayerState* LastPlayerSelect;

	/** scene capture for spawn preview */
	UPROPERTY(VisibleAnywhere)
	USceneCaptureComponent2D* SpawnPreviewCapture;

	UPROPERTY()
	class UUTHUDWidgetMessage_KillIconMessages* KillIconWidget;

	/** player start being displayed in the spawn preview */
	UPROPERTY()
	APlayerStart* PreviewPlayerStart;
	/** set when spawn preview is waiting on rendering */
	bool bPendingSpawnPreview;

	virtual void BeginPlay() override;

	virtual EInputMode::Type GetInputMode_Implementation() const;
	virtual bool OverrideMouseClick(FKey Key, EInputEvent EventType) override;

	virtual void DrawMinimap(const FColor& DrawColor, float MapSize, FVector2D DrawPos) override;
	virtual void DrawHUD() override;

	/** Show players in order of spawn selection. */
	virtual void DrawPlayerList();

	// get Actor for icon mouse pointer is hovering over
	virtual AActor* FindHoveredIconActor() const;

	virtual UUTHUDWidget* AddHudWidget(TSubclassOf<UUTHUDWidget> NewWidgetClass) override;

protected:
	/** set when PlayerOwner's look input has been locked for interacting with the spawn selection map, so we know to restore the input later */
	bool bLockedLookInput;
};