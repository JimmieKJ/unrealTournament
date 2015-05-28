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
	FText ExtraSearchInfoText;

	FFormatNamedArguments Args;
	Args.Add(TEXT("Key"), FText::FromString(InKey));
	Args.Add(TEXT("Value"), InValue);
	ExtraSearchInfoText = FText::Format(LOCTEXT("ExtraSearchInfo", "{Key}: {Value}"), Args);

	TSharedPtr< FFindInBlueprintsResult > SearchResult(new FFindInBlueprintsResult(ExtraSearchInfoText, InParent) );
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

FText FFindInBlueprintsResult::GetCategory() const
{
	return FText::GetEmpty();
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
		UObject* Object = LoadObject<UObject>(NULL, *DisplayText.ToString(), NULL, 0, NULL);
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

FText FFindInBlueprintsResult::GetDisplayString() const
{
	return DisplayText;
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

FText FFindInBlueprintsGraphNode::GetCategory() const
{
	if(Class == UK2Node_CallFunction::StaticClass())
	{
		return LOCTEXT("CallFuctionCat", "Function Call");
	}
	else if(Class == UK2Node_MacroInstance::StaticClass())
	{
		return LOCTEXT("MacroCategory", "Macro");
	}
	else if(Class == UK2Node_Event::StaticClass())
	{
		return LOCTEXT("EventCat", "Event");
	}
	else if(Class == UK2Node_VariableGet::StaticClass())
	{
		return LOCTEXT("VariableGetCategory", "Variable Get");
	}
	else if(Class == UK2Node_VariableSet::StaticClass())
	{
		return LOCTEXT("VariableSetCategory", "Variable Set");
	}

	return LOCTEXT("NodeCategory", "Node");
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
		.ToolTipText(FText::FromString(FindInBlueprintsHelpers::GetPinTypeAsString(PinType)));
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

FText FFindInBlueprintsPin::GetCategory() const
{
	return LOCTEXT("PinCategory", "Pin");
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
		.ToolTipText( FText::FromString(FindInBlueprintsHelpers::GetPinTypeAsString(PinType)) );
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

FText FFindInBlueprintsProperty::GetCategory() const
{
	if(bIsSCSComponent)
	{
		return LOCTEXT("Component", "Component");
	}
	return LOCTEXT("Variable", "Variable");
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

FText FFindInBlueprintsGraph::GetCategory() const
{
	if(GraphType == GT_Function)
	{
		return LOCTEXT("FunctionGraphCategory", "Function");
	}
	else if(GraphType == GT_Macro)
	{
		return LOCTEXT("MacroGraphCategory", "Macro");
	}
	return LOCTEXT("GraphCategory", "Graph");
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

	virtual uint32 Run() override;

	virtual void Stop() override
	{
		StopTaskCounter.Increment();
	}

	virtual void Exit() override
	{

	}
	/** End FRunnable Interface */

	TArray<FSearchData> MakeExtractionBatch() const;

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
					.Text( LOCTEXT("SearchResults", "Searching...") )
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

void SFindInBlueprints::ConditionallyAddCacheBar()
{
	FFindInBlueprintSearchManager& FindInBlueprintManager = FFindInBlueprintSearchManager::Get();

	// Do not add a second cache bar and do not add it when there are no uncached Blueprints
	if(FindInBlueprintManager.GetNumberUncachedBlueprints() > 0 || FindInBlueprintManager.GetFailedToCacheCount() > 0)
	{
		if(MainVerticalBox.IsValid() && !CacheBarSlot.IsValid())
		{
			// Create a single string of all the Blueprint paths that failed to cache, on separate lines
			FString PackageList;
			TArray<FString> FailedToCacheList = FFindInBlueprintSearchManager::Get().GetFailedToCachePathList();
			for (FString Package : FailedToCacheList)
			{
				PackageList += Package + TEXT("\n");
			}

			// Lambda to put together the popup menu detailing the failed to cache paths
			auto OnDisplayCacheFailLambda = [](TWeakPtr<SWidget> InParentWidget, FString InPackageList)->FReply
			{
				if (InParentWidget.IsValid())
				{
					TSharedRef<SWidget> DisplayWidget = 
						SNew(SBox)
						.MaxDesiredHeight(512)
						.MaxDesiredWidth(512)
						.Content()
						[
							SNew(SBorder)
							.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
							[
								SNew(SScrollBox)
								+SScrollBox::Slot()
								[
									SNew(SMultiLineEditableText)
									.AutoWrapText(true)
									.IsReadOnly(true)
									.Text(FText::FromString(InPackageList))
								]
							]
						];

					FSlateApplication::Get().PushMenu(
						InParentWidget.Pin().ToSharedRef(),
						DisplayWidget,
						FSlateApplication::Get().GetCursorPos(),
						FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
						);	
				}
				return FReply::Handled();
			};

			MainVerticalBox.Pin()->AddSlot()
				.AutoHeight()
				[
					SAssignNew(CacheBarSlot, SBorder)
					.Visibility( this, &SFindInBlueprints::GetCachingBarVisibility )
					.BorderBackgroundColor( this, &SFindInBlueprints::GetCachingBarColor )
					.BorderImage( FCoreStyle::Get().GetBrush("ErrorReporting.Box") )
					.Padding( FMargin(3,1) )
					[
						SNew(SVerticalBox)

						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.VAlign(EVerticalAlignment::VAlign_Center)
							.AutoWidth()
							[
								SNew(STextBlock)
								.Text(this, &SFindInBlueprints::GetUncachedBlueprintWarningText)
								.ColorAndOpacity( FCoreStyle::Get().GetColor("ErrorReporting.ForegroundColor") )
							]

							// Cache All button
							+SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(EVerticalAlignment::VAlign_Center)
								.Padding(6.0f, 2.0f, 4.0f, 2.0f)
								[
									SNew(SButton)
									.Text(LOCTEXT("IndexAllBlueprints", "Index All"))
									.OnClicked( this, &SFindInBlueprints::OnCacheAllBlueprints )
									.Visibility( this, &SFindInBlueprints::GetCacheAllButtonVisibility )
									.ToolTip(IDocumentation::Get()->CreateToolTip(
									LOCTEXT("IndexAlLBlueprints_Tooltip", "Loads all non-indexed Blueprints and saves them with their search data. This can be a very slow process and the editor may become unresponsive."),
									NULL,
									TEXT("Shared/Editors/BlueprintEditor"),
									TEXT("FindInBlueprint_IndexAll")))
								]


							// View of failed Blueprint paths
							+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(4.0f, 2.0f, 0.0f, 2.0f)
								[
									SNew(SButton)
									.Text(LOCTEXT("ShowFailedPackages", "Show Failed Packages"))
									.OnClicked(FOnClicked::CreateLambda(OnDisplayCacheFailLambda, TWeakPtr<SWidget>(SharedThis(this)), PackageList))
									.Visibility( this, &SFindInBlueprints::GetFailedToCacheListVisibility )
									.ToolTip(IDocumentation::Get()->CreateToolTip(
									LOCTEXT("FailedCache_Tooltip", "Displays a list of packages that failed to save."),
									NULL,
									TEXT("Shared/Editors/BlueprintEditor"),
									TEXT("FindInBlueprint_FailedCache")))
								]

							// Cache progress bar
							+SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.Padding(4.0f, 2.0f, 4.0f, 2.0f)
								[
									SNew(SProgressBar)
									.Percent( this, &SFindInBlueprints::GetPercentCompleteCache )
									.Visibility( this, &SFindInBlueprints::GetCachingProgressBarVisiblity )
								]

							// Cancel button
							+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(4.0f, 2.0f, 0.0f, 2.0f)
								[
									SNew(SButton)
									.Text(LOCTEXT("CancelCacheAll", "Cancel"))
									.OnClicked( this, &SFindInBlueprints::OnCancelCacheAll )
									.Visibility( this, &SFindInBlueprints::GetCachingProgressBarVisiblity )
									.ToolTipText( LOCTEXT("CancelCacheAll_Tooltip", "Stops the caching process from where ever it is, can be started back up where it left off when needed.") )
								]

							// "X" to remove the bar
							+SHorizontalBox::Slot()
								.HAlign(HAlign_Right)
								[
									SNew(SButton)
									.ButtonStyle( FCoreStyle::Get(), "NoBorder" )
									.ContentPadding(0)
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Center)
									.OnClicked( this, &SFindInBlueprints::OnRemoveCacheBar )
									.ForegroundColor( FSlateColor::UseForeground() )
									[
										SNew(SImage)
										.Image( FCoreStyle::Get().GetBrush("EditableComboBox.Delete") )
										.ColorAndOpacity( FSlateColor::UseForeground() )
									]
								]
						]

						+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(8.0f, 0.0f, 0.0f, 2.0f)
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text(this, &SFindInBlueprints::GetCurrentCacheBlueprintName)
									.Visibility( this, &SFindInBlueprints::GetCachingBlueprintNameVisiblity )
									.ColorAndOpacity( FCoreStyle::Get().GetColor("ErrorReporting.ForegroundColor") )
								]

								+SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("FiBUnresponsiveEditorWarning", "NOTE: the editor may become unresponsive for some time!"))
									.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>( "SmallText" ))
								]
							]
					]
				];
		}
	}
	else
	{
		// Because there are no uncached Blueprints, remove the bar
		OnRemoveCacheBar();
	}
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

	FFindInBlueprintSearchManager::Get().CancelCacheAll(this);
}

EActiveTimerReturnType SFindInBlueprints::UpdateSearchResults( double InCurrentTime, float InDeltaTime )
{
	if ( StreamSearch.IsValid() )
	{
		bool bShouldShutdownThread = false;
		bShouldShutdownThread = StreamSearch->IsComplete();

		TArray<FSearchResult> BackgroundItemsFound;

		StreamSearch->GetFilteredItems( BackgroundItemsFound );
		if ( BackgroundItemsFound.Num() )
		{
			for ( auto& Item : BackgroundItemsFound )
			{
				Item->ExpandAllChildren( TreeView );
				ItemsFound.Add( Item );
			}
			TreeView->RequestTreeRefresh();
		}

		// If the thread is complete, shut it down properly
		if ( bShouldShutdownThread )
		{
			if ( ItemsFound.Num() == 0 )
			{
				// Insert a fake result to inform user if none found
				ItemsFound.Add( FSearchResult( new FFindInBlueprintsResult( LOCTEXT( "BlueprintSearchNoResults", "No Results found" ) ) ) );
				TreeView->RequestTreeRefresh();
			}

			// Add the cache bar if needed.
			ConditionallyAddCacheBar();

			StreamSearch->EnsureCompletion();
			StreamSearch.Reset();
		}
	}

	return StreamSearch.IsValid() ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
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
	if(SearchValue.Contains("\"") && SearchValue.ParseIntoArray(Tokens, TEXT("\""), true)>0)
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
		SearchValue.ParseIntoArray(Tokens, TEXT(" "), true);
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
	else
	{
		// If the stream search wasn't already running, register the active timer
		RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateSP( this, &SFindInBlueprints::UpdateSearchResults ) );
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
					.ToolTipText(LOCTEXT("BlueprintCatSearchToolTip", "Blueprint"))
				]
			];
	}
	else // Functions/Event/Pin widget
	{
		FText CommentText = FText::GetEmpty();

		if(!InItem->GetCommentText().IsEmpty())
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Comment"), FText::FromString(InItem->GetCommentText()));

			CommentText = FText::Format(LOCTEXT("NodeComment", "Node Comment:[{Comment}]"), Args);
		}

		FFormatNamedArguments Args;
		Args.Add(TEXT("Category"), InItem->GetCategory());
		Args.Add(TEXT("DisplayTitle"), InItem->DisplayText);

		FText Tooltip = FText::Format(LOCTEXT("BlueprintResultSearchToolTip", "{Category} : {DisplayTitle}"), Args);

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

