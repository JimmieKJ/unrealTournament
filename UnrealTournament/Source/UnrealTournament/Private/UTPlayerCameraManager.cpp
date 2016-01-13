// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPlayerCameraManager.h"
#include "UTCTFFlagBase.h"
#include "UTViewPlaceholder.h"

AUTPlayerCameraManager::AUTPlayerCameraManager(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FreeCamOffset = FVector(-256.f, 0.f, 90.f);
	EndGameFreeCamOffset = FVector(-256.f, 0.f, 45.f);
	EndGameFreeCamDistance = 55.0f;

	DeathCamDistance = 55.f;
	DeathCamOffset = FVector(-128.f, 0.f, 30.f);
	FlagBaseFreeCamOffset = FVector(0, 0, 90);
	bUseClientSideCameraUpdates = false;
	
	PrimaryActorTick.bTickEvenWhenPaused = true;
	ViewPitchMin = -89.0f;
	ViewPitchMax = 89.0f;

	DefaultPPSettings.SetBaseValues();
	DefaultPPSettings.bOverride_AmbientCubemapIntensity = true;
	DefaultPPSettings.bOverride_BloomIntensity = true;
	DefaultPPSettings.bOverride_BloomDirtMaskIntensity = true;
	DefaultPPSettings.bOverride_AutoExposureMinBrightness = true;
	DefaultPPSettings.bOverride_AutoExposureMaxBrightness = true;
	DefaultPPSettings.bOverride_LensFlareIntensity = true;
	DefaultPPSettings.bOverride_MotionBlurAmount = true;
	DefaultPPSettings.bOverride_ScreenSpaceReflectionIntensity = true;
	DefaultPPSettings.BloomIntensity = 0.20f;
	DefaultPPSettings.BloomDirtMaskIntensity = 0.0f;
	DefaultPPSettings.AutoExposureMinBrightness = 1.0f;
	DefaultPPSettings.AutoExposureMaxBrightness = 1.0f;
	DefaultPPSettings.VignetteIntensity = 0.20f;
	DefaultPPSettings.MotionBlurAmount = 0.0f;
	DefaultPPSettings.ScreenSpaceReflectionIntensity = 0.0f;

	StylizedPPSettings.AddZeroed();
	StylizedPPSettings[0].bOverride_BloomIntensity = true;
	StylizedPPSettings[0].bOverride_MotionBlurAmount = true;
	StylizedPPSettings[0].BloomIntensity = 0.20f;
	StylizedPPSettings[0].MotionBlurAmount = 0.0f;
	StylizedPPSettings[0].bOverride_MotionBlurAmount = true;
	StylizedPPSettings[0].bOverride_MotionBlurMax = true;
	StylizedPPSettings[0].bOverride_DepthOfFieldDepthBlurAmount = true;
	StylizedPPSettings[0].bOverride_DepthOfFieldFocalDistance = true;
	StylizedPPSettings[0].bOverride_DepthOfFieldScale = true;
	StylizedPPSettings[0].bOverride_DepthOfFieldNearBlurSize = true;
	StylizedPPSettings[0].bOverride_DepthOfFieldFarBlurSize = true;

	LastThirdPersonCameraLoc = FVector::ZeroVector;
	ThirdPersonCameraSmoothingSpeed = 6.0f;
	CurrentCameraRoll = 0.f;
	WallSlideCameraRoll = 12.5f;
}

// @TODO FIXMESTEVE SPLIT OUT true spectator controls
FName AUTPlayerCameraManager::GetCameraStyleWithOverrides() const
{
	static const FName NAME_FreeCam = FName(TEXT("FreeCam"));
	static const FName NAME_FirstPerson = FName(TEXT("FirstPerson"));
	static const FName NAME_Default = FName(TEXT("Default"));

	AActor* ViewTarget = GetViewTarget();
	ACameraActor* CameraActor = Cast<ACameraActor>(ViewTarget);
	if (CameraActor)
	{
		return NAME_Default;
	}

	AUTCharacter* UTCharacter = Cast<AUTCharacter>(ViewTarget);
	if (UTCharacter == NULL)
	{
		return ((ViewTarget == PCOwner->GetPawn()) || (ViewTarget == PCOwner->GetSpectatorPawn())) ? NAME_FirstPerson : NAME_FreeCam;
	}
	else if (UTCharacter->IsDead() || UTCharacter->IsRagdoll() || UTCharacter->IsThirdPersonTaunting())
	{
		// force third person if target is dead, ragdoll or emoting
		return NAME_FreeCam;
	}
	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	return (GameState != NULL) ? GameState->OverrideCameraStyle(PCOwner, CameraStyle) : CameraStyle;
}

