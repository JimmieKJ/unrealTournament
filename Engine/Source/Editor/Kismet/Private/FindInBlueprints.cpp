// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "BlueprintEditorPrivatePCH.h"
#include "BlueprintEditorCommands.h"
#include "BlueprintEditor.h"
#include "FindInBlueprints.h"
#include "FindInBlueprintManager.h"
#include "Editor/UnrealEd/Public/Kismet2/DebuggerCommands.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistryModule.h"
#include "Json.h"
#include "IDocumentation.h"
#include "SSearchBox.h"
#include "GenericCommands.h"

#define LOCTEXT_NAMESPACE "FindInBlueprints"

/** Some utility functions to help with Find-in-Blueprint functionality */
namespace FindInBlueprintsHelpers
{
	/**
	 * Retrieves the pin type as a string value
	 *
	 * @param InPinType		The pin type to look at
	 *
	 * @return				The pin type as a string in format [category]'[sub-category object]'
	 */
	FString GetPinTypeAsString(const FEdGraphPinType& InPinType)
	{
		FString Result = InPinType.PinCategory;
		if(UObject* SubCategoryObject = InPinType.PinSubCategoryObject.Get()) 
		{
			Result += FString(" '") + SubCategoryObject->GetName() + "'";
		}

		return Result;
	}

	/**
	 * Determines if a string matches the search tokens
	 *
	 * @param InTokens					Search tokens to compare with
	 * @param InComparisonString		The string to check for matching tokens in
	 *
	 * @return							TRUE if the string matches any of the search tokens
	 */
	bool StringMatchesSearchTokens(const TArray<FString>& InTokens, const FString& InComparisonString)
	{
		const FString SafeSearchString = InComparisonString.Replace( TEXT( " " ), TEXT( "" ) );

		bool bFoundAllTokens = true;
		//search the entry for each token, it must have all of them to pass
		for(auto TokIT(InTokens.CreateConstIterator());TokIT;++TokIT)
		{
			const FString& Token = *TokIT;
			if(!SafeSearchString.Contains(Token))
			{
				bFoundAllTokens = false;
				break;
			}
		}
		return bFoundAllTokens;
	}

	/**
	 * Determines if an FText matches the search tokens, comparing both the (localized) display string and the (unlocalized) source string
	 *
	 * @param InTokens					Search tokens to compare with
	 * @param InComparisonText			The FText to check for matching tokens in
	 *
	 * @return							TRUE if the FText matches any of the search tokens
	 */
	bool TextMatchesSearchTokens(const TArray<FString>& InTokens, const FText& InComparisonText)
	{
		return StringMatchesSearchTokens(InTokens, InComparisonText.ToString()) || StringMatchesSearchTokens(InTokens, InComparisonText.BuildSourceString());
	}

	/**
	 * Helper function to extract all graph data from a Json object and into a search result. There is little type safety, it assumes all Json objects found in the InCategoryTitle are graphs
	 *
	 * @param InJsonObject					The JsonObject to extract the graphs from
	 * @param InCategoryTitle				The category the graphs are titled under.
	 * @param InTokens						Filtering tokens to use
	 * @param InGraphType					There is nothing in the Json script to identify graphs, they are all grouped by category, so pass in what type they should be considered
	 * @param OutBlueprintCategory			The search category these graphs will be children to
	 */
	void ExtractGraph( TSharedPtr< FJsonObject > InJsonObject, FString InCategoryTitle, const TArray<FString>& InTokens, EGraphType InGraphType, FSearchResult& OutBlueprintCategory )
	{
		if(InJsonObject->HasField(InCategoryTitle))
		{
			// Pulls out all functions for this Blueprint
			TArray<TSharedPtr< FJsonValue > > FunctionList = InJsonObject->GetArrayField(InCategoryTitle);
			for( TSharedPtr< FJsonValue > FunctionValue : FunctionList )
			{
				FSearchResult Result(new FFindInBlueprintsGraph(FText::GetEmpty(), OutBlueprintCategory, InGraphType) );

				if( Result->ExtractContent(FunctionValue->AsObject(), InTokens) )
				{
					OutBlueprintCategory->Children.Add(Result);
				}
			}
		}
	}

	/**
	 * Utility function to begin a search
	 *
	 * @param InBuffer					The search buffer, a Json string to be parsed and converted into search result
	 * @param InTokens					Search tokens to filter the results by
	 * @param OutBlueprintCategory		The root category for the search results
	 */
	void Extract( const FString& InBuffer, const TArray<FString>& InTokens, FSearchResult& OutBlueprintCategory )
	{
		// Skip empty buffers
		if(InBuffer.Len() == 0)
		{
			return;
		}

		TSharedPtr< FJsonObject > JsonObject = NULL;
		JsonObject = FFindInBlueprintSearchManager::ConvertJsonStringToObject(InBuffer);

		if(JsonObject.IsValid())
		{
			if(JsonObject->HasField(FFindInBlueprintSearchTags::FiB_Properties.ToString()))
			{
				// Pulls out all properties (variables) for this Blueprint
				TArray<TSharedPtr< FJsonValue > > PropertyList = JsonObject->GetArrayField(FFindInBlueprintSearchTags::FiB_Properties.ToString());
				for( TSharedPtr< FJsonValue > PropertyValue : PropertyList )
				{
					FSearchResult Result(new FFindInBlueprintsProperty(FText::GetEmpty(), OutBlueprintCategory) );

					if( Result->ExtractContent(PropertyValue->AsObject(), InTokens) )
					{
						OutBlueprintCategory->Children.Add(Result);
					}
				}
			}

			ExtractGraph(JsonObject, FFindInBlueprintSearchTags::FiB_Functions.ToString(), InTokens, EGraphType::GT_Function, OutBlueprintCategory);
			ExtractGraph(JsonObject, FFindInBlueprintSearchTags::FiB_Macros.ToString(), InTokens, EGraphType::GT_Macro, OutBlueprintCategory);
			ExtractGraph(JsonObject, FFindInBlueprintSearchTags::FiB_UberGraphs.ToString(), InTokens, EGraphType::GT_Ubergraph, OutBlueprintCategory);
			ExtractGraph(JsonObject, FFindInBlueprintSearchTags::FiB_SubGraphs.ToString(), InTokens, EGraphType::GT_Ubergraph, OutBlueprintCategory);

			if(JsonObject->HasField(FFindInBlueprintSearchTags::FiB_Components.ToString()))
			{
				FSearchResult ComponentsResult(new FFindInBlueprintsResult(FFindInBlueprintSearchTags::FiB_Components, OutBlueprintCategory) );

				// Pulls out all properties (variables) for this Blueprint
				TArray<TSharedPtr< FJsonValue > > ComponentList = JsonObject->GetArrayField(FFindInBlueprintSearchTags::FiB_Components.ToString());
				for( TSharedPtr< FJsonValue > ComponentValue : ComponentList )
				{
					FSearchResult Result(new FFindInBlueprintsProperty(FText::GetEmpty(), ComponentsResult) );

					if( Result->ExtractContent(ComponentValue->AsObject(), InTokens) )
					{
						ComponentsResult->Children.Add(Result);
					}
				}

				// Only add the component list category if there were components
				if(ComponentsResult->Children.Num())
				{
					OutBlueprintCategory->Children.Add(ComponentsResult);
				}
			}
		}
	}

