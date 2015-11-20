// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "LeapMotionControllerPrivatePCH.h"

#include "Leap_NoPI.h"

#include "LeapMotionDevice.h"
#include "LeapMotionTypes.h"


//---------------------------------------------------
// Leap Motion Device Implementation
//---------------------------------------------------

FLeapMotionDevice::FLeapMotionDevice()
{
	ReferenceFrameCounter = 0;
}

FLeapMotionDevice::~FLeapMotionDevice()
{
}

bool FLeapMotionDevice::StartupDevice()
{
	// Never fail for now, but can be hooked up to check device config, etc.
	return true;
}

void FLeapMotionDevice::ShutdownDevice()
{
}

bool FLeapMotionDevice::IsConnected() const
{
	return LeapController.isConnected();
}

bool FLeapMotionDevice::GetAllHandsIds(TArray<int32>& OutAllHandIds) const
{
	OutAllHandIds.Empty();
	if (IsConnected())
	{
		for (Leap::Hand hand : Frame().hands())
		{
			OutAllHandIds.Add(hand.id());
		}
		return true;
	}
	return false;
}


bool FLeapMotionDevice::GetOldestLeftOrRightHandId(ELeapSide LeapSide, int32& OutHandId) const
{
	OutHandId = -1;
	float TimeVisible = 0.0f;
	if (IsConnected())
	{
		for (Leap::Hand hand : Frame().hands())
		{
			if ((hand.isLeft() ^ (LeapSide == ELeapSide::Right)) && (OutHandId == -1 || TimeVisible < hand.timeVisible()))
			{
				OutHandId = hand.id();
				TimeVisible = hand.timeVisible();
			}
		}
		return true;
	}
	return false;
}


bool FLeapMotionDevice::GetIsHandLeft(int32 HandId, bool& OutIsLeft) const
{
	OutIsLeft = false;
	if (IsConnected())
	{
		OutIsLeft = Frame().hand(HandId).isLeft();
		return true;
	}
	return false;
}

bool FLeapMotionDevice::GetBonePostionAndOrientation(int32 HandId, ELeapBone LeapBone, FVector& OutPosition, FRotator& OutOrientation) const
{
	OutPosition = FVector::ZeroVector;
	OutOrientation = FRotator::ZeroRotator;
	bool BoneFound = false;

	if (IsConnected())
	{
		Leap::Hand TargetHand = Frame().hand(HandId);

		if (TargetHand.isValid())
		{
			Leap::Matrix LeapBasis;

			switch (LeapBone) 
			{
			case ELeapBone::Forearm:
				OutPosition = LEAPVECTOR_TO_FVECTOR(TargetHand.arm().wristPosition()); // Returns (wrist) position at the front of the arm 
				LeapBasis = TargetHand.arm().basis();
				BoneFound = true;
				break;
			case ELeapBone::Palm:
				OutPosition = LEAPVECTOR_TO_FVECTOR(TargetHand.palmPosition());
				LeapBasis = TargetHand.basis();
				BoneFound = true;
				break;
			default:
				// Any other finger bone
				{
				int FingerIdx = ((int)LeapBone - (int)ELeapBone::ThumbBase) / 3;
				int BoneIdx = ((int)LeapBone - (int)ELeapBone::ThumbBase) % 3 + 1; // we ignore matacarpal (in-palm) bones here
					if (FingerIdx < 5 && BoneIdx < 4)
					{
						Leap::Bone BoneRef = TargetHand.fingers()[FingerIdx].bone((Leap::Bone::Type)BoneIdx);
						OutPosition = LEAPVECTOR_TO_FVECTOR(BoneRef.nextJoint()); // Returns position at the front of the finger bone
						LeapBasis = BoneRef.basis();
						BoneFound = true;
					}
				}
				break;
			}

			if (BoneFound)
			{
				FVector Basis[3];
				Basis[0] = LEAPVECTOR_TO_FVECTOR(LeapBasis.xBasis); // points either left or right, for left/right hand
				Basis[1] = LEAPVECTOR_TO_FVECTOR(LeapBasis.yBasis); // points up
				Basis[2] = LEAPVECTOR_TO_FVECTOR(LeapBasis.zBasis); // points forward
				OutOrientation = FRotationMatrix::MakeFromXZ(-Basis[1], -Basis[2]).Rotator();
			}
		}
	}
	return BoneFound;
}

