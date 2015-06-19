// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "Editor/SoundCueEditor/Public/SoundCueEditorModule.h"
#include "SoundDefinitions.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"

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

void FAssetTypeActions_SoundBase::ExecutePlaySound(TArray<TWeakObjectPtr<USoundBase>> Objects)
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

void FAssetTypeActions_SoundBase::ExecuteStopSound(TArray<TWeakObjectPtr<USoundBase>> Objects)
{
	StopSound();
}

void FAssetTypeActions_SoundBase::PlaySound(USoundBase* Sound)
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

void FAssetTypeActions_SoundBase::StopSound()
{
	GEditor->ResetPreviewAudioComponent();
}

#undef LOCTEXT_NAMESPACE
