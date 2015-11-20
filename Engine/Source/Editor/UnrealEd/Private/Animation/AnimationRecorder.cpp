// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "AnimationRecorder.h"
#include "Animation/AnimCompress.h"
#include "Animation/AnimCompress_BitwiseCompressOnly.h"
#include "SCreateAnimationDlg.h"
#include "Runtime/AssetRegistry/Public/AssetRegistryModule.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "EngineLogs.h"

#define LOCTEXT_NAMESPACE "FAnimationRecorder"

/////////////////////////////////////////////////////

float FAnimationRecorder::DefaultSampleRate = DEFAULT_SAMPLERATE;

static TAutoConsoleVariable<float> CVarAnimRecorderSampleRate(
	TEXT("AnimRecorder.SampleRate"),
	DEFAULT_SAMPLERATE,
	TEXT("Sets the sample rate for the animation recorder system"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAnimRecorderWorldSpace(
	TEXT("AnimRecorder.RecordInWorldSpace"),
	1,
	TEXT("True to record anim keys in world space, false to record only in local space."),
	ECVF_Default);

FAnimationRecorder::FAnimationRecorder()
	: AnimationObject(nullptr)
	, bRecordLocalToWorld(false)
	, bAutoSaveAsset(false)

{
	SetSampleRate(DefaultSampleRate);
}

FAnimationRecorder::~FAnimationRecorder()
{
	StopRecord(false);
}

void FAnimationRecorder::SetSampleRate(float SampleRateHz)
{
	if (SampleRateHz <= 0.f)
	{
		// invalid rate passed in, fall back to default
		SampleRateHz = FAnimationRecorder::DefaultSampleRate;
	}

	IntervalTime = 1.0f / SampleRateHz;
	MaxFrame = SampleRateHz * 60 * 10;		 // for now, set the max limit to 10 mins. 
}

bool FAnimationRecorder::SetAnimCompressionScheme(TSubclassOf<UAnimCompress> SchemeClass)
{
	if (AnimationObject)
	{
		UAnimCompress* const SchemeObject = NewObject<UAnimCompress>(GetTransientPackage(), SchemeClass);
		if (SchemeObject)
		{
			AnimationObject->CompressionScheme = SchemeObject;
			return true;
		}
	}

	return false;
}

// Internal. Pops up a dialog to get saved asset path
static bool PromptUserForAssetPath(FString& AssetPath, FString& AssetName)
{
	TSharedRef<SCreateAnimationDlg> NewAnimDlg = SNew(SCreateAnimationDlg);
	if (NewAnimDlg->ShowModal() != EAppReturnType::Cancel)
	{
		AssetPath = NewAnimDlg->GetFullAssetPath();
		AssetName = NewAnimDlg->GetAssetName();
		return true;
	}

	return false;
}

bool FAnimationRecorder::TriggerRecordAnimation(USkeletalMeshComponent* Component)
{
	FString AssetPath;
	FString AssetName;

	if (!Component || !Component->SkeletalMesh || !Component->SkeletalMesh->Skeleton)
	{
		return false;
	}

	// ask for path
	if (PromptUserForAssetPath(AssetPath, AssetName))
	{
		return TriggerRecordAnimation(Component, AssetPath, AssetName);
	}

	return false;
}

bool FAnimationRecorder::TriggerRecordAnimation(USkeletalMeshComponent* Component, FString AssetPath, FString AssetName)
{
	if (!Component || !Component->SkeletalMesh || !Component->SkeletalMesh->Skeleton)
	{
		return false;
	}

	// create the asset
	UObject* Parent = AssetPath.IsEmpty() ? nullptr : CreatePackage(nullptr, *AssetPath);
	if (Parent == nullptr)
	{
		// bad or no path passed in, do the popup
		if (PromptUserForAssetPath(AssetPath, AssetName) == false)
		{
			return false;
		}
		
		Parent = CreatePackage(nullptr, *AssetPath);
	}

	UObject* const Object = LoadObject<UObject>(Parent, *AssetName, nullptr, LOAD_None, nullptr);
	// if object with same name exists, warn user
	if (Object)
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_AssetExist", "Asset with same name exists. Can't overwrite another asset"));
		return false;		// failed
	}

	// If not, create new one now.
	UAnimSequence* const NewSeq = NewObject<UAnimSequence>(Parent, *AssetName, RF_Public | RF_Standalone);
	if (NewSeq)
	{
		// set skeleton
		NewSeq->SetSkeleton(Component->SkeletalMesh->Skeleton);
		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(NewSeq);
		StartRecord(Component, NewSeq);

		return true;
	}

	return false;
}


void FAnimationRecorder::StartRecord(USkeletalMeshComponent* Component, UAnimSequence* InAnimationObject)
{
	TimePassed = 0.f;
	AnimationObject = InAnimationObject;

	AnimationObject->RawAnimationData.Empty();
	AnimationObject->TrackToSkeletonMapTable.Empty();
	AnimationObject->AnimationTrackNames.Empty();

	PreviousSpacesBases = Component->GetSpaceBases();
	PreviousComponentToWorld = Component->ComponentToWorld;

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
	Record(Component, PreviousComponentToWorld, PreviousSpacesBases, 0);
}

UAnimSequence* FAnimationRecorder::StopRecord(bool bShowMessage)
{
	if (AnimationObject)
	{
		int32 NumFrames = LastFrame  + 1;
		AnimationObject->NumFrames = NumFrames;

		// can't use TimePassed. That is just total time that has been passed, not necessarily match with frame count
		AnimationObject->SequenceLength = (NumFrames>1) ? (NumFrames-1) * IntervalTime : MINIMUM_ANIMATION_LENGTH;
		AnimationObject->PostProcessSequence();
		AnimationObject->MarkPackageDirty();
		
		// save the package to disk, for convenience and so we can run this in standalone mode
		if (bAutoSaveAsset)
		{
			UPackage* const Package = AnimationObject->GetOutermost();
			FString const PackageName = Package->GetName();
			FString const PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

			UPackage::SavePackage(Package, NULL, RF_Standalone, *PackageFileName, GError, nullptr, false, true, SAVE_NoError);
		}

		UAnimSequence* ReturnObject = AnimationObject;

		// notify to user
		if (bShowMessage)
		{
			const FText NotificationText = FText::Format(LOCTEXT("RecordAnimation", "'{0}' has been successfully recorded [{1} frames : {2} sec(s) @ {3} Hz]"),
				FText::FromString(AnimationObject->GetName()),
				FText::AsNumber(AnimationObject->NumFrames),
				FText::AsNumber(AnimationObject->SequenceLength),
				FText::AsNumber(1.f / IntervalTime)
				);

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

void FAnimationRecorder::UpdateRecord(USkeletalMeshComponent* Component, float DeltaTime)
{
	// if no animation object, return
	if (!AnimationObject || !Component)
	{
		return;
	}

	// no sime time, no record
	if (DeltaTime <= 0.f)
	{
		return;
	}

	float const PreviousTimePassed = TimePassed;
	TimePassed += DeltaTime;

	// time passed has been updated
	// now find what frames we need to update
	int32 FramesRecorded = LastFrame;
	int32 FramesToRecord = FPlatformMath::TruncToInt(TimePassed / IntervalTime);

	if (FramesRecorded < FramesToRecord)
	{
		const TArray<FTransform>& SpaceBases = Component->GetSpaceBases();

		check(SpaceBases.Num() == PreviousSpacesBases.Num());

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
			float BlendAlpha = (CurrentTime - PreviousTimePassed) / DeltaTime;

			UE_LOG(LogAnimation, Warning, TEXT("Current Frame Count : %d, BlendAlpha : %0.2f"), FramesRecorded + 1, BlendAlpha);

			// for now we just concern component space, not skeleton space
			for (int32 BoneIndex = 0; BoneIndex<SpaceBases.Num(); ++BoneIndex)
			{
				BlendedSpaceBases[BoneIndex].Blend(PreviousSpacesBases[BoneIndex], SpaceBases[BoneIndex], BlendAlpha);
			}

			FTransform BlendedComponentToWorld;
			BlendedComponentToWorld.Blend(PreviousComponentToWorld, Component->ComponentToWorld, BlendAlpha);

			Record(Component, BlendedComponentToWorld, BlendedSpaceBases, FramesRecorded + 1);
			++FramesRecorded;
		}
	}

	//save to current transform
	PreviousSpacesBases = Component->GetSpaceBases();
	PreviousComponentToWorld = Component->ComponentToWorld;

	// if we passed MaxFrame, just stop it
	if (FramesRecorded > MaxFrame)
	{
		UE_LOG(LogAnimation, Warning, TEXT("Animation Recording exceeds the time limited (10 mins). Stopping recording animation... "))
		StopRecord(true);
	}
}

void FAnimationRecorder::Record( USkeletalMeshComponent* Component, FTransform const& ComponentToWorld, TArray<FTransform> SpacesBases, int32 FrameToAdd )
{
	if (ensure(AnimationObject))
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
					LocalTransform *= ComponentToWorld;
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

void FAnimationRecorderManager::Tick(float DeltaTime)
{
	for (auto& Inst : RecorderInstances)
	{
		Inst.Update(DeltaTime);
	}
}

FAnimationRecorderManager::FAnimationRecorderManager()
{
}

FAnimationRecorderManager::~FAnimationRecorderManager()
{
}

FAnimationRecorderManager& FAnimationRecorderManager::Get()
{
	static FAnimationRecorderManager AnimRecorderManager;
	return AnimRecorderManager;
}


FAnimRecorderInstance::FAnimRecorderInstance()
	: Actor(nullptr),
	SkelComp(nullptr),
	Recorder(nullptr)
{
}

void FAnimRecorderInstance::Init(AActor* InActor, USkeletalMeshComponent* InComponent, FString InAssetPath, FString InAssetName, float SampleRateHz, bool bRecordInWorldSpace)
{
	Actor = InActor;
	SkelComp = InComponent;
	AssetPath = InAssetPath;
	AssetName = InAssetName;
	Recorder = new FAnimationRecorder();
	Recorder->SetSampleRate(SampleRateHz);
	Recorder->bRecordLocalToWorld = bRecordInWorldSpace;
	Recorder->SetAnimCompressionScheme(UAnimCompress_BitwiseCompressOnly::StaticClass());

	// always autosave through this codepath
	Recorder->bAutoSaveAsset = true;

	if (InComponent)
	{
		CachedSkelCompForcedLodModel = InComponent->ForcedLodModel;
		InComponent->ForcedLodModel = 1;
	}
}

FAnimRecorderInstance::~FAnimRecorderInstance()
{
	if (Recorder)
	{
		delete Recorder;
		Recorder = nullptr;
	}
}

bool FAnimRecorderInstance::BeginRecording()
{
	if (Recorder)
	{
		return Recorder->TriggerRecordAnimation(SkelComp.Get(), AssetPath, AssetName);
	}

	return false;
}

void FAnimRecorderInstance::Update(float DeltaTime)
{
	if (Recorder)
	{
		Recorder->UpdateRecord(SkelComp.Get(), DeltaTime);
	}
}

void FAnimRecorderInstance::FinishRecording()
{
	const FText FinishRecordingAnimationSlowTask = LOCTEXT("FinishRecordingAnimationSlowTask", "Finalizing recorded animation");
	if (Recorder)
	{
		Recorder->StopRecord(true);
	}

	// restore force lod setting
	if (SkelComp.IsValid())
	{
		SkelComp->ForcedLodModel = CachedSkelCompForcedLodModel;
	}
}

bool FAnimationRecorderManager::RecordAnimation(AActor* Actor, USkeletalMeshComponent* Component, FString AssetPath, FString AssetName)
{
	int32 const NewInstIdx = RecorderInstances.AddDefaulted();
	
	FAnimRecorderInstance& NewInst = RecorderInstances[NewInstIdx];
	NewInst.Init(Actor, Component, AssetPath, AssetName, CVarAnimRecorderSampleRate.GetValueOnAnyThread(), (CVarAnimRecorderWorldSpace.GetValueOnAnyThread() != 0));

	bool const bSuccess = NewInst.BeginRecording();
	if (bSuccess == false)
	{
		// failed, remove it
		RecorderInstances.RemoveAtSwap(NewInstIdx);
	}

	return bSuccess;
}

void FAnimationRecorderManager::StopRecordingAnimation(AActor* Actor, USkeletalMeshComponent* Component)
{
	FAnimRecorderInstance* InstToStop = nullptr;
	for (int32 Idx = 0; Idx < RecorderInstances.Num(); ++Idx)
	{
		FAnimRecorderInstance& Inst = RecorderInstances[Idx];
		if (Inst.Actor == Actor && Inst.SkelComp == Component)
		{
			// stop and finalize recoded data
			Inst.FinishRecording();

			// remove instance, which will clean itself up
			RecorderInstances.RemoveAtSwap(Idx);

			// all done
			break;
		}
	}
}

void FAnimationRecorderManager::StopRecordingAllAnimations()
{
	for (auto& Inst : RecorderInstances)
	{
		Inst.FinishRecording();
	}

	RecorderInstances.Empty();
}


#undef LOCTEXT_NAMESPACE