	/**
	 * Parses a pin type from passed in key names and values
	 *
	 * @param InKey					The key name for what the data should be translated as
	 * @param InValue				Value to be be translated
	 * @param InOutPinType			Modifies the PinType based on the passed parameters, building it up over multiple calls
	 * @param InOutPinSubCategory	When the pin's sub-category is found, this value will be changed. This value will never be wiped during other translations. It is not thread safe to determine the sub-object.
	 * @return						TRUE when the parsing is successful
	 */
	bool ParsePinType(FString InKey, FText InValue, FEdGraphPinType& InOutPinType, FString& OutPinSubCategory)
	{
		bool bParsed = true;

		if(InKey == FFindInBlueprintSearchTags::FiB_PinCategory.ToString())
		{
			InOutPinType.PinCategory = InValue.ToString();
		}
		else if(InKey == FFindInBlueprintSearchTags::FiB_PinSubCategory.ToString())
		{
			InOutPinType.PinSubCategory = InValue.ToString();
		}
		else if(InKey == FFindInBlueprintSearchTags::FiB_ObjectClass.ToString())
		{
			OutPinSubCategory = InValue.ToString();
		}
		else if(InKey == FFindInBlueprintSearchTags::FiB_IsArray.ToString())
		{
			InOutPinType.bIsArray = InValue.ToString().ToBool();
		}
		else if(InKey == FFindInBlueprintSearchTags::FiB_IsReference.ToString())
		{
			InOutPinType.bIsReference = InValue.ToString().ToBool();
		}
		else
		{
			bParsed = false;
		}

		return bParsed;
	}
}

//////////////////////////////////////////////////////////////////////////
// FBlueprintSearchResult

FFindInBlueprintsResult::FFindInBlueprintsResult(const FText& InDisplayText ) 
	: DisplayText(InDisplayText)
{
}

FFindInBlueprintsResult::FFindInBlueprintsResult( const FText& InDisplayText, TSharedPtr<FFindInBlueprintsResult> InParent) 
	: Parent(InParent), DisplayText(InDisplayText)
{
}

FReply FFindInBlueprintsResult::OnClick()
{
	// If there is a parent, handle it using the parent's functionality
	if(Parent.IsValid())
	{
		return Parent.Pin()->OnClick();
	}
	else
	{
		// As a last resort, find the parent Blueprint, and open that, it will get the user close to what they want
		UBlueprint* Blueprint = GetParentBlueprint();
		if(Blueprint)
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Blueprint, false);
		}
	}
	
	return FReply::Handled();
}

bool FFindInBlueprintsResult::ParseSearchInfo(const TArray<FString> &InTokens, FString InKey, FText InValue, TSharedPtr< FFindInBlueprintsResult > InParent)
{
	// By default, nothing much gets processed, just check to see if it matches the search requirements
	return FindInBlueprintsHelpers::TextMatchesSearchTokens(InTokens, InValue);
}

void FFindInBlueprintsResult::AddExtraSearchInfo(FString InKey, FText InValue, TSharedPtr< FFindInBlueprintsResult > InParent)
{
	FText DisplayText;

	FFormatNamedArguments Args;
	Args.Add(TEXT("Key"), FText::FromString(InKey));
	Args.Add(TEXT("Value"), InValue);
	DisplayText = FText::Format(LOCTEXT("ExtraSearchInfo", "{Key}: {Value}"), Args);

	TSharedPtr< FFindInBlueprintsResult > SearchResult(new FFindInBlueprintsResult(DisplayText, InParent) );
	InParent->Children.Add(SearchResult);
}

bool FFindInBlueprintsResult::ExtractJsonValue(const TArray<FString>& InTokens, FText InKey, TSharedPtr< FJsonValue > InValue, TSharedPtr< FFindInBlueprintsResult > InParentOverride)
{
	bool bMatchesSearchResults = false;

	TSharedPtr< FFindInBlueprintsResult > ParentResultPtr = InParentOverride.IsValid()? InParentOverride : SharedThis( this );

	if( InValue->Type == EJson::Array)
	{
		TSharedPtr< FFindInBlueprintsResult > ArrayHeader = MakeShareable(new FFindInBlueprintsResult(InKey, ParentResultPtr) );
		bool bArrayHasMatches = false;

		TArray< TSharedPtr< FJsonValue > > Array = InValue->AsArray();
		for( int ArrayIdx = 0; ArrayIdx < Array.Num(); ++ArrayIdx )
		{
			if(Array[ArrayIdx]->Type == EJson::String)
			{
				bArrayHasMatches |= ParseSearchInfo(InTokens, FString::FromInt(ArrayIdx), FText::FromString(Array[ArrayIdx]->AsString()), ArrayHeader);
			}
			else
			{
				bArrayHasMatches |= ExtractJsonValue(InTokens, FText::FromString(FString::FromInt(ArrayIdx)), Array[ArrayIdx], ArrayHeader);
			}
		}

		bMatchesSearchResults |= bArrayHasMatches;

		if(bArrayHasMatches)
		{
			ParentResultPtr->Children.Add(ArrayHeader);
		}
	}
	else if(InValue->Type == EJson::Object)
	{
		TSharedPtr< FFindInBlueprintsResult > ObjectResultHeader = MakeShareable(new FFindInBlueprintsResult(InKey, ParentResultPtr) );

		bMatchesSearchResults |= ExtractContent(InValue->AsObject(), InTokens, ObjectResultHeader);

		if(bMatchesSearchResults)
		{
			ParentResultPtr->Children.Add(ObjectResultHeader);
		}
	}
	else if(InValue->Type == EJson::String)
	{
		bMatchesSearchResults |= ParseSearchInfo(InTokens, InKey.ToString(), FText::FromString(InValue->AsString()), ParentResultPtr);
	}
	else
	{
		bMatchesSearchResults |= ParseSearchInfo(InTokens, InKey.ToString(), FText::FromString(InValue->AsString()), ParentResultPtr);
	}

	return bMatchesSearchResults;
}

