// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "PersonaPrivatePCH.h"
#include "AnimGraphDefinitions.h"
#include "AnimPreviewInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Persona.h"
#include "SAnimationScrubPanel.h"
#include "Editor/KismetWidgets/Public/SScrubControlPanel.h"
#include "AnimationUtils.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "AnimationScrubPanel"

void SAnimationScrubPanel::Construct( const SAnimationScrubPanel::FArguments& InArgs )
{
	bSliderBeingDragged = false;
	PersonaPtr = InArgs._Persona;
	LockedSequence = InArgs._LockedSequence;
	OnSetInputViewRange = InArgs._OnSetInputViewRange;

	// register delegate for anim change notification
	PersonaPtr.Pin()->RegisterOnAnimChanged( FPersona::FOnAnimChanged::CreateSP(this, &SAnimationScrubPanel::AnimChanged) );
	check(PersonaPtr.IsValid());

	this->ChildSlot
	[
		SNew(SHorizontalBox)
		.AddMetaData<FTagMetaData>(TEXT("AnimScrub.Scrub"))
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Fill) 
		.VAlign(VAlign_Center)
		.FillWidth(1)
		.Padding( FMargin( 3.0f, 0.0f) )
		[
			SAssignNew(ScrubControlPanel, SScrubControlPanel)
			.IsEnabled(true)//this, &SAnimationScrubPanel::DoesSyncViewport)
			.Value(this, &SAnimationScrubPanel::GetScrubValue)
			.NumOfKeys(this, &SAnimationScrubPanel::GetNumOfFrames)
			.SequenceLength(this, &SAnimationScrubPanel::GetSequenceLength)
			.OnValueChanged(this, &SAnimationScrubPanel::OnValueChanged)
			.OnBeginSliderMovement(this, &SAnimationScrubPanel::OnBeginSliderMovement)
			.OnEndSliderMovement(this, &SAnimationScrubPanel::OnEndSliderMovement)
			.OnClickedForwardPlay(this, &SAnimationScrubPanel::OnClick_Forward)
			.OnClickedForwardStep(this, &SAnimationScrubPanel::OnClick_Forward_Step)
			.OnClickedForwardEnd(this, &SAnimationScrubPanel::OnClick_Forward_End)
			.OnClickedBackwardPlay(this, &SAnimationScrubPanel::OnClick_Backward)
			.OnClickedBackwardStep(this, &SAnimationScrubPanel::OnClick_Backward_Step)
			.OnClickedBackwardEnd(this, &SAnimationScrubPanel::OnClick_Backward_End)
			.OnClickedToggleLoop(this, &SAnimationScrubPanel::OnClick_ToggleLoop)
			.OnGetLooping(this, &SAnimationScrubPanel::IsLoopStatusOn)
			.OnGetPlaybackMode(this, &SAnimationScrubPanel::GetPlaybackMode)
			.ViewInputMin(InArgs._ViewInputMin)
			.ViewInputMax(InArgs._ViewInputMax)
			.OnSetInputViewRange(InArgs._OnSetInputViewRange)
			.OnCropAnimSequence( this, &SAnimationScrubPanel::OnCropAnimSequence )
			.OnAddAnimSequence( this, &SAnimationScrubPanel::OnInsertAnimSequence )
			.OnAppendAnimSequence(this, &SAnimationScrubPanel::OnAppendAnimSequence )
			.OnReZeroAnimSequence( this, &SAnimationScrubPanel::OnReZeroAnimSequence )
			.bAllowZoom(InArgs._bAllowZoom)
			.IsRealtimeStreamingMode(this, &SAnimationScrubPanel::IsRealtimeStreamingMode)
		]
	];

}

SAnimationScrubPanel::~SAnimationScrubPanel()
{
	if (PersonaPtr.IsValid())
	{
		PersonaPtr.Pin()->UnregisterOnAnimChanged(this);
	}
}

FReply SAnimationScrubPanel::OnClick_Forward_Step()
{
	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		PreviewInstance->SetPlaying(false);
		PreviewInstance->StepForward();
	}
	else if (UDebugSkelMeshComponent* SMC = PersonaPtr.Pin()->GetPreviewMeshComponent())
	{
		//@TODO: Should we hardcode 30 Hz here?
		{
			const float TargetFramerate = 30.0f;

			// Advance a single frame, leaving it paused afterwards
			SMC->GlobalAnimRateScale = 1.0f;
			SMC->TickAnimation(1.0f / TargetFramerate, false);
			SMC->GlobalAnimRateScale = 0.0f;
		}
	}

	return FReply::Handled();
}

