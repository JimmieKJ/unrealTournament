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
	//USelection::SelectObjectEvent.AddUObject(this, &AUTInGameIntroZoneVisualizationCharacter::OnObjectSelected);
}

void AUTLineUpZoneVisualizationCharacter::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(PlayerState);
	if (UTPS)
	{
		UTPS->SetPlayerName("TestName12345");
	}
	AUTCharacter::PostRenderFor(PC, Canvas, CameraPosition, CameraDir);
}

void AUTLineUpZoneVisualizationCharacter::OnChangeTeamNum()
{
	DynMaterial = UMaterialInstanceDynamic::Create(Material, this);
	DynMaterial->SetScalarParameterValue("Use Team Colors", 1.0f);
	DynMaterial->SetScalarParameterValue("TeamSelect", TeamNum);

	GetMesh()->SetMaterial(0, DynMaterial);
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



//void AUTInGameIntroZoneVisualizationCharacter::OnObjectSelected(UObject* Object)
//{
//	if (Object == this && GEditor)
//	{
//		/*FEditorModeTools& FEdTools = GLevelEditorModeTools();
//		
//		TArray<HHitProxy*> AllHitProxiesVisible;
//
//		FIntPoint ViewSize = GEditor->GetActiveViewport()->GetSizeXY();
//		GEditor->GetActiveViewport()->GetHitProxyMap(FIntRect(0, 0, ViewSize.X, ViewSize.Y), AllHitProxiesVisible);
//
//		for (HHitProxy* HitProxy : AllHitProxiesVisible)
//		{
//			
//		}*/
//		//HPropertyWidgetProxy TempProxy(FString(TEXT("FFATeamSpawnLocations")), 0, true);
//
//		//FEdTools->HandleClick(nullptr, TempProxy, Click());
//		
//		//AUTInGameIntroZone* SpawnOwner = Cast<AUTInGameIntroZone>(GetAttachParentActor());
//		//if (SpawnOwner)
//		//{
//		//	GEditor->SelectNone(true, true, false);
//		//	GEditor->SelectActor(SpawnOwner, true, false, false, false);
//		//}
//	}
//}
#endif // WITH_EDITORONLY_DATA