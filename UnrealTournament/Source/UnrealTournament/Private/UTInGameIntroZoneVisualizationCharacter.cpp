#include "UnrealTournament.h"
#include "UTInGameIntroZoneVisualizationCharacter.h"

#include "UTInGameIntroZone.h"

AUTInGameIntroZoneVisualizationCharacter::AUTInGameIntroZoneVisualizationCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AUTInGameIntroZoneVisualizationCharacter::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(PlayerState);
	if (UTPS)
	{
		UTPS->SetPlayerName("TestName12345");
	}
	AUTCharacter::PostRenderFor(PC, Canvas, CameraPosition, CameraDir);
}

void AUTInGameIntroZoneVisualizationCharacter::OnChangeTeamNum()
{
	DynMaterial = UMaterialInstanceDynamic::Create(Material, this);
	DynMaterial->SetScalarParameterValue("Use Team Colors", 1.0f);
	DynMaterial->SetScalarParameterValue("TeamSelect", TeamNum);

	GetMesh()->SetMaterial(0, DynMaterial);
}

#if WITH_EDITORONLY_DATA

void AUTInGameIntroZoneVisualizationCharacter::PostEditMove(bool bFinished)
{
	if (bFinished)
	{
		AUTInGameIntroZone* ZoneOwner = Cast<AUTInGameIntroZone>(GetOwner());
		if (ZoneOwner != nullptr)
		{
			ZoneOwner->UpdateSpawnLocationsWithVisualizationMove();
		}
	}
}

#endif // WITH_EDITORONLY_DATA