#pragma once

#include "UTInGameIntroZoneVisualizationCharacter.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTInGameIntroZoneVisualizationCharacter : public AUTCharacter
{
	GENERATED_UCLASS_BODY()

	void PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir) override;

#if WITH_EDITORONLY_DATA
	
	virtual void PostEditMove(bool bFinished) override;

#endif // WITH_EDITORONLY_DATA
};