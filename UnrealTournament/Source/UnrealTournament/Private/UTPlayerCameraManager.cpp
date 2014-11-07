// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPlayerCameraManager.h"
#include "UTCTFFlagBase.h"

AUTPlayerCameraManager::AUTPlayerCameraManager(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	FreeCamOffset = FVector(-256, 0, 90);
	EndGameFreeCamOffset = FVector(-256, 0, 45);
	EndGameFreeCamDistance = 55.0f;
	FlagBaseFreeCamOffset = FVector(0, 0, 90);
	bUseClientSideCameraUpdates = false;

	DefaultPPSettings.SetBaseValues();
	DefaultPPSettings.bOverride_AmbientCubemapIntensity = true;
	DefaultPPSettings.bOverride_BloomIntensity = true;
	DefaultPPSettings.bOverride_BloomDirtMaskIntensity = true;
	DefaultPPSettings.bOverride_AutoExposureMinBrightness = true;
	DefaultPPSettings.bOverride_AutoExposureMaxBrightness = true;
	DefaultPPSettings.bOverride_LensFlareIntensity = true;
	DefaultPPSettings.bOverride_MotionBlurAmount = true;
	DefaultPPSettings.bOverride_AntiAliasingMethod = true;
	DefaultPPSettings.bOverride_ScreenSpaceReflectionIntensity = true;
	DefaultPPSettings.BloomIntensity = 0.20f;
	DefaultPPSettings.BloomDirtMaskIntensity = 0.0f;
	DefaultPPSettings.AutoExposureMinBrightness = 1.0f;
	DefaultPPSettings.AutoExposureMaxBrightness = 1.0f;
	DefaultPPSettings.VignetteIntensity = 0.20f;
	DefaultPPSettings.MotionBlurAmount = 0.0f;
	DefaultPPSettings.AntiAliasingMethod = AAM_FXAA;
	DefaultPPSettings.ScreenSpaceReflectionIntensity = 0.0f;

	LastThirdPersonCameraLoc = FVector(0);
	ThirdPersonCameraSmoothingSpeed = 6.0f;
}

FName AUTPlayerCameraManager::GetCameraStyleWithOverrides() const
{
	static const FName NAME_FreeCam = FName(TEXT("FreeCam"));

	AUTCharacter* UTCharacter = Cast<AUTCharacter>(GetViewTarget());

	bool bViewingKiller = false;
	if (PCOwner != nullptr)
	{
		AUTPlayerState* UTPSOwner = Cast<AUTPlayerState>(PCOwner->PlayerState);
		if (UTPSOwner != nullptr && UTCharacter != nullptr && UTPSOwner->LastKillerPlayerState != nullptr)
		{
			if (UTCharacter->PlayerState == UTPSOwner->LastKillerPlayerState)
			{
				bViewingKiller = true;
			}
		}
	}

	// force third person if target is dead, ragdoll or emoting
	if (UTCharacter != NULL && (UTCharacter->IsDead() || UTCharacter->IsRagdoll() || UTCharacter->EmoteCount > 0 || bViewingKiller))
	{
		return NAME_FreeCam;
	}
	else
	{
		AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
		return (GameState != NULL) ? GameState->OverrideCameraStyle(PCOwner, CameraStyle) : CameraStyle;
	}
}

void AUTPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	static const FName NAME_FreeCam = FName(TEXT("FreeCam"));
	static const FName NAME_GameOver = FName(TEXT("GameOver"));	

	FName SavedCameraStyle = CameraStyle;
	CameraStyle = GetCameraStyleWithOverrides();
	AUTCharacter* UTCharacter = Cast<AUTCharacter>(OutVT.Target);
	AUTCTFFlagBase* UTFlagBase = Cast<AUTCTFFlagBase>(OutVT.Target);

	// smooth third person camera all the time
	if (CameraStyle == NAME_FreeCam)
	{
		OutVT.POV.FOV = DefaultFOV;
		OutVT.POV.OrthoWidth = DefaultOrthoWidth;
		OutVT.POV.bConstrainAspectRatio = false;
		OutVT.POV.ProjectionMode = bIsOrthographic ? ECameraProjectionMode::Orthographic : ECameraProjectionMode::Perspective;
		OutVT.POV.PostProcessBlendWeight = 1.0f;

		FVector DesiredLoc = OutVT.Target->GetActorLocation();

		// we must use the capsule location here as the ragdoll's root component can be rubbbing a wall
		if (UTCharacter != nullptr && UTCharacter->IsRagdoll() && UTCharacter->CapsuleComponent != nullptr)
		{
			DesiredLoc = UTCharacter->CapsuleComponent->GetComponentLocation();
		}

		if (UTFlagBase != nullptr)
		{
			DesiredLoc += FlagBaseFreeCamOffset;
		}

		FRotator Rotator = PCOwner->GetControlRotation();

		FVector Loc = DesiredLoc;
		if (LastThirdPersonCameraLoc != FVector(0))
		{
			Loc = FMath::VInterpTo(LastThirdPersonCameraLoc, DesiredLoc, DeltaTime, ThirdPersonCameraSmoothingSpeed);
		}
		LastThirdPersonCameraLoc = Loc;

		float CameraDistance = FreeCamDistance;
		FVector CameraOffset = FreeCamOffset;
		AUTPlayerController* UTPC = Cast<AUTPlayerController>(PCOwner);
		if (UTPC != nullptr && UTPC->GetStateName() == NAME_GameOver)
		{
			CameraDistance = EndGameFreeCamDistance;
			CameraOffset = EndGameFreeCamOffset;
		}

		FVector Pos = Loc + FRotationMatrix(Rotator).TransformVector(CameraOffset) - Rotator.Vector() * CameraDistance;
		FCollisionQueryParams BoxParams(NAME_FreeCam, false, this);
		BoxParams.AddIgnoredActor(OutVT.Target);
		FHitResult Result;

		GetWorld()->SweepSingle(Result, Loc, Pos, FQuat::Identity, ECC_Camera, FCollisionShape::MakeBox(FVector(12.f)), BoxParams);
		OutVT.POV.Location = !Result.bBlockingHit ? Pos : Result.Location;
		OutVT.POV.Rotation = Rotator;

		// Synchronize the actor with the view target results
		SetActorLocationAndRotation(OutVT.POV.Location, OutVT.POV.Rotation, false);
	}
	else
	{
		LastThirdPersonCameraLoc = FVector(0);

		Super::UpdateViewTarget(OutVT, DeltaTime);
	}

	CameraStyle = SavedCameraStyle;
}

void AUTPlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ApplyCameraModifiers(DeltaTime, InOutPOV);

	// if no PP volumes, force our default PP in at the beginning
	if (GetWorld()->PostProcessVolumes.Num() == 0)
	{
		PostProcessBlendCache.Insert(DefaultPPSettings, 0);
		PostProcessBlendCacheWeights.Insert(1.0f, 0);
	}
}