#include "UnrealTournament.h"
#include "UTInGameIntroZoneVisualizationCharacter.h"

#include "UTInGameIntroZone.h"

AUTInGameIntroZoneVisualizationCharacter::AUTInGameIntroZoneVisualizationCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AUTInGameIntroZoneVisualizationCharacter::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{

}

#if WITH_EDITORONLY_DATA

void AUTInGameIntroZoneVisualizationCharacter::PostEditMove(bool bFinished)
{
	if (bFinished)
	{
		AUTInGameIntroZoneTeamSpawnPointList* ZoneOwner = Cast<AUTInGameIntroZoneTeamSpawnPointList>(GetOwner());
		if (ZoneOwner != nullptr)
		{
			ZoneOwner->UpdateSpawnLocationsWithVisualizationMove();
		}
	}
}

#endif // WITH_EDITORONLY_DATA