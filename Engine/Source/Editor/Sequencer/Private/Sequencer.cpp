// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "Sequencer.h"
#include "ISequencerObjectBindingManager.h"
#include "MovieScene.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/SequencerWidgets/Public/ITimeSlider.h"
#include "Editor/EditorWidgets/Public/ITransportControl.h"
#include "Editor/EditorWidgets/Public/EditorWidgetsModule.h"
#include "Editor/LevelEditor/Public/ILevelEditor.h"
#include "Editor/LevelEditor/Public/ILevelViewport.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "SSequencer.h"
#include "ScopedTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintEditorModule.h"
#include "MovieSceneTrack.h"
#include "MovieSceneDirectorTrack.h"
#include "MovieSceneAudioTrack.h"
#include "MovieSceneAnimationTrack.h"
#include "MovieSceneTrackEditor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_PlayMovieScene.h"
#include "AssetSelection.h"
#include "ScopedTransaction.h"
#include "MovieSceneShotSection.h"
#include "SubMovieSceneTrack.h"
#include "ISequencerSection.h"
#include "MovieSceneInstance.h"
#include "IKeyArea.h"
#include "SnappingUtils.h"
#include "STextEntryPopup.h"
#include "GenericCommands.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Selection.h"

#define LOCTEXT_NAMESPACE "Sequencer"

DEFINE_LOG_CATEGORY(LogSequencer);

bool FSequencer::IsSequencerEnabled()
{
	return true;
}


void FSequencer::InitSequencer( const FSequencerInitParams& InitParams, const TArray<FOnCreateTrackEditor>& TrackEditorDelegates )
{
	if( IsSequencerEnabled() )
	{
		bIsEditingWithinLevelEditor = InitParams.bEditWithinLevelEditor;

		ToolkitHost = InitParams.ToolkitHost;

		LastViewRange = InitParams.ViewParams.InitalViewRange;
		ScrubPosition = InitParams.ViewParams.InitialScrubPosition;

		ObjectChangeListener = InitParams.ObjectChangeListener;
		ObjectBindingManager = InitParams.ObjectBindingManager;

		check( ObjectChangeListener.IsValid() );
		check( ObjectBindingManager.IsValid() );
		
		UMovieScene& RootMovieScene = *InitParams.RootMovieScene;
		RootMovieScene.SetFlags( RF_Transactional );
		
		// Focusing the initial movie scene needs to be done before the first time GetFocusedMovieSceneInstane or GetRootMovieSceneInstance is used
		RootMovieSceneInstance = MakeShareable( new FMovieSceneInstance( RootMovieScene ) );
		MovieSceneStack.Add( RootMovieSceneInstance.ToSharedRef() );

		// Make internal widgets
		SequencerWidget = SNew( SSequencer, SharedThis( this ) )
			.ViewRange( this, &FSequencer::OnGetViewRange )
			.ScrubPosition( this, &FSequencer::OnGetScrubPosition )
			.OnScrubPositionChanged( this, &FSequencer::OnScrubPositionChanged )
			.OnViewRangeChanged( this, &FSequencer::OnViewRangeChanged, false );

		// When undo occurs, get a notification so we can make sure our view is up to date
		GEditor->RegisterForUndo(this);

		if( bIsEditingWithinLevelEditor )
		{
			// @todo remove when world-centric mode is added
			// Hook into the editor's mechanism for checking whether we need live capture of PIE/SIE actor state
			GEditor->GetActorRecordingState().AddSP(this, &FSequencer::GetActorRecordingState);
		}

		// Create tools and bind them to this sequencer
		for( int32 DelegateIndex = 0; DelegateIndex < TrackEditorDelegates.Num(); ++DelegateIndex )
		{
			check( TrackEditorDelegates[DelegateIndex].IsBound() );
			// Tools may exist in other modules, call a delegate that will create one for us 
			TSharedRef<FMovieSceneTrackEditor> TrackEditor = TrackEditorDelegates[DelegateIndex].Execute( SharedThis( this ) );

			// Keep track of certain editors
			if ( TrackEditor->SupportsType( UMovieSceneDirectorTrack::StaticClass() ) )
			{
				DirectorTrackEditor = TrackEditor;
			}
			else if ( TrackEditor->SupportsType( UMovieSceneAnimationTrack::StaticClass() ) )
			{
				AnimationTrackEditor = TrackEditor;
			}

			TrackEditors.Add( TrackEditor );
		}



		ZoomAnimation = FCurveSequence();
		ZoomCurve = ZoomAnimation.AddCurve(0.f, 0.35f, ECurveEaseFunction::QuadIn);
		OverlayAnimation = FCurveSequence();
		OverlayCurve = OverlayAnimation.AddCurve(0.f, 0.35f, ECurveEaseFunction::QuadIn);

		// Update initial movie scene data
		NotifyMovieSceneDataChanged();

		// NOTE: Could fill in asset editor commands here!

		BindSequencerCommands();
	}
}


FSequencer::FSequencer()
	: SequencerCommandBindings( new FUICommandList )
	, TargetViewRange(0.f, 5.f)
	, LastViewRange(0.f, 5.f)
	, PlaybackState( EMovieScenePlayerStatus::Stopped )
	, ScrubPosition( 0.0f )
	, bLoopingEnabled( false )
	, bAllowAutoKey( false )
	, bPerspectiveViewportPossessionEnabled( true )
	, bIsEditingWithinLevelEditor( false )
	, bNeedTreeRefresh( false )
{

}

FSequencer::~FSequencer()
{
	GEditor->GetActorRecordingState().RemoveAll( this );

	GEditor->UnregisterForUndo( this );

	DestroySpawnablesForAllMovieScenes();
	
	ObjectBindingManager.Reset();

	TrackEditors.Empty();

	SequencerWidget.Reset();
}

void FSequencer::Tick(float InDeltaTime)
{
	if (bNeedTreeRefresh)
	{
		// @todo - Sequencer Will be called too often
		UpdateRuntimeInstances();

		SequencerWidget->UpdateLayoutTree();
		bNeedTreeRefresh = false;
	}

	float NewTime = GetGlobalTime() + InDeltaTime;
	if (PlaybackState == EMovieScenePlayerStatus::Playing ||
		PlaybackState == EMovieScenePlayerStatus::Recording)
	{
		TRange<float> TimeBounds = GetTimeBounds();
		if (!TimeBounds.IsEmpty())
		{
			if (NewTime > TimeBounds.GetUpperBoundValue())
			{
				if (bLoopingEnabled)
				{
					NewTime -= TimeBounds.Size<float>();
				}
				else
				{
					NewTime = TimeBounds.GetUpperBoundValue();
					PlaybackState = EMovieScenePlayerStatus::Stopped;
				}
			}

			if (NewTime < TimeBounds.GetLowerBoundValue())
			{
				NewTime = TimeBounds.GetLowerBoundValue();
			}

			SetGlobalTime(NewTime);
		}
		else
		{
			// no bounds at all, stop playing
			PlaybackState = EMovieScenePlayerStatus::Stopped;
		}
	}

	// Tick all the tools we own as well
	for (int32 EditorIndex = 0; EditorIndex < TrackEditors.Num(); ++EditorIndex)
	{
		TrackEditors[EditorIndex]->Tick(InDeltaTime);
	}
}

