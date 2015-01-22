// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"

#include "FindInBlueprintManager.h"
#include "FindInBlueprints.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistryModule.h"
#include "DerivedDataCacheInterface.h"
#include "DerivedDataPluginInterface.h"

#include "JsonUtilities.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "Engine/SimpleConstructionScript.h"

#define LOCTEXT_NAMESPACE "FindInBlueprintManager"

FFindInBlueprintSearchManager* FFindInBlueprintSearchManager::Instance = NULL;

const FText FFindInBlueprintSearchTags::FiB_Properties = LOCTEXT("Properties", "Properties");

const FText FFindInBlueprintSearchTags::FiB_Components = LOCTEXT("Components", "Components");
const FText FFindInBlueprintSearchTags::FiB_IsSCSComponent = LOCTEXT("IsSCSComponent", "IsSCSComponent");

const FText FFindInBlueprintSearchTags::FiB_Nodes = LOCTEXT("Nodes", "Nodes");

const FText FFindInBlueprintSearchTags::FiB_SchemaName = LOCTEXT("SchemaName", "SchemaName");

const FText FFindInBlueprintSearchTags::FiB_UberGraphs = LOCTEXT("Uber", "Uber");
const FText FFindInBlueprintSearchTags::FiB_Functions = LOCTEXT("Functions", "Functions");
const FText FFindInBlueprintSearchTags::FiB_Macros = LOCTEXT("Macros", "Macros");
const FText FFindInBlueprintSearchTags::FiB_SubGraphs = LOCTEXT("Sub", "Sub");

const FText FFindInBlueprintSearchTags::FiB_Name = LOCTEXT("Name", "Name");
const FText FFindInBlueprintSearchTags::FiB_ClassName = LOCTEXT("ClassName", "ClassName");
const FText FFindInBlueprintSearchTags::FiB_NodeGuid = LOCTEXT("NodeGuid", "NodeGuid");
const FText FFindInBlueprintSearchTags::FiB_Tooltip = LOCTEXT("Tooltip", "Tooltip");
const FText FFindInBlueprintSearchTags::FiB_DefaultValue = LOCTEXT("DefaultValue", "DefaultValue");
const FText FFindInBlueprintSearchTags::FiB_Description = LOCTEXT("Description", "Description");
const FText FFindInBlueprintSearchTags::FiB_Comment = LOCTEXT("Comment", "Comment");

const FText FFindInBlueprintSearchTags::FiB_Pins = LOCTEXT("Pins", "Pins");
const FText FFindInBlueprintSearchTags::FiB_PinCategory = LOCTEXT("PinCategory", "PinCategory");
const FText FFindInBlueprintSearchTags::FiB_PinSubCategory = LOCTEXT("SubCategory", "SubCategory");
const FText FFindInBlueprintSearchTags::FiB_ObjectClass = LOCTEXT("ObjectClass", "ObjectClass");
const FText FFindInBlueprintSearchTags::FiB_IsArray = LOCTEXT("IsArray", "IsArray");
const FText FFindInBlueprintSearchTags::FiB_IsReference = LOCTEXT("IsReference", "IsReference");
const FText FFindInBlueprintSearchTags::FiB_Glyph = LOCTEXT("Glyph", "Glyph");
const FText FFindInBlueprintSearchTags::FiB_GlyphColor = LOCTEXT("GlyphColor", "GlyphColor");

namespace BlueprintSearchMetaDataHelpers
{
	/** Json Writer used for serializing FText's in the correct format for Find-in-Blueprints */
	template < class PrintPolicy = TPrettyJsonPrintPolicy<TCHAR> >
	class TJsonFindInBlueprintStringWriter : public TJsonStringWriter<PrintPolicy>
	{
	public:
		static TSharedRef< TJsonFindInBlueprintStringWriter > Create( FString* const InStream )
		{
			return MakeShareable( new TJsonFindInBlueprintStringWriter( InStream ) );
		}

		using TJsonStringWriter<PrintPolicy>::WriteObjectStart;

		void WriteObjectStart( const FText& Identifier )
		{
			check( this->Stack.Top() == EJson::Object );
			WriteIdentifier( Identifier );

			PrintPolicy::WriteLineTerminator(this->Stream);
			PrintPolicy::WriteTabs(this->Stream, this->IndentLevel);
			PrintPolicy::WriteChar(this->Stream, TCHAR('{'));
			++(this->IndentLevel);
			this->Stack.Push( EJson::Object );
			this->PreviousTokenWritten = EJsonToken::CurlyOpen;
		}

		void WriteArrayStart( const FText& Identifier )
		{
			check( this->Stack.Top() == EJson::Object );
			WriteIdentifier( Identifier );

			PrintPolicy::WriteSpace( this->Stream );
			PrintPolicy::WriteChar(this->Stream, TCHAR('['));
			++(this->IndentLevel);
			this->Stack.Push( EJson::Array );
			this->PreviousTokenWritten = EJsonToken::SquareOpen;
		}

		void WriteValue( const FText& Identifier, const bool Value )
		{
			check( this->Stack.Top() == EJson::Object );
			WriteIdentifier( Identifier );

			PrintPolicy::WriteSpace(this->Stream);
			this->WriteBoolValue( Value );
			this->PreviousTokenWritten = Value ? EJsonToken::True : EJsonToken::False;
		}

		void WriteValue( const FText& Identifier, const double Value )
		{
			check( this->Stack.Top() == EJson::Object );
			WriteIdentifier( Identifier );

			PrintPolicy::WriteSpace(this->Stream);
			this->WriteNumberValue( Value );
			this->PreviousTokenWritten = EJsonToken::Number;
		}

		void WriteValue( const FText& Identifier, const int32 Value )
		{
			check( this->Stack.Top() == EJson::Object );
			WriteIdentifier( Identifier );

			PrintPolicy::WriteSpace(this->Stream);
			this->WriteIntegerValue( Value );
			this->PreviousTokenWritten = EJsonToken::Number;
		}

		void WriteValue( const FText& Identifier, const int64 Value )
		{
			check( this->Stack.Top() == EJson::Object );
			WriteIdentifier( Identifier );

			PrintPolicy::WriteSpace(this->Stream);
			this->WriteIntegerValue( Value );
			this->PreviousTokenWritten = EJsonToken::Number;
		}

		void WriteValue( const FText& Identifier, const FString& Value )
		{
			check( this->Stack.Top() == EJson::Object );
			WriteIdentifier( Identifier );

			PrintPolicy::WriteSpace(this->Stream);
			WriteStringValue( Value );
			this->PreviousTokenWritten = EJsonToken::String;
		}

		void WriteValue( const FText& Identifier, const FText& Value )
		{
			check( this->Stack.Top() == EJson::Object );
			WriteIdentifier( Identifier );

			PrintPolicy::WriteSpace(this->Stream);
			WriteTextValue( Value );
			this->PreviousTokenWritten = EJsonToken::String;
		}

		/** Converts the lookup table of ints (which are stored as identifiers and string values in the Json) and the FText's they represent to an FString. */
		FString GetSerializedLookupTable()
		{
			TArray<uint8> SerializedData;
			FMemoryWriter Ar(SerializedData, /*bIsPersistent=*/ true);

			Ar << LookupTable;
			Ar.Close();

			FString Result = BytesToString(SerializedData.GetData(), SerializedData.Num());

			SerializedData.Empty();

			// Attach, as the first value and in an FString, the length of the Lookup table, this is to help sort through the data during deserialization
			FMemoryWriter ArWithLength(SerializedData, /*bIsPersistent=*/ true);
			int32 Length = Result.Len();
			ArWithLength << Length;
			return BytesToString(SerializedData.GetData(), SerializedData.Num()) + Result;
		}

		struct FLookupTableItem
		{
			FText Text;

			FLookupTableItem(FText InText)
				: Text(InText)
			{

			}

			bool operator==(const FLookupTableItem& InObject) const
			{
				if(!Text.CompareTo(InObject.Text) &&
					!FTextInspector::GetSourceString(Text)->Compare(*FTextInspector::GetDisplayString(InObject.Text), ESearchCase::CaseSensitive) )
				{
					return true;
				}

				return false;
			}
		};

	protected:
		TJsonFindInBlueprintStringWriter( FString* const InOutString )
			: TJsonStringWriter<PrintPolicy>( InOutString, 0 )
		{
		}

		virtual void WriteStringValue( const FString& String ) override
		{
			// We just want to make sure all strings are converted into FText hex strings, used by the FiB system
			WriteTextValue(FText::FromString(String));
		}

		void WriteTextValue( const FText& Text )
		{
			// Check to see if the value has already been added.
			int32* TableLookupValuePtr = ReverseLookupTable.Find(FLookupTableItem(Text));
			if(TableLookupValuePtr)
			{
				TJsonStringWriter<PrintPolicy>::WriteStringValue( FString::FromInt(*TableLookupValuePtr) );
			}
			else
			{
				// Add the FText to the table and write to the Json the ID to look the item up using
				int32 TableLookupValue = LookupTable.Num();
				LookupTable.Add(TableLookupValue, Text);
				ReverseLookupTable.Add(FLookupTableItem(Text), TableLookupValue);
				TJsonStringWriter<PrintPolicy>::WriteStringValue( FString::FromInt(TableLookupValue) );
			}
		}

		FORCEINLINE void WriteIdentifier( const FText& Identifier )
		{
			this->WriteCommaIfNeeded();
			PrintPolicy::WriteLineTerminator(this->Stream);

			PrintPolicy::WriteTabs(this->Stream, this->IndentLevel);

			WriteTextValue( Identifier );
			PrintPolicy::WriteChar(this->Stream, TCHAR(':'));
		}
		
		// This gets serialized
		TMap< int32, FText > LookupTable;

		// This is just locally needed for the write, to lookup the integer value by using the string of the FText
		TMap< FLookupTableItem, int32 > ReverseLookupTable;
	};

	static uint32 GetTypeHash(const BlueprintSearchMetaDataHelpers::TJsonFindInBlueprintStringWriter<TCondensedJsonPrintPolicy<TCHAR>>::FLookupTableItem& InObject)
	{
		return FCrc::MemCrc32(&InObject.Text, sizeof(InObject.Text));
	}

	typedef TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> SearchMetaDataWriterParentClass;
	typedef TJsonFindInBlueprintStringWriter<TCondensedJsonPrintPolicy<TCHAR>> SearchMetaDataWriter;

	/** Json Writer used for serializing FText's in the correct format for Find-in-Blueprints */
	template <class CharType = TCHAR>
	class TJsonFindInBlueprintStringReader : public TJsonReader<CharType>
	{
	public:
		static TSharedRef< TJsonFindInBlueprintStringReader< TCHAR > > Create( FArchive* const Stream, TMap< int32, FText >& InLookupTable  )
		{
			return MakeShareable( new TJsonFindInBlueprintStringReader( Stream, InLookupTable ) );
		}

		TJsonFindInBlueprintStringReader( FArchive* InStream,  TMap< int32, FText >& InLookupTable )
			: TJsonReader<CharType>(InStream)
			, LookupTable(MoveTemp(InLookupTable))
		{

		}

		FORCEINLINE virtual const FString& GetIdentifier() const override
		{
			// The identifier from Json is a Hex value that must be looked up in the LookupTable to find the FText it represents
			if(const FText* LookupText = LookupTable.Find(FCString::Atoi(*this->Identifier)))
			{
				return LookupText->ToString();
			}
			return this->Identifier;
		}

		FORCEINLINE virtual  const FString& GetValueAsString() const override
		{ 
			check( this->CurrentToken == EJsonToken::String ); 
			// The string value from Json is a Hex value that must be looked up in the LookupTable to find the FText it represents
			if(const FText* LookupText = LookupTable.Find(FCString::Atoi(*this->StringValue)))
			{
				return LookupText->ToString();
			}
			return this->StringValue;
		}

		TMap< int32, FText > LookupTable;
	};

	typedef TJsonFindInBlueprintStringReader<TCHAR> SearchMetaDataReader;

	/** Json Writer used for serializing FText's in the correct format for Find-in-Blueprints */
	template <class CharType = TCHAR>
	class TJsonFindInBlueprintStringReaderDDC : public FJsonStringReader
	{
	public:
		static TSharedRef< TJsonFindInBlueprintStringReaderDDC< TCHAR > > Create( const FString& InJsonString  )
		{
			return MakeShareable( new TJsonFindInBlueprintStringReaderDDC(InJsonString) );
		}

		TJsonFindInBlueprintStringReaderDDC( const FString& InJsonString )
			: FJsonStringReader(InJsonString)
		{

		}

		FORCEINLINE virtual const FString& GetIdentifier() const override
		{
			if(this->Identifier.Len())
			{
				// Remove the const, non-ideal, but this code is to bridge between the old DDC method and the new Asset Registry method of story FiB data.
				FString& IdentifierNonConst = const_cast<FString&>(this->Identifier);
				IdentifierNonConst = FFindInBlueprintSearchManager::ConvertHexStringToFText(this->Identifier).ToString();
			}
			return this->Identifier;
		}

		FORCEINLINE virtual  const FString& GetValueAsString() const override
		{ 
			check( this->CurrentToken == EJsonToken::String );
			if(this->StringValue.Len())
			{
				// Remove the const, non-ideal, but this code is to bridge between the old DDC method and the new Asset Registry method of story FiB data.
				FString& StringValueNonConst = const_cast<FString&>(this->StringValue);
				StringValueNonConst = FFindInBlueprintSearchManager::ConvertHexStringToFText(this->StringValue).ToString();
			}
			return this->StringValue;
		}
	};

	typedef TJsonFindInBlueprintStringReaderDDC<TCHAR> SearchMetaDataReaderDDC;


	/**
	 * Checks if Json value is searchable, eliminating data that not considered useful to search for
	 *
	 * @param InJsonValue		The Json value object to examine for searchability
	 * @return					TRUE if the value should be searchable
	 */
	bool CheckIfJsonValueIsSearchable( TSharedPtr< FJsonValue > InJsonValue )
	{
		/** Check for interesting values
		 *  booleans are not interesting, there are a lot of them
		 *  strings are not interesting if they are empty
		 *  numbers are not interesting if they are 0
		 *  arrays are not interesting if they are empty or if they are filled with un-interesting types
		 *  objects may not have interesting values when dug into
		 */
		bool bValidPropetyValue = true;
		if(InJsonValue->Type == EJson::Boolean || InJsonValue->Type == EJson::None || InJsonValue->Type == EJson::Null)
		{
			bValidPropetyValue = false;
		}
		else if(InJsonValue->Type == EJson::String)
		{
			FString temp = InJsonValue->AsString();
			if(InJsonValue->AsString().IsEmpty())
			{
				bValidPropetyValue = false;
			}
		}
		else if(InJsonValue->Type == EJson::Number)
		{
			if(InJsonValue->AsNumber() == 0.0)
			{
				bValidPropetyValue = false;
			}
		}
		else if(InJsonValue->Type == EJson::Array)
		{
			auto JsonArray = InJsonValue->AsArray();
			if(JsonArray.Num() > 0)
			{
				// Some types are never interesting and the contents of the array should be ignored. Other types can be interesting, the contents of the array should be stored (even if
				// the values may not be interesting, so that index values can be obtained)
				if(JsonArray[0]->Type != EJson::Array && JsonArray[0]->Type != EJson::String && JsonArray[0]->Type != EJson::Number && JsonArray[0]->Type != EJson::Object)
				{
					bValidPropetyValue = false;
				}
			}
		}
		else if(InJsonValue->Type == EJson::Object)
		{
			// Start it out as not being valid, if we find any sub-items that are searchable, it will be marked to TRUE
			bValidPropetyValue = false;

			// Go through all value/key pairs to see if any of them are searchable, remove the ones that are not
			auto JsonObject = InJsonValue->AsObject();
			for(auto Iter = JsonObject->Values.CreateIterator(); Iter; ++Iter)
			{
				if(!CheckIfJsonValueIsSearchable(Iter->Value))
				{
					Iter.RemoveCurrent();
				}
				else
				{
					bValidPropetyValue = true;
				}
			}
			
		}

		return bValidPropetyValue;
	}

	/**
	 * Saves a graph pin type to a Json object
	 *
	 * @param InWriter				Writer used for saving the Json
	 * @param InPinType				The pin type to save
	 */
	void SavePinTypeToJson(TSharedRef< SearchMetaDataWriter>& InWriter, const FEdGraphPinType& InPinType)
	{
		// Only save strings that are not empty

		if(!InPinType.PinCategory.IsEmpty())
		{
			InWriter->WriteValue(FFindInBlueprintSearchTags::FiB_PinCategory, InPinType.PinCategory);
		}

		if(!InPinType.PinSubCategory.IsEmpty())
		{
			InWriter->WriteValue(FFindInBlueprintSearchTags::FiB_PinSubCategory, InPinType.PinSubCategory);
		}

		if(InPinType.PinSubCategoryObject.IsValid())
		{
			InWriter->WriteValue(FFindInBlueprintSearchTags::FiB_ObjectClass, FText::FromString(InPinType.PinSubCategoryObject->GetName()));
		}
		InWriter->WriteValue(FFindInBlueprintSearchTags::FiB_IsArray, InPinType.bIsArray);
		InWriter->WriteValue(FFindInBlueprintSearchTags::FiB_IsReference, InPinType.bIsReference);
	}

	/**
	 * Helper function to save a variable description to Json
	 *
	 * @param InWriter					Json writer object
	 * @param InBlueprint				Blueprint the property for the variable can be found in, if any
	 * @param InVariableDescription		The variable description being serialized to Json
	 */
	void SaveVariableDescriptionToJson(TSharedRef< SearchMetaDataWriter>& InWriter, const UBlueprint* InBlueprint, const FBPVariableDescription& InVariableDescription)
	{
		FEdGraphPinType VariableType = InVariableDescription.VarType;

		InWriter->WriteObjectStart();

		InWriter->WriteValue(FFindInBlueprintSearchTags::FiB_Name, InVariableDescription.FriendlyName);

		// Find the variable's tooltip
		FString TooltipResult;
		
		if(InVariableDescription.HasMetaData(FBlueprintMetadata::MD_Tooltip))
		{
			TooltipResult = InVariableDescription.GetMetaData(FBlueprintMetadata::MD_Tooltip);
		}
		InWriter->WriteValue(FFindInBlueprintSearchTags::FiB_Tooltip, TooltipResult);

		// Save the variable's pin type
		SavePinTypeToJson(InWriter, VariableType);

		// Find the UProperty and convert it into a Json value.
		UProperty* VariableProperty = FindField<UProperty>(InBlueprint->GeneratedClass, InVariableDescription.VarName);
		if(VariableProperty)
		{
			const uint8* PropData = VariableProperty->ContainerPtrToValuePtr<uint8>(InBlueprint->GeneratedClass->GetDefaultObject());
			auto JsonValue = FJsonObjectConverter::UPropertyToJsonValue(VariableProperty, PropData, 0, 0);

			// Only use the value if it is searchable
			if(BlueprintSearchMetaDataHelpers::CheckIfJsonValueIsSearchable(JsonValue))
			{
				TSharedRef< FJsonValue > JsonValueAsSharedRef = JsonValue.ToSharedRef();
				FJsonSerializer::Serialize(JsonValue, FFindInBlueprintSearchTags::FiB_DefaultValue.ToString(), StaticCastSharedRef<SearchMetaDataWriterParentClass>(InWriter) );
			}
		}

		InWriter->WriteObjectEnd();
	}

	/**
	 * Gathers all nodes from a specified graph and serializes their searchable data to Json
	 *
	 * @param InWriter		The Json writer to use for serialization
	 * @param InGraph		The graph to search through
	 */
	void GatherNodesFromGraph(TSharedRef< SearchMetaDataWriter>& InWriter, const UEdGraph* InGraph)
	{
		// Collect all macro graphs
		InWriter->WriteArrayStart(FFindInBlueprintSearchTags::FiB_Nodes);
		{
			for(auto* Node : InGraph->Nodes)
			{
				if(Node)
				{
					// Make sure we don't collect search data for nodes that are going away soon
					if( Node->GetOuter()->IsPendingKill() )
					{
						continue;
					}

					InWriter->WriteObjectStart();

					// Retrieve the search metadata from the node, some node types may have extra metadata to be searchable.
					TArray<struct FSearchTagDataPair> Tags;
					Node->AddSearchMetaDataInfo(Tags);

					// Go through the node metadata tags and put them into the Json object.
					for(const FSearchTagDataPair& SearchData : Tags)
					{
						InWriter->WriteValue(SearchData.Key, SearchData.Value);
					}

					// Find all the pins and extract their metadata
					InWriter->WriteArrayStart(FFindInBlueprintSearchTags::FiB_Pins);
					for(UEdGraphPin* Pin : Node->Pins)
					{
						// Hidden pins are not searchable
						if(Pin->bHidden == false)
						{
							InWriter->WriteObjectStart();
							{
								InWriter->WriteValue(FFindInBlueprintSearchTags::FiB_Name, FText::FromString(Pin->GetSchema()->GetPinDisplayName(Pin)));
								InWriter->WriteValue(FFindInBlueprintSearchTags::FiB_DefaultValue, FText::FromString(Pin->GetDefaultAsString()));
							}
							SavePinTypeToJson(InWriter, Pin->PinType);
							InWriter->WriteObjectEnd();
						}
					}
					InWriter->WriteArrayEnd();

					InWriter->WriteObjectEnd();
				}
				
			}
		}
		InWriter->WriteArrayEnd();
	}

	/** 
	 * Gathers all graph's search data (and subojects) and serializes them to Json
	 *
	 * @param InWriter			The Json writer to use for serialization
	 * @param InGraphArray		All the graphs to process
	 * @param InTitle			The array title to place these graphs into
	 * @param InOutSubGraphs	All the subgraphs that need to be processed later
	 */
	void GatherGraphSearchData(TSharedRef< SearchMetaDataWriter>& InWriter, const UBlueprint* InBlueprint, const TArray< UEdGraph* >& InGraphArray, FText InTitle, TArray< UEdGraph* >* InOutSubGraphs)
	{
		if(InGraphArray.Num() > 0)
		{
			// Collect all graphs
			InWriter->WriteArrayStart(InTitle);
			{
				for(const UEdGraph* Graph : InGraphArray)
				{
					InWriter->WriteObjectStart();

					FGraphDisplayInfo DisplayInfo;
					Graph->GetSchema()->GetGraphDisplayInformation(*Graph, DisplayInfo);
					InWriter->WriteValue(FFindInBlueprintSearchTags::FiB_Name, DisplayInfo.PlainName);

					FString GraphDescription = FBlueprintEditorUtils::GetGraphDescription(Graph).ToString();
					if(!GraphDescription.IsEmpty())
					{
						InWriter->WriteValue(FFindInBlueprintSearchTags::FiB_Description, FText::FromString(GraphDescription));
					}
					// All nodes will appear as children to the graph in search results
					GatherNodesFromGraph(InWriter, Graph);

					// Collect local variables
					TArray<UK2Node_FunctionEntry*> FunctionEntryNodes;
					Graph->GetNodesOfClass<UK2Node_FunctionEntry>(FunctionEntryNodes);

					InWriter->WriteArrayStart(FFindInBlueprintSearchTags::FiB_Properties);
					{
						// Search in all FunctionEntry nodes for their local variables and add them to the list
						FString ActionCategory;
						for (UK2Node_FunctionEntry* const FunctionEntry : FunctionEntryNodes)
						{
							for( const FBPVariableDescription& Variable : FunctionEntry->LocalVariables )
							{
								SaveVariableDescriptionToJson(InWriter, InBlueprint, Variable);
							}
						}
					}
					InWriter->WriteArrayEnd(); // Properties

					InWriter->WriteObjectEnd();

					// Only if asked to do it
					if(InOutSubGraphs)
					{
						Graph->GetAllChildrenGraphs(*InOutSubGraphs);
					}
				}
			}
			InWriter->WriteArrayEnd();
		}
	}
}

FFindInBlueprintSearchManager& FFindInBlueprintSearchManager::Get()
{
	if (Instance == NULL)
	{
		Instance = new FFindInBlueprintSearchManager();
		Instance->Initialize();
	}

	return *Instance;
}

FFindInBlueprintSearchManager::FFindInBlueprintSearchManager()
	: bIsPausing(false)
{
}

FFindInBlueprintSearchManager::~FFindInBlueprintSearchManager()
{
	AssetRegistryModule->Get().OnAssetAdded().RemoveAll(this);
	AssetRegistryModule->Get().OnAssetRemoved().RemoveAll(this);
	AssetRegistryModule->Get().OnAssetRenamed().RemoveAll(this);
	FCoreUObjectDelegates::PreGarbageCollect.RemoveAll(this);
	FCoreUObjectDelegates::PostGarbageCollect.RemoveAll(this);
	FCoreUObjectDelegates::OnAssetLoaded.RemoveAll(this);
}

void FFindInBlueprintSearchManager::Initialize()
{
	AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule->Get().OnAssetAdded().AddRaw(this, &FFindInBlueprintSearchManager::OnAssetAdded);
	AssetRegistryModule->Get().OnAssetRemoved().AddRaw(this, &FFindInBlueprintSearchManager::OnAssetRemoved);
	AssetRegistryModule->Get().OnAssetRenamed().AddRaw(this, &FFindInBlueprintSearchManager::OnAssetRenamed);
	FCoreUObjectDelegates::PreGarbageCollect.AddRaw(this, &FFindInBlueprintSearchManager::PauseFindInBlueprintSearch);
	FCoreUObjectDelegates::PostGarbageCollect.AddRaw(this, &FFindInBlueprintSearchManager::UnpauseFindInBlueprintSearch);
	FCoreUObjectDelegates::OnAssetLoaded.AddRaw(this, &FFindInBlueprintSearchManager::OnAssetLoaded);

	if(!GIsSavingPackage)
	{
		// Do an immediate load of the cache to catch any Blueprints that were discovered by the asset registry before we initialized.
		BuildCache();
	}
}

void FFindInBlueprintSearchManager::OnAssetAdded(const FAssetData& InAssetData)
{
	if(InAssetData.GetClass()->IsChildOf(UBlueprint::StaticClass()) || InAssetData.GetClass()->IsChildOf(UWorld::StaticClass()))
	{
		FString BlueprintPackagePath = FPaths::GetPath(InAssetData.ObjectPath.ToString()) / FPaths::GetBaseFilename(InAssetData.ObjectPath.ToString());

		// Confirm that the Blueprint has not been added already, this can occur during duplication of Blueprints.
		int32* IndexPtr = SearchMap.Find(BlueprintPackagePath);
		if(!IndexPtr)
		{
			FSearchData NewSearchData;

			NewSearchData.BlueprintPath = InAssetData.ObjectPath.ToString();
			NewSearchData.bMarkedForDeletion = false;

			if(InAssetData.IsAssetLoaded())
			{
				UBlueprint* BlueprintAsset = nullptr;

				if(InAssetData.GetClass()->IsChildOf(UBlueprint::StaticClass()))
				{
					BlueprintAsset = Cast<UBlueprint>(InAssetData.GetAsset());
				}
				else if(InAssetData.GetClass()->IsChildOf(UWorld::StaticClass()))
				{
					UWorld* WorldAsset = Cast<UWorld>(InAssetData.GetAsset());
					if(WorldAsset->PersistentLevel)
					{
						BlueprintAsset = Cast<UBlueprint>((UObject*)WorldAsset->PersistentLevel->GetLevelScriptBlueprint(true));
					}
				}

				// Levels do not always have Blueprints
				if(BlueprintAsset)
				{
					// Cache the searchable data
					AddOrUpdateBlueprintSearchMetadata(BlueprintAsset);

					NewSearchData.Blueprint = Cast<UBlueprint>(BlueprintAsset);
				}

				// No more work, we have added the Blueprint if there is one, and if it's a level without one, it need not be added.
				return;
			}
			else if(const FString* FiBSearchData = InAssetData.TagsAndValues.Find("FiB"))
			{
				// Since the asset was not loaded, pull out the searchable data stored in the asset
				NewSearchData.Value = *FiBSearchData;
			}
			else
			{
				// We will look for DDC information about this asset
				FString SearchGuid;
				const FString* SearchGuidPtr = InAssetData.TagsAndValues.Find("SearchGuid");
				if(SearchGuidPtr)
				{
					SearchGuid = *SearchGuidPtr;

					FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();
					if(InAssetData.IsAssetLoaded())
					{
						NewSearchData.Blueprint = Cast<UBlueprint>(InAssetData.GetAsset());
					}
					NewSearchData.BlueprintPath = InAssetData.ObjectPath.ToString();
					NewSearchData.DDCRetrievalID = DDC.GetAsynchronous(*SearchGuid);
					NewSearchData.bMarkedForDeletion = false;
				}
			}

			int32 ArrayIndex = SearchArray.Add(NewSearchData);

			// Add the asset file path to the map along with the index into the array
			SearchMap.Add(BlueprintPackagePath, ArrayIndex);
		}
	}
}

void FFindInBlueprintSearchManager::OnAssetRemoved(const class FAssetData& InAssetData)
{
	if(InAssetData.IsAssetLoaded())
	{
		int32* SearchIdx = SearchMap.Find(InAssetData.GetAsset()->GetOutermost()->GetPathName());
		if(SearchIdx)
		{
			SearchArray[*SearchIdx].bMarkedForDeletion = true;
		}
	}
}

void FFindInBlueprintSearchManager::OnAssetRenamed(const class FAssetData& InAssetData, const FString& InOldName)
{
	// Renaming removes the item from the manager, it will be re-added in the OnAssetAdded event under the new name.
	if(InAssetData.IsAssetLoaded())
	{
		FString Test = FPaths::GetPath(InOldName) / FPaths::GetBaseFilename(InOldName);
		int32* SearchIdx = SearchMap.Find(FPaths::GetPath(InOldName) / FPaths::GetBaseFilename(InOldName));
		if(SearchIdx)
		{
			SearchArray[*SearchIdx].bMarkedForDeletion = true;
		}
	}
}

void FFindInBlueprintSearchManager::OnAssetLoaded(UObject* InAsset)
{
	UBlueprint* BlueprintAsset = nullptr;
	FString BlueprintPackagePath;

	if(UWorld* WorldAsset = Cast<UWorld>(InAsset))
	{
		if(WorldAsset->PersistentLevel)
		{
			BlueprintAsset = Cast<UBlueprint>((UObject*)WorldAsset->PersistentLevel->GetLevelScriptBlueprint(true));
			if(BlueprintAsset)
			{
				BlueprintPackagePath = BlueprintAsset->GetOutermost()->GetPathName();
			}
		}
	}
	else
	{
		BlueprintAsset = Cast<UBlueprint>(InAsset);
		BlueprintPackagePath = BlueprintAsset->GetPathName();
	}
	if(BlueprintAsset)
	{
		// Find and update the item in the search array. Searches may currently be active, this will do no harm to them

		// Confirm that the Blueprint has not been added already, this can occur during duplication of Blueprints.
		int32* IndexPtr = SearchMap.Find(BlueprintPackagePath);

		// The asset registry might not have informed us of this asset yet.
		if(IndexPtr)
		{
			// That index should never have a Blueprint already, but if it does, it should be the same Blueprint!
			ensure(!SearchArray[*IndexPtr].Blueprint.IsValid() || SearchArray[*IndexPtr].Blueprint == BlueprintAsset);
			//check(!SearchArray[*IndexPtr].Blueprint);
			SearchArray[*IndexPtr].Blueprint = BlueprintAsset;
		}
	}
}

FString FFindInBlueprintSearchManager::GatherBlueprintSearchMetadata(const UBlueprint* Blueprint)
{
	FString SearchMetaData;

	// The search registry tags for a Blueprint are all in Json
	TSharedRef< BlueprintSearchMetaDataHelpers::TJsonFindInBlueprintStringWriter<TCondensedJsonPrintPolicy<TCHAR>> > Writer = BlueprintSearchMetaDataHelpers::TJsonFindInBlueprintStringWriter<TCondensedJsonPrintPolicy<TCHAR>>::Create( &SearchMetaData );

	TMap<FString, TMap<FString,int>> AllPaths;
	Writer->WriteObjectStart();

	// Only pull properties if the Blueprint has been compiled
	if(Blueprint->SkeletonGeneratedClass)
	{
		Writer->WriteArrayStart(FFindInBlueprintSearchTags::FiB_Properties);
		{
			for( const FBPVariableDescription& Variable : Blueprint->NewVariables )
			{
				BlueprintSearchMetaDataHelpers::SaveVariableDescriptionToJson(Writer, Blueprint, Variable);
			}
		}
		Writer->WriteArrayEnd(); // Properties
	}

	// Gather all graph searchable data
	TArray< UEdGraph* > SubGraphs;
	BlueprintSearchMetaDataHelpers::GatherGraphSearchData(Writer, Blueprint, Blueprint->UbergraphPages, FFindInBlueprintSearchTags::FiB_UberGraphs, &SubGraphs);
	BlueprintSearchMetaDataHelpers::GatherGraphSearchData(Writer, Blueprint, Blueprint->FunctionGraphs, FFindInBlueprintSearchTags::FiB_Functions, &SubGraphs);
	BlueprintSearchMetaDataHelpers::GatherGraphSearchData(Writer, Blueprint, Blueprint->MacroGraphs, FFindInBlueprintSearchTags::FiB_Macros, &SubGraphs);
	// Sub graphs are processed separately so that they do not become children in the TreeView, cluttering things up if the tree is deep
	BlueprintSearchMetaDataHelpers::GatherGraphSearchData(Writer, Blueprint, SubGraphs, FFindInBlueprintSearchTags::FiB_SubGraphs, NULL);

	// Gather all SCS components
	// If we have an SCS but don't support it, then we remove it
	if(Blueprint->SimpleConstructionScript)
	{
		// Remove any SCS variable nodes
		TArray<USCS_Node*> AllSCSNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
		Writer->WriteArrayStart(FFindInBlueprintSearchTags::FiB_Components);
		for (TFieldIterator<UProperty> PropertyIt(Blueprint->SkeletonGeneratedClass, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* Property = *PropertyIt;
			UObjectPropertyBase* Obj = Cast<UObjectPropertyBase>(Property);
			const bool bComponentProperty = Obj && Obj->PropertyClass ? Obj->PropertyClass->IsChildOf<UActorComponent>() : false;
			FName PropName = Property->GetFName();
			if(bComponentProperty && FBlueprintEditorUtils::FindSCS_Node(Blueprint, PropName) != INDEX_NONE)
			{
				FEdGraphPinType PropertyPinType;
				if(UEdGraphSchema_K2::StaticClass()->GetDefaultObject<UEdGraphSchema_K2>()->ConvertPropertyToPinType(Property, PropertyPinType))
				{
					Writer->WriteObjectStart();
					{
						Writer->WriteValue(FFindInBlueprintSearchTags::FiB_Name, FText::FromName(PropName));
						Writer->WriteValue(FFindInBlueprintSearchTags::FiB_IsSCSComponent, true);
						SavePinTypeToJson(Writer,  PropertyPinType);
					}
					Writer->WriteObjectEnd();
				}
			}
		}
		Writer->WriteArrayEnd(); // Components
	}

	Writer->WriteObjectEnd();
	Writer->Close();

	SearchMetaData = Writer->GetSerializedLookupTable() + SearchMetaData;
	return SearchMetaData;
}

void FFindInBlueprintSearchManager::AddOrUpdateBlueprintSearchMetadata(UBlueprint* InBlueprint, bool bInForceReCache/* = false*/)
{
	double StartSeconds = FPlatformTime::Seconds();

	check(InBlueprint);

	// Allow only one thread modify the search data at a time
	FScopeLock ScopeLock(&SafeModifyCacheCriticalSection);

	FString BlueprintPackagePath = InBlueprint->GetOutermost()->GetPathName();

	FString BlueprintPath;
	if(FBlueprintEditorUtils::IsLevelScriptBlueprint(InBlueprint))
	{
		if(UWorld* World = InBlueprint->GetTypedOuter<UWorld>())
		{
			BlueprintPath = World->GetPathName();
		}
	}
	else
	{
		BlueprintPath = InBlueprint->GetPathName();
	}

	int32* IndexPtr = SearchMap.Find(BlueprintPackagePath);
	int32 Index = 0;
	if(!IndexPtr)
	{
		FSearchData SearchData;
		SearchData.Blueprint = InBlueprint;
		SearchData.BlueprintPath = BlueprintPath;
		Index = SearchArray.Add(SearchData);

		SearchMap.Add(BlueprintPackagePath, Index);
	}
	else
	{
		Index = *IndexPtr;
	}

	// Build the search data
	SearchArray[Index].BlueprintPath = BlueprintPath;
	SearchArray[Index].Value = GatherBlueprintSearchMetadata(InBlueprint);
	SearchArray[Index].bMarkedForDeletion = false;
}

void FFindInBlueprintSearchManager::BeginSearchQuery(const FStreamSearch* InSearchOriginator)
{
	// Cannot begin a search thread while saving
	FScopeLock ScopeLock(&PauseThreadsCriticalSection);
	FScopeLock ScopeLock2(&SafeQueryModifyCriticalSection);

	ActiveSearchCounter.Increment();
	ActiveSearchQueries.FindOrAdd(InSearchOriginator) = 0;
}

bool FFindInBlueprintSearchManager::ContinueSearchQuery(const FStreamSearch* InSearchOriginator, FSearchData& OutSearchData)
{
	// Check if the thread has been told to pause, this occurs for the Garbage Collector and for saving to disk
	if(bIsPausing == true)
	{
		// Pause all searching, the GC is running and we will also be saving the database
		ActiveSearchCounter.Decrement();
		FScopeLock ScopeLock(&PauseThreadsCriticalSection);
		ActiveSearchCounter.Increment();
	}

	int32* SearchIdxPtr = NULL;

	{
		// Must lock this behind a critical section to ensure that no other thread is accessing it at the same time
		FScopeLock ScopeLock(&SafeQueryModifyCriticalSection);
		SearchIdxPtr = ActiveSearchQueries.Find(InSearchOriginator);
	}

	if(SearchIdxPtr)
	{
		int32& SearchIdx = *SearchIdxPtr;
		while(SearchIdx < SearchArray.Num())
		{
			// If the Blueprint is not marked for deletion, and the asset is valid, we will check to see if we want to refresh the searchable data.
			if( SearchArray[SearchIdx].bMarkedForDeletion || (SearchArray[SearchIdx].Blueprint.IsValid() && SearchArray[SearchIdx].Blueprint->HasAllFlags(RF_PendingKill)) )
			{
				// Mark it for deletion, it will be removed on next save
				SearchArray[SearchIdx].bMarkedForDeletion = true;
			}
			else
			{
 				OutSearchData = SearchArray[SearchIdx++];
				if(OutSearchData.Value.IsEmpty())
				{
					if(OutSearchData.Blueprint.IsValid())
					{
						RetrieveDDCData(SearchArray[SearchIdx - 1], *OutSearchData.Blueprint->GetPathName());
					}
					else
					{
						// Attempt to retrieve the data from the DDC, if there is none still, it
						RetrieveDDCData(SearchArray[SearchIdx - 1], *SearchArray[SearchIdx - 1].BlueprintPath);
					}
					OutSearchData = SearchArray[SearchIdx - 1];
				}
				return true;
			}

			++SearchIdx;
		}
	}

	{
		// Must lock this behind a critical section to ensure that no other thread is accessing it at the same time
		FScopeLock ScopeLock(&SafeQueryModifyCriticalSection);
		ActiveSearchQueries.Remove(InSearchOriginator);
	}
	ActiveSearchCounter.Decrement();

	return false;
}

void FFindInBlueprintSearchManager::EnsureSearchQueryEnds(const class FStreamSearch* InSearchOriginator)
{
	// Must lock this behind a critical section to ensure that no other thread is accessing it at the same time
	FScopeLock ScopeLock(&SafeQueryModifyCriticalSection);
	int32* SearchIdxPtr = ActiveSearchQueries.Find(InSearchOriginator);

	// If the search thread is still considered active, remove it
	if(SearchIdxPtr)
	{
		ActiveSearchQueries.Remove(InSearchOriginator);
		ActiveSearchCounter.Decrement();
	}
}

float FFindInBlueprintSearchManager::GetPercentComplete(const FStreamSearch* InSearchOriginator) const
{
	FScopeLock ScopeLock(&SafeQueryModifyCriticalSection);
	const int32* SearchIdxPtr = ActiveSearchQueries.Find(InSearchOriginator);

	float ReturnPercent = 0.0f;

	if(SearchIdxPtr)
	{
		ReturnPercent = (float)*SearchIdxPtr / (float)SearchArray.Num();
	}

	return ReturnPercent;
}

void FFindInBlueprintSearchManager::RetrieveDDCData(FSearchData& InOutSearchData, FName InBlueprintPath)
{
	FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();

	// The data is valid, but we may still need to wait on the DDC.
	if(InOutSearchData.DDCRetrievalID != INDEX_NONE)
	{
		// Allow only one thread to query the DDC at a single time, this is important in the case that two threads are waiting for the same DDC information, once the information is pulled, it is deleted and is no longer accessible.
		FScopeLock ScopeLock(&SafeModifyCacheCriticalSection);

		// Check to see if the data was pulled by another thread while we waited
		if(InOutSearchData.DDCRetrievalID != INDEX_NONE)
		{
			// Wait for completion of the DDC retrieval, we are on a separate thread so this will not stall the editor
			DDC.WaitAsynchronousCompletion(InOutSearchData.DDCRetrievalID);

			TArray<uint8> DerivedData;

			if( DDC.GetAsynchronousResults(InOutSearchData.DDCRetrievalID, DerivedData) ) 
			{
				check(DerivedData.Num());

				FMemoryReader DDCAr(DerivedData, /*bIsPersistent=*/ true);
				DDCAr << InOutSearchData.BlueprintPath;
				DDCAr << InOutSearchData.Value;
				DDCAr.Close();
			}

			InOutSearchData.DDCRetrievalID = INDEX_NONE;
		}
	}
}

FString FFindInBlueprintSearchManager::QuerySingleBlueprint(UBlueprint* InBlueprint, bool bInRebuildSearchData/* = true*/)
{
	if(bInRebuildSearchData)
	{
		// Update the Blueprint, make sure it is fully up-to-date
		AddOrUpdateBlueprintSearchMetadata(InBlueprint, true);
	}

	FString Key = InBlueprint->GetOutermost()->GetPathName();

	int32* ArrayIdx = SearchMap.Find(Key);
	// This should always be true since we make sure to refresh the search data for this Blueprint when doing the search, unless we do not rebuild the searchable data
	check((bInRebuildSearchData && ArrayIdx && *ArrayIdx < SearchArray.Num()) || !bInRebuildSearchData);

	if(ArrayIdx)
	{
		return SearchArray[*ArrayIdx].Value;
	}
	return FString();
}

void FFindInBlueprintSearchManager::PauseFindInBlueprintSearch()
{
	// Lock the critical section and flag that threads need to pause, they will pause when they can
	PauseThreadsCriticalSection.Lock();
	bIsPausing = true;

	// It is UNSAFE to lock any other critical section here, threads need them to finish a cycle of searching. Next cycle they will pause

	// Wait until all threads have come to a stop, it won't take long
	while(ActiveSearchCounter.GetValue() > 0)
	{
		FPlatformProcess::Sleep(0.1f);
	}
}

void FFindInBlueprintSearchManager::UnpauseFindInBlueprintSearch()
{
	// Before unpausing, we clean the cache of any excess data to keep it from bloating in size
	CleanCache();
	bIsPausing = false;

	// Release the threads to continue searching.
	PauseThreadsCriticalSection.Unlock();
}

void FFindInBlueprintSearchManager::CleanCache()
{
	// *NOTE* SaveCache is a thread safe operation by design, all searching threads are paused during the operation so there is no critical section locking

	// We need to cache where the active queries are so that we can put them back in a safe and expected position
	TMap< const FStreamSearch*, FString > CacheQueries;
	for( auto It = ActiveSearchQueries.CreateIterator() ; It ; ++It )
	{
	 	const FStreamSearch* ActiveSearch = It.Key();
	 	check(ActiveSearch);
	 	{
			FSearchData searchData;
	 		ContinueSearchQuery(ActiveSearch, searchData);

			// We will be looking the path up in the map, which uses the package path instead of the Blueprint, so rebuild the package path from the Blueprint path
			FString CachePath = FPaths::GetPath(searchData.BlueprintPath) / FPaths::GetBaseFilename(searchData.BlueprintPath);
	 		CacheQueries.Add(ActiveSearch, CachePath);
	 	}
	}

	TMap<FString, int> NewSearchMap;
	TArray<FSearchData> NewSearchArray;

	for(auto& SearchValuePair : SearchMap)
	{
		// Here it builds the new map/array, clean of deleted content.

		// If the database item is not marked for deletion and not pending kill (if loaded), keep it in the database
		if( !SearchArray[SearchValuePair.Value].bMarkedForDeletion && !(SearchArray[SearchValuePair.Value].Blueprint.IsValid() && SearchArray[SearchValuePair.Value].Blueprint->HasAllFlags(RF_PendingKill)) )
		{
			// Build the new map/array
			NewSearchMap.Add(SearchValuePair.Key, NewSearchArray.Add(SearchArray[SearchValuePair.Value]) );
		}
		else
		{
			// Level Blueprints are destroyed when you open a new level, we need to re-add it as an unloaded asset so long as they were not marked for deletion
			if(!SearchArray[SearchValuePair.Value].bMarkedForDeletion)
			{
				SearchArray[SearchValuePair.Value].Blueprint = nullptr;

				AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

				// The asset was not user deleted, so this should usually find the asset. New levels can be deleted if they were not saved
				FAssetData AssetData = AssetRegistryModule->Get().GetAssetByObjectPath(*SearchArray[SearchValuePair.Value].BlueprintPath);
				if(AssetData.IsValid())
				{
					if(const FString* FiBSearchData = AssetData.TagsAndValues.Find("FiB"))
					{
						SearchArray[SearchValuePair.Value].Value = *FiBSearchData;
					}
					// Build the new map/array
					NewSearchMap.Add(SearchValuePair.Key, NewSearchArray.Add(SearchArray[SearchValuePair.Value]) );
				}
				else
				{
					SearchArray[SearchValuePair.Value].Blueprint = SearchArray[SearchValuePair.Value].Blueprint;
				}
			}
		}
	}

	SearchMap = NewSearchMap;
	SearchArray = NewSearchArray;

	// After the search, we have to place the active search queries where they belong
	for( auto& CacheQuery : CacheQueries )
	{
	 	int32 NewMappedIndex = 0;
	 	// If the CachePath has a length, it means it is valid, otherwise we are at the end and there are no more search results, leave the query there so it can handle shutdown on it's own
	 	if(CacheQuery.Value.Len() > 0)
	 	{
	 		int32* NewMappedIndexPtr = SearchMap.Find(CacheQuery.Value);
	 		check(NewMappedIndexPtr);
	 
	 		NewMappedIndex = *NewMappedIndexPtr;
	 	}
	 	else
	 	{
	 		NewMappedIndex = SearchArray.Num();
	 	}
	 
		// Update the active search to the new index of where it is at in the search
	 	*(ActiveSearchQueries.Find(CacheQuery.Key)) = NewMappedIndex;
	}
}

void FFindInBlueprintSearchManager::BuildCache()
{
	AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray< FAssetData > BlueprintAssets;
	FARFilter ClassFilter;
	ClassFilter.bRecursiveClasses = true;
	ClassFilter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());
	ClassFilter.ClassNames.Add(UWorld::StaticClass()->GetFName());

	AssetRegistryModule->Get().GetAssets(ClassFilter, BlueprintAssets);
	
	for( FAssetData& Asset : BlueprintAssets )
	{
		OnAssetAdded(Asset);
	}
}

FText FFindInBlueprintSearchManager::ConvertHexStringToFText(FString InHexString)
{
	TArray<uint8> SerializedData;
	SerializedData.AddZeroed(InHexString.Len());

	HexToBytes(InHexString, SerializedData.GetData());

	FText ResultText;

	FMemoryReader Ar(SerializedData, /*bIsPersistent=*/ true);
	Ar << ResultText;
	Ar.Close();

	return ResultText;
}

FString FFindInBlueprintSearchManager::ConvertFTextToHexString(FText InValue)
{
	TArray<uint8> SerializedData;
	FMemoryWriter Ar(SerializedData, /*bIsPersistent=*/ true);

	Ar << InValue;
	Ar.Close();

	return BytesToHex(SerializedData.GetData(), SerializedData.Num());
}

TSharedPtr< FJsonObject > FFindInBlueprintSearchManager::ConvertJsonStringToObject(FString InJsonString)
{
	if(InJsonString.Len() && InJsonString[0] == '{')
	{
		// This is a DDC version Json string, use old method of deserialization
		TSharedPtr< FJsonObject > JsonObject = NULL;
		TSharedRef< TJsonReader<> > Reader = BlueprintSearchMetaDataHelpers::SearchMetaDataReaderDDC::Create( InJsonString );
		FJsonSerializer::Deserialize( Reader, JsonObject );

		return JsonObject;
	}

	/** The searchable data is more complicated than a Json string, the Json being the main searchable body that is parsed. Below is a diagram of the full data:
	 *  | int32 "Size" | TMap "Lookup Table" | Json String |
	 *
	 * Size: The size of the TMap in bytes
	 * Lookup Table: The Json's identifiers and string values are in Hex strings and stored in a TMap, the Json stores these values as ints and uses them as the Key into the TMap
	 * Json String: The Json string to be deserialized in full
	 */
	TArray<uint8> DerivedData;

	// SearchData is currently the full string
	// We want to first extract the size of the TMap we will be serializing
	int32 SizeOfData;
	FBufferReader ReaderStream((void*)*InJsonString, InJsonString.Len() * sizeof(TCHAR), false);

	// Read, as a byte string, the number of characters composing the Lookup Table for the Json.
	FString SizeOfDataAsHex;
	SizeOfDataAsHex.GetCharArray().AddUninitialized(5);
	SizeOfDataAsHex[4] = TEXT('\0');
	ReaderStream.Serialize((char*)SizeOfDataAsHex.GetCharArray().GetData(), sizeof(TCHAR) * 4);

	// Convert the number (which is stored in 1 serialized byte per TChar) into an int32
	DerivedData.Empty();
	DerivedData.AddUninitialized(4);
	StringToBytes(SizeOfDataAsHex, DerivedData.GetData(), 4);
	FMemoryReader SizeOfDataAr(DerivedData, /*bIsPersistent=*/ true);
	SizeOfDataAr << SizeOfData;

	// With the size of the TMap in hand, let's serialize JUST that (as a byte string)
	FString MapAsByteString;
	MapAsByteString.GetCharArray().AddUninitialized(SizeOfData + 1);
	MapAsByteString[SizeOfData] = TEXT('\0');
	ReaderStream.Serialize((char*)MapAsByteString.GetCharArray().GetData(), sizeof(TCHAR) * SizeOfData);

	// With the hex of the TMap, serialize that into a TMap
	DerivedData.Empty();
	DerivedData.AddUninitialized(SizeOfData);
	StringToBytes(MapAsByteString, DerivedData.GetData(), SizeOfData);

	TMap<int32, FText> LookupTable;
	FMemoryReader LookupTableAr(DerivedData, /*bIsPersistent=*/ true);
	LookupTableAr << LookupTable;

	// The original BufferReader should be positioned at the Json
	TSharedPtr< FJsonObject > JsonObject = NULL;
	TSharedRef< TJsonReader<> > Reader = BlueprintSearchMetaDataHelpers::SearchMetaDataReader::Create( &ReaderStream, LookupTable );
	FJsonSerializer::Deserialize( Reader, JsonObject );

	return JsonObject;
}

#undef LOCTEXT_NAMESPACE
