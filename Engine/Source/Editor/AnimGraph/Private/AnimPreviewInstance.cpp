// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "AnimGraphPrivatePCH.h"
#include "AnimPreviewInstance.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif 

#define LOCTEXT_NAMESPACE "AnimPreviewInstance"

UAnimPreviewInstance::UAnimPreviewInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SkeletalControlAlpha(1.0f)
#if WITH_EDITORONLY_DATA
	, bForceRetargetBasePose(false)
#endif
	, bSetKey(false)
{
	RootMotionMode = ERootMotionMode::RootMotionFromEverything;
	bEnableControllers = true;
}

void UAnimPreviewInstance::NativeInitializeAnimation()
{
	// Cache our play state from the previous animation otherwise set to play
	bool bCachedIsPlaying = (CurrentAsset != NULL) ? bPlaying : true;
	bSetKey = false;
	Super::NativeInitializeAnimation();

	SetPlaying(bCachedIsPlaying);

	RefreshCurveBoneControllers();
}

void UAnimPreviewInstance::ResetModifiedBone(bool bCurveController/*=false*/)
{
	TArray<FAnimNode_ModifyBone>& Controllers = (bCurveController)?CurveBoneControllers : BoneControllers;
	Controllers.Empty();
}

FAnimNode_ModifyBone* UAnimPreviewInstance::FindModifiedBone(const FName& InBoneName, bool bCurveController/*=false*/)
{
	TArray<FAnimNode_ModifyBone>& Controllers = (bCurveController)?CurveBoneControllers : BoneControllers;

	return Controllers.FindByPredicate(
		[InBoneName](const FAnimNode_ModifyBone& InController) -> bool
	{
		return InController.BoneToModify.BoneName == InBoneName;
	}
	);
}

FAnimNode_ModifyBone& UAnimPreviewInstance::ModifyBone(const FName& InBoneName, bool bCurveController/*=false*/)
{
	FAnimNode_ModifyBone* SingleBoneController = FindModifiedBone(InBoneName, bCurveController);
	TArray<FAnimNode_ModifyBone>& Controllers = (bCurveController)?CurveBoneControllers : BoneControllers;

	if(SingleBoneController == nullptr)
	{
		int32 NewIndex = Controllers.Add(FAnimNode_ModifyBone());
		SingleBoneController = &Controllers[NewIndex];
	}

	SingleBoneController->BoneToModify.BoneName = InBoneName;

	if (bCurveController)
	{
		SingleBoneController->TranslationMode = BMM_Additive;
		SingleBoneController->TranslationSpace = BCS_BoneSpace;

		SingleBoneController->RotationMode = BMM_Additive;
		SingleBoneController->RotationSpace = BCS_BoneSpace;

		SingleBoneController->ScaleMode = BMM_Additive;
		SingleBoneController->ScaleSpace = BCS_BoneSpace;
	}
	else
	{
		SingleBoneController->TranslationMode = BMM_Replace;
		SingleBoneController->TranslationSpace = BCS_BoneSpace;

		SingleBoneController->RotationMode = BMM_Replace;
		SingleBoneController->RotationSpace = BCS_BoneSpace;

		SingleBoneController->ScaleMode = BMM_Replace;
		SingleBoneController->ScaleSpace = BCS_BoneSpace;
	}

	return *SingleBoneController;
}

void UAnimPreviewInstance::RemoveBoneModification(const FName& InBoneName, bool bCurveController/*=false*/)
{
	TArray<FAnimNode_ModifyBone>& Controllers = (bCurveController)?CurveBoneControllers : BoneControllers;
	Controllers.RemoveAll(
		[InBoneName](const FAnimNode_ModifyBone& InController)
	{
		return InController.BoneToModify.BoneName == InBoneName;
	}
	);
}

void UAnimPreviewInstance::NativeUpdateAnimation(float DeltaTimeX)
{
#if WITH_EDITORONLY_DATA
	if(bForceRetargetBasePose)
	{
		// nothing to be done here
		return;
	}
#endif // #if WITH_EDITORONLY_DATA

	Super::NativeUpdateAnimation(DeltaTimeX);
}

