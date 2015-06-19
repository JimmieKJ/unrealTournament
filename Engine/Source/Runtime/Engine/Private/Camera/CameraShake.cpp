// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraAnimInst.h"

//////////////////////////////////////////////////////////////////////////
// FFOscillator

// static
float FFOscillator::UpdateOffset(FFOscillator const& Osc, float& CurrentOffset, float DeltaTime)
{
	if (Osc.Amplitude != 0.f)
	{
		CurrentOffset += DeltaTime * Osc.Frequency;
		return Osc.Amplitude * FMath::Sin(CurrentOffset);
	}
	return 0.f;
}

// static
float FFOscillator::GetInitialOffset(FFOscillator const& Osc)
{
	return (Osc.InitialOffset == EOO_OffsetRandom)
		? FMath::FRand() * (2.f * PI)
		: 0.f;
}

//////////////////////////////////////////////////////////////////////////
// UCameraShake

UCameraShake::UCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AnimPlayRate = 1.0f;
	AnimScale = 1.0f;
	AnimBlendInTime = 0.2f;
	AnimBlendOutTime = 0.2f;
	OscillationBlendInTime = 0.1f;
	OscillationBlendOutTime = 0.2f;
}

void UCameraShake::StopShake()
{
	// stop cam anim if playing
	if ((AnimInst != nullptr) && !AnimInst->bFinished)
	{
		CameraOwner->StopCameraAnimInst(AnimInst, true);
		AnimInst = nullptr;
	}

	ReceiveStopShake();
}

void UCameraShake::PlayShake(APlayerCameraManager* Camera, float Scale, ECameraAnimPlaySpace::Type InPlaySpace, FRotator UserPlaySpaceRot)
{
	ShakeScale = Scale;
	CameraOwner = Camera;

	// init oscillations
	if (OscillationDuration != 0.f)
	{

		if (OscillatorTimeRemaining > 0.f)
		{
			// this shake was already playing
			OscillatorTimeRemaining = OscillationDuration;

			if (bBlendingOut)
			{
				bBlendingOut = false;
				CurrentBlendOutTime = 0.f;

				// stop any blendout and reverse it to a blendin
				if (OscillationBlendInTime > 0.f)
				{
					bBlendingIn = true;
					CurrentBlendInTime = OscillationBlendInTime * (1.f - CurrentBlendOutTime / OscillationBlendOutTime);
				}
				else
				{
					bBlendingIn = false;
					CurrentBlendInTime = 0.f;
				}
			}
		}
		else
		{
			RotSinOffset.X = FFOscillator::GetInitialOffset(RotOscillation.Pitch);
			RotSinOffset.Y = FFOscillator::GetInitialOffset(RotOscillation.Yaw);
			RotSinOffset.Z = FFOscillator::GetInitialOffset(RotOscillation.Roll);

			LocSinOffset.X = FFOscillator::GetInitialOffset(LocOscillation.X);
			LocSinOffset.Y = FFOscillator::GetInitialOffset(LocOscillation.Y);
			LocSinOffset.Z = FFOscillator::GetInitialOffset(LocOscillation.Z);

			FOVSinOffset = FFOscillator::GetInitialOffset(FOVOscillation);

			OscillatorTimeRemaining = OscillationDuration;

			if (OscillationBlendInTime > 0.f)
			{
				bBlendingIn = true;
				CurrentBlendInTime = 0.f;
			}
		}
	}

	// init cameraanim shakes
	if (Anim != nullptr)
	{
		if (AnimInst)
		{
			float const Duration = bRandomAnimSegment ? RandomAnimSegmentDuration : 0.f;
			float const FinalAnimScale = Scale * AnimScale;
			AnimInst->Update(AnimPlayRate, FinalAnimScale, AnimBlendInTime, AnimBlendOutTime, Duration);
		}
		else
		{
			bool bLoop = false;
			bool bRandomStart = false;
			float Duration = 0.f;
			if (bRandomAnimSegment)
			{
				bLoop = true;
				bRandomStart = true;
				Duration = RandomAnimSegmentDuration;
			}

			float const FinalAnimScale = Scale * AnimScale;
			if (FinalAnimScale > 0.f)
			{
				AnimInst = CameraOwner->PlayCameraAnim(Anim, AnimPlayRate, FinalAnimScale, AnimBlendInTime, AnimBlendOutTime, bLoop, bRandomStart, Duration, InPlaySpace, UserPlaySpaceRot);
			}
		}
	}

	if (InPlaySpace == ECameraAnimPlaySpace::UserDefined)
	{
		UserPlaySpaceMatrix = FRotationMatrix(UserPlaySpaceRot);
	}

	ReceivePlayShake(Scale);
}