TArray< UMovieScene* > FSequencer::GetMovieScenesBeingEdited()
{
	TArray<UMovieScene* > OutMovieScenes;

	// Get the root movie scene
	OutMovieScenes.Add( RootMovieSceneInstance->GetMovieScene() );

	// Get Sub-MovieScenes
	for( auto It = MovieSceneSectionToInstanceMap.CreateConstIterator(); It; ++It )
	{
		OutMovieScenes.AddUnique( It.Value()->GetMovieScene() );
	}

	return OutMovieScenes;
}

UMovieScene* FSequencer::GetRootMovieScene() const
{
	return MovieSceneStack[0]->GetMovieScene();
}

UMovieScene* FSequencer::GetFocusedMovieScene() const
{
	// the last item is the focused movie scene
	return MovieSceneStack.Top()->GetMovieScene();
}

void FSequencer::ResetToNewRootMovieScene( UMovieScene& NewRoot, TSharedRef<ISequencerObjectBindingManager> NewObjectBindingManager )
{
	DestroySpawnablesForAllMovieScenes();

	//@todo Sequencer - Encapsulate this better
	MovieSceneStack.Empty();
	Selection.Empty();
	FilteringShots.Empty();
	UnfilterableSections.Empty();
	UnfilterableObjects.Empty();
	MovieSceneSectionToInstanceMap.Empty();

	NewRoot.SetFlags(RF_Transactional);

	ObjectBindingManager = NewObjectBindingManager;

	// Focusing the initial movie scene needs to be done before the first time GetFocusedMovieSceneInstane or GetRootMovieSceneInstance is used
	RootMovieSceneInstance = MakeShareable(new FMovieSceneInstance(NewRoot));
	MovieSceneStack.Add(RootMovieSceneInstance.ToSharedRef());

	SequencerWidget->ResetBreadcrumbs();

	NotifyMovieSceneDataChanged();
}

TSharedRef<FMovieSceneInstance> FSequencer::GetRootMovieSceneInstance() const
{
	return MovieSceneStack[0];
}


TSharedRef<FMovieSceneInstance> FSequencer::GetFocusedMovieSceneInstance() const
{
	// the last item is the focused movie scene
	return MovieSceneStack.Top();
}


void FSequencer::FocusSubMovieScene( TSharedRef<FMovieSceneInstance> SubMovieSceneInstance )
{
	// Check for infinite recursion
	check( SubMovieSceneInstance != MovieSceneStack.Top() );

	// Focus the movie scene
	MovieSceneStack.Push( SubMovieSceneInstance );

	// Reset data that is only used for the previous movie scene
	ResetPerMovieSceneData();

	// Update internal data for the new movie scene
	NotifyMovieSceneDataChanged();
}

TSharedRef<FMovieSceneInstance> FSequencer::GetInstanceForSubMovieSceneSection( UMovieSceneSection& SubMovieSceneSection ) const
{
	return MovieSceneSectionToInstanceMap.FindChecked( &SubMovieSceneSection );
}

void FSequencer::PopToMovieScene( TSharedRef<FMovieSceneInstance> SubMovieSceneInstance )
{
	if( MovieSceneStack.Num() > 1 )
	{
		// Pop until we find the movie scene to focus
		while( SubMovieSceneInstance != MovieSceneStack.Last() )
		{
			MovieSceneStack.Pop();
		}
	
		check( MovieSceneStack.Num() > 0 );

		ResetPerMovieSceneData();

		NotifyMovieSceneDataChanged();
	}
}

void FSequencer::AddNewShot(FGuid CameraGuid)
{
	if (DirectorTrackEditor.IsValid())
	{
		DirectorTrackEditor.Pin()->AddKey(CameraGuid);
	}
}

void FSequencer::AddAnimation(FGuid ObjectGuid, class UAnimSequence* AnimSequence)
{
	if (AnimationTrackEditor.IsValid())
	{
		AnimationTrackEditor.Pin()->AddKey(ObjectGuid, AnimSequence);
	}
}

void FSequencer::RenameShot(UMovieSceneSection* ShotSection)
{
	auto ActualShotSection = CastChecked<UMovieSceneShotSection>(ShotSection);

	TSharedRef<STextEntryPopup> TextEntry = 
		SNew(STextEntryPopup)
		.Label(NSLOCTEXT("Sequencer", "RenameShotHeader", "Name"))
		.DefaultText( ActualShotSection->GetTitle() )
		.OnTextCommitted(this, &FSequencer::RenameShotCommitted, ShotSection)
		.ClearKeyboardFocusOnCommit( false );
	
	NameEntryPopupWindow = FSlateApplication::Get().PushMenu(
		SequencerWidget.ToSharedRef(),
		TextEntry,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup )
		);
}

void FSequencer::DeleteSection(class UMovieSceneSection* Section)
{
	UMovieScene* MovieScene = GetFocusedMovieScene();
	
	bool bAnythingRemoved = false;

	UMovieSceneTrack* Track = CastChecked<UMovieSceneTrack>( Section->GetOuter() );

	// If this check fails then the section is outered to a type that doesnt know about the section
	//checkSlow( Track->HasSection(Section) );
	
	Track->SetFlags( RF_Transactional );

	FScopedTransaction DeleteSectionTransaction( NSLOCTEXT("Sequencer", "DeleteSection_Transaction", "Delete Section") );
	
	Track->Modify();

	Track->RemoveSection(Section);

	bAnythingRemoved = true;
	
	if( bAnythingRemoved )
	{
		UpdateRuntimeInstances();
	}
}