FReply SFindInBlueprints::OnCacheAllBlueprints()
{
	if(!FFindInBlueprintSearchManager::Get().IsCacheInProgress())
	{
		// Request from the SearchManager a delegate to use for ticking the cache system.
		FWidgetActiveTimerDelegate WidgetActiveTimer;
		FFindInBlueprintSearchManager::Get().CacheAllUncachedBlueprints(SharedThis(this), WidgetActiveTimer);
		RegisterActiveTimer(0.f, WidgetActiveTimer);
	}

	return FReply::Handled();
}

FReply SFindInBlueprints::OnCancelCacheAll()
{
	FFindInBlueprintSearchManager::Get().CancelCacheAll(this);

	// Resubmit the last search
	OnSearchTextCommitted(SearchTextField->GetText(), ETextCommit::OnEnter);

	return FReply::Handled();
}

int32 SFindInBlueprints::GetCurrentCacheIndex() const
{
	return FFindInBlueprintSearchManager::Get().GetCurrentCacheIndex();
}

TOptional<float> SFindInBlueprints::GetPercentCompleteCache() const
{
	return FFindInBlueprintSearchManager::Get().GetCacheProgress();
}

EVisibility SFindInBlueprints::GetCachingProgressBarVisiblity() const
{
	return IsCacheInProgress()? EVisibility::Visible : EVisibility::Hidden;
}