bool FFindInBlueprintsResult::ExtractContent(TSharedPtr< FJsonObject > InJsonNode, const TArray<FString>& InTokens, TSharedPtr< FFindInBlueprintsResult > InParentOverride)
{
	bool bMatchesSearchResults = false;

	TSharedPtr< FFindInBlueprintsResult > ParentResultPtr = InParentOverride.IsValid()? InParentOverride : SharedThis( this );

	for( auto MapValues : InJsonNode->Values )
	{
		bMatchesSearchResults |= ExtractJsonValue(InTokens, FText::FromString(MapValues.Key), MapValues.Value, ParentResultPtr);
	}
	return bMatchesSearchResults;
}

FString FFindInBlueprintsResult::GetCategory() const
{
	return FString();
}

TSharedRef<SWidget> FFindInBlueprintsResult::CreateIcon() const
{
	FLinearColor IconColor = FLinearColor::White;
	const FSlateBrush* Brush = NULL;

	return 	SNew(SImage)
			.Image(Brush)
			.ColorAndOpacity(IconColor)
			.ToolTipText( GetCategory() );
}

FString FFindInBlueprintsResult::GetCommentText() const
{
	return CommentText;
}

UBlueprint* FFindInBlueprintsResult::GetParentBlueprint() const
{
	UBlueprint* ResultBlueprint = nullptr;
	if (Parent.IsValid())
	{
		ResultBlueprint = Parent.Pin()->GetParentBlueprint();
	}
	else
	{
		GIsEditorLoadingPackage = true;
		UObject* Object = LoadObject<UObject>(NULL, *DisplayText.ToString(), NULL, 0, NULL); //StaticLoadObject(UObject::StaticClass(), NULL, *DisplayText.ToString());
		GIsEditorLoadingPackage = false;

		if(UBlueprint* BlueprintObj = Cast<UBlueprint>(Object))
		{
			ResultBlueprint = BlueprintObj;
		}
		else if(UWorld* WorldObj = Cast<UWorld>(Object))
		{
			if(WorldObj->PersistentLevel)
			{
				ResultBlueprint = Cast<UBlueprint>((UObject*)WorldObj->PersistentLevel->GetLevelScriptBlueprint(true));
			}
		}

	}
	return ResultBlueprint;
}

void FFindInBlueprintsResult::ExpandAllChildren( TSharedPtr< STreeView< TSharedPtr< FFindInBlueprintsResult > > > InTreeView )
{
	if( Children.Num() )
	{
		InTreeView->SetItemExpansion(this->AsShared(), true);
		for( int32 i = 0; i < Children.Num(); i++ )
		{
			Children[i]->ExpandAllChildren(InTreeView);
		}
	}
}

FString FFindInBlueprintsResult::GetDisplayString() const
{
	return DisplayText.ToString();
}

//////////////////////////////////////////////////////////
// FFindInBlueprintsGraphNode

FFindInBlueprintsGraphNode::FFindInBlueprintsGraphNode(const FText& InValue, TSharedPtr<FFindInBlueprintsResult> InParent)
	: FFindInBlueprintsResult(InValue, InParent)
	, Class(nullptr)
{
}

FReply FFindInBlueprintsGraphNode::OnClick()
{
	UBlueprint* Blueprint = GetParentBlueprint();
	if(Blueprint)
	{
		UEdGraphNode* OutNode = NULL;
		if(	UEdGraphNode* GraphNode = FBlueprintEditorUtils::GetNodeByGUID(Blueprint, NodeGuid) )
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(GraphNode, /*bRequestRename=*/false);
			return FReply::Handled();
		}
	}

	return FFindInBlueprintsResult::OnClick();
}

TSharedRef<SWidget> FFindInBlueprintsGraphNode::CreateIcon() const
{
	return 	SNew(SImage)
		.Image(GlyphBrush)
		.ColorAndOpacity(GlyphColor)
		.ToolTipText( GetCategory() );
}

bool FFindInBlueprintsGraphNode::ParseSearchInfo(const TArray<FString> &InTokens, FString InKey, FText InValue, TSharedPtr< FFindInBlueprintsResult > InParent)
{
	if(InKey == FFindInBlueprintSearchTags::FiB_NodeGuid.ToString())
	{
		FString NodeGUIDAsString = InValue.ToString();
		FGuid::Parse(NodeGUIDAsString, NodeGuid);

		// NodeGuid's are not searchable
		return false;
	}

	bool bMatchesTokens = FindInBlueprintsHelpers::TextMatchesSearchTokens(InTokens, InValue);

	if(InKey == FFindInBlueprintSearchTags::FiB_ClassName.ToString())
	{
		ClassName = InValue.ToString();
		bMatchesTokens = false;
	}
	else if(InKey == FFindInBlueprintSearchTags::FiB_Name.ToString())
	{
		DisplayText = InValue;
	}
	else if(InKey == FFindInBlueprintSearchTags::FiB_Comment.ToString())
	{
		CommentText = InValue.ToString();
	}
	else if(InKey == FFindInBlueprintSearchTags::FiB_Glyph.ToString())
	{
		GlyphBrush = FEditorStyle::GetBrush(*InValue.ToString());
		bMatchesTokens = false;
	}
	else if(InKey == FFindInBlueprintSearchTags::FiB_GlyphColor.ToString())
	{
		GlyphColor.InitFromString(InValue.ToString());
		bMatchesTokens = false;
	}
	else
	{
		// Any generic keys and value pairs get put in as "extra" search info
		if(bMatchesTokens)
		{
			AddExtraSearchInfo(InKey, InValue, InParent);
			return true;
		}
	}
	return bMatchesTokens;
}