FReply SAnimationScrubPanel::OnClick_Forward_End()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		PreviewInstance->SetPlaying(false);
		PreviewInstance->SetPosition(PreviewInstance->GetLength(), false);
	}

	return FReply::Handled();
}

FReply SAnimationScrubPanel::OnClick_Backward_Step()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		PreviewInstance->SetPlaying(false);
		PreviewInstance->StepBackward();
	}
	return FReply::Handled();
}

FReply SAnimationScrubPanel::OnClick_Backward_End()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		PreviewInstance->SetPlaying(false);
		PreviewInstance->SetPosition(0.f, false);
	}
	return FReply::Handled();
}

FReply SAnimationScrubPanel::OnClick_Forward()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		bool bIsReverse = PreviewInstance->IsReverse();
		bool bIsPlaying = PreviewInstance->IsPlaying();
		// if current bIsReverse and bIsPlaying, we'd like to just turn off reverse
		if (bIsReverse && bIsPlaying)
		{
			PreviewInstance->SetReverse(false);
		}
		// already playing, simply pause
		else if (bIsPlaying) 
		{
			PreviewInstance->SetPlaying(false);
		}
		// if not playing, play forward
		else 
		{
			//if we're at the end of the animation, jump back to the beginning before playing
			if ( GetScrubValue() >= GetSequenceLength() )
			{
				PreviewInstance->SetPosition(0.0f, false);
			}

			PreviewInstance->SetReverse(false);
			PreviewInstance->SetPlaying(true);
		}
	}
	else if (UDebugSkelMeshComponent* SMC = PersonaPtr.Pin()->GetPreviewMeshComponent())
	{

		SMC->GlobalAnimRateScale = (SMC->GlobalAnimRateScale > 0.0f) ? 0.0f : 1.0f;
	}

	return FReply::Handled();
}

FReply SAnimationScrubPanel::OnClick_Backward()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		bool bIsReverse = PreviewInstance->IsReverse();
		bool bIsPlaying = PreviewInstance->IsPlaying();
		// if currently playing forward, just simply turn on reverse
		if (!bIsReverse && bIsPlaying)
		{
			PreviewInstance->SetReverse(true);
		}
		else if (bIsPlaying)
		{
			PreviewInstance->SetPlaying(false);
		}
		else
		{
			//if we're at the beginning of the animation, jump back to the end before playing
			if ( GetScrubValue() <= 0.0f )
			{
				PreviewInstance->SetPosition(GetSequenceLength(), false);
			}

			PreviewInstance->SetPlaying(true);
			PreviewInstance->SetReverse(true);
		}
	}
	return FReply::Handled();
}

FReply SAnimationScrubPanel::OnClick_ToggleLoop()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		bool bIsLooping = PreviewInstance->IsLooping();
		PreviewInstance->SetLooping(!bIsLooping);
	}
	return FReply::Handled();
}

bool SAnimationScrubPanel::IsLoopStatusOn() const
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	return (PreviewInstance && PreviewInstance->IsLooping());
}

EPlaybackMode::Type SAnimationScrubPanel::GetPlaybackMode() const
{
	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		if (PreviewInstance->IsPlaying())
		{
			return PreviewInstance->IsReverse() ? EPlaybackMode::PlayingReverse : EPlaybackMode::PlayingForward;
		}
		return EPlaybackMode::Stopped;
	}
	else if (UDebugSkelMeshComponent* SMC = PersonaPtr.Pin()->GetPreviewMeshComponent())
	{
		return (SMC->GlobalAnimRateScale > 0.0f) ? EPlaybackMode::PlayingForward : EPlaybackMode::Stopped;
	}
	
	return EPlaybackMode::Stopped;
}

bool SAnimationScrubPanel::IsRealtimeStreamingMode() const
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	return ( ! (PreviewInstance && PreviewInstance->GetCurrentAsset()) );
}

void SAnimationScrubPanel::OnValueChanged(float NewValue)
{
	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		PreviewInstance->SetPosition(NewValue);
	}
	else
	{
		UAnimInstance* Instance;
		FAnimBlueprintDebugData* DebugData;
		if (GetAnimBlueprintDebugData(/*out*/ Instance, /*out*/ DebugData))
		{
			DebugData->SetSnapshotIndexByTime(Instance, NewValue);
		}
	}
}

// make sure viewport is freshes
void SAnimationScrubPanel::OnBeginSliderMovement()
{
	bSliderBeingDragged = true;

	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		PreviewInstance->SetPlaying(false);
	}
}

void SAnimationScrubPanel::OnEndSliderMovement(float NewValue)
{
	bSliderBeingDragged = false;
}