bool FLeapMotionDevice::GetBoneWidthAndLength(int32 HandId, ELeapBone LeapBone, float& OutWidth, float& OutLength) const
{
	OutWidth = 0.0f;
	OutLength = 0.0f;

	// Remember to scale the lengths from Leap's millimeters to Unreal's centimeters
	const float LeapToUnreal = 0.1f;
	bool BoneFound = false;

	if (IsConnected())
	{
		Leap::Hand TargetHand = Frame().hand(HandId);

		if (TargetHand.isValid())
		{
			FVector Basis[3];

			switch (LeapBone) 
			{
			case ELeapBone::Forearm:
				OutWidth = TargetHand.arm().width();
				OutLength = (TargetHand.arm().wristPosition() - TargetHand.arm().elbowPosition()).magnitude();
				BoneFound = true;
				break;
			case ELeapBone::Palm:
				OutWidth = TargetHand.palmWidth();
				OutLength = TargetHand.palmWidth();
				BoneFound = true;
				break;
			default:
				// Any other finger bone
				{
					int FingerIdx = ((int)LeapBone - (int)ELeapBone::ThumbBase) / 3;
					int BoneIdx = ((int)LeapBone - (int)ELeapBone::ThumbBase) % 3 + 1; // we ignore matacarpal (in-palm) bones here
					if (FingerIdx < 5 && BoneIdx < 4)
					{
						Leap::Bone BoneRef = TargetHand.fingers()[FingerIdx].bone((Leap::Bone::Type)BoneIdx);
						OutWidth = BoneRef.width();
						OutLength = (BoneRef.nextJoint() - BoneRef.prevJoint()).magnitude();
						BoneFound = true;
					}
				}
				break;
			}

			OutWidth *= LeapToUnreal;
			OutLength *= LeapToUnreal;
		}
	}
	return BoneFound;
}

bool FLeapMotionDevice::GetGrabStrength(int32 HandId, float& OutGrabStrength) const
{ 
	if (IsConnected())
	{
		OutGrabStrength = Frame().hand(HandId).grabStrength();
		return true;
	}
	return false;
}

bool FLeapMotionDevice::GetPinchStrength(int32 HandId, float& OutPinchStrength) const
{
	if (IsConnected())
	{
		OutPinchStrength = Frame().hand(HandId).pinchStrength();
		return true;
	}
	return false;
}

bool FLeapMotionDevice::SetHmdPolicy(bool UseHmdPolicy)
{
	if (IsConnected())
	{ 
		Leap::Controller::PolicyFlag PolicyFlags = LeapController.policyFlags();
		if (UseHmdPolicy)
		{
			PolicyFlags = (Leap::Controller::PolicyFlag)(PolicyFlags | Leap::Controller::PolicyFlag::POLICY_OPTIMIZE_HMD);
		}
		else
		{
			PolicyFlags = (Leap::Controller::PolicyFlag)(PolicyFlags & ~Leap::Controller::PolicyFlag::POLICY_OPTIMIZE_HMD);
		}
		LeapController.setPolicyFlags(PolicyFlags);
		return true;
	}
	return false;
}

bool FLeapMotionDevice::SetImagePolicy(bool UseImagePolicy)
{
	if (IsConnected())
	{
		Leap::Controller::PolicyFlag PolicyFlags = LeapController.policyFlags();
		if (UseImagePolicy)
		{
			PolicyFlags = (Leap::Controller::PolicyFlag)(PolicyFlags | Leap::Controller::PolicyFlag::POLICY_IMAGES);
		}
		else
		{
			PolicyFlags = (Leap::Controller::PolicyFlag)(PolicyFlags & ~Leap::Controller::PolicyFlag::POLICY_IMAGES);
		}
		LeapController.setPolicyFlags(PolicyFlags);
		return true;
	}
	return false;
}

const Leap::Frame FLeapMotionDevice::Frame() const
{
	return ReferenceLeapFrame;
}

void FLeapMotionDevice::SetReferenceFrameOncePerTick()
{
	if (ReferenceFrameCounter != GFrameCounter)
	{
		ReferenceFrameCounter = GFrameCounter;
		ReferenceLeapFrame = LeapController.frame();
	}
}
