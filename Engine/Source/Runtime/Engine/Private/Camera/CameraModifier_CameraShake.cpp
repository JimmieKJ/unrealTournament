// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Camera/CameraAnimInst.h"
#include "Camera/CameraModifier_CameraShake.h"

//////////////////////////////////////////////////////////////////////////
// UCameraModifier_CameraShake

UCameraModifier_CameraShake::UCameraModifier_CameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SplitScreenShakeScale = 0.5f;
}

float UCameraModifier_CameraShake::GetShakeScale(FCameraShakeInstance const& ShakeInst) const
{
	return ShakeInst.Scale * Alpha;
}

static inline float UpdateFOscillator(FFOscillator const& Osc, float& CurrentOffset, float DeltaTime)
{
	if (Osc.Amplitude != 0.f)
	{
		CurrentOffset += DeltaTime * Osc.Frequency;
		return Osc.Amplitude * FMath::Sin(CurrentOffset);
	}
	return 0.f;
}

void UCameraModifier_CameraShake::UpdateCameraShake(float DeltaTime, FCameraShakeInstance& Shake, FMinimalViewInfo& InOutPOV)
{
	if (!Shake.SourceShake)
	{
		return;
	}

	// this is the base scale for the whole shake, anim and oscillation alike
	float const BaseShakeScale = GetShakeScale(Shake);
	// do not update if percentage is null
	if (BaseShakeScale <= 0.f)
	{
		return;
	}

	// update anims with any desired scaling
	if (Shake.AnimInst)
	{
		Shake.AnimInst->TransientScaleModifier *= BaseShakeScale;
	}

	// update oscillation times
 	if (Shake.OscillatorTimeRemaining > 0.f)
 	{
 		Shake.OscillatorTimeRemaining -= DeltaTime;
 		Shake.OscillatorTimeRemaining = FMath::Max(0.f, Shake.OscillatorTimeRemaining);
 	}
	if (Shake.bBlendingIn)
	{
		Shake.CurrentBlendInTime += DeltaTime;
	}
	if (Shake.bBlendingOut)
	{
		Shake.CurrentBlendOutTime += DeltaTime;
	}

	// see if we've crossed any important time thresholds and deal appropriately
	bool bOscillationFinished = false;

	if (Shake.OscillatorTimeRemaining == 0.f)
	{
		// finished!
		bOscillationFinished = true;
	}
	else if (Shake.OscillatorTimeRemaining < 0.0f)
	{
		// indefinite shaking
	}
	else if (Shake.OscillatorTimeRemaining < Shake.SourceShake->OscillationBlendOutTime)
	{
		// start blending out
		Shake.bBlendingOut = true;
		Shake.CurrentBlendOutTime = Shake.SourceShake->OscillationBlendOutTime - Shake.OscillatorTimeRemaining;
	}

	if (Shake.bBlendingIn)
	{
		if (Shake.CurrentBlendInTime > Shake.SourceShake->OscillationBlendInTime)
		{
			// done blending in!
			Shake.bBlendingIn = false;
		}
	}
	if (Shake.bBlendingOut)
	{
		if (Shake.CurrentBlendOutTime > Shake.SourceShake->OscillationBlendOutTime)
		{
			// done!!
			Shake.CurrentBlendOutTime = Shake.SourceShake->OscillationBlendOutTime;
			bOscillationFinished = true;
		}
	}

	// calculate blend weight. calculating separately and taking the minimum handles overlapping blends nicely.
	float const BlendInWeight = (Shake.bBlendingIn) ? (Shake.CurrentBlendInTime / Shake.SourceShake->OscillationBlendInTime) : 1.f;
	float const BlendOutWeight = (Shake.bBlendingOut) ? (1.f - Shake.CurrentBlendOutTime / Shake.SourceShake->OscillationBlendOutTime) : 1.f;
	float const CurrentBlendWeight = FMath::Min(BlendInWeight, BlendOutWeight);


	// Do not update oscillation further if finished
	if (bOscillationFinished)
	{
		return;
	}

	// this is the oscillation scale, which includes oscillation fading
	float const OscillationScale = GetShakeScale(Shake) * CurrentBlendWeight;

	if (OscillationScale > 0.f)
	{
		// View location offset, compute sin wave value for each component
		FVector	LocOffset = FVector(0);
		LocOffset.X = UpdateFOscillator(Shake.SourceShake->LocOscillation.X, Shake.LocSinOffset.X, DeltaTime);
		LocOffset.Y = UpdateFOscillator(Shake.SourceShake->LocOscillation.Y, Shake.LocSinOffset.Y, DeltaTime);
		LocOffset.Z = UpdateFOscillator(Shake.SourceShake->LocOscillation.Z, Shake.LocSinOffset.Z, DeltaTime);
		LocOffset *= OscillationScale;

		// View rotation offset, compute sin wave value for each component
		FRotator RotOffset;
		RotOffset.Pitch = UpdateFOscillator(Shake.SourceShake->RotOscillation.Pitch, Shake.RotSinOffset.X, DeltaTime) * OscillationScale;
		RotOffset.Yaw = UpdateFOscillator(Shake.SourceShake->RotOscillation.Yaw, Shake.RotSinOffset.Y, DeltaTime) * OscillationScale;
		RotOffset.Roll = UpdateFOscillator(Shake.SourceShake->RotOscillation.Roll, Shake.RotSinOffset.Z, DeltaTime) * OscillationScale;

		if (Shake.PlaySpace == ECameraAnimPlaySpace::CameraLocal)
		{
			// the else case will handle this as well, but this is the faster, cleaner, most common code path

			// apply loc offset relative to camera orientation
			FRotationMatrix CamRotMatrix(InOutPOV.Rotation);
			InOutPOV.Location += CamRotMatrix.TransformVector(LocOffset);

			// apply rot offset relative to camera orientation
			FRotationMatrix const AnimRotMat( RotOffset );
			InOutPOV.Rotation = (AnimRotMat * FRotationMatrix(InOutPOV.Rotation)).Rotator();
		}
		else
		{
			// find desired space
			FMatrix const PlaySpaceToWorld = (Shake.PlaySpace == ECameraAnimPlaySpace::UserDefined) ? Shake.UserPlaySpaceMatrix : FMatrix::Identity;

			// apply loc offset relative to desired space
			InOutPOV.Location += PlaySpaceToWorld.TransformVector( LocOffset );

			// apply rot offset relative to desired space

			// find transform from camera to the "play space"
			FRotationMatrix const CamToWorld(InOutPOV.Rotation);
			FMatrix const CameraToPlaySpace = CamToWorld * PlaySpaceToWorld.Inverse();			// CameraToWorld * WorldToPlaySpace

			// find transform from anim (applied in playspace) back to camera
			FRotationMatrix const AnimToPlaySpace(RotOffset);
			FMatrix const AnimToCamera = AnimToPlaySpace * CameraToPlaySpace.Inverse();			// AnimToPlaySpace * PlaySpaceToCamera

			// RCS = rotated camera space, meaning camera space after it's been animated
			// this is what we're looking for, the diff between rotated cam space and regular cam space.
			// apply the transform back to camera space from the post-animated transform to get the RCS
			FMatrix const RCSToCamera = CameraToPlaySpace * AnimToCamera;

			// now apply to real camera
			InOutPOV.Rotation = (RCSToCamera * CamToWorld).Rotator();
		}

		// Compute FOV change
		InOutPOV.FOV += OscillationScale * UpdateFOscillator(Shake.SourceShake->FOVOscillation, Shake.FOVSinOffset, DeltaTime);
	}
}