void FSequencer::DeleteSelectedKeys()
{
	FScopedTransaction DeleteKeysTransaction( NSLOCTEXT("Sequencer", "DeleteSelectedKeys_Transaction", "Delete Selected Keys") );
	bool bAnythingRemoved = false;
	TArray<FSelectedKey> SelectedKeysArray = Selection.GetSelectedKeys()->Array();

	for ( const FSelectedKey& Key : SelectedKeysArray )
	{
		if (Key.IsValid())
		{
			Key.Section->Modify();
			Key.KeyArea->DeleteKey(Key.KeyHandle.GetValue());
			bAnythingRemoved = true;
		}
	}

	if (bAnythingRemoved)
	{
		UpdateRuntimeInstances();
	}
}

void FSequencer::RenameShotCommitted(const FText& RenameText, ETextCommit::Type CommitInfo, UMovieSceneSection* Section)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		auto ShotSection = CastChecked<UMovieSceneShotSection>(Section);
		ShotSection->SetTitle(RenameText);
	}

	if (NameEntryPopupWindow.IsValid())
	{
		NameEntryPopupWindow.Pin()->RequestDestroyWindow();
	}
}

void FSequencer::SpawnOrDestroyPuppetObjects( TSharedRef<FMovieSceneInstance> MovieSceneInstance )
{
	if( ObjectBindingManager->AllowsSpawnableObjects() )
	{
		const bool bDestroyAll = false;
		ObjectBindingManager->SpawnOrDestroyObjectsForInstance( MovieSceneInstance, bDestroyAll );
	}
}

void FSequencer::OnMapChanged( class UWorld* NewWorld, EMapChangeType::Type MapChangeType )
{
	// @todo Sequencer Sub-MovieScenes Needs more investigation of what to spawn
	// Destroy our puppets because the world is going away.  We probably don't have to do this (the actors will
	// be naturally destroyed with the level, but we might as well.)
	DestroySpawnablesForAllMovieScenes();


	// @todo sequencer: We should only wipe/respawn puppets that are affected by the world that is being changed! (multi-UWorld support)
	if( ( MapChangeType == EMapChangeType::LoadMap || MapChangeType == EMapChangeType::NewMap ) )
	{
		SpawnOrDestroyPuppetObjects( GetFocusedMovieSceneInstance() );
	}
}

void FSequencer::OnActorsDropped( const TArray<TWeakObjectPtr<AActor> >& Actors )
{
	bool bPossessableAdded = false;
	for( TWeakObjectPtr<AActor> WeakActor : Actors )
	{
		AActor* Actor = WeakActor.Get();
		if( Actor != NULL )
		{
			// Grab the MovieScene that is currently focused.  We'll add our Blueprint as an inner of the
			// MovieScene asset.
			UMovieScene* OwnerMovieScene = GetFocusedMovieScene();
			
			// @todo sequencer: Undo doesn't seem to be working at all
			const FScopedTransaction Transaction( LOCTEXT("UndoPossessingObject", "Possess Object with MovieScene") );
			
			// Possess the object!
			{
				// Create a new possessable
				OwnerMovieScene->Modify();
				
				const FGuid PossessableGuid = OwnerMovieScene->AddPossessable( Actor->GetActorLabel(), Actor->GetClass() );
				 
				if ( IsShotFilteringOn() )
				{
					AddUnfilterableObject(PossessableGuid);
				}
				
				ObjectBindingManager->BindPossessableObject( PossessableGuid, *Actor );
				
				bPossessableAdded = true;
			}
		}
	}
	
	if( bPossessableAdded )
	{
		SpawnOrDestroyPuppetObjects( GetFocusedMovieSceneInstance() );
		
		NotifyMovieSceneDataChanged();
	}
}

void FSequencer::NotifyMovieSceneDataChanged()
{
	PlaybackState = EMovieScenePlayerStatus::Stopped;
	bNeedTreeRefresh = true;
}


TRange<float> FSequencer::OnGetViewRange() const
{
	return TRange<float>(FMath::Lerp(LastViewRange.GetLowerBoundValue(), TargetViewRange.GetLowerBoundValue(), ZoomCurve.GetLerp()),
		FMath::Lerp(LastViewRange.GetUpperBoundValue(), TargetViewRange.GetUpperBoundValue(), ZoomCurve.GetLerp()));
}

bool FSequencer::IsAutoKeyEnabled() const 
{
	return bAllowAutoKey;
}

bool FSequencer::IsRecordingLive() const 
{
	return PlaybackState == EMovieScenePlayerStatus::Recording && GIsPlayInEditorWorld;
}

float FSequencer::GetCurrentLocalTime( UMovieScene& MovieScene )
{
	//@todo Sequencer - Nested movie scenes:  Figure out the parent of the passed in movie scene and 
	// calculate local time
	return ScrubPosition;
}

float FSequencer::GetGlobalTime()
{
	return ScrubPosition;
}

void FSequencer::SetGlobalTime( float NewTime )
{
	float LastTime = ScrubPosition;

	// Update the position
	ScrubPosition = NewTime;

	RootMovieSceneInstance->Update( ScrubPosition, LastTime, *this );
}

void FSequencer::SetPerspectiveViewportPossessionEnabled(bool bEnabled)
{
	bPerspectiveViewportPossessionEnabled = bEnabled;
}


FGuid FSequencer::GetHandleToObject( UObject* Object )
{
	TSharedRef<FMovieSceneInstance> FocusedMovieSceneInstance = GetFocusedMovieSceneInstance();
	UMovieScene* FocusedMovieScene = FocusedMovieSceneInstance->GetMovieScene();

	FGuid ObjectGuid = ObjectBindingManager->FindGuidForObject( *FocusedMovieScene, *Object );

	if (ObjectGuid.IsValid())
	{
		// Make sure that the possessable is still valid, if it's not remove the binding so new one 
		// can be created.  This can happen due to undo.
		FMovieScenePossessable* Possessable = FocusedMovieScene->FindPossessable(ObjectGuid);
		if (Possessable == nullptr )
		{
			ObjectBindingManager->UnbindPossessableObjects(ObjectGuid);
			ObjectGuid.Invalidate();
		}
	}
	
	bool bPossessableAdded = false;
	
	// If the object guid was not found attempt to add it
	// Note: Only possessed actors can be added like this
	if( !ObjectGuid.IsValid() && ObjectBindingManager->CanPossessObject( *Object ) )
	{
		// @todo sequencer: Undo doesn't seem to be working at all
		const FScopedTransaction Transaction( LOCTEXT("UndoPossessingObject", "Possess Object with MovieScene") );
		
		// Possess the object!
		{
			// Create a new possessable
			FocusedMovieScene->Modify();

			ObjectGuid = FocusedMovieScene->AddPossessable( Object->GetName(), Object->GetClass() );
			
			if ( IsShotFilteringOn() )
			{
				AddUnfilterableObject(ObjectGuid);
			}
			
			ObjectBindingManager->BindPossessableObject( ObjectGuid, *Object );
			
			bPossessableAdded = true;
		}
	}
	
	if( bPossessableAdded )
	{
		SpawnOrDestroyPuppetObjects( GetFocusedMovieSceneInstance() );
			
		NotifyMovieSceneDataChanged();
	}
	
	return ObjectGuid;
}

