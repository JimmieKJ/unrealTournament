// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "ModuleManager.h"
#include "Sequencer.h"
#include "Toolkits/ToolkitManager.h"
#include "SequencerCommands.h"
#include "SequencerAssetEditor.h"
#include "SequencerObjectChangeListener.h"

/**
 * SequencerModule implementation (private)
 */
class FSequencerModule : public ISequencerModule
{

	/** ISequencerModule interface */
	virtual TSharedPtr<ISequencer> CreateSequencer( UMovieScene* RootMovieScene, const FSequencerViewParams& InViewParams, TSharedRef<ISequencerObjectBindingManager> ObjectBindingManager ) override
	{
		TSharedRef< FSequencer > Sequencer = MakeShareable(new FSequencer);
		
		FSequencerInitParams SequencerInitParams;
		SequencerInitParams.ViewParams = InViewParams;
		SequencerInitParams.ObjectChangeListener = MakeShareable( new FSequencerObjectChangeListener( Sequencer, false ) );
		
		SequencerInitParams.ObjectBindingManager = ObjectBindingManager;
		
		SequencerInitParams.RootMovieScene = RootMovieScene;
		
		SequencerInitParams.bEditWithinLevelEditor = false;
		
		SequencerInitParams.ToolkitHost = nullptr;
		
		Sequencer->InitSequencer( SequencerInitParams, TrackEditorDelegates );
		
		return Sequencer;
	}
	
	virtual TSharedPtr<ISequencer> CreateSequencerAssetEditor( const EToolkitMode::Type Mode, const FSequencerViewParams& InViewParams, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UMovieScene* InRootMovieScene, bool bEditWithinLevelEditor ) override
	{
		TSharedRef< FSequencerAssetEditor > SequencerAssetEditor = MakeShareable(new FSequencerAssetEditor);

		SequencerAssetEditor->InitSequencerAssetEditor( Mode, InViewParams, InitToolkitHost, InRootMovieScene, TrackEditorDelegates, bEditWithinLevelEditor );
		return SequencerAssetEditor->GetSequencerInterface();
	}

	virtual void RegisterTrackEditor( FOnCreateTrackEditor InOnCreateTrackEditor ) override
	{
		TrackEditorDelegates.AddUnique( InOnCreateTrackEditor );
	}

	virtual void UnRegisterTrackEditor( FOnCreateTrackEditor InOnCreateTrackEditor ) override
	{
		TrackEditorDelegates.Remove( InOnCreateTrackEditor );
	}

	virtual void StartupModule() override
	{
		if (GIsEditor)
		{
			FSequencerCommands::Register();
		}
	}

	virtual void ShutdownModule() override
	{
		if (GIsEditor)
		{
			FSequencerCommands::Unregister();
		}
	}
private:
	/** List of auto-key handler delegates sequencers will execute when they are created */
	TArray< FOnCreateTrackEditor > TrackEditorDelegates;
};



IMPLEMENT_MODULE( FSequencerModule, Sequencer );