bool UCameraModifier_CameraShake::ModifyCamera(class APlayerCameraManager* Camera, float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	// Update the alpha
	UpdateAlpha(Camera, DeltaTime);

	// Call super where modifier may be disabled
	Super::ModifyCamera(Camera, DeltaTime, InOutPOV);

	// If no alpha, exit early
	if( Alpha <= 0.f )
	{
		return false;
	}

	// Update Screen Shakes array
	if( ActiveShakes.Num() > 0 )
	{
		for(int32 i=0; i<ActiveShakes.Num(); i++)
		{
			UpdateCameraShake(DeltaTime, ActiveShakes[i], InOutPOV);
		}

		// Delete any obsolete shakes
		for(int32 i=ActiveShakes.Num()-1; i>=0; i--)
		{
			FCameraShakeInstance& ShakeInst = ActiveShakes[i];
			if( !ShakeInst.SourceShake
				|| ( (ShakeInst.OscillatorTimeRemaining == 0.f) && 
					 ((ShakeInst.AnimInst == NULL) || (ShakeInst.AnimInst->bFinished == true)) ) )
			{
				ActiveShakes.RemoveAt(i,1);
			}
		}
	}

	// If ModifyCamera returns true, exit loop
	// Allows high priority things to dictate if they are
	// the last modifier to be applied
	// Returning true causes to stop adding another modifier! 
	// Returning false is the right behavior since this is not high priority modifier.
	return false;
}

