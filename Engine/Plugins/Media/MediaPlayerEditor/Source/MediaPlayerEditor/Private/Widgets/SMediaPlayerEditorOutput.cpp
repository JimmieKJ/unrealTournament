// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPCH.h"
#include "SMediaPlayerEditorOutput.h"


/* SMediaPlayerEditorOutput structors
 *****************************************************************************/

SMediaPlayerEditorOutput::SMediaPlayerEditorOutput()
	: CurrentSoundWave(nullptr)
	, CurrentTexture(nullptr)
	, DefaultSoundWave(nullptr)
	, DefaultTexture(nullptr)
	, Material(nullptr)
	, MediaPlayer(nullptr)
{
	DefaultSoundWave = NewObject<UMediaSoundWave>(GetTransientPackage(), NAME_None, RF_Transient | RF_Public);

	if (DefaultSoundWave != nullptr)
	{
		DefaultSoundWave->AddToRoot();

		AudioComponent = FAudioDevice::CreateComponent(DefaultSoundWave, nullptr, nullptr, false, false);
	
		if (AudioComponent != nullptr)
		{
			AudioComponent->SetVolumeMultiplier(1.0f);
			AudioComponent->SetPitchMultiplier(1.0f);
			AudioComponent->bAllowSpatialization = false;
			AudioComponent->bIsUISound = true;
			AudioComponent->bAutoDestroy = false;
			AudioComponent->AddToRoot();
		}
	}
}


SMediaPlayerEditorOutput::~SMediaPlayerEditorOutput()
{
	if (MediaPlayer != nullptr)
	{
		MediaPlayer->OnMediaEvent().RemoveAll(this);
	
		// remove default sinks from native player
		TSharedPtr<IMediaPlayer> Player = MediaPlayer->GetPlayer();

		if (Player.IsValid())
		{
			IMediaOutput& Output = Player->GetOutput();

			if (MediaPlayer->GetSoundWave() == nullptr)
			{
				Output.SetAudioSink(nullptr);
			}

			if (MediaPlayer->GetVideoTexture() == nullptr)
			{
				Output.SetVideoSink(nullptr);
			}
		}
	}

	// release default sinks
	if (AudioComponent != nullptr)
	{
		AudioComponent->Stop();
		AudioComponent->RemoveFromRoot();
	}

	if (Material != nullptr)
	{
		Material->RemoveFromRoot();
	}

	if (DefaultSoundWave != nullptr)
	{
		DefaultSoundWave->RemoveFromRoot();
	}

	if (DefaultTexture != nullptr)
	{
		DefaultTexture->RemoveFromRoot();
	}
}


/* SMediaPlayerEditorOutput interface
 *****************************************************************************/

void SMediaPlayerEditorOutput::Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer)
{
	MediaPlayer = &InMediaPlayer;

	// create wrapper material
	Material = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient | RF_Public);

	if (Material != nullptr)
	{
		TextureSampler = NewObject<UMaterialExpressionTextureSample>(Material);
		{
			TextureSampler->SamplerType = SAMPLERTYPE_Color;
		}

		FExpressionOutput& Output = TextureSampler->GetOutputs()[0];
		FExpressionInput& Input = Material->EmissiveColor;
		{
			Input.Expression = TextureSampler;
			Input.Mask = Output.Mask;
			Input.MaskR = Output.MaskR;
			Input.MaskG = Output.MaskG;
			Input.MaskB = Output.MaskB;
			Input.MaskA = Output.MaskA;
		}

		Material->Expressions.Add(TextureSampler);
		Material->MaterialDomain = EMaterialDomain::MD_UI;
		Material->AddToRoot();
	}

	// create Slate brush
	MaterialBrush = MakeShareable(new FSlateBrush());
	{
		MaterialBrush->SetResourceObject(Material);
	}

	UpdateMaterial();

	ChildSlot
	[
		SNew(SImage)
			.Image(MaterialBrush.Get())
	];

	MediaPlayer->OnMediaEvent().AddRaw(this, &SMediaPlayerEditorOutput::HandleMediaPlayerMediaEvent);
}


/* SWidget interface
 *****************************************************************************/

void SMediaPlayerEditorOutput::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	UpdateMaterial();
	UpdateSoundWave();
}


/* SMediaPlayerEditorOutput implementation
 *****************************************************************************/

void SMediaPlayerEditorOutput::UpdateMaterial()
{
	UMediaTexture* Texture = MediaPlayer->GetVideoTexture();

	if (Texture == nullptr)
	{
		// create default texture if needed
		if (DefaultTexture == nullptr)
		{
			DefaultTexture = NewObject<UMediaTexture>(GetTransientPackage(), NAME_None, RF_Transient | RF_Public);
			
			if (DefaultTexture != nullptr)
			{
				DefaultTexture->UpdateResource();
				DefaultTexture->AddToRoot();
			}
		}

		// set default texture as output sink
		if (DefaultTexture != nullptr)
		{
			TSharedPtr<IMediaPlayer> Player = MediaPlayer->GetPlayer();

			if (Player.IsValid())
			{
				Player->GetOutput().SetVideoSink(DefaultTexture);
			}
		}

		Texture = DefaultTexture;
	}

	// assign new texture to material
	if (Texture != CurrentTexture)
	{
		TextureSampler->Texture = Texture;
		Material->PostEditChange();
		CurrentTexture = Texture;
	}

	// resize current texture
	if (CurrentTexture != nullptr)
	{
		MaterialBrush->ImageSize.X = CurrentTexture->GetSurfaceWidth();
		MaterialBrush->ImageSize.Y = CurrentTexture->GetSurfaceHeight();
	}
	else
	{
		MaterialBrush->ImageSize = FVector2D::ZeroVector;
	}
}


void SMediaPlayerEditorOutput::UpdateSoundWave()
{
	if (AudioComponent == nullptr)
	{
		return;
	}

	if (GEditor->PlayWorld == nullptr)
	{
		UMediaSoundWave* SoundWave = MediaPlayer->GetSoundWave();

		if (SoundWave == nullptr)
		{
			// set default sound wave as output sink
			if (DefaultSoundWave != nullptr)
			{
				TSharedPtr<IMediaPlayer> Player = MediaPlayer->GetPlayer();

				if (Player.IsValid())
				{
					Player->GetOutput().SetAudioSink(DefaultSoundWave);
				}
			}

			SoundWave = DefaultSoundWave;
		}

		// assign new sound wave to audio component
		if (SoundWave != CurrentSoundWave)
		{
			CurrentSoundWave = SoundWave;
			AudioComponent->SetSound(SoundWave);
		}
	}
	else
	{
		// no audio in PIE, PIV1, Simulate
		AudioComponent->SetSound(nullptr);
		CurrentSoundWave = nullptr;
	}
}


/* SMediaPlayerEditorOutput callbacks
 *****************************************************************************/

void SMediaPlayerEditorOutput::HandleMediaPlayerMediaEvent(EMediaEvent Event)
{
	if ((AudioComponent == nullptr) || !CurrentSoundWave.IsValid())
	{
		return;
	}

	switch (Event)
	{
	case EMediaEvent::PlaybackEndReached:
	case EMediaEvent::PlaybackSuspended:
		AudioComponent->Stop();
		break;

	case EMediaEvent::PlaybackResumed:
		AudioComponent->Play();
		break;
	}
}