ISequencerObjectChangeListener& FSequencer::GetObjectChangeListener()
{ 
	return *ObjectChangeListener;
}


void FSequencer::SpawnActorsForMovie( TSharedRef<FMovieSceneInstance> MovieSceneInstance )
{
	SpawnOrDestroyPuppetObjects( MovieSceneInstance );
}

void FSequencer::GetRuntimeObjects( TSharedRef<FMovieSceneInstance> MovieSceneInstance, const FGuid& ObjectHandle, TArray< UObject*>& OutObjects ) const
{
	ObjectBindingManager->GetRuntimeObjects( MovieSceneInstance, ObjectHandle, OutObjects );
	
	/*if( ObjectSpawner.IsValid() )
	{	
		// First, try to find spawnable puppet objects for the specified Guid
		UObject* FoundPuppetObject = ObjectSpawner->FindPuppetObjectForSpawnableGuid( MovieSceneInstance, ObjectHandle );
		if( FoundPuppetObject != NULL )
		{
			// Found a puppet object!
			OutObjects.Reset();
			OutObjects.Add( FoundPuppetObject );
		}
		else
		{
			// No puppets were found for spawnables, so now we'll check for possessed actors
			if( PlayMovieSceneNode != NULL )
			{
				// @todo Sequencer SubMovieScene: Figure out how to instance possessables
				OutObjects = PlayMovieSceneNode->FindBoundObjects( ObjectHandle );
			}
		}
	}
	else
	{
		FMovieScenePossessable* Possessable = MovieSceneInstance->GetMovieScene()->FindPossessable( ObjectHandle );
		if( Possessable && Possessable->GetPossessableObject() )
		{
			OutObjects.Add( Possessable->GetPossessableObject() );
		}
	}*/
}

void FSequencer::UpdatePreviewViewports(UObject* ObjectToViewThrough) const
{
	if(!IsPerspectiveViewportPosessionEnabled())
	{
		return;
	}

	for(FLevelEditorViewportClient* LevelVC : GEditor->LevelViewportClients)
	{
		if(LevelVC && LevelVC->IsPerspective() && LevelVC->AllowMatineePreview())
		{
			LevelVC->SetMatineeActorLock(Cast<AActor>(ObjectToViewThrough));
		}
	}
}

EMovieScenePlayerStatus::Type FSequencer::GetPlaybackStatus() const
{
	return PlaybackState;
}

void FSequencer::AddMovieSceneInstance( UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneInstance> InstanceToAdd )
{
	MovieSceneSectionToInstanceMap.Add( &MovieSceneSection, InstanceToAdd );

	SpawnOrDestroyPuppetObjects( InstanceToAdd );
}

void FSequencer::RemoveMovieSceneInstance( UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneInstance> InstanceToRemove )
{
	if( ObjectBindingManager->AllowsSpawnableObjects() )
	{
		const bool bDestroyAll = true;
		ObjectBindingManager->SpawnOrDestroyObjectsForInstance( InstanceToRemove, bDestroyAll );
	}
	

	MovieSceneSectionToInstanceMap.Remove( &MovieSceneSection );
}

void FSequencer::AddReferencedObjects( FReferenceCollector& Collector )
{
	for( int32 MovieSceneIndex = 0; MovieSceneIndex < MovieSceneStack.Num(); ++MovieSceneIndex )
	{
		UMovieScene* Scene = MovieSceneStack[MovieSceneIndex]->GetMovieScene();
		Collector.AddReferencedObject( Scene );
	}
}

void FSequencer::ResetPerMovieSceneData()
{
	FilterToShotSections( TArray< TWeakObjectPtr<UMovieSceneSection> >(), false );

	//@todo Sequencer - We may want to preserve selections when moving between movie scenes
	Selection.Empty();

	// @todo run through all tracks for new movie scene changes
	//  needed for audio track decompression
}

void FSequencer::UpdateRuntimeInstances()
{
	// Spawn runtime objects for the root movie scene
	SpawnOrDestroyPuppetObjects( RootMovieSceneInstance.ToSharedRef() );

	// Refresh the current root instance
	RootMovieSceneInstance->RefreshInstance( *this );
	RootMovieSceneInstance->Update(ScrubPosition, ScrubPosition, *this);
}

void FSequencer::DestroySpawnablesForAllMovieScenes()
{
	if( ObjectBindingManager->AllowsSpawnableObjects() )
	{
		ObjectBindingManager->DestroyAllSpawnedObjects();
	}
}

FReply FSequencer::OnPlay()
{
	if( PlaybackState == EMovieScenePlayerStatus::Playing ||
		PlaybackState == EMovieScenePlayerStatus::Recording )
	{
		PlaybackState = EMovieScenePlayerStatus::Stopped;
		// Update on stop (cleans up things like sounds that are playing)
		RootMovieSceneInstance->Update( ScrubPosition, ScrubPosition, *this );
	}
	else
	{
		TRange<float> TimeBounds = GetTimeBounds();
		if (!TimeBounds.IsEmpty())
		{
			float CurrentTime = GetGlobalTime();
			if (CurrentTime < TimeBounds.GetLowerBoundValue() || CurrentTime >= TimeBounds.GetUpperBoundValue())
			{
				SetGlobalTime(TimeBounds.GetLowerBoundValue());
			}
			PlaybackState = EMovieScenePlayerStatus::Playing;
			
			// Make sure Slate ticks during playback
			SequencerWidget->RegisterActiveTimerForPlayback();
		}
	}

	return FReply::Handled();
}

FReply FSequencer::OnRecord()
{
	if (PlaybackState != EMovieScenePlayerStatus::Recording)
	{
		PlaybackState = EMovieScenePlayerStatus::Recording;
		
		// Make sure Slate ticks during playback
		SequencerWidget->RegisterActiveTimerForPlayback();

		// @todo sequencer livecapture: Ideally we would support fixed timestep capture from simulation
		//			Basically we need to run the PIE world at a fixed time step, capturing key frames every frame. 
		//			The editor world would still be ticked at the normal throttle-real-time rate
	}
	else
	{
		PlaybackState = EMovieScenePlayerStatus::Stopped;
	}

	return FReply::Handled();
}

