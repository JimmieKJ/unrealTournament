// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "Sequencer.h"
#include "SequencerEdMode.h"
#include "SequencerDetailKeyframeHandler.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "MovieSceneFolder.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/SequencerWidgets/Public/ITimeSlider.h"
#include "Editor/EditorWidgets/Public/ITransportControl.h"
#include "Editor/EditorWidgets/Public/EditorWidgetsModule.h"
#include "Editor/LevelEditor/Public/ILevelEditor.h"
#include "Editor/LevelEditor/Public/ILevelViewport.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "EditorSupportDelegates.h"
#include "SSequencer.h"
#include "SSequencerTreeView.h"
#include "ScopedTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintEditorModule.h"
#include "MovieSceneTrack.h"
#include "MovieSceneCameraCutTrack.h"
#include "MovieSceneAudioTrack.h"
#include "MovieSceneSkeletalAnimationTrack.h"
#include "MovieSceneTrackEditor.h"
#include "MovieSceneToolHelpers.h"
#include "ScopedTransaction.h"
#include "MovieSceneBoolSection.h"
#include "MovieSceneCameraCutSection.h"
#include "MovieScene3DTransformSection.h"
#include "MovieSceneSubSection.h"
#include "MovieSceneSubTrack.h"
#include "ISequencerSection.h"
#include "MovieSceneSequenceInstance.h"
#include "IKeyArea.h"
#include "IDetailsView.h"
#include "SnappingUtils.h"
#include "GenericCommands.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Selection.h"
#include "Tracks/MovieSceneSpawnTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "LevelEditor.h"
#include "IMenu.h"
#include "NotificationManager.h"
#include "SNotificationList.h"
#include "MovieSceneClipboard.h"
#include "SequencerClipboardReconciler.h"
#include "STextEntryPopup.h"
#include "SequencerHotspots.h"
#include "MovieSceneCaptureDialogModule.h"
#include "AutomatedLevelSequenceCapture.h"
#include "MovieSceneCommonHelpers.h"
#include "SceneOutlinerModule.h"
#include "SceneOutlinerPublicTypes.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "PackageTools.h"
#include "GroupedKeyArea.h"
#include "VirtualTrackArea.h"
#include "SequencerUtilities.h"
#include "MovieSceneCinematicShotTrack.h"
#include "ISequenceRecorder.h"

#define LOCTEXT_NAMESPACE "Sequencer"

DEFINE_LOG_CATEGORY(LogSequencer);


namespace
{
	TWeakPtr<FSequencer> ActiveSequencerPtr;
}


void FSequencer::InitSequencer(const FSequencerInitParams& InitParams, const TSharedRef<ISequencerObjectChangeListener>& InObjectChangeListener, const TSharedRef<IDetailKeyframeHandler>& InDetailKeyframeHandler, const TArray<FOnCreateTrackEditor>& TrackEditorDelegates)
{
	bIsEditingWithinLevelEditor = InitParams.bEditWithinLevelEditor;
	SilentModeCount = 0;

	if (InitParams.SpawnRegister.IsValid())
	{
		SpawnRegister = InitParams.SpawnRegister;
	}
	else
	{
		// Spawnables not supported
		SpawnRegister = MakeShareable(new FNullMovieSceneSpawnRegister);
	}

	// If this sequencer edits the level, close out the existing active sequencer and mark this as the active sequencer.
	if (bIsEditingWithinLevelEditor)
	{
		if (ActiveSequencerPtr.IsValid())
		{
			ActiveSequencerPtr.Pin()->Close();
		}

		ActiveSequencerPtr = SharedThis(this);

		// Register for saving the level so that the state of the scene can be restored before saving and updated after saving.
		FEditorDelegates::PreSaveWorld.AddSP(this, &FSequencer::OnPreSaveWorld);
		FEditorDelegates::PostSaveWorld.AddSP(this, &FSequencer::OnPostSaveWorld);
		FEditorDelegates::PreBeginPIE.AddSP(this, &FSequencer::OnPreBeginPIE);
		FEditorDelegates::EndPIE.AddSP(this, &FSequencer::OnEndPIE);

		FEditorDelegates::NewCurrentLevel.AddSP(this, &FSequencer::OnNewCurrentLevel);
		FEditorDelegates::OnMapOpened.AddSP(this, &FSequencer::OnMapOpened);

		USelection::SelectionChangedEvent.AddSP( this, &FSequencer::OnActorSelectionChanged );
	}

	Settings = USequencerSettingsContainer::GetOrCreate<USequencerSettings>(*InitParams.ViewParams.UniqueName);

	ToolkitHost = InitParams.ToolkitHost;

	ScrubPosition = InitParams.ViewParams.InitialScrubPosition;
	ObjectChangeListener = InObjectChangeListener;
	DetailKeyframeHandler = InDetailKeyframeHandler;

	check( ObjectChangeListener.IsValid() );
		
	UMovieSceneSequence& RootSequence = *InitParams.RootSequence;
		
	// Focusing the initial movie scene needs to be done before the first time GetFocusedMovieSceneSequenceInstance or GetRootMovieSceneInstance is used
	RootMovieSceneSequenceInstance = MakeShareable(new FMovieSceneSequenceInstance(RootSequence));
	SequenceInstanceStack.Add(RootMovieSceneSequenceInstance.ToSharedRef());

	// Make internal widgets
	SequencerWidget = SNew( SSequencer, SharedThis( this ) )
		.ViewRange( this, &FSequencer::GetViewRange )
		.ClampRange( this, &FSequencer::GetClampRange )
		.InOutRange( this, &FSequencer::GetInOutRange )
		.PlaybackRange( this, &FSequencer::GetPlaybackRange )
		.PlaybackStatus( this, &FSequencer::GetPlaybackStatus )
		.OnInOutRangeChanged( this, &FSequencer::SetInOutRange )
		.OnBeginInOutRangeDrag( this, &FSequencer::OnBeginInOutRangeDrag )
		.OnEndInOutRangeDrag( this, &FSequencer::OnEndInOutRangeDrag )
		.OnPlaybackRangeChanged( this, &FSequencer::SetPlaybackRange )
		.OnBeginPlaybackRangeDrag( this, &FSequencer::OnBeginPlaybackRangeDrag )
		.OnEndPlaybackRangeDrag( this, &FSequencer::OnEndPlaybackRangeDrag )
		.ScrubPosition( this, &FSequencer::OnGetScrubPosition )
		.OnBeginScrubbing( this, &FSequencer::OnBeginScrubbing )
		.OnEndScrubbing( this, &FSequencer::OnEndScrubbing )
		.OnScrubPositionChanged( this, &FSequencer::OnScrubPositionChanged )
		.OnViewRangeChanged( this, &FSequencer::SetViewRange )
		.OnClampRangeChanged( this, &FSequencer::OnClampRangeChanged )
		.OnGetAddMenuContent(InitParams.ViewParams.OnGetAddMenuContent)
		.AddMenuExtender(InitParams.ViewParams.AddMenuExtender);

	// When undo occurs, get a notification so we can make sure our view is up to date
	GEditor->RegisterForUndo(this);

	if( bIsEditingWithinLevelEditor )
	{
		// @todo remove when world-centric mode is added
		// Hook into the editor's mechanism for checking whether we need live capture of PIE/SIE actor state
		GEditor->GetActorRecordingState().AddSP(this, &FSequencer::GetActorRecordingState);

		ActivateDetailKeyframeHandler();
				
		FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		EditModule.OnPropertyEditorOpened().AddSP(this, &FSequencer::OnPropertyEditorOpened);
	}

	// Create tools and bind them to this sequencer
	for( int32 DelegateIndex = 0; DelegateIndex < TrackEditorDelegates.Num(); ++DelegateIndex )
	{
		check( TrackEditorDelegates[DelegateIndex].IsBound() );
		// Tools may exist in other modules, call a delegate that will create one for us 
		TSharedRef<ISequencerTrackEditor> TrackEditor = TrackEditorDelegates[DelegateIndex].Execute( SharedThis( this ) );
		TrackEditors.Add( TrackEditor );
	}

	ZoomAnimation = FCurveSequence();
	ZoomCurve = ZoomAnimation.AddCurve(0.f, 0.2f, ECurveEaseFunction::QuadIn);
	OverlayAnimation = FCurveSequence();
	OverlayCurve = OverlayAnimation.AddCurve(0.f, 0.2f, ECurveEaseFunction::QuadIn);

	// Update initial movie scene data
	NotifyMovieSceneDataChanged();
	UpdateTimeBoundsToFocusedMovieScene();

	// NOTE: Could fill in asset editor commands here!

	BindSequencerCommands();
	ActivateSequencerEditorMode();

	if( bIsEditingWithinLevelEditor )
	{
		AttachTransportControlsToViewports();
	}

	for (auto TrackEditor : TrackEditors)
	{
		TrackEditor->OnInitialize();
	}
}


FSequencer::FSequencer()
	: SequencerCommandBindings( new FUICommandList )
	, SequencerSharedBindings( new FUICommandList )
	, TargetViewRange(0.f, 5.f)
	, LastViewRange(0.f, 5.f)
	, ViewRangeBeforeZoom(TRange<float>::Empty())
	, PlaybackState( EMovieScenePlayerStatus::Stopped )
	, ScrubPosition( 0.0f )
	, bPerspectiveViewportPossessionEnabled( true )
	, bPerspectiveViewportCameraCutEnabled( false )
	, bIsEditingWithinLevelEditor( false )
	, bNeedTreeRefresh( false )
	, bWasClosed( false )
	, NodeTree( MakeShareable( new FSequencerNodeTree( *this ) ) )
	, bUpdatingSequencerSelection( false )
	, bUpdatingExternalSelection( false )
	, OldMaxTickRate(GEngine->GetMaxFPS())
{
	Selection.GetOnOutlinerNodeSelectionChanged().AddRaw(this, &FSequencer::OnSelectedOutlinerNodesChanged);
}


FSequencer::~FSequencer()
{
	GEditor->GetActorRecordingState().RemoveAll( this );
	GEditor->UnregisterForUndo( this );

	if( bIsEditingWithinLevelEditor )
	{
		DetachTransportControlsFromViewports();
	}

	// When a new level sequence asset is opened while an existing one is open, the original level sequence asset is closed, 
	// the new level sequence asset is initialized, and then the original level sequence asset is destroyed. In that case, 
	// the track editors should not be released a second time. However, a level sequence asset is opened and closed without 
	// opening another, the track editors still need to be released because Close() wil not be called.
	//
	if (!bWasClosed)
	{
		for (auto TrackEditor : TrackEditors)
		{
			TrackEditor->OnRelease();
		}
		TrackEditors.Empty();
	}
	SequencerWidget.Reset();
}


void FSequencer::Close()
{
	if (ActiveSequencerPtr.IsValid() && ActiveSequencerPtr.Pin().Get() == this)
	{
		RootMovieSceneSequenceInstance->RestoreState(*this);

		FEditorDelegates::PreSaveWorld.RemoveAll(this);
		FEditorDelegates::PostSaveWorld.RemoveAll(this);
		FEditorDelegates::PreBeginPIE.RemoveAll(this);
		FEditorDelegates::EndPIE.RemoveAll(this);

		FEditorDelegates::NewCurrentLevel.RemoveAll(this);
		FEditorDelegates::OnMapOpened.RemoveAll(this);
		
		for (auto TrackEditor : TrackEditors)
		{
			TrackEditor->OnRelease();
		}

		ActiveSequencerPtr.Reset();

		if (GLevelEditorModeTools().IsModeActive(FSequencerEdMode::EM_SequencerMode))
		{
			GLevelEditorModeTools().DeactivateMode(FSequencerEdMode::EM_SequencerMode);
		}
	}

	if (bIsEditingWithinLevelEditor)
	{
		DeactivateDetailKeyframeHandler();
			
		FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");			
		EditModule.OnPropertyEditorOpened().RemoveAll(this);

		DetachTransportControlsFromViewports();
	}
	
	SequencerWidget.Reset();
	bWasClosed = true;
}


void FSequencer::Tick(float InDeltaTime)
{
	Selection.Tick();

	if (bNeedTreeRefresh)
	{
		// @todo - Sequencer Will be called too often
		UpdateRuntimeInstances();

		SequencerWidget->UpdateLayoutTree();
		bNeedTreeRefresh = false;
	}

	static const float AutoScrollFactor = 0.1f;

	// Animate the autoscroll offset if it's set
	if (AutoscrollOffset.IsSet())
	{
		float Offset = AutoscrollOffset.GetValue() * AutoScrollFactor;
		SetViewRange(TRange<float>(TargetViewRange.GetLowerBoundValue() + Offset, TargetViewRange.GetUpperBoundValue() + Offset), EViewRangeInterpolation::Immediate);
	}

	// Animate the autoscrub offset if it's set
	if (AutoscrubOffset.IsSet())
	{
		float Offset = AutoscrubOffset.GetValue() * AutoScrollFactor;
		SetGlobalTimeDirectly(GetGlobalTime() + Offset);
	}

	// override max frame rate
	if (PlaybackState == EMovieScenePlayerStatus::Playing)
	{
		const float TimeSnapInterval = Settings->GetTimeSnapInterval();

		if (SequencerSnapValues::IsTimeSnapIntervalFrameRate(TimeSnapInterval) && Settings->GetFixedTimeStepPlayback())
		{
			GEngine->SetMaxFPS(1.f / TimeSnapInterval);
		}
		else
		{
			GEngine->SetMaxFPS(OldMaxTickRate);
		}
	}

	// calculate new global time
	float NewTime = GetGlobalTime() + InDeltaTime * GWorld->GetWorldSettings()->MatineeTimeDilation;

	if (PlaybackState == EMovieScenePlayerStatus::Playing ||
		PlaybackState == EMovieScenePlayerStatus::Recording)
	{
		SetGlobalTimeLooped(NewTime);
	}

	// Tick all the tools we own as well
	for (int32 EditorIndex = 0; EditorIndex < TrackEditors.Num(); ++EditorIndex)
	{
		TrackEditors[EditorIndex]->Tick(InDeltaTime);
	}

	if (!IsInSilentMode())
	{
		PostTickRenderStateFixup();
	}

	ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
	if(SequenceRecorder.IsRecording())
	{
		UMovieSceneSubSection* Section = UMovieSceneSubSection::GetRecordingSection();
		if(Section != nullptr)
		{
			Section->SetEndTime(Section->GetStartTime() + SequenceRecorder.GetCurrentRecordingLength());
		}
	}
}

void FSequencer::PostTickRenderStateFixup()
{
	if (PlaybackState == EMovieScenePlayerStatus::Jumping || PlaybackState == EMovieScenePlayerStatus::Stepping)
	{
		const bool bApplyFixup = true;

		if (!bApplyFixup)
		{
			// Clear any delayed state changes regardless
			SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
		}
		else
		{
			const float Time = GetGlobalTime();
			const float FixedTickDelta = 1.f / 60.f;
			const float StepDelta = (PlaybackState == EMovieScenePlayerStatus::Jumping) ? 0.f : FixedTickDelta;
			
			// Find a time for our previous frame, if there is one
			float TimeLowerBound = GetPlaybackRange().GetLowerBoundValue();

			if (UMovieSceneSequence* MovieSequence = GetFocusedMovieSceneSequence())
			{
				UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(MovieSequence->GetMovieScene()->FindMasterTrack(UMovieSceneCinematicShotTrack::StaticClass()));
				if (!SubTrack)
				{
					SubTrack = Cast<UMovieSceneSubTrack>(MovieSequence->GetMovieScene()->FindMasterTrack(UMovieSceneSubTrack::StaticClass()));
				}

				if (SubTrack)
				{
					for (UMovieSceneSection* Section : SubTrack->GetAllSections())
					{
						if (Section->IsInfinite() || Section->IsTimeWithinSection(Time))
						{
							UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>(Section);

							TimeLowerBound = FMath::Max(TimeLowerBound, SubSection->GetStartTime());
							break;
						}
					}
				}
			}

			float PreviousTime = FMath::Max(TimeLowerBound, Time - StepDelta);
			PreviousTime = FMath::Min(PreviousTime, Time);

			// Force previous frame in sequence to re-render
			SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
			SetGlobalTimeDirectly(PreviousTime);
			Tick(StepDelta);

			GWorld->SendAllEndOfFrameUpdates();

			SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
			SetGlobalTimeDirectly(Time);
			Tick(StepDelta);
		}
	}
}


UMovieSceneSequence* FSequencer::GetRootMovieSceneSequence() const
{
	if (SequenceInstanceStack.Num())
	{
		return SequenceInstanceStack[0]->GetSequence();
	}

	return nullptr;
}


UMovieSceneSequence* FSequencer::GetFocusedMovieSceneSequence() const
{
	// the last item is the focused movie scene
	if (SequenceInstanceStack.Num())
	{
		return SequenceInstanceStack.Top()->GetSequence();
	}

	return nullptr;
}


void FSequencer::ResetToNewRootSequence(UMovieSceneSequence& NewSequence)
{
	if (RootMovieSceneSequenceInstance.IsValid())
	{
		RootMovieSceneSequenceInstance->RestoreState(*this);
	}

	//@todo Sequencer - Encapsulate this better
	SequenceInstanceStack.Empty();
	Selection.Empty();
	SequenceInstanceBySection.Empty();

	// Focusing the initial movie scene needs to be done before the first time NewSequence or GetRootMovieSceneInstance is used
	RootMovieSceneSequenceInstance = MakeShareable(new FMovieSceneSequenceInstance(NewSequence));
	SequenceInstanceStack.Add(RootMovieSceneSequenceInstance.ToSharedRef());

	SequencerWidget->ResetBreadcrumbs();

	NotifyMovieSceneDataChanged();
	UpdateTimeBoundsToFocusedMovieScene();
}


TSharedRef<FMovieSceneSequenceInstance> FSequencer::GetRootMovieSceneSequenceInstance() const
{
	return SequenceInstanceStack[0];
}


TSharedRef<FMovieSceneSequenceInstance> FSequencer::GetFocusedMovieSceneSequenceInstance() const
{
	// the last item is the focused movie scene
	return SequenceInstanceStack.Top();
}


void FSequencer::FocusSequenceInstance(UMovieSceneSubSection& InSubSection)
{
	TSharedRef<FMovieSceneSequenceInstance> SequenceInstance = GetSequenceInstanceForSection(InSubSection);

	// Check for infinite recursion
	check(SequenceInstance != SequenceInstanceStack.Top());

	// Focus the movie scene
	SequenceInstanceStack.Push(SequenceInstance);

	// Find the time in the focused sequence instance that is relative to the current time
	TRange<float> PlaybackRange = TRange<float>(0, 0);
	float PlaybackRangeStart = 0.f;
	if (InSubSection.GetSequence() != nullptr)
	{
		UMovieScene* ShotMovieScene = InSubSection.GetSequence()->GetMovieScene();
		PlaybackRange = ShotMovieScene->GetPlaybackRange();
	}

	const float ShotOffset = InSubSection.StartOffset + PlaybackRange.GetLowerBoundValue() - InSubSection.PrerollTime;
	float AbsoluteShotPosition = ShotOffset + (GetGlobalTime() - (InSubSection.GetStartTime() - InSubSection.PrerollTime)) / InSubSection.TimeScale;
	if (!PlaybackRange.Contains(AbsoluteShotPosition))
	{
		AbsoluteShotPosition = PlaybackRange.GetLowerBoundValue();
	}

	// Reset data that is only used for the previous movie scene
	ResetPerMovieSceneData();

	// Update internal data for the new movie scene
	NotifyMovieSceneDataChanged();
	UpdateTimeBoundsToFocusedMovieScene();

	SequencerWidget->UpdateBreadcrumbs();

	SetGlobalTime(AbsoluteShotPosition, ESnapTimeMode::STM_Interval);
}


FGuid FSequencer::CreateBinding(UObject& InObject, const FString& InName)
{
	UMovieSceneSequence* OwnerSequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();
		
	const FGuid PossessableGuid = OwnerMovieScene->AddPossessable(InName, InObject.GetClass());

	UObject* BindingContext = OwnerSequence->GetParentObject(&InObject);
	if (BindingContext)
	{
		// Ensure we have possessed the outer object, if necessary
		FGuid ParentGuid = GetHandleToObject(BindingContext);
		
		// Set up parent/child guids for possessables within spawnables
		if (ParentGuid.IsValid())
		{
			FMovieScenePossessable* ChildPossessable = OwnerMovieScene->FindPossessable(PossessableGuid);
			if (ensure(ChildPossessable))
			{
				ChildPossessable->SetParent(ParentGuid);
			}

			FMovieSceneSpawnable* ParentSpawnable = OwnerMovieScene->FindSpawnable(ParentGuid);
			if (ParentSpawnable)
			{
				ParentSpawnable->AddChildPossessable(PossessableGuid);
			}
		}
	}
	else
	{
		BindingContext = GetPlaybackContext();
	}

	OwnerSequence->BindPossessableObject(PossessableGuid, InObject, BindingContext);

	return PossessableGuid;
}