void AUTPlayerCameraManager::UpdateCamera(float DeltaTime)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		CameraStyle = NAME_Default;
		LastThirdPersonCameraLoc = FVector::ZeroVector;
		ViewTarget.CheckViewTarget(PCOwner);
		// our camera is now viewing there
		FMinimalViewInfo NewPOV;
		NewPOV.FOV = DefaultFOV;
		NewPOV.OrthoWidth = DefaultOrthoWidth;
		NewPOV.bConstrainAspectRatio = false;
		NewPOV.ProjectionMode = bIsOrthographic ? ECameraProjectionMode::Orthographic : ECameraProjectionMode::Perspective;
		NewPOV.PostProcessBlendWeight = 1.0f;

		const bool bK2Camera = BlueprintUpdateCamera(ViewTarget.Target, NewPOV.Location, NewPOV.Rotation,NewPOV.FOV);
		if (!bK2Camera)
		{
			ViewTarget.Target->CalcCamera(DeltaTime, NewPOV);
		}

		// Cache results
		FillCameraCache(NewPOV);
	}
	else
	{
		Super::UpdateCamera(DeltaTime);
	}
}

float AUTPlayerCameraManager::RatePlayerCamera(AUTPlayerState* InPS, AUTCharacter *Character, APlayerState* CurrentCamPS)
{
	// 100 is about max
	float Score = 1.f;
	if (InPS == CurrentCamPS)
	{
		Score += CurrentCamBonus;
	}
	float LastActionTime = GetWorld()->GetTimeSeconds() - FMath::Max(Character->LastTakeHitTime, Character->LastWeaponFireTime);
	Score += FMath::Max(0.f, CurrentActionBonus - LastActionTime);

	if (InPS->CarriedObject)
	{
		Score += FlagCamBonus;
	}

	if (Character->GetWeaponOverlayFlags() != 0)
	{
		Score += PowerupBonus;
	}

	if (CurrentCamPS)
	{
		Score += (InPS->Score > CurrentCamPS->Score) ? HigherScoreBonus : -1.f * HigherScoreBonus;
	}

	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		Score += GameState->ScoreCameraView(InPS, Character);
	}
	// todo - have redeemer, armor, etc

	return Score;
}

void AUTPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	static const FName NAME_FreeCam = FName(TEXT("FreeCam"));
	static const FName NAME_GameOver = FName(TEXT("GameOver"));	

	FName SavedCameraStyle = CameraStyle;
	CameraStyle = GetCameraStyleWithOverrides();

	// smooth third person camera all the time
	if (OutVT.Target == PCOwner)
	{
		OutVT.POV.FOV = DefaultFOV;
		OutVT.POV.OrthoWidth = DefaultOrthoWidth;
		OutVT.POV.bConstrainAspectRatio = false;
		OutVT.POV.ProjectionMode = bIsOrthographic ? ECameraProjectionMode::Orthographic : ECameraProjectionMode::Perspective;
		OutVT.POV.PostProcessBlendWeight = 1.0f;

		ApplyCameraModifiers(DeltaTime, OutVT.POV);

		if (OutVT.POV.Location.IsZero())
		{
			OutVT.POV.Location = PCOwner->GetFocalLocation();
		}
		OutVT.POV.Rotation = PCOwner->GetControlRotation();
	}
	else if (CameraStyle == NAME_FreeCam)
	{
		AUTCharacter* UTCharacter = Cast<AUTCharacter>(OutVT.Target);
		AUTCTFFlagBase* UTFlagBase = Cast<AUTCTFFlagBase>(OutVT.Target);
		AUTCarriedObject* UTFlag = Cast<AUTCarriedObject>(OutVT.Target);
		OutVT.POV.FOV = DefaultFOV;
		OutVT.POV.OrthoWidth = DefaultOrthoWidth;
		OutVT.POV.bConstrainAspectRatio = false;
		OutVT.POV.ProjectionMode = bIsOrthographic ? ECameraProjectionMode::Orthographic : ECameraProjectionMode::Perspective;
		OutVT.POV.PostProcessBlendWeight = 1.0f;

		FVector DesiredLoc = (Cast<AController>(OutVT.Target) && !LastThirdPersonCameraLoc.IsZero()) ? LastThirdPersonCameraLoc : OutVT.Target->GetActorLocation();
		AActor* TargetActor = OutVT.Target;
		if (UTCharacter != nullptr && UTCharacter->IsRagdoll() && UTCharacter->GetCapsuleComponent() != nullptr)
		{
			// we must use the capsule location here as the ragdoll's root component can be rubbing a wall
			DesiredLoc = UTCharacter->GetCapsuleComponent()->GetComponentLocation();
		}
		else if (UTFlagBase != nullptr)
		{
			DesiredLoc += FlagBaseFreeCamOffset;
		}
		else if (UTFlag && (GetWorld()->GetTimeSeconds() < UTFlag->LastTeleportedTime + 2.5f))
		{
			DesiredLoc = UTFlag->LastTeleportedLoc;
			LastThirdPersonTarget = UTFlag;
			TargetActor = UTFlag;
		}
		else if (Cast<APlayerState>(OutVT.Target))
		{
			DesiredLoc = LastThirdPersonCameraLoc;
		}

		//If camera is jumping to a new location and we have LOS then interp 
		bool bSnapCamera = ((UTCharacter && UTCharacter->IsDead()) || LastThirdPersonCameraLoc.IsZero() || (TargetActor != LastThirdPersonTarget));
		if (!bSnapCamera && ((DesiredLoc - LastThirdPersonCameraLoc).SizeSquared() > 250000.f))
		{
			FHitResult Result;
			CheckCameraSweep(Result, TargetActor, DesiredLoc, LastThirdPersonCameraLoc);
			bSnapCamera = Result.bBlockingHit;
		}
		FVector Loc = bSnapCamera ? DesiredLoc : FMath::VInterpTo(LastThirdPersonCameraLoc, DesiredLoc, DeltaTime, ThirdPersonCameraSmoothingSpeed);

		AUTCharacter* BaseChar = UTCharacter;
		if (!BaseChar)
		{
			BaseChar = UTFlag && UTFlag->Holder ? Cast<AUTCharacter>(UTFlag->AttachmentReplication.AttachParent) : NULL;
		}
		if (BaseChar && BaseChar->GetMovementBase() && MovementBaseUtility::UseRelativeLocation(BaseChar->GetMovementBase()))
		{
			// don't smooth vertically if on lift
			Loc.Z = DesiredLoc.Z;
		}

		LastThirdPersonCameraLoc = Loc;
		LastThirdPersonTarget = TargetActor;

		AUTPlayerController* UTPC = Cast<AUTPlayerController>(PCOwner);
		bool bGameOver = (UTPC != nullptr && UTPC->GetStateName() == NAME_GameOver);
		bool bUseDeathCam = !bGameOver && UTCharacter && (UTCharacter->IsDead() || UTCharacter->IsRagdoll());

		float CameraDistance = bUseDeathCam ? DeathCamDistance : FreeCamDistance;
		FVector CameraOffset = bUseDeathCam ? DeathCamOffset : FreeCamOffset;
		if (bGameOver)
		{
			CameraDistance = EndGameFreeCamDistance;
			CameraOffset = EndGameFreeCamOffset;
		}
		FRotator Rotator = (!UTPC || UTPC->bSpectatorMouseChangesView) ? PCOwner->GetControlRotation() : UTPC->GetSpectatingRotation(Loc, DeltaTime);
		if (bUseDeathCam)
		{
			Rotator.Pitch = FRotator::NormalizeAxis(Rotator.Pitch);
			Rotator.Pitch = FMath::Clamp(Rotator.Pitch, -85.f, -5.f);
		}
		if (Cast<AUTProjectile>(TargetActor) && !TargetActor->IsPendingKillPending())
		{
			Rotator = TargetActor->GetVelocity().Rotation();
			CameraDistance = 60.f;
			Loc = DesiredLoc;
		}

		FVector Pos = Loc + FRotationMatrix(Rotator).TransformVector(CameraOffset) - Rotator.Vector() * CameraDistance;
		FHitResult Result;
		CheckCameraSweep(Result, TargetActor, Loc, Pos);
		OutVT.POV.Location = !Result.bBlockingHit ? Pos : Result.Location;
		OutVT.POV.Rotation = Rotator;
		ApplyCameraModifiers(DeltaTime, OutVT.POV);

		// Synchronize the actor with the view target results
		SetActorLocationAndRotation(OutVT.POV.Location, OutVT.POV.Rotation, false);
	}
	else
	{
		LastThirdPersonCameraLoc = FVector::ZeroVector;
		Super::UpdateViewTarget(OutVT, DeltaTime);
		AUTCharacter* UTCharacter = Cast<AUTCharacter>(OutVT.Target);
		if (UTCharacter)
		{
			float DesiredRoll = 0.f;
			if (UTCharacter->bApplyWallSlide)
			{
				FVector Cross = UTCharacter->GetActorRotation().Vector() ^ FVector(0.f, 0.f, 1.f);
				DesiredRoll = -1.f * (Cross.GetSafeNormal() | UTCharacter->UTCharacterMovement->WallSlideNormal) * WallSlideCameraRoll;
			}
			else if (UTCharacter->UTCharacterMovement->bIsFloorSliding)
			{
				FVector Cross = UTCharacter->GetVelocity().GetSafeNormal() ^ FVector(0.f, 0.f, 1.f);
				DesiredRoll = -1.f * (Cross.GetSafeNormal() | UTCharacter->GetActorRotation().Vector()) * WallSlideCameraRoll;

			}
			float AdjustRate = FMath::Min(1.f, 10.f*DeltaTime);
			CurrentCameraRoll = (1.f - AdjustRate) * CurrentCameraRoll + AdjustRate*DesiredRoll;
			OutVT.POV.Rotation.Roll = CurrentCameraRoll;
		}
	}

	CameraStyle = SavedCameraStyle;
}

void AUTPlayerCameraManager::CheckCameraSweep(FHitResult& OutHit, AActor* TargetActor, const FVector& Start, const FVector& End)
{
	static const FName NAME_FreeCam = FName(TEXT("FreeCam"));
	FCollisionQueryParams BoxParams(NAME_FreeCam, false, TargetActor);

	AUTCTFFlag* Flag = Cast<AUTCTFFlag>(TargetActor);
	if (Flag)
	{
		if (Flag->Holder)
		{
			BoxParams.AddIgnoredActor(Flag->Holder);
		}
		if (Flag->HomeBase)
		{
			BoxParams.AddIgnoredActor(Flag->HomeBase);
		}
		if (Flag->AttachmentReplication.AttachParent)
		{
			BoxParams.AddIgnoredActor(Flag->AttachmentReplication.AttachParent);
		}
	}
	GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_Camera, FCollisionShape::MakeBox(FVector(12.f)), BoxParams);
}

static TAutoConsoleVariable<int32> CVarBloomDirt(TEXT("r.BloomDirt"), 0, TEXT("Enables screen dirt effect in high light/bloom"), ECVF_Default);

void AUTPlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ApplyCameraModifiers(DeltaTime, InOutPOV);

	// if no PP volumes, force our default PP in at the beginning
	if (GetWorld()->PostProcessVolumes.Num() == 0)
	{
		PostProcessBlendCache.Insert(DefaultPPSettings, 0);
		PostProcessBlendCacheWeights.Insert(1.0f, 0);
	}

	AUTPlayerController* UTPCOwner = Cast<AUTPlayerController>(PCOwner);
	if (UTPCOwner && UTPCOwner->StylizedPPIndex != INDEX_NONE)
	{
		PostProcessBlendCache.Insert(StylizedPPSettings[UTPCOwner->StylizedPPIndex], 0);
		PostProcessBlendCacheWeights.Insert(1.0f, 0);
	}

	if (!CVarBloomDirt.GetValueOnGameThread())
	{
		FPostProcessSettings OverrideSettings;
		OverrideSettings.bOverride_BloomDirtMaskIntensity = true;
		OverrideSettings.BloomDirtMaskIntensity = 0.0f;
		PostProcessBlendCache.Add(OverrideSettings);
		PostProcessBlendCacheWeights.Add(1.0f);
	}
}

void AUTPlayerCameraManager::ProcessViewRotation(float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot)
{
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PCOwner);
	AUTPlayerState* PS = UTPC ? UTPC->UTPlayerState : NULL;
	if (UTPC && PS && (PS->bOnlySpectator || PS->bOutOfLives) && !UTPC->bSpectatorMouseChangesView && (GetViewTarget() != PCOwner->GetSpectatorPawn()))
	{
		LimitViewPitch(OutViewRotation, ViewPitchMin, ViewPitchMax);
		return;
	}
	Super::ProcessViewRotation(DeltaTime, OutViewRotation, OutDeltaRot);
}