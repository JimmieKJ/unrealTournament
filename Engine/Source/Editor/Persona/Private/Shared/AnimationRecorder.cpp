// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"
#include "AnimationRecorder.h"
#include "SAnimationDlgs.h"
#include "Runtime/AssetRegistry/Public/AssetRegistryModule.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "EngineLogs.h"

#define LOCTEXT_NAMESPACE "FAnimationRecorder"

/////////////////////////////////////////////////////

FAnimationRecorder::FAnimationRecorder()
	: IntervalTime(1.0f/DEFAULT_SAMPLERATE)
	, MaxFrame(DEFAULT_SAMPLERATE * 60 * 10) // for now, set the max limit to 10 mins. 
	, AnimationObject(NULL)
	, bRecordLocalToWorld(false)
{

}

FAnimationRecorder::~FAnimationRecorder()
{
	StopRecord(false);
}

// get record configuration
bool GetRecordConfig(FString& AssetPath, FString& AssetName)
{
	TSharedRef<SCreateAnimationDlg> NewAnimDlg =
		SNew(SCreateAnimationDlg);

	if(NewAnimDlg->ShowModal() != EAppReturnType::Cancel)
	{
		AssetPath = NewAnimDlg->GetFullAssetPath();
		AssetName = NewAnimDlg->GetAssetName();
		return true;
	}

	return false;
}

bool FAnimationRecorder::TriggerRecordAnimation(USkeletalMeshComponent * Component)
{
	// ask for path
	FString AssetPath;
	FString AssetName;

	if (!Component || !Component->SkeletalMesh || !Component->SkeletalMesh->Skeleton)
	{
		return false;
	}

	if(GetRecordConfig(AssetPath, AssetName))
	{
		// create the asset
		UObject* 	Parent = CreatePackage(NULL, *AssetPath);
		UObject* Object = LoadObject<UObject>(Parent, *AssetName, NULL, LOAD_None, NULL);
		// if object with same name exists, warn user
		if(Object)
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_AssetExist", "Asset with same name exists. Can't overwrite another asset"));
			return false; // Move on to next sequence...
		}

		// If not, create new one now.
		UAnimSequence * NewSeq = NewObject<UAnimSequence>(Parent, *AssetName, RF_Public | RF_Standalone);
		if(NewSeq)
		{
			// set skeleton
			NewSeq->SetSkeleton(Component->SkeletalMesh->Skeleton);
			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(NewSeq);
			StartRecord(Component, NewSeq);

			return true;
		}
	}

	return false;
}

void FAnimationRecorder::StartRecord(USkeletalMeshComponent * Component, UAnimSequence * InAnimationObject)
{
	TimePassed = 0.f;
	AnimationObject = InAnimationObject;

	AnimationObject->RawAnimationData.Empty();
	AnimationObject->TrackToSkeletonMapTable.Empty();
	AnimationObject->AnimationTrackNames.Empty();

	PreviousSpacesBases = Component->GetSpaceBases();

	LastFrame = 0;
	AnimationObject->SequenceLength = 0.f;
	AnimationObject->NumFrames = 0;

	USkeleton* AnimSkeleton = AnimationObject->GetSkeleton();
	// add all frames
	for (int32 BoneIndex=0; BoneIndex <PreviousSpacesBases.Num(); ++BoneIndex)
	{
		// verify if this bone exists in skeleton
		int32 BoneTreeIndex = AnimSkeleton->GetSkeletonBoneIndexFromMeshBoneIndex(Component->SkeletalMesh, BoneIndex);
		if (BoneTreeIndex != INDEX_NONE)
		{
			// add tracks for the bone existing
			FName BoneTreeName = AnimSkeleton->GetReferenceSkeleton().GetBoneName(BoneTreeIndex);
			AnimationObject->AnimationTrackNames.Add(BoneTreeName);
			// add mapping to skeleton bone track
			AnimationObject->TrackToSkeletonMapTable.Add(FTrackToSkeletonMap(BoneTreeIndex));
		}
	}

	int32 NumTracks = AnimationObject->AnimationTrackNames.Num();
	// add tracks
	AnimationObject->RawAnimationData.AddZeroed(NumTracks);

	// record the first frame;
	Record(Component, PreviousSpacesBases, 0);
}

UAnimSequence * FAnimationRecorder::StopRecord(bool bShowMessage)
{
	if (AnimationObject)
	{
		int32 NumFrames = LastFrame  + 1;
		AnimationObject->NumFrames = NumFrames;

		// can't use TimePassed. That is just total time that has been passed, not necessarily match with frame count
		AnimationObject->SequenceLength = (NumFrames>1)? (NumFrames-1) * IntervalTime : MINIMUM_ANIMATION_LENGTH;
		AnimationObject->PostProcessSequence();
		AnimationObject->MarkPackageDirty();

		UAnimSequence * ReturnObject = AnimationObject;

		// notify to user
		if (bShowMessage)
		{
			const FText NotificationText = FText::Format( LOCTEXT("RecordAnimation", "'{0}' has been successfully recorded [{1} frames : {2} sec(s)]" ), 
				FText::FromString( AnimationObject->GetName() ),
				FText::AsNumber( AnimationObject->NumFrames ),
				FText::AsNumber( AnimationObject->SequenceLength ));

			//This is not showing well in the Persona, so opening dialog first. 
			//right now it will crash if you don't wait until end of the record, so it is important for users to know
			//this is done
					
			FNotificationInfo Info(NotificationText);
			Info.ExpireDuration = 5.0f;
			Info.bUseLargeFont = false;
			TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
			if ( Notification.IsValid() )
			{
				Notification->SetCompletionState( SNotificationItem::CS_Success );
			}

			FAssetRegistryModule::AssetCreated(AnimationObject);
			
			FMessageDialog::Open(EAppMsgType::Ok, NotificationText);
		}

		AnimationObject = NULL;
		PreviousSpacesBases.Empty();

		return ReturnObject;
	}

	return NULL;
}

