// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGameVolume.h"
#include "UTDmgType_Suicide.h"
#include "UTGameState.h"
#include "UTWeaponLocker.h"
#include "UTPlayerState.h"
#include "UTTeleporter.h"
#include "UTFlagRunGameState.h"
#include "UTCharacterVoice.h"
#include "UTRallyPoint.h"

AUTGameVolume::AUTGameVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VolumeName = FText::GetEmpty();
	TeamIndex = 255;
	bShowOnMinimap = true;
	bIsNoRallyZone = false;
	bIsTeamSafeVolume = false;
	bIsTeleportZone = false;
	RouteID = -1;
	bReportDefenseStatus = false;
	bHasBeenEntered = false;
	bHasFCEntry = false;
	MinEnemyInBaseInterval = 7.f;

	static ConstructorHelpers::FObjectFinder<USoundBase> AlarmSoundFinder(TEXT("SoundCue'/Game/RestrictedAssets/Audio/Gameplay/A_FlagRunBaseAlarm.A_FlagRunBaseAlarm'"));
	AlarmSound = AlarmSoundFinder.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> EarnedSoundFinder(TEXT("SoundCue'/Game/RestrictedAssets/Pickups/Health/Asset/A_Pickups_Health_Small_Cue_Modulated.A_Pickups_Health_Small_Cue_Modulated'"));
	HealthSound = EarnedSoundFinder.Object;

	GetBrushComponent()->SetCollisionObjectType(COLLISION_GAMEVOLUME);
}

void AUTGameVolume::Reset_Implementation()
{
	bHasBeenEntered = false;
	bHasFCEntry = false;
}

void AUTGameVolume::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if ((VoiceLinesSet == NAME_None) && !VolumeName.IsEmpty() && !bIsTeleportZone && !bIsTeamSafeVolume)
	{
		VoiceLinesSet = UUTCharacterVoice::StaticClass()->GetDefaultObject<UUTCharacterVoice>()->GetFallbackLines(FName(*VolumeName.ToString()));
		if (VoiceLinesSet == NAME_None)
		{
			UE_LOG(UT, Warning, TEXT("No voice lines found for %s"), *VolumeName.ToString());
		}
	}
}

void AUTGameVolume::HealthTimer()
{
	TArray<AActor*> TouchingActors;
	GetOverlappingActors(TouchingActors, AUTCharacter::StaticClass());

	for (int32 iTouching = 0; iTouching < TouchingActors.Num(); ++iTouching)
	{
		AUTCharacter* const P = Cast<AUTCharacter>(TouchingActors[iTouching]);
		if (P && (GetWorld()->GetTimeSeconds() - P->EnteredSafeVolumeTime) > 1.f)
		{
			if (P->Health < P->HealthMax)
			{
				P->Health = FMath::Max<int32>(P->Health, FMath::Min<int32>(P->Health + 20, P->HealthMax));
				AUTPlayerController* PC = Cast<AUTPlayerController>(P->GetController());
				if (PC)
				{
					PC->ClientPlaySound(HealthSound);
				}
			}
			if ((TeamLockers.Num() > 0) && TeamLockers[0] && P->bHasLeftSafeVolume)
			{
				TeamLockers[0]->ProcessTouch(P);
			}
		}
	}
}

void AUTGameVolume::ActorEnteredVolume(class AActor* Other)
{
	if ((Role == ROLE_Authority) && Cast<AUTCharacter>(Other))
	{
		AUTCharacter* P = ((AUTCharacter*)(Other));
		P->LastGameVolume = this;
		AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
		if (GS != nullptr && P->PlayerState != nullptr && !GS->IsMatchIntermission() && (GS->IsMatchInProgress() || (Cast<AUTPlayerState>(P->PlayerState) && ((AUTPlayerState*)(P->PlayerState))->bIsWarmingUp)))
		{
			if (bIsTeamSafeVolume)
			{
				// friendlies are invulnerable, enemies must die
				if (!GS->OnSameTeam(this, P))
				{
					P->TakeDamage(1000.f, FDamageEvent(UUTDmgType_Suicide::StaticClass()), nullptr, this);
				}
				else
				{
					P->bDamageHurtsHealth = false;
					P->EnteredSafeVolumeTime = GetWorld()->GetTimeSeconds();
					if ((P->Health < 80) && Cast<AUTPlayerState>(P->PlayerState) && (GetWorld()->GetTimeSeconds() - ((AUTPlayerState*)(P->PlayerState))->LastNeedHealthTime > 20.f))
					{
						((AUTPlayerState*)(P->PlayerState))->LastNeedHealthTime = GetWorld()->GetTimeSeconds();
						((AUTPlayerState*)(P->PlayerState))->AnnounceStatus(StatusMessage::NeedHealth, 0, true);
					}
				}
			}
			if (bIsTeleportZone)
			{
				if (AssociatedTeleporter == nullptr)
				{
					for (FActorIterator It(GetWorld()); It; ++It)
					{
						AssociatedTeleporter = Cast<AUTTeleporter>(*It);
						if (AssociatedTeleporter)
						{
							break;
						}
					}
				}
				if (AssociatedTeleporter)
				{
					AssociatedTeleporter->OnOverlapBegin(this, P);
				}
			}
			else if (P->GetCarriedObject())
			{
				/*if (VoiceLinesSet != NAME_None)
				{
					UE_LOG(UT, Warning, TEXT("VoiceLineSet %s for %s location %s"), *VoiceLinesSet.ToString(), *GetName(), *VolumeName.ToString());
					//((AUTPlayerState *)(P->PlayerState))->AnnounceStatus(VoiceLinesSet, 1);
				}
				else
				{
					UE_LOG(UT, Warning, TEXT("No VoiceLineSet for %s location %s"), *GetName(), *VolumeName.ToString());
				}*/
				if (bIsNoRallyZone && !bIsTeamSafeVolume && !P->GetCarriedObject()->bWasInEnemyBase)
				{
					if (GetWorld()->GetTimeSeconds() - P->GetCarriedObject()->EnteredEnemyBaseTime > 2.f)
					{
						// play alarm
						UUTGameplayStatics::UTPlaySound(GetWorld(), AlarmSound, P, SRT_All, false, FVector::ZeroVector, NULL, NULL, false);
					}
					P->GetCarriedObject()->EnteredEnemyBaseTime = GetWorld()->GetTimeSeconds();
				}

				// possibly announce flag carrier changed zones
				if (bIsNoRallyZone && !bIsTeamSafeVolume && !P->GetCarriedObject()->bWasInEnemyBase && (GetWorld()->GetTimeSeconds() - FMath::Min(GS->LastEnemyFCEnteringBaseTime, GS->LastEnteringEnemyBaseTime) > 2.f))
				{
					if ((GetWorld()->GetTimeSeconds() - GS->LastEnteringEnemyBaseTime > 2.f) && Cast<AUTPlayerState>(P->PlayerState))
					{
						((AUTPlayerState *)(P->PlayerState))->AnnounceStatus(StatusMessage::ImGoingIn);
						if (VoiceLinesSet != NAME_None)
						{
							((AUTPlayerState *)(P->PlayerState))->AnnounceStatus(VoiceLinesSet, 1);
							GS->LastFriendlyLocationReportTime = GetWorld()->GetTimeSeconds();
							GS->LastFriendlyLocationName = VoiceLinesSet;
						}
						GS->LastEnteringEnemyBaseTime = GetWorld()->GetTimeSeconds();
					}
					if (GetWorld()->GetTimeSeconds() - GS->LastEnemyFCEnteringBaseTime > 2.f)
					{
						AUTPlayerState* PS = P->GetCarriedObject()->LastPinger;
						if (!PS)
						{
							for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
							{
								AController* C = Iterator->Get();
								if (C && !GS->OnSameTeam(P, C) && Cast<AUTPlayerState>(C->PlayerState))
								{
									PS = ((AUTPlayerState*)(C->PlayerState));
									break;
								}
							}
						}
						if (PS)
						{
							PS->AnnounceStatus(StatusMessage::Incoming);
							if (VoiceLinesSet != NAME_None)
							{
								PS->AnnounceStatus(VoiceLinesSet, 0);
								GS->LastEnemyLocationReportTime = GetWorld()->GetTimeSeconds();
								GS->LastEnemyLocationName = VoiceLinesSet;
							}
							GS->LastEnemyFCEnteringBaseTime = GetWorld()->GetTimeSeconds();
						}
					}
				}
				else if ((GetWorld()->GetTimeSeconds() - FMath::Min(GS->LastFriendlyLocationReportTime, GS->LastEnemyLocationReportTime) > 1.f) || bIsWarningZone || !bHasFCEntry)
				{
					if ((VoiceLinesSet != NAME_None) && ((GetWorld()->GetTimeSeconds() - GS->LastFriendlyLocationReportTime > 1.f) || !bHasFCEntry) && Cast<AUTPlayerState>(P->PlayerState) && (GS->LastFriendlyLocationName != VoiceLinesSet))
					{
						((AUTPlayerState *)(P->PlayerState))->AnnounceStatus(VoiceLinesSet, 1);
						GS->LastFriendlyLocationReportTime = GetWorld()->GetTimeSeconds();
						GS->LastFriendlyLocationName = VoiceLinesSet;
					}
					if ((VoiceLinesSet != NAME_None) && P->GetCarriedObject()->bCurrentlyPinged && P->GetCarriedObject()->LastPinger && ((GetWorld()->GetTimeSeconds() - GS->LastEnemyLocationReportTime > 1.f) || !bHasFCEntry) && (GS->LastEnemyLocationName != VoiceLinesSet))
					{
						P->GetCarriedObject()->LastPinger->AnnounceStatus(VoiceLinesSet, 0);
						GS->LastEnemyLocationReportTime = GetWorld()->GetTimeSeconds();
						GS->LastEnemyLocationName = VoiceLinesSet;
					}
					else if (bIsWarningZone && !P->bWasInWarningZone && !bIsNoRallyZone)
					{
						// force ping if important zone, wasn't already in important zone
						// also do if no pinger if important zone
						P->GetCarriedObject()->LastPingedTime = FMath::Max(P->GetCarriedObject()->LastPingedTime, GetWorld()->GetTimeSeconds() - P->GetCarriedObject()->PingedDuration + 1.f);
						if (VoiceLinesSet != NAME_None)
						{
							AUTPlayerState* Warner = P->GetCarriedObject()->LastPinger;
							if (Warner == nullptr)
							{
								for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
								{
									AController* C = Iterator->Get();
									if (C && !GS->OnSameTeam(P, C) && Cast<AUTPlayerState>(C->PlayerState))
									{
										Warner = ((AUTPlayerState*)(C->PlayerState));
										break;
									}
								}
							}
							if (Warner && ((GetWorld()->GetTimeSeconds() - GS->LastEnemyLocationReportTime > 1.f) || !bHasFCEntry))
							{
								Warner->AnnounceStatus(VoiceLinesSet, 0);
								GS->LastEnemyLocationReportTime = GetWorld()->GetTimeSeconds();
								GS->LastEnemyLocationName = VoiceLinesSet;
							}
						}
					}
				}
				P->bWasInWarningZone = bIsWarningZone;
				P->GetCarriedObject()->bWasInEnemyBase = bIsNoRallyZone && !bIsTeamSafeVolume;
				bHasFCEntry = true;
			}
			else if (bIsNoRallyZone && !bIsTeamSafeVolume && Cast<AUTPlayerState>(P->PlayerState) && ((AUTPlayerState*)(P->PlayerState))->Team && (GS->bRedToCap == (((AUTPlayerState*)(P->PlayerState))->Team->TeamIndex == 0)))
			{
				// warn base is under attack
				if (GetWorld()->GetTimeSeconds() - GS->LastEnemyEnteringBaseTime > MinEnemyInBaseInterval)
				{
					AUTPlayerState* PS = (P->LastTargeter && !GS->OnSameTeam(P, P->LastTargeter)) ? P->LastTargeter : nullptr;
					if (!PS)
					{
						for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
						{
							AController* C = Iterator->Get();
							if (C && !GS->OnSameTeam(P, C) && Cast<AUTPlayerState>(C->PlayerState))
							{
								PS = ((AUTPlayerState*)(C->PlayerState));
								break;
							}
						}
					}
					if (PS)
					{
						PS->AnnounceStatus(StatusMessage::BaseUnderAttack);
						GS->LastEnemyEnteringBaseTime = GetWorld()->GetTimeSeconds();
					}
				}
			}
			else if (!bHasBeenEntered && bReportDefenseStatus && (VoiceLinesSet != NAME_None))
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(P->PlayerState);
				AUTFlagRunGameState* FRGS = GetWorld()->GetGameState<AUTFlagRunGameState>();
				if (PS && PS->Team && FRGS && (FRGS->bRedToCap == (PS->Team->TeamIndex == 1)))
				{
					PS->AnnounceStatus(VoiceLinesSet, 2);
				}
			}
		}
		bHasBeenEntered = true;
	}

	if (!VolumeName.IsEmpty())
	{
		AUTCharacter* P = Cast<AUTCharacter>(Other);
		if (P != nullptr)
		{
			//P->LastKnownLocation = this;
			AUTPlayerState* PS = Cast<AUTPlayerState>(P->PlayerState);
			if (PS != nullptr)
			{
				PS->LastKnownLocation = this;
			}
		}
	}
}

void AUTGameVolume::ActorLeavingVolume(class AActor* Other)
{
	AUTCharacter* UTCharacter = Cast<AUTCharacter>(Other);
	if (UTCharacter)
	{
		if (bIsTeamSafeVolume)
		{
			UTCharacter->bDamageHurtsHealth = true;
			UTCharacter->bHasLeftSafeVolume = true;
			if (UTCharacter->GetController() && (TeamLockers.Num() > 0) && TeamLockers[0])
			{
				TeamLockers[0]->ProcessTouch(UTCharacter);
			}
			AUTPlayerController* PC = Cast<AUTPlayerController>(UTCharacter->GetController());
			if (PC)
			{
				PC->LeftSpawnVolumeTime = GetWorld()->GetTimeSeconds();
			}
		}
	}
}

void AUTGameVolume::SetTeamForSideSwap_Implementation(uint8 NewTeamNum)
{
	TeamIndex = NewTeamNum;
}