bool UAnimPreviewInstance::NativeEvaluateAnimation(FPoseContext& Output)
{
#if WITH_EDITORONLY_DATA
	if(bForceRetargetBasePose)
	{
		USkeletalMeshComponent* MeshComponent = GetSkelMeshComponent();
		if(MeshComponent && MeshComponent->SkeletalMesh)
		{
			FAnimationRuntime::FillWithRetargetBaseRefPose(Output.Pose.Bones, GetSkelMeshComponent()->SkeletalMesh, RequiredBones);
		}
		else
		{
			// ideally we'll return just ref pose, but not sure if this will work with LODs
			FAnimationRuntime::FillWithRefPose(Output.Pose.Bones, RequiredBones);
		}
	}
	else
#endif // #if WITH_EDITORONLY_DATA
	{
		Super::NativeEvaluateAnimation(Output);
	}

	if (bEnableControllers)
	{
		UDebugSkelMeshComponent* Component = Cast<UDebugSkelMeshComponent>(GetSkelMeshComponent());

		if(Component && CurrentSkeleton)
		{
			// update curve controllers
			UpdateCurveController();

			// create bone controllers from 
			if(BoneControllers.Num() > 0 || CurveBoneControllers.Num() > 0)
			{
				TArray<FTransform> PreController, PostController;
				// if set key is true, we should save pre controller local space transform 
				// so that we can calculate the delta correctly
				if(bSetKey)
				{
					PreController = Output.Pose.Bones;
				}

				FA2CSPose OutMeshPose;
				OutMeshPose.AllocateLocalPoses(RequiredBones, Output.Pose);

				// apply curve data first
				ApplyBoneControllers(Component, CurveBoneControllers, OutMeshPose);

				// and now apply bone controllers data
				// it is possible they can be overlapping, but then bone controllers will overwrite
				ApplyBoneControllers(Component, BoneControllers, OutMeshPose);

				// convert back to local @todo check this
				OutMeshPose.ConvertToLocalPoses(Output.Pose);

				if(bSetKey)
				{
					// now we have post controller, and calculate delta now
					PostController = Output.Pose.Bones;
					SetKeyImplementation(PreController, PostController);
				}
			}
			// if any other bone is selected, still go for set key even if nothing changed
			else if(Component->BonesOfInterest.Num() > 0)
			{
				if(bSetKey)
				{
					// in this case, pose is same
					SetKeyImplementation(Output.Pose.Bones, Output.Pose.Bones);
				}
			}
		}

		// we should unset here, just in case somebody clicks the key when it's not valid
		if(bSetKey)
		{
			bSetKey = false;
		}
	}

	return true;
}

void UAnimPreviewInstance::ApplyBoneControllers(USkeletalMeshComponent* Component, TArray<FAnimNode_ModifyBone> &InBoneControllers, FA2CSPose& OutMeshPose)
{
	for(auto& SingleBoneController : InBoneControllers)
	{
		SingleBoneController.BoneToModify.BoneIndex = RequiredBones.GetPoseBoneIndexForBoneName(SingleBoneController.BoneToModify.BoneName);
		if(SingleBoneController.BoneToModify.BoneIndex != INDEX_NONE)
		{
			TArray<FBoneTransform> BoneTransforms;
			SingleBoneController.EvaluateBoneTransforms(Component, RequiredBones, OutMeshPose, BoneTransforms);
			if(BoneTransforms.Num() > 0)
			{
				OutMeshPose.LocalBlendCSBoneTransforms(BoneTransforms, 1.0f);
			}
		}
	}
}

void UAnimPreviewInstance::SetKeyImplementation(const TArray<FTransform>& PreControllerInLocalSpace, const TArray<FTransform>& PostControllerInLocalSpace)
{
#if WITH_EDITOR
	// evaluate the curve data first
	UAnimSequence* CurrentSequence = Cast<UAnimSequence>(CurrentAsset);
	UDebugSkelMeshComponent* Component = Cast<UDebugSkelMeshComponent> (GetSkelMeshComponent());

	if(CurrentSequence && CurrentSkeleton && Component && Component->SkeletalMesh)
	{
		FScopedTransaction ScopedTransaction(LOCTEXT("SetKey", "Set Key"));
		CurrentSequence->Modify(true);
		Modify();

		TArray<FName> BonesToModify;
		// need to get component transform first. Depending on when this gets called, the transform is not up-to-date. 
		// first look at the bonecontrollers, and convert each bone controller to transform curve key
		// and add new curvebonecontrollers with additive data type
		// clear bone controller data
		for(auto& SingleBoneController : BoneControllers)
		{
			// find bone name, and just get transform of the bone in local space
			// and get the additive data
			// find if this already exists, then just add curve data only
			FName BoneName = SingleBoneController.BoneToModify.BoneName;
			// now convert data
			const int32 BoneIndex = Component->GetBoneIndex(BoneName);
			FTransform  LocalTransform = PostControllerInLocalSpace[BoneIndex];

			// now we have LocalTransform and get additive data
			FTransform AdditiveTransform = LocalTransform.GetRelativeTransform(PreControllerInLocalSpace[BoneIndex]);
			AddKeyToSequence(CurrentSequence, CurrentTime, BoneName, AdditiveTransform);

			BonesToModify.Add(BoneName);
		}

		// see if the bone is selected right now and if that is added - if bone is selected, we should add identity key to it. 
		if ( Component->BonesOfInterest.Num() > 0 )
		{
			// if they're selected, we should add to the modifyBone list even if they're not modified, so that they can key that point. 
			// first make sure those are added 
			// if not added, make sure to set the key for them
			for (const auto& BoneIndex : Component->BonesOfInterest)
			{
				FName BoneName = Component->GetBoneName(BoneIndex);
				// if it's not on BonesToModify, add identity here. 
				if (!BonesToModify.Contains(BoneName))
				{
					AddKeyToSequence(CurrentSequence, CurrentTime, BoneName, FTransform::Identity);
				}
			}
		}

		ResetModifiedBone(false);

		OnSetKeyCompleteDelegate.ExecuteIfBound();
	}
#endif
}

void UAnimPreviewInstance::AddKeyToSequence(UAnimSequence* Sequence, float Time, const FName& BoneName, const FTransform& AdditiveTransform) 
{
	Sequence->AddKeyToSequence(Time, BoneName, AdditiveTransform);

	// now add to the controller
	// find if it exists in CurveBoneController
	// make sure you add it there
	ModifyBone(BoneName, true);

	RequiredBones.SetUseSourceData(true);
}

void UAnimPreviewInstance::SetKey(FSimpleDelegate InOnSetKeyCompleteDelegate)
{
#if WITH_EDITOR
	bSetKey = true;
	OnSetKeyCompleteDelegate = InOnSetKeyCompleteDelegate;
#endif
}

void UAnimPreviewInstance::RefreshCurveBoneControllers()
{
	// go through all curves and see if it has Transform Curve
	// if so, find what bone that belong to and create BoneMOdifier for them
	UAnimSequence* CurrentSequence = Cast<UAnimSequence>(CurrentAsset);

	CurveBoneControllers.Empty();

	// do not apply if BakedAnimation is on
	if(CurrentSequence)
	{
		// make sure if this needs source update
		if ( !CurrentSequence->DoesContainTransformCurves() )
		{
			return;
		}

		RequiredBones.SetUseSourceData(true);

		TArray<FTransformCurve>& Curves = CurrentSequence->RawCurveData.TransformCurves;
		FSmartNameMapping* NameMapping = CurrentSkeleton->SmartNames.GetContainer(USkeleton::AnimTrackCurveMappingName);

		for (auto& Curve : Curves)
		{
			// skip if disabled
			if (Curve.GetCurveTypeFlag(ACF_Disabled))
			{
				continue;
			}

			// add bone modifier
			FName CurveName;
			NameMapping->GetName(Curve.CurveUid, CurveName);

			// @TODO: this is going to be issue. If they don't save skeleton with it, we don't have name at all?
 			if (CurveName == NAME_None)
 			{
				FSmartNameMapping::UID NewUID;
 				NameMapping->AddOrFindName(Curve.LastObservedName, NewUID);
				Curve.CurveUid = NewUID;

				CurveName = Curve.LastObservedName;
 			}

			FName BoneName = CurveName;
			if (BoneName != NAME_None && CurrentSkeleton->GetReferenceSkeleton().FindBoneIndex(BoneName) != INDEX_NONE)
			{
				ModifyBone(BoneName, true);
			}
		}
	}
}

