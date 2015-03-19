// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeapAttachment_LinkGun.h"

void AUTWeapAttachment_LinkGun::PlayFiringEffects()
{
	uint8 SavedFireMode = UTOwner->FireMode;

	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();

	AActor* LinkTarget = NULL;
	if (UTOwner->FireMode == 10)
	{
		UTOwner->FireMode = 1;
		// linked mode
		if (GS != NULL)
		{
			TArray<FOverlapResult> Hits;
			GetWorld()->OverlapMulti(Hits, UTOwner->FlashLocation, FQuat::Identity, COLLISION_TRACE_WEAPON, FCollisionShape::MakeSphere(10.0f), FCollisionQueryParams(NAME_None, false, UTOwner));
			for (const FOverlapResult& TestHit : Hits)
			{
				if (TestHit.Actor.Get() != NULL && GS->OnSameTeam(TestHit.Actor.Get(), UTOwner))
				{
					LinkTarget = TestHit.Actor.Get();
					UTOwner->FlashLocation = LinkTarget->GetTargetLocation();
				}
			}
		}
	}

	Super::PlayFiringEffects();

	UTOwner->FireMode = SavedFireMode;

	if (MuzzleFlash.Num() > 1 && MuzzleFlash[1] != NULL)
	{
		static FName NAME_TeamColor(TEXT("TeamColor"));
		bool bGotTeamColor = false;
		if (LinkTarget != NULL)
		{
			// apply team color to beam
			uint8 TeamNum = UTOwner->GetTeamNum();
			if (GS->Teams.IsValidIndex(TeamNum) && GS->Teams[TeamNum] != NULL)
			{
				MuzzleFlash[1]->SetVectorParameter(NAME_TeamColor, FVector(GS->Teams[TeamNum]->TeamColor.R, GS->Teams[TeamNum]->TeamColor.G, GS->Teams[TeamNum]->TeamColor.B));
				bGotTeamColor = true;
			}
		}
		if (!bGotTeamColor)
		{
			MuzzleFlash[1]->ClearParameter(NAME_TeamColor);
		}
	}
}

void AUTWeapAttachment_LinkGun::FiringExtraUpdated()
{
	if (UTOwner->FlashExtra > 0 && UTOwner->FireMode == 1)
	{
		// display beam pulse
		LastBeamPulseTime = GetWorld()->TimeSeconds;
	}
}

void AUTWeapAttachment_LinkGun::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (MuzzleFlash.IsValidIndex(1) && MuzzleFlash[1] != NULL)
	{
		static FName NAME_PulseScale(TEXT("PulseScale"));
		float NewScale = 1.0f + FMath::Max<float>(0.0f, 1.0f - (GetWorld()->TimeSeconds - LastBeamPulseTime) / 0.25f);
		MuzzleFlash[1]->SetVectorParameter(NAME_PulseScale, FVector(NewScale, NewScale, NewScale));
	}
}