float UCameraModifier_CameraShake::InitializeOffset( const FFOscillator& Param ) const
{
	switch( Param.InitialOffset )
	{
		case EOO_OffsetRandom	: return FMath::FRand() * 2.f * PI;	break;
		case EOO_OffsetZero		: return 0.f;					break;
		default					: return 0.f;
	}
}

void UCameraModifier_CameraShake::ReinitShake(FCameraShakeInstance& ShakeInst, float NewScale)
{
	if (GEngine->IsSplitScreen(CameraOwner->GetWorld()))
	{
		NewScale *= SplitScreenShakeScale;
	}
	ShakeInst.Scale = NewScale;

	UCameraShake const* const SourceShake = ShakeInst.SourceShake;

	if (SourceShake->OscillationDuration != 0.f)
	{
		ShakeInst.OscillatorTimeRemaining = SourceShake->OscillationDuration;

		if (ShakeInst.bBlendingOut)
		{
			ShakeInst.bBlendingOut = false;
			ShakeInst.CurrentBlendOutTime = 0.f;

			// stop any blendout and reverse it to a blendin
			ShakeInst.bBlendingIn = true;
			ShakeInst.CurrentBlendInTime = ShakeInst.SourceShake->OscillationBlendInTime * (1.f - ShakeInst.CurrentBlendOutTime / ShakeInst.SourceShake->OscillationBlendOutTime);
		}
	}

	if (SourceShake->Anim != NULL)
	{
		bool bLoop = false, bRandomStart = false;
		float Duration = 0.f;
		if (SourceShake->bRandomAnimSegment)
		{
			bLoop = true;
			bRandomStart = true;
			Duration = SourceShake->RandomAnimSegmentDuration;
		}

		float AnimScale = NewScale * SourceShake->AnimScale;

		if (ShakeInst.AnimInst != nullptr)
		{
			ShakeInst.AnimInst->Update(SourceShake->AnimPlayRate, AnimScale, SourceShake->AnimBlendInTime, SourceShake->AnimBlendOutTime, Duration);
		}
		else
		{
			ShakeInst.AnimInst = CameraOwner->PlayCameraAnim(SourceShake->Anim, SourceShake->AnimPlayRate, AnimScale, SourceShake->AnimBlendInTime, SourceShake->AnimBlendOutTime, bLoop, bRandomStart, Duration);
		}
	}
}

FCameraShakeInstance UCameraModifier_CameraShake::InitializeShake(TSubclassOf<class UCameraShake> NewShake, float Scale, ECameraAnimPlaySpace::Type PlaySpace, FRotator UserPlaySpaceRot)
{
	FCameraShakeInstance Inst;
	Inst.SourceShakeName = NewShake->GetFName();

	// Create a camera shake object of class NewShake
	UCameraShake const* const SourceShake = CastChecked<UCameraShake>(NewShake->GetDefaultObject());
	Inst.SourceShake = SourceShake;

	Inst.Scale = Scale;
	if (GEngine->IsSplitScreen(CameraOwner->GetWorld()))
	{
		Scale *= SplitScreenShakeScale;
	}

	// init oscillations
	if ( SourceShake->OscillationDuration != 0.f )
	{
		Inst.RotSinOffset.X		= InitializeOffset( SourceShake->RotOscillation.Pitch );
		Inst.RotSinOffset.Y		= InitializeOffset( SourceShake->RotOscillation.Yaw );
		Inst.RotSinOffset.Z		= InitializeOffset( SourceShake->RotOscillation.Roll );

		Inst.LocSinOffset.X		= InitializeOffset( SourceShake->LocOscillation.X );
		Inst.LocSinOffset.Y		= InitializeOffset( SourceShake->LocOscillation.Y );
		Inst.LocSinOffset.Z		= InitializeOffset( SourceShake->LocOscillation.Z );

		Inst.FOVSinOffset		= InitializeOffset( SourceShake->FOVOscillation );

		Inst.OscillatorTimeRemaining = SourceShake->OscillationDuration;

		if (SourceShake->OscillationBlendInTime > 0.f)
		{
			Inst.bBlendingIn = true;
			Inst.CurrentBlendInTime = 0.f;
		}
	}

	// init anims
	if (SourceShake->Anim != NULL)
	{
		bool bLoop = false;
		bool bRandomStart = false;
		float Duration = 0.f;
		if (SourceShake->bRandomAnimSegment)
		{
			bLoop = true;
			bRandomStart = true;
			Duration = SourceShake->RandomAnimSegmentDuration;
		}

		float AnimScale = Scale * SourceShake->AnimScale;
		
		if (AnimScale > 0.f)
		{
			Inst.AnimInst = CameraOwner->PlayCameraAnim(SourceShake->Anim, SourceShake->AnimPlayRate, AnimScale, SourceShake->AnimBlendInTime, SourceShake->AnimBlendOutTime, bLoop, bRandomStart, Duration, PlaySpace, UserPlaySpaceRot);
		}
	}

	Inst.PlaySpace = PlaySpace;
	if (Inst.PlaySpace == ECameraAnimPlaySpace::UserDefined)
	{
		Inst.UserPlaySpaceMatrix = FRotationMatrix(UserPlaySpaceRot);
	}

	return Inst;
}


void UCameraModifier_CameraShake::AddCameraShake( TSubclassOf<class UCameraShake> NewShake, float Scale, ECameraAnimPlaySpace::Type PlaySpace, FRotator UserPlaySpaceRot )
{
	if (NewShake != NULL)
	{
		UCameraShake const* const ShakeCDO = GetDefault<UCameraShake>(NewShake);

		if (ShakeCDO && ShakeCDO->bSingleInstance)
		{
			for (FCameraShakeInstance& ShakeInst : ActiveShakes)
			{
				if (ShakeInst.SourceShakeName == NewShake->GetFName())
				{
					ReinitShake(ShakeInst, Scale);
					return;
				}
			}
		}

		int32 NumShakes = ActiveShakes.Num();

		// Initialize new shake and add it to the list of active shakes
		ActiveShakes.Add(InitializeShake(NewShake, Scale, PlaySpace, UserPlaySpaceRot));
	}
}

void UCameraModifier_CameraShake::RemoveCameraShake(TSubclassOf<class UCameraShake> Shake)
{
	for (int32 i = 0; i < ActiveShakes.Num(); ++i)
	{
		if (ActiveShakes[i].SourceShakeName == Shake->GetFName())
		{
			UCameraAnimInst* AnimInst = ActiveShakes[i].AnimInst;
			if ( (AnimInst != NULL) && !AnimInst->bFinished )
			{
				CameraOwner->StopCameraAnimInst(AnimInst, true);
			}

			ActiveShakes.RemoveAt(i,1);
			return;
		}
	}
}

void UCameraModifier_CameraShake::RemoveAllCameraShakes()
{
	// clean up any active camera shake anims
	for (int32 Idx=0; Idx<ActiveShakes.Num(); ++Idx)
	{
		UCameraAnimInst* AnimInst = ActiveShakes[Idx].AnimInst;
		if ( (AnimInst != NULL) && !AnimInst->bFinished )
		{
			CameraOwner->StopCameraAnimInst(AnimInst, true);
		}
	}

	// clear ActiveShakes array
	ActiveShakes.Empty();
}