void UAnimPreviewInstance::UpdateCurveController()
{
	// evaluate the curve data first
	UAnimSequenceBase* CurrentSequence = Cast<UAnimSequenceBase>(CurrentAsset);

	if (CurrentSequence && CurrentSkeleton)
	{
		TMap<FName, FTransform> ActiveCurves;
		CurrentSequence->RawCurveData.EvaluateTransformCurveData(CurrentSkeleton, ActiveCurves, CurrentTime, 1.f);

		// make sure those curves exists in the bone controller, otherwise problem
		if ( ActiveCurves.Num() > 0 )
		{
			for(auto& SingleBoneController : CurveBoneControllers)
			{
				// make sure the curve exists
				FName CurveName = SingleBoneController.BoneToModify.BoneName;

				// we should add extra key to front and back whenever animation length changes or so. 
				// animation length change requires to bake down animation first 
				// this will make sure all the keys that were embedded at the start/end will automatically be backed to the data
				const FTransform* Value = ActiveCurves.Find(CurveName);
				if (Value)
				{
					// apply this change
					SingleBoneController.Translation = Value->GetTranslation();
					SingleBoneController.Scale = Value->GetScale3D();
					// sasd we're converting twice
					SingleBoneController.Rotation = Value->GetRotation().Rotator();
				}
			}
		}
		else
		{
			// should match
			ensure (CurveBoneControllers.Num() == 0);
			CurveBoneControllers.Empty();
		}
	}
}

/** Set SkeletalControl Alpha**/
void UAnimPreviewInstance::SetSkeletalControlAlpha(float InSkeletalControlAlpha)
{
	SkeletalControlAlpha = FMath::Clamp<float>(InSkeletalControlAlpha, 0.f, 1.f);
}

UAnimSequence* UAnimPreviewInstance::GetAnimSequence()
{
	return Cast<UAnimSequence>(CurrentAsset);
}

void UAnimPreviewInstance::RestartMontage(UAnimMontage* Montage, FName FromSection)
{
	if (Montage == CurrentAsset)
	{
		MontagePreviewType = EMPT_Normal;
		Montage_Play(Montage, PlayRate);
		if (FromSection != NAME_None)
		{
			Montage_JumpToSection(FromSection);
		}
		MontagePreview_SetLoopNormal(bLooping, Montage->GetSectionIndex(FromSection));
	}
}

void UAnimPreviewInstance::SetAnimationAsset(UAnimationAsset* NewAsset, bool bIsLooping, float InPlayRate)
{
	// make sure to turn that off before setting new asset
	RequiredBones.SetUseSourceData(false);

	Super::SetAnimationAsset(NewAsset, bIsLooping, InPlayRate);
	RootMotionMode = Cast<UAnimMontage>(CurrentAsset) != NULL ? ERootMotionMode::RootMotionFromMontagesOnly : ERootMotionMode::RootMotionFromEverything;

	// should re sync up curve bone controllers from new asset
	RefreshCurveBoneControllers();
}

void UAnimPreviewInstance::MontagePreview_SetLooping(bool bIsLooping)
{
	bLooping = bIsLooping;

	if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
	{
		switch (MontagePreviewType)
		{
		case EMPT_AllSections:
			MontagePreview_SetLoopAllSections(bLooping);
			break;
		case EMPT_Normal:
		default:
			MontagePreview_SetLoopNormal(bLooping);
			break;
		}
	}
}

void UAnimPreviewInstance::MontagePreview_SetPlaying(bool bIsPlaying)
{
	bPlaying = bIsPlaying;

	if (FAnimMontageInstance* CurMontageInstance = GetActiveMontageInstance())
	{
		CurMontageInstance->bPlaying = bIsPlaying;
	}
	else if (bPlaying)
	{
		UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset);
		if (Montage)
		{
			switch (MontagePreviewType)
			{
			case EMPT_AllSections:
				MontagePreview_PreviewAllSections();
				break;
			case EMPT_Normal:
			default:
				MontagePreview_PreviewNormal();
				break;
			}
		}
	}
}

void UAnimPreviewInstance::MontagePreview_SetReverse(bool bInReverse)
{
	Super::SetReverse(bInReverse);

	if (FAnimMontageInstance* CurMontageInstance = GetActiveMontageInstance())
	{
		// copy the current playrate
		CurMontageInstance->SetPlayRate(PlayRate);
	}
}

void UAnimPreviewInstance::MontagePreview_Restart()
{
	if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
	{
		switch (MontagePreviewType)
		{
		case EMPT_AllSections:
			MontagePreview_PreviewAllSections();
			break;
		case EMPT_Normal:
		default:
			MontagePreview_PreviewNormal();
			break;
		}
	}
}