UObject* FSequencer::GetPlaybackContext() const
{
	if (bIsEditingWithinLevelEditor)
	{
		if(GEditor && GEditor->PlayWorld)
		{
			return GEditor->PlayWorld;
		}
		else if(GEditor && GEditor->EditorWorld)
		{
			return GEditor->EditorWorld;
		}
		else
		{
			return GWorld;	
		}
	}

	return nullptr;
}


void FSequencer::GetAllKeyedProperties(UObject& Object, TSet<UProperty*>& OutProperties)
{
	GetAllKeyedPropertiesForInstance(Object, *GetRootMovieSceneSequenceInstance(), OutProperties);
}


void FSequencer::GetAllKeyedPropertiesForInstance(UObject& Object, FMovieSceneSequenceInstance& Instance, TSet<UProperty*>& OutProperties)
{
	GetKeyedProperties(Object, Instance, OutProperties);

	UMovieSceneSubTrack* SubTrack = Instance.GetSequence()->GetMovieScene()->FindMasterTrack<UMovieSceneSubTrack>();
	if (SubTrack)
	{
		// Iterate sub movie scenes
		for (UMovieSceneSection* SubSection : SubTrack->GetAllSections())
		{
			// Find the instance for the section
			TSharedRef<FMovieSceneSequenceInstance>* SubInstance = SequenceInstanceBySection.Find(SubSection);
			GetAllKeyedPropertiesForInstance(Object, SubInstance->Get(), OutProperties);
		}
	}
}


void FSequencer::GetKeyedProperties(UObject& Object, FMovieSceneSequenceInstance& Instance, TSet<UProperty*>& OutProperties)
{
	FGuid BindingId = Instance.FindObjectId(Object);

	const FMovieSceneBinding* Binding = BindingId.IsValid() ? Instance.GetSequence()->GetMovieScene()->GetBindings().FindByPredicate([&](const FMovieSceneBinding& InBinding){
		return BindingId == InBinding.GetObjectGuid();
	}) : nullptr;

	if (!Binding)
	{
		return;
	}

	// Add any properties for this object
	for (UMovieSceneTrack* Track : Binding->GetTracks())
	{
		if (UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(Track))
		{
			// Add transform properties manually. This is technically not correct, since it's the scene component that's being keyed, not the actor,
			// But since the transform track is always stored on an actor binding, we have to add them this way
			AActor* Actor = Cast<AActor>(&Object);
			if (Actor)
			{
				UClass* Class = USceneComponent::StaticClass();
				OutProperties.Add(Class->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeLocation)));
				OutProperties.Add(Class->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeRotation)));
				OutProperties.Add(Class->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeScale3D)));
			}
		}
		else if (UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(Track))
		{
			FTrackInstancePropertyBindings InstanceBinding(PropertyTrack->GetPropertyName(), PropertyTrack->GetPropertyPath());
			UProperty* Property = InstanceBinding.GetProperty(&Object);
			if (Property)
			{
				OutProperties.Add(Property);
			}
		}
	}
}

void FSequencer::GetKeysFromSelection(TUniquePtr<ISequencerKeyCollection>& KeyCollection)
{
	if (!KeyCollection.IsValid())
	{
		KeyCollection.Reset(new FGroupedKeyCollection);
	}

	TArray<FSequencerDisplayNode*> SelectedNodes;
	for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
	{
		SelectedNodes.Add(&Node.Get());
	}

	// Anything within .5 pixel's worth of time is a duplicate as far as we're concerened
	FVirtualTrackArea TrackArea = SequencerWidget->GetVirtualTrackArea();;
	const float DuplicateThreshold = (TrackArea.PixelToTime(0.f) - TrackArea.PixelToTime(1.f)) * .5f;

	KeyCollection->InitializeRecursive(SelectedNodes, DuplicateThreshold);
}

TSharedRef<FMovieSceneSequenceInstance> FSequencer::GetSequenceInstanceForSection(UMovieSceneSection& Section) const
{
	return SequenceInstanceBySection.FindChecked(&Section);
}


bool FSequencer::HasSequenceInstanceForSection(UMovieSceneSection& Section) const
{
	return SequenceInstanceBySection.Contains(&Section);
}


void FSequencer::PopToSequenceInstance(TSharedRef<FMovieSceneSequenceInstance> SequenceInstance)
{
	if( SequenceInstanceStack.Num() > 1 )
	{
		// Pop until we find the movie scene to focus
		while( SequenceInstance != SequenceInstanceStack.Last() )
		{
			SequenceInstanceStack.Pop();
		}
	
		check( SequenceInstanceStack.Num() > 0 );

		ResetPerMovieSceneData();
		NotifyMovieSceneDataChanged();
		UpdateTimeBoundsToFocusedMovieScene();
	}
}


void FSequencer::DeleteSections(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections)
{
	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	bool bAnythingRemoved = false;

	FScopedTransaction DeleteSectionTransaction( NSLOCTEXT("Sequencer", "DeleteSection_Transaction", "Delete Section") );

	for (const auto Section : Sections)
	{
		if (!Section.IsValid() || Section->IsLocked())
		{
			continue;
		}

		// if this check fails then the section is outered to a type that doesnt know about the section
		UMovieSceneTrack* Track = CastChecked<UMovieSceneTrack>(Section->GetOuter());
		{
			Track->SetFlags(RF_Transactional);
			Track->Modify();
			Track->RemoveSection(*Section);
		}

		bAnythingRemoved = true;
		Selection.RemoveFromSelection(Section.Get());
	}

	if (bAnythingRemoved)
	{
		// Full refresh required just in case the last section was removed from any track.
		NotifyMovieSceneDataChanged();
	}

	SequencerHelpers::ValidateNodesWithSelectedKeysOrSections(*this);
}


void FSequencer::DeleteSelectedKeys()
{
	FScopedTransaction DeleteKeysTransaction( NSLOCTEXT("Sequencer", "DeleteSelectedKeys_Transaction", "Delete Selected Keys") );
	bool bAnythingRemoved = false;
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();

	for (const FSequencerSelectedKey& Key : SelectedKeysArray)
	{
		if (Key.IsValid())
		{
			if (Key.Section->TryModify())
			{
				Key.KeyArea->DeleteKey(Key.KeyHandle.GetValue());
				bAnythingRemoved = true;
			}
		}
	}

	if (bAnythingRemoved)
	{
		UpdateRuntimeInstances();
	}

	Selection.EmptySelectedKeys();
	SequencerHelpers::ValidateNodesWithSelectedKeysOrSections(*this);
}


void FSequencer::SetInterpTangentMode(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode)
{
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();
	if (SelectedKeysArray.Num() == 0)
	{
		return;
	}

	FScopedTransaction SetInterpTangentModeTransaction(NSLOCTEXT("Sequencer", "SetInterpTangentMode_Transaction", "Set Interpolation and Tangent Mode"));
	bool bAnythingChanged = false;

	for (const FSequencerSelectedKey& Key : SelectedKeysArray)
	{
		if (Key.IsValid())
		{
			if (Key.Section->TryModify())
			{
				Key.KeyArea->SetKeyInterpMode(Key.KeyHandle.GetValue(), InterpMode);
				Key.KeyArea->SetKeyTangentMode(Key.KeyHandle.GetValue(), TangentMode);
				bAnythingChanged = true;
			}
		}
	}

	if (bAnythingChanged)
	{
		UpdateRuntimeInstances();
	}
}


bool FSequencer::IsInterpTangentModeSelected(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode) const
{
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();

	bool bAllSelected = false;
	for (const FSequencerSelectedKey& Key : SelectedKeysArray)
	{
		if (Key.IsValid())
		{
			bAllSelected = true;
			if (Key.KeyArea->GetKeyInterpMode(Key.KeyHandle.GetValue()) != InterpMode || 
				Key.KeyArea->GetKeyTangentMode(Key.KeyHandle.GetValue()) != TangentMode)
			{
				bAllSelected = false;
				break;
			}
		}
	}
	return bAllSelected;
}


void FSequencer::SnapToFrame()
{
	FScopedTransaction SnapToFrameTransaction(NSLOCTEXT("Sequencer", "SnapToFrame_Transaction", "Snap Selected Keys to Frame"));
	bool bAnythingChanged = false;
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();

	for (const FSequencerSelectedKey& Key : SelectedKeysArray)
	{
		if (Key.IsValid())
		{
			if (Key.Section->TryModify())
			{
				float NewKeyTime = Key.KeyArea->GetKeyTime(Key.KeyHandle.GetValue());

				// Convert to frame
				float FrameRate = 1.0f / Settings->GetTimeSnapInterval();
				int32 NewFrame = SequencerHelpers::TimeToFrame(NewKeyTime, FrameRate);

				// Convert back to time
				NewKeyTime = SequencerHelpers::FrameToTime(NewFrame, FrameRate);

				Key.KeyArea->SetKeyTime(Key.KeyHandle.GetValue(), NewKeyTime);
				bAnythingChanged = true;
			}
		}
	}

	if (bAnythingChanged)
	{
		UpdateRuntimeInstances();
	}
}


bool FSequencer::CanSnapToFrame() const
{
	const bool bKeysSelected = Selection.GetSelectedKeys().Num() > 0;

	return bKeysSelected && CanShowFrameNumbers();
}


void FSequencer::NotifyMapChanged( class UWorld* NewWorld, EMapChangeType MapChangeType )
{
	// @todo sequencer: We should only wipe/respawn puppets that are affected by the world that is being changed! (multi-UWorld support)
	if( ( MapChangeType == EMapChangeType::LoadMap || MapChangeType == EMapChangeType::NewMap || MapChangeType == EMapChangeType::TearDownWorld) )
	{
		SpawnRegister->CleanUp(*this);

		NotifyMovieSceneDataChanged();
	}
}


void FSequencer::OnActorsDropped( const TArray<TWeakObjectPtr<AActor> >& Actors )
{
	// @todo sequencer: Undo doesn't seem to be working at all
	const FScopedTransaction Transaction(LOCTEXT("UndoPossessingObject", "Possess Object in Sequencer"));
	GetFocusedMovieSceneSequence()->Modify();

	bool bPossessableAdded = false;
	for (TWeakObjectPtr<AActor> WeakActor : Actors)
	{
		if (AActor* Actor = WeakActor.Get())
		{
			FGuid ExistingGuid = SequenceInstanceStack.Top()->FindObjectId(*Actor);
			if (!ExistingGuid.IsValid())
			{
				FGuid PossessableGuid = CreateBinding(*Actor, Actor->GetActorLabel());

				UpdateRuntimeInstances();

				OnActorAddedToSequencerEvent.Broadcast(Actor, PossessableGuid);
			}
			bPossessableAdded = true;
		}
	}
	
	if (bPossessableAdded)
	{
		NotifyMovieSceneDataChanged();
	}
}


void FSequencer::NotifyMovieSceneDataChanged()
{
	LabelManager.SetMovieScene(SequenceInstanceStack.Top()->GetSequence()->GetMovieScene());
	SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
	bNeedTreeRefresh = true;

	UpdatePlaybackRange();
}


FAnimatedRange FSequencer::GetViewRange() const
{
	FAnimatedRange AnimatedRange(FMath::Lerp(LastViewRange.GetLowerBoundValue(), TargetViewRange.GetLowerBoundValue(), ZoomCurve.GetLerp()),
		FMath::Lerp(LastViewRange.GetUpperBoundValue(), TargetViewRange.GetUpperBoundValue(), ZoomCurve.GetLerp()));

	if (ZoomAnimation.IsPlaying())
	{
		AnimatedRange.AnimationTarget = TargetViewRange;
	}

	return AnimatedRange;
}


FAnimatedRange FSequencer::GetClampRange() const
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	return FocusedMovieScene->GetEditorData().WorkingRange;
}


void FSequencer::SetClampRange(TRange<float> InNewClampRange)
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	FocusedMovieScene->GetEditorData().WorkingRange = InNewClampRange;
}


TRange<float> FSequencer::GetInOutRange() const
{
	return GetFocusedMovieSceneSequence()->GetMovieScene()->GetInOutRange();
}


void FSequencer::SetInOutRange(TRange<float> Range)
{
	const FScopedTransaction Transaction(LOCTEXT("SetPlaybackRange_Transaction", "Set In/Out Range"));
	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	FocusedMovieScene->SetInOutRange(Range);
}


void FSequencer::SetInOutRangeEnd()
{
	const float GlobalTime = GetGlobalTime();

	if (GetInOutRange().GetLowerBoundValue() >= GlobalTime)
	{
		SetInOutRange(TRange<float>(GlobalTime));
	}
	else
	{
		SetInOutRange(TRange<float>(GetInOutRange().GetLowerBoundValue(), GlobalTime));
	}
}


void FSequencer::SetInOutRangeStart()
{
	const float GlobalTime = GetGlobalTime();

	if (GetInOutRange().GetUpperBoundValue() <= GlobalTime)
	{
		SetInOutRange(TRange<float>(GlobalTime));
	}
	else
	{
		SetInOutRange(TRange<float>(GlobalTime, GetInOutRange().GetUpperBoundValue()));
	}
}


TRange<float> FSequencer::GetPlaybackRange() const
{
	return GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange();
}


void FSequencer::SetPlaybackRange(TRange<float> Range)
{
	if (ensure(Range.HasLowerBound() && Range.HasUpperBound() && !Range.IsDegenerate()))
	{
		const FScopedTransaction Transaction(LOCTEXT("SetPlaybackRange_Transaction", "Set Playback Range"));

		UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
		FocusedMovieScene->SetPlaybackRange(Range.GetLowerBoundValue(), Range.GetUpperBoundValue());
	}
}


void FSequencer::ResetViewRange()
{
	float InRange = GetPlaybackRange().GetLowerBoundValue();
	float OutRange = GetPlaybackRange().GetUpperBoundValue();
	const float OutputViewSize = OutRange - InRange;
	const float OutputChange = OutputViewSize * 0.1f;

	InRange -= OutputChange;
	OutRange += OutputChange;

	SetClampRange(TRange<float>(InRange, OutRange));	
	SetViewRange(TRange<float>(InRange, OutRange), EViewRangeInterpolation::Animated);
}


void FSequencer::ZoomViewRange(float InZoomDelta)
{
	float LocalViewRangeMax = TargetViewRange.GetUpperBoundValue();
	float LocalViewRangeMin = TargetViewRange.GetLowerBoundValue();
	const float OutputViewSize = LocalViewRangeMax - LocalViewRangeMin;
	const float OutputChange = OutputViewSize * InZoomDelta;

	float CurrentPositionFraction = (ScrubPosition - LocalViewRangeMin) / OutputViewSize;

	float NewViewOutputMin = LocalViewRangeMin - (OutputChange * CurrentPositionFraction);
	float NewViewOutputMax = LocalViewRangeMax + (OutputChange * (1.f - CurrentPositionFraction));

	if (NewViewOutputMin < NewViewOutputMax)
	{
		SetViewRange(TRange<float>(NewViewOutputMin, NewViewOutputMax), EViewRangeInterpolation::Animated);
	}
}


void FSequencer::ZoomInViewRange()
{
	ZoomViewRange(-0.1f);
}


void FSequencer::ZoomOutViewRange()
{
	ZoomViewRange(0.1f);
}


void FSequencer::UpdatePlaybackRange()
{
	if (Settings->ShouldKeepPlayRangeInSectionBounds())
	{
		UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
		TArray<UMovieSceneSection*> AllSections = FocusedMovieScene->GetAllSections();

		if (AllSections.Num() > 0)
		{
			TRange<float> NewBounds = AllSections[0]->GetRange();
	
			for (auto MovieSceneSection : AllSections)
			{
				NewBounds = TRange<float>( FMath::Min(NewBounds.GetLowerBoundValue(), MovieSceneSection->GetRange().GetLowerBoundValue()),
											FMath::Max(NewBounds.GetUpperBoundValue(), MovieSceneSection->GetRange().GetUpperBoundValue()) );
			}

			// When the playback range is determined by the section bounds, don't mark the change in the playback range otherwise the scene will be marked dirty
			const bool bAlwaysMarkDirty = false;
			FocusedMovieScene->SetPlaybackRange(NewBounds.GetLowerBoundValue(), NewBounds.GetUpperBoundValue(), bAlwaysMarkDirty);
		}
	}
	else
	{
		UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
		TRange<float> NewBounds = TRange<float>::Empty();

		for (auto MasterTrack : FocusedMovieScene->GetMasterTracks())
		{
			if (MasterTrack->AddsSectionBoundsToPlayRange())
			{
				NewBounds = TRange<float>( FMath::Min(NewBounds.GetLowerBoundValue(), MasterTrack->GetSectionBoundaries().GetLowerBoundValue()),
											FMath::Max(NewBounds.GetUpperBoundValue(), MasterTrack->GetSectionBoundaries().GetUpperBoundValue()) );
			}
		}
			
		if (!NewBounds.IsEmpty())
		{
			FocusedMovieScene->SetPlaybackRange(NewBounds.GetLowerBoundValue(), NewBounds.GetUpperBoundValue());
		}
	}
}


EAutoKeyMode FSequencer::GetAutoKeyMode() const 
{
	return Settings->GetAutoKeyMode();
}


void FSequencer::SetAutoKeyMode(EAutoKeyMode AutoKeyMode)
{
	Settings->SetAutoKeyMode(AutoKeyMode);
}


bool FSequencer::GetKeyAllEnabled() const 
{
	return Settings->GetKeyAllEnabled();
}


void FSequencer::SetKeyAllEnabled(bool bKeyAllEnabled) 
{
	Settings->SetKeyAllEnabled(bKeyAllEnabled);
}


bool FSequencer::GetKeyInterpPropertiesOnly() const 
{
	return Settings->GetKeyInterpPropertiesOnly();
}


void FSequencer::SetKeyInterpPropertiesOnly(bool bKeyInterpPropertiesOnly) 
{
	Settings->SetKeyInterpPropertiesOnly(bKeyInterpPropertiesOnly);
}


EMovieSceneKeyInterpolation FSequencer::GetKeyInterpolation() const
{
	return Settings->GetKeyInterpolation();
}


void FSequencer::SetKeyInterpolation(EMovieSceneKeyInterpolation InKeyInterpolation)
{
	Settings->SetKeyInterpolation(InKeyInterpolation);
}


bool FSequencer::GetInfiniteKeyAreas() const
{
	return Settings->GetInfiniteKeyAreas();
}


void FSequencer::SetInfiniteKeyAreas(bool bInfiniteKeyAreas)
{
	Settings->SetInfiniteKeyAreas(bInfiniteKeyAreas);
}


bool FSequencer::IsRecordingLive() const 
{
	return PlaybackState == EMovieScenePlayerStatus::Recording && GIsPlayInEditorWorld;
}


float FSequencer::GetCurrentLocalTime( UMovieSceneSequence& InMovieSceneSequence )
{
	//@todo Sequencer: Nested movie scenes:  Figure out the parent of the passed in movie scene and 
	// calculate local time
	return ScrubPosition;
}


float FSequencer::GetGlobalTime() const
{
	return ScrubPosition;
}


void FSequencer::SetGlobalTime( float NewTime, ESnapTimeMode SnapTimeMode )
{
	if (IsAutoScrollEnabled())
	{
		float RangeOffset = CalculateAutoscrollEncroachment(NewTime).Get(0.f);
			
		// When not scrubbing, we auto scroll the view range immediately
		if (RangeOffset != 0.f)
		{
			TRange<float> WorkingRange = GetClampRange();
			if (TargetViewRange.GetLowerBoundValue() + RangeOffset > WorkingRange.GetLowerBoundValue() &&
				TargetViewRange.GetUpperBoundValue() + RangeOffset < WorkingRange.GetUpperBoundValue())
			{
				SetViewRange(TRange<float>(TargetViewRange.GetLowerBoundValue() + RangeOffset, TargetViewRange.GetUpperBoundValue() + RangeOffset), EViewRangeInterpolation::Immediate);
			}
		}
	}

	SetGlobalTimeDirectly(NewTime, SnapTimeMode);
}


void FSequencer::SetGlobalTimeDirectly( float NewTime, ESnapTimeMode SnapTimeMode )
{
	float LastTime = ScrubPosition;

	if ((SnapTimeMode & ESnapTimeMode::STM_Interval) && Settings->GetIsSnapEnabled())
	{
		NewTime = Settings->SnapTimeToInterval(NewTime);
	}

	if ((SnapTimeMode & ESnapTimeMode::STM_Keys) && (Settings->GetSnapPlayTimeToKeys() || FSlateApplication::Get().GetModifierKeys().IsShiftDown()))
	{
		NewTime = FindNearestKey(NewTime);
	}

	// Update the position
	ScrubPosition = NewTime;

	EMovieSceneUpdateData UpdateData(NewTime, LastTime);
	SequenceInstanceStack.Top()->Update(UpdateData, *this);

	// Ensure any relevant spawned objects are cleaned up if we're playing back the master sequence
	if (SequenceInstanceStack.Num() == 1)
	{
		if (!GetPlaybackRange().Contains(NewTime))
		{
			SpawnRegister->OnMasterSequenceExpired(*this);
		}
	}

	// If realtime is off, this needs to be called to update the pivot location when scrubbing.
	GUnrealEd->UpdatePivotLocationForSelection();

	if (!IsInSilentMode())
	{
		// Redraw
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
		OnGlobalTimeChangedDelegate.Broadcast();
	}
}