FReply FSequencer::OnStepForward()
{
	PlaybackState = EMovieScenePlayerStatus::Stopped;
	// @todo Sequencer Add proper step sizes
	SetGlobalTime(ScrubPosition + 0.1f);
	return FReply::Handled();
}

FReply FSequencer::OnStepBackward()
{
	PlaybackState = EMovieScenePlayerStatus::Stopped;
	// @todo Sequencer Add proper step sizes
	SetGlobalTime(ScrubPosition - 0.1f);
	return FReply::Handled();
}

FReply FSequencer::OnStepToEnd()
{
	PlaybackState = EMovieScenePlayerStatus::Stopped;
	TRange<float> TimeBounds = GetTimeBounds();
	if (!TimeBounds.IsEmpty())
	{
		SetGlobalTime(TimeBounds.GetUpperBoundValue());
	}
	return FReply::Handled();
}

FReply FSequencer::OnStepToBeginning()
{
	PlaybackState = EMovieScenePlayerStatus::Stopped;
	TRange<float> TimeBounds = GetTimeBounds();
	if (!TimeBounds.IsEmpty())
	{
		SetGlobalTime(TimeBounds.GetLowerBoundValue());
	}
	return FReply::Handled();
}

FReply FSequencer::OnToggleLooping()
{
	bLoopingEnabled = !bLoopingEnabled;
	return FReply::Handled();
}

bool FSequencer::IsLooping() const
{
	return bLoopingEnabled;
}

EPlaybackMode::Type FSequencer::GetPlaybackMode() const
{
	return PlaybackState == EMovieScenePlayerStatus::Playing ? EPlaybackMode::PlayingForward :
		PlaybackState == EMovieScenePlayerStatus::Recording ? EPlaybackMode::Recording :
		EPlaybackMode::Stopped;
}


TRange<float> FSequencer::GetTimeBounds() const
{
	// When recording, we never want to constrain the time bound range.  You might not even have any sections or keys yet
	// but we need to be able to move the time cursor during playback so you can capture data in real-time
	if( PlaybackState == EMovieScenePlayerStatus::Recording )
	{
		return TRange<float>( -100000.0f, 100000.0f );
	}

	const UMovieScene* MovieScene = GetFocusedMovieScene();
	const UMovieSceneTrack* AnimatableShot = MovieScene->FindMasterTrack( UMovieSceneDirectorTrack::StaticClass() );
	if (AnimatableShot)
	{
		// try getting filtered shot boundaries
		TRange<float> Bounds = GetFilteringShotsTimeBounds();
		if (!Bounds.IsEmpty()) {return Bounds;}

		// try getting the bounds of all shots
		Bounds = AnimatableShot->GetSectionBoundaries();
		if (!Bounds.IsEmpty()) {return Bounds;}
	}
	
	return MovieScene->GetTimeRange();
}

TRange<float> FSequencer::GetFilteringShotsTimeBounds() const
{
	if (IsShotFilteringOn())
	{
		TArray< TRange<float> > Bounds;
		for (int32 i = 0; i < FilteringShots.Num(); ++i)
		{
			Bounds.Add(FilteringShots[i]->GetRange());
		}

		return TRange<float>::Hull(Bounds);
	}

	return TRange<float>::Empty();
}



void FSequencer::OnViewRangeChanged( TRange<float> NewViewRange, bool bSmoothZoom )
{
	if (!NewViewRange.IsEmpty())
	{
		if (bSmoothZoom)
		{
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
	}
}

void FSequencer::OnScrubPositionChanged( float NewScrubPosition, bool bScrubbing )
{
	if (bScrubbing)
	{
		PlaybackState =
			PlaybackState == EMovieScenePlayerStatus::BeginningScrubbing || PlaybackState == EMovieScenePlayerStatus::Scrubbing ?
			EMovieScenePlayerStatus::Scrubbing : EMovieScenePlayerStatus::BeginningScrubbing;
	}
	else
	{
		PlaybackState = EMovieScenePlayerStatus::Stopped;
	}

	SetGlobalTime( NewScrubPosition );
}

void FSequencer::OnToggleAutoKey()
{
	bAllowAutoKey = !bAllowAutoKey;
}

FGuid FSequencer::AddSpawnableForAssetOrClass( UObject* Object, UObject* CounterpartGamePreviewObject )
{
	FGuid NewSpawnableGuid;
	
	if( ObjectBindingManager->AllowsSpawnableObjects() )
	{
		// Grab the MovieScene that is currently focused.  We'll add our Blueprint as an inner of the
		// MovieScene asset.
		UMovieScene* OwnerMovieScene = GetFocusedMovieScene();

		// @todo sequencer: Undo doesn't seem to be working at all
		const FScopedTransaction Transaction( LOCTEXT("UndoAddingObject", "Add Object to MovieScene") );

		// Use the class as the spawnable's name if this is an actor class, otherwise just use the object name (asset)
		const bool bIsActorClass = Object->IsA( AActor::StaticClass() ) && !Object->HasAnyFlags( RF_ArchetypeObject );
		const FName AssetName = bIsActorClass ? Object->GetClass()->GetFName() : Object->GetFName();

		// Inner objects don't need a name (it will be auto-generated by the UObject system), but we want one in this case
		// because the class of any actors that are created from this Blueprint may end up being user-facing.
		const FName BlueprintName = MakeUniqueObjectName( OwnerMovieScene, UBlueprint::StaticClass(), AssetName );

		// Use the asset name as the initial spawnable name
		const FString NewSpawnableName = AssetName.ToString();		// @todo sequencer: Need UI to allow user to rename these slots

		// Create our new blueprint!
		UBlueprint* NewBlueprint = NULL;
		{
			// @todo sequencer: Add support for forcing specific factories for an asset
			UActorFactory* FactoryToUse = NULL;
			if( bIsActorClass )
			{
				// Placing an actor class directly::
				FactoryToUse = GEditor->FindActorFactoryForActorClass( Object->GetClass() );
			}
			else
			{
				// Placing an asset
				FactoryToUse = FActorFactoryAssetProxy::GetFactoryForAssetObject( Object );
			}

			if( FactoryToUse != NULL )
			{
				// Create the blueprint
				NewBlueprint = FactoryToUse->CreateBlueprint( Object, OwnerMovieScene, BlueprintName );
			}
			else if( bIsActorClass )
			{
				// We don't have a factory, but we can still try to create a blueprint for this actor class
				NewBlueprint = FKismetEditorUtilities::CreateBlueprint( Object->GetClass(), OwnerMovieScene, BlueprintName, EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass() );
			}
		}

		if( ensure( NewBlueprint != NULL ) )
		{
			if( NewBlueprint->GeneratedClass != NULL && FBlueprintEditorUtils::IsActorBased( NewBlueprint ) )
			{
				AActor* ActorCDO = CastChecked< AActor >( NewBlueprint->GeneratedClass->ClassDefaultObject );

				// If we have a counterpart object, then copy the properties from that object back into our blueprint's CDO
				// @todo sequencer livecapture: This isn't really good enough to handle complex actors.  The dynamically-spawned actor could have components that
				// were created in its construction script or via straight-up C++ code.  Instead what we should probably do is duplicate the PIE actor and generate
				// our CDO from that duplicate.  It could get pretty complex though.
				if( CounterpartGamePreviewObject != NULL )
				{
					AActor* CounterpartGamePreviewActor = CastChecked< AActor >( CounterpartGamePreviewObject );
					CopyActorProperties( CounterpartGamePreviewActor, ActorCDO );
				}
				else
				{
					// Place the new spawnable in front of the camera (unless we were automatically created from a PIE actor)
					PlaceActorInFrontOfCamera( ActorCDO );
				}
			}

			NewSpawnableGuid = OwnerMovieScene->AddSpawnable( NewSpawnableName, NewBlueprint, CounterpartGamePreviewObject );

			if (IsShotFilteringOn())
			{
				AddUnfilterableObject(NewSpawnableGuid);
			}
		}
	
	}

	return NewSpawnableGuid;
}

void FSequencer::AddSubMovieScene( UMovieScene* SubMovieScene )
{
	// @todo Sequencer - sub-moviescenes This should be moved to the sub-moviescene editor
	SubMovieScene->SetFlags( RF_Transactional );
	
	// Grab the MovieScene that is currently focused.  THis is the movie scene that will contain the sub-moviescene
	UMovieScene* OwnerMovieScene = GetFocusedMovieScene();

	// @todo sequencer: Undo doesn't seem to be working at all
	const FScopedTransaction Transaction( LOCTEXT("UndoAddingObject", "Add Object to MovieScene") );

	OwnerMovieScene->Modify();

	UMovieSceneTrack* Type = OwnerMovieScene->FindMasterTrack( USubMovieSceneTrack::StaticClass() ) ;
	if( !Type )
	{
		Type = OwnerMovieScene->AddMasterTrack( USubMovieSceneTrack::StaticClass() );
	}

	USubMovieSceneTrack* SubMovieSceneType = CastChecked<USubMovieSceneTrack>( Type );

	SubMovieSceneType->AddMovieSceneSection( SubMovieScene, ScrubPosition );
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
			check(!bWasConsumed);
			bWasConsumed = true;
		}
	}
	return bWasConsumed;
}

void FSequencer::OnRequestNodeDeleted( TSharedRef<const FSequencerDisplayNode>& NodeToBeDeleted )
{
	bool bAnySpawnablesRemoved = false;
	bool bAnythingRemoved = false;
	
	TSharedRef<FMovieSceneInstance> MovieSceneInstance = GetFocusedMovieSceneInstance();
	UMovieScene* OwnerMovieScene = MovieSceneInstance->GetMovieScene();

	// Only object nodes or section areas can be deleted
	if( NodeToBeDeleted->GetType() == ESequencerNode::Object  )
	{
		OwnerMovieScene->SetFlags( RF_Transactional );
		const FGuid& BindingToRemove = StaticCastSharedRef<const FObjectBindingNode>( NodeToBeDeleted )->GetObjectBinding();

		//@todo Sequencer - add transaction
		
		// Try to remove as a spawnable first
		bool bRemoved = OwnerMovieScene->RemoveSpawnable( BindingToRemove );
		if( bRemoved )
		{
			bAnySpawnablesRemoved = true;
		}

		if( !bRemoved )
		{
			// The guid should be associated with a possessable if it wasnt a spawnable
			bRemoved = OwnerMovieScene->RemovePossessable( BindingToRemove );
			
			// @todo Sequencer - undo needs to work here
			ObjectBindingManager->UnbindPossessableObjects( BindingToRemove );
			
			// If this check fails the guid was not associated with a spawnable or possessable so there was an invalid guid being stored on a node
			check( bRemoved );
		}

		bAnythingRemoved = true;
	}
	else if( NodeToBeDeleted->GetType() == ESequencerNode::Track  )
	{
		TSharedRef<const FTrackNode> SectionAreaNode = StaticCastSharedRef<const FTrackNode>( NodeToBeDeleted );
		UMovieSceneTrack* Track = SectionAreaNode->GetTrack();

		UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

		FocusedMovieScene->SetFlags( RF_Transactional );
		
		if( FocusedMovieScene->IsAMasterTrack( Track ) )
		{
			FocusedMovieScene->RemoveMasterTrack( Track );
		}
		else
		{
			FocusedMovieScene->RemoveTrack( Track );
		}
		
		bAnythingRemoved = true;
	}

	
	if( bAnythingRemoved )
	{
		if( bAnySpawnablesRemoved )
		{
			// @todo Sequencer Sub-MovieScenes needs to destroy objects for all movie scenes that had this node
			SpawnOrDestroyPuppetObjects( MovieSceneInstance );
		}

		NotifyMovieSceneDataChanged();
	}
}

void FSequencer::PlaceActorInFrontOfCamera( AActor* ActorCDO )
{
	// Place the actor in front of the active perspective camera if we have one
	if( GCurrentLevelEditingViewportClient != NULL && GCurrentLevelEditingViewportClient->IsPerspective() )
	{
		// Don't allow this when the active viewport is showing a simulation/PIE level
		const bool bIsViewportShowingPIEWorld = ( GCurrentLevelEditingViewportClient->GetWorld()->GetOutermost()->PackageFlags & PKG_PlayInEditor ) != 0;
		if( !bIsViewportShowingPIEWorld )
		{
			// @todo sequencer actors: Ideally we could use the actor's collision to figure out how far to push out
			// the object (like when placing in viewports), but we can't really do that because we're only dealing with a CDO
			const float DistanceFromCamera = 50.0f;

			// Find a place to put the object
			// @todo sequencer cleanup: This code should be reconciled with the GEditor->MoveActorInFrontOfCamera() stuff
			const FVector& CameraLocation = GCurrentLevelEditingViewportClient->GetViewLocation();
			const FRotator& CameraRotation = GCurrentLevelEditingViewportClient->GetViewRotation();
			const FVector CameraDirection = CameraRotation.Vector();

			FVector NewLocation = CameraLocation + CameraDirection * ( DistanceFromCamera + GetDefault<ULevelEditorViewportSettings>()->BackgroundDropDistance );
			FSnappingUtils::SnapPointToGrid( NewLocation, FVector::ZeroVector );

			ActorCDO->SetActorRelativeLocation( NewLocation );
			ActorCDO->SetActorRelativeRotation( CameraRotation );
		}
	}
}