void UAnimPreviewInstance::MontagePreview_StepForward()
{
	if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
	{
		bool bWasPlaying = IsPlayingMontage() && (bLooping || bPlaying); // we need to handle non-looped case separately, even if paused during playthrough
		MontagePreview_SetReverse(false);
		if (! bWasPlaying)
		{
			if (! bLooping)
			{
				float StoppedAt = CurrentTime;
				if (! bWasPlaying)
				{
					// play montage but at last known location
					MontagePreview_Restart();
					SetPosition(StoppedAt, false);
				}
				int32 LastPreviewSectionIdx = MontagePreview_FindLastSection(MontagePreviewStartSectionIdx);
				if (FMath::Abs(CurrentTime - (Montage->CompositeSections[LastPreviewSectionIdx].GetTime() + Montage->GetSectionLength(LastPreviewSectionIdx))) <= MontagePreview_CalculateStepLength())
				{
					// we're at the end, jump right to the end
					Montage_JumpToSectionsEnd(Montage->GetSectionName(LastPreviewSectionIdx));
					if (! bWasPlaying)
					{
						MontagePreview_SetPlaying(false);
					}
					return; // can't go further than beginning of this
				}
			}
			else
			{
				MontagePreview_Restart();
			}
		}
		MontagePreview_SetPlaying(true);

		// Advance a single frame, leaving it paused afterwards
		int32 NumFrames = Montage->GetNumberOfFrames();
		// Add DELTA to prefer next frame when we're close to the boundary
		float CurrentFraction = CurrentTime / Montage->SequenceLength + DELTA;
		float NextFrame = FMath::Clamp<float>(FMath::FloorToFloat(CurrentFraction * NumFrames) + 1.0f, 0, NumFrames);
		float NewTime = Montage->SequenceLength * (NextFrame / NumFrames);

		GetSkelMeshComponent()->GlobalAnimRateScale = 1.0f;
		GetSkelMeshComponent()->TickAnimation(NewTime - CurrentTime);

		MontagePreview_SetPlaying(false);
	}
}

void UAnimPreviewInstance::MontagePreview_StepBackward()
{
	if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
	{
		bool bWasPlaying = IsPlayingMontage() && (bLooping || bPlaying); // we need to handle non-looped case separately, even if paused during playthrough
		MontagePreview_SetReverse(true);
		if (! bWasPlaying)
		{
			if (! bLooping)
			{
				float StoppedAt = CurrentTime;
				if (! bWasPlaying)
				{
					// play montage but at last known location
					MontagePreview_Restart();
					SetPosition(StoppedAt, false);
				}
				int32 LastPreviewSectionIdx = MontagePreview_FindLastSection(MontagePreviewStartSectionIdx);
				if (FMath::Abs(CurrentTime - (Montage->CompositeSections[LastPreviewSectionIdx].GetTime() + Montage->GetSectionLength(LastPreviewSectionIdx))) <= MontagePreview_CalculateStepLength())
				{
					// special case as we could stop at the end of our last section which is also beginning of following section - we don't want to get stuck there, but be inside of our starting section
					Montage_JumpToSection(Montage->GetSectionName(LastPreviewSectionIdx));
				}
				else if (FMath::Abs(CurrentTime - Montage->CompositeSections[MontagePreviewStartSectionIdx].GetTime()) <= MontagePreview_CalculateStepLength())
				{
					// we're at the end of playing backward, jump right to the end
					Montage_JumpToSectionsEnd(Montage->GetSectionName(MontagePreviewStartSectionIdx));
					if (! bWasPlaying)
					{
						MontagePreview_SetPlaying(false);
					}
					return; // can't go further than beginning of first section
				}
			}
			else
			{
				MontagePreview_Restart();
			}
		}
		MontagePreview_SetPlaying(true);

		// Advance a single frame, leaving it paused afterwards
		int32 NumFrames = Montage->GetNumberOfFrames();
		// Add DELTA to prefer next frame when we're close to the boundary
		float CurrentFraction = CurrentTime / Montage->SequenceLength + DELTA;
		float NextFrame = FMath::Clamp<float>(FMath::FloorToFloat(CurrentFraction * NumFrames) - 1.0f, 0, NumFrames);
		float NewTime = Montage->SequenceLength * (NextFrame / NumFrames);

		GetSkelMeshComponent()->GlobalAnimRateScale = 1.0f;
		GetSkelMeshComponent()->TickAnimation(FMath::Abs(NewTime - CurrentTime));

		MontagePreview_SetPlaying(false);
	}
}