void FSequencer::UpdateAutoScroll(float NewTime)
{
	float ThresholdPercentage = 0.025f;
	AutoscrollOffset = CalculateAutoscrollEncroachment(NewTime, ThresholdPercentage);

	if (!AutoscrollOffset.IsSet())
	{
		AutoscrubOffset.Reset();
		return;
	}

	TRange<float> ViewRange = GetViewRange();
	const float Threshold = (ViewRange.GetUpperBoundValue() - ViewRange.GetLowerBoundValue()) * ThresholdPercentage;

	// If we have no autoscrub offset yet, we move the scrub position to the boundary of the autoscroll threasdhold, then autoscrub from there
	if (!AutoscrubOffset.IsSet())
	{
		if (AutoscrollOffset.GetValue() < 0 && ScrubPosition > ViewRange.GetLowerBoundValue() + Threshold)
		{
			SetGlobalTimeDirectly( ViewRange.GetLowerBoundValue() + Threshold );
		}
		else if (AutoscrollOffset.GetValue() > 0 && ScrubPosition < ViewRange.GetUpperBoundValue() - Threshold)
		{
			SetGlobalTimeDirectly( ViewRange.GetUpperBoundValue() - Threshold );
		}
	}

	// Don't autoscrub if we're at the extremes of the movie scene range
	const TRange<float>& WorkingRange = GetFocusedMovieSceneSequence()->GetMovieScene()->GetEditorData().WorkingRange;
	if (NewTime < WorkingRange.GetLowerBoundValue() + Threshold ||
		NewTime > WorkingRange.GetUpperBoundValue() - Threshold
		)
	{
		AutoscrubOffset.Reset();
		return;
	}

	// Scrub at the same rate we scroll
	AutoscrubOffset = AutoscrollOffset;
}


TOptional<float> FSequencer::CalculateAutoscrollEncroachment(float NewTime, float ThresholdPercentage) const
{
	enum class EDirection { Positive, Negative };
	const EDirection Movement = NewTime - ScrubPosition >= 0 ? EDirection::Positive : EDirection::Negative;

	const TRange<float> CurrentRange = GetViewRange();
	const float RangeMin = CurrentRange.GetLowerBoundValue(), RangeMax = CurrentRange.GetUpperBoundValue();
	const float AutoScrollThreshold = (RangeMax - RangeMin) * ThresholdPercentage;

	if (Movement == EDirection::Negative && NewTime < RangeMin + AutoScrollThreshold)
	{
		// Scrolling backwards in time, and have hit the threshold
		return NewTime - (RangeMin + AutoScrollThreshold);
	}
	
	if (Movement == EDirection::Positive && NewTime > RangeMax - AutoScrollThreshold)
	{
		// Scrolling forwards in time, and have hit the threshold
		return NewTime - (RangeMax - AutoScrollThreshold);
	}

	return TOptional<float>();
}


void FSequencer::SetPerspectiveViewportPossessionEnabled(bool bEnabled)
{
	bPerspectiveViewportPossessionEnabled = bEnabled;
}


void FSequencer::SetPerspectiveViewportCameraCutEnabled(bool bEnabled)
{
	bPerspectiveViewportCameraCutEnabled = bEnabled;
}

ISequencer::FOnActorAddedToSequencer& FSequencer::OnActorAddedToSequencer()
{
	return OnActorAddedToSequencerEvent;
}

ISequencer::FOnCameraCut& FSequencer::OnCameraCut()
{
	return OnCameraCutEvent;
}

TSharedRef<INumericTypeInterface<float>> FSequencer::GetNumericTypeInterface()
{
	return SequencerWidget->GetNumericTypeInterface();
}

TSharedRef<INumericTypeInterface<float>> FSequencer::GetZeroPadNumericTypeInterface()
{
	return SequencerWidget->GetZeroPadNumericTypeInterface();
}

TSharedRef<SWidget> FSequencer::MakeTimeRange(const TSharedRef<SWidget>& InnerContent, bool bShowWorkingRange, bool bShowViewRange, bool bShowPlaybackRange)
{
	return SequencerWidget->MakeTimeRange(InnerContent, bShowWorkingRange, bShowViewRange, bShowPlaybackRange);
}

/** Attempt to find an object binding ID that relates to an unspawned spawnable object */
FGuid FindUnspawnedObjectGuid(UObject& InObject, UMovieSceneSequence& Sequence)
{
	UMovieScene* MovieScene = Sequence.GetMovieScene();

	// If the object is a CDO, the it relates to an unspawned spawnable.
	UObject* ParentObject = Sequence.GetParentObject(&InObject);
	if (ParentObject && ParentObject->HasAnyFlags(RF_ClassDefaultObject))
	{
		// Find the unspawned spawnable object's CDO
		UClass* ObjectClass = ParentObject->GetClass();

		FMovieSceneSpawnable* ParentSpawnable = MovieScene->FindSpawnable([&](FMovieSceneSpawnable& InSpawnable){
			return InSpawnable.GetClass() == ObjectClass;
		});

		if (ParentSpawnable)
		{
			UObject* ParentContext = ObjectClass->GetDefaultObject();

			// The only way to find the object now is to resolve all the child bindings, and see if they are the same
			for (const FGuid& ChildGuid : ParentSpawnable->GetChildPossessables())
			{
				UObject* ChildObject = Sequence.FindPossessableObject(ChildGuid, ParentContext);
				if (ChildObject == &InObject)
				{
					return ChildGuid;
				}
			}
		}
	}
	else if (InObject.HasAnyFlags(RF_ClassDefaultObject))
	{
		UClass* ObjectClass = InObject.GetClass();

		FMovieSceneSpawnable* SpawnableByClass = MovieScene->FindSpawnable([&](FMovieSceneSpawnable& InSpawnable){
			return InSpawnable.GetClass() == ObjectClass;
		});

		if (SpawnableByClass)
		{
			return SpawnableByClass->GetGuid();
		}
	}

	return FGuid();
}

FGuid FSequencer::GetHandleToObject( UObject* Object, bool bCreateHandleIfMissing )
{
	if (Object == nullptr)
	{
		return FGuid();
	}

	TSharedRef<FMovieSceneSequenceInstance> FocusedMovieSceneSequenceInstance = GetFocusedMovieSceneSequenceInstance();
	UMovieSceneSequence* FocusedMovieSceneSequence = FocusedMovieSceneSequenceInstance->GetSequence();

	UMovieScene* FocusedMovieScene = FocusedMovieSceneSequence->GetMovieScene();
	
	// Attempt to resolve the object through the movie scene instance first, 
	FGuid ObjectGuid = FocusedMovieSceneSequenceInstance->FindObjectId(*Object);

	// Check here for spawnable otherwise spawnables get recreated as possessables, which doesn't make sense
	FMovieSceneSpawnable* Spawnable = FocusedMovieScene->FindSpawnable(ObjectGuid);
	if (Spawnable)
	{
		return ObjectGuid;
	}

	// Make sure that the possessable is still valid, if it's not remove the binding so new one 
	// can be created.  This can happen due to undo.
	FMovieScenePossessable* Possessable = FocusedMovieScene->FindPossessable(ObjectGuid);
	if(Possessable == nullptr)
	{
		FocusedMovieSceneSequence->UnbindPossessableObjects(ObjectGuid);
		ObjectGuid.Invalidate();
	}

	if (!ObjectGuid.IsValid())
	{
		ObjectGuid = FindUnspawnedObjectGuid(*Object, *FocusedMovieSceneSequence);
	}

	if (ObjectGuid.IsValid())
	{
		return ObjectGuid;
	}

	// If the object guid was not found attempt to add it
	// Note: Only possessed actors can be added like this
	if (FocusedMovieSceneSequence->CanPossessObject(*Object) && bCreateHandleIfMissing)
	{
		AActor* PossessedActor = Cast<AActor>(Object);

		ObjectGuid = CreateBinding(*Object, PossessedActor != nullptr ? PossessedActor->GetActorLabel() : Object->GetName());

		NotifyMovieSceneDataChanged();
	}
	
	return ObjectGuid;
}


ISequencerObjectChangeListener& FSequencer::GetObjectChangeListener()
{ 
	return *ObjectChangeListener;
}


void FSequencer::GetRuntimeObjects( TSharedRef<FMovieSceneSequenceInstance> MovieSceneInstance, const FGuid& ObjectHandle, TArray<TWeakObjectPtr<UObject>>& OutObjects ) const
{
	UObject* FoundObject = MovieSceneInstance->FindObject(ObjectHandle, *this);
	if (FoundObject)
	{
		OutObjects.Add(FoundObject);
	}
}


void FSequencer::UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut) const
{
	OnCameraCutEvent.Broadcast(CameraObject, bJumpCut);

	// tell the camera we cut
	UCameraComponent* const CamComp = MovieSceneHelpers::CameraComponentFromRuntimeObject(CameraObject);
	if (CamComp)
	{
		CamComp->NotifyCameraCut();
	}

	if (!IsPerspectiveViewportCameraCutEnabled())
	{
		return;
	}

	AActor* UnlockIfCameraActor = Cast<AActor>(UnlockIfCameraObject);

	for (FLevelEditorViewportClient* LevelVC : GEditor->LevelViewportClients)
	{
		if ((LevelVC == nullptr) || !LevelVC->IsPerspective() || !LevelVC->AllowsCinematicPreview())
		{
			continue;
		}

		if ((CameraObject != nullptr) || LevelVC->IsLockedToActor(UnlockIfCameraActor))
		{
			UpdatePreviewLevelViewportClientFromCameraCut(*LevelVC, CameraObject, bJumpCut);
		}
	}
}


void FSequencer::SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap)
{
	if (!IsPerspectiveViewportPossessionEnabled())
	{
		return;
	}

	for (FLevelEditorViewportClient* LevelVC : GEditor->LevelViewportClients)
	{
		if (LevelVC && LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview())
		{
			if (ViewportParamsMap.Contains(LevelVC))
			{
				const EMovieSceneViewportParams* ViewportParams = ViewportParamsMap.Find(LevelVC);
				if (ViewportParams->SetWhichViewportParam & EMovieSceneViewportParams::SVP_FadeAmount)
				{
					LevelVC->FadeAmount = ViewportParams->FadeAmount;
					LevelVC->bEnableFading = true;
				}
				if (ViewportParams->SetWhichViewportParam & EMovieSceneViewportParams::SVP_FadeColor)
				{
					LevelVC->FadeColor = ViewportParams->FadeColor.ToRGBE();
					LevelVC->bEnableFading = true;
				}
				if (ViewportParams->SetWhichViewportParam & EMovieSceneViewportParams::SVP_ColorScaling)
				{
					LevelVC->bEnableColorScaling = ViewportParams->bEnableColorScaling;
					LevelVC->ColorScale = ViewportParams->ColorScale;
				}
			}
		}
	}
}


void FSequencer::GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const
{
	for (FLevelEditorViewportClient* LevelVC : GEditor->LevelViewportClients)
	{
		if (LevelVC && LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview())
		{
			EMovieSceneViewportParams ViewportParams;
			ViewportParams.FadeAmount = LevelVC->FadeAmount;
			ViewportParams.FadeColor = FLinearColor(LevelVC->FadeColor);
			ViewportParams.ColorScale = LevelVC->ColorScale;

			ViewportParamsMap.Add(LevelVC, ViewportParams);
		}
	}
}


EMovieScenePlayerStatus::Type FSequencer::GetPlaybackStatus() const
{
	return PlaybackState;
}


void FSequencer::SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus)
{
	PlaybackState = InPlaybackStatus;

	// backup or restore tick rate
	if (InPlaybackStatus == EMovieScenePlayerStatus::Playing)
	{
		OldMaxTickRate = GEngine->GetMaxFPS();
	}
	else
	{
		GEngine->SetMaxFPS(OldMaxTickRate);
	}
}


void FSequencer::AddOrUpdateMovieSceneInstance( UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneSequenceInstance> InstanceToAdd )
{
	if( !SequenceInstanceBySection.Contains( &MovieSceneSection ) )
	{
		SequenceInstanceBySection.Add( &MovieSceneSection, InstanceToAdd );
	}
}


void FSequencer::RemoveMovieSceneInstance( UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneSequenceInstance> InstanceToRemove )
{
	UMovieSceneSequence* Sequence = InstanceToRemove->GetSequence();

	SpawnRegister->CleanUpSequence(*InstanceToRemove, *this);

	SequenceInstanceBySection.Remove( &MovieSceneSection );
}


void FSequencer::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( Settings );

	for( int32 MovieSceneIndex = 0; MovieSceneIndex < SequenceInstanceStack.Num(); ++MovieSceneIndex )
	{
		UMovieSceneSequence* Sequence = SequenceInstanceStack[MovieSceneIndex]->GetSequence();
		Collector.AddReferencedObject( Sequence );
	}
}


void FSequencer::ResetPerMovieSceneData()
{
	//@todo Sequencer - We may want to preserve selections when moving between movie scenes
	Selection.Empty();

	// @todo run through all tracks for new movie scene changes
	//  needed for audio track decompression
}


void FSequencer::UpdateRuntimeInstances()
{
	// Refresh the current root instance
	SequenceInstanceStack.Top()->RefreshInstance( *this );

	EMovieSceneUpdateData UpdateData(ScrubPosition, ScrubPosition);
	SequenceInstanceStack.Top()->Update(UpdateData, *this);

	// Ensure any relevant spawned objects are cleaned up if we're playing back the master sequence
	if (SequenceInstanceStack.Num() == 1)
	{
		if (!GetPlaybackRange().Contains(ScrubPosition))
		{
			SpawnRegister->OnMasterSequenceExpired(*this);
		}
	}

	// If realtime is off, this needs to be called to update the pivot location when scrubbing.
	GUnrealEd->UpdatePivotLocationForSelection();
		
	// Redraw
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

TSharedRef<SWidget> FSequencer::MakeTransportControls(bool bExtended)
{
	FEditorWidgetsModule& EditorWidgetsModule = FModuleManager::Get().LoadModuleChecked<FEditorWidgetsModule>( "EditorWidgets" );

	FTransportControlArgs TransportControlArgs;
	{
		TransportControlArgs.OnBackwardEnd.BindSP( this, &FSequencer::OnStepToBeginning );
		TransportControlArgs.OnBackwardStep.BindSP( this, &FSequencer::OnStepBackward );
		TransportControlArgs.OnForwardPlay.BindSP( this, &FSequencer::OnPlay, true );
		TransportControlArgs.OnForwardStep.BindSP( this, &FSequencer::OnStepForward );
		TransportControlArgs.OnForwardEnd.BindSP( this, &FSequencer::OnStepToEnd );
		TransportControlArgs.OnToggleLooping.BindSP( this, &FSequencer::OnToggleLooping );
		TransportControlArgs.OnGetLooping.BindSP( this, &FSequencer::IsLooping );
		TransportControlArgs.OnGetPlaybackMode.BindSP( this, &FSequencer::GetPlaybackMode );

		if(bExtended)
		{
			TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportSetPlaybackStart)));
		}
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::BackwardEnd));
		if(bExtended)
		{
			TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportJumpToPreviousKey)));
		}
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::BackwardStep));
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::ForwardPlay));
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportRecord)));
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::ForwardStep));
		if(bExtended)
		{
			TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportJumpToNextKey)));
		}
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::ForwardEnd));
		if(bExtended)
		{
			TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(FOnMakeTransportWidget::CreateSP(this, &FSequencer::OnCreateTransportSetPlaybackEnd)));
		}
		TransportControlArgs.WidgetsToCreate.Add(FTransportControlWidget(ETransportControlWidgetType::Loop));
	}

	return EditorWidgetsModule.CreateTransportControl( TransportControlArgs );
}

TSharedRef<SWidget> FSequencer::OnCreateTransportSetPlaybackStart()
{
	return SNew(SButton)
		.OnClicked(this, &FSequencer::SetPlaybackStart)
		.ToolTipText(LOCTEXT("SetPlayStart_Tooltip", "Set playback start to the current position"))
		.ButtonStyle(FEditorStyle::Get(), "Sequencer.Transport.SetPlayStart")
		.ContentPadding(2.0f);
}

TSharedRef<SWidget> FSequencer::OnCreateTransportJumpToPreviousKey()
{
	return SNew(SButton)
		.OnClicked(this, &FSequencer::JumpToPreviousKey)
		.ToolTipText(LOCTEXT("JumpToPreviousKey_Tooltip", "Jump to the previous key in the selected track(s)"))
		.ButtonStyle(FEditorStyle::Get(), "Sequencer.Transport.JumpToPreviousKey")
		.ContentPadding(2.0f);
}

TSharedRef<SWidget> FSequencer::OnCreateTransportJumpToNextKey()
{
	return SNew(SButton)
		.OnClicked(this, &FSequencer::JumpToNextKey)
		.ToolTipText(LOCTEXT("JumpToNextKey_Tooltip", "Jump to the next key in the selected track(s)"))
		.ButtonStyle(FEditorStyle::Get(), "Sequencer.Transport.JumpToNextKey")
		.ContentPadding(2.0f);
}

TSharedRef<SWidget> FSequencer::OnCreateTransportSetPlaybackEnd()
{
	return SNew(SButton)
		.OnClicked(this, &FSequencer::SetPlaybackEnd)
		.ToolTipText(LOCTEXT("SetPlayEnd_Tooltip", "Set playback end to the current position"))
		.ButtonStyle(FEditorStyle::Get(), "Sequencer.Transport.SetPlayEnd")
		.ContentPadding(2.0f);
}

TSharedRef<SWidget> FSequencer::OnCreateTransportRecord()
{
	ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");

	return SNew(SButton)
		.OnClicked(this, &FSequencer::OnRecord)
		.ToolTipText_Lambda([&](){ return SequenceRecorder.IsRecording() ? LOCTEXT("StopRecord_Tooltip", "Stop recording current sub-track.") : LOCTEXT("Record_Tooltip", "Record the primed sequence sub-track."); })
		.ButtonStyle(FEditorStyle::Get(), "Animation.Record")
		.ContentPadding(2.0f)
		.Visibility(this, &FSequencer::GetRecordButtonVisibility);
}

namespace FaderConstants
{	
	/** The opacity when we are hovered */
	const float HoveredOpacity = 1.0f;
	/** The opacity when we are not hovered */
	const float NonHoveredOpacity = 0.75f;
	/** The amount of time spent actually fading in or out */
	const float FadeTime = 0.15f;
}

/** Wrapper widget allowing us to fade widgets in and out on hover state */
class SFader : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SFader)
		: _Content()
	{}

	SLATE_DEFAULT_SLOT(FArguments, Content)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		FadeInSequence = FCurveSequence(0.0f, FaderConstants::FadeTime);
		FadeOutSequence = FCurveSequence(0.0f, FaderConstants::FadeTime);
		FadeOutSequence.JumpToEnd();

		SBorder::Construct(SBorder::FArguments()
			.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
			.Padding(0.0f)
			.VAlign(VAlign_Center)
			.ColorAndOpacity(this, &SFader::GetColorAndOpacity)
			.Content()
			[
				InArgs._Content.Widget
			]);
	}

	FLinearColor GetColorAndOpacity() const
	{
		FLinearColor Color = FLinearColor::White;
	
		if(FadeOutSequence.IsPlaying() || !bIsHovered)
		{
			Color.A = FMath::Lerp(FaderConstants::HoveredOpacity, FaderConstants::NonHoveredOpacity, FadeOutSequence.GetLerp());
		}
		else
		{
			Color.A = FMath::Lerp(FaderConstants::NonHoveredOpacity, FaderConstants::HoveredOpacity, FadeInSequence.GetLerp());
		}

		return Color;
	}

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if(!FSlateApplication::Get().IsUsingHighPrecisionMouseMovment())
		{
			bIsHovered = true;
			if(FadeOutSequence.IsPlaying())
			{
				// Fade out is already playing so just force the fade in curve to the end so we don't have a "pop" 
				// effect from quickly resetting the alpha
				FadeInSequence.JumpToEnd();
			}
			else
			{
				FadeInSequence.Play(AsShared());
			}
		}
	}

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		if(!FSlateApplication::Get().IsUsingHighPrecisionMouseMovment())
		{
			bIsHovered = false;
			FadeOutSequence.Play(AsShared());
		}
	}

private:
	/** Curve sequence for fading out the widget */
	FCurveSequence FadeOutSequence;
	/** Curve sequence for fading in the widget */
	FCurveSequence FadeInSequence;
};

void FSequencer::SetViewportTransportControlsVisibility(bool bVisible)
{
	Settings->SetShowViewportTransportControls(bVisible);
}