void SAnimationScrubPanel::AnimChanged(UAnimationAsset* AnimAsset)
{
}

uint32 SAnimationScrubPanel::GetNumOfFrames() const
{
	if (DoesSyncViewport())
	{
		UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
		float Length = PreviewInstance->GetLength();
		// if anim sequence, use correct num frames
		int32 NumFrames = (int32) (Length/0.0333f); 
		if (PreviewInstance->GetCurrentAsset() && PreviewInstance->GetCurrentAsset()->IsA(UAnimSequenceBase::StaticClass()))
		{
			NumFrames = CastChecked<UAnimSequenceBase>(PreviewInstance->GetCurrentAsset())->GetNumberOfFrames();
		}
		return NumFrames;
	}
	else if (LockedSequence)
	{
		return LockedSequence->GetNumberOfFrames();
	}
	else
	{
		UAnimInstance* Instance;
		FAnimBlueprintDebugData* DebugData;
		if (GetAnimBlueprintDebugData(/*out*/ Instance, /*out*/ DebugData))
		{
			return DebugData->GetSnapshotLengthInFrames();
		}
	}

	return 1;
}

float SAnimationScrubPanel::GetSequenceLength() const
{
	if (DoesSyncViewport())
	{
		UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
		return PreviewInstance->GetLength();
	}
	else if (LockedSequence)
	{
		return LockedSequence->SequenceLength;
	}
	else
	{
		UAnimInstance* Instance;
		FAnimBlueprintDebugData* DebugData;
		if (GetAnimBlueprintDebugData(/*out*/ Instance, /*out*/ DebugData))
		{
			return Instance->LifeTimer;
		}
	}

	return 0.f;
}

bool SAnimationScrubPanel::DoesSyncViewport() const
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();

	return (( LockedSequence==NULL && PreviewInstance ) || ( LockedSequence && PreviewInstance && PreviewInstance->GetCurrentAsset() == LockedSequence ));
}

void SAnimationScrubPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bSliderBeingDragged)
	{
		if (PersonaPtr.IsValid())
		{
			PersonaPtr.Pin()->RefreshViewport();
		}
		else
		{
			// if character editor isn't valid, we should just ignore update
			bSliderBeingDragged = false;
		}
	}
}

class UAnimSingleNodeInstance* SAnimationScrubPanel::GetPreviewInstance() const
{
	UDebugSkelMeshComponent* PreviewMeshComponent = PersonaPtr.Pin()->GetPreviewMeshComponent();
	return PreviewMeshComponent && PreviewMeshComponent->IsPreviewOn()? PreviewMeshComponent->PreviewInstance : NULL;
}

float SAnimationScrubPanel::GetScrubValue() const
{
	if (DoesSyncViewport())
	{
		UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
		if (PreviewInstance)
		{
			return PreviewInstance->GetCurrentTime(); 
		}
	}
	else
	{
		UAnimInstance* Instance;
		FAnimBlueprintDebugData* DebugData;
		if (GetAnimBlueprintDebugData(/*out*/ Instance, /*out*/ DebugData))
		{
			return Instance->CurrentLifeTimerScrubPosition;
		}
	}

	return 0.f;
}

void SAnimationScrubPanel::ReplaceLockedSequence(class UAnimSequenceBase* NewLockedSequence)
{
	LockedSequence = NewLockedSequence;
}

UAnimInstance* SAnimationScrubPanel::GetAnimInstanceWithBlueprint() const
{
	if (UDebugSkelMeshComponent* DebugComponent = PersonaPtr.Pin()->GetPreviewMeshComponent())
	{
		UAnimInstance* Instance = DebugComponent->GetAnimInstance();

		if ((Instance != NULL) && (Instance->GetClass()->ClassGeneratedBy != NULL))
		{
			return Instance;
		}
	}

	return NULL;
}

bool SAnimationScrubPanel::GetAnimBlueprintDebugData(UAnimInstance*& Instance, FAnimBlueprintDebugData*& DebugInfo) const
{
	Instance = GetAnimInstanceWithBlueprint();

	if (Instance != NULL)
	{
		// Avoid updating the instance if we're replaying the past
		if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(Instance->GetClass()))
		{
			if (UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(AnimBlueprintClass->ClassGeneratedBy))
			{
				if (Blueprint->GetObjectBeingDebugged() == Instance)
				{
					DebugInfo = &(AnimBlueprintClass->GetAnimBlueprintDebugData());
					return true;
				}
			}
		}
	}

	return false;
}