void FSequencer::CopyActorProperties( AActor* PuppetActor, AActor* TargetActor ) const
{
	// @todo sequencer: How do we make this undoable with the original action that caused the change in the first place? 
	//       -> Ideally we are still within the transaction scope while performing this change

	// @todo sequencer: If we decide to propagate changes BACK to an actor after changing the CDO, we may need to
	// do additional work to make sure transient state is refreshed (e.g. call PostEditMove() is transform is changed)

	// @todo sequencer: Pass option here to call PostEditChange, if needed
	const int32 CopiedPropertyCount = EditorUtilities::CopyActorProperties( PuppetActor, TargetActor );
}


void FSequencer::GetActorRecordingState( bool& bIsRecording /* In+Out */ ) const
{
	if( IsRecordingLive() )
	{
		bIsRecording = true;
	}
}

void FSequencer::PostUndo(bool bSuccess)
{
	NotifyMovieSceneDataChanged();
}


void FSequencer::OnSectionSelectionChanged()
{
	for (TWeakObjectPtr<UMovieSceneSection> SelectedSection : *Selection.GetSelectedSections())
	{
		// if we select something, consider it unfilterable until we change shot filters
		UnfilterableSections.AddUnique(TWeakObjectPtr<UMovieSceneSection>(SelectedSection));
	}
}

void FSequencer::ZoomToSelectedSections()
{
	TArray< TRange<float> > Bounds;
	for (TWeakObjectPtr<UMovieSceneSection> SelectedSection : *Selection.GetSelectedSections())
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
		OnViewRangeChanged(BoundsHull, true);
	}
}

void FSequencer::FilterToShotSections(const TArray< TWeakObjectPtr<class UMovieSceneSection> >& ShotSections, bool bZoomToShotBounds )
{
	TArray< TWeakObjectPtr<UMovieSceneSection> > ActualShotSections;
	for (int32 i = 0; i < ShotSections.Num(); ++i)
	{
		if (ShotSections[i]->IsA<UMovieSceneShotSection>())
		{
			ActualShotSections.Add(ShotSections[i]);
		}
	}

	bool bWasPreviouslyFiltering = IsShotFilteringOn();
	bool bIsNowFiltering = ActualShotSections.Num() > 0;

	FilteringShots.Empty();
	UnfilterableSections.Empty();
	UnfilterableObjects.Empty();

	if (bIsNowFiltering)
	{
		for ( int32 ShotSectionIndex = 0; ShotSectionIndex < ActualShotSections.Num(); ++ShotSectionIndex )
		{
			FilteringShots.Add( ActualShotSections[ShotSectionIndex] );
		}

		// populate unfilterable shots - shots that started not filtered
		TArray< TWeakObjectPtr<class UMovieSceneSection> > TempUnfilterableSections;
		const TArray<UMovieSceneSection*>& AllSections = GetFocusedMovieScene()->GetAllSections();
		for (int32 SectionIndex = 0; SectionIndex < AllSections.Num(); ++SectionIndex)
		{
			UMovieSceneSection* Section = AllSections[SectionIndex];
			if (!Section->IsA<UMovieSceneShotSection>() && IsSectionVisible(Section))
			{
				TempUnfilterableSections.Add(Section);
			}
		}
		// wait until after we've collected them all before applying in order to
		// prevent wastefully searching through UnfilterableSections in IsSectionVisible
		UnfilterableSections = TempUnfilterableSections;
	}

	if (!bWasPreviouslyFiltering && bIsNowFiltering)
	{
		OverlayAnimation.Play( SequencerWidget.ToSharedRef() );
	}
	else if (bWasPreviouslyFiltering && !bIsNowFiltering) 
	{
		OverlayAnimation.PlayReverse( SequencerWidget.ToSharedRef() );
	}

	if( bZoomToShotBounds )
	{
		// zoom in
		OnViewRangeChanged(GetTimeBounds(), true);
	}

	SequencerWidget->UpdateBreadcrumbs(ActualShotSections);
}

void FSequencer::FilterToSelectedShotSections(bool bZoomToShotBounds)
{
	TArray< TWeakObjectPtr<UMovieSceneSection> > SelectedShotSections;
	for (TWeakObjectPtr<UMovieSceneSection> SelectedSection : *Selection.GetSelectedSections())
	{
		if (SelectedSection->IsA<UMovieSceneShotSection>())
		{
			SelectedShotSections.Add(SelectedSection);
		}
	}
	FilterToShotSections(SelectedShotSections, bZoomToShotBounds);
}

bool FSequencer::CanKeyProperty(const UClass& ObjectClass, const IPropertyHandle& PropertyHandle) const
{
	return ObjectChangeListener->IsTypeKeyable( ObjectClass, PropertyHandle );
} 

void FSequencer::KeyProperty(const TArray<UObject*>& ObjectsToKey, const class IPropertyHandle& PropertyHandle) 
{
	ObjectChangeListener->KeyProperty( ObjectsToKey, PropertyHandle );
}

TSharedRef<ISequencerObjectBindingManager> FSequencer::GetObjectBindingManager() const
{
	return ObjectBindingManager.ToSharedRef();
}

FSequencerSelection* FSequencer::GetSelection()
{
	return &Selection;
}

TArray< TWeakObjectPtr<UMovieSceneSection> > FSequencer::GetFilteringShotSections() const
{
	return FilteringShots;
}

bool FSequencer::IsObjectUnfilterable(const FGuid& ObjectGuid) const
{
	return UnfilterableObjects.Find(ObjectGuid) != INDEX_NONE;
}

void FSequencer::AddUnfilterableObject(const FGuid& ObjectGuid)
{
	UnfilterableObjects.AddUnique(ObjectGuid);
}

bool FSequencer::IsShotFilteringOn() const
{
	return FilteringShots.Num() > 0;
}

float FSequencer::GetOverlayFadeCurve() const
{
	return OverlayCurve.GetLerp();
}

