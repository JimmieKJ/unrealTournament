// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFCapturePoint.h"
#include "UTCTFFlagBaseCapturePoint.h"

AUTCTFFlagBaseCapturePoint::AUTCTFFlagBaseCapturePoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void AUTCTFFlagBaseCapturePoint::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bIsCapturePointActive)
	{
		AUTCharacter* Character = Cast<AUTCharacter>(OtherActor);
		if (Character != NULL && Role == ROLE_Authority)
		{
			AUTCTFFlag* CharFlag = Cast<AUTCTFFlag>(Character->GetCarriedObject());
			if (CharFlag != NULL && CharFlag != CarriedObject && CarriedObject != NULL && CarriedObject->ObjectState == CarriedObjectState::Home && CharFlag->GetTeamNum() != GetTeamNum() &&
				!GetWorld()->LineTraceTestByChannel(OtherActor->GetActorLocation(), Capsule->GetComponentLocation(), ECC_Pawn, FCollisionQueryParams(), WorldResponseParams))
			{
				AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
				if (GS == NULL || (GS->IsMatchInProgress() && !GS->IsMatchIntermission()))
				{
					DeliveredFlag = CharFlag;

					bIsCapturePointActive = true;
					CharFlag->PlayCaptureEffect();
					
					CharFlag->bAnyoneCanPickup = false;
					CharFlag->bFriendlyCanPickup = false;
					CharFlag->bAnyoneCanPickup = false;
					CharFlag->bGradualAutoReturn = false;
					Character->DropCarriedObject();
					CharFlag->ChangeState(CarriedObjectState::Delivered);

					ClearDefenseEffect();

					if (CapturePoint)
					{
						CapturePoint->bIsActive = true;
						CapturePoint->OnCaptureCompletedDelegate.BindDynamic(this, &AUTCTFFlagBaseCapturePoint::OnCapturePointFinished);
						OnCapturePointActivated();
					}
				}
			}
		}
	}
}

void AUTCTFFlagBaseCapturePoint::OnCapturePointFinished()
{
	if (CapturePoint)
	{
		//Find what enemy captured our point.
		AUTCharacter* UTCaptureChar = nullptr;
		for (int index = 0; index < CapturePoint->GetCharactersInCapturePoint().Num(); ++index)
		{
			AUTCharacter* UTChar = CapturePoint->GetCharactersInCapturePoint()[index];
			if (UTChar && (UTChar->GetTeamNum() != TeamNum))
			{
				UTCaptureChar = UTChar;
				break;
			}
		}

		if (UTCaptureChar && DeliveredFlag)
		{
			DeliveredFlag->Score(FName(TEXT("FlagCapture")), UTCaptureChar, Cast<AUTPlayerState>(UTCaptureChar->PlayerState));
			DeliveredFlag->SendHome();
		}

		CapturePoint->bIsActive = false;
	}
}

void AUTCTFFlagBaseCapturePoint::Reset_Implementation()
{
	bIsCapturePointActive = false;
	DeliveredFlag = nullptr;

	if (CapturePoint)
	{
		CapturePoint->bIsActive = false;
	}
}

void AUTCTFFlagBaseCapturePoint::OnCapturePointActivated_Implementation()
{

}