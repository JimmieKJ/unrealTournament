// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPrivatePCH.h"
#include "ContentBrowserModule.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"


/* FMediaPlayerActions constructors
 *****************************************************************************/

FMediaPlayerActions::FMediaPlayerActions( const TSharedRef<ISlateStyle>& InStyle )
	: Style(InStyle)
{ }


/* FAssetTypeActions_Base overrides
 *****************************************************************************/

bool FMediaPlayerActions::CanFilter()
{
	return true;
}


void FMediaPlayerActions::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	FAssetTypeActions_Base::GetActions(InObjects, MenuBuilder);

	auto MediaPlayers = GetTypedWeakObjectPtrs<UMediaPlayer>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MediaPlayer_PlayMovie", "Play Movie"),
		LOCTEXT("MediaPlayer_PlayMovieToolTip", "Starts playback of the media."),
		FSlateIcon( FEditorStyle::GetStyleSetName(), "MediaAsset.AssetActions.Play" ),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMediaPlayerActions::HandlePlayMovieActionExecute, MediaPlayers),
			FCanExecuteAction()
		)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MediaPlayer_PauseMovie", "Pause Movie"),
		LOCTEXT("MediaPlayer_PauseMovieToolTip", "Pauses playback of the media."),
		FSlateIcon( FEditorStyle::GetStyleSetName(), "MediaAsset.AssetActions.Pause" ),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMediaPlayerActions::HandlePauseMovieActionExecute, MediaPlayers),
			FCanExecuteAction()
		)
	);

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MediaPlayer_CreateMediaSoundWave", "Create Media Sound Wave"),
		LOCTEXT("MediaPlayer_CreateMediaSoundWaveTooltip", "Creates a new MediaSoundWave using this MediaPlayer asset."),
		FSlateIcon( FEditorStyle::GetStyleSetName(), "ClassIcon.MediaSoundWave" ),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMediaPlayerActions::ExecuteCreateMediaSoundWave, MediaPlayers),
			FCanExecuteAction()
		)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MediaPlayer_CreateMediaTexture", "Create Media Texture"),
		LOCTEXT("MediaPlayer_CreateMediaTextureTooltip", "Creates a new MediaTexture using this MediaPlayer asset."),
		FSlateIcon( FEditorStyle::GetStyleSetName(), "ClassIcon.MediaTexture" ),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMediaPlayerActions::ExecuteCreateMediaTexture, MediaPlayers),
			FCanExecuteAction()
		)
	);

	FAssetTypeActions_Base::GetActions(InObjects, MenuBuilder);
}


uint32 FMediaPlayerActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}


FText FMediaPlayerActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MediaPlayer", "Media Player");
}


UClass* FMediaPlayerActions::GetSupportedClass() const
{
	return UMediaPlayer::StaticClass();
}


FColor FMediaPlayerActions::GetTypeColor() const
{
	return FColor::Red;
}


bool FMediaPlayerActions::HasActions( const TArray<UObject*>& InObjects ) const
{
	return true;
}


void FMediaPlayerActions::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid()
		? EToolkitMode::WorldCentric
		: EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto MediaPlayer = Cast<UMediaPlayer>(*ObjIt);

		if (MediaPlayer != nullptr)
		{
			TSharedRef<FMediaPlayerEditorToolkit> EditorToolkit = MakeShareable(new FMediaPlayerEditorToolkit(Style));
			EditorToolkit->Initialize(MediaPlayer, Mode, EditWithinLevelEditor);
		}
	}
}


/* FAssetTypeActions_MediaPlayer callbacks
 *****************************************************************************/

void FMediaPlayerActions::ExecuteCreateMediaSoundWave( TArray<TWeakObjectPtr<UMediaPlayer>> Objects )
{
	IContentBrowserSingleton& ContentBrowserSingleton = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	const FString DefaultSuffix = TEXT("_Wav");

	if (Objects.Num() == 1)
	{
		auto Object = Objects[0].Get();

		if (Object != nullptr)
		{
			// Determine an appropriate name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			UMediaSoundWaveFactoryNew* Factory = NewObject<UMediaSoundWaveFactoryNew>();
			Factory->InitialMediaPlayer = Object;

			ContentBrowserSingleton.CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), UMediaSoundWave::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;

		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();

			if (Object != nullptr)
			{
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				UMediaSoundWaveFactoryNew* Factory = NewObject<UMediaSoundWaveFactoryNew>();
				Factory->InitialMediaPlayer = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UMediaSoundWave::StaticClass(), Factory);

				if (NewAsset != nullptr)
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if (ObjectsToSync.Num() > 0)
		{
			ContentBrowserSingleton.SyncBrowserToAssets(ObjectsToSync);
		}
	}
}


void FMediaPlayerActions::ExecuteCreateMediaTexture( TArray<TWeakObjectPtr<UMediaPlayer>> Objects )
{
	IContentBrowserSingleton& ContentBrowserSingleton = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	const FString DefaultSuffix = TEXT("_Tex");

	if (Objects.Num() == 1)
	{
		auto Object = Objects[0].Get();

		if (Object != nullptr)
		{
			// Determine an appropriate name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			UMediaTextureFactoryNew* Factory = NewObject<UMediaTextureFactoryNew>();
			Factory->InitialMediaPlayer = Object;

			ContentBrowserSingleton.CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), UMediaTexture::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;

		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();

			if (Object != nullptr)
			{
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				UMediaTextureFactoryNew* Factory = NewObject<UMediaTextureFactoryNew>();
				Factory->InitialMediaPlayer = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UMediaTexture::StaticClass(), Factory);

				if (NewAsset != nullptr)
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if (ObjectsToSync.Num() > 0)
		{
			ContentBrowserSingleton.SyncBrowserToAssets(ObjectsToSync);
		}
	}
}


void FMediaPlayerActions::HandlePauseMovieActionExecute( TArray<TWeakObjectPtr<UMediaPlayer>> Objects )
{
	for (auto MediaPlayer : Objects)
	{
		if (MediaPlayer.IsValid())
		{
			MediaPlayer->Pause();
		}
	}
}


void FMediaPlayerActions::HandlePlayMovieActionExecute( TArray<TWeakObjectPtr<UMediaPlayer>> Objects )
{
	for (auto MediaPlayer : Objects)
	{
		if (MediaPlayer.IsValid())
		{
			MediaPlayer->Play();
		}
	}
}


#undef LOCTEXT_NAMESPACE