bool FSequencer::IsSectionVisible(UMovieSceneSection* Section) const
{
	// if no shots are being filtered, don't filter at all
	bool bShowAll = !IsShotFilteringOn();

	bool bIsAShotSection = Section->IsA<UMovieSceneShotSection>();

	bool bIsUnfilterable = UnfilterableSections.Find(Section) != INDEX_NONE;

	// do not draw if not within the filtering shot sections
	TRange<float> SectionRange = Section->GetRange();
	bool bIsVisible = bShowAll || bIsAShotSection || bIsUnfilterable;
	for (int32 i = 0; i < FilteringShots.Num() && !bIsVisible; ++i)
	{
		TRange<float> ShotRange = FilteringShots[i]->GetRange();
		bIsVisible |= SectionRange.Overlaps(ShotRange);
	}

	return bIsVisible;
}

void FSequencer::DeleteSelectedItems()
{
	if (Selection.GetActiveSelection() == FSequencerSelection::EActiveSelection::KeyAndSection)
	{
		DeleteSelectedKeys();
		for (TWeakObjectPtr<UMovieSceneSection> SelectedSection : *Selection.GetSelectedSections())
		{
			DeleteSection(SelectedSection.Get());
		}
	}
	else if (Selection.GetActiveSelection() == FSequencerSelection::EActiveSelection::OutlinerNode)
	{
		SequencerWidget->DeleteSelectedNodes();
	}
}

void FSequencer::SetKey()
{
	USelection* CurrentSelection = GEditor->GetSelectedActors();
	TArray<UObject*> SelectedActors;
	CurrentSelection->GetSelectedObjects( AActor::StaticClass(), SelectedActors );
	for (TArray<UObject*>::TIterator It(SelectedActors); It; ++It)
	{
		// @todo Handle case of actors which aren't in sequencer yet

		FGuid ObjectGuid = GetHandleToObject(*It);
		for ( auto& TrackEditor : TrackEditors )
		{
			// @todo Handle this director track business better
			if (TrackEditor != DirectorTrackEditor.Pin())
			{
				TrackEditor->AddKey(ObjectGuid);
			}
		}
	}
}


void FSequencer::BindSequencerCommands()
{
	const FSequencerCommands& Commands = FSequencerCommands::Get();

	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP( this, &FSequencer::DeleteSelectedItems ) );

	SequencerCommandBindings->MapAction(
		Commands.SetKey,
		FExecuteAction::CreateSP( this, &FSequencer::SetKey ) );

	USequencerSettings* Settings = GetMutableDefault<USequencerSettings>();

	SequencerCommandBindings->MapAction(
		Commands.ToggleAutoKeyEnabled,
		FExecuteAction::CreateSP( this, &FSequencer::OnToggleAutoKey ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateSP( this, &FSequencer::OnGetAllowAutoKey ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleCleanView,
		FExecuteAction::CreateLambda( [Settings]{ Settings->SetIsUsingCleanView( !Settings->GetIsUsingCleanView() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [Settings]{ return Settings->GetIsUsingCleanView(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleIsSnapEnabled,
		FExecuteAction::CreateLambda( [Settings]{ Settings->SetIsSnapEnabled( !Settings->GetIsSnapEnabled() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [Settings]{ return Settings->GetIsSnapEnabled(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapKeyTimesToInterval,
		FExecuteAction::CreateLambda( [Settings]{ Settings->SetSnapKeyTimesToInterval( !Settings->GetSnapKeyTimesToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [Settings]{ return Settings->GetSnapKeyTimesToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapKeyTimesToKeys,
		FExecuteAction::CreateLambda( [Settings]{ Settings->SetSnapKeyTimesToKeys( !Settings->GetSnapKeyTimesToKeys() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [Settings]{ return Settings->GetSnapKeyTimesToKeys(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapSectionTimesToInterval,
		FExecuteAction::CreateLambda( [Settings]{ Settings->SetSnapSectionTimesToInterval( !Settings->GetSnapSectionTimesToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [Settings]{ return Settings->GetSnapSectionTimesToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapSectionTimesToSections,
		FExecuteAction::CreateLambda( [Settings]{ Settings->SetSnapSectionTimesToSections( !Settings->GetSnapSectionTimesToSections() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [Settings]{ return Settings->GetSnapSectionTimesToSections(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapPlayTimeToInterval,
		FExecuteAction::CreateLambda( [Settings]{ Settings->SetSnapPlayTimeToInterval( !Settings->GetSnapPlayTimeToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [Settings]{ return Settings->GetSnapPlayTimeToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleSnapCurveValueToInterval,
		FExecuteAction::CreateLambda( [Settings]{ Settings->SetSnapCurveValueToInterval( !Settings->GetSnapCurveValueToInterval() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [Settings]{ return Settings->GetSnapCurveValueToInterval(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleShowCurveEditor,
		FExecuteAction::CreateLambda( [Settings]{ Settings->SetShowCurveEditor(!Settings->GetShowCurveEditor()); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [Settings]{ return Settings->GetShowCurveEditor(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.ToggleShowCurveEditorCurveToolTips,
		FExecuteAction::CreateLambda( [Settings]{ Settings->SetShowCurveEditorCurveToolTips( !Settings->GetShowCurveEditorCurveToolTips() ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [Settings]{ return Settings->GetShowCurveEditorCurveToolTips(); } ) );

	SequencerCommandBindings->MapAction(
		Commands.SetAllCurveVisibility,
		FExecuteAction::CreateLambda( [Settings]{ Settings->SetCurveVisibility( ESequencerCurveVisibility::AllCurves ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [Settings]{ return Settings->GetCurveVisibility() == ESequencerCurveVisibility::AllCurves; } ) );

	SequencerCommandBindings->MapAction(
		Commands.SetSelectedCurveVisibility,
		FExecuteAction::CreateLambda( [Settings]{ Settings->SetCurveVisibility( ESequencerCurveVisibility::SelectedCurves ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [Settings]{ return Settings->GetCurveVisibility() == ESequencerCurveVisibility::SelectedCurves; } ) );

	SequencerCommandBindings->MapAction(
		Commands.SetAnimatedCurveVisibility,
		FExecuteAction::CreateLambda( [Settings]{ Settings->SetCurveVisibility( ESequencerCurveVisibility::AnimatedCurves ); } ),
		FCanExecuteAction::CreateLambda( []{ return true; } ),
		FIsActionChecked::CreateLambda( [Settings]{ return Settings->GetCurveVisibility() == ESequencerCurveVisibility::AnimatedCurves; } ) );
}

void FSequencer::BuildObjectBindingContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	for (int32 i = 0; i < TrackEditors.Num(); ++i)
	{
		TrackEditors[i]->BuildObjectBindingContextMenu(MenuBuilder, ObjectBinding, ObjectClass);
	}
}

#undef LOCTEXT_NAMESPACE