float UAnimPreviewInstance::MontagePreview_CalculateStepLength()
{
	return 1.0f / 30.0f;
}

void UAnimPreviewInstance::MontagePreview_JumpToStart()
{
	if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
	{
		int32 SectionIdx = 0;
		if (MontagePreviewType == EMPT_Normal)
		{
			SectionIdx = MontagePreviewStartSectionIdx;
		}
		// TODO hack - Montage_JumpToSection requires montage being played
		bool bWasPlaying = IsPlayingMontage();
		if (! bWasPlaying)
		{
			MontagePreview_Restart();
		}
		if (PlayRate < 0.f)
		{
			Montage_JumpToSectionsEnd(Montage->GetSectionName(SectionIdx));
		}
		else
		{
			Montage_JumpToSection(Montage->GetSectionName(SectionIdx));
		}
		if (! bWasPlaying)
		{
			MontagePreview_SetPlaying(false);
		}
	}
}

void UAnimPreviewInstance::MontagePreview_JumpToEnd()
{
	if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
	{
		int32 SectionIdx = 0;
		if (MontagePreviewType == EMPT_Normal)
		{
			SectionIdx = MontagePreviewStartSectionIdx;
		}
		// TODO hack - Montage_JumpToSectionsEnd requires montage being played
		bool bWasPlaying = IsPlayingMontage();
		if (! bWasPlaying)
		{
			MontagePreview_Restart();
		}
		if (PlayRate < 0.f)
		{
			Montage_JumpToSection(Montage->GetSectionName(MontagePreview_FindLastSection(SectionIdx)));
		}
		else
		{
			Montage_JumpToSectionsEnd(Montage->GetSectionName(MontagePreview_FindLastSection(SectionIdx)));
		}
		if (! bWasPlaying)
		{
			MontagePreview_SetPlaying(false);
		}
	}
}

void UAnimPreviewInstance::MontagePreview_JumpToPreviewStart()
{
	if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
	{
		int32 SectionIdx = 0;
		if (MontagePreviewType == EMPT_Normal)
		{
			SectionIdx = MontagePreviewStartSectionIdx;
		}
		// TODO hack - Montage_JumpToSectionsEnd requires montage being played
		bool bWasPlaying = IsPlayingMontage();
		if (! bWasPlaying)
		{
			MontagePreview_Restart();
		}
		Montage_JumpToSection(Montage->GetSectionName(PlayRate > 0.f? SectionIdx : MontagePreview_FindLastSection(SectionIdx)));
		if (! bWasPlaying)
		{
			MontagePreview_SetPlaying(false);
		}
	}
}

void UAnimPreviewInstance::MontagePreview_JumpToPosition(float NewPosition)
{
	SetPosition(NewPosition, false);
	if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
	{
		// this section will be first
		int32 NewMontagePreviewStartSectionIdx = MontagePreview_FindFirstSectionAsInMontage(Montage->GetSectionIndexFromPosition(NewPosition));
		if (MontagePreviewStartSectionIdx != NewMontagePreviewStartSectionIdx &&
			MontagePreviewType == EMPT_Normal)
		{
			MontagePreviewStartSectionIdx = NewMontagePreviewStartSectionIdx;
		}
		// setup looping to match normal playback
		MontagePreview_SetLooping(bLooping);
	}
}

void UAnimPreviewInstance::MontagePreview_RemoveBlendOut()
{
	if (FAnimMontageInstance* CurMontageInstance = GetActiveMontageInstance())
	{
		CurMontageInstance->DefaultBlendTimeMultiplier = 0.0f;
	}
}

void UAnimPreviewInstance::MontagePreview_PreviewNormal(int32 FromSectionIdx)
{
	if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
	{
		int32 PreviewFromSection = FromSectionIdx;
		if (FromSectionIdx != INDEX_NONE)
		{
			MontagePreviewStartSectionIdx = MontagePreview_FindFirstSectionAsInMontage(FromSectionIdx);
		}
		else
		{
			FromSectionIdx = MontagePreviewStartSectionIdx;
			PreviewFromSection = MontagePreviewStartSectionIdx;
		}
		MontagePreviewType = EMPT_Normal;
		Montage_Play(Montage, PlayRate);
		MontagePreview_SetLoopNormal(bLooping, FromSectionIdx);
		Montage_JumpToSection(Montage->GetSectionName(PreviewFromSection));
		MontagePreview_RemoveBlendOut();
		bPlaying = true;
	}
}