void SAnimationScrubPanel::OnCropAnimSequence( bool bFromStart, float CurrentTime )
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if(PreviewInstance)
	{
		float Length = PreviewInstance->GetLength();
		if (PreviewInstance->GetCurrentAsset())
		{
			UAnimSequence* AnimSequence = Cast<UAnimSequence>( PreviewInstance->GetCurrentAsset() );
			if( AnimSequence )
			{
				const FScopedTransaction Transaction( LOCTEXT("CropAnimSequence", "Crop Animation Sequence") );

				//Call modify to restore slider position
				PreviewInstance->Modify();

				//Call modify to restore anim sequence current state
				AnimSequence->Modify();

				// Crop the raw anim data.
				AnimSequence->CropRawAnimData( CurrentTime, bFromStart );

				//Resetting slider position to the first frame
				PreviewInstance->SetPosition( 0.0f, false );

				OnSetInputViewRange.ExecuteIfBound(0, AnimSequence->SequenceLength);
			}
		}
	}
}

void SAnimationScrubPanel::OnAppendAnimSequence( bool bFromStart, int32 NumOfFrames )
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if(PreviewInstance && PreviewInstance->GetCurrentAsset())
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(PreviewInstance->GetCurrentAsset());
		if(AnimSequence)
		{
			const FScopedTransaction Transaction(LOCTEXT("InsertAnimSequence", "Insert Animation Sequence"));

			//Call modify to restore slider position
			PreviewInstance->Modify();

			//Call modify to restore anim sequence current state
			AnimSequence->Modify();

			// Crop the raw anim data.
			int32 StartFrame = (bFromStart)? 0 : AnimSequence->NumFrames - 1;
			int32 EndFrame = StartFrame + NumOfFrames;
			int32 CopyFrame = StartFrame;
			AnimSequence->InsertFramesToRawAnimData(StartFrame, EndFrame, CopyFrame);

			OnSetInputViewRange.ExecuteIfBound(0, AnimSequence->SequenceLength);
		}
	}
}

void SAnimationScrubPanel::OnInsertAnimSequence( bool bBefore, int32 CurrentFrame )
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if(PreviewInstance && PreviewInstance->GetCurrentAsset())
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(PreviewInstance->GetCurrentAsset());
		if(AnimSequence)
		{
			const FScopedTransaction Transaction(LOCTEXT("InsertAnimSequence", "Insert Animation Sequence"));

			//Call modify to restore slider position
			PreviewInstance->Modify();

			//Call modify to restore anim sequence current state
			AnimSequence->Modify();

			// Crop the raw anim data.
			int32 StartFrame = (bBefore)? CurrentFrame : CurrentFrame + 1;
			int32 EndFrame = StartFrame + 1;
			AnimSequence->InsertFramesToRawAnimData(StartFrame, EndFrame, CurrentFrame);

			OnSetInputViewRange.ExecuteIfBound(0, AnimSequence->SequenceLength);
		}
	}
}

void SAnimationScrubPanel::OnReZeroAnimSequence( )
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if(PreviewInstance)
	{
		UDebugSkelMeshComponent* PreviewSkelComp = PersonaPtr.Pin()->GetPreviewMeshComponent();

		if (PreviewInstance->GetCurrentAsset() && PreviewSkelComp )
		{
			UAnimSequence* AnimSequence = Cast<UAnimSequence>( PreviewInstance->GetCurrentAsset() );
			if( AnimSequence )
			{
				const FScopedTransaction Transaction( LOCTEXT("ReZeroAnimation", "ReZero Animation Sequence") );

				//Call modify to restore anim sequence current state
				AnimSequence->Modify();

				// Find vector that would translate current root bone location onto origin.
				FVector ApplyTranslation = -1.f * PreviewSkelComp->GetComponentSpaceTransforms()[0].GetLocation();

				// Convert into world space and eliminate 'z' translation. Don't want to move character into ground.
				FVector WorldApplyTranslation = PreviewSkelComp->ComponentToWorld.TransformVector(ApplyTranslation);
				WorldApplyTranslation.Z = 0.f;
				ApplyTranslation = PreviewSkelComp->ComponentToWorld.InverseTransformVector(WorldApplyTranslation);

				// As above, animations don't have any idea of hierarchy, so we don't know for sure if track 0 is the root bone's track.
				FRawAnimSequenceTrack& RawTrack = AnimSequence->RawAnimationData[0];
				for(int32 i=0; i<RawTrack.PosKeys.Num(); i++)
				{
					RawTrack.PosKeys[i] += ApplyTranslation;
				}

				// Handle Raw Data changing
				AnimSequence->MarkRawDataAsModified();
				AnimSequence->OnRawDataChanged();

				AnimSequence->MarkPackageDirty();
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
