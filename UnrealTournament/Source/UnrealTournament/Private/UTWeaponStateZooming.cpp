// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeaponStateZooming.h"

void FZoomTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	if (ZoomState != NULL && !ZoomState->HasAnyFlags(RF_PendingKill | RF_Unreachable) && ZoomState->GetUTOwner() != NULL && ZoomState->GetUTOwner()->GetWeapon() == ZoomState->GetOuterAUTWeapon())
	{
		ZoomState->TickZoom(DeltaTime);
	}
}
FString FZoomTickFunction::DiagnosticMessage()
{
	return *FString::Printf(TEXT("%s::ZoomTick()"), *GetPathNameSafe(ZoomState));
}

UUTWeaponStateZooming::UUTWeaponStateZooming(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	MinFOV = 12.f;
	ZoomTime = 1.0f;
}

void UUTWeaponStateZooming::PendingFireStarted()
{
	if (bIsZoomed)
	{
		bIsZoomed = false;
		APlayerCameraManager* Camera = GetUTOwner()->GetPlayerCameraManager();
		if (Camera != NULL)
		{
			Camera->UnlockFOV();
		}
	}
	else
	{
		bIsZoomed = true;
		StartZoomTime = GetWorld()->TimeSeconds;
		ZoomTickHandler.ZoomState = this;
		ZoomTickHandler.RegisterTickFunction(GetOuterAUTWeapon()->GetLevel());
	}
}
void UUTWeaponStateZooming::PendingFireStopped()
{
	ZoomTickHandler.UnRegisterTickFunction();
}

void UUTWeaponStateZooming::BeginFiringSequence(uint8 FireModeNum)
{
	// this isn't actually firing so immediately switch to other fire mode
	if (FireModeNum != GetOuterAUTWeapon()->GetCurrentFireMode() && GetOuterAUTWeapon()->FiringState.IsValidIndex(FireModeNum) && GetOuterAUTWeapon()->HasAmmo(FireModeNum))
	{
		GetOuterAUTWeapon()->GotoFireMode(FireModeNum);
	}
}
void UUTWeaponStateZooming::EndFiringSequence(uint8 FireModeNum)
{
	GetOuterAUTWeapon()->GotoActiveState();
}

void UUTWeaponStateZooming::TickZoom(float DeltaTime)
{
	APlayerCameraManager* Camera = GetUTOwner()->GetPlayerCameraManager();
	if (Camera != NULL)
	{
		Camera->SetFOV(Camera->DefaultFOV - (Camera->DefaultFOV - MinFOV) * FMath::Min<float>((GetWorld()->TimeSeconds - StartZoomTime) / ZoomTime, 1.0f));
	}
}