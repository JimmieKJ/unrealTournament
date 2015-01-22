// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "PackageTools.h"
#include "ARFilter.h"
#include "AssetData.h"
#include "AssetRegistryModule.h"
#include "PackageHelperFunctions.h"
#include "InternationalizationMetadata.h"
#include "Sound/DialogueWave.h"
#include "Sound/DialogueVoice.h"

DEFINE_LOG_CATEGORY_STATIC(LogGatherTextFromAssetsCommandlet, Log, All);

#define LOC_DEFINE_REGION

//////////////////////////////////////////////////////////////////////////
//UGatherTextFromAssetsCommandlet

const FString UGatherTextFromAssetsCommandlet::UsageText
	(
	TEXT("GatherTextFromAssetsCommandlet usage...\r\n")
	TEXT("    <GameName> UGatherTextFromAssetsCommandlet -root=<parsed code root folder> -exclude=<paths to exclude>\r\n")
	TEXT("    \r\n")
	TEXT("    <paths to include> Paths to include. Delimited with ';'. Accepts wildcards. eg \"*Content/Developers/*;*/TestMaps/*\" OPTIONAL: If not present, everything will be included. \r\n")
	TEXT("    <paths to exclude> Paths to exclude. Delimited with ';'. Accepts wildcards. eg \"*Content/Developers/*;*/TestMaps/*\" OPTIONAL: If not present, nothing will be excluded.\r\n")
	);

const FString UGatherTextFromAssetsCommandlet::FDialogueHelper::DialogueNamespace						= TEXT("Dialogue");
const FString UGatherTextFromAssetsCommandlet::FDialogueHelper::PropertyName_VoiceActorDirection		= TEXT("Voice Actor Direction");
const FString UGatherTextFromAssetsCommandlet::FDialogueHelper::PropertyName_Speaker					= TEXT("Speaker");
const FString UGatherTextFromAssetsCommandlet::FDialogueHelper::PropertyName_Speakers					= TEXT("Speakers");
const FString UGatherTextFromAssetsCommandlet::FDialogueHelper::PropertyName_Targets					= TEXT("Targets");
const FString UGatherTextFromAssetsCommandlet::FDialogueHelper::PropertyName_GrammaticalGender			= TEXT("Gender");
const FString UGatherTextFromAssetsCommandlet::FDialogueHelper::PropertyName_GrammaticalPlurality		= TEXT("Plurality");
const FString UGatherTextFromAssetsCommandlet::FDialogueHelper::PropertyName_TargetGrammaticalGender	= TEXT("TargetGender");
const FString UGatherTextFromAssetsCommandlet::FDialogueHelper::PropertyName_TargetGrammaticalNumber	= TEXT("TargetPlurality");
const FString UGatherTextFromAssetsCommandlet::FDialogueHelper::PropertyName_Optional					= TEXT("Optional");
const FString UGatherTextFromAssetsCommandlet::FDialogueHelper::PropertyName_DialogueVariations			= TEXT("Variations");
const FString UGatherTextFromAssetsCommandlet::FDialogueHelper::PropertyName_IsMature					= TEXT("*IsMature");

bool UGatherTextFromAssetsCommandlet::FDialogueHelper::ProcessDialogueWave( const UDialogueWave* DialogueWave )
{
	if( !DialogueWave )
	{
		return false;
	}

	DialogueKey = DialogueWave->LocalizationGUID.ToString();
	SourceLocation = DialogueWave->GetPathName();
	SpokenSource = DialogueWave->SpokenText;
#if WITH_EDITORONLY_DATA
	VoiceActorDirection = DialogueWave->VoiceActorDirection;
#endif
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
			}
		}

		FString FinalTargetGender = GetGrammaticalGenderString( AccumulatedTargetGender );
		FString FinalTargetPlurality = GetGrammaticalNumberString( AccumulatedTargetPlurality );

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


			TSharedRef< FContext > Context = MakeShareable( new FContext );

			// Setup the context
			{
				Context->Key = DialogueWave->GetContextLocalizationKey( DialogueContext );
				Context->SourceLocation = SourceLocation;
				Context->bIsOptional = true;
				Context->KeyMetadataObj = KeyMetaDataObject->Values.Num() > 0 ? KeyMetaDataObject : NULL;
				Context->InfoMetadataObj = InfoMetaDataObject->Values.Num() > 0 ? InfoMetaDataObject : NULL;
			}

			// Add this context specific variation to
			ContextSpecificVariations.Add( Context );

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

	// Create the base, non-optional entry
	{
		TSharedPtr< FLocMetadataObject > InfoMetaDataObject = MakeShareable( new FLocMetadataObject() );

		// Setup a loc metadata object with all the context specific info.  This usually includes human readable descriptions of the dialogue
		{
			if( VariationsDisplayInfoList.Num() > 0 )
			{
				InfoMetaDataObject->SetArrayField( PropertyName_DialogueVariations, VariationsDisplayInfoList );
			}

			if( !VoiceActorDirection.IsEmpty() )
			{
				InfoMetaDataObject->SetStringField( PropertyName_VoiceActorDirection, VoiceActorDirection );
			}
		}

		TSharedRef< FContext > Context = MakeShareable( new FContext );

		// Setup the context
		{
			Context->Key = DialogueKey;
			Context->SourceLocation = SourceLocation;
			Context->bIsOptional = false;
			Context->InfoMetadataObj = InfoMetaDataObject->Values.Num() > 0 ? InfoMetaDataObject : NULL;
		}

		Base = Context;
	}

	return true;
}

FString UGatherTextFromAssetsCommandlet::FDialogueHelper::GetGrammaticalNumberString(TEnumAsByte<EGrammaticalNumber::Type> Plurality)
{
	const static UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EGrammaticalNumber"), true);
	check (Enum);
	check (Enum->NumEnums() > Plurality);

	const FString KeyName = TEXT("DisplayName");
	const FString PluralityString = Enum->GetMetaData(*KeyName, Plurality);

	return PluralityString;
}



FString UGatherTextFromAssetsCommandlet::FDialogueHelper::GetGrammaticalGenderString(TEnumAsByte<EGrammaticalGender::Type> Gender)
{
	const static UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EGrammaticalGender"), true);
	check (Enum);
	check (Enum->NumEnums() > Gender);

	const FString KeyName = TEXT("DisplayName");
	const FString GenderString = Enum->GetMetaData(*KeyName, Gender);

	return GenderString;
}


FString UGatherTextFromAssetsCommandlet::FDialogueHelper::GetDialogueVoiceName( const UDialogueVoice* DialogueVoice )
{
	FString Name = DialogueVoice->GetName();
	return Name;
}

FString UGatherTextFromAssetsCommandlet::FDialogueHelper::ArrayMetaDataToString( const TArray< TSharedPtr< FLocMetadataValue > >& MetadataArray )
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

TSharedPtr< FLocMetadataValue > UGatherTextFromAssetsCommandlet::FDialogueHelper::GenSourceTargetMetadata( const FString& SpeakerName, const TArray< FString >& TargetNames, bool bCompact )
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


UGatherTextFromAssetsCommandlet::UGatherTextFromAssetsCommandlet(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{

}

namespace
{
	template<typename TConcreteChildClass>
	class FGatherTextFromObject_Base
	{
	public:
		static inline void Execute(UGatherTextFromAssetsCommandlet* const Commandlet, UObject* const Object, const UPackage* const ObjectPackage, const bool ShouldFixBroken)
		{
			if( !Object->HasAnyFlags( RF_Transient | RF_PendingKill ) )
			{
				TConcreteChildClass(Commandlet, Object, ObjectPackage, ShouldFixBroken).Execute_Internal();
			}
		}

	protected:
		FGatherTextFromObject_Base(UGatherTextFromAssetsCommandlet* const InCommandlet, UObject* const InObject, const UPackage* const InObjectPackage, const bool InShouldFixBroken)
			: Commandlet(InCommandlet)
			, Object(InObject)
			, ObjectPackage(InObjectPackage)
			, ShouldFixBroken(InShouldFixBroken)
		{
		}

		void ProcessProperty(UProperty* const Property, void* const ValueAddress)
		{
			// Property is a struct property.
			UStructProperty* const StructProperty = Cast<UStructProperty>(Property);
			if(StructProperty)
			{
				// Struct property may be a native array of fixed size.
				for(int32 i = 0; i < Property->ArrayDim; ++i)
				{
					uint8* StructureValueAddress = reinterpret_cast<uint8*>(ValueAddress);
					if(Property->ArrayDim > 1)
					{
						StructureValueAddress += StructProperty->Struct->GetStructureSize() * i;
					}

					// Iterate over all properties of the struct's class.
					for (TFieldIterator<UProperty>PropIt(StructProperty->Struct, EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::IncludeInterfaces); PropIt; ++PropIt)
					{
						ProcessProperty( *PropIt, PropIt->ContainerPtrToValuePtr<void>(StructureValueAddress) );
					}
				}
			}

			// Property is an array property.
			UArrayProperty* const ArrayProperty = Cast<UArrayProperty>(Property);
			if(ArrayProperty)
			{
				for(int32 i = 0; i < Property->ArrayDim; ++i)
				{
					void* ArrayValueAddress;
					if(Property->ArrayDim > 1)
					{
						ArrayValueAddress = Property->ContainerPtrToValuePtr<void>(ValueAddress, i);
					}
					else
					{
						ArrayValueAddress = ValueAddress;
					}

					// Iterate over all objects of the array.
					FScriptArrayHelper ScriptArrayHelper(ArrayProperty, ArrayValueAddress);
					const int32 ElementCount = ScriptArrayHelper.Num();
					for(int32 j = 0; j < ElementCount; ++j)
					{
						ProcessProperty( ArrayProperty->Inner, ScriptArrayHelper.GetRawPtr(j) );
					}
				}
			}

			// Property is a text property.
			UTextProperty* const TextProperty = Cast<UTextProperty>(Property);
			if(TextProperty)
			{
				// Text property may be a native array of fixed size.
				for(int32 i = 0; i < Property->ArrayDim; ++i)
				{
					FText* Text;
					if(Property->ArrayDim > 1)
					{
						uint8* ArrayValueAddress = reinterpret_cast<uint8*>(ValueAddress);
						Text = reinterpret_cast<FText*>(ArrayValueAddress + Property->ElementSize * i);
					}
					else
					{
						Text = reinterpret_cast<FText*>(ValueAddress);
					}

					if( FTextInspector::GetFlags(*Text) & ETextFlag::ConvertedProperty )
					{
						ObjectPackage->MarkPackageDirty();
					}

					bool Fixed = false;
					Commandlet->ProcessTextProperty(TextProperty, Text, Object, /*OUT*/Fixed );
					if( Fixed )
					{
						ObjectPackage->MarkPackageDirty();
					}
				}
			}
		}

	protected:
		UGatherTextFromAssetsCommandlet* const Commandlet;
		UObject* const Object;
		const UPackage* const ObjectPackage;
		const bool ShouldFixBroken;
	};

	class FGatherTextFromObject : public FGatherTextFromObject_Base<FGatherTextFromObject>
	{
	public:
		FGatherTextFromObject(UGatherTextFromAssetsCommandlet* const InCommandlet, UObject* const InObject, const UPackage* const InObjectPackage, const bool InShouldFixBroken)
			: FGatherTextFromObject_Base(InCommandlet, InObject, InObjectPackage, InShouldFixBroken)
		{
		}

		void Execute_Internal()
		{
			// Iterate over all fields of the object's class.
			for (TFieldIterator<UProperty>PropIt(Object->GetClass(), EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::IncludeInterfaces); PropIt; ++PropIt)
			{
				ProcessProperty( *PropIt, PropIt->ContainerPtrToValuePtr<void>(Object) );
			}
		}
	};

	class FGatherTextFromDataTable : public FGatherTextFromObject_Base<FGatherTextFromDataTable>
	{
	public:
		FGatherTextFromDataTable(UGatherTextFromAssetsCommandlet* const InCommandlet, UObject* const InDataTable, const UPackage* const InObjectPackage, const bool InShouldFixBroken)
			: FGatherTextFromObject_Base(InCommandlet, InDataTable, InObjectPackage, InShouldFixBroken)
		{
		}

		void Execute_Internal()
		{
			UDataTable* const DataTable = Cast<UDataTable>(Object);

			// Iterate over all properties of the struct's class.
			for (TFieldIterator<UProperty>PropIt(DataTable->RowStruct, EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::IncludeInterfaces); PropIt; ++PropIt)
			{
				for (const auto& Pair : DataTable->RowMap)
				{
					ProcessProperty( *PropIt, PropIt->ContainerPtrToValuePtr<void>(Pair.Value) );
				}
			}
		}
	};
}

void UGatherTextFromAssetsCommandlet::ProcessPackages( const TArray< UPackage* >& PackagesToProcess )
{
	for( int32 i = 0; i < PackagesToProcess.Num(); ++i )
	{
		UPackage* Package = PackagesToProcess[i];
		TArray<UObject*> Objects;
		GetObjectsWithOuter(Package, Objects);

		for( int32 j = 0; j < Objects.Num(); ++j )
		{
			UObject* Object = Objects[j];
			if ( Object->IsA( UBlueprint::StaticClass() ) )
			{
				UBlueprint* Blueprint = Cast<UBlueprint>( Object );

				if( Blueprint->GeneratedClass != NULL )
				{
					FGatherTextFromObject::Execute(this, Blueprint->GeneratedClass->GetDefaultObject(), Package, bFixBroken);
				}
				else
				{
					UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("%s - Invalid generated class!"), *Blueprint->GetFullName());
				}
			}
			else if ( Object->IsA( UDataTable::StaticClass() ) )
			{
				UDataTable* DataTable = Cast<UDataTable>(Object);
				FGatherTextFromDataTable::Execute(this, DataTable, Package, bFixBroken);
			}
			else if( Object->IsA( UDialogueWave::StaticClass() ) )
			{
				UDialogueWave* DialogueWave = Cast<UDialogueWave>(Object);
				ProcessDialogueWave(DialogueWave);
			}

			FGatherTextFromObject::Execute(this, Object, Package, bFixBroken);
		}
	}
}

void UGatherTextFromAssetsCommandlet::ProcessDialogueWave( const UDialogueWave* DialogueWave )
{
	if( !DialogueWave || DialogueWave->HasAnyFlags( RF_Transient | RF_PendingKill ) )
	{
		return;
	}

	FString DialogueName = DialogueWave->GetName();
	// Use a helper class to extract the dialogue info and prepare it for the manifest
	FDialogueHelper DialogueHelper;
	if( DialogueHelper.ProcessDialogueWave( DialogueWave ) )
	{
		const FString& SpokenSource = DialogueHelper.GetSpokenSource();

		if( !SpokenSource.IsEmpty() )
		{
			{
				TSharedPtr< const FContext > Base = DialogueHelper.GetBaseContext();

				FString EntryDescription = FString::Printf( TEXT("In non-optional variation of %s"), *DialogueName);
				ManifestInfo->AddEntry(EntryDescription, DialogueHelper.DialogueNamespace, SpokenSource, *Base);
			}
			
			{
				const TArray< TSharedPtr< FContext > > Variations = DialogueHelper.GetContextSpecificVariations();
				for( TSharedPtr< FContext > Variation : Variations )
				{
					FString EntryDescription = FString::Printf( TEXT("In context specific variation of %s"), *DialogueName);
					ManifestInfo->AddEntry(EntryDescription, DialogueHelper.DialogueNamespace, SpokenSource, *Variation);
				}
			}
		}
	}
}

void UGatherTextFromAssetsCommandlet::ProcessTextProperty(UTextProperty* InTextProp, FText* Data, UObject* Object, bool& OutFixed)
{
	OutFixed = false;

	FConflictTracker::FEntry NewEntry;
	NewEntry.ObjectPath = Object->GetPathName();
	NewEntry.SourceString = Data->GetSourceString();
	NewEntry.Status = EAssetTextGatherStatus::None;

	TSharedPtr< FString, ESPMode::ThreadSafe > Namespace;
	TSharedPtr< FString, ESPMode::ThreadSafe > Key;
	FTextLocalizationManager::Get().FindKeyNamespaceFromDisplayString(Data->DisplayString, Namespace, Key);

	// Check if text is localizable and check for missing key. Unlocalizable texts don't need a key.
	if( ( !Key.IsValid() || Key->IsEmpty() ) && Data->ShouldGatherForLocalization() )
	{
		NewEntry.Status = EAssetTextGatherStatus::MissingKey;

		// Fix missing key if allowed.
		if (bFixBroken)
		{
			// Create key if needed.
			if( !( Key.IsValid() ) )
			{
				Key = MakeShareable( new FString() );
			}
			// Generate new GUID for key.
			*(Key) = FGuid::NewGuid().ToString();

			// Fixed.
			NewEntry.Status = EAssetTextGatherStatus::MissingKey_Resolved;
		}
	}

	// Must have valid key to test for identity conflicts. Even if a text doesn't require localization, if it conflicts, it should be warned about.
	if( Key.IsValid() && !( Key->IsEmpty() ) )
	{
		// Find existing entry from manifest or manifest dependencies.
		FContext SearchContext;
		SearchContext.Key = *Key;
		TSharedPtr< FManifestEntry > ExistingEntry = ManifestInfo->GetManifest()->FindEntryByContext( Namespace.IsValid() ? *Namespace : TEXT(""), SearchContext );

		if( !ExistingEntry.IsValid() )
		{
			FString FileInfo;
			ExistingEntry = ManifestInfo->FindDependencyEntrybyContext( Namespace.IsValid() ? *Namespace : TEXT(""), SearchContext, FileInfo );
		}

		// Entry already exists, check for conflict.
		if( ExistingEntry.IsValid() && ExistingEntry->Source.Text != ( NewEntry.SourceString.IsValid() ? **(NewEntry.SourceString) : TEXT("") ))
		{
			NewEntry.Status = EAssetTextGatherStatus::IdentityConflict;

			// Fix conflict if allowed.
			if (bFixBroken)
			{
				// Generate new GUID for key.
				*(Key) = FGuid::NewGuid().ToString();

				// Fixed.
				NewEntry.Status = EAssetTextGatherStatus::IdentityConflict_Resolved;

				// Conflict resolved, no existing entry.
				ExistingEntry.Reset();
			}
		}
	}

	// Gather if it requires gathering and isn't in a bad state.
	if( Data->ShouldGatherForLocalization() && (NewEntry.Status & EAssetTextGatherStatus::BadBitMask) == false)
	{
		FString SrcLocation = Object->GetPathName() + TEXT(".") + InTextProp->GetName();

		// Adjust the source location if needed.
		{
			UClass* Class = Object->GetClass();
			UObject* CDO = Class ? Class->GetDefaultObject() : NULL;

			if( CDO && CDO != Object )
			{
				for( TFieldIterator<UTextProperty> PropIt(CDO->GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt )
				{
					UTextProperty* TextProp	=  Cast<UTextProperty>( *(PropIt) );
					FText* DataCDO = TextProp->ContainerPtrToValuePtr<FText>( CDO );
					TSharedPtr< FString, ESPMode::ThreadSafe > NamespaceCDO;
					TSharedPtr< FString, ESPMode::ThreadSafe > KeyCDO;
					FTextLocalizationManager::Get().FindKeyNamespaceFromDisplayString(DataCDO->DisplayString, NamespaceCDO, KeyCDO);

					if( KeyCDO == Key || ( KeyCDO.Get() && Key.Get() && ( *KeyCDO == *Key ) ) )
					{
						SrcLocation = CDO->GetPathName() + TEXT(".") + TextProp->GetName();
						break;
					}
				}
			}
		}

		FContext Context;
		Context.Key = *Key;
		Context.SourceLocation = SrcLocation;

		FString EntryDescription = FString::Printf( TEXT("In %s"), *Object->GetFullName());
		ManifestInfo->AddEntry(EntryDescription, Namespace.Get() ? *Namespace : TEXT(""), NewEntry.SourceString.Get() ? *(NewEntry.SourceString) : TEXT(""), Context );
	}

	// Add to conflict tracker.
	FConflictTracker::FKeyTable& KeyTable = ConflictTracker.Namespaces.FindOrAdd( Namespace.IsValid() ? *Namespace : TEXT("") );
	FConflictTracker::FEntryArray& EntryArray = KeyTable.FindOrAdd( Key.IsValid() ? *Key : TEXT("") );
	EntryArray.Add(NewEntry);

	OutFixed = (NewEntry.Status == EAssetTextGatherStatus::MissingKey_Resolved || NewEntry.Status == EAssetTextGatherStatus::IdentityConflict_Resolved);
}

int32 UGatherTextFromAssetsCommandlet::Main(const FString& Params)
{
	TGuardValue<bool> DisableCompileOnLoad(GForceDisableBlueprintCompileOnLoad, true);

	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	//Set config file
	const FString* ParamVal = ParamVals.Find(FString(TEXT("Config")));
	FString GatherTextConfigPath;

	if ( ParamVal )
	{
		GatherTextConfigPath = *ParamVal;
	}
	else
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("No config specified."));
		return -1;
	}

	//Set config section
	ParamVal = ParamVals.Find(FString(TEXT("Section")));
	FString SectionName;

	if ( ParamVal )
	{
		SectionName = *ParamVal;
	}
	else
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("No config section specified."));
		return -1;
	}

	//Modules to Preload
	TArray<FString> ModulesToPreload;
	GetConfigArray(*SectionName, TEXT("ModulesToPreload"), ModulesToPreload, GatherTextConfigPath);

	for (const FString& ModuleName : ModulesToPreload)
	{
		FModuleManager::Get().LoadModule(*ModuleName);
	}

	//Include paths
	TArray<FString> IncludePaths;
	GetConfigArray(*SectionName, TEXT("IncludePaths"), IncludePaths, GatherTextConfigPath);

	if (IncludePaths.Num() == 0)
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("No include paths in section %s"), *SectionName);
		return -1;
	}

	for (FString& IncludePath : IncludePaths)
	{
		FPaths::NormalizeDirectoryName(IncludePath);
		if (FPaths::IsRelative(IncludePath))
		{
			if (!FPaths::GameDir().IsEmpty())
			{
				IncludePath = FPaths::Combine( *( FPaths::GameDir() ), *IncludePath );
			}
			else
			{
				IncludePath = FPaths::Combine( *( FPaths::EngineDir() ), *IncludePath );
			}
		}

		IncludePath = FPaths::ConvertRelativePathToFull(IncludePath);

		// All paths must ends with "/*"
		if ( !IncludePath.EndsWith(TEXT("/*")) )
		{
			// If it ends in a slash, add the star.
			if ( IncludePath.EndsWith(TEXT("/")) )
			{
				IncludePath.AppendChar(TEXT('*'));
			}
			// If it doesn't end in a slash or slash star, just add slash star.
			else
			{
				IncludePath.Append(TEXT("/*"));
			}
		}
	}

	//Exclude paths
	TArray<FString> ExcludePaths;
	GetConfigArray(*SectionName, TEXT("ExcludePaths"), ExcludePaths, GatherTextConfigPath);

	//package extensions
	TArray<FString> PackageExts;
	GetConfigArray(*SectionName, TEXT("PackageExtensions"), PackageExts, GatherTextConfigPath);

	if (PackageExts.Num() == 0)
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("No package extensions specified in section %s, using defaults"), *SectionName);

		PackageExts.Add(FString("*") + FPackageName::GetAssetPackageExtension());
		PackageExts.Add(FString("*") + FPackageName::GetMapPackageExtension());
	}

	//asset class exclude
	TArray<FString> ExcludeClasses;
	GetConfigArray(*SectionName, TEXT("ExcludeClasses"), ExcludeClasses, GatherTextConfigPath);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().SearchAllAssets( true );
	FARFilter Filter;

	for(int32 i = 0; i < ExcludeClasses.Num(); i++)
	{
		UClass* FilterClass = FindObject<UClass>(ANY_PACKAGE, *ExcludeClasses[i]);
		if(FilterClass)
		{
			Filter.ClassNames.Add( FilterClass->GetFName() );
		}
		else
		{
			UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Invalid exclude class %s"), *ExcludeClasses[i]);
		}
	}

	TArray<FAssetData> AssetData;
	AssetRegistryModule.Get().GetAssets(Filter, AssetData);

	FString UAssetPackageExtension = FPackageName::GetAssetPackageExtension();
	TSet< FString > LongPackageNamesToExclude;
	for (int Index = 0; Index < AssetData.Num(); Index++)
	{
		LongPackageNamesToExclude.Add( FPackageName::LongPackageNameToFilename( AssetData[Index].PackageName.ToString(), UAssetPackageExtension ) );
	}

	//Get whether we should fix broken properties that we find.
	GetConfigBool(*SectionName, TEXT("bFixBroken"), bFixBroken, GatherTextConfigPath);

	// Add any manifest dependencies if they were provided
	TArray<FString> ManifestDependenciesList;
	GetConfigArray(*SectionName, TEXT("ManifestDependencies"), ManifestDependenciesList, GatherTextConfigPath);
	
	for (FString& ManifestDependency : ManifestDependenciesList)
	{
		if (FPaths::IsRelative(ManifestDependency))
		{
			if (!FPaths::GameDir().IsEmpty())
			{
				ManifestDependency = FPaths::Combine( *( FPaths::GameDir() ), *ManifestDependency );
			}
			else
			{
				ManifestDependency = FPaths::Combine( *( FPaths::EngineDir() ), *ManifestDependency );
			}
		}
	}

	if( !ManifestInfo->AddManifestDependencies( ManifestDependenciesList ) )
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("The GatherTextFromAssets commandlet couldn't find all the specified manifest dependencies."));
		return -1;
	}

	//The main array of files to work from.
	TArray< FString > PackageFileNamesToLoad;
	TSet< FString > LongPackageNamesToProcess;

	TArray<FString> PackageFilesNotInIncludePath;
	TArray<FString> PackageFilesInExcludePath;
	TArray<FString> PackageFilesExcludedByClass;

	//Fill the list of packages to work from.
	uint8 PackageFilter = NORMALIZE_DefaultFlags;
	TArray<FString> Unused;	
	for ( int32 PackageFilenameWildcardIdx = 0; PackageFilenameWildcardIdx < PackageExts.Num(); PackageFilenameWildcardIdx++ )
	{
		const bool IsAssetPackage = PackageExts[PackageFilenameWildcardIdx] == ( FString( TEXT("*") )+ FPackageName::GetAssetPackageExtension() );

		TArray<FString> PackageFiles;
		if ( !NormalizePackageNames( Unused, PackageFiles, PackageExts[PackageFilenameWildcardIdx], PackageFilter) )
		{
			UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("No packages found with extension %i: '%s'"), PackageFilenameWildcardIdx, *PackageExts[PackageFilenameWildcardIdx]);
			continue;
		}
		else
		{
			UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Found %i packages with extension %i: '%s'"), PackageFiles.Num(), PackageFilenameWildcardIdx, *PackageExts[PackageFilenameWildcardIdx]);
		}

		//Run through all the files found and add any that pass the include, exclude and filter constraints to OrderedPackageFilesToLoad
		for (FString& PackageFile : PackageFiles)
		{
			PackageFile = FPaths::ConvertRelativePathToFull(PackageFile);

			bool bExclude = false;
			//Ensure it matches the include paths if there are some.
			for (FString& IncludePath : IncludePaths)
			{
				bExclude = true;
				if( PackageFile.MatchesWildcard(IncludePath) )
				{
					bExclude = false;
					break;
				}
			}

			if ( bExclude )
			{
				PackageFilesNotInIncludePath.Add(PackageFile);
			}

			//Ensure it does not match the exclude paths if there are some.
			for( int32 ExcludePathIdx=0; !bExclude && ExcludePathIdx<ExcludePaths.Num() ; ++ExcludePathIdx )
			{
				if( PackageFile.MatchesWildcard(ExcludePaths[ExcludePathIdx]) )
				{
					bExclude = true;
					PackageFilesInExcludePath.Add(PackageFile);
					break;
				}
			}

			//Check that this is not on the list of packages that we don't care about e.g. textures.
			if ( !bExclude && IsAssetPackage && LongPackageNamesToExclude.Contains( PackageFile ) )
			{
				bExclude = true;
				PackageFilesExcludedByClass.Add(PackageFile);
			}

			//If we haven't failed one of the above checks, add it to the array of packages to process.
			if(!bExclude)
			{
				TScopedPointer< FArchive > FileReader( IFileManager::Get().CreateFileReader( *PackageFile ) );
				if( FileReader )
				{
					// Read package file summary from the file
					FPackageFileSummary PackageSummary;
					(*FileReader) << PackageSummary;

					// Early out check if the package has been flagged as needing localization gathering
					if( PackageSummary.PackageFlags & PKG_RequiresLocalizationGather || PackageSummary.GetFileVersionUE4() < VER_UE4_PACKAGE_REQUIRES_LOCALIZATION_GATHER_FLAGGING )
					{
						PackageFileNamesToLoad.Add( PackageFile );
					}
				}
			}
		}
	}

	if ( PackageFileNamesToLoad.Num() == 0 )
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("No files found. Or none passed the include/exclude criteria."));
	}

	CollectGarbage( RF_Native );

	//Now go through the remaining packages in the main array and process them in batches.
	int32 PackagesPerBatchCount = 100;
	TArray< UPackage* > LoadedPackages;
	TArray< FString > LoadedPackageFileNames;
	TArray< FString > FailedPackageFileNames;
	TArray< UPackage* > PackagesToProcess;

	const int32 PackageCount = PackageFileNamesToLoad.Num();
	const int32 BatchCount = PackageCount / PackagesPerBatchCount + (PackageCount % PackagesPerBatchCount > 0 ? 1 : 0); // Add an extra batch for any remainder if necessary
	if(PackageCount > 0)
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Log, TEXT("Loading %i packages in %i batches of %i."), PackageCount, BatchCount, PackagesPerBatchCount);
	}

	//Load the packages in batches
	int32 PackageIndex = 0;
	for( int32 BatchIndex = 0; BatchIndex < BatchCount; ++BatchIndex )
	{
		int32 PackagesInThisBatch = 0;
		int32 FailuresInThisBatch = 0;
		for( ; PackageIndex < PackageCount && PackagesInThisBatch < PackagesPerBatchCount; ++PackageIndex )
		{
			FString PackageFileName = PackageFileNamesToLoad[PackageIndex];

			UPackage *Package = LoadPackage( NULL, *PackageFileName, LOAD_None );
			if( Package )
			{
				LoadedPackages.Add(Package);
				LoadedPackageFileNames.Add(PackageFileName);

				// Because packages may not have been resaved after this flagging was implemented, we may have added packages to load that weren't flagged - potential false positives.
				// The loading process should have reflagged said packages so that only true positives will have this flag.
				if( Package->RequiresLocalizationGather() )
				{
					PackagesToProcess.Add( Package );
				}
			}
			else
			{
				FailedPackageFileNames.Add( PackageFileName );
				++FailuresInThisBatch;
				continue;
			}

			++PackagesInThisBatch;
		}

		UE_LOG(LogGatherTextFromAssetsCommandlet, Log, TEXT("Loaded %i packages in batch %i of %i. %i failed."), PackagesInThisBatch, BatchIndex + 1, BatchCount, FailuresInThisBatch);

		ProcessPackages(PackagesToProcess);
		PackagesToProcess.Empty(PackagesPerBatchCount);

		if( bFixBroken )
		{
			for( int32 LoadedPackageIndex=0; LoadedPackageIndex < LoadedPackages.Num() ; ++LoadedPackageIndex )
			{
				UPackage *Package = LoadedPackages[LoadedPackageIndex];
				const FString PackageName = LoadedPackageFileNames[LoadedPackageIndex];

				//Todo - link with source control.
				if( Package )
				{
					if( Package->IsDirty() )
					{
						if( SavePackageHelper( Package, *PackageName ) )
						{
							UE_LOG(LogGatherTextFromAssetsCommandlet, Log, TEXT("Saved Package %s."),*PackageName);
						}
						else
						{
							//TODO - Work out how to integrate with source control. The code from the source gatherer doesn't work.
							UE_LOG(LogGatherTextFromAssetsCommandlet, Log, TEXT("Could not save package %s. Probably due to source control. "),*PackageName);
						}
					}
				}
				else
				{
					UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Failed to find one of the loaded packages."));
				}
			}
		}

		CollectGarbage( RF_Native );
		LoadedPackages.Empty(PackagesPerBatchCount);	
		LoadedPackageFileNames.Empty(PackagesPerBatchCount);
	}

	for(auto i = ConflictTracker.Namespaces.CreateConstIterator(); i; ++i)
	{
		const FString& NamespaceName = i.Key();
		const FConflictTracker::FKeyTable& KeyTable = i.Value();
		for(auto j = KeyTable.CreateConstIterator(); j; ++j)
		{
			const FString& KeyName = j.Key();
			const FConflictTracker::FEntryArray& EntryArray = j.Value();

			for(int k = 0; k < EntryArray.Num(); ++k)
			{
				const FConflictTracker::FEntry& Entry = EntryArray[k];
				switch(Entry.Status)
				{
				case EAssetTextGatherStatus::MissingKey:
					{
						UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Detected missing key on asset \"%s\"."), *Entry.ObjectPath);
					}
					break;
				case EAssetTextGatherStatus::MissingKey_Resolved:
					{
						UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Fixed missing key on asset \"%s\"."), *Entry.ObjectPath);
					}
					break;
				case EAssetTextGatherStatus::IdentityConflict:
					{
						UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Detected duplicate identity with differing source on asset \"%s\"."), *Entry.ObjectPath);
					}
					break;
				case EAssetTextGatherStatus::IdentityConflict_Resolved:
					{
						UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Fixed duplicate identity with differing source on asset \"%s\"."), *Entry.ObjectPath);
					}
					break;
				}
			}
		}
	}

	return 0;
}

#undef LOC_DEFINE_REGION

//////////////////////////////////////////////////////////////////////////