void FSequencer::AttachTransportControlsToViewports()
{
	FLevelEditorModule* Module = FModuleManager::Get().LoadModulePtr<FLevelEditorModule>("LevelEditor");
	
	if (Module == nullptr)
	{
		return;
	}

	// register level editor viewport menu extenders
	ViewMenuExtender = FLevelEditorModule::FLevelEditorMenuExtender::CreateSP(this, &FSequencer::OnExtendLevelEditorViewMenu);
	Module->GetAllLevelViewportOptionsMenuExtenders().Add(ViewMenuExtender);		
	LevelEditorExtenderDelegateHandle = Module->GetAllLevelViewportOptionsMenuExtenders().Last().GetHandle();

	TSharedPtr<ILevelEditor> LevelEditor = Module->GetFirstLevelEditor();
	const TArray< TSharedPtr<ILevelViewport> >& LevelViewports = LevelEditor->GetViewports();
		
	FEditorWidgetsModule& EditorWidgetsModule = FModuleManager::Get().LoadModuleChecked<FEditorWidgetsModule>( "EditorWidgets" );

	for (int32 i = 0; i < LevelViewports.Num(); ++i)
	{
		const TSharedPtr<ILevelViewport>& LevelViewport = LevelViewports[i];
			
		TSharedRef<SHorizontalBox> TransportControl =
			SNew(SHorizontalBox)
				.Visibility(EVisibility::SelfHitTestInvisible)

			+SHorizontalBox::Slot()
				.FillWidth(1)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Bottom)
				.Padding(4.f)
				[
					SNew(SFader)
					.Content()
					[
						SNew(SBorder)
						.Padding(4.f)
						.Cursor( EMouseCursor::Default )
						.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
						.Visibility(this, &FSequencer::GetTransportControlVisibility, LevelViewport)
						.Content()
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								MakeTransportControls(false)
							]
							+SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Top)
							[
								SNew(SButton)
								.ButtonStyle(&FEditorStyle::Get().GetWidgetStyle< FButtonStyle >("Sequencer.Transport.CloseButton"))
								.ToolTipText(LOCTEXT("CloseTransportControlsToolTip", "Hide the transport controls. You can re-enable transport controls from the viewport menu."))
								.OnClicked_Lambda([this]() { Settings->SetShowViewportTransportControls(!Settings->GetShowViewportTransportControls()); return FReply::Handled(); })
								
							]
						]
					]
				];

		LevelViewport->AddOverlayWidget(TransportControl);

		TransportControls.Add(LevelViewport, TransportControl);
	}
}


void FSequencer::DetachTransportControlsFromViewports()
{
	FLevelEditorModule* Module = FModuleManager::Get().LoadModulePtr<FLevelEditorModule>("LevelEditor");
	if (Module)
	{
		TSharedPtr<ILevelEditor> LevelEditor = Module->GetFirstLevelEditor();
		if (LevelEditor.IsValid())
		{
			const TArray< TSharedPtr<ILevelViewport> >& LevelViewports = LevelEditor->GetViewports();
		
			for (int32 i = 0; i < LevelViewports.Num(); ++i)
			{
				const TSharedPtr<ILevelViewport>& LevelViewport = LevelViewports[i];

				TSharedPtr<SWidget>* TransportControl = TransportControls.Find(LevelViewport);
				if (TransportControl && TransportControl->IsValid())
				{
					LevelViewport->RemoveOverlayWidget(TransportControl->ToSharedRef());
				}
			}
		}

		Module->GetAllLevelViewportOptionsMenuExtenders().RemoveAll([&](const FLevelEditorModule::FLevelEditorMenuExtender& Delegate) {
			return Delegate.GetHandle() == LevelEditorExtenderDelegateHandle;
		});
	}
}

TSharedRef<FExtender> FSequencer::OnExtendLevelEditorViewMenu(const TSharedRef<FUICommandList> CommandList)
{
	TSharedRef<FExtender> Extender(new FExtender());

	Extender->AddMenuExtension(
		"LevelViewportViewportOptions2",
		EExtensionHook::First,
		NULL,
		FMenuExtensionDelegate::CreateSP(this, &FSequencer::CreateTransportToggleMenuEntry));

	return Extender;
}

void FSequencer::CreateTransportToggleMenuEntry(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ShowTransportControls", "Show Transport Controls"),
		LOCTEXT("ShowTransportControlsToolTip", "Show or hide the Sequencer transport controls when a sequence is active."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this](){ Settings->SetShowViewportTransportControls(!Settings->GetShowViewportTransportControls()); }),
			FCanExecuteAction(), 
			FGetActionCheckState::CreateLambda([this](){ return Settings->GetShowViewportTransportControls() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
}

EVisibility FSequencer::GetTransportControlVisibility(TSharedPtr<ILevelViewport> LevelViewport) const
{
	FLevelEditorViewportClient& ViewportClient = LevelViewport->GetLevelViewportClient();
	return Settings->GetShowViewportTransportControls() &&
		ViewportClient.ViewportType == LVT_Perspective &&
		ViewportClient.AllowsCinematicPreview() ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply FSequencer::OnPlay(bool bTogglePlay)
{
	if( (PlaybackState == EMovieScenePlayerStatus::Playing ||
		 PlaybackState == EMovieScenePlayerStatus::Recording) && bTogglePlay )
	{
		SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);

		// Snap time when stopping play
		SetGlobalTime(ScrubPosition, ESnapTimeMode::STM_Interval);

		// Update on stop (cleans up things like sounds that are playing)
		EMovieSceneUpdateData UpdateData(ScrubPosition, ScrubPosition);
		SequenceInstanceStack.Top()->Update(UpdateData, *this );
	}
	else
	{
		SetPlaybackStatus(EMovieScenePlayerStatus::Playing);

		// Make sure Slate ticks during playback
		SequencerWidget->RegisterActiveTimerForPlayback();
	}

	return FReply::Handled();
}


EVisibility FSequencer::GetRecordButtonVisibility() const
{
	return UMovieSceneSubSection::IsSetAsRecording() ? EVisibility::Visible : EVisibility::Collapsed;
}


FReply FSequencer::OnRecord()
{
	ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");

	if(UMovieSceneSubSection::IsSetAsRecording() && !SequenceRecorder.IsRecording())
	{
		UWorld* PlaybackContext = Cast<UWorld>(GetPlaybackContext());
		const FString& ActorToRecord = UMovieSceneSubSection::GetActorToRecord();
		const FString& PathToRecordTo = UMovieSceneSubSection::GetRecordingSection()->GetTargetPathToRecordTo();
		const FString& SequenceName = UMovieSceneSubSection::GetRecordingSection()->GetTargetSequenceName();
		SequenceRecorder.StartRecording(PlaybackContext, ActorToRecord, FOnRecordingStarted::CreateSP(this, &FSequencer::HandleRecordingStarted), FOnRecordingFinished::CreateSP(this, &FSequencer::HandleRecordingFinished), PathToRecordTo, SequenceName);
	}
	else if(SequenceRecorder.IsRecording())
	{
		SequenceRecorder.StopRecording();
	}

	return FReply::Handled();
}

void FSequencer::HandleRecordingStarted(UMovieSceneSequence* Sequence)
{
	OnPlay(false);
		
	// Make sure Slate ticks during playback
	SequencerWidget->RegisterActiveTimerForPlayback();

	// sync recording section to start
	UMovieSceneSubSection* Section = UMovieSceneSubSection::GetRecordingSection();
	if(Section != nullptr)
	{
		Section->SetStartTime(ScrubPosition);
		Section->SetEndTime(ScrubPosition + Settings->GetTimeSnapInterval());
	}
}

void FSequencer::HandleRecordingFinished(UMovieSceneSequence* Sequence)
{
	// toggle us to no playing if we are still playing back
	// as the post processing takes such a long time we don't really care if the sequence doesnt carry on
	if(PlaybackState == EMovieScenePlayerStatus::Playing)
	{
		OnPlay(true);
	}

	// now patchup the section that was recorded to
	UMovieSceneSubSection* Section = UMovieSceneSubSection::GetRecordingSection();
	if(Section != nullptr)
	{
		Section->SetAsRecording(false);
		Section->SetSequence(Sequence);
		Section->SetEndTime(Section->GetStartTime() + Sequence->GetMovieScene()->GetPlaybackRange().Size<float>());
	}

	bNeedTreeRefresh = true;
}

FReply FSequencer::OnStepForward()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
	float NewPosition = ScrubPosition + Settings->GetTimeSnapInterval();
	SetGlobalTime(NewPosition, ESnapTimeMode::STM_Interval);
	return FReply::Handled();
}


FReply FSequencer::OnStepBackward()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
	float NewPosition = ScrubPosition - Settings->GetTimeSnapInterval();
	const bool bAllowSnappingToFrames = true;
	SetGlobalTime(NewPosition, ESnapTimeMode::STM_Interval);
	return FReply::Handled();
}


FReply FSequencer::OnStepToEnd()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
	SetGlobalTime(GetPlaybackRange().GetUpperBoundValue());
	return FReply::Handled();
}

FReply FSequencer::OnStepToBeginning()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
	SetGlobalTime(GetPlaybackRange().GetLowerBoundValue());
	return FReply::Handled();
}


FReply FSequencer::OnToggleLooping()
{
	Settings->SetLooping(!Settings->IsLooping());
	return FReply::Handled();
}


FReply FSequencer::SetPlaybackEnd()
{
	const UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();
	if (FocusedSequence)
	{
		TRange<float> CurrentRange = FocusedSequence->GetMovieScene()->GetPlaybackRange();
		float NewPos = FMath::Max(GetGlobalTime(), CurrentRange.GetLowerBoundValue());
		SetPlaybackRange(TRange<float>(CurrentRange.GetLowerBoundValue(), NewPos));
	}

	return FReply::Handled();
}

FReply FSequencer::SetPlaybackStart()
{
	const UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();
	if (FocusedSequence)
	{
		TRange<float> CurrentRange = FocusedSequence->GetMovieScene()->GetPlaybackRange();
		float NewPos = FMath::Max(GetGlobalTime(), CurrentRange.GetUpperBoundValue());
		SetPlaybackRange(TRange<float>(NewPos, CurrentRange.GetUpperBoundValue()));	
	}

	return FReply::Handled();
}

FReply FSequencer::JumpToPreviousKey()
{
	TUniquePtr<ISequencerKeyCollection> ActiveKeyCollection;
	GetKeysFromSelection(ActiveKeyCollection);
	if (ActiveKeyCollection.IsValid())
	{
		TRange<float> FindRange(TRange<float>::BoundsType(), GetGlobalTime());

		TOptional<float> NewTime = ActiveKeyCollection->FindFirstKeyInRange(FindRange, EFindKeyDirection::Backwards);
		if (NewTime.IsSet())
		{
			SetGlobalTime(NewTime.GetValue());
		}
	}

	return FReply::Handled();
}

FReply FSequencer::JumpToNextKey()
{
	TUniquePtr<ISequencerKeyCollection> ActiveKeyCollection;
	GetKeysFromSelection(ActiveKeyCollection);
	if (ActiveKeyCollection.IsValid())
	{
		TRange<float> FindRange(GetGlobalTime(), TRange<float>::BoundsType());

		TOptional<float> NewTime = ActiveKeyCollection->FindFirstKeyInRange(FindRange, EFindKeyDirection::Forwards);
		if (NewTime.IsSet())
		{
			SetGlobalTime(NewTime.GetValue());
		}
	}

	return FReply::Handled();
}

bool FSequencer::IsLooping() const
{
	return Settings->IsLooping();
}


void FSequencer::SetGlobalTimeLooped(float InTime)
{
	if (Settings->IsLooping())
	{
		const UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();
		if (FocusedSequence)
		{
			if (InTime > FocusedSequence->GetMovieScene()->GetPlaybackRange().GetUpperBoundValue())
			{
				InTime -= FocusedSequence->GetMovieScene()->GetPlaybackRange().Size<float>();
			}
		}
	}
	else
	{
		TRange<float> TimeBounds = GetTimeBounds();
		TRange<float> WorkingRange = GetClampRange();

		const bool bReachedEnd = GetGlobalTime() < TimeBounds.GetUpperBoundValue() && InTime >= TimeBounds.GetUpperBoundValue();

		// Stop if we hit the playback range end
		if (bReachedEnd)
		{
			InTime = TimeBounds.GetUpperBoundValue();
			SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
		}
		// Constrain to the play range if necessary
		else if (Settings->ShouldKeepCursorInPlayRange())
		{
			// Clamp to lower bound
			InTime = FMath::Max(InTime, TimeBounds.GetLowerBoundValue());

			// Jump back if necessary
			if (InTime >= TimeBounds.GetUpperBoundValue())
			{
				InTime = TimeBounds.GetLowerBoundValue();
			}
		}
		// Ensure the time is within the working range
		else if (!WorkingRange.Contains(InTime))
		{
			InTime = FMath::Clamp(InTime, WorkingRange.GetLowerBoundValue(), WorkingRange.GetUpperBoundValue());
			SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
		}
	}

	SetGlobalTime(InTime);
}


bool FSequencer::CanShowFrameNumbers() const
{
	return SequencerSnapValues::IsTimeSnapIntervalFrameRate(Settings->GetTimeSnapInterval());
}


EPlaybackMode::Type FSequencer::GetPlaybackMode() const
{
	return PlaybackState == EMovieScenePlayerStatus::Playing ? EPlaybackMode::PlayingForward :
		PlaybackState == EMovieScenePlayerStatus::Recording ? EPlaybackMode::Recording :
		EPlaybackMode::Stopped;
}


void FSequencer::UpdateTimeBoundsToFocusedMovieScene()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	// Set the view range to:
	// 1. The moviescene view range
	// 2. The moviescene playback range
	// 3. Some sensible default
	TRange<float> NewRange = FocusedMovieScene->GetEditorData().ViewRange;

	if (NewRange.IsEmpty() || NewRange.IsDegenerate())
	{
		NewRange = FocusedMovieScene->GetPlaybackRange();
	}
	if (NewRange.IsEmpty() || NewRange.IsDegenerate())
	{
		NewRange = TRange<float>(0.f, 5.f);
	}

	// Set the view range to the new range
	SetViewRange(NewRange, EViewRangeInterpolation::Immediate);

	// Make sure the current time is within the bounds
	if (!TargetViewRange.Contains(ScrubPosition))
	{
		ScrubPosition = LastViewRange.GetLowerBoundValue();
		OnGlobalTimeChangedDelegate.Broadcast();
	}
}


TRange<float> FSequencer::GetTimeBounds() const
{
	const UMovieSceneSequence* FocusedSequence = GetFocusedMovieSceneSequence();

	// When recording, we never want to constrain the time bound range.  You might not even have any sections or keys yet
	// but we need to be able to move the time cursor during playback so you can capture data in real-time
	if( PlaybackState == EMovieScenePlayerStatus::Recording || !FocusedSequence)
	{
		return TRange<float>( -100000.0f, 100000.0f );
	}
	
	return FocusedSequence->GetMovieScene()->GetPlaybackRange();
}


void FSequencer::SetViewRange(TRange<float> NewViewRange, EViewRangeInterpolation Interpolation)
{
	if (!ensure(NewViewRange.HasUpperBound() && NewViewRange.HasLowerBound() && !NewViewRange.IsDegenerate()))
	{
		return;
	}

	const float AnimationLengthSeconds = Interpolation == EViewRangeInterpolation::Immediate ? 0.f : 0.1f;
	if (AnimationLengthSeconds != 0.f)
	{
		if (ZoomAnimation.GetCurve(0).DurationSeconds != AnimationLengthSeconds)
		{
			ZoomAnimation = FCurveSequence();
			ZoomCurve = ZoomAnimation.AddCurve(0.f, AnimationLengthSeconds, ECurveEaseFunction::QuadIn);
		}

		if (!ZoomAnimation.IsPlaying())
		{
			LastViewRange = TargetViewRange;
			ZoomAnimation.Play( SequencerWidget.ToSharedRef() );
		}
		TargetViewRange = NewViewRange;
	}
	else
	{
		TargetViewRange = LastViewRange = NewViewRange;
		ZoomAnimation.JumpToEnd();
	}


	UMovieSceneSequence* FocusedMovieSequence = GetFocusedMovieSceneSequence();
	if (FocusedMovieSequence != nullptr)
	{
		UMovieScene* FocusedMovieScene = FocusedMovieSequence->GetMovieScene();
		if (FocusedMovieScene != nullptr)
		{
			FocusedMovieScene->GetEditorData().ViewRange = TargetViewRange;

			// Always ensure the working range is big enough to fit the view range
			TRange<float>& WorkingRange = FocusedMovieScene->GetEditorData().WorkingRange;

			WorkingRange = TRange<float>(
				FMath::Min(TargetViewRange.GetLowerBoundValue(), WorkingRange.GetLowerBoundValue()),
				FMath::Max(TargetViewRange.GetUpperBoundValue(), WorkingRange.GetUpperBoundValue())
				);
		}
	}
}


void FSequencer::OnClampRangeChanged( TRange<float> NewClampRange )
{
	if (!NewClampRange.IsEmpty())
	{
		GetFocusedMovieSceneSequence()->GetMovieScene()->GetEditorData().WorkingRange = NewClampRange;
	}
}

float FSequencer::FindNearestKey(float NewScrubPosition)
{
	TUniquePtr<ISequencerKeyCollection> ActiveKeyCollection;
	GetKeysFromSelection(ActiveKeyCollection);

	if (ActiveKeyCollection.IsValid())
	{
		TRange<float> FindRangeBackwards(TRange<float>::BoundsType(), NewScrubPosition);
		TOptional<float> NewTimeBackwards = ActiveKeyCollection->FindFirstKeyInRange(FindRangeBackwards, EFindKeyDirection::Backwards);

		TRange<float> FindRangeForwards(NewScrubPosition, TRange<float>::BoundsType());
		TOptional<float> NewTimeForwards = ActiveKeyCollection->FindFirstKeyInRange(FindRangeForwards, EFindKeyDirection::Forwards);
		if (NewTimeForwards.IsSet())
		{
			if (NewTimeBackwards.IsSet())
			{
				if (FMath::Abs(NewTimeForwards.GetValue() - NewScrubPosition) < FMath::Abs(NewTimeBackwards.GetValue() - NewScrubPosition))
				{
					NewScrubPosition = NewTimeForwards.GetValue();
				}
				else
				{
					NewScrubPosition = NewTimeBackwards.GetValue();
				}
			}
			else
			{
				NewScrubPosition = NewTimeForwards.GetValue();
			}
		}
		else if (NewTimeBackwards.IsSet())
		{
			NewScrubPosition = NewTimeBackwards.GetValue();
		}
	}
	return NewScrubPosition;
}

void FSequencer::OnScrubPositionChanged( float NewScrubPosition, bool bScrubbing )
{
	bool bClampToViewRange = true;

	if (PlaybackState == EMovieScenePlayerStatus::Scrubbing)
	{
		if (!bScrubbing)
		{
			OnEndScrubbing();
		}
		else if (IsAutoScrollEnabled())
		{
			// Clamp to the view range when not auto-scrolling
			bClampToViewRange = false;
	
			UpdateAutoScroll(NewScrubPosition);
			
			// When scrubbing, we animate auto-scrolled scrub position in Tick()
			if (AutoscrubOffset.IsSet())
			{
				return;
			}
		}
	}

	if (bClampToViewRange)
	{
		NewScrubPosition = FMath::Clamp(NewScrubPosition, TargetViewRange.GetLowerBoundValue(), TargetViewRange.GetUpperBoundValue());
	}

	SetGlobalTimeDirectly( NewScrubPosition, Settings->GetSnapKeyTimesToKeys() ? ESnapTimeMode::STM_Keys : ESnapTimeMode::STM_None );
}


void FSequencer::OnBeginScrubbing()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Scrubbing);
	SequencerWidget->RegisterActiveTimerForPlayback();
}


void FSequencer::OnEndScrubbing()
{
	SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
	AutoscrubOffset.Reset();
	StopAutoscroll();
}


void FSequencer::OnBeginPlaybackRangeDrag()
{
	GEditor->BeginTransaction(LOCTEXT("SetPlaybackRange_Transaction", "Set playback range"));
}


void FSequencer::OnEndPlaybackRangeDrag()
{
	GEditor->EndTransaction();
}


void FSequencer::OnBeginInOutRangeDrag()
{
	GEditor->BeginTransaction(LOCTEXT("SetSelectionRange_Transaction", "Set selection range"));
}


void FSequencer::OnEndInOutRangeDrag()
{
	GEditor->EndTransaction();
}


void FSequencer::StartAutoscroll(float UnitsPerS)
{
	AutoscrollOffset = UnitsPerS;
}


void FSequencer::StopAutoscroll()
{
	AutoscrollOffset.Reset();
}


void FSequencer::OnToggleAutoScroll()
{
	Settings->SetAutoScrollEnabled(!Settings->GetAutoScrollEnabled());
}


bool FSequencer::IsAutoScrollEnabled() const
{
	return Settings->GetAutoScrollEnabled();
}


void FSequencer::FindInContentBrowser()
{
	if (GetFocusedMovieSceneSequence())
	{
		TArray<UObject*> ObjectsToFocus;
		ObjectsToFocus.Add(GetCurrentAsset());

		GEditor->SyncBrowserToObjects(ObjectsToFocus);
	}
}


UObject* FSequencer::GetCurrentAsset() const
{
	// For now we find the asset by looking at the root movie scene's outer.
	// @todo: this may need refining if/when we support editing movie scene instances
	return GetFocusedMovieSceneSequence()->GetMovieScene()->GetOuter();
}


void FSequencer::VerticalScroll(float ScrollAmountUnits)
{
	SequencerWidget->GetTreeView()->ScrollByDelta(ScrollAmountUnits);
}

