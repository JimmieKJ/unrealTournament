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
	UTexture2D* PlayerStartBGTexture;
	/** icon for player starts on the minimap (foreground) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
	UTexture2D* PlayerStartTexture;
	/** drawn over selected player starts on the minimap */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
	UTexture2D* SelectedSpawnTexture;

	/** scene capture for spawn preview */
	UPROPERTY(VisibleAnywhere)
	USceneCaptureComponent2D* SpawnPreviewCapture;
	/** player start being displayed in the spawn preview */
	UPROPERTY()
	APlayerStart* PreviewPlayerStart;
	/** set when spawn preview is waiting on rendering */
	bool bPendingSpawnPreview;

	virtual void BeginPlay() override;

	virtual EInputMode::Type GetInputMode_Implementation();
	virtual bool OverrideMouseClick(FKey Key, EInputEvent EventType) override;

	virtual void DrawMinimap(const FColor& DrawColor, float MapSize, FVector2D DrawPos) override;
	virtual void DrawHUD() override;

	// get PlayerStart icon mouse pointer is hovering over
	virtual APlayerStart* GetHoveredPlayerStart() const;

protected:
	/** set when PlayerOwner's look input has been locked for interacting with the spawn selection map, so we know to restore the input later */
	bool bLockedLookInput;
};