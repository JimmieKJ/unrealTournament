// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ActiveSound.h"
#include "Sound/DialogueWave.h"
#include "Sound/DialogueSoundWaveProxy.h"
#include "Sound/SoundWave.h"
#include "SubtitleManager.h"
#include "GatherableTextData.h"
#include "InternationalizationMetadata.h"
#include "Sound/DialogueVoice.h"

#if WITH_EDITORONLY_DATA
namespace
{
	// Class that takes DialogueWaves and prepares internationalization manifest entries 
	class FDialogueHelper
	{
	public:
		bool ProcessDialogueWave( const UDialogueWave* DialogueWave );
		const FString& GetSpokenSource() const { return SpokenSource; }

		const FTextSourceSiteContext& GetBaseContext() const { return Base; };
		const TArray<FTextSourceSiteContext>& GetContextSpecificVariations() const { return ContextSpecificVariations; }

	private:
		TSharedPtr< FLocMetadataValue > GenSourceTargetMetadata( const FString& SpeakerName, const TArray< FString >& TargetNames, bool bCompact = true );
		FString ArrayMetaDataToString( const TArray< TSharedPtr< FLocMetadataValue > >& MetadataArray );
		FString GetDialogueVoiceName( const UDialogueVoice* DialogueVoice );
		FString GetGrammaticalGenderString( TEnumAsByte<EGrammaticalGender::Type> Gender );
		FString GetGrammaticalNumberString( TEnumAsByte<EGrammaticalNumber::Type> Plurality );

	public:
		static const FString DialogueNamespace;
		static const FString PropertyName_VoiceActorDirection;
		static const FString PropertyName_Speaker;
		static const FString PropertyName_Speakers;
		static const FString PropertyName_Targets;
		static const FString PropertyName_GrammaticalGender;
		static const FString PropertyName_GrammaticalPlurality;
		static const FString PropertyName_TargetGrammaticalGender;
		static const FString PropertyName_TargetGrammaticalNumber;
		static const FString PropertyName_Optional;
		static const FString PropertyName_DialogueVariations;
		static const FString PropertyName_IsMature;

	private:
		FString DialogueKey;
		FString SourceLocation;
		FString SpokenSource;
		FString VoiceActorDirection;
		bool bIsMature;

		// The non-optional entry
		FTextSourceSiteContext Base;
		// Optional entries that are context specific
		TArray< FTextSourceSiteContext > ContextSpecificVariations;
	};

	const FString FDialogueHelper::DialogueNamespace						= TEXT("");
	const FString FDialogueHelper::PropertyName_VoiceActorDirection		= TEXT("Voice Actor Direction");
	const FString FDialogueHelper::PropertyName_Speaker					= TEXT("Speaker");
	const FString FDialogueHelper::PropertyName_Speakers					= TEXT("Speakers");
	const FString FDialogueHelper::PropertyName_Targets					= TEXT("Targets");
	const FString FDialogueHelper::PropertyName_GrammaticalGender			= TEXT("Gender");
	const FString FDialogueHelper::PropertyName_GrammaticalPlurality		= TEXT("Plurality");
	const FString FDialogueHelper::PropertyName_TargetGrammaticalGender	= TEXT("TargetGender");
	const FString FDialogueHelper::PropertyName_TargetGrammaticalNumber	= TEXT("TargetPlurality");
	const FString FDialogueHelper::PropertyName_Optional					= TEXT("Optional");
	const FString FDialogueHelper::PropertyName_DialogueVariations			= TEXT("Variations");
	const FString FDialogueHelper::PropertyName_IsMature					= TEXT("*IsMature");

	bool FDialogueHelper::ProcessDialogueWave( const UDialogueWave* DialogueWave )
	{
		if( !DialogueWave )
		{
			return false;
		}

		DialogueKey = DialogueWave->LocalizationGUID.ToString();
		SourceLocation = DialogueWave->GetPathName();
		SpokenSource = DialogueWave->SpokenText;
		VoiceActorDirection = DialogueWave->VoiceActorDirection;
		bIsMature = DialogueWave->bMature;

		// Stores the human readable info describing source and targets for each context of this DialogueWave
		TArray< TSharedPtr< FLocMetadataValue > > VariationsDisplayInfoList;

		for( const FDialogueContextMapping& ContextMapping : DialogueWave->ContextMappings )
		{
			const FDialogueContext& DialogueContext = ContextMapping.Context;
			const UDialogueVoice* SpeakerDialogueVoice = DialogueContext.Speaker;

			// Skip over entries with invalid speaker
			if( !SpeakerDialogueVoice )
			{
				continue;
			}

			// Collect speaker info
			FString SpeakerDisplayName = GetDialogueVoiceName( SpeakerDialogueVoice );
			FString SpeakerGender = GetGrammaticalGenderString( SpeakerDialogueVoice->Gender );
			FString SpeakerPlurality = GetGrammaticalNumberString( SpeakerDialogueVoice->Plurality );
			FString SpeakerGuid = SpeakerDialogueVoice->LocalizationGUID.ToString();

			EGrammaticalGender::Type AccumulatedTargetGender = (EGrammaticalGender::Type)-1;
			EGrammaticalNumber::Type AccumulatedTargetPlurality = (EGrammaticalNumber::Type)-1;

			TArray<FString> TargetGuidsList;
			TArray<FString> TargetDisplayNameList;

			// Collect info on all the targets
			bool HasTarget = false;
			for( const UDialogueVoice* TargetDialogueVoice : DialogueContext.Targets )
			{
				if( TargetDialogueVoice )
				{
					FString TargetDisplayName = GetDialogueVoiceName( TargetDialogueVoice );
					TargetDisplayNameList.AddUnique( TargetDisplayName );
					FString TargetGender = GetGrammaticalGenderString( TargetDialogueVoice->Gender );
					FString TargetPlurality = GetGrammaticalNumberString( TargetDialogueVoice->Plurality );
					FString TargetGuid = TargetDialogueVoice->LocalizationGUID.ToString();

					TargetGuidsList.AddUnique( TargetGuid );

					if( AccumulatedTargetGender == -1)
					{
						AccumulatedTargetGender = TargetDialogueVoice->Gender;
					}
					else if( AccumulatedTargetGender != TargetDialogueVoice->Gender )
					{
						AccumulatedTargetGender = EGrammaticalGender::Mixed;
					}

					if( AccumulatedTargetPlurality == -1 )
					{
						AccumulatedTargetPlurality = TargetDialogueVoice->Plurality;
					}
					else if( AccumulatedTargetPlurality == EGrammaticalNumber::Singular )
					{
						AccumulatedTargetPlurality = EGrammaticalNumber::Plural;
					}

					HasTarget = true;
				}
			}

			FString FinalTargetGender = HasTarget ? GetGrammaticalGenderString( AccumulatedTargetGender ) : TEXT("");
			FString FinalTargetPlurality = HasTarget ? GetGrammaticalNumberString( AccumulatedTargetPlurality ) : TEXT("");

			// Add the context specific variation
			{
				TSharedPtr< FLocMetadataObject > KeyMetaDataObject = MakeShareable( new FLocMetadataObject() );

				// Setup a loc metadata object with all the context specific keys.
				{
					KeyMetaDataObject->SetStringField( PropertyName_GrammaticalGender, SpeakerGender );
					KeyMetaDataObject->SetStringField( PropertyName_GrammaticalPlurality, SpeakerPlurality );
					KeyMetaDataObject->SetStringField( PropertyName_Speaker, SpeakerGuid );
					KeyMetaDataObject->SetStringField( PropertyName_TargetGrammaticalGender, FinalTargetGender );
					KeyMetaDataObject->SetStringField( PropertyName_TargetGrammaticalNumber, FinalTargetPlurality );

					TArray< TSharedPtr< FLocMetadataValue > > TargetGuidsMetadata;
					for( FString& TargetGuid : TargetGuidsList )
					{
						TargetGuidsMetadata.Add( MakeShareable( new FLocMetadataValueString( TargetGuid ) ) );
					}

					KeyMetaDataObject->SetArrayField( PropertyName_Targets, TargetGuidsMetadata );
				}

				// Create the human readable info that describes the source and target of this dialogue
				TSharedPtr< FLocMetadataValue > SourceTargetInfo = GenSourceTargetMetadata( SpeakerDisplayName, TargetDisplayNameList );

				TSharedPtr< FLocMetadataObject > InfoMetaDataObject = MakeShareable( new FLocMetadataObject() );

				// Setup a loc metadata object with all the context specific info.  This usually includes human readable descriptions of the dialogue
				{
					if( SourceTargetInfo.IsValid() )
					{
						InfoMetaDataObject->SetField( PropertyName_DialogueVariations, SourceTargetInfo );
					}

					if( !VoiceActorDirection.IsEmpty() )
					{
						InfoMetaDataObject->SetStringField( PropertyName_VoiceActorDirection, VoiceActorDirection );
					}
				}


				FTextSourceSiteContext& Context = *(new(ContextSpecificVariations) FTextSourceSiteContext);

				// Setup the context
				{
					Context.KeyName = DialogueWave->GetContextLocalizationKey( DialogueContext );
					Context.SiteDescription = SourceLocation;
					Context.IsOptional = false;
					Context.KeyMetaData = KeyMetaDataObject->Values.Num() > 0 ? *KeyMetaDataObject : FLocMetadataObject();
					Context.InfoMetaData = InfoMetaDataObject->Values.Num() > 0 ? *InfoMetaDataObject : FLocMetadataObject();
				}

				{
					// Add human readable info describing the source and targets of this dialogue to the non-optional manifest entry if it does not exist already
					bool bAddContextDisplayInfoToBase = true;
					for( TSharedPtr< FLocMetadataValue > VariationInfo : VariationsDisplayInfoList )
					{
						if( *SourceTargetInfo == *VariationInfo)
						{
							bAddContextDisplayInfoToBase = false;
							break;
						}
					}
					if( bAddContextDisplayInfoToBase )
					{
						VariationsDisplayInfoList.Add( SourceTargetInfo );
					}
				}
			}
		}

		return true;
	}

	FString FDialogueHelper::GetGrammaticalNumberString(TEnumAsByte<EGrammaticalNumber::Type> Plurality)
	{
		switch(Plurality)
		{
		case EGrammaticalNumber::Singular:
			return TEXT("Singular");
			break;
		case EGrammaticalNumber::Plural:
			return TEXT("Plural");
			break;
		default:
			return FString();
		}
	}

	FString FDialogueHelper::GetGrammaticalGenderString(TEnumAsByte<EGrammaticalGender::Type> Gender)
	{
		switch(Gender)
		{
		case EGrammaticalGender::Neuter:
			return TEXT("Neuter");
			break;
		case EGrammaticalGender::Masculine:
			return TEXT("Masculine");
			break;
		case EGrammaticalGender::Feminine:
			return TEXT("Feminine");
			break;
		case EGrammaticalGender::Mixed:
			return TEXT("Mixed");
			break;
		default:
			return TEXT("");
		}
	}

	FString FDialogueHelper::GetDialogueVoiceName( const UDialogueVoice* DialogueVoice )
	{
		FString Name = DialogueVoice->GetName();
		return Name;
	}

	FString FDialogueHelper::ArrayMetaDataToString( const TArray< TSharedPtr< FLocMetadataValue > >& MetadataArray )
	{
		FString FinalString;
		if( MetadataArray.Num() > 0 )
		{
			TArray< FString > MetadataStrings;
			for( TSharedPtr< FLocMetadataValue > DataEntry : MetadataArray )
			{
				if( DataEntry->Type == ELocMetadataType::String )
				{
					MetadataStrings.Add(DataEntry->AsString());
				}
			}
			MetadataStrings.Sort();
			for( const FString& StrEntry : MetadataStrings )
			{
				if(FinalString.Len())
				{
					FinalString += TEXT(",");
				}
				FinalString += StrEntry;
			}
		}
		return FinalString;
	}

	TSharedPtr< FLocMetadataValue > FDialogueHelper::GenSourceTargetMetadata( const FString& SpeakerName, const TArray< FString >& TargetNames, bool bCompact )
	{
		/*
		This function can support two different formats.

		The first format is compact and results in string entries that will later be combined into something like this
		"Variations": [
		"Jenny -> Audience",
		"Zak -> Audience"
		]

		The second format is verbose and results in object entries that will later be combined into something like this
		"VariationsTest": [
		{
		"Speaker": "Jenny",
		"Targets": [
		"Audience"
		]
		},
		{
		"Speaker": "Zak",
		"Targets": [
		"Audience"
		]
		}
		]
		*/

		TSharedPtr< FLocMetadataValue > Result;
		if( bCompact )
		{
			TArray<FString> SortedTargetNames = TargetNames;
			SortedTargetNames.Sort();
			FString TargetNamesString;
			for( const FString& StrEntry : SortedTargetNames )
			{
				if(TargetNamesString.Len())
				{
					TargetNamesString += TEXT(",");
				}
				TargetNamesString += StrEntry;
			}
			Result = MakeShareable( new FLocMetadataValueString( FString::Printf( TEXT("%s -> %s" ), *SpeakerName, *TargetNamesString ) ) );
		}
		else
		{
			TArray< TSharedPtr< FLocMetadataValue > > TargetNamesMetadataList;
			for( const FString& StrEntry: TargetNames )
			{
				TargetNamesMetadataList.Add( MakeShareable( new FLocMetadataValueString( StrEntry ) ) );
			}

			TSharedPtr< FLocMetadataObject > MetadataObj = MakeShareable( new FLocMetadataObject() );
			MetadataObj->SetStringField( PropertyName_Speaker, SpeakerName );
			MetadataObj->SetArrayField( PropertyName_Targets, TargetNamesMetadataList );

			Result = MakeShareable( new FLocMetadataValueObject( MetadataObj.ToSharedRef() ) );
		}
		return Result;
	}

	void GatherDialogueWaveForLocalization(const UObject* const Object, TArray<FGatherableTextData>& GatherableTextDataArray)
	{
		const UDialogueWave* const DialogueWave = CastChecked<UDialogueWave>(Object);

		FDialogueHelper DialogueHelper;
		if( DialogueHelper.ProcessDialogueWave( DialogueWave ) )
		{
			const FString& SpokenSource = DialogueHelper.GetSpokenSource();

			if( !SpokenSource.IsEmpty() )
			{
				FTextSourceData SourceData;
				{
					SourceData.SourceString = SpokenSource;
				}

				FGatherableTextData* GatherableTextData = GatherableTextDataArray.FindByPredicate([&](const FGatherableTextData& Candidate)
				{
					return	Candidate.NamespaceName == DialogueHelper.DialogueNamespace && 
						Candidate.SourceData.SourceString == SourceData.SourceString &&
						Candidate.SourceData.SourceStringMetaData == SourceData.SourceStringMetaData;
				});
				if(!GatherableTextData)
				{
					GatherableTextData = new(GatherableTextDataArray) FGatherableTextData;
					GatherableTextData->NamespaceName = DialogueHelper.DialogueNamespace;
					GatherableTextData->SourceData = SourceData;
				}

				{
					const FTextSourceSiteContext& Base = DialogueHelper.GetBaseContext();

					FTextSourceSiteContext& SourceSiteContext = *(new(GatherableTextData->SourceSiteContexts) FTextSourceSiteContext(Base));
				}

				{
					const TArray<FTextSourceSiteContext>& Variations = DialogueHelper.GetContextSpecificVariations();
					for( const FTextSourceSiteContext& Variation : Variations )
					{
						FTextSourceSiteContext& SourceSiteContext = *(new(GatherableTextData->SourceSiteContexts) FTextSourceSiteContext(Variation));
					}
				}
			}
		}
	}
}
#endif

bool operator==(const FDialogueContextMapping& LHS, const FDialogueContextMapping& RHS)
{
	return	LHS.Context == RHS.Context &&
		LHS.SoundWave == RHS.SoundWave;
}

bool operator!=(const FDialogueContextMapping& LHS, const FDialogueContextMapping& RHS)
{
	return	LHS.Context != RHS.Context ||
		LHS.SoundWave != RHS.SoundWave;
}

FDialogueContextMapping::FDialogueContextMapping() : SoundWave(NULL), Proxy(NULL)
{

}

UDialogueSoundWaveProxy::UDialogueSoundWaveProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UDialogueSoundWaveProxy::IsPlayable() const
{
	return SoundWave->IsPlayable();
}

const FAttenuationSettings* UDialogueSoundWaveProxy::GetAttenuationSettingsToApply() const
{
	return SoundWave->GetAttenuationSettingsToApply();
}

float UDialogueSoundWaveProxy::GetMaxAudibleDistance()
{
	return SoundWave->GetMaxAudibleDistance();
}

float UDialogueSoundWaveProxy::GetDuration()
{
	return SoundWave->GetDuration();
}

float UDialogueSoundWaveProxy::GetVolumeMultiplier()
{
	return SoundWave->GetVolumeMultiplier();
}

float UDialogueSoundWaveProxy::GetPitchMultiplier()
{
	return SoundWave->GetPitchMultiplier();
}

void UDialogueSoundWaveProxy::Parse( class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	int OldWaveInstanceCount = WaveInstances.Num();
	SoundWave->Parse(AudioDevice, NodeWaveInstanceHash, ActiveSound, ParseParams, WaveInstances);
	int NewWaveInstanceCount = WaveInstances.Num();

	FWaveInstance* WaveInstance = NULL;
	if(NewWaveInstanceCount == OldWaveInstanceCount + 1)
	{
		WaveInstance = WaveInstances[WaveInstances.Num() - 1];
	}

	// Add in the subtitle if they exist
	if (ActiveSound.bHandleSubtitles && Subtitles.Num() > 0)
	{
		// TODO - Audio Threading. This would need to be a call back to the main thread.
		UAudioComponent* AudioComponent = ActiveSound.GetAudioComponent();
		if (AudioComponent && AudioComponent->OnQueueSubtitles.IsBound())
		{
			// intercept the subtitles if the delegate is set
			AudioComponent->OnQueueSubtitles.ExecuteIfBound( Subtitles, GetDuration() );
		}
		else
		{
			// otherwise, pass them on to the subtitle manager for display
			// Subtitles are hashed based on the associated sound (wave instance).
			if( ActiveSound.World.IsValid() )
			{
				FSubtitleManager::GetSubtitleManager()->QueueSubtitles( ( PTRINT )WaveInstance, ActiveSound.SubtitlePriority, false, false, GetDuration(), Subtitles, 0.0f );
			}
		}
	}
}

USoundClass* UDialogueSoundWaveProxy::GetSoundClass() const
{
	return SoundWave->GetSoundClass();
}

UDialogueWave::UDialogueWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LocalizationGUID( FGuid::NewGuid() )
{
#if WITH_EDITORONLY_DATA
	struct FAutomaticRegistrationOfLocalizationGatherer
	{
		FAutomaticRegistrationOfLocalizationGatherer()
		{
			UPackage::GetTypeSpecificLocalizationDataGatheringCallbacks().Add(UDialogueWave::StaticClass(), &GatherDialogueWaveForLocalization);
		}
	} AutomaticRegistrationOfLocalizationGatherer;
#endif

	ContextMappings.Add( FDialogueContextMapping() );
}

// Begin UObject interface. 
void UDialogueWave::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar.ThisRequiresLocalizationGather();

	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
	{
		UE_LOG(LogAudio, Fatal, TEXT("This platform requires cooked packages, and audio data was not cooked into %s."), *GetFullName());
	}
}

bool UDialogueWave::IsReadyForFinishDestroy()
{
	return true;
}

FString UDialogueWave::GetDesc()
{
	return FString::Printf( TEXT( "Dialogue Wave Description" ) );
}

void UDialogueWave::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
}

void UDialogueWave::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	if( !bDuplicateForPIE )
	{
		LocalizationGUID = FGuid::NewGuid();
	}
}

void UDialogueWave::PostLoad()
{
	Super::PostLoad();

	for(FDialogueContextMapping& ContextMapping : ContextMappings)
	{
		UpdateMappingProxy(ContextMapping);
	}
}

#if WITH_EDITOR
void UDialogueWave::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	int32 NewArrayIndex = PropertyChangedEvent.GetArrayIndex(TEXT("ContextMappings"));

	if (ContextMappings.IsValidIndex(NewArrayIndex))
	{
		UpdateMappingProxy(ContextMappings[NewArrayIndex]);
	}
	else if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UDialogueWave, SpokenText))
	{
		for(FDialogueContextMapping& ContextMapping : ContextMappings)
		{
			UpdateMappingProxy(ContextMapping);
		}
	}
}
#endif

// End UObject interface. 

// Begin UDialogueWave interface.
bool UDialogueWave::SupportsContext(const FDialogueContext& Context) const
{
	for(int32 i = 0; i < ContextMappings.Num(); ++i)
	{
		const FDialogueContextMapping& ContextMapping = ContextMappings[i];
		if(ContextMapping.Context == Context)
		{
			return true;
		}
	}

	return false;
}

USoundBase* UDialogueWave::GetWaveFromContext(const FDialogueContext& Context) const
{
	if(Context.Speaker == NULL)
	{
		UE_LOG(LogAudio, Warning, TEXT("UDialogueWave::GetWaveFromContext requires a Context.Speaker (%s)."), *GetPathName());
		return NULL;
	}

	UDialogueSoundWaveProxy* Proxy = NULL;
	for(int32 i = 0; i < ContextMappings.Num(); ++i)
	{
		const FDialogueContextMapping& ContextMapping = ContextMappings[i];
		if(ContextMapping.Context == Context)
		{
			Proxy = ContextMapping.Proxy;
			break;
		}
	}

	return Proxy;
}

FString UDialogueWave::GetContextLocalizationKey( const FDialogueContext& Context ) const
{
	FString Key;
	if( SupportsContext(Context) )
	{
		Key = LocalizationGUID.ToString() + TEXT(" ") + Context.GetLocalizationKey();
	}
	return Key;
}


// End UDialogueWave interface.

void UDialogueWave::UpdateMappingProxy(FDialogueContextMapping& ContextMapping)
{
	if (ContextMapping.SoundWave)
	{
		if (!ContextMapping.Proxy)
		{
			ContextMapping.Proxy = NewObject<UDialogueSoundWaveProxy>();
		}
	}
	else
	{
		ContextMapping.Proxy = NULL;
	}

	if(ContextMapping.Proxy)
	{
		// Copy the properties that the proxy shares with the sound in case it's used as a SoundBase
		ContextMapping.Proxy->SoundWave = ContextMapping.SoundWave;
		UEngine::CopyPropertiesForUnrelatedObjects(ContextMapping.SoundWave, ContextMapping.Proxy);

		FSubtitleCue NewSubtitleCue;
		FString Key = GetContextLocalizationKey( ContextMapping.Context );

		if( !( FText::FindText( FString(), Key, NewSubtitleCue.Text )) )
		{
			Key = LocalizationGUID.ToString();

			if ( !FText::FindText( FString(), Key, /*OUT*/NewSubtitleCue.Text ) )
			{
				NewSubtitleCue.Text = FText::FromString( SpokenText );
			}
		}
		NewSubtitleCue.Time = 0.0f;
		ContextMapping.Proxy->Subtitles.Empty();
		ContextMapping.Proxy->Subtitles.Add(NewSubtitleCue);
	}
}