FGuid FSequencer::AddSpawnable(UObject& Object)
{
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	if (!Sequence->AllowsSpawnableObjects())
	{
		return FGuid();
	}

	// Grab the MovieScene that is currently focused.  We'll add our Blueprint as an inner of the
	// MovieScene asset.
	UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

	TValueOrError<FNewSpawnable, FText> Result = SpawnRegister->CreateNewSpawnableType(Object, *OwnerMovieScene);
	if (!Result.IsValid())
	{
		FNotificationInfo Info(Result.GetError());
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return FGuid();
	}

	FNewSpawnable& NewSpawnable = Result.GetValue();

	auto DuplName = [&](FMovieSceneSpawnable& InSpawnable)
	{
		return InSpawnable.GetName() == NewSpawnable.Name;
	};

	int32 Index = 2;
	FString UniqueString;
	while (OwnerMovieScene->FindSpawnable(DuplName))
	{
		NewSpawnable.Name.RemoveFromEnd(UniqueString);
		UniqueString = FString::Printf(TEXT(" (%d)"), Index++);
		NewSpawnable.Name += UniqueString;
	}

	FGuid NewGuid = OwnerMovieScene->AddSpawnable(NewSpawnable.Name, NewSpawnable.Blueprint);

	UpdateRuntimeInstances();

	return NewGuid;
}

FGuid FSequencer::MakeNewSpawnable( UObject& Object )
{
	// @todo sequencer: Undo doesn't seem to be working at all
	const FScopedTransaction Transaction( LOCTEXT("UndoAddingObject", "Add Object to MovieScene") );

	FGuid NewGuid = AddSpawnable(Object);
	if (!NewGuid.IsValid())
	{
		return FGuid();
	}

	// Spawn the object so we can position it correctly, it's going to get spawned anyway since things default to spawned.
	UObject* SpawnedObject = SpawnRegister->SpawnObject(NewGuid, *GetFocusedMovieSceneSequenceInstance(), *this);

	FTransformData DefaultTransform;

	AActor* SpawnedActor = Cast<AActor>(SpawnedObject);
	if (SpawnedActor)
	{
		// Place the new spawnable in front of the camera (unless we were automatically created from a PIE actor)
		if (Settings->GetSpawnPosition() == SSP_PlaceInFrontOfCamera)
		{
			PlaceActorInFrontOfCamera( SpawnedActor );
		}
		DefaultTransform.Translation = SpawnedActor->GetActorLocation();
		DefaultTransform.Rotation = SpawnedActor->GetActorRotation();
		DefaultTransform.Scale = FVector(1.0f, 1.0f, 1.0f);
		DefaultTransform.bValid = true;

		OnActorAddedToSequencerEvent.Broadcast(SpawnedActor, NewGuid);

		const bool bNotifySelectionChanged = true;
		const bool bDeselectBSP = true;
		const bool bWarnAboutTooManyActors = false;
		const bool bSelectEvenIfHidden = false;

		GEditor->SelectNone( bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors );
		GEditor->SelectActor( SpawnedActor, true, bNotifySelectionChanged, bSelectEvenIfHidden );
	}

	SetupDefaultsForSpawnable(NewGuid, DefaultTransform);

	return NewGuid;
}

void FSequencer::SetupDefaultsForSpawnable( const FGuid& Guid, const FTransformData& DefaultTransform )
{
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

	// Ensure it has a spawn track
	UMovieSceneSpawnTrack* SpawnTrack = Cast<UMovieSceneSpawnTrack>(OwnerMovieScene->FindTrack(UMovieSceneSpawnTrack::StaticClass(), Guid, NAME_None));
	if (!SpawnTrack)
	{
		SpawnTrack = Cast<UMovieSceneSpawnTrack>(OwnerMovieScene->AddTrack(UMovieSceneSpawnTrack::StaticClass(), Guid));
	}

	if (SpawnTrack)
	{
		UMovieSceneBoolSection* SpawnSection = Cast<UMovieSceneBoolSection>(SpawnTrack->CreateNewSection());
		SpawnSection->SetDefault(true);
		SpawnSection->SetIsInfinite(GetInfiniteKeyAreas());
		SpawnTrack->AddSection(*SpawnSection);
		SpawnTrack->SetObjectId(Guid);
	}
	
	// Ensure it will spawn in the right place
	if (DefaultTransform.bValid)
	{
		UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(OwnerMovieScene->FindTrack(UMovieScene3DTransformTrack::StaticClass(), Guid, "Transform"));
		if (!TransformTrack)
		{
			TransformTrack = Cast<UMovieScene3DTransformTrack>(OwnerMovieScene->AddTrack(UMovieScene3DTransformTrack::StaticClass(), Guid));
		}

		if (TransformTrack)
		{
			const bool bUnwindRotation = false;

			EMovieSceneKeyInterpolation Interpolation = GetKeyInterpolation();

			const TArray<UMovieSceneSection*>& Sections = TransformTrack->GetAllSections();
			if (!Sections.Num())
			{
				TransformTrack->AddSection(*TransformTrack->CreateNewSection());
			}
			
			for (UMovieSceneSection* Section : Sections)
			{
				UMovieScene3DTransformSection* TransformSection = CastChecked<UMovieScene3DTransformSection>(Section);

				TransformSection->SetDefault( FTransformKey( EKey3DTransformChannel::Translation, EAxis::X, DefaultTransform.Translation.X, bUnwindRotation ) );
				TransformSection->SetDefault( FTransformKey( EKey3DTransformChannel::Translation, EAxis::Y, DefaultTransform.Translation.Y, bUnwindRotation ) );
				TransformSection->SetDefault( FTransformKey( EKey3DTransformChannel::Translation, EAxis::Z, DefaultTransform.Translation.Z, bUnwindRotation ) );

				TransformSection->SetDefault( FTransformKey( EKey3DTransformChannel::Rotation, EAxis::X, DefaultTransform.Rotation.Euler().X, bUnwindRotation ) );
				TransformSection->SetDefault( FTransformKey( EKey3DTransformChannel::Rotation, EAxis::Y, DefaultTransform.Rotation.Euler().Y, bUnwindRotation ) );
				TransformSection->SetDefault( FTransformKey( EKey3DTransformChannel::Rotation, EAxis::Z, DefaultTransform.Rotation.Euler().Z, bUnwindRotation ) );

				TransformSection->SetDefault( FTransformKey( EKey3DTransformChannel::Scale, EAxis::X, DefaultTransform.Scale.X, bUnwindRotation ) );
				TransformSection->SetDefault( FTransformKey( EKey3DTransformChannel::Scale, EAxis::Y, DefaultTransform.Scale.Y, bUnwindRotation ) );
				TransformSection->SetDefault( FTransformKey( EKey3DTransformChannel::Scale, EAxis::Z, DefaultTransform.Scale.Z, bUnwindRotation ) );

				TransformSection->SetIsInfinite(GetInfiniteKeyAreas());
			}
		}
	}
}

void FSequencer::AddSubSequence(UMovieSceneSequence* Sequence)
{
	// @todo Sequencer - sub-moviescenes This should be moved to the sub-moviescene editor

	// Grab the MovieScene that is currently focused.  THis is the movie scene that will contain the sub-moviescene
	UMovieScene* OwnerMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	// @todo sequencer: Undo doesn't seem to be working at all
	const FScopedTransaction Transaction( LOCTEXT("UndoAddingObject", "Add Object to MovieScene") );
	OwnerMovieScene->Modify();

	UMovieSceneSubTrack* SubTrack = OwnerMovieScene->AddMasterTrack<UMovieSceneSubTrack>();
	float Duration = Sequence->GetMovieScene()->GetPlaybackRange().Size<float>();
	SubTrack->AddSequence(Sequence, ScrubPosition, Duration);
}


bool FSequencer::OnHandleAssetDropped(UObject* DroppedAsset, const FGuid& TargetObjectGuid)
{
	bool bWasConsumed = false;
	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		bool bWasHandled = TrackEditors[i]->HandleAssetAdded(DroppedAsset, TargetObjectGuid);
		if (bWasHandled)
		{
			// @todo Sequencer - This will crash if multiple editors try to handle a single asset
			// Should we allow this? How should it consume then?
			// gmp 10/7/2015: the user should be presented with a dialog asking what kind of track they want to create
			check(!bWasConsumed);
			bWasConsumed = true;
		}
	}
	return bWasConsumed;
}


// Takes a display node and traverses it's parents to find the nearest track node if any.  Also collects the names of the nodes which make
// up the path from the track node to the display node being checked.  The name path includes the name of the node being checked, but not
// the name of the track node.
void GetParentTrackNodeAndNamePath(TSharedRef<const FSequencerDisplayNode> DisplayNode, TSharedPtr<FSequencerTrackNode>& OutParentTrack, TArray<FName>& OutNamePath )
{
	TArray<FName> PathToTrack;
	PathToTrack.Add( DisplayNode->GetNodeName() );
	TSharedPtr<FSequencerDisplayNode> CurrentParent = DisplayNode->GetParent();

	while ( CurrentParent.IsValid() && CurrentParent->GetType() != ESequencerNode::Track )
	{
		PathToTrack.Add( CurrentParent->GetNodeName() );
		CurrentParent = CurrentParent->GetParent();
	}

	if ( CurrentParent.IsValid() )
	{
		OutParentTrack = StaticCastSharedPtr<FSequencerTrackNode>( CurrentParent );
		for ( int32 i = PathToTrack.Num() - 1; i >= 0; i-- )
		{
			OutNamePath.Add( PathToTrack[i] );
		}
	}
}


void FSequencer::OnRequestNodeDeleted( TSharedRef<const FSequencerDisplayNode> NodeToBeDeleted )
{
	bool bAnythingRemoved = false;
	
	TSharedRef<FMovieSceneSequenceInstance> MovieSceneInstance = GetFocusedMovieSceneSequenceInstance();
	UMovieSceneSequence* Sequence = MovieSceneInstance->GetSequence();
	UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

	if ( NodeToBeDeleted->GetType() == ESequencerNode::Folder )
	{
		// Delete Children
		for ( const TSharedRef<FSequencerDisplayNode>& ChildNode : NodeToBeDeleted->GetChildNodes() )
		{
			OnRequestNodeDeleted( ChildNode );
		}

		// Delete from parent, or root.
		TSharedRef<const FSequencerFolderNode> FolderToBeDeleted = StaticCastSharedRef<const FSequencerFolderNode>(NodeToBeDeleted);
		if ( NodeToBeDeleted->GetParent().IsValid() )
		{
			TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>( NodeToBeDeleted->GetParent() );
			ParentFolder->GetFolder().Modify();
			ParentFolder->GetFolder().RemoveChildFolder( &FolderToBeDeleted->GetFolder() );
		}
		else
		{
			UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
			FocusedMovieScene->Modify();
			FocusedMovieScene->GetRootFolders().Remove( &FolderToBeDeleted->GetFolder() );
		}

		bAnythingRemoved = true;

	}
	else if (NodeToBeDeleted->GetType() == ESequencerNode::Object)
	{
		// Delete any child object bindings
		for (const TSharedRef<FSequencerDisplayNode>& ChildNode : NodeToBeDeleted->GetChildNodes())
		{
			if (ChildNode->GetType() == ESequencerNode::Object)
			{
				OnRequestNodeDeleted(ChildNode);
			}
		}

		const FGuid& BindingToRemove = StaticCastSharedRef<const FSequencerObjectBindingNode>( NodeToBeDeleted )->GetObjectBinding();

		// Remove from a parent folder if necessary.
		if ( NodeToBeDeleted->GetParent().IsValid() && NodeToBeDeleted->GetParent()->GetType() == ESequencerNode::Folder )
		{
			TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>( NodeToBeDeleted->GetParent() );
			ParentFolder->GetFolder().Modify();
			ParentFolder->GetFolder().RemoveChildObjectBinding( BindingToRemove );
		}
		
		// Try to remove as a spawnable first
		if (OwnerMovieScene->RemoveSpawnable(BindingToRemove))
		{
			SpawnRegister->DestroySpawnedObject(BindingToRemove, *MovieSceneInstance, *this);
		}
		// The guid should be associated with a possessable if it wasnt a spawnable
		else if (OwnerMovieScene->RemovePossessable(BindingToRemove))
		{
			Sequence->Modify();
			Sequence->UnbindPossessableObjects( BindingToRemove );
		}
		else
		{
			// If this check fails there was an invalid guid being stored on a node
			checkf(false, TEXT("Guid was not associated with a spawnable or possessable"));
		}

		bAnythingRemoved = true;
	}
	else if( NodeToBeDeleted->GetType() == ESequencerNode::Track  )
	{
		TSharedRef<const FSequencerTrackNode> SectionAreaNode = StaticCastSharedRef<const FSequencerTrackNode>( NodeToBeDeleted );
		UMovieSceneTrack* Track = SectionAreaNode->GetTrack();

		// Remove from a parent folder if necessary.
		if ( NodeToBeDeleted->GetParent().IsValid() && NodeToBeDeleted->GetParent()->GetType() == ESequencerNode::Folder )
		{
			TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>( NodeToBeDeleted->GetParent() );
			ParentFolder->GetFolder().Modify();
			ParentFolder->GetFolder().RemoveChildMasterTrack( Track );
		}

		if (Track != nullptr)
		{
			if (OwnerMovieScene->IsAMasterTrack(*Track))
			{
				OwnerMovieScene->RemoveMasterTrack(*Track);
			}
			else if (OwnerMovieScene->GetCameraCutTrack() == Track)
			{
				OwnerMovieScene->RemoveCameraCutTrack();
			}
			else
			{
				OwnerMovieScene->RemoveTrack(*Track);
			}
		
			bAnythingRemoved = true;
		}
	}
	else if ( NodeToBeDeleted->GetType() == ESequencerNode::Category )
	{
		TSharedPtr<FSequencerTrackNode> ParentTrackNode;
		TArray<FName> PathFromTrack;
		GetParentTrackNodeAndNamePath(NodeToBeDeleted, ParentTrackNode, PathFromTrack);
		if ( ParentTrackNode.IsValid() )
		{
			for ( TSharedRef<ISequencerSection> Section : ParentTrackNode->GetSections() )
			{
				bAnythingRemoved |= Section->RequestDeleteCategory( PathFromTrack );
			}
		}
	}
	else if ( NodeToBeDeleted->GetType() == ESequencerNode::KeyArea )
	{
		TSharedPtr<FSequencerTrackNode> ParentTrackNode;
		TArray<FName> PathFromTrack;
		GetParentTrackNodeAndNamePath( NodeToBeDeleted, ParentTrackNode, PathFromTrack );
		if ( ParentTrackNode.IsValid() )
		{
			for ( TSharedRef<ISequencerSection> Section : ParentTrackNode->GetSections() )
			{
				bAnythingRemoved |= Section->RequestDeleteKeyArea( PathFromTrack );
			}
		}
	}

	if( bAnythingRemoved )
	{
		NotifyMovieSceneDataChanged();
	}
}


void FSequencer::PlaceActorInFrontOfCamera( AActor* ActorCDO )
{
	// Place the actor in front of the active perspective camera if we have one
	if ((GCurrentLevelEditingViewportClient != nullptr) && GCurrentLevelEditingViewportClient->IsPerspective())
	{
		// Don't allow this when the active viewport is showing a simulation/PIE level
		const bool bIsViewportShowingPIEWorld = GCurrentLevelEditingViewportClient->GetWorld()->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor);
		if (!bIsViewportShowingPIEWorld)
		{
			// @todo sequencer actors: Ideally we could use the actor's collision to figure out how far to push out
			// the object (like when placing in viewports), but we can't really do that because we're only dealing with a CDO
			const float DistanceFromCamera = 50.0f;

			// Find a place to put the object
			// @todo sequencer cleanup: This code should be reconciled with the GEditor->MoveActorInFrontOfCamera() stuff
			const FVector& CameraLocation = GCurrentLevelEditingViewportClient->GetViewLocation();
			FRotator CameraRotation = GCurrentLevelEditingViewportClient->GetViewRotation();
			const FVector CameraDirection = CameraRotation.Vector();

			FVector NewLocation = CameraLocation + CameraDirection * ( DistanceFromCamera + GetDefault<ULevelEditorViewportSettings>()->BackgroundDropDistance );
			FSnappingUtils::SnapPointToGrid( NewLocation, FVector::ZeroVector );

			CameraRotation.Roll = 0.f;
			CameraRotation.Pitch =0.f;

			ActorCDO->SetActorRelativeLocation( NewLocation );
			ActorCDO->SetActorRelativeRotation( CameraRotation );
		}
	}
}


void FSequencer::GetActorRecordingState( bool& bIsRecording /* In+Out */ ) const
{
	if (IsRecordingLive())
	{
		bIsRecording = true;
	}
}


void FSequencer::PostUndo(bool bSuccess)
{
	NotifyMovieSceneDataChanged();
}


void FSequencer::OnPreSaveWorld(uint32 SaveFlags, class UWorld* World)
{
	// Restore the saved state so that the level save can save that instead of the animated state.
	RootMovieSceneSequenceInstance->RestoreState(*this);
}


void FSequencer::OnPostSaveWorld(uint32 SaveFlags, class UWorld* World, bool bSuccess)
{
	// Reset the time after saving so that an update will be triggered to put objects back to their animated state.
	RootMovieSceneSequenceInstance->RefreshInstance(*this);
	SetGlobalTime(GetGlobalTime());
}


void FSequencer::OnNewCurrentLevel()
{
	ActivateSequencerEditorMode();
}


void FSequencer::OnMapOpened(const FString& Filename, bool bLoadAsTemplate)
{
	ActivateSequencerEditorMode();
}


void FSequencer::ActivateSequencerEditorMode()
{
	if( GLevelEditorModeTools().IsModeActive( FSequencerEdMode::EM_SequencerMode ) )
	{
		GLevelEditorModeTools().DeactivateMode( FSequencerEdMode::EM_SequencerMode );
	}

	GLevelEditorModeTools().ActivateMode( FSequencerEdMode::EM_SequencerMode );

	FSequencerEdMode* SequencerEdMode = (FSequencerEdMode*)(GLevelEditorModeTools().GetActiveMode(FSequencerEdMode::EM_SequencerMode));
	SequencerEdMode->SetSequencer(ActiveSequencerPtr.Pin().Get());
}


void FSequencer::OnPreBeginPIE(bool bIsSimulating)
{
	RootMovieSceneSequenceInstance->RestoreState(*this);
}


void FSequencer::OnEndPIE(bool bIsSimulating)
{
	UpdateRuntimeInstances();
}


void FSequencer::ActivateDetailKeyframeHandler()
{			
	// Add sequencer detail keyframe handler
	static const FName DetailsTabIdentifiers[] = { "LevelEditorSelectionDetails", "LevelEditorSelectionDetails2", "LevelEditorSelectionDetails3", "LevelEditorSelectionDetails4" };
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	for (const FName& DetailsTabIdentifier : DetailsTabIdentifiers)
	{
		TSharedPtr<IDetailsView> DetailsView = EditModule.FindDetailView(DetailsTabIdentifier);

		if(DetailsView.IsValid())
		{
			DetailsView->SetKeyframeHandler(DetailKeyframeHandler);
		}
	}
}


void FSequencer::DeactivateDetailKeyframeHandler()
{
	static const FName DetailsTabIdentifiers[] = { "LevelEditorSelectionDetails", "LevelEditorSelectionDetails2", "LevelEditorSelectionDetails3", "LevelEditorSelectionDetails4" };
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	for (const FName& DetailsTabIdentifier : DetailsTabIdentifiers)
	{
		TSharedPtr<IDetailsView> DetailsView = EditModule.FindDetailView(DetailsTabIdentifier);

		if (DetailsView.IsValid())
		{
			if (DetailsView->GetKeyframeHandler() == DetailKeyframeHandler)
			{
				DetailsView->SetKeyframeHandler(0);
			}
		}
	}
}


void FSequencer::OnPropertyEditorOpened()
{
	ActivateDetailKeyframeHandler();
}


void FSequencer::UpdatePreviewLevelViewportClientFromCameraCut(FLevelEditorViewportClient& InViewportClient, UObject* InCameraObject, bool bJumpCut) const
{
	AActor* CameraActor = Cast<AActor>(InCameraObject);

	if (CameraActor)
	{
		InViewportClient.SetViewLocation(CameraActor->GetActorLocation());
		InViewportClient.SetViewRotation(CameraActor->GetActorRotation());
		InViewportClient.bEditorCameraCut = !InViewportClient.IsLockedToActor(CameraActor) || bJumpCut;
	}
	else
	{
		InViewportClient.ViewFOV = InViewportClient.FOVAngle;
		InViewportClient.bEditorCameraCut = bJumpCut;
	}

	// Set the actor lock.
	InViewportClient.SetMatineeActorLock(CameraActor);
	InViewportClient.bLockedCameraView = CameraActor != nullptr;
	InViewportClient.RemoveCameraRoll();

	// If viewing through a camera - enforce aspect ratio.
	if (CameraActor)
	{
		UCameraComponent* CameraComponent = Cast<UCameraComponent>(CameraActor->GetComponentByClass(UCameraComponent::StaticClass()));
		if (CameraComponent)
		{
			if (CameraComponent->AspectRatio == 0)
			{
				InViewportClient.AspectRatio = 1.7f;
			}
			else
			{
				InViewportClient.AspectRatio = CameraComponent->AspectRatio;
			}

			//don't stop the camera from zooming when not playing back
			InViewportClient.ViewFOV = CameraComponent->FieldOfView;

			// If there are selected actors, invalidate the viewports hit proxies, otherwise they won't be selectable afterwards
			if (InViewportClient.Viewport && GEditor->GetSelectedActorCount() > 0)
			{
				InViewportClient.Viewport->InvalidateHitProxy();
			}
		}
	}

	// Update ControllingActorViewInfo, so it is in sync with the updated viewport
	InViewportClient.UpdateViewForLockedActor();
}


