// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "ISequencerSection.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "MovieSceneTrack.h"
#include "MovieSceneAudioTrack.h"
#include "ScopedTransaction.h"
#include "ISequencerObjectChangeListener.h"
#include "ISectionLayoutBuilder.h"
#include "IKeyArea.h"
#include "MovieSceneToolHelpers.h"
#include "MovieSceneTrackEditor.h"
#include "AudioTrackEditor.h"
#include "MovieSceneAudioSection.h"
#include "CommonMovieSceneTools.h"
#include "SoundDefinitions.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Runtime/Engine/Public/Slate/SlateTextures.h"
#include "ObjectTools.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "Runtime/MovieSceneCoreTypes/Classes/MovieSceneShotSection.h"
#include "Runtime/Engine/Public/AudioDecompress.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"


namespace AnimatableAudioEditorConstants
{
	// @todo Sequencer Allow this to be customizable
	const uint32 AudioTrackHeight = 50;

	// Optimization - maximum samples per pixel this sound allows
	const uint32 MaxSamplesPerPixel = 60;
}



/** These utility functions should go away once we start handling sound cues properly */
USoundWave* DeriveSoundWave(UMovieSceneAudioSection* AudioSection)
{
	USoundWave* SoundWave = NULL;
	
	USoundBase* Sound = AudioSection->GetSound();

	if (Sound->IsA<USoundWave>())
	{
		SoundWave = Cast<USoundWave>(Sound);
	}
	else if (Sound->IsA<USoundCue>())
	{
		USoundCue* SoundCue = Cast<USoundCue>(Sound);

		// @todo Sequencer - Right now for sound cues, we just use the first sound wave in the cue
		// In the future, it would be better to properly generate the sound cue's data after forcing determinism
		const TArray<USoundNode*>& AllNodes = SoundCue->AllNodes;
		for (int32 i = 0; i < AllNodes.Num(); ++i)
		{
			if (AllNodes[i]->IsA<USoundNodeWavePlayer>())
			{
				SoundWave = Cast<USoundNodeWavePlayer>(AllNodes[i])->SoundWave;
				break;
			}
		}
	}

	return SoundWave;
}

float DeriveUnloopedDuration(UMovieSceneAudioSection* AudioSection)
{
	USoundWave* SoundWave = DeriveSoundWave(AudioSection);

	float Duration = SoundWave->GetDuration();
	return Duration == INDEFINITELY_LOOPING_DURATION ? SoundWave->Duration : Duration;
}



/**
 * The audio thumnail, which holds a texture which it can pass back to a viewport to render
 */
class FAudioThumbnail : public ISlateViewport, public TSharedFromThis<FAudioThumbnail>
{
public:
	FAudioThumbnail(UMovieSceneSection& InSection, TRange<float> DrawRange, int InTextureSize);
	~FAudioThumbnail();

	/* ISlateViewport interface */
	virtual FIntPoint GetSize() const override;
	virtual class FSlateShaderResource* GetViewportRenderTargetTexture() const override;
	virtual bool RequiresVsync() const override;

	/** Returns whether this thumbnail has a texture to render */
	virtual bool ShouldRender() const {return TextureSize > 0;}
	
private:
	/** Generates the waveform preview and dumps it out to the OutBuffer */
	void GenerateWaveformPreview(TArray<uint8>& OutBuffer, TRange<float> DrawRange);

	/** Given a buffer of data, a PCM lookup, draws a line of the PCM waveform at the specified X of the data buffer */
	void DrawWaveformLine(TArray<uint8>& OutData, int32 NumChannels, int16* LookupData, int32 LookupStartIndex, int32 LookupEndIndex, int32 LookupSize, int32 X, int32 XAxisHeight, int32 MaxAmplitude);

private:
	/** The section we are visualizing */
	UMovieSceneSection& Section;

	/** The Texture RHI that holds the thumbnail */
	class FSlateTexture2DRHIRef* Texture;

	/** Size of the texture */
	int32 TextureSize;
};

FAudioThumbnail::FAudioThumbnail(UMovieSceneSection& InSection, TRange<float> DrawRange, int32 InTextureSize)
	: Section(InSection)
	, Texture(NULL)
	, TextureSize(InTextureSize)
{
	UMovieSceneAudioSection* AudioSection = Cast<UMovieSceneAudioSection>(&Section);
	
	if (ShouldRender())
	{
		uint32 Size = GetSize().X * GetSize().Y * GPixelFormats[PF_B8G8R8A8].BlockBytes;
		TArray<uint8> RawData;
		RawData.AddZeroed(Size);

		GenerateWaveformPreview(RawData, DrawRange);

		FSlateTextureDataPtr BulkData(new FSlateTextureData(GetSize().X, GetSize().Y, GPixelFormats[PF_B8G8R8A8].BlockBytes, RawData));

		Texture = new FSlateTexture2DRHIRef(GetSize().X, GetSize().Y, PF_B8G8R8A8, BulkData, TexCreate_Dynamic);

		BeginInitResource( Texture );
	}
}

FAudioThumbnail::~FAudioThumbnail()
{
	if (ShouldRender())
	{
		BeginReleaseResource( Texture );

		FlushRenderingCommands();
	}

	if (Texture) 
	{
		delete Texture;
	}
}

FIntPoint FAudioThumbnail::GetSize() const {return FIntPoint(TextureSize, AnimatableAudioEditorConstants::AudioTrackHeight);}
FSlateShaderResource* FAudioThumbnail::GetViewportRenderTargetTexture() const {return Texture;}
bool FAudioThumbnail::RequiresVsync() const {return false;}

void FAudioThumbnail::GenerateWaveformPreview(TArray<uint8>& OutData, TRange<float> DrawRange)
{
	UMovieSceneAudioSection* AudioSection = Cast<UMovieSceneAudioSection>(&Section);

	USoundWave* SoundWave = DeriveSoundWave(AudioSection);
	check(SoundWave);
	
	check(SoundWave->NumChannels == 1 || SoundWave->NumChannels == 2);

	// @todo Sequencer - This may be fixed when we switch to double precision, but the waveform flickers a bit on resize
	// If double precision don't solve the issue, this system may have to be re-engineered

	// decompress PCM data if necessary
	if (SoundWave->RawPCMData == NULL)
	{
		// @todo Sequencer optimize - We might want to generate the data when we generate the texture
		// and then discard the data afterwards, though that might be a perf hit traded for better memory usage
		FAudioDevice* AudioDevice = GEngine->GetMainAudioDevice();
		if (AudioDevice)
		{
			SoundWave->InitAudioResource(AudioDevice->GetRuntimeFormat(SoundWave));
			FAsyncAudioDecompress TempDecompress(SoundWave);
			TempDecompress.StartSynchronousTask();
		}
	}

	int16* LookupData = (int16*)SoundWave->RawPCMData;
	int32 LookupDataSize = SoundWave->RawPCMDataSize;
	int32 LookupSize = LookupDataSize * sizeof(uint8) / sizeof(int16);

	// @todo Sequencer This fixes looping drawing by pretending we are only dealing with a SoundWave
	TRange<float> AudioTrueRange = TRange<float>(
		AudioSection->GetAudioStartTime(),
		AudioSection->GetAudioStartTime() + DeriveUnloopedDuration(AudioSection) * AudioSection->GetAudioDilationFactor());
	float TrueRangeSize = AudioTrueRange.Size<float>();

	float DrawRangeSize = DrawRange.Size<float>();

	for (int32 X = 0; X < GetSize().X; ++X)
	{
		float LookupTime = ((float)(X - 0.5f) / (float)GetSize().X) * DrawRangeSize + DrawRange.GetLowerBoundValue();
		float LookupFraction = (LookupTime - AudioTrueRange.GetLowerBoundValue()) / TrueRangeSize;
		int32 LookupIndex = FMath::TruncToInt(LookupFraction * LookupSize);
		
		float NextLookupTime = ((float)(X + 0.5f) / (float)GetSize().X) * DrawRangeSize + DrawRange.GetLowerBoundValue();
		float NextLookupFraction = (NextLookupTime - AudioTrueRange.GetLowerBoundValue()) / TrueRangeSize;
		int32 NextLookupIndex = FMath::TruncToInt(NextLookupFraction * LookupSize);

		DrawWaveformLine(OutData, SoundWave->NumChannels, LookupData, LookupIndex, NextLookupIndex, LookupSize, X, GetSize().Y / 2, GetSize().Y / 2);
	}
}

void FAudioThumbnail::DrawWaveformLine(TArray<uint8>& OutData, int32 NumChannels, int16* LookupData, int32 LookupStartIndex, int32 LookupEndIndex, int32 LookupSize, int32 X, int32 XAxisHeight, int32 MaxAmplitude)
{
	int32 ModifiedLookupStartIndex = NumChannels == 2 ? (LookupStartIndex % 2 == 0 ? LookupStartIndex : LookupStartIndex - 1) : LookupStartIndex;

	int32 ClampedStartIndex = FMath::Max(0, ModifiedLookupStartIndex);
	int32 ClampedEndIndex = FMath::Max(FMath::Min(LookupSize-1, LookupEndIndex), ClampedStartIndex + 1);
	
	int32 StepSize = NumChannels;

	// optimization - don't take more than a maximum number of samples per pixel
	int32 Range = ClampedEndIndex - ClampedStartIndex;
	int32 SampleCount = Range / StepSize;
	int32 MaxSampleCount = AnimatableAudioEditorConstants::MaxSamplesPerPixel;
	int32 ModifiedStepSize = (SampleCount > MaxSampleCount ? SampleCount / MaxSampleCount : 1) * StepSize;

	// Two techniques for rendering smooth waveforms: Summed Density Histograms and Root Mean Square
#if 1 // Summed Density Histogram
	struct FChannelData
	{
		FChannelData(int32 InMaxAmplitude)
			: HistogramSum(0)
		{
			Histogram.AddZeroed(InMaxAmplitude);
		}

		TArray<int32> Histogram;
		int32 HistogramSum;
	};
	TArray<FChannelData> ChannelData;
	for (int32 i = 0; i < NumChannels; ++i)
	{
		ChannelData.Add(FChannelData(MaxAmplitude));
	}

	// build up the histogram(s)
	for (int32 i = ClampedStartIndex; i < ClampedEndIndex; i += ModifiedStepSize)
	{
		for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
		{
			int32 DataPoint = LookupData[i + ChannelIndex];
			int32 HistogramData = FMath::Clamp(FMath::TruncToInt(FMath::Abs(DataPoint) / 32768.f * MaxAmplitude), 0, MaxAmplitude - 1);

			++ChannelData[ChannelIndex].Histogram[HistogramData];
			++ChannelData[ChannelIndex].HistogramSum;
		}
	}

	// output the data to the texture
	const int32 MaxSupportedChannels = 2;
	for (int32 ChannelIndex = 0; ChannelIndex < MaxSupportedChannels; ++ChannelIndex)
	{
		FChannelData& ChannelDataUsed = ChannelData[ChannelIndex % NumChannels];
		int32 IntensitySummation = 0;
		for (int32 PixelIndex = MaxAmplitude - 1; PixelIndex >= 0; --PixelIndex)
		{
			IntensitySummation += ChannelDataUsed.Histogram[PixelIndex];
			if (IntensitySummation > 0)
			{
				int32 AlphaIntensity = FMath::Clamp(IntensitySummation * 255 / ChannelDataUsed.HistogramSum, 0, 255);

				int32 Y = ChannelIndex == 0 ? XAxisHeight - PixelIndex : XAxisHeight + PixelIndex;
				int32 Index = (Y * GetSize().X + X) * GPixelFormats[PF_B8G8R8A8].BlockBytes;
				OutData[Index+0] = 255;
				OutData[Index+1] = 255;
				OutData[Index+2] = 255;
				OutData[Index+3] = AlphaIntensity;
			}
		}
	}
#else // Root Mean Square
	int32 Samples = 0;
	float SummedData = 0;
	int32 DataAmplitude = 0;
	for (int32 i = ClampedStartIndex; i < ClampedEndIndex; i += StepSize)
	{
		int32 DataPoint = LookupData[i];
		SummedData += DataPoint * DataPoint;
		++Samples;
		DataAmplitude = FMath::Max(DataAmplitude, FMath::Abs(DataPoint));
	}
	float RootMeanSquare = FMath::Sqrt(SummedData / (float)Samples);
	
	int32 RMSHeight = FMath::Clamp(FMath::TruncToInt(RootMeanSquare / 32768.f * MaxAmplitude), 1, MaxAmplitude);
	int32 MaxHeight = FMath::Clamp(FMath::TruncToInt(DataAmplitude / 32768.f * MaxAmplitude), 1, MaxAmplitude);
	float HeightRange = FMath::Max(1, MaxHeight - RMSHeight);
	
	for (int32 i = 0; i < MaxHeight; ++i)
	{
		float Intensity = (MaxHeight - i) / HeightRange;
		int32 IntegralIntensity = FMath::Clamp(FMath::TruncToInt(Intensity * 255), 0, 255);

		int32 Y = XAxisHeight - i;
		int32 Index = (Y * GetSize().X + X) * GPixelFormats[PF_B8G8R8A8].BlockBytes;
		OutData[Index+0] = 255;
		OutData[Index+1] = 255;
		OutData[Index+2] = 255;
		OutData[Index+3] = IntegralIntensity;

		Y = XAxisHeight + i;
		Index = (Y * GetSize().X + X) * GPixelFormats[PF_B8G8R8A8].BlockBytes;
		OutData[Index+0] = 255;
		OutData[Index+1] = 255;
		OutData[Index+2] = 255;
		OutData[Index+3] = IntegralIntensity;
	}
#endif
}



FAudioSection::FAudioSection( UMovieSceneSection& InSection, bool bOnAMasterTrack )
	: Section( InSection )
	, StoredDrawRange(TRange<float>::Empty())
	, bIsOnAMasterTrack(bOnAMasterTrack)
{
}

FAudioSection::~FAudioSection()
{
}

UMovieSceneSection* FAudioSection::GetSectionObject()
{ 
	return &Section;
}

FText FAudioSection::GetDisplayName() const
{
	return bIsOnAMasterTrack ? NSLOCTEXT("FAudioSection", "MasterAudioDisplayName", "Master Audio") :  NSLOCTEXT("FAudioSection", "AudioDisplayName", "Audio");
}

FText FAudioSection::GetSectionTitle() const
{
	return FText::FromString( Cast<UMovieSceneAudioSection>(&Section)->GetSound()->GetName() );
}

float FAudioSection::GetSectionHeight() const
{
	return (float)AnimatableAudioEditorConstants::AudioTrackHeight;
 }

int32 FAudioSection::OnPaintSection( const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled ) const
{
	// Add a box for the section
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		FEditorStyle::GetBrush("Sequencer.GenericSection.Background"),
		SectionClippingRect,
		ESlateDrawEffect::None,
		FLinearColor(0.4f, 0.8f, 0.4f, 1.f)
	);

	if (WaveformThumbnail.IsValid() && WaveformThumbnail->ShouldRender())
	{
		// @todo Sequencer draw multiple times if looping possibly - requires some thought about SoundCues
		FSlateDrawElement::MakeViewport(
			OutDrawElements,
			LayerId+1,
			AllottedGeometry.ToPaintGeometry(FVector2D(StoredXOffset, 0), FVector2D(StoredXSize, AllottedGeometry.Size.Y)),
			WaveformThumbnail,
			SectionClippingRect,
			false,
			false,
			ESlateDrawEffect::None,
			FLinearColor(1.f, 1.f, 1.f, 0.9f)
		);
	}

	return LayerId+1;
}

void FAudioSection::Tick( const FGeometry& AllottedGeometry, const FGeometry& ParentGeometry, const double InCurrentTime, const float InDeltaTime )
{
	UMovieSceneAudioSection* AudioSection = Cast<UMovieSceneAudioSection>(&Section);

	USoundWave* SoundWave = DeriveSoundWave(AudioSection);
	if (SoundWave->NumChannels == 1 || SoundWave->NumChannels == 2)
	{
		FTimeToPixel TimeToPixelConverter( AllottedGeometry, TRange<float>( AudioSection->GetStartTime(), AudioSection->GetEndTime() ) );

		TRange<float> VisibleRange = TRange<float>(
			TimeToPixelConverter.PixelToTime(-AllottedGeometry.Position.X),
			TimeToPixelConverter.PixelToTime(-AllottedGeometry.Position.X + ParentGeometry.Size.X));
		TRange<float> SectionRange = AudioSection->GetRange();
		TRange<float> AudioRange = TRange<float>(
			AudioSection->GetAudioStartTime(),
			AudioSection->GetAudioStartTime() + DeriveUnloopedDuration(AudioSection) * AudioSection->GetAudioDilationFactor());

		TRange<float> DrawRange = TRange<float>::Intersection(TRange<float>::Intersection(SectionRange, AudioRange), VisibleRange);
	
		TRange<int32> PixelRange = TRange<int32>(
			TimeToPixelConverter.TimeToPixel(DrawRange.GetLowerBoundValue()),
			TimeToPixelConverter.TimeToPixel(DrawRange.GetUpperBoundValue()));
		
		// generate texture x offset and x size
		int32 XOffset = PixelRange.GetLowerBoundValue() - TimeToPixelConverter.TimeToPixel(SectionRange.GetLowerBoundValue());
		int32 XSize = PixelRange.Size<int32>();

		if (!FMath::IsNearlyEqual(DrawRange.GetLowerBoundValue(), StoredDrawRange.GetLowerBoundValue()) ||
			!FMath::IsNearlyEqual(DrawRange.GetUpperBoundValue(), StoredDrawRange.GetUpperBoundValue()) ||
			XOffset != StoredXOffset || XSize != StoredXSize)
		{
			RegenerateWaveforms(DrawRange, XOffset, XSize);
		}
	}
}

void FAudioSection::RegenerateWaveforms(TRange<float> DrawRange, int32 XOffset, int32 XSize)
{
	StoredDrawRange = DrawRange;
	StoredXOffset = XOffset;
	StoredXSize = XSize;
	
	if (DrawRange.IsDegenerate() || DrawRange.IsEmpty() || Cast<UMovieSceneAudioSection>(&Section)->GetSound() == NULL)
	{
		WaveformThumbnail.Reset();
	}
	else
	{
		WaveformThumbnail = MakeShareable(new FAudioThumbnail(Section, DrawRange, XSize));
	}
}



FAudioTrackEditor::FAudioTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMovieSceneTrackEditor( InSequencer ) 
{
}

FAudioTrackEditor::~FAudioTrackEditor()
{
}

TSharedRef<FMovieSceneTrackEditor> FAudioTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FAudioTrackEditor( InSequencer ) );
}

bool FAudioTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	return Type == UMovieSceneAudioTrack::StaticClass();
}

TSharedRef<ISequencerSection> FAudioTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack* Track )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );
	
	bool bIsAMasterTrack = Cast<UMovieScene>(Track->GetOuter())->IsAMasterTrack(Track);
	TSharedRef<ISequencerSection> NewSection( new FAudioSection(SectionObject, bIsAMasterTrack) );

	return NewSection;
}

bool FAudioTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	if (Asset->IsA<USoundBase>())
	{
		auto Sound = Cast<USoundBase>(Asset);
		
		if (TargetObjectGuid.IsValid())
		{
			TArray<UObject*> OutObjects;
			GetSequencer()->GetRuntimeObjects(GetSequencer()->GetFocusedMovieSceneInstance(), TargetObjectGuid, OutObjects);

			AnimatablePropertyChanged(UMovieSceneAudioTrack::StaticClass(), false,
				FOnKeyProperty::CreateRaw(this, &FAudioTrackEditor::AddNewAttachedSound, Sound, OutObjects));
		}
		else
		{
			AnimatablePropertyChanged(UMovieSceneAudioTrack::StaticClass(), false,
				FOnKeyProperty::CreateRaw(this, &FAudioTrackEditor::AddNewMasterSound, Sound));
		}

		return true;
	}
	return false;
}

void FAudioTrackEditor::AddNewMasterSound( float KeyTime, USoundBase* Sound )
{
	UMovieSceneTrack* Track = GetMasterTrack( UMovieSceneAudioTrack::StaticClass() );

	auto AudioTrack = Cast<UMovieSceneAudioTrack>(Track);
	AudioTrack->AddNewSound( Sound, KeyTime );
}

void FAudioTrackEditor::AddNewAttachedSound( float KeyTime, USoundBase* Sound, TArray<UObject*> ObjectsToAttachTo )
{
	for( int32 ObjectIndex = 0; ObjectIndex < ObjectsToAttachTo.Num(); ++ObjectIndex )
	{
		UObject* Object = ObjectsToAttachTo[ObjectIndex];

		FGuid ObjectHandle = FindOrCreateHandleToObject( Object );
		if (ObjectHandle.IsValid())
		{
			
			UMovieSceneTrack* Track = GetTrackForObject( ObjectHandle, UMovieSceneAudioTrack::StaticClass(), AudioTrackConstants::UniqueTrackName );

			if (ensure(Track))
			{
				Cast<UMovieSceneAudioTrack>(Track)->AddNewSound( Sound, KeyTime );
			}
		}
	}
}
