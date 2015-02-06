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
		PropertyTrackEditorCreateTrackEditorDelegateHandle      = SequencerModule.RegisterTrackEditor_Handle( FOnCreateTrackEditor::CreateStatic( &FPropertyTrackEditor::CreateTrackEditor ) );
		TransformTrackEditorCreateTrackEditorDelegateHandle     = SequencerModule.RegisterTrackEditor_Handle( FOnCreateTrackEditor::CreateStatic( &F3DTransformTrackEditor::CreateTrackEditor ) );
		DirectorTrackEditorCreateTrackEditorDelegateHandle      = SequencerModule.RegisterTrackEditor_Handle( FOnCreateTrackEditor::CreateStatic( &FDirectorTrackEditor::CreateTrackEditor ) );
		SubMovieSceneTrackEditorCreateTrackEditorDelegateHandle = SequencerModule.RegisterTrackEditor_Handle( FOnCreateTrackEditor::CreateStatic( &FSubMovieSceneTrackEditor::CreateTrackEditor ) );
		AudioTrackEditorCreateTrackEditorDelegateHandle         = SequencerModule.RegisterTrackEditor_Handle( FOnCreateTrackEditor::CreateStatic( &FAudioTrackEditor::CreateTrackEditor ) );
		AnimationTrackEditorCreateTrackEditorDelegateHandle     = SequencerModule.RegisterTrackEditor_Handle( FOnCreateTrackEditor::CreateStatic( &FAnimationTrackEditor::CreateTrackEditor ) );
		ParticleTrackEditorCreateTrackEditorDelegateHandle      = SequencerModule.RegisterTrackEditor_Handle( FOnCreateTrackEditor::CreateStatic( &FParticleTrackEditor::CreateTrackEditor ) );
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
			SequencerModule.UnRegisterTrackEditor_Handle( PropertyTrackEditorCreateTrackEditorDelegateHandle );
			SequencerModule.UnRegisterTrackEditor_Handle( TransformTrackEditorCreateTrackEditorDelegateHandle );
			SequencerModule.UnRegisterTrackEditor_Handle( DirectorTrackEditorCreateTrackEditorDelegateHandle );
			SequencerModule.UnRegisterTrackEditor_Handle( SubMovieSceneTrackEditorCreateTrackEditorDelegateHandle );
			SequencerModule.UnRegisterTrackEditor_Handle( AudioTrackEditorCreateTrackEditorDelegateHandle );
			SequencerModule.UnRegisterTrackEditor_Handle( AnimationTrackEditorCreateTrackEditorDelegateHandle );
			SequencerModule.UnRegisterTrackEditor_Handle( ParticleTrackEditorCreateTrackEditorDelegateHandle );
		}
		
	}


private:

	/** Asset type actions for MovieScene assets.  Cached here so that we can unregister it during shutdown. */
	TSharedPtr< FAssetTypeActions_MovieScene > MovieSceneAssetTypeActions;

	/** Registered delegate handles */
	FDelegateHandle PropertyTrackEditorCreateTrackEditorDelegateHandle;
	FDelegateHandle TransformTrackEditorCreateTrackEditorDelegateHandle;
	FDelegateHandle DirectorTrackEditorCreateTrackEditorDelegateHandle;
	FDelegateHandle SubMovieSceneTrackEditorCreateTrackEditorDelegateHandle;
	FDelegateHandle AudioTrackEditorCreateTrackEditorDelegateHandle;
	FDelegateHandle AnimationTrackEditorCreateTrackEditorDelegateHandle;
	FDelegateHandle ParticleTrackEditorCreateTrackEditorDelegateHandle;
};


IMPLEMENT_MODULE( FMovieSceneToolsModule, MovieSceneTools );