void GetDescendantMovieScenes(UMovieSceneSequence* InSequence, TArray<UMovieScene*> & InMovieScenes)
{
	UMovieScene* InMovieScene = InSequence->GetMovieScene();
	if (InMovieScene == nullptr)
	{
		return;
	}

	InMovieScenes.Add(InMovieScene);

	for (auto MasterTrack : InMovieScene->GetMasterTracks())
	{
		for (auto Section : MasterTrack->GetAllSections())
		{
			UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
			if (SubSection != nullptr)
			{
				UMovieSceneSequence* SubSequence = SubSection->GetSequence();
				if (SubSequence != nullptr)
				{
					GetDescendantMovieScenes(SubSequence, InMovieScenes);
				}
			}
		}
	}
}

void FSequencer::SaveCurrentMovieScene()
{
	// Capture thumbnail
	// Convert UObject* array to FAssetData array
	TArray<FAssetData> AssetDataList;
	AssetDataList.Add(FAssetData(GetCurrentAsset()));

	FViewport* Viewport = GEditor->GetActiveViewport();

	// If there's no active viewport, find any other viewport that allows cinematic preview.
	if (Viewport == nullptr)
	{
		for (FLevelEditorViewportClient* LevelVC : GEditor->LevelViewportClients)
		{
			if ((LevelVC == nullptr) || !LevelVC->IsPerspective() || !LevelVC->AllowsCinematicPreview())
			{
				continue;
			}

			Viewport = LevelVC->Viewport;
		}
	}

	if ( ensure(GCurrentLevelEditingViewportClient) && Viewport != nullptr )
	{
		bool bIsInGameView = GCurrentLevelEditingViewportClient->IsInGameView();
		GCurrentLevelEditingViewportClient->SetGameView(true);

		//have to re-render the requested viewport
		FLevelEditorViewportClient* OldViewportClient = GCurrentLevelEditingViewportClient;
		//remove selection box around client during render
		GCurrentLevelEditingViewportClient = NULL;

		Viewport->Draw();

		IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
		ContentBrowser.CaptureThumbnailFromViewport(Viewport, AssetDataList);

		//redraw viewport to have the yellow highlight again
		GCurrentLevelEditingViewportClient = OldViewportClient;
		GCurrentLevelEditingViewportClient->SetGameView(bIsInGameView);
		Viewport->Draw();
	}

	TArray<UPackage*> PackagesToSave;
	TArray<UMovieScene*> MovieScenesToSave;
	GetDescendantMovieScenes(RootMovieSceneSequenceInstance->GetSequence(), MovieScenesToSave);
	for (auto MovieSceneToSave : MovieScenesToSave)
	{
		UPackage* MovieScenePackageToSave = MovieSceneToSave->GetOuter()->GetOutermost();
		if (MovieScenePackageToSave->IsDirty())
		{
			PackagesToSave.Add(MovieScenePackageToSave);
		}
	}

	// If there's more than 1 movie scene to save, prompt the user whether to save all dirty movie scenes.
	const bool bCheckDirty = PackagesToSave.Num() > 1;
	const bool bPromptToSave = PackagesToSave.Num() > 1;

	FEditorFileUtils::PromptForCheckoutAndSave( PackagesToSave, bCheckDirty, bPromptToSave );
}


void FSequencer::SaveCurrentMovieSceneAs()
{
	TSharedPtr<IToolkitHost> MyToolkitHost = GetToolkitHost();

	if (!MyToolkitHost.IsValid())
	{
		return;
	}

	TArray<UObject*> AssetsToSave;
	AssetsToSave.Add(GetCurrentAsset());

	TArray<UObject*> SavedAssets;
	FEditorFileUtils::SaveAssetsAs(AssetsToSave, SavedAssets);

	if ((SavedAssets[0] != AssetsToSave[0]) && (SavedAssets[0] != nullptr))
	{
		FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();
		AssetEditorManager.CloseAllEditorsForAsset(AssetsToSave[0]);
		AssetEditorManager.OpenEditorForAssets(SavedAssets, EToolkitMode::Standalone, MyToolkitHost.ToSharedRef());
	}
}


void FSequencer::AddSelectedObjects()
{
	UMovieSceneSequence* OwnerSequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();
	USelection* CurrentSelection = GEditor->GetSelectedActors();

	TArray<AActor*> SelectedActors;
	TArray<UObject*> SelectedObjects;

	CurrentSelection->GetSelectedObjects( AActor::StaticClass(), SelectedObjects );

	for (TArray<UObject*>::TIterator It(SelectedObjects); It; ++It)
	{
		AActor* Actor = CastChecked<AActor>(*It);

		if (Actor != nullptr)
		{
			SelectedActors.Add(Actor);
		}
	}

	bool bPossessableAdded = false;
	if (SelectedActors.Num() != 0)
	{
		const FScopedTransaction Transaction(LOCTEXT("UndoPossessingObject", "Possess Object in Sequencer"));
		OwnerSequence->Modify();

		for (auto Actor : SelectedActors)
		{
			FGuid ExistingGuid = SequenceInstanceStack.Top()->FindObjectId(*Actor);
			if (!ExistingGuid.IsValid())
			{
				FGuid PossessableGuid = CreateBinding(*Actor, Actor->GetActorLabel());

				UpdateRuntimeInstances();

				OnActorAddedToSequencerEvent.Broadcast(Actor, PossessableGuid);

				bPossessableAdded = true;
			}
		}
	}

	if (bPossessableAdded)
	{
		NotifyMovieSceneDataChanged();
	}
}


void FSequencer::OnSectionSelectionChanged()
{
}


void FSequencer::OnSelectedOutlinerNodesChanged()
{
	if ( bUpdatingSequencerSelection == false )
	{
		bUpdatingExternalSelection = true;
		SynchronizeExternalSelectionWithSequencerSelection();
		bUpdatingExternalSelection = false;
	}
}


void FSequencer::SynchronizeExternalSelectionWithSequencerSelection()
{
	if ( !IsLevelEditorSequencer() )
	{
		return;
	}

	TSet<AActor*> SelectedSequencerActors;
	for ( TSharedRef<FSequencerDisplayNode> SelectedOutlinerNode : Selection.GetSelectedOutlinerNodes() )
	{
		// Get the root object binding node.
		TSharedPtr<FSequencerDisplayNode> CurrentNode = SelectedOutlinerNode;
		TSharedPtr<FSequencerObjectBindingNode> ObjectBindingNode;
		while ( CurrentNode.IsValid() )
		{
			if ( CurrentNode->GetType() == ESequencerNode::Object )
			{
				ObjectBindingNode = StaticCastSharedPtr<FSequencerObjectBindingNode>(CurrentNode);
			}
			CurrentNode = CurrentNode->GetParent();
		}

		// If the root node is an object node, try to get the actor nodes from it.
		if ( ObjectBindingNode.IsValid() )
		{
			TArray<TWeakObjectPtr<UObject>> RuntimeObjects;
			GetRuntimeObjects( GetFocusedMovieSceneSequenceInstance(), ObjectBindingNode->GetObjectBinding(), RuntimeObjects );
			
			for (auto RuntimeObject : RuntimeObjects )
			{
				AActor* Actor = Cast<AActor>(RuntimeObject.Get());
				if ( Actor != nullptr )
				{
					SelectedSequencerActors.Add( Actor );
				}
			}
		}
	}

	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "UpdatingActorSelection", "Select Actors" ) );

	const bool bNotifySelectionChanged = false;
	const bool bDeselectBSP = true;
	const bool bWarnAboutTooManyActors = false;
	const bool bSelectEvenIfHidden = true;

	GEditor->GetSelectedActors()->Modify();
	GEditor->GetSelectedActors()->BeginBatchSelectOperation();

	GEditor->SelectNone( bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors );

	for ( AActor* SelectedSequencerActor : SelectedSequencerActors )
	{
		GEditor->SelectActor( SelectedSequencerActor, true, bNotifySelectionChanged, bSelectEvenIfHidden );
	}

	GEditor->GetSelectedActors()->EndBatchSelectOperation();
	GEditor->NoteSelectionChange();
}


void FSequencer::OnActorSelectionChanged( UObject* )
{
	if ( bUpdatingExternalSelection == false )
	{
		bUpdatingSequencerSelection = true;
		SynchronizeSequencerSelectionWithExternalSelection();
		bUpdatingSequencerSelection = false;
	}
}


void GetRootObjectBindingNodes(const TArray<TSharedRef<FSequencerDisplayNode>>& DisplayNodes, TArray<TSharedRef<FSequencerObjectBindingNode>>& RootObjectBindings )
{
	for ( TSharedRef<FSequencerDisplayNode> DisplayNode : DisplayNodes )
	{
		switch ( DisplayNode->GetType() )
		{
		case ESequencerNode::Folder:
			GetRootObjectBindingNodes( DisplayNode->GetChildNodes(), RootObjectBindings );
			break;
		case ESequencerNode::Object:
			RootObjectBindings.Add( StaticCastSharedRef<FSequencerObjectBindingNode>( DisplayNode ) );
			break;
		}
	}
}


void FSequencer::SynchronizeSequencerSelectionWithExternalSelection()
{
	if ( !IsLevelEditorSequencer() )
	{
		return;
	}

	TArray<TSharedRef<FSequencerObjectBindingNode>> RootObjectBindingNodes;
	GetRootObjectBindingNodes( NodeTree->GetRootNodes(), RootObjectBindingNodes );

	TSet<TSharedRef<FSequencerDisplayNode>> NodesToSelect;
	for ( TSharedRef<FSequencerObjectBindingNode> ObjectBindingNode : RootObjectBindingNodes )
	{
		TArray<TWeakObjectPtr<UObject>> RuntimeObjects;
		GetRuntimeObjects( GetFocusedMovieSceneSequenceInstance(), ObjectBindingNode->GetObjectBinding(), RuntimeObjects );

		for ( TWeakObjectPtr<UObject> RuntimeObjectPtr : RuntimeObjects )
		{
			UObject* RuntimeObject = RuntimeObjectPtr.Get();
			if ( RuntimeObject != nullptr && GEditor->GetSelectedActors()->IsSelected( RuntimeObject ) )
			{
				NodesToSelect.Add( ObjectBindingNode );
			}
		}
	}

	Selection.SuspendBroadcast();
	Selection.EmptySelectedOutlinerNodes();
	for ( TSharedRef<FSequencerDisplayNode> NodeToSelect : NodesToSelect)
	{
		Selection.AddToSelection( NodeToSelect );
	}
	Selection.ResumeBroadcast();
	Selection.GetOnOutlinerNodeSelectionChanged().Broadcast();
}


void FSequencer::ZoomToSelectedSections()
{
	TArray< TRange<float> > Bounds;
	for (TWeakObjectPtr<UMovieSceneSection> SelectedSection : Selection.GetSelectedSections())
	{
		Bounds.Add(SelectedSection->GetRange());
	}
	TRange<float> BoundsHull = TRange<float>::Hull(Bounds);

	if (BoundsHull.IsEmpty())
	{
		BoundsHull = GetTimeBounds();
	}

	if (!BoundsHull.IsEmpty() && !BoundsHull.IsDegenerate())
	{
		// Zoom back to last view range if already expanded
		if (!ViewRangeBeforeZoom.IsEmpty() &&
			FMath::IsNearlyEqual(BoundsHull.GetLowerBoundValue(), GetViewRange().GetLowerBoundValue(), KINDA_SMALL_NUMBER) &&
			FMath::IsNearlyEqual(BoundsHull.GetUpperBoundValue(), GetViewRange().GetUpperBoundValue(), KINDA_SMALL_NUMBER))
		{
			SetViewRange(ViewRangeBeforeZoom, EViewRangeInterpolation::Animated);
		}
		else
		{
			ViewRangeBeforeZoom = GetViewRange();

			SetViewRange(BoundsHull, EViewRangeInterpolation::Animated);
		}
	}
}


bool FSequencer::CanKeyProperty(FCanKeyPropertyParams CanKeyPropertyParams) const
{
	return ObjectChangeListener->CanKeyProperty(CanKeyPropertyParams);
} 


void FSequencer::KeyProperty(FKeyPropertyParams KeyPropertyParams) 
{
	ObjectChangeListener->KeyProperty(KeyPropertyParams);
}


FSequencerSelection& FSequencer::GetSelection()
{
	return Selection;
}


FSequencerSelectionPreview& FSequencer::GetSelectionPreview()
{
	return SelectionPreview;
}


float FSequencer::GetOverlayFadeCurve() const
{
	return OverlayCurve.GetLerp();
}


void FSequencer::DeleteSelectedItems()
{
	if (Selection.GetSelectedKeys().Num())
	{
		FScopedTransaction DeleteKeysTransaction( NSLOCTEXT("Sequencer", "DeleteKeys_Transaction", "Delete Keys") );
		
		DeleteSelectedKeys();
	}
	else if (Selection.GetSelectedSections().Num())
	{
		FScopedTransaction DeleteSectionsTransaction( NSLOCTEXT("Sequencer", "DeleteSections_Transaction", "Delete Sections") );
	
		DeleteSections(Selection.GetSelectedSections());
	}
	else if (Selection.GetSelectedOutlinerNodes().Num())
	{
		DeleteSelectedNodes();
	}
}


void FSequencer::AssignActor(FMenuBuilder& MenuBuilder, FGuid InObjectBinding)
{
	UObject* RuntimeObject = SequenceInstanceStack.Top()->FindObject(InObjectBinding, *this);

	auto IsActorValidForAssignment = [=](const AActor* InActor, UObject* CurrentObject){
		return CurrentObject != InActor;
	};

	using namespace SceneOutliner;

	// Set up a menu entry to assign an actor to the object binding node
	FInitializationOptions InitOptions;
	{
		InitOptions.Mode = ESceneOutlinerMode::ActorPicker;

		// We hide the header row to keep the UI compact.
		InitOptions.bShowHeaderRow = false;
		InitOptions.bShowSearchBox = true;
		InitOptions.bShowCreateNewFolder = false;
		// Only want the actor label column
		InitOptions.ColumnMap.Add(FBuiltInColumnTypes::Label(), FColumnInfo(EColumnVisibility::Visible, 0));

		// Only display actors that are not possessed already
		InitOptions.Filters->AddFilterPredicate( FActorFilterPredicate::CreateLambda( IsActorValidForAssignment, RuntimeObject ) );
	}

	// actor selector to allow the user to choose an actor
	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
	TSharedRef< SWidget > MiniSceneOutliner =
		SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(400.0f)
		[
			SceneOutlinerModule.CreateSceneOutliner(
				InitOptions,
				FOnActorPicked::CreateLambda([=](AActor* Actor){
					// Create a new binding for this actor
					FSlateApplication::Get().DismissAllMenus();
					DoAssignActor(&Actor, 1, InObjectBinding);
				})
			)
		];

	MenuBuilder.AddWidget(MiniSceneOutliner, FText::GetEmpty(), true);
	MenuBuilder.EndSection();
}


void FSequencer::DoAssignActor(AActor*const* InActors, int32 NumActors, FGuid InObjectBinding)
{
	if (NumActors <= 0)
	{
		return;
	}

	AActor* Actor = InActors[0];

	if (Actor == nullptr)
	{
		return;
	}

	FScopedTransaction AssignActor( NSLOCTEXT("Sequencer", "AssignActor", "Assign Actor") );

	RootMovieSceneSequenceInstance->SaveState(*this);

	UMovieSceneSequence* OwnerSequence = GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();

	Actor->Modify();
	OwnerSequence->Modify();
	OwnerMovieScene->Modify();

	UObject* RuntimeObject = SequenceInstanceStack.Top()->FindObject(InObjectBinding, *this);

	// Replace the object itself
	FGuid ParentGuid;
	{
		// Get the object guid to assign, remove the binding if it already exists
		ParentGuid = SequenceInstanceStack.Top()->FindObjectId(*Actor);
		FString NewActorLabel = Actor->GetActorLabel();
		if (ParentGuid.IsValid())
		{
			OwnerMovieScene->RemovePossessable(ParentGuid);
			OwnerSequence->UnbindPossessableObjects(ParentGuid);
		}

		// Add this object
		FMovieScenePossessable NewPossessable( NewActorLabel, Actor->GetClass());
		ParentGuid = NewPossessable.GetGuid();
		OwnerSequence->BindPossessableObject(ParentGuid, *Actor, GetPlaybackContext());

		// Replace
		OwnerMovieScene->ReplacePossessable(InObjectBinding, ParentGuid, NewActorLabel);
	}

	auto UpdateComponent = [&]( FGuid OldComponentGuid, UActorComponent* NewComponent )
	{
		// Get the object guid to assign, remove the binding if it already exists
		FGuid NewComponentGuid = SequenceInstanceStack.Top()->FindObjectId( *NewComponent );
		FString NewComponentLabel = NewComponent->GetName();
		if ( NewComponentGuid.IsValid() )
		{
			OwnerMovieScene->RemovePossessable( NewComponentGuid );
			OwnerSequence->UnbindPossessableObjects( NewComponentGuid );
		}

		// Add this object
		FMovieScenePossessable NewPossessable( NewComponentLabel, NewComponent->GetClass() );
		NewComponentGuid = NewPossessable.GetGuid();
		OwnerSequence->BindPossessableObject( NewComponentGuid, *NewComponent, Actor );

		// Replace
		OwnerMovieScene->ReplacePossessable( OldComponentGuid, NewComponentGuid, NewComponentLabel );

		FMovieScenePossessable* ThisPossessable = OwnerMovieScene->FindPossessable( NewComponentGuid );
		if ( ensure( ThisPossessable ) )
		{
			ThisPossessable->SetParent( ParentGuid );
		}
	};

	// Handle components
	AActor* ActorToReplace = Cast<AActor>(RuntimeObject);
	if (ActorToReplace != nullptr && ActorToReplace->IsActorBeingDestroyed() == false)
	{
		for (UActorComponent* ComponentToReplace : ActorToReplace->GetComponents())
		{
			if (ComponentToReplace != nullptr)
			{
				FGuid ComponentGuid = SequenceInstanceStack.Top()->FindObjectId(*ComponentToReplace);
				if (ComponentGuid.IsValid())
				{
					UActorComponent* NewComponent = Actor->GetComponentByClass(ComponentToReplace->GetClass());
					if (NewComponent)
					{
						UpdateComponent( ComponentGuid, NewComponent );
					}
				}
			}
		}
	}
	else // If the actor didn't exist, try to find components who's parent guids were the previous actors guid.
	{
		TMap<FString, UActorComponent*> ComponentNameToComponent;
		for ( UActorComponent* Component : Actor->GetComponents() )
		{
			ComponentNameToComponent.Add( Component->GetName(), Component );
		}
		for ( int32 i = 0; i < OwnerMovieScene->GetPossessableCount(); i++ )
		{
			FMovieScenePossessable& OldPossessable = OwnerMovieScene->GetPossessable(i);
			if ( OldPossessable.GetParent() == InObjectBinding )
			{
				UActorComponent** ComponentPtr = ComponentNameToComponent.Find( OldPossessable.GetName() );
				if ( ComponentPtr != nullptr )
				{
					UpdateComponent( OldPossessable.GetGuid(), *ComponentPtr );
				}
			}
		}
	}

	// Try to fix up folders.
	TArray<UMovieSceneFolder*> FoldersToCheck;
	FoldersToCheck.Append(GetFocusedMovieSceneSequence()->GetMovieScene()->GetRootFolders());
	bool bFolderFound = false;
	while ( FoldersToCheck.Num() > 0 && bFolderFound == false )
	{
		UMovieSceneFolder* Folder = FoldersToCheck[0];
		FoldersToCheck.RemoveAt(0);
		if ( Folder->GetChildObjectBindings().Contains( InObjectBinding ) )
		{
			Folder->Modify();
			Folder->RemoveChildObjectBinding( InObjectBinding );
			Folder->AddChildObjectBinding( ParentGuid );
			bFolderFound = true;
		}

		for ( UMovieSceneFolder* ChildFolder : Folder->GetChildFolders() )
		{
			FoldersToCheck.Add( ChildFolder );
		}
	}

	RootMovieSceneSequenceInstance->RestoreState(*this);
	NotifyMovieSceneDataChanged();
}


void FSequencer::DeleteNode(TSharedRef<FSequencerDisplayNode> NodeToBeDeleted)
{
	// If this node is selected, delete all selected nodes
	if (GetSelection().IsSelected(NodeToBeDeleted))
	{
		DeleteSelectedNodes();
	}
	else
	{
		const FScopedTransaction Transaction( NSLOCTEXT("Sequencer", "UndoDeletingObject", "Delete Node") );
		OnRequestNodeDeleted(NodeToBeDeleted);
	}
}


void FSequencer::DeleteSelectedNodes()
{
	TSet< TSharedRef<FSequencerDisplayNode> > SelectedNodesCopy = GetSelection().GetSelectedOutlinerNodes();

	if (SelectedNodesCopy.Num() == 0)
	{
		return;
	}

	const FScopedTransaction Transaction( NSLOCTEXT("Sequencer", "UndoDeletingObject", "Delete Node") );

	for( const TSharedRef<FSequencerDisplayNode>& SelectedNode : SelectedNodesCopy )
	{
		if( !SelectedNode->IsHidden() )
		{
			// Delete everything in the entire node
			TSharedRef<const FSequencerDisplayNode> NodeToBeDeleted = StaticCastSharedRef<const FSequencerDisplayNode>(SelectedNode);
			OnRequestNodeDeleted( NodeToBeDeleted );
		}
	}
}


void FSequencer::ToggleNodeActive()
{
	bool bIsActive = !IsNodeActive();
	const FScopedTransaction Transaction( NSLOCTEXT("Sequencer", "ToggleNodeActive", "Toggle Node Active") );

	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(OutlinerNode, Sections);

		for (auto Section : Sections)
		{
			Section->SetIsActive(bIsActive);
		}
	}
}


bool FSequencer::IsNodeActive() const
{
	// Active only if all are active
	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(OutlinerNode, Sections);

		for (auto Section : Sections)
		{
			if (!Section->IsActive())
			{
				return false;
			}
		}
	}
	return true;
}


void FSequencer::ToggleNodeLocked()
{
	bool bIsLocked = !IsNodeLocked();

	const FScopedTransaction Transaction( NSLOCTEXT("Sequencer", "ToggleNodeLocked", "Toggle Node Locked") );

	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(OutlinerNode, Sections);

		for (auto Section : Sections)
		{
			Section->SetIsLocked(bIsLocked);
		}
	}
}


bool FSequencer::IsNodeLocked() const
{
	// Locked only if all are locked
	int NumSections = 0;
	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(OutlinerNode, Sections);

		for (auto Section : Sections)
		{
			if (!Section->IsLocked())
			{
				return false;
			}
			++NumSections;
		}
	}
	return NumSections > 0;
}

void FSequencer::ConvertToSpawnable(TSharedRef<FSequencerObjectBindingNode> NodeToBeConverted)
{
	const FScopedTransaction Transaction( LOCTEXT("ConvertSelectedSpawnable", "Convert Node to Spawnables") );

	// Ensure we're in a non-possessed state
	RootMovieSceneSequenceInstance->RestoreState(*this);
	GetFocusedMovieSceneSequence()->GetMovieScene()->Modify();
	ConvertToSpawnableInternal(NodeToBeConverted);
	NotifyMovieSceneDataChanged();
}

void FSequencer::ConvertSelectedNodesToSpawnables()
{
	// @todo sequencer: Undo doesn't seem to be working at all
	const FScopedTransaction Transaction( LOCTEXT("ConvertSelectedSpawnable", "Convert Selected Nodes to Spawnables") );

	UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	// Ensure we're in a non-possessed state
	RootMovieSceneSequenceInstance->RestoreState(*this);
	MovieScene->Modify();

	TArray<TSharedRef<FSequencerObjectBindingNode>> ObjectBindingNodes;

	for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
	{
		if (Node->GetType() == ESequencerNode::Object)
		{
			auto ObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);

			// If we have a possessable for this node, and it has no parent, we can convert it to a spawnable
			FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBindingNode->GetObjectBinding());
			if (Possessable && !Possessable->GetParent().IsValid())
			{
				ObjectBindingNodes.Add(ObjectBindingNode);
			}
		}
	}

	FScopedSlowTask SlowTask(ObjectBindingNodes.Num(), LOCTEXT("ConvertSpawnableProgress", "Converting Selected Possessable Nodes to Spawnables"));
	SlowTask.MakeDialog(true);

	for (const TSharedRef<FSequencerObjectBindingNode>& ObjectBindingNode : ObjectBindingNodes)
	{
		SlowTask.EnterProgressFrame();
		ConvertToSpawnableInternal(ObjectBindingNode);

		if (GWarn->ReceivedUserCancel())
		{
			break;
		}
	}

	NotifyMovieSceneDataChanged();
}

void FSequencer::ConvertToSpawnableInternal(TSharedRef<FSequencerObjectBindingNode> NodeToBeConverted)
{
	UMovieSceneSequence* Sequence = GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	FMovieScenePossessable* Possessable = MovieScene->FindPossessable(NodeToBeConverted->GetObjectBinding());

	if (!Possessable)
	{
		return;
	}

	// Find the object in the environment
	TSharedRef<FMovieSceneSequenceInstance> FocusedSequenceInstance = SequenceInstanceStack.Top();
	UMovieSceneSequence* FocusedSequence = FocusedSequenceInstance->GetSequence();

	UObject* FoundObject = FocusedSequenceInstance->FindObject(NodeToBeConverted->GetObjectBinding(), *this);
	if (!FoundObject || !FocusedSequence)
	{
		return;
	}

	Sequence->Modify();

	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(AddSpawnable(*FoundObject));
	if (!Spawnable)
	{
		return;
	}

	// Swap the guids, so the possessable's tracks now belong to the spawnable
	{
		FGuid BenignSpawnableGuid = Spawnable->GetGuid();
		FGuid PersistentGuid = Possessable->GetGuid();

		Possessable->SetGuid(BenignSpawnableGuid);
		Spawnable->SetGuid(PersistentGuid);

		if (MovieScene->RemovePossessable(BenignSpawnableGuid))
		{
			Sequence->UnbindPossessableObjects(PersistentGuid);
		}

		for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index)
		{
			FMovieScenePossessable& MovieScenePossessable = MovieScene->GetPossessable(Index);

			bool bBelongsToNewSpawnable = MovieScenePossessable.GetParent() == PersistentGuid;
			if (!bBelongsToNewSpawnable)
			{
				// This is potentially slow, but fixes old data where we didn't assign parent object guids
				UObject* PotentialChildObject = FocusedSequenceInstance->FindObject(MovieScenePossessable.GetGuid(), *this);
				bBelongsToNewSpawnable = PotentialChildObject && FocusedSequence->GetParentObject(PotentialChildObject) == FoundObject;
			}
			
			if (bBelongsToNewSpawnable)
			{
				MovieScenePossessable.SetParent(PersistentGuid);
				Spawnable->AddChildPossessable(MovieScenePossessable.GetGuid());
			}
		}
	}
	
	FTransformData DefaultTransform;

	// @todo: Where should this code live (not here)?
	AActor* OldActor = Cast<AActor>(FoundObject);
	if (OldActor)
	{
		DefaultTransform.Translation = OldActor->GetActorLocation();
		DefaultTransform.Rotation = OldActor->GetActorRotation();
		DefaultTransform.Scale = OldActor->GetActorScale();
		DefaultTransform.bValid = true;

		AActor* CDO = CastChecked<AActor>(Spawnable->GetClass()->GetDefaultObject());

		EditorUtilities::CopyActorProperties(OldActor, CDO, EditorUtilities::ECopyOptions::Default);
		GWorld->DestroyActor(OldActor);
	}

	SetupDefaultsForSpawnable(Spawnable->GetGuid(), DefaultTransform);
	SetGlobalTimeDirectly(ScrubPosition);
}


void FSequencer::OnAddFolder()
{
	// Check if a folder, or child of a folder is currently selected.
	TArray<UMovieSceneFolder*> SelectedParentFolders;
	if ( Selection.GetSelectedOutlinerNodes().Num() > 0 )
	{
		for ( TSharedRef<FSequencerDisplayNode> SelectedNode : Selection.GetSelectedOutlinerNodes() )
		{
			TSharedPtr<FSequencerDisplayNode> CurrentNode = SelectedNode;
			while ( CurrentNode.IsValid() && CurrentNode->GetType() != ESequencerNode::Folder )
			{
				CurrentNode = CurrentNode->GetParent();
			}
			if ( CurrentNode.IsValid() )
			{
				SelectedParentFolders.Add( &StaticCastSharedPtr<FSequencerFolderNode>( CurrentNode )->GetFolder() );
			}
		}
	}

	TArray<FName> ExistingFolderNames;
	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();
	
	// If there is a folder selected the existing folder names are the sibling folders.
	if ( SelectedParentFolders.Num() == 1 )
	{
		for ( UMovieSceneFolder* SiblingFolder : SelectedParentFolders[0]->GetChildFolders() )
		{
			ExistingFolderNames.Add( SiblingFolder->GetFolderName() );
		}
	}
	// Otherwise use the root folders.
	else
	{
		for ( UMovieSceneFolder* MovieSceneFolder : FocusedMovieScene->GetRootFolders() )
		{
			ExistingFolderNames.Add( MovieSceneFolder->GetFolderName() );
		}
	}

	FName UniqueName = FSequencerUtilities::GetUniqueName(FName("New Folder"), ExistingFolderNames);
	UMovieSceneFolder* NewFolder = NewObject<UMovieSceneFolder>( FocusedMovieScene, NAME_None, RF_Transactional );
	NewFolder->SetFolderName( UniqueName );

	if ( SelectedParentFolders.Num() == 1 )
	{
		SelectedParentFolders[0]->Modify();
		SelectedParentFolders[0]->AddChildFolder( NewFolder );
	}
	else
	{
		FocusedMovieScene->GetRootFolders().Add( NewFolder );
	}

	NotifyMovieSceneDataChanged();
}


void FSequencer::TogglePlay()
{
	OnPlay();
}


void FSequencer::PlayForward()
{
	OnPlay(false);
}


void FSequencer::Rewind()
{
	OnStepToBeginning();
}


void FSequencer::StepForward()
{
	OnStepForward();
}


void FSequencer::StepBackward()
{
	OnStepBackward();
}


void FSequencer::StepToNextKey()
{
	SequencerWidget->StepToNextKey();
}


void FSequencer::StepToPreviousKey()
{
	SequencerWidget->StepToPreviousKey();
}


void FSequencer::StepToNextCameraKey()
{
	SequencerWidget->StepToNextCameraKey();
}


void FSequencer::StepToPreviousCameraKey()
{
	SequencerWidget->StepToPreviousCameraKey();
}


void FSequencer::ExpandAllNodesAndDescendants()
{
	const bool bExpandAll = true;
	SequencerWidget->GetTreeView()->ExpandNodes(ETreeRecursion::Recursive, bExpandAll);
}


void FSequencer::CollapseAllNodesAndDescendants()
{
	const bool bExpandAll = true;
	SequencerWidget->GetTreeView()->CollapseNodes(ETreeRecursion::Recursive, bExpandAll);
}


void FSequencer::ToggleExpandCollapseNodes()
{
	SequencerWidget->GetTreeView()->ToggleExpandCollapseNodes(ETreeRecursion::NonRecursive);
}


void FSequencer::ToggleExpandCollapseNodesAndDescendants()
{
	SequencerWidget->GetTreeView()->ToggleExpandCollapseNodes(ETreeRecursion::Recursive);
}


void FSequencer::SetKey()
{
	FScopedTransaction SetKeyTransaction( NSLOCTEXT("Sequencer", "SetKey_Transaction", "Set Key") );

	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		if (OutlinerNode->GetType() == ESequencerNode::Track)
		{
			TSharedRef<FSequencerTrackNode> TrackNode = StaticCastSharedRef<FSequencerTrackNode>(OutlinerNode);
						
			TSharedRef<FSequencerDisplayNode> ObjectBindingNode = OutlinerNode;
			if (SequencerHelpers::FindObjectBindingNode(TrackNode, ObjectBindingNode))
			{
				FGuid ObjectGuid = StaticCastSharedRef<FSequencerObjectBindingNode>(ObjectBindingNode)->GetObjectBinding();
				TrackNode->AddKey(ObjectGuid);
			}
		}
	}

	TSet<TSharedPtr<IKeyArea> > KeyAreas;
	for (auto OutlinerNode : Selection.GetSelectedOutlinerNodes())
	{
		SequencerHelpers::GetAllKeyAreas(OutlinerNode, KeyAreas);
	}

	if (KeyAreas.Num() > 0)
	{	
		for (auto KeyArea : KeyAreas)
		{
			if (KeyArea->GetOwningSection()->TryModify())
			{
				KeyArea->AddKeyUnique(GetGlobalTime(), GetKeyInterpolation());
			}
		}
	}

	UpdatePlaybackRange();
}


bool FSequencer::CanSetKeyTime() const
{
	return Selection.GetSelectedKeys().Num() > 0;
}


void FSequencer::SetKeyTime(const bool bUseFrames)
{
	TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();

	float KeyTime = 0.f;
	for ( const FSequencerSelectedKey& Key : SelectedKeysArray )
	{
		if (Key.IsValid())
		{
			KeyTime = Key.KeyArea->GetKeyTime(Key.KeyHandle.GetValue());
			break;
		}
	}

	float FrameRate = 1.0f / Settings->GetTimeSnapInterval();
	
	GenericTextEntryModeless(
		bUseFrames ? NSLOCTEXT("Sequencer.Popups", "SetKeyFramePopup", "New Frame") : NSLOCTEXT("Sequencer.Popups", "SetKeyTimePopup", "New Time"),
		bUseFrames ? FText::AsNumber( SequencerHelpers::TimeToFrame( KeyTime, FrameRate )) : FText::AsNumber( KeyTime ),
		FOnTextCommitted::CreateSP(this, &FSequencer::OnSetKeyTimeTextCommitted, bUseFrames)
	);
}


void FSequencer::OnSetKeyTimeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, const bool bUseFrames)
{
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		float FrameRate = 1.0f / Settings->GetTimeSnapInterval();
		double dNewTime = bUseFrames ? SequencerHelpers::FrameToTime(FCString::Atod(*InText.ToString()), FrameRate) : FCString::Atod(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric(); 
		if(!bIsNumber)
			return;

		const float NewKeyTime = (float)dNewTime;

		FScopedTransaction SetKeyTimeTransaction(NSLOCTEXT("Sequencer", "SetKeyTime_Transaction", "Set Key Time"));
		TArray<FSequencerSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys().Array();
	
		for ( const FSequencerSelectedKey& Key : SelectedKeysArray )
		{
			if (Key.IsValid())
			{
				if (Key.Section->TryModify())
				{
					Key.KeyArea->SetKeyTime(Key.KeyHandle.GetValue(), NewKeyTime);
				}
			}
		}
	}
}


void FSequencer::SelectTrackKeys(TWeakObjectPtr<UMovieSceneSection> Section, float KeyTime, bool bAddToSelection, bool bToggleSelection)
{
	if (!bAddToSelection && !bToggleSelection)
	{
		Selection.EmptySelectedKeys();
	}

	TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
	Sections.Add(Section);
	TArray<FSectionHandle> SectionHandles = SequencerWidget->GetSectionHandles(Sections);
	for (auto SectionHandle : SectionHandles)
	{
		if (SectionHandle.TrackNode.IsValid())
		{
			TSet<TSharedPtr<IKeyArea> > KeyAreas;
			SequencerHelpers::GetAllKeyAreas(SectionHandle.TrackNode, KeyAreas);

			for (auto KeyArea : KeyAreas)
			{
				if (KeyArea.IsValid())
				{
					TArray<FKeyHandle> KeyHandles = KeyArea.Get()->GetUnsortedKeyHandles();
					for (auto KeyHandle : KeyHandles)
					{
						float KeyHandleTime = KeyArea.Get()->GetKeyTime(KeyHandle);

						if (FMath::IsNearlyEqual(KeyHandleTime, KeyTime, KINDA_SMALL_NUMBER))
						{
							FSequencerSelectedKey SelectedKey(*Section.Get(), KeyArea, KeyHandle);

							if (bToggleSelection)
							{
								if (Selection.IsSelected(SelectedKey))
								{
									Selection.RemoveFromSelection(SelectedKey);
								}
								else
								{
									Selection.AddToSelection(SelectedKey);
								}
							}
							else
							{
								Selection.AddToSelection(SelectedKey);
							}
						}
					}
				}
			}
		}
	}

	SequencerHelpers::ValidateNodesWithSelectedKeysOrSections(*this);
}


TArray<TSharedPtr<FMovieSceneClipboard>> GClipboardStack;


void FSequencer::CopySelectedKeys()
{
	if (Selection.GetSelectedKeys().Num() == 0)
	{
		return;
	}

	TOptional<float> CopyRelativeTo;
	
	// Copy relative to the current key hotspot, if applicable
	if (Hotspot.IsValid() && Hotspot->GetType() == ESequencerHotspot::Key)
	{
		FKeyHotspot* KeyHotspot = StaticCastSharedPtr<FKeyHotspot>(Hotspot).Get();
		if (KeyHotspot->Key.KeyArea.IsValid() && KeyHotspot->Key.KeyHandle.IsSet())
		{
			CopyRelativeTo = KeyHotspot->Key.KeyArea->GetKeyTime(KeyHotspot->Key.KeyHandle.GetValue());
		}
	}

	FMovieSceneClipboardBuilder Builder;

	// Map selected keys to their key areas
	TMap<const IKeyArea*, TArray<FKeyHandle>> KeyAreaMap;
	for (const FSequencerSelectedKey& Key : Selection.GetSelectedKeys())
	{
		if (Key.KeyHandle.IsSet())
		{
			KeyAreaMap.FindOrAdd(Key.KeyArea.Get()).Add(Key.KeyHandle.GetValue());
		}
	}

	// Serialize each key area to the clipboard
	for (auto& Pair : KeyAreaMap)
	{
		Pair.Key->CopyKeys(Builder, [&](FKeyHandle Handle, const IKeyArea&){
			return Pair.Value.Contains(Handle);
		});
	}

	TSharedRef<FMovieSceneClipboard> Clipboard = MakeShareable( new FMovieSceneClipboard(Builder.Commit(CopyRelativeTo)) );
	
	if (Clipboard->GetKeyTrackGroups().Num())
	{
		GClipboardStack.Push(Clipboard);

		if (GClipboardStack.Num() > 10)
		{
			GClipboardStack.RemoveAt(0, 1);
		}
	}
}


void FSequencer::CutSelectedKeys()
{
	FScopedTransaction CutKeysTransaction( LOCTEXT("CutSelectedKeys_Transaction", "Cut Selected Keys") );
	CopySelectedKeys();
	DeleteSelectedKeys();
}


const TArray<TSharedPtr<FMovieSceneClipboard>>& FSequencer::GetClipboardStack() const
{
	return GClipboardStack;
}


void FSequencer::OnClipboardUsed(TSharedPtr<FMovieSceneClipboard> Clipboard)
{
	Clipboard->GetEnvironment().DateTime = FDateTime::UtcNow();

	// Last entry in the stack should be the most up-to-date
	GClipboardStack.Sort([](const TSharedPtr<FMovieSceneClipboard>& A, const TSharedPtr<FMovieSceneClipboard>& B){
		return A->GetEnvironment().DateTime < B->GetEnvironment().DateTime;
	});
}


void FSequencer::DiscardChanges()
{
	if (SequenceInstanceStack.Num() == 0)
	{
		return;
	}

	TSharedPtr<IToolkitHost> MyToolkitHost = GetToolkitHost();

	if (!MyToolkitHost.IsValid())
	{
		return;
	}

	UMovieSceneSequence* EditedSequence = SequenceInstanceStack.Top()->GetSequence();

	if (EditedSequence == nullptr)
	{
		return;
	}

	if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("RevertConfirm", "Are you sure you want to discard your current changes?")) != EAppReturnType::Yes)
	{
		return;
	}

	FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();
	UClass* SequenceClass = EditedSequence->GetClass();
	FString SequencePath = EditedSequence->GetPathName();
	UPackage* SequencePackage = EditedSequence->GetOutermost();

	// close asset editor
	AssetEditorManager.CloseAllEditorsForAsset(EditedSequence);

	// move existing objects into transient package
	UPackage* const TransientPackage = GetTransientPackage();
	TMap<FString, UObject*> MovedObjects;

	ForEachObjectWithOuter(SequencePackage, [&](UObject* Object) {
		const FName OldName = Object->GetFName();
		const FName NewName = MakeUniqueObjectName(TransientPackage, Object->GetClass(), OldName);
		Object->Rename(*NewName.ToString(), TransientPackage, REN_DontCreateRedirectors | REN_DoNotDirty);
		Object->SetFlags(RF_Transient);
		Object->ClearFlags(RF_Standalone | RF_Transactional);
		MovedObjects.Add(Object->GetPathName(), Object);
	}, true);

	// unload package
	TArray<UPackage*> PackagesToUnload;
	{
		PackagesToUnload.Add(SequencePackage);
	}

	FText PackageUnloadError;
	PackageTools::UnloadPackages(PackagesToUnload, PackageUnloadError);

	if (!PackageUnloadError.IsEmpty())
	{
		ResetLoaders(SequencePackage);
		SequencePackage->ClearFlags(RF_WasLoaded);
		SequencePackage->bHasBeenFullyLoaded = false;
		SequencePackage->GetMetaData()->RemoveMetaDataOutsidePackage();
	}

	// reload package
	TMap<UObject*, UObject*> MovedToReloadedObjectMap;

	for (const auto MovedObject : MovedObjects)
	{
		UObject* ReloadedObject = StaticLoadObject(MovedObject.Value->GetClass(), nullptr, *MovedObject.Key);
		check(ReloadedObject != nullptr);
		MovedToReloadedObjectMap.Add(MovedObject.Value, ReloadedObject);
	}

	for (TObjectIterator<UObject> It; It; ++It)
	{
		// @todo sequencer: only process objects that actually reference the package?
		FArchiveReplaceObjectRef<UObject> Ar(*It, MovedToReloadedObjectMap, false, false, false, false);
	}

	auto ReloadedSequence = Cast<UMovieSceneSequence>(StaticLoadObject(SequenceClass, nullptr, *SequencePath));
	SequencePackage->SetDirtyFlag(false);

	// release transient objects
	for (auto MovedObject : MovedObjects)
	{
		MovedObject.Value->RemoveFromRoot();
		MovedObject.Value->MarkPendingKill();
	}

	// clear undo buffer
	if (true) // @todo sequencer: check whether objects are actually referenced in undo buffer
	{
		GEditor->Trans->Reset(LOCTEXT("UnloadedSequence", "Unloaded Sequence"));
	}

	// reopen asset editor
	TArray<UObject*> AssetsToReopen;
	AssetsToReopen.Add(ReloadedSequence);
	AssetEditorManager.OpenEditorForAssets(AssetsToReopen, EToolkitMode::Standalone, MyToolkitHost.ToSharedRef());
}


