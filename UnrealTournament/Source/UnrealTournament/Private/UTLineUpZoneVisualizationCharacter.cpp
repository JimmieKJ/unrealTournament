#include "UnrealTournament.h"
#include "UTLineUpZoneVisualizationCharacter.h"

#if UE_EDITOR
#include "EngineUtils.h"
#include "ISourceControlModule.h"
#include "UnrealEd.h"

#include "HitProxies.h"
#endif

#include "UTLineUpZone.h"

AUTLineUpZoneVisualizationCharacter::AUTLineUpZoneVisualizationCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AUTLineUpZoneVisualizationCharacter::OnChangeTeamNum()
{
	DynMaterial = UMaterialInstanceDynamic::Create(Material, this);
	DynMaterial->SetScalarParameterValue("Use Team Colors", 1.0f);
	DynMaterial->SetScalarParameterValue("TeamSelect", TeamNum);

	GetMesh()->SetMaterial(0, DynMaterial);
}

uint8 AUTLineUpZoneVisualizationCharacter::GetTeamNum() const
{
	return TeamNum;
}

#if WITH_EDITORONLY_DATA

void AUTLineUpZoneVisualizationCharacter::PostEditMove(bool bFinished)
{
	if (bFinished)
	{
		AUTLineUpZone* ZoneOwner = Cast<AUTLineUpZone>(GetOwner());
		if (ZoneOwner != nullptr)
		{
			ZoneOwner->UpdateSpawnLocationsWithVisualizationMove();
		}
	}
}

#endif // WITH_EDITORONLY_DATA