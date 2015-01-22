// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "IAssetTools.h"
#include "AssetTypeActions_MovieScene.h"
#include "Toolkits/IToolkit.h"
#include "ISequencerModule.h"
#include "MovieScene.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"


UClass* FAssetTypeActions_MovieScene::GetSupportedClass() const
{
	return UMovieScene::StaticClass(); 
}

void FAssetTypeActions_MovieScene::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UMovieScene* MovieScene = Cast<UMovieScene>(*ObjIt);
		if (MovieScene != NULL)
		{
			// @todo sequencer: Only allow users to create new MovieScenes if that feature is turned on globally.
			if( FParse::Param( FCommandLine::Get(), TEXT( "Sequencer" ) ) )
			{
				FSequencerViewParams ViewParams;
				FModuleManager::LoadModuleChecked< ISequencerModule >( "Sequencer" ).CreateSequencerAssetEditor( Mode, ViewParams, EditWithinLevelEditor, MovieScene, true );
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE