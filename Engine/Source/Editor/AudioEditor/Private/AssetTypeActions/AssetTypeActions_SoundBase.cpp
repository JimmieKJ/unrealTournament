// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SoundBase.h"
#include "Sound/SoundBase.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Editor.h"
#include "EditorStyleSet.h"
#include "Components/AudioComponent.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundBase::GetSupportedClass() const
{
	return USoundBase::StaticClass();
}

void FAssetTypeActions_SoundBase::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto Sounds = GetTypedWeakObjectPtrs<USoundBase>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Sound_PlaySound", "Play"),
		LOCTEXT("Sound_PlaySoundTooltip", "Plays the selected sound."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "MediaAsset.AssetActions.Play"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_SoundBase::ExecutePlaySound, Sounds ),
			FCanExecuteAction::CreateSP( this, &FAssetTypeActions_SoundBase::CanExecutePlayCommand, Sounds )
			)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Sound_StopSound", "Stop"),
		LOCTEXT("Sound_StopSoundTooltip", "Stops the selected sounds."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "MediaAsset.AssetActions.Stop"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_SoundBase::ExecuteStopSound, Sounds ),
			FCanExecuteAction()
			)
		);
}

bool FAssetTypeActions_SoundBase::CanExecutePlayCommand(TArray<TWeakObjectPtr<USoundBase>> Objects) const
{
	return Objects.Num() == 1;
}

void FAssetTypeActions_SoundBase::AssetsActivated( const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType )
{
	if (ActivationType == EAssetTypeActivationMethod::Previewed)
	{
		USoundBase* TargetSound = NULL;

		for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			TargetSound = Cast<USoundBase>(*ObjIt);
			if ( TargetSound )
			{
				// Only target the first valid sound cue
				break;
			}
		}

		UAudioComponent* PreviewComp = GEditor->GetPreviewAudioComponent();
		if ( PreviewComp && PreviewComp->IsPlaying() )
		{
			// Already previewing a sound, if it is the target cue then stop it, otherwise play the new one
			if ( !TargetSound || PreviewComp->Sound == TargetSound )
			{
				StopSound();
			}
			else
			{
				PlaySound(TargetSound);
			}
		}
		else
		{
			// Not already playing, play the target sound cue if it exists
			PlaySound(TargetSound);
		}
	}
	else
	{
		FAssetTypeActions_Base::AssetsActivated(InObjects, ActivationType);
	}
}

void FAssetTypeActions_SoundBase::ExecutePlaySound(TArray<TWeakObjectPtr<USoundBase>> Objects) const
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		USoundBase* Sound = (*ObjIt).Get();
		if ( Sound )
		{
			// Only play the first valid sound
			PlaySound(Sound);
			break;
		}
	}
}

void FAssetTypeActions_SoundBase::ExecuteStopSound(TArray<TWeakObjectPtr<USoundBase>> Objects) const
{
	StopSound();
}

void FAssetTypeActions_SoundBase::PlaySound(USoundBase* Sound) const
{
	if ( Sound )
	{
		GEditor->PlayPreviewSound(Sound);
	}
	else
	{
		StopSound();
	}
}

void FAssetTypeActions_SoundBase::StopSound() const
{
	GEditor->ResetPreviewAudioComponent();
}

bool FAssetTypeActions_SoundBase::IsSoundPlaying(TArray<TWeakObjectPtr<USoundBase>> Objects) const
{
	UAudioComponent* PreviewComp = GEditor->GetPreviewAudioComponent();
	if (PreviewComp)
	{
		for (TWeakObjectPtr<USoundBase> SoundItem : Objects)
		{
			if (PreviewComp->Sound == SoundItem && PreviewComp->IsPlaying())
			{
				return true;
			}
		}
	}

	return false;
}

TSharedPtr<SWidget> FAssetTypeActions_SoundBase::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	TArray<TWeakObjectPtr<USoundBase>> SoundList;
	SoundList.Add(TWeakObjectPtr<USoundBase>((USoundBase*)AssetData.GetAsset()));

	auto OnGetDisplayBrushLambda = [this, SoundList]() -> const FSlateBrush*
	{
		if (IsSoundPlaying(SoundList))
		{
			return FEditorStyle::GetBrush("MediaAsset.AssetActions.Stop");
		}

	return FEditorStyle::GetBrush("MediaAsset.AssetActions.Play");
	};

	auto IsEnabledLambda = [this, SoundList]() -> bool
	{
		return CanExecutePlayCommand(SoundList);
	};

	auto OnClickedLambda = [this, SoundList]() -> FReply
	{
		if (IsSoundPlaying(SoundList))
		{
			ExecuteStopSound(SoundList);
		}
		else
		{
			ExecutePlaySound(SoundList);
		}
		return FReply::Handled();
	};

	auto OnToolTipTextLambda = [this, SoundList]() -> FText
	{
		if (IsSoundPlaying(SoundList))
		{
			return LOCTEXT("Thumbnail_StopSoundToolTip", "Stop selected sound");
		}

		return LOCTEXT("Thumbnail_PlaySoundToolTip", "Play selected sound");
	};

	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(SButton)
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.ToolTipText_Lambda(OnToolTipTextLambda)
			.Cursor(EMouseCursor::Default) // The outer widget can specify a DragHand cursor, so we need to override that here
			.ForegroundColor(FSlateColor::UseForeground())		
			.IsEnabled_Lambda(IsEnabledLambda)
			.OnClicked_Lambda(OnClickedLambda)
			[
				SNew(SBox)
				.MinDesiredWidth(16)
				.MinDesiredHeight(16)
				[
					SNew(SImage)
					.Image_Lambda(OnGetDisplayBrushLambda)
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