bool FFindInBlueprintsGraphNode::ExtractContent(TSharedPtr< FJsonObject > InJsonNode, const TArray<FString>& InTokens)
{
	// Very important to get the schema first, other bits of data depend on it
	FString SchemaName;
	TSharedPtr< FJsonValue > SchemaNameValue = InJsonNode->GetField< EJson::String >(FFindInBlueprintSearchTags::FiB_SchemaName.ToString());
	if(SchemaNameValue.IsValid())
	{
		SchemaName = SchemaNameValue->AsString();
	}

	bool bMatchesSearchResults = false;

	for( auto MapValues : InJsonNode->Values )
	{
		FText Key = FText::FromString(MapValues.Key);

		if(Key.CompareTo(FFindInBlueprintSearchTags::FiB_Pins) == 0)
		{
			TArray< TSharedPtr< FJsonValue > > PinsList = MapValues.Value->AsArray();

			for( TSharedPtr< FJsonValue > Pin : PinsList )
			{
				// The pin
				TSharedPtr< FFindInBlueprintsPin > PinResult(new FFindInBlueprintsPin(FText::GetEmpty(), SharedThis( this ), SchemaName) );
				if(PinResult->ExtractContent(Pin->AsObject(), InTokens))
				{
					Children.Add(PinResult);
					bMatchesSearchResults = true;
				}
			}
		}
		else if(Key.CompareTo(FFindInBlueprintSearchTags::FiB_SchemaName) == 0)
		{
			// Previously extracted
			continue;
		}
		else
		{
			bMatchesSearchResults |= ExtractJsonValue(InTokens, FText::FromString(MapValues.Key), MapValues.Value, SharedThis( this ));
		}
	}
	return bMatchesSearchResults;
}

FString FFindInBlueprintsGraphNode::GetCategory() const
{
	if(Class == UK2Node_CallFunction::StaticClass())
	{
		return LOCTEXT("CallFuctionCat", "Function Call").ToString();
	}
	else if(Class == UK2Node_MacroInstance::StaticClass())
	{
		return LOCTEXT("MacroCategory", "Macro").ToString();
	}
	else if(Class == UK2Node_Event::StaticClass())
	{
		return LOCTEXT("EventCat", "Event").ToString();
	}
	else if(Class == UK2Node_VariableGet::StaticClass())
	{
		return LOCTEXT("VariableGetCategory", "Variable Get").ToString();
	}
	else if(Class == UK2Node_VariableSet::StaticClass())
	{
		return LOCTEXT("VariableSetCategory", "Variable Set").ToString();
	}

	return LOCTEXT("NodeCategory", "Node").ToString();
}

void FFindInBlueprintsGraphNode::FinalizeSearchData()
{
	if(!ClassName.IsEmpty())
	{
		Class = FindObject<UClass>(ANY_PACKAGE, *ClassName, true);
		ClassName.Empty();
	}
}

//////////////////////////////////////////////////////////
// FFindInBlueprintsPin

FFindInBlueprintsPin::FFindInBlueprintsPin(const FText& InValue, TSharedPtr<FFindInBlueprintsResult> InParent, FString InSchemaName)
	: FFindInBlueprintsResult(InValue, InParent)
	, SchemaName(InSchemaName)
	, IconColor(FSlateColor::UseForeground())
{
}

TSharedRef<SWidget> FFindInBlueprintsPin::CreateIcon() const
{
	const FSlateBrush* Brush = NULL;

	if( PinType.bIsArray )
	{
		Brush = FEditorStyle::GetBrush( TEXT("GraphEditor.ArrayPinIcon") );
	}
	else if( PinType.bIsReference )
	{
		Brush = FEditorStyle::GetBrush( TEXT("GraphEditor.RefPinIcon") );
	}
	else
	{
		Brush = FEditorStyle::GetBrush( TEXT("GraphEditor.PinIcon") );
	}

	return 	SNew(SImage)
		.Image(Brush)
		.ColorAndOpacity(IconColor)
		.ToolTipText(FindInBlueprintsHelpers::GetPinTypeAsString(PinType) );
}

bool FFindInBlueprintsPin::ParseSearchInfo(const TArray<FString> &InTokens, FString InKey, FText InValue, TSharedPtr< FFindInBlueprintsResult > InParent)
{
	bool bMatchesTokens = FindInBlueprintsHelpers::TextMatchesSearchTokens(InTokens, InValue);

	if(InKey == FFindInBlueprintSearchTags::FiB_Name.ToString())
	{
		DisplayText = InValue;
		bMatchesTokens = FindInBlueprintsHelpers::StringMatchesSearchTokens(InTokens, DisplayText.ToString());
	}
	else if(FindInBlueprintsHelpers::ParsePinType(InKey, InValue, PinType, PinSubCategory))
	{
		// For pins, the only information in the Pin Type we care about is the object class, so return false if the Key is not that
		if(InKey != FFindInBlueprintSearchTags::FiB_ObjectClass.ToString())
		{
			return false;
		}
	}
	else
	{
		// Any generic keys and value pairs get put in as "extra" search info
		if(bMatchesTokens)
		{
			AddExtraSearchInfo(InKey, InValue, InParent);
		}
	}
	return bMatchesTokens;
}

FString FFindInBlueprintsPin::GetCategory() const
{
	return LOCTEXT("PinCategory", "Pin").ToString();
}

void FFindInBlueprintsPin::FinalizeSearchData()
{
	if(!PinSubCategory.IsEmpty())
	{
		PinType.PinSubCategoryObject = FindObject<UClass>(ANY_PACKAGE, *PinSubCategory, true);
		if(!PinType.PinSubCategoryObject.IsValid())
		{
			PinType.PinSubCategoryObject = FindObject<UScriptStruct>(UObject::StaticClass(), *PinSubCategory);
		}
		PinSubCategory.Empty();
	}

	if(!SchemaName.IsEmpty())
	{
		UEdGraphSchema* Schema = nullptr;
		UClass* SchemaClass = FindObject<UClass>(ANY_PACKAGE, *SchemaName, true);
		if(SchemaClass)
		{
			Schema = SchemaClass->GetDefaultObject<UEdGraphSchema>();
		} 
		IconColor = Schema->GetPinTypeColor(PinType);

		SchemaName.Empty();
	}
}

//////////////////////////////////////////////////////////
// FFindInBlueprintsProperty

FFindInBlueprintsProperty::FFindInBlueprintsProperty(const FText& InValue, TSharedPtr<FFindInBlueprintsResult> InParent)
	: FFindInBlueprintsResult(InValue, InParent)
	, bIsSCSComponent(false)
{
}

TSharedRef<SWidget> FFindInBlueprintsProperty::CreateIcon() const
{
	FLinearColor IconColor = FLinearColor::White;
	const FSlateBrush* Brush = FEditorStyle::GetBrush(UK2Node_Variable::GetVarIconFromPinType(PinType, IconColor));
	IconColor = UEdGraphSchema_K2::StaticClass()->GetDefaultObject<UEdGraphSchema_K2>()->GetPinTypeColor(PinType);

	return 	SNew(SImage)
		.Image(Brush)
		.ColorAndOpacity(IconColor)
		.ToolTipText( FindInBlueprintsHelpers::GetPinTypeAsString(PinType) );
}

bool FFindInBlueprintsProperty::ParseSearchInfo(const TArray<FString> &InTokens, FString InKey, FText InValue, TSharedPtr< FFindInBlueprintsResult > InParent)
{
	bool bMatchesTokens = FindInBlueprintsHelpers::TextMatchesSearchTokens(InTokens, InValue);

	if(InKey == FFindInBlueprintSearchTags::FiB_Name.ToString())
	{
		DisplayText = InValue;
	}
	else if(FindInBlueprintsHelpers::ParsePinType(InKey, InValue, PinType, PinSubCategory))
	{
		// For properties, we care about all the information in the pin type, except whether its a reference or an array
		if(InKey == FFindInBlueprintSearchTags::FiB_IsArray.ToString() || InKey == FFindInBlueprintSearchTags::FiB_IsReference.ToString())
		{
			return false;
		}
	}
	else if(InKey == FFindInBlueprintSearchTags::FiB_IsSCSComponent.ToString())
	{
		bIsSCSComponent = true;

		// This value is not searchable
		bMatchesTokens = false;
	}
	else
	{
		// Any generic keys and value pairs get put in as "extra" search info
		if(bMatchesTokens)
		{
			AddExtraSearchInfo(InKey, InValue, InParent);
		}
	}
	return bMatchesTokens;
}

FString FFindInBlueprintsProperty::GetCategory() const
{
	if(bIsSCSComponent)
	{
		return LOCTEXT("Component", "Component").ToString();
	}
	return LOCTEXT("Variable", "Variable").ToString();
}

void FFindInBlueprintsProperty::FinalizeSearchData()
{
	if(!PinSubCategory.IsEmpty())
	{
		PinType.PinSubCategoryObject = FindObject<UClass>(ANY_PACKAGE, *PinSubCategory, true);
		if(!PinType.PinSubCategoryObject.IsValid())
		{
			PinType.PinSubCategoryObject = FindObject<UScriptStruct>(UObject::StaticClass(), *PinSubCategory);
		}
		PinSubCategory.Empty();
	}
}

//////////////////////////////////////////////////////////
// FFindInBlueprintsGraph

FFindInBlueprintsGraph::FFindInBlueprintsGraph(const FText& InValue, TSharedPtr<FFindInBlueprintsResult> InParent, EGraphType InGraphType)
	: FFindInBlueprintsResult(InValue, InParent)
	, GraphType(InGraphType)
{
}

FReply FFindInBlueprintsGraph::OnClick()
{
	UBlueprint* Blueprint = GetParentBlueprint();
	if(Blueprint)
	{
		TArray<UEdGraph*> BlueprintGraphs;
		Blueprint->GetAllGraphs(BlueprintGraphs);

		for( auto Graph : BlueprintGraphs)
		{
			FGraphDisplayInfo DisplayInfo;
			Graph->GetSchema()->GetGraphDisplayInformation(*Graph, DisplayInfo);

			if(DisplayInfo.PlainName.EqualTo(DisplayText))
			{
				FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Graph);
				break;
			}
		}
	}
	else
	{
		return FFindInBlueprintsResult::OnClick();
	}
	return FReply::Handled();
}

TSharedRef<SWidget> FFindInBlueprintsGraph::CreateIcon() const
{
	const FSlateBrush* Brush = NULL;
	if(GraphType == GT_Function)
	{
		Brush = FEditorStyle::GetBrush(TEXT("GraphEditor.Function_16x"));
	}
	else if(GraphType == GT_Macro)
	{
		Brush = FEditorStyle::GetBrush(TEXT("GraphEditor.Macro_16x"));
	}

	return 	SNew(SImage)
		.Image(Brush)
		.ToolTipText( GetCategory() );
}

bool FFindInBlueprintsGraph::ExtractContent(TSharedPtr< FJsonObject > InJsonNode, const TArray<FString>& InTokens)
{
	bool bMatchesSearchResults = false;

	for( auto MapValues : InJsonNode->Values )
	{
		FText Key = FText::FromString(MapValues.Key);

		if(Key.CompareTo(FFindInBlueprintSearchTags::FiB_Nodes) == 0)
		{
			TArray< TSharedPtr< FJsonValue > > NodeList = MapValues.Value->AsArray();

			for( TSharedPtr< FJsonValue > NodeValue : NodeList )
			{
				TSharedPtr< FFindInBlueprintsGraphNode > NodeResult(new FFindInBlueprintsGraphNode(FText::GetEmpty(), SharedThis( this )) );

				if( NodeResult->ExtractContent(NodeValue->AsObject(), InTokens) )
				{
					Children.Add(NodeResult);
					bMatchesSearchResults = true;
				}
			}
		}
		else if(Key.CompareTo(FFindInBlueprintSearchTags::FiB_SchemaName) == 0)
		{
			// Previously extracted
			continue;
		}
		else if(Key.CompareTo(FFindInBlueprintSearchTags::FiB_Properties) == 0)
		{
			// Pulls out all properties (variables) for this Blueprint
			TArray<TSharedPtr< FJsonValue > > PropertyList = MapValues.Value->AsArray();
			for( TSharedPtr< FJsonValue > PropertyValue : PropertyList )
			{
				FSearchResult Result(new FFindInBlueprintsProperty(FText::GetEmpty(), SharedThis( this )) );

				if( Result->ExtractContent(PropertyValue->AsObject(), InTokens) )
				{
					Children.Add(Result);
					bMatchesSearchResults = true;
				}
			}
		}
		else
		{
			bMatchesSearchResults |= ExtractJsonValue( InTokens, FText::FromString(MapValues.Key), MapValues.Value, SharedThis( this ) );
		}
	}
	return bMatchesSearchResults;
}

bool FFindInBlueprintsGraph::ParseSearchInfo(const TArray<FString> &InTokens, FString InKey, FText InValue, TSharedPtr< FFindInBlueprintsResult > InParent)
{
	bool bMatchesTokens = FindInBlueprintsHelpers::TextMatchesSearchTokens(InTokens, InValue);

	if(InKey == FFindInBlueprintSearchTags::FiB_Name.ToString())
	{
		DisplayText = InValue;
	}
	else
	{
		if(bMatchesTokens)
		{
			AddExtraSearchInfo(InKey, InValue, InParent);
		}
	}

	return bMatchesTokens;
}

FString FFindInBlueprintsGraph::GetCategory() const
{
	if(GraphType == GT_Function)
	{
		return LOCTEXT("FunctionGraphCategory", "Function").ToString();
	}
	else if(GraphType == GT_Macro)
	{
		return LOCTEXT("MacroGraphCategory", "Macro").ToString();
	}
	return LOCTEXT("GraphCategory", "Graph").ToString();
}

////////////////////////////////////
// FStreamSearch

/**
 * Async task for searching Blueprints
 */
class FStreamSearch : public FRunnable
{
public:
	/** Constructor */
	FStreamSearch(const TArray<FString>& InSearchTokens )
		: SearchTokens(InSearchTokens)
		, bThreadCompleted(false)
		, StopTaskCounter(0)
	{
		// Add on a Guid to the thread name to ensure the thread is uniquely named.
		Thread = FRunnableThread::Create( this, *FString::Printf(TEXT("FStreamSearch%s"), *FGuid::NewGuid().ToString()), 0, TPri_BelowNormal );
	}

	/** Begin FRunnable Interface */
	virtual bool Init() override
	{
		return true;
	}

	virtual uint32 Run() override
	{
		FFindInBlueprintSearchManager::Get().BeginSearchQuery(this);

		FSearchData QueryResult;

		// Searching comes to an end if it is requested using the StopTaskCounter or continuing the search query yields no results
		while(StopTaskCounter.GetValue() == 0 && FFindInBlueprintSearchManager::Get().ContinueSearchQuery(this, QueryResult))
		{
			if(QueryResult.BlueprintPath.Len())
			{
				//Each blueprints acts as a category
				FSearchResult BlueprintCategory = FSearchResult(new FFindInBlueprintsResult(FText::FromString(QueryResult.BlueprintPath)));
				FindInBlueprintsHelpers::Extract(QueryResult.Value, SearchTokens, BlueprintCategory);

				// If there are children, add the item to the search results
				if(BlueprintCategory->Children.Num() > 0)
				{
					FScopeLock ScopeLock(&SearchCriticalSection);
					ItemsFound.Add(BlueprintCategory);
				}
			}
		}

		if(StopTaskCounter.GetValue() > 0)
		{
			// Ensure that the FiB Manager knows that we are done searching
			FFindInBlueprintSearchManager::Get().EnsureSearchQueryEnds(this);
		}

		bThreadCompleted = true;

		return 0;
	}

	virtual void Stop() override
	{
		StopTaskCounter.Increment();
	}

	virtual void Exit() override
	{

	}
	/** End FRunnable Interface */

	/** Brings the thread to a safe stop before continuing. */
	void EnsureCompletion()
	{
		{
			FScopeLock CritSectionLock(&SearchCriticalSection);
			ItemsFound.Empty();
		}

		Stop();
		Thread->WaitForCompletion();
		delete Thread;
		Thread = NULL;
	}

	/** Returns TRUE if the thread is done with it's work. */
	bool IsComplete() const
	{
		return bThreadCompleted;
	}

	/**
	 * Appends the items filtered through the search tokens to the passed array
	 *
	 * @param OutItemsFound		All the items found since last queried
	 */
	void GetFilteredItems(TArray<FSearchResult>& OutItemsFound)
	{
		FScopeLock ScopeLock(&SearchCriticalSection);
		OutItemsFound.Append(ItemsFound);
		ItemsFound.Empty();
	}

	/** Helper function to query the percent complete this search is */
	float GetPercentComplete() const
	{
		return FFindInBlueprintSearchManager::Get().GetPercentComplete(this);
	}

public:
	/** Thread to run the cleanup FRunnable on */
	FRunnableThread* Thread;

	/** A list of items found, cleared whenever the main thread pulls them to display to screen */
	TArray<FSearchResult> ItemsFound;

	/** The search tokens to filter results by */
	TArray<FString> SearchTokens;

	/** Prevents searching while other threads are pulling search results */
	FCriticalSection SearchCriticalSection;

	// Whether the thread has finished running
	bool bThreadCompleted;

	/** > 0 if we've been asked to abort work in progress at the next opportunity */
	FThreadSafeCounter StopTaskCounter;
};

//////////////////////////////////////////////////////////////////////////
// SBlueprintSearch

void SFindInBlueprints::Construct( const FArguments& InArgs, TSharedPtr<FBlueprintEditor> InBlueprintEditor)
{
	BlueprintEditorPtr = InBlueprintEditor;

	RegisterCommands();

	bIsInFindWithinBlueprintMode = true;
	
	this->ChildSlot
		[
			SAssignNew(MainVerticalBox, SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SAssignNew(SearchTextField, SSearchBox)
					.HintText(LOCTEXT("BlueprintSearchHint", "Enter function or event name to find references..."))
					.OnTextChanged(this, &SFindInBlueprints::OnSearchTextChanged)
					.OnTextCommitted(this, &SFindInBlueprints::OnSearchTextCommitted)
				]
				+SHorizontalBox::Slot()
				.Padding(2.f, 0.f)
				.AutoWidth()
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this, &SFindInBlueprints::OnFindModeChanged)
					.IsChecked(this, &SFindInBlueprints::OnGetFindModeChecked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BlueprintSearchModeChange", "Find In Current Blueprint Only"))
					]
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				[
					SAssignNew(TreeView, STreeViewType)
					.ItemHeight(24)
					.TreeItemsSource( &ItemsFound )
					.OnGenerateRow( this, &SFindInBlueprints::OnGenerateRow )
					.OnGetChildren( this, &SFindInBlueprints::OnGetChildren )
					.OnMouseButtonDoubleClick(this,&SFindInBlueprints::OnTreeSelectionDoubleClicked)
					.SelectionMode( ESelectionMode::Multi )
					.OnContextMenuOpening(this, &SFindInBlueprints::OnContextMenuOpening)
				]
			]

			+SVerticalBox::Slot()
				.AutoHeight()
			[
				SNew(SHorizontalBox)

				// Text
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 2)
				[
					SNew(STextBlock)
					.Font( FEditorStyle::GetFontStyle("AssetDiscoveryIndicator.DiscovertingAssetsFont") )
					.Text( LOCTEXT("SearchResults", "Searching...").ToString() )
					.Visibility(this, &SFindInBlueprints::GetSearchbarVisiblity)
				]

				// Progress bar
				+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(2.0f, 3.0f, 0.0f, 0.0f)
				[
					SNew(SProgressBar)
						.Visibility(this, &SFindInBlueprints::GetSearchbarVisiblity).Percent( this, &SFindInBlueprints::GetPercentCompleteSearch )
				]
			]
		];
}

FReply SFindInBlueprints::OnRemoveCacheBar()
{
	if(MainVerticalBox.IsValid() && CacheBarSlot.IsValid())
	{
		MainVerticalBox.Pin()->RemoveSlot(CacheBarSlot.Pin().ToSharedRef());
	}

	return FReply::Handled();
}

SFindInBlueprints::~SFindInBlueprints()
{
	if(StreamSearch.IsValid())
	{
		StreamSearch->Stop();
		StreamSearch->EnsureCompletion();
	}
}

void SFindInBlueprints::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if(StreamSearch.IsValid())
	{
		bool bShouldShutdownThread = false;
		bShouldShutdownThread = StreamSearch->IsComplete();

		TArray<FSearchResult> BackgroundItemsFound;

		StreamSearch->GetFilteredItems(BackgroundItemsFound);
		if(BackgroundItemsFound.Num())
		{
			for(auto& Item : BackgroundItemsFound)
			{
				Item->ExpandAllChildren(TreeView);
				ItemsFound.Add(Item);
			}
			TreeView->RequestTreeRefresh();
		}

		// If the thread is complete, shut it down properly
		if(bShouldShutdownThread)
		{
			if(ItemsFound.Num() == 0)
			{
				// Insert a fake result to inform user if none found
				ItemsFound.Add(FSearchResult(new FFindInBlueprintsResult(LOCTEXT("BlueprintSearchNoResults", "No Results found"))));
				TreeView->RequestTreeRefresh();
			}

			StreamSearch->EnsureCompletion();
			StreamSearch.Reset();
		}
	}
}

void SFindInBlueprints::RegisterCommands()
{
	TSharedPtr<FUICommandList> ToolKitCommandList = BlueprintEditorPtr.Pin()->GetToolkitCommands();

	ToolKitCommandList->MapAction( FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &SFindInBlueprints::OnCopyAction) );

	ToolKitCommandList->MapAction( FGenericCommands::Get().SelectAll,
		FExecuteAction::CreateSP(this, &SFindInBlueprints::OnSelectAllAction) );
}

void SFindInBlueprints::FocusForUse(bool bSetFindWithinBlueprint, FString NewSearchTerms, bool bSelectFirstResult)
{
	// NOTE: Careful, GeneratePathToWidget can be reentrant in that it can call visibility delegates and such
	FWidgetPath FilterTextBoxWidgetPath;
	FSlateApplication::Get().GeneratePathToWidgetUnchecked( SearchTextField.ToSharedRef(), FilterTextBoxWidgetPath );

	// Set keyboard focus directly
	FSlateApplication::Get().SetKeyboardFocus( FilterTextBoxWidgetPath, EFocusCause::SetDirectly );

	// Set the filter mode
	bIsInFindWithinBlueprintMode = bSetFindWithinBlueprint;

	if (!NewSearchTerms.IsEmpty())
	{
		SearchTextField->SetText(FText::FromString(NewSearchTerms));
		InitiateSearch();

		// Select the first result
		if (bSelectFirstResult && ItemsFound.Num())
		{
			auto ItemToFocusOn = ItemsFound[0];

			// We want the first childmost item to select, as that is the item that is most-likely to be what was searched for (parents being graphs).
			// Will fail back upward as neccessary to focus on a focusable item
			while(ItemToFocusOn->Children.Num())
			{
				ItemToFocusOn = ItemToFocusOn->Children[0];
			}
			TreeView->SetSelection(ItemToFocusOn);
			ItemToFocusOn->OnClick();
		}
	}
}

void SFindInBlueprints::OnSearchTextChanged( const FText& Text)
{
	SearchValue = Text.ToString();
}

void SFindInBlueprints::OnSearchTextCommitted( const FText& Text, ETextCommit::Type CommitType )
{
	if (CommitType == ETextCommit::OnEnter)
	{
		InitiateSearch();
	}
}

void SFindInBlueprints::OnFindModeChanged(ECheckBoxState CheckState)
{
	bIsInFindWithinBlueprintMode = CheckState == ECheckBoxState::Checked;
}

ECheckBoxState SFindInBlueprints::OnGetFindModeChecked() const
{
	return bIsInFindWithinBlueprintMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFindInBlueprints::InitiateSearch()
{
	TArray<FString> Tokens;
	if(SearchValue.Contains("\"") && SearchValue.ParseIntoArray(&Tokens, TEXT("\""), true)>0)
	{
		for( auto &TokenIt : Tokens )
		{
			// we have the token, we don't need the quotes anymore, they'll just confused the comparison later on
			TokenIt = TokenIt.TrimQuotes();
			// We remove the spaces as all later comparison strings will also be de-spaced
			TokenIt = TokenIt.Replace( TEXT( " " ), TEXT( "" ) );
		}

		// due to being able to handle multiple quoted blocks like ("Make Epic" "Game Now") we can end up with
		// and empty string between (" ") blocks so this simply removes them
		struct FRemoveMatchingStrings
		{ 
			bool operator()(const FString& RemovalCandidate) const
			{
				return RemovalCandidate.IsEmpty();
			}
		};
		Tokens.RemoveAll( FRemoveMatchingStrings() );
	}
	else
	{
		// unquoted search equivalent to a match-any-of search
		SearchValue.ParseIntoArray(&Tokens, TEXT(" "), true);
	}

	if(ItemsFound.Num())
	{
		// Reset the scroll to the top
		TreeView->RequestScrollIntoView(ItemsFound[0]);
	}

	ItemsFound.Empty();

	if (Tokens.Num() > 0)
	{
		OnRemoveCacheBar();

		TreeView->RequestTreeRefresh();

		HighlightText = FText::FromString( SearchValue );
		if (bIsInFindWithinBlueprintMode)
		{
			MatchTokensWithinBlueprint(Tokens);
		}
		else
		{
			LaunchStreamThread(Tokens);
		}
	}
}

void SFindInBlueprints::MatchTokensWithinBlueprint(const TArray<FString>& Tokens)
{
	if(StreamSearch.IsValid() && !StreamSearch->IsComplete())
	{
		StreamSearch->Stop();
		StreamSearch->EnsureCompletion();
		StreamSearch->GetFilteredItems(ItemsFound);
		ItemsFound.Empty();
		StreamSearch.Reset();
	}

	// No multi-threading for single Blueprint search, just query for the data block and process it.
	UBlueprint* Blueprint = BlueprintEditorPtr.Pin()->GetBlueprintObj();

	RootSearchResult = FSearchResult(new FFindInBlueprintsResult(FText::FromString(Blueprint->GetPathName())));
	
	FindInBlueprintsHelpers::Extract(FFindInBlueprintSearchManager::Get().QuerySingleBlueprint(Blueprint), Tokens, RootSearchResult);

	// We do not use the root, the user already knows what Blueprint they are searching within
	ItemsFound = RootSearchResult->Children;
	
	if(ItemsFound.Num() == 0)
	{
		// Insert a fake result to inform user if none found
		ItemsFound.Add(FSearchResult(new FFindInBlueprintsResult(LOCTEXT("BlueprintSearchNoResults", "No Results found"))));
		HighlightText = FText::GetEmpty();
		TreeView->RequestTreeRefresh();
	}
	else
	{
		for(auto Item : ItemsFound)
		{
			Item->ExpandAllChildren(TreeView);
		}
	}

	TreeView->RequestTreeRefresh();
}

void SFindInBlueprints::LaunchStreamThread(const TArray<FString>& InTokens)
{
	if(StreamSearch.IsValid() && !StreamSearch->IsComplete())
	{
		StreamSearch->Stop();
		StreamSearch->EnsureCompletion();
	}

	StreamSearch = MakeShareable(new FStreamSearch(InTokens));
}

TSharedRef<ITableRow> SFindInBlueprints::OnGenerateRow( FSearchResult InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	// Finalize the search data, this does some non-thread safe actions that could not be done on the separate thread.
	InItem->FinalizeSearchData();

	bool bIsACategoryWidget = !bIsInFindWithinBlueprintMode && !InItem->Parent.IsValid();

	if (bIsACategoryWidget)
	{
		return SNew( STableRow< TSharedPtr<FFindInBlueprintsResult> >, OwnerTable )
			[
				SNew(SBorder)
				.VAlign(VAlign_Center)
				.BorderImage(FEditorStyle::GetBrush("PropertyWindow.CategoryBackground"))
				.Padding(FMargin(2.0f))
				.ForegroundColor(FEditorStyle::GetColor("PropertyWindow.CategoryForeground"))
				[
					SNew(STextBlock)
					.Text(InItem.Get(), &FFindInBlueprintsResult::GetDisplayString)
					.ToolTipText(LOCTEXT("BlueprintCatSearchToolTip", "Blueprint").ToString())
				]
			];
	}
	else // Functions/Event/Pin widget
	{
		FString CommentText;

		if(!InItem->GetCommentText().IsEmpty())
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Comment"), FText::FromString(InItem->GetCommentText()));

			CommentText = FText::Format(LOCTEXT("NodeComment", "Node Comment:[{Comment}]"), Args).ToString();
		}

		FString Tooltip;

		FFormatNamedArguments Args;
		Args.Add(TEXT("Category"), FText::FromString(InItem->GetCategory()));
		Args.Add(TEXT("DisplayTitle"), InItem->DisplayText);

		Tooltip = FText::Format(LOCTEXT("BlueprintResultSearchToolTip", "{Category} : {DisplayTitle}"), Args).ToString();

		return SNew( STableRow< TSharedPtr<FFindInBlueprintsResult> >, OwnerTable )
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				.AutoWidth()
				[
					InItem->CreateIcon()
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2,0)
				[
					SNew(STextBlock)
						.Text(InItem.Get(), &FFindInBlueprintsResult::GetDisplayString)
						.HighlightText(HighlightText)
						.ToolTipText(Tooltip)
				]
				+SHorizontalBox::Slot()
				.FillWidth(1)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(2,0)
				[
					SNew(STextBlock)
					.Text( CommentText )
					.ColorAndOpacity(FLinearColor::Yellow)
					.HighlightText(HighlightText)
				]
			];
	}
}

void SFindInBlueprints::OnGetChildren( FSearchResult InItem, TArray< FSearchResult >& OutChildren )
{
	OutChildren += InItem->Children;
}

void SFindInBlueprints::OnTreeSelectionDoubleClicked( FSearchResult Item )
{
	if(Item.IsValid())
	{
		Item->OnClick();
	}
}

TOptional<float> SFindInBlueprints::GetPercentCompleteSearch() const
{
	if(StreamSearch.IsValid())
	{
		return StreamSearch->GetPercentComplete();
	}
	return 0.0f;
}

EVisibility SFindInBlueprints::GetSearchbarVisiblity() const
{
	return StreamSearch.IsValid()? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedPtr<SWidget> SFindInBlueprints::OnContextMenuOpening()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection,BlueprintEditorPtr.Pin()->GetToolkitCommands());

	MenuBuilder.BeginSection("BasicOperations");
	{
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().SelectAll);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Copy);
	}

	return MenuBuilder.MakeWidget();
}

void SFindInBlueprints::SelectAllItemsHelper(FSearchResult InItemToSelect)
{
	// Iterates over all children and recursively selects all items in the results
	TreeView->SetItemSelection(InItemToSelect, true);

	for( const auto Child : InItemToSelect->Children )
	{
		SelectAllItemsHelper(Child);
	}
}

void SFindInBlueprints::OnSelectAllAction()
{
	for( const auto Item : ItemsFound )
	{
		SelectAllItemsHelper(Item);
	}
}

void SFindInBlueprints::OnCopyAction()
{
	TArray< FSearchResult > SelectedItems = TreeView->GetSelectedItems();

	FString SelectedText;

	for( const auto SelectedItem : SelectedItems)
	{
		// Add indents for each layer into the tree the item is
		for(auto ParentItem = SelectedItem->Parent; ParentItem.IsValid(); ParentItem = ParentItem.Pin()->Parent)
		{
			SelectedText += TEXT("\t");
		}

		// Add the display string
		SelectedText += SelectedItem->GetDisplayString();

		// If there is a comment, add two indents and then the comment
		FString CommentText = SelectedItem->GetCommentText();
		if(!CommentText.IsEmpty())
		{
			SelectedText += TEXT("\t\t") + CommentText;
		}
		
		// Line terminator so the next item will be on a new line
		SelectedText += LINE_TERMINATOR;
	}

	// Copy text to clipboard
	FPlatformMisc::ClipboardCopy( *SelectedText );
}

#undef LOCTEXT_NAMESPACE