void FSequencer::FixActorReferences()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

	TMap<FString, AActor*> ActorNameToActorMap;
	for ( TActorIterator<AActor> ActorItr( GWorld ); ActorItr; ++ActorItr )
	{
		// Same as with the Object Iterator, access the subclass instance with the * or -> operators.
		AActor *Actor = *ActorItr;
		ActorNameToActorMap.Add( Actor->GetActorLabel(), Actor);
	}

	// Cache the possessables to fix up first since the bindings will change as the fix ups happen.
	TArray<FMovieScenePossessable> ActorsPossessablesToFix;
	for ( int32 i = 0; i < FocusedMovieScene->GetPossessableCount(); i++ )
	{
		FMovieScenePossessable& Possessable = FocusedMovieScene->GetPossessable( i );
		// Possesbles with parents are components so ignore them.
		if ( Possessable.GetParent().IsValid() == false )
		{
			TArray<TWeakObjectPtr<UObject>> RuntimeObjects;
			GetRuntimeObjects( GetFocusedMovieSceneSequenceInstance(), Possessable.GetGuid(), RuntimeObjects );
			if ( RuntimeObjects.Num() == 0 )
			{
				ActorsPossessablesToFix.Add( Possessable );
			}
		}
	}

	// For the posseables to fix, look up the actors by name and reassign them if found.
	for ( const FMovieScenePossessable& ActorPossessableToFix : ActorsPossessablesToFix )
	{
		AActor** ActorPtr = ActorNameToActorMap.Find( ActorPossessableToFix.GetName() );
		if ( ActorPtr != nullptr )
		{
			DoAssignActor( ActorPtr, 1, ActorPossessableToFix.GetGuid() );
		}
	}
}


void FSequencer::GenericTextEntryModeless(const FText& DialogText, const FText& DefaultText, FOnTextCommitted OnTextComitted)
{
	TSharedRef<STextEntryPopup> TextEntryPopup = 
		SNew(STextEntryPopup)
		.Label(DialogText)
		.DefaultText(DefaultText)
		.OnTextCommitted(OnTextComitted)
		.ClearKeyboardFocusOnCommit(false)
		.SelectAllTextWhenFocused(true)
		.MaxWidth(1024.0f);

	EntryPopupMenu = FSlateApplication::Get().PushMenu(
		ToolkitHost.Pin()->GetParentWidget(),
		FWidgetPath(),
		TextEntryPopup,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
	);
}


void FSequencer::CloseEntryPopupMenu()
{
	if (EntryPopupMenu.IsValid())
	{
		EntryPopupMenu.Pin()->Dismiss();
	}
}


void FSequencer::TrimSection(bool bTrimLeft)
{
	FScopedTransaction TrimSectionTransaction( NSLOCTEXT("Sequencer", "TrimSection_Transaction", "Trim Section") );
	MovieSceneToolHelpers::TrimSection(Selection.GetSelectedSections(), GetGlobalTime(), bTrimLeft);
	NotifyMovieSceneDataChanged();
}


void FSequencer::SplitSection()
{
	FScopedTransaction SplitSectionTransaction( NSLOCTEXT("Sequencer", "SplitSection_Transaction", "Split Section") );
	MovieSceneToolHelpers::SplitSection(Selection.GetSelectedSections(), GetGlobalTime());
	NotifyMovieSceneDataChanged();
}

const ISequencerEditTool* FSequencer::GetEditTool() const
{
	return SequencerWidget->GetEditTool();
}

TSharedPtr<ISequencerHotspot> FSequencer::GetHotspot() const
{
	return Hotspot;
}

void FSequencer::SetHotspot(TSharedPtr<ISequencerHotspot> NewHotspot)
{
	Hotspot = MoveTemp(NewHotspot);
}

void FSequencer::BindSequencerCommands()
{
	const FSequencerCommands& Commands = FSequencerCommands::Get();

	SequencerCommandBindings->MapAction(
		Commands.StepToNextKey,
		FExecuteAction::CreateSP( this, &FSequencer::StepToNextKey ) );

	SequencerCommandBindings->MapAction(
		Commands.StepToPreviousKey,
		FExecuteAction::CreateSP( this, &FSequencer::StepToPreviousKey ) );

	SequencerCommandBindings->MapAction(
		Commands.StepToNextCameraKey,
		FExecuteAction::CreateSP( this, &FSequencer::StepToNextCameraKey ) );

	SequencerCommandBindings->MapAction(
		Commands.StepToPreviousCameraKey,
		FExecuteAction::CreateSP( this, &FSequencer::StepToPreviousCameraKey ) );

	SequencerCommandBindings->MapAction(
		Commands.SetStartPlaybackRange,
		FExecuteAction::CreateSP( this, &FSequencer::SetPlaybackRangeStart ) );

	SequencerCommandBindings->MapAction(
		Commands.ResetViewRange,
		FExecuteAction::CreateSP( this, &FSequencer::ResetViewRange ) );

	SequencerCommandBindings->MapAction(
		Commands.ZoomInViewRange,
		FExecuteAction::CreateSP( this, &FSequencer::ZoomInViewRange ),
		FCanExecuteAction(),
		EUIActionRepeatMode::RepeatEnabled );

	SequencerCommandBindings->MapAction(
		Commands.ZoomOutViewRange,
		FExecuteAction::CreateSP( this, &FSequencer::ZoomOutViewRange ),		
		FCanExecuteAction(),
		EUIActionRepeatMode::RepeatEnabled );

	SequencerCommandBindings->MapAction(
		Commands.SetEndPlaybackRange,
		FExecuteAction::CreateSP( this, &FSequencer::SetPlaybackRangeEnd ) );

	SequencerCommandBindings->MapAction(
		Commands.ExpandAllNodesAndDescendants,
		FExecuteAction::CreateSP(this, &FSequencer::ExpandAllNodesAndDescendants));

	SequencerCommandBindings->MapAction(
		Commands.CollapseAllNodesAndDescendants,
		FExecuteAction::CreateSP(this, &FSequencer::CollapseAllNodesAndDescendants));

	SequencerCommandBindings->MapAction(
		Commands.ToggleExpandCollapseNodes,
		FExecuteAction::CreateSP(this, &FSequencer::ToggleExpandCollapseNodes));

	SequencerCommandBindings->MapAction(
		Commands.ToggleExpandCollapseNodesAndDescendants,
		FExecuteAction::CreateSP(this, &FSequencer::ToggleExpandCollapseNodesAndDescendants));

	SequencerCommandBindings->MapAction(
		Commands.SetKey,
		FExecuteAction::CreateSP( this, &FSequencer::SetKey ) );

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationCubicAuto,
		FExecuteAction::CreateSP( this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Cubic, ERichCurveTangentMode::RCTM_Auto ) );

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationCubicUser,
		FExecuteAction::CreateSP( this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Cubic, ERichCurveTangentMode::RCTM_User ) );

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationCubicBreak,
		FExecuteAction::CreateSP( this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Cubic, ERichCurveTangentMode::RCTM_Break ) );

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationLinear,
		FExecuteAction::CreateSP( this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Linear, ERichCurveTangentMode::RCTM_Auto ) );

	SequencerCommandBindings->MapAction(
		Commands.SetInterpolationConstant,
		FExecuteAction::CreateSP( this, &FSequencer::SetInterpTangentMode, ERichCurveInterpMode::RCIM_Constant, ERichCurveTangentMode::RCTM_Auto ) );

	SequencerCommandBindings->MapAction(
		Commands.TrimSectionLeft,
		FExecuteAction::CreateSP( this, &FSequencer::TrimSection, true ) );

	SequencerCommandBindings->MapAction(
		Commands.TrimSectionRight,
		FExecuteAction::CreateSP( this, &FSequencer::TrimSection, false ) );

	SequencerCommandBindings->MapAction(
		Commands.SplitSection,
		FExecuteAction::CreateSP( this, &FSequencer::SplitSection ) );

	// We can convert to spawnables if anything selected is a root-level possessable
	auto CanConvertToSpawnables = [this]{
		UMovieScene* MovieScene = GetFocusedMovieSceneSequence()->GetMovieScene();

		for (const TSharedRef<FSequencerDisplayNode>& Node : Selection.GetSelectedOutlinerNodes())
		{
			if (Node->GetType() == ESequencerNode::Object)
			{
				FMovieScenePossessable* Possessable = MovieScene->FindPossessable(static_cast<FSequencerObjectBindingNode&>(*Node).GetObjectBinding());
				if (Possessable && !Possessable->GetParent().IsValid())
				{
					return true;
				}
			}
		}
		return false;
	};
	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().ConvertToSpawnable,
		FExecuteAction::CreateSP(this, &FSequencer::ConvertSelectedNodesToSpawnables),
		FCanExecuteAction::CreateLambda(CanConvertToSpawnables)
	);

	SequencerCommandBindings->MapAction(
		Commands.SetAutoKeyModeAll,
		FExecuteAction::CreateLambda( [this]{ Settings->SetAutoKeyMode( EAutoKeyMode::KeyAll ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetAutoKeyMode() == EAutoKeyMode::KeyAll; } ) );

	SequencerCommandBindings->MapAction(
		Commands.SetAutoKeyModeAnimated,
		FExecuteAction::CreateLambda([this] { Settings->SetAutoKeyMode(EAutoKeyMode::KeyAnimated); }),
		FCanExecuteAction::CreateLambda([] { return true; }),
		FIsActionChecked::CreateLambda([this] { return Settings->GetAutoKeyMode() == EAutoKeyMode::KeyAnimated; }));

	SequencerCommandBindings->MapAction(
		Commands.SetAutoKeyModeNone,
		FExecuteAction::CreateLambda([this] { Settings->SetAutoKeyMode(EAutoKeyMode::KeyNone); }),
		FCanExecuteAction::CreateLambda([] { return true; }),
		FIsActionChecked::CreateLambda([this] { return Settings->GetAutoKeyMode() == EAutoKeyMode::KeyNone; }));

	SequencerCommandBindings->MapAction(
		Commands.ToggleKeyAllEnabled,
		FExecuteAction::CreateLambda( [this]{ Settings->SetKeyAllEnabled( !Settings->GetKeyAllEnabled() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetKeyAllEnabled(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleAutoScroll,
		FExecuteAction::CreateLambda( [this]{ Settings->SetAutoScrollEnabled( !Settings->GetAutoScrollEnabled() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetAutoScrollEnabled(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.FindInContentBrowser,
		FExecuteAction::CreateSP( this, &FSequencer::FindInContentBrowser ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleCombinedKeyframes,
		FExecuteAction::CreateLambda( [this]{
			Settings->SetShowCombinedKeyframes( !Settings->GetShowCombinedKeyframes() );
		} ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetShowCombinedKeyframes(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleChannelColors,
		FExecuteAction::CreateLambda( [this]{
			Settings->SetShowChannelColors( !Settings->GetShowChannelColors() );
		} ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetShowChannelColors(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleLabelBrowser,
		FExecuteAction::CreateLambda( [this]{
			Settings->SetLabelBrowserVisible( !Settings->GetLabelBrowserVisible() );
		} ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetLabelBrowserVisible(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleShowFrameNumbers,
		FExecuteAction::CreateLambda( [this]{ Settings->SetShowFrameNumbers( !Settings->GetShowFrameNumbers() ); } ),
		FCanExecuteAction::CreateSP( this, &FSequencer::CanShowFrameNumbers ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetShowFrameNumbers(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleShowRangeSlider,
		FExecuteAction::CreateLambda( [this]{ Settings->SetShowRangeSlider( !Settings->GetShowRangeSlider() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetShowRangeSlider(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleIsSnapEnabled,
		FExecuteAction::CreateLambda( [this]{ Settings->SetIsSnapEnabled( !Settings->GetIsSnapEnabled() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetIsSnapEnabled(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapKeyTimesToInterval,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapKeyTimesToInterval( !Settings->GetSnapKeyTimesToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapKeyTimesToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapKeyTimesToKeys,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapKeyTimesToKeys( !Settings->GetSnapKeyTimesToKeys() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapKeyTimesToKeys(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapSectionTimesToInterval,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapSectionTimesToInterval( !Settings->GetSnapSectionTimesToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapSectionTimesToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapSectionTimesToSections,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapSectionTimesToSections( !Settings->GetSnapSectionTimesToSections() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapSectionTimesToSections(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapPlayTimeToKeys,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapPlayTimeToKeys( !Settings->GetSnapPlayTimeToKeys() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapPlayTimeToKeys(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapPlayTimeToInterval,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapPlayTimeToInterval( !Settings->GetSnapPlayTimeToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapPlayTimeToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapPlayTimeToDraggedKey,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapPlayTimeToDraggedKey( !Settings->GetSnapPlayTimeToDraggedKey() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapPlayTimeToDraggedKey(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleFixedTimeStepPlayback,
		FExecuteAction::CreateLambda( [this]{ Settings->SetFixedTimeStepPlayback( !Settings->GetFixedTimeStepPlayback() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetFixedTimeStepPlayback(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapCurveValueToInterval,
		FExecuteAction::CreateLambda( [this]{ Settings->SetSnapCurveValueToInterval( !Settings->GetSnapCurveValueToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetSnapCurveValueToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleShowCurveEditor,
		FExecuteAction::CreateLambda( [this]{ Settings->SetShowCurveEditor(!Settings->GetShowCurveEditor()); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetShowCurveEditor(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleLinkCurveEditorTimeRange,
		FExecuteAction::CreateLambda( [this]{ Settings->SetLinkCurveEditorTimeRange(!Settings->GetLinkCurveEditorTimeRange()); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->GetLinkCurveEditorTimeRange(); } ) );

	auto CanCutOrCopy = [this]{
		UMovieSceneTrack* Track = nullptr;
		for (FSequencerSelectedKey Key : Selection.GetSelectedKeys())
		{
			if (!Track)
			{
				Track = Key.Section->GetTypedOuter<UMovieSceneTrack>();
			}
			if (!Track || Track != Key.Section->GetTypedOuter<UMovieSceneTrack>())
			{
				return false;
			}
		}
		return true;
	};

	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Cut,
		FExecuteAction::CreateSP(this, &FSequencer::CutSelectedKeys),
		FCanExecuteAction::CreateLambda(CanCutOrCopy)
	);

	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &FSequencer::CopySelectedKeys),
		FCanExecuteAction::CreateLambda(CanCutOrCopy)
	);

	SequencerCommandBindings->MapAction(
		Commands.ToggleKeepCursorInPlaybackRange,
		FExecuteAction::CreateLambda( [this]{ Settings->SetKeepCursorInPlayRange( !Settings->ShouldKeepCursorInPlayRange() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->ShouldKeepCursorInPlayRange(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleKeepPlaybackRangeInSectionBounds,
		FExecuteAction::CreateLambda( [this]{ Settings->SetKeepPlayRangeInSectionBounds( !Settings->ShouldKeepPlayRangeInSectionBounds() ); NotifyMovieSceneDataChanged(); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [this]{ return Settings->ShouldKeepPlayRangeInSectionBounds(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.RenderMovie,
		FExecuteAction::CreateLambda([this]{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

			// Create a new movie scene capture object for an automated level sequence, and open the tab
			UAutomatedLevelSequenceCapture* MovieSceneCapture = NewObject<UAutomatedLevelSequenceCapture>(GetTransientPackage(), UAutomatedLevelSequenceCapture::StaticClass(), NAME_None, RF_Transient);
			MovieSceneCapture->LoadConfig();

			MovieSceneCapture->SetLevelSequenceAsset(GetCurrentAsset()->GetPathName());

			if (CanShowFrameNumbers())
			{
				MovieSceneCapture->Settings.FrameRate = FMath::RoundToInt(1.f / Settings->GetTimeSnapInterval());
				MovieSceneCapture->Settings.bUseRelativeFrameNumbers = false;

				const int32 SequenceStartFrame = FMath::RoundToInt( GetPlaybackRange().GetLowerBoundValue() * MovieSceneCapture->Settings.FrameRate );
				const int32 SequenceEndFrame = FMath::Max( SequenceStartFrame, FMath::RoundToInt( GetPlaybackRange().GetUpperBoundValue() * MovieSceneCapture->Settings.FrameRate ) );

				MovieSceneCapture->StartFrame = SequenceStartFrame;
				MovieSceneCapture->EndFrame = SequenceEndFrame;
			}

			IMovieSceneCaptureDialogModule::Get().OpenDialog(LevelEditorModule.GetLevelEditorTabManager().ToSharedRef(), MovieSceneCapture);
		})
	);

	SequencerCommandBindings->MapAction(
		Commands.DiscardChanges,
		FExecuteAction::CreateSP(this, &FSequencer::DiscardChanges),
		FCanExecuteAction::CreateLambda([this]{
			if (SequenceInstanceStack.Num() == 0)
			{
				return false;
			}

			UMovieSceneSequence* EditedSequence = SequenceInstanceStack.Top()->GetSequence();
			UPackage* EditedPackage = EditedSequence->GetOutermost();

			return ((EditedSequence != nullptr) && (EditedPackage->FileSize != 0) && EditedPackage->IsDirty());
		})
	);

	SequencerCommandBindings->MapAction(
		Commands.FixActorReferences,
		FExecuteAction::CreateSP( this, &FSequencer::FixActorReferences ),
		FCanExecuteAction::CreateLambda( []{ return true; } ) );

	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		TrackEditors[i]->BindCommands(SequencerCommandBindings);
	}

	// copy subset of sequencer commands to shared commands
	*SequencerSharedBindings = *SequencerCommandBindings;

	// Sequencer-only bindings
	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP( this, &FSequencer::DeleteSelectedItems ));

	SequencerCommandBindings->MapAction(
		Commands.TogglePlay,
		FExecuteAction::CreateSP( this, &FSequencer::TogglePlay ));

	SequencerCommandBindings->MapAction(
		Commands.PlayForward,
		FExecuteAction::CreateSP( this, &FSequencer::PlayForward ));

	SequencerCommandBindings->MapAction(
		Commands.Rewind,
		FExecuteAction::CreateSP( this, &FSequencer::Rewind ));

	SequencerCommandBindings->MapAction(
		Commands.StepForward,
		FExecuteAction::CreateSP( this, &FSequencer::StepForward ),
		EUIActionRepeatMode::RepeatEnabled );

	SequencerCommandBindings->MapAction(
		Commands.StepBackward,
		FExecuteAction::CreateSP( this, &FSequencer::StepBackward ),
		EUIActionRepeatMode::RepeatEnabled );

	SequencerCommandBindings->MapAction(
		Commands.SetInOutEnd,
		FExecuteAction::CreateLambda([this]{ SetInOutRangeEnd(); }));

	SequencerCommandBindings->MapAction(
		Commands.SetInOutStart,
		FExecuteAction::CreateLambda([this]{ SetInOutRangeStart(); }));

	SequencerWidget->BindCommands(SequencerCommandBindings);
}


void FSequencer::BuildAddTrackMenu(class FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT( "AddFolder", "Add Folder" ),
		LOCTEXT( "AddFolderToolTip", "Adds a new folder." ),
		FSlateIcon( FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetTreeFolderOpen" ),
		FUIAction( FExecuteAction::CreateRaw( this, &FSequencer::OnAddFolder ) ) );

	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		TrackEditors[i]->BuildAddTrackMenu(MenuBuilder);
	}
}


void FSequencer::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		TrackEditors[i]->BuildObjectBindingTrackMenu(MenuBuilder, ObjectBinding, ObjectClass);
	}
}


void FSequencer::BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		TrackEditors[i]->BuildObjectBindingEditButtons(EditBox, ObjectBinding, ObjectClass);
	}
}


#undef LOCTEXT_NAMESPACE
