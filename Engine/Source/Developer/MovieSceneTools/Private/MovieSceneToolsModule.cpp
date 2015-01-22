// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "ModuleManager.h"

#include "MovieSceneFactory.h"
#include "K2Node_PlayMovieScene.h"

#include "ISequencerModule.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "MovieSceneSection.h"
#include "ISequencerSection.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions_MovieScene.h"
#include "ScopedTransaction.h"
#include "MovieScene.h"
#include "MovieSceneTrackEditor.h"
#include "PropertyTrackEditor.h"
#include "TransformTrackEditor.h"
#include "DirectorTrackEditor.h"
#include "SubMovieSceneTrackEditor.h"
#include "AudioTrackEditor.h"
#include "AnimationTrackEditor.h"
#include "ParticleTrackEditor.h"

/**
 * MovieSceneTools module implementation (private)
 */
class FMovieSceneToolsModule : public IMovieSceneTools
{

	virtual void StartupModule() override
	{
		MovieSceneAssetTypeActions = MakeShareable( new FAssetTypeActions_MovieScene );
		FModuleManager::LoadModuleChecked< FAssetToolsModule >( "AssetTools" ).Get().RegisterAssetTypeActions( MovieSceneAssetTypeActions.ToSharedRef() );

		// Register with the sequencer module that we provide auto-key handlers.
		ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>( "Sequencer" );
		SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FPropertyTrackEditor::CreateTrackEditor ) );
		SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &F3DTransformTrackEditor::CreateTrackEditor ) );
		SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FDirectorTrackEditor::CreateTrackEditor ) );
		SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FSubMovieSceneTrackEditor::CreateTrackEditor ) );
		SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FAudioTrackEditor::CreateTrackEditor ) );
		SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FAnimationTrackEditor::CreateTrackEditor ) );
		SequencerModule.RegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FParticleTrackEditor::CreateTrackEditor ) );
	}


	virtual void ShutdownModule() override
	{
		// Only unregister if the asset tools module is loaded.  We don't want to forcibly load it during shutdown phase.
		check( MovieSceneAssetTypeActions.IsValid() );
		if( FModuleManager::Get().IsModuleLoaded( "AssetTools" ) )
		{
			FModuleManager::GetModuleChecked< FAssetToolsModule >( "AssetTools" ).Get().UnregisterAssetTypeActions( MovieSceneAssetTypeActions.ToSharedRef() );
		}
		MovieSceneAssetTypeActions.Reset();


		if( FModuleManager::Get().IsModuleLoaded( "Sequencer" ) )
		{
			// Unregister auto key handlers
			ISequencerModule& SequencerModule = FModuleManager::Get().GetModuleChecked<ISequencerModule>( "Sequencer" );
			SequencerModule.UnRegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FPropertyTrackEditor::CreateTrackEditor ) );
			SequencerModule.UnRegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &F3DTransformTrackEditor::CreateTrackEditor ) );
			SequencerModule.UnRegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FDirectorTrackEditor::CreateTrackEditor ) );
			SequencerModule.UnRegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FSubMovieSceneTrackEditor::CreateTrackEditor ) );
			SequencerModule.UnRegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FAudioTrackEditor::CreateTrackEditor ) );
			SequencerModule.UnRegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FAnimationTrackEditor::CreateTrackEditor ) );
			SequencerModule.UnRegisterTrackEditor( FOnCreateTrackEditor::CreateStatic( &FParticleTrackEditor::CreateTrackEditor ) );
		}
		
	}


private:

	/** Asset type actions for MovieScene assets.  Cached here so that we can unregister it during shutdown. */
	TSharedPtr< FAssetTypeActions_MovieScene > MovieSceneAssetTypeActions;
};


IMPLEMENT_MODULE( FMovieSceneToolsModule, MovieSceneTools );

