// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"

#include "GraphEditorActions.h"
#include "ScopedTransaction.h"
#include "CompilerResultsLog.h"
#include "AnimGraphNode_BlendListByEnum.h"
#include "AnimationGraphSchema.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"

#define LOCTEXT_NAMESPACE "BlendListByEnum"

/////////////////////////////////////////////////////
// UAnimGraphNode_BlendListByEnum

UAnimGraphNode_BlendListByEnum::UAnimGraphNode_BlendListByEnum(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FString UAnimGraphNode_BlendListByEnum::GetNodeCategory() const
{
	return FString::Printf(TEXT("%s, Blend List by enum"), *Super::GetNodeCategory() );
}

FText UAnimGraphNode_BlendListByEnum::GetTooltipText() const
{
	// FText::Format() is slow, so we utilize the cached list title
	return GetNodeTitle(ENodeTitleType::ListView);
}

FText UAnimGraphNode_BlendListByEnum::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (BoundEnum == nullptr)
	{
		return LOCTEXT("AnimGraphNode_BlendListByEnum_TitleError", "ERROR: Blend Poses (by missing enum)");
	}
	// @TODO: don't know enough about this node type to comfortably assert that
	//        the BoundEnum won't change after the node has spawned... until
	//        then, we'll leave this optimization off
	else //if (CachedNodeTitle.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("EnumName"), FText::FromString(BoundEnum->GetName()));
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("AnimGraphNode_BlendListByEnum_Title", "Blend Poses ({EnumName})"), Args), this);
	}
	return CachedNodeTitle;
}

void UAnimGraphNode_BlendListByEnum::PostPlacedNewNode()
{
	// Make sure we start out with a pin
	Node.AddPose();
}

void UAnimGraphNode_BlendListByEnum::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	auto CustomizeBlendListEnumNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, UEnum* EnumValue)
	{
		UAnimGraphNode_BlendListByEnum* BlendListEnumNode = CastChecked<UAnimGraphNode_BlendListByEnum>(NewNode);
		BlendListEnumNode->BoundEnum = EnumValue;
	};

	// add all blendlist enum entries
	for (TObjectIterator<UEnum> EnumIt; EnumIt; ++EnumIt)
	{
		UEnum* CurrentEnum = *EnumIt;

		// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
		// check to make sure that the registrar is looking for actions of this type
		// (could be regenerating actions for a specific asset, and therefore the 
		// registrar would only accept actions corresponding to that asset)
		if (!ActionRegistrar.IsOpenForRegistration(CurrentEnum))
		{
			continue;
		}

		const bool bIsBlueprintType = UEdGraphSchema_K2::IsAllowableBlueprintVariableType(CurrentEnum);
		if (bIsBlueprintType)
		{
			UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
			check(NodeSpawner != nullptr);

			NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeBlendListEnumNodeLambda, CurrentEnum);

			ActionRegistrar.AddBlueprintAction(CurrentEnum, NodeSpawner);
		}
	}
}

void UAnimGraphNode_BlendListByEnum::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	if (!Context.bIsDebugging && (BoundEnum != NULL))
	{
		if ((Context.Pin != NULL) && (Context.Pin->Direction == EGPD_Input))
		{
			int32 RawArrayIndex = 0;
			bool bIsPosePin = false;
			bool bIsTimePin = false;
			GetPinInformation(Context.Pin->PinName, /*out*/ RawArrayIndex, /*out*/ bIsPosePin, /*out*/ bIsTimePin);

			if (bIsPosePin || bIsTimePin)
			{
				const int32 ExposedEnumIndex = RawArrayIndex - 1;

				if (ExposedEnumIndex != INDEX_NONE)
				{
					// Offer to remove this specific pin
					FUIAction Action = FUIAction( FExecuteAction::CreateUObject( this, &UAnimGraphNode_BlendListByEnum::RemovePinFromBlendList, const_cast<UEdGraphPin*>(Context.Pin)) );
					Context.MenuBuilder->AddMenuEntry( LOCTEXT("RemovePose", "Remove Pose"), FText::GetEmpty(), FSlateIcon(), Action );
				}
			}
		}

		// Offer to add any not-currently-visible pins
		bool bAddedHeader = false;
		const int32 MaxIndex = BoundEnum->NumEnums() - 1; // we don't want to show _MAX enum
		for (int32 Index = 0; Index < MaxIndex; ++Index)
		{
			FName ElementName = BoundEnum->GetEnum(Index);
			if (!VisibleEnumEntries.Contains(ElementName))
			{
				FText PrettyElementName = BoundEnum->GetEnumText(Index);

				// Offer to add this entry
				if (!bAddedHeader)
				{
					bAddedHeader = true;
					Context.MenuBuilder->BeginSection("AnimGraphNodeAddElementPin", LOCTEXT("ExposeHeader", "Add pin for element"));
					{
						FUIAction Action = FUIAction( FExecuteAction::CreateUObject( this, &UAnimGraphNode_BlendListByEnum::ExposeEnumElementAsPin, ElementName) );
						Context.MenuBuilder->AddMenuEntry(PrettyElementName, PrettyElementName, FSlateIcon(), Action);
					}
					Context.MenuBuilder->EndSection();
				}
				else
				{
					FUIAction Action = FUIAction( FExecuteAction::CreateUObject( this, &UAnimGraphNode_BlendListByEnum::ExposeEnumElementAsPin, ElementName) );
					Context.MenuBuilder->AddMenuEntry(PrettyElementName, PrettyElementName, FSlateIcon(), Action);
				}
			}
		}
	}
}

void UAnimGraphNode_BlendListByEnum::ExposeEnumElementAsPin(FName EnumElementName)
{
	if (!VisibleEnumEntries.Contains(EnumElementName))
	{
		FScopedTransaction Transaction( LOCTEXT("ExposeElement", "ExposeElement") );
		Modify();

		VisibleEnumEntries.Add(EnumElementName);

		Node.AddPose();
	
		ReconstructNode();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}
}

void UAnimGraphNode_BlendListByEnum::RemovePinFromBlendList(UEdGraphPin* Pin)
{
	int32 RawArrayIndex = 0;
	bool bIsPosePin = false;
	bool bIsTimePin = false;
	GetPinInformation(Pin->PinName, /*out*/ RawArrayIndex, /*out*/ bIsPosePin, /*out*/ bIsTimePin);

	const int32 ExposedEnumIndex = (bIsPosePin || bIsTimePin) ? (RawArrayIndex - 1) : INDEX_NONE;

	if (ExposedEnumIndex != INDEX_NONE)
	{
		FScopedTransaction Transaction( LOCTEXT("RemovePin", "RemovePin") );
		Modify();

		// Record it as no longer exposed
		VisibleEnumEntries.RemoveAt(ExposedEnumIndex);

		// Remove the pose from the node
		UProperty* AssociatedProperty;
		int32 ArrayIndex;
		GetPinAssociatedProperty(GetFNodeType(), Pin, /*out*/ AssociatedProperty, /*out*/ ArrayIndex);

		ensure(ArrayIndex == (ExposedEnumIndex + 1));

		// setting up removed pins info 
		RemovedPinArrayIndex = ArrayIndex;
		Node.RemovePose(ArrayIndex);
		ReconstructNode();
		//@TODO: Just want to invalidate the visual representation currently
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}
}

void UAnimGraphNode_BlendListByEnum::GetPinInformation(const FString& InPinName, int32& Out_PinIndex, bool& Out_bIsPosePin, bool& Out_bIsTimePin)
{
	const int32 UnderscoreIndex = InPinName.Find(TEXT("_"));
	if (UnderscoreIndex != INDEX_NONE)
	{
		const FString ArrayName = InPinName.Left(UnderscoreIndex);
		Out_PinIndex = FCString::Atoi(*(InPinName.Mid(UnderscoreIndex + 1)));

		Out_bIsPosePin = ArrayName == TEXT("BlendPose");
		Out_bIsTimePin = ArrayName == TEXT("BlendTime");
	}
	else
	{
		Out_bIsPosePin = false;
		Out_bIsTimePin = false;
		Out_PinIndex = INDEX_NONE;
	}
}

void UAnimGraphNode_BlendListByEnum::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const
{
	// if pin name starts with BlendPose or BlendWeight, change to enum name 
	bool bIsPosePin;
	bool bIsTimePin;
	int32 RawArrayIndex;
	GetPinInformation(Pin->PinName, /*out*/ RawArrayIndex, /*out*/ bIsPosePin, /*out*/ bIsTimePin);
	checkSlow(RawArrayIndex == ArrayIndex);

	if (bIsPosePin || bIsTimePin)
	{
		if (RawArrayIndex > 0)
		{
			const int32 ExposedEnumPinIndex = RawArrayIndex - 1;

			// find pose index and see if it's mapped already or not
			if (VisibleEnumEntries.IsValidIndex(ExposedEnumPinIndex) && (BoundEnum != NULL))
			{
				const FName& EnumElementName = VisibleEnumEntries[ExposedEnumPinIndex];
				const int32 EnumIndex = BoundEnum->FindEnumIndex(EnumElementName);
				if (EnumIndex != INDEX_NONE)
				{
					Pin->PinFriendlyName = BoundEnum->GetEnumText(EnumIndex);
				}
				else
				{
					Pin->PinFriendlyName = FText::FromName(EnumElementName);
				}
			}
			else
			{
				Pin->PinFriendlyName = LOCTEXT("InvalidIndex", "Invalid index");
			}
		}
		else if (ensure(RawArrayIndex == 0))
		{
			Pin->PinFriendlyName = LOCTEXT("Default", "Default");
		}

		// Append the pin type
		if (bIsPosePin)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("PinFriendlyName"), Pin->PinFriendlyName);
			Pin->PinFriendlyName = FText::Format(LOCTEXT("FriendlyNamePose", "{PinFriendlyName} Pose"), Args);
		}

		if (bIsTimePin)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("PinFriendlyName"), Pin->PinFriendlyName);
			Pin->PinFriendlyName = FText::Format(LOCTEXT("FriendlyNameBlendTime", "{PinFriendlyName} Blend Time"), Args);
		}
	}
}

void UAnimGraphNode_BlendListByEnum::ValidateAnimNodeDuringCompilation(class USkeleton* ForSkeleton, class FCompilerResultsLog& MessageLog)
{
	if (BoundEnum == NULL)
	{
		MessageLog.Error(TEXT("@@ references an unknown enum; please delete the node and recreate it"), this);
	}
}

void UAnimGraphNode_BlendListByEnum::PreloadRequiredAssets()
{
	PreloadObject(BoundEnum);

	Super::PreloadRequiredAssets();
}

void UAnimGraphNode_BlendListByEnum::BakeDataDuringCompilation(class FCompilerResultsLog& MessageLog)
{
	if (BoundEnum != NULL)
	{
		PreloadObject(BoundEnum);

		// Zero the array out so it looks up the default value, and stat counting at index 1
		Node.EnumToPoseIndex.Empty();
		Node.EnumToPoseIndex.AddZeroed(BoundEnum->NumEnums());
		int32 PinIndex = 1;

		// Run thru the enum entries
		for (auto ExposedIt = VisibleEnumEntries.CreateConstIterator(); ExposedIt; ++ExposedIt)
		{
			const FName& EnumElementName = *ExposedIt;
			const int32 EnumIndex = BoundEnum->FindEnumIndex(EnumElementName);

			if (EnumIndex != INDEX_NONE)
			{
				Node.EnumToPoseIndex[EnumIndex] = PinIndex;
			}
			else
			{
				MessageLog.Error(*FString::Printf(TEXT("@@ references an unknown enum entry %s"), *EnumElementName.ToString()), this);
			}

			++PinIndex;
		}
	}
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