void UCameraShake::UpdateAndApplyCameraShake(float DeltaTime, float Alpha, FMinimalViewInfo& InOutPOV)
{
	// this is the base scale for the whole shake, anim and oscillation alike
	float const BaseShakeScale = FMath::Max<float>(Alpha * ShakeScale, 0.0f);

	// update anims with any desired scaling
	if (AnimInst)
	{
		AnimInst->TransientScaleModifier *= BaseShakeScale;
	}

	// update oscillation times
	if (OscillatorTimeRemaining > 0.f)
	{
		OscillatorTimeRemaining -= DeltaTime;
		OscillatorTimeRemaining = FMath::Max(0.f, OscillatorTimeRemaining);
	}
	if (bBlendingIn)
	{
		CurrentBlendInTime += DeltaTime;
	}
	if (bBlendingOut)
	{
		CurrentBlendOutTime += DeltaTime;
	}

	// see if we've crossed any important time thresholds and deal appropriately
	bool bOscillationFinished = false;

	if (OscillatorTimeRemaining == 0.f)
	{
		// finished!
		bOscillationFinished = true;
	}
	else if (OscillatorTimeRemaining < 0.0f)
	{
		// indefinite shaking
	}
	else if (OscillatorTimeRemaining < OscillationBlendOutTime)
	{
		// start blending out
		bBlendingOut = true;
		CurrentBlendOutTime = OscillationBlendOutTime - OscillatorTimeRemaining;
	}

	if (bBlendingIn)
	{
		if (CurrentBlendInTime > OscillationBlendInTime)
		{
			// done blending in!
			bBlendingIn = false;
		}
	}
	if (bBlendingOut)
	{
		if (CurrentBlendOutTime > OscillationBlendOutTime)
		{
			// done!!
			CurrentBlendOutTime = OscillationBlendOutTime;
			bOscillationFinished = true;
		}
	}

	// Do not update oscillation further if finished
	if (bOscillationFinished == false)
	{
		// calculate blend weight. calculating separately and taking the minimum handles overlapping blends nicely.
		float const BlendInWeight = (bBlendingIn) ? (CurrentBlendInTime / OscillationBlendInTime) : 1.f;
		float const BlendOutWeight = (bBlendingOut) ? (1.f - CurrentBlendOutTime / OscillationBlendOutTime) : 1.f;
		float const CurrentBlendWeight = FMath::Min(BlendInWeight, BlendOutWeight);

		// this is the oscillation scale, which includes oscillation fading
		float const OscillationScale = BaseShakeScale * CurrentBlendWeight;

		if (OscillationScale > 0.f)
		{
			// View location offset, compute sin wave value for each component
			FVector	LocOffset = FVector(0);
			LocOffset.X = FFOscillator::UpdateOffset(LocOscillation.X, LocSinOffset.X, DeltaTime);
			LocOffset.Y = FFOscillator::UpdateOffset(LocOscillation.Y, LocSinOffset.Y, DeltaTime);
			LocOffset.Z = FFOscillator::UpdateOffset(LocOscillation.Z, LocSinOffset.Z, DeltaTime);
			LocOffset *= OscillationScale;

			// View rotation offset, compute sin wave value for each component
			FRotator RotOffset;
			RotOffset.Pitch = FFOscillator::UpdateOffset(RotOscillation.Pitch, RotSinOffset.X, DeltaTime) * OscillationScale;
			RotOffset.Yaw = FFOscillator::UpdateOffset(RotOscillation.Yaw, RotSinOffset.Y, DeltaTime) * OscillationScale;
			RotOffset.Roll = FFOscillator::UpdateOffset(RotOscillation.Roll, RotSinOffset.Z, DeltaTime) * OscillationScale;

			if (PlaySpace == ECameraAnimPlaySpace::CameraLocal)
			{
				// the else case will handle this as well, but this is the faster, cleaner, most common code path

				// apply loc offset relative to camera orientation
				FRotationMatrix CamRotMatrix(InOutPOV.Rotation);
				InOutPOV.Location += CamRotMatrix.TransformVector(LocOffset);

				// apply rot offset relative to camera orientation
				FRotationMatrix const AnimRotMat(RotOffset);
				InOutPOV.Rotation = (AnimRotMat * FRotationMatrix(InOutPOV.Rotation)).Rotator();
			}
			else
			{
				// find desired space
				FMatrix const PlaySpaceToWorld = (PlaySpace == ECameraAnimPlaySpace::UserDefined) ? UserPlaySpaceMatrix : FMatrix::Identity;

				// apply loc offset relative to desired space
				InOutPOV.Location += PlaySpaceToWorld.TransformVector(LocOffset);

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
			InOutPOV.FOV += OscillationScale * FFOscillator::UpdateOffset(FOVOscillation, FOVSinOffset, DeltaTime);
		}
	}

	BlueprintUpdateCameraShake(DeltaTime, Alpha, InOutPOV, InOutPOV);
}

bool UCameraShake::IsFinished() const
{
	return ((OscillatorTimeRemaining <= 0.f) &&						// oscillator is finished
		((AnimInst == nullptr) || AnimInst->bFinished) &&		// anim is finished
		ReceiveIsFinished()										// BP thinks it's finished
		);
}

bool UCameraShake::ReceiveIsFinished_Implementation() const
{
	return true;
}