// return false if nothing to update
// return true if it has properly updated
void FAnimationRecorder::UpdateRecord(USkeletalMeshComponent * Component, float DeltaTime)
{
	// if no animation object, return
	if (!AnimationObject)
	{
		return;
	}

	const float MaxDelta = 1.f/10.f;
	float UseDelta = FMath::Min<float>(DeltaTime, MaxDelta);

	// if longer than 1 second, you'll see the halt on the animation
	if (UseDelta <= 0.f)
	{
		return;
	}

	float PreviousTimePassed = TimePassed;
	TimePassed += UseDelta;

	// time passed has been updated
	// now find what frames we need to update
	int32 FramesRecorded = LastFrame;
	int32 FramesToRecord = FPlatformMath::TruncToInt(TimePassed / IntervalTime);

	if (FramesRecorded >= FramesToRecord)
	{
		return;
	}

	const TArray<FTransform> & SpaceBases = Component->GetSpaceBases();

	check (SpaceBases.Num() == PreviousSpacesBases.Num());

	TArray<FTransform> BlendedSpaceBases;
	BlendedSpaceBases.AddZeroed(SpaceBases.Num());

	UE_LOG(LogAnimation, Warning, TEXT("DeltaTime : %0.2f, Current Frame Count : %d, Frames To Record : %d, TimePassed : %0.2f"), DeltaTime
		, FramesRecorded, FramesToRecord, TimePassed);

	// if we need to record frame
	while (FramesToRecord > FramesRecorded)
	{
		// find what frames we need to record
		// convert to time
		float CurrentTime = (FramesRecorded + 1) * IntervalTime;
		float BlendAlpha = (CurrentTime - PreviousTimePassed)/UseDelta;

		UE_LOG(LogAnimation, Warning, TEXT("Current Frame Count : %d, BlendAlpha : %0.2f"), FramesRecorded + 1, BlendAlpha);

		// for now we just concern component space, not skeleton space
		for (int32 BoneIndex=0; BoneIndex<SpaceBases.Num(); ++BoneIndex)
		{
			BlendedSpaceBases[BoneIndex].Blend(PreviousSpacesBases[BoneIndex], SpaceBases[BoneIndex], BlendAlpha);
		}

		Record(Component, BlendedSpaceBases, FramesRecorded + 1);
		++FramesRecorded;
	}

	//save to current transform
	PreviousSpacesBases = Component->GetSpaceBases();

	// if we passed MaxFrame, just stop it
	if (FramesRecorded > MaxFrame)
	{
		UE_LOG(LogAnimation, Warning, TEXT("Animation Recording exceeds the time limited (10 mins). Stopping recording animation... "))
		StopRecord(true);
	}
}

void FAnimationRecorder::Record( USkeletalMeshComponent * Component, TArray<FTransform> SpacesBases, int32 FrameToAdd )
{
	if (ensure (AnimationObject))
	{
		USkeleton* AnimSkeleton = AnimationObject->GetSkeleton();
		for (int32 TrackIndex=0; TrackIndex <AnimationObject->RawAnimationData.Num(); ++TrackIndex)
		{
			// verify if this bone exists in skeleton
			int32 BoneTreeIndex = AnimationObject->GetSkeletonIndexFromTrackIndex(TrackIndex);
			if (BoneTreeIndex != INDEX_NONE)
			{
				int32 BoneIndex = AnimSkeleton->GetMeshBoneIndexFromSkeletonBoneIndex(Component->SkeletalMesh, BoneTreeIndex);
				int32 ParentIndex = Component->SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
				FTransform LocalTransform = SpacesBases[BoneIndex];
				if ( ParentIndex != INDEX_NONE )
				{
					LocalTransform.SetToRelativeTransform(SpacesBases[ParentIndex]);
				}
				// if record local to world, we'd like to consider component to world to be in root
				else if (bRecordLocalToWorld)
				{
					LocalTransform *= Component->ComponentToWorld;
				}

				FRawAnimSequenceTrack& RawTrack = AnimationObject->RawAnimationData[TrackIndex];
				RawTrack.PosKeys.Add(LocalTransform.GetTranslation());
				RawTrack.RotKeys.Add(LocalTransform.GetRotation());
				RawTrack.ScaleKeys.Add(LocalTransform.GetScale3D());

				// verification
				check (FrameToAdd == RawTrack.PosKeys.Num()-1);
			}
		}

		LastFrame = FrameToAdd;
	}
}

#undef LOCTEXT_NAMESPACE

