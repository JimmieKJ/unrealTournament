// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGameVolume.h"
#include "UTDmgType_Suicide.h"
#include "UTGameState.h"
#include "UTWeaponLocker.h"
#include "UTPlayerState.h"
#include "UTTeleporter.h"

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
}

void AUTGameVolume::ActorEnteredVolume(class AActor* Other)
{
	if ((Role == ROLE_Authority) && Cast<AUTCharacter>(Other))
	{
		AUTCharacter* P = ((AUTCharacter*)(Other));
		P->LastGameVolume = this;
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != nullptr && P->PlayerState != nullptr)
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
				}
			}
			else if (bIsTeleportZone)
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
					AssociatedTeleporter->OnOverlapBegin(P);
				}
			}
			else if (P->GetCarriedObject())
			{
/*				if (VoiceLinesSet != NAME_None)
				{
					UE_LOG(UT, Warning, TEXT("VoiceLineSet %s for %s location %s"), *VoiceLinesSet.ToString(), *GetName(), *VolumeName.ToString());
					((AUTPlayerState *)(P->PlayerState))->AnnounceStatus(VoiceLinesSet, 1);
				}
				else
				{
					UE_LOG(UT, Warning, TEXT("No VoiceLineSet for %s location %s"), *GetName(), *VolumeName.ToString());
				}
				return;*/
				// possibly announce flag carrier changed zones
				if (bIsNoRallyZone && (GetWorld()->GetTimeSeconds() - FMath::Max(GS->LastEnemyEnteringBaseTime, GS->LastEnteringEnemyBaseTime) > 10.f))
				{
					if ((GetWorld()->GetTimeSeconds() - GS->LastEnteringEnemyBaseTime > 10.f) && Cast<AUTPlayerState>(P->PlayerState))
					{
						if (VoiceLinesSet != NAME_None)
						{
							((AUTPlayerState *)(P->PlayerState))->AnnounceStatus(VoiceLinesSet, 1);
						}
						else
						{
							((AUTPlayerState *)(P->PlayerState))->AnnounceStatus(StatusMessage::ImGoingIn);
						}
						GS->LastEnteringEnemyBaseTime = GetWorld()->GetTimeSeconds();
					}
					if (GetWorld()->GetTimeSeconds() - GS->LastEnemyEnteringBaseTime > 12.f)
					{
						bool bHasVisibility = P->GetCarriedObject()->bCurrentlyPinged;
						AUTPlayerState* PS = P->GetCarriedObject()->LastPinger;
						if (!bHasVisibility)
						{
							for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
							{
								AController* C = *Iterator;
								if (C && !GS->OnSameTeam(P, C) && C->GetPawn() && Cast<AUTPlayerState>(C->PlayerState) && C->LineOfSightTo(P))
								{
									PS = ((AUTPlayerState*)(C->PlayerState));
									if (PS)
									{
										bHasVisibility = true;
										break;
									}
								}
							}
						}
						else if (!PS)
						{
							for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
							{
								AController* C = *Iterator;
								if (C && !GS->OnSameTeam(P, C) && C->GetPawn() && Cast<AUTPlayerState>(C->PlayerState))
								{
									PS = ((AUTPlayerState*)(C->PlayerState));
									break;
								}
							}
						}
						if (bHasVisibility && PS)
						{
							if (VoiceLinesSet != NAME_None)
							{
								((AUTPlayerState *)(P->PlayerState))->AnnounceStatus(VoiceLinesSet, 0);
							}
							else
							{
								PS->AnnounceStatus(StatusMessage::BaseUnderAttack);
							}
							GS->LastEnemyEnteringBaseTime = GetWorld()->GetTimeSeconds();
						}
					}
				}
				else if ((VoiceLinesSet != NAME_None) && (GetWorld()->GetTimeSeconds() - FMath::Max(GS->LastFriendlyLocationReportTime, GS->LastEnemyLocationReportTime) > 3.f) && (GS->LastFriendlyLocationName != VoiceLinesSet))
				{
					if ((GetWorld()->GetTimeSeconds() - GS->LastFriendlyLocationReportTime > 3.f) && Cast<AUTPlayerState>(P->PlayerState))
					{
						((AUTPlayerState *)(P->PlayerState))->AnnounceStatus(VoiceLinesSet, 1);
						GS->LastFriendlyLocationReportTime = GetWorld()->GetTimeSeconds();
						GS->LastFriendlyLocationName = VoiceLinesSet;
					}
					if (P->GetCarriedObject()->bCurrentlyPinged && P->GetCarriedObject()->LastPinger && (GetWorld()->GetTimeSeconds() - GS->LastEnemyLocationReportTime > 3.f) && (GS->LastEnemyLocationName != VoiceLinesSet))
					{
						P->GetCarriedObject()->LastPinger->AnnounceStatus(VoiceLinesSet, 0);
						GS->LastEnemyLocationReportTime = GetWorld()->GetTimeSeconds();
						GS->LastEnemyLocationName = VoiceLinesSet;
					}
				}
			}
		}
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
	if (Other && bIsTeamSafeVolume)
	{
		AUTCharacter* UTCharacter = Cast<AUTCharacter>(Other);
		if (UTCharacter)
		{
			UTCharacter->bDamageHurtsHealth = true;
			if ((Role == ROLE_Authority) && UTCharacter->GetController() && TeamLocker)
			{
				TeamLocker->ProcessTouch(UTCharacter);
			}
		}
	}
}

void AUTGameVolume::SetTeamForSideSwap_Implementation(uint8 NewTeamNum)
{
	TeamIndex = NewTeamNum;
}