void UAnimPreviewInstance::MontagePreview_PreviewAllSections()
{
	UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset);
	if (Montage && Montage->SequenceLength > 0.f)
	{
		MontagePreviewType = EMPT_AllSections;
		Montage_Play(Montage, PlayRate);
		MontagePreview_SetLoopAllSections(bLooping);
		MontagePreview_JumpToPreviewStart();
		MontagePreview_RemoveBlendOut();
		bPlaying = true;
	}
}

void UAnimPreviewInstance::MontagePreview_SetLoopNormal(bool bIsLooping, int32 PreferSectionIdx)
{
	if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
	{
		MontagePreview_ResetSectionsOrder();

		if (PreferSectionIdx == INDEX_NONE)
		{
			PreferSectionIdx = Montage->GetSectionIndexFromPosition(CurrentTime);
		}
		int32 TotalSection = Montage->CompositeSections.Num();
		if (TotalSection > 0)
		{
			int PreferedInChain = TotalSection;
			TArray<bool> AlreadyUsed;
			AlreadyUsed.AddZeroed(TotalSection);
			while (true)
			{
				// find first not already used section
				int32 NotUsedIdx = 0;
				while (NotUsedIdx < TotalSection)
				{
					if (! AlreadyUsed[NotUsedIdx])
					{
						break;
					}
					++ NotUsedIdx;
				}
				if (NotUsedIdx >= TotalSection)
				{
					break;
				}
				// find if this is one we're looking for closest to starting one
				int32 CurSectionIdx = NotUsedIdx;
				int32 InChain = 0;
				while (true)
				{
					// find first that contains this
					if (CurSectionIdx == PreferSectionIdx &&
						InChain < PreferedInChain)
					{
						PreferedInChain = InChain;
						PreferSectionIdx = NotUsedIdx;
					}
					AlreadyUsed[CurSectionIdx] = true;
					FName NextSection = Montage->CompositeSections[CurSectionIdx].NextSectionName;
					CurSectionIdx = Montage->GetSectionIndex(NextSection);
					if (CurSectionIdx == INDEX_NONE || AlreadyUsed[CurSectionIdx]) // break loops
					{
						break;
					}
					++ InChain;
				}
				// loop this section
				SetMontageLoop(Montage, bIsLooping, Montage->CompositeSections[NotUsedIdx].SectionName);
			}
			if (Montage->CompositeSections.IsValidIndex(PreferSectionIdx))
			{
				SetMontageLoop(Montage, bIsLooping, Montage->CompositeSections[PreferSectionIdx].SectionName);
			}
		}
	}
}

void UAnimPreviewInstance::MontagePreview_SetLoopAllSetupSections(bool bIsLooping)
{
	if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
	{
		MontagePreview_ResetSectionsOrder();

		int32 TotalSection = Montage->CompositeSections.Num();
		if (TotalSection > 0)
		{
			FName FirstSection = Montage->CompositeSections[0].SectionName;
			FName PreviousSection = FirstSection;
			TArray<bool> AlreadyUsed;
			AlreadyUsed.AddZeroed(TotalSection);
			while (true)
			{
				// find first not already used section
				int32 NotUsedIdx = 0;
				while (NotUsedIdx < TotalSection)
				{
					if (! AlreadyUsed[NotUsedIdx])
					{
						break;
					}
					++ NotUsedIdx;
				}
				if (NotUsedIdx >= TotalSection)
				{
					break;
				}
				// go through all connected to join them into one big chain
				int CurSectionIdx = NotUsedIdx;
				while (true)
				{
					AlreadyUsed[CurSectionIdx] = true;
					FName CurrentSection = Montage->CompositeSections[CurSectionIdx].SectionName;
					Montage_SetNextSection(PreviousSection, CurrentSection);
					PreviousSection = CurrentSection;

					FName NextSection = Montage->CompositeSections[CurSectionIdx].NextSectionName;
					CurSectionIdx = Montage->GetSectionIndex(NextSection);
					if (CurSectionIdx == INDEX_NONE || AlreadyUsed[CurSectionIdx]) // break loops
					{
						break;
					}
				}
			}
			if (bIsLooping)
			{
				// and loop all
				Montage_SetNextSection(PreviousSection, FirstSection);
			}
		}
	}
}

void UAnimPreviewInstance::MontagePreview_SetLoopAllSections(bool bIsLooping)
{
	if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
	{
		int32 TotalSection = Montage->CompositeSections.Num();
		if (TotalSection > 0)
		{
			if (bIsLooping)
			{
				for (int i = 0; i < TotalSection; ++ i)
				{
					Montage_SetNextSection(Montage->CompositeSections[i].SectionName, Montage->CompositeSections[(i+1) % TotalSection].SectionName);
				}
			}
			else
			{
				for (int i = 0; i < TotalSection - 1; ++ i)
				{
					Montage_SetNextSection(Montage->CompositeSections[i].SectionName, Montage->CompositeSections[i+1].SectionName);
				}
				Montage_SetNextSection(Montage->CompositeSections[TotalSection - 1].SectionName, NAME_None);
			}
		}
	}
}

void UAnimPreviewInstance::MontagePreview_ResetSectionsOrder()
{
	if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
	{
		int32 TotalSection = Montage->CompositeSections.Num();
		// restore to default
		for (int i = 0; i < TotalSection; ++ i)
		{
			Montage_SetNextSection(Montage->CompositeSections[i].SectionName, Montage->CompositeSections[i].NextSectionName);
		}
	}
}

int32 UAnimPreviewInstance::MontagePreview_FindFirstSectionAsInMontage(int32 ForSectionIdx)
{
	int32 ResultIdx = ForSectionIdx;
	// Montage does not have looping set up, so it should be valid and it gets
	if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
	{
		TArray<bool> AlreadyVisited;
		AlreadyVisited.AddZeroed(Montage->CompositeSections.Num());
		bool bFoundResult = false;
		while (! bFoundResult)
		{
			int32 UnusedSectionIdx = INDEX_NONE;
			for (int32 Idx = 0; Idx < Montage->CompositeSections.Num(); ++ Idx)
			{
				if (! AlreadyVisited[Idx])
				{
					UnusedSectionIdx = Idx;
					break;
				}
			}
			if (UnusedSectionIdx == INDEX_NONE)
			{
				break;
			}
			// check if this has ForSectionIdx
			int32 CurrentSectionIdx = UnusedSectionIdx;
			while (CurrentSectionIdx != INDEX_NONE && ! AlreadyVisited[CurrentSectionIdx])
			{
				if (CurrentSectionIdx == ForSectionIdx)
				{
					ResultIdx = UnusedSectionIdx;
					bFoundResult = true;
					break;
				}
				AlreadyVisited[CurrentSectionIdx] = true;
				FName NextSection = Montage->CompositeSections[CurrentSectionIdx].NextSectionName;
				CurrentSectionIdx = Montage->GetSectionIndex(NextSection);
			}
		}
	}
	return ResultIdx;
}

int32 UAnimPreviewInstance::MontagePreview_FindLastSection(int32 StartSectionIdx)
{
	int32 ResultIdx = StartSectionIdx;
	if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
	{
		if (FAnimMontageInstance* CurMontageInstance = GetActiveMontageInstance())
		{
			int32 TotalSection = Montage->CompositeSections.Num();
			if (TotalSection > 0)
			{
				TArray<bool> AlreadyVisited;
				AlreadyVisited.AddZeroed(TotalSection);
				int32 CurrentSectionIdx = StartSectionIdx;
				while (CurrentSectionIdx != INDEX_NONE && ! AlreadyVisited[CurrentSectionIdx])
				{
					AlreadyVisited[CurrentSectionIdx] = true;
					ResultIdx = CurrentSectionIdx;
					if (CurMontageInstance->NextSections.IsValidIndex(CurrentSectionIdx))
					{
						CurrentSectionIdx = CurMontageInstance->NextSections[CurrentSectionIdx];
					}
				}
			}
		}
	}
	return ResultIdx;
}

void UAnimPreviewInstance::BakeAnimation()
{
	if (UAnimSequence* Sequence = Cast<UAnimSequence>(CurrentAsset))
	{
		FScopedTransaction ScopedTransaction(LOCTEXT("BakeAnimation", "Bake Animation"));
		Sequence->Modify(true);
		Sequence->BakeTrackCurvesToRawAnimation();
	}
}

void UAnimPreviewInstance::EnableControllers(bool bEnable)
{
	bEnableControllers = bEnable;
}
#undef LOCTEXT_NAMESPACE