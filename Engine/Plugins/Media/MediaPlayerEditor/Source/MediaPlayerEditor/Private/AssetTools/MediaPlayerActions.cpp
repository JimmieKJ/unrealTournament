// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPCH.h"
#include "MediaPlayerActions.h"
#include "MediaPlayerEditorToolkit.h"
#include "MediaSoundWaveFactoryNew.h"
#include "MediaTextureFactoryNew.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"


/* FMediaPlayerActions constructors
 *****************************************************************************/

FMediaPlayerActions::FMediaPlayerActions(const TSharedRef<ISlateStyle>& InStyle)
	: Style(InStyle)
{ }


/* FAssetTypeActions_Base interface
 *****************************************************************************/

bool FMediaPlayerActions::CanFilter()
{
	return true;
}


void FMediaPlayerActions::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	FAssetTypeActions_Base::GetActions(InObjects, MenuBuilder);

	auto MediaPlayers = GetTypedWeakObjectPtrs<UMediaPlayer>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MediaPlayer_CreateMediaSoundWave", "Create Media Sound Wave"),
		LOCTEXT("MediaPlayer_CreateMediaSoundWaveTooltip", "Creates a new MediaSoundWave and assigns it to this MediaPlayer asset."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.MediaSoundWave"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMediaPlayerActions::ExecuteCreateMediaSoundWave, MediaPlayers),
			FCanExecuteAction()
		)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MediaPlayer_CreateMediaTexture", "Create Media Texture"),
		LOCTEXT("MediaPlayer_CreateMediaTextureTooltip", "Creates a new MediaTexture and assigns it to this MediaPlayer asset."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.MediaTexture"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMediaPlayerActions::ExecuteCreateMediaTexture, MediaPlayers),
			FCanExecuteAction()
		)
	);
}


uint32 FMediaPlayerActions::GetCategories()
{
	return EAssetTypeCategories::Media;
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


bool FMediaPlayerActions::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}


void FMediaPlayerActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
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

void FMediaPlayerActions::ExecuteCreateMediaSoundWave(TArray<TWeakObjectPtr<UMediaPlayer>> Objects)
{
	const FString DefaultSuffix = TEXT("_Sound");

	IContentBrowserSingleton& ContentBrowserSingleton = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	TArray<UObject*> ObjectsToSync;

	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto MediaPlayer = (*ObjIt).Get();

		if (MediaPlayer == nullptr)
		{
			continue;
		}

		// generate unique name
		FString Name;
		FString PackageName;

		CreateUniqueAssetName(MediaPlayer->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

		// create asset
		auto Factory = NewObject<UMediaSoundWaveFactoryNew>();

		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UMediaSoundWave::StaticClass(), Factory);

		if (NewAsset != nullptr)
		{
			MediaPlayer->SetSoundWave(Cast<UMediaSoundWave>(NewAsset));
			ObjectsToSync.Add(NewAsset);
		}
	}

	if (ObjectsToSync.Num() > 0)
	{
		ContentBrowserSingleton.SyncBrowserToAssets(ObjectsToSync);
	}
}


void FMediaPlayerActions::ExecuteCreateMediaTexture(TArray<TWeakObjectPtr<UMediaPlayer>> Objects)
{
	const FString DefaultSuffix = TEXT("_Video");

	IContentBrowserSingleton& ContentBrowserSingleton = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	TArray<UObject*> ObjectsToSync;

	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto MediaPlayer = (*ObjIt).Get();

		if (MediaPlayer == nullptr)
		{
			continue;
		}

		// generate unique name
		FString Name;
		FString PackageName;

		CreateUniqueAssetName(MediaPlayer->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

		// create asset
		auto Factory = NewObject<UMediaTextureFactoryNew>();

		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UMediaTexture::StaticClass(), Factory);

		if (NewAsset != nullptr)
		{
			MediaPlayer->SetVideoTexture(Cast<UMediaTexture>(NewAsset));
			ObjectsToSync.Add(NewAsset);
		}
	}

	if (ObjectsToSync.Num() > 0)
	{
		ContentBrowserSingleton.SyncBrowserToAssets(ObjectsToSync);
	}
}


#undef LOCTEXT_NAMESPACE