EVisibility SFindInBlueprints::GetCacheAllButtonVisibility() const
{
	return IsCacheInProgress()? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SFindInBlueprints::GetCachingBarVisibility() const
{
	FFindInBlueprintSearchManager& FindInBlueprintManager = FFindInBlueprintSearchManager::Get();
	return (FindInBlueprintManager.GetNumberUncachedBlueprints() > 0 || FindInBlueprintManager.GetFailedToCacheCount())? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SFindInBlueprints::GetCachingBlueprintNameVisiblity() const
{
	return IsCacheInProgress()? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SFindInBlueprints::GetFailedToCacheListVisibility() const
{
	return FFindInBlueprintSearchManager::Get().GetFailedToCacheCount()? EVisibility::Visible : EVisibility::Collapsed;
}

bool SFindInBlueprints::IsCacheInProgress() const
{
	return FFindInBlueprintSearchManager::Get().IsCacheInProgress();
}

FSlateColor SFindInBlueprints::GetCachingBarColor() const
{
	// The caching bar's default color is a darkish red
	FSlateColor ReturnColor = FSlateColor(FLinearColor(0.4f, 0.0f, 0.0f));
	if(IsCacheInProgress())
	{
		// It turns yellow when in progress
		ReturnColor = FSlateColor(FLinearColor(0.4f, 0.4f, 0.0f));
	}
	return ReturnColor;
}

FText SFindInBlueprints::GetUncachedBlueprintWarningText() const
{
	FFindInBlueprintSearchManager& FindInBlueprintManager = FFindInBlueprintSearchManager::Get();

	int32 FailedToCacheCount = FindInBlueprintManager.GetFailedToCacheCount();

	// The number of unindexed Blueprints is the total of those that failed to cache and those that haven't been attempted yet.
	FFormatNamedArguments Args;
	Args.Add(TEXT("Count"), FindInBlueprintManager.GetNumberUncachedBlueprints() + FailedToCacheCount);

	FText ReturnDisplayText;
	if(IsCacheInProgress())
	{
		Args.Add(TEXT("CurrentIndex"), FindInBlueprintManager.GetCurrentCacheIndex());

		ReturnDisplayText = FText::Format(LOCTEXT("CachingBlueprints", "Indexing Blueprints... {CurrentIndex}/{Count}"), Args);
	}
	else
	{
		ReturnDisplayText = FText::Format(LOCTEXT("UncachedBlueprints", "Search incomplete. {Count} Blueprints are not indexed!"), Args);

		if (FailedToCacheCount > 0)
		{
			FFormatNamedArguments ArgsWithCacheFails;
			Args.Add(TEXT("BaseMessage"), ReturnDisplayText);
			Args.Add(TEXT("CacheFails"), FailedToCacheCount);
			ReturnDisplayText = FText::Format(LOCTEXT("UncachedBlueprintsWithCacheFails", "{BaseMessage} {CacheFails} Blueprints failed to cache."), Args);
		}
	}

	return ReturnDisplayText;
}

FText SFindInBlueprints::GetCurrentCacheBlueprintName() const
{
	return FText::FromString(FFindInBlueprintSearchManager::Get().GetCurrentCacheBlueprintName());
}

void SFindInBlueprints::OnCacheComplete()
{
	// Resubmit the last search, which will also remove the bar if needed
	OnSearchTextCommitted(SearchTextField->GetText(), ETextCommit::OnEnter);
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
		SelectedText += SelectedItem->GetDisplayString().ToString();

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

////////////////////////////////////
// FExtractionTask

/**
 * Task to search a batch of blueprints
 *
 * Each small batch is non-abandonable, but if the search is cancelled, no more batches are queued
 */
class FExtractionTask : public FNonAbandonableTask
{
public:
	/** Async task function */
	void DoWork();

	/** Batch of blueprints and associated data to search */
	TArray<FSearchData> QueryResultBatch;

	/** Pointer to array of tokens to search for */
	const TArray<FString>* Tokens;

	/** Function to call once the search result is ready */
	TFunction<void(const FSearchResult&)> OnResultReady;

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FExtractionTask, STATGROUP_ThreadPoolAsyncTasks );
	}
};

TArray<FSearchData> FStreamSearch::MakeExtractionBatch() const
{
	// Batch sizes between 10 and 30 seem fastest
	static const int BatchSize = 15;

	TArray<FSearchData> Batch;
	do
	{
		if (StopTaskCounter.GetValue())
			return TArray<FSearchData>();

		FSearchData QueryResult;
		if (!FFindInBlueprintSearchManager::Get().ContinueSearchQuery(this, QueryResult))
			return Batch;

		Batch.Push(QueryResult);
	}
	while (Batch.Num() <= BatchSize);

	return Batch;
}

uint32 FStreamSearch::Run()
{
	FFindInBlueprintSearchManager::Get().BeginSearchQuery(this);

	// Seems to go fastest at or slightly above one thread per physical core
	TArray<FAsyncTask<FExtractionTask>> ExtractionTasks;
	ExtractionTasks.SetNum(FPlatformMisc::NumberOfCores());

	TFunction<void(const FSearchResult&)> OnResultReady = [this](const FSearchResult& Result) {
		FScopeLock ScopeLock(&SearchCriticalSection);
		ItemsFound.Add(Result);
	};

	// Searching comes to an end if it is requested using the StopTaskCounter or continuing the search query yields no results
	TArray<FSearchData> QueryResultBatch;
	for (;;)
	{
		if (QueryResultBatch.Num() == 0)
		{
			QueryResultBatch = MakeExtractionBatch();
		}
		else
		{
			// Spinning until a task becomes available
			FPlatformProcess::Sleep(0);
		}

		if (QueryResultBatch.Num() == 0)
		{
			// All search work has been dispatched
			break;
		}

		for (auto& Task: ExtractionTasks)
		{
			if (Task.IsWorkDone())
			{
				Task.EnsureCompletion();

				auto& worker = Task.GetTask();
				worker.QueryResultBatch = MoveTemp(QueryResultBatch);
				worker.Tokens = &SearchTokens;
				worker.OnResultReady = OnResultReady;

				Task.StartBackgroundTask();

				// Ensure batch array is empty (probably already is due to move above)
				QueryResultBatch.Reset();
				break;
			}
		}
	}

	// Wait for all work to finish
	for (auto& Task: ExtractionTasks)
	{
		Task.EnsureCompletion();
	}

	if (StopTaskCounter.GetValue())
	{
		// Ensure that the FiB Manager knows that we are done searching
		FFindInBlueprintSearchManager::Get().EnsureSearchQueryEnds(this);
	}

	bThreadCompleted = true;

	return 0;
}

void FExtractionTask::DoWork()
{
	for (const auto& QueryResult: QueryResultBatch)
	{
		FSearchResult BlueprintCategory = FSearchResult(new FFindInBlueprintsResult(FText::FromString(QueryResult.BlueprintPath)));
		FindInBlueprintsHelpers::Extract(QueryResult.Value, *Tokens, BlueprintCategory);

		// If there are children, add the item to the search results
		if(BlueprintCategory->Children.Num() != 0)
		{
			OnResultReady(BlueprintCategory);
		}
	}
}

#undef LOCTEXT_NAMESPACE
