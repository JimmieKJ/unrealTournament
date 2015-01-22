// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "BlueprintDragDropMenuItem.h"
#include "BlueprintActionMenuItem.h" // for PerformAction()
#include "BlueprintNodeSpawner.h"
#include "BlueprintDelegateNodeSpawner.h"
#include "BlueprintVariableNodeSpawner.h"
#include "BPDelegateDragDropAction.h"
#include "BPVariableDragDropAction.h"
#include "BlueprintEditor.h"		// for GetVarIconAndColor()
#include "ObjectEditorUtils.h"
#include "SlateColor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "BlueprintActionFilter.h"	// for FBlueprintActionContext
#include "EditorCategoryUtils.h"
#include "EditorStyleSettings.h"	// for bShowFriendlyNames

#define LOCTEXT_NAMESPACE "BlueprintDragDropMenuItem"
DEFINE_LOG_CATEGORY_STATIC(LogBlueprintDragDropMenuItem, Log, All);

//------------------------------------------------------------------------------
FBlueprintDragDropMenuItem::FBlueprintDragDropMenuItem(FBlueprintActionContext const& Context, UBlueprintNodeSpawner const* SampleAction, int32 MenuGrouping/* = 0*/)
{
	AppendAction(SampleAction);
	check(SampleAction != nullptr);
	Grouping  = MenuGrouping;

	UProperty const* SampleProperty = nullptr;
	if (UBlueprintDelegateNodeSpawner const* DelegateSpawner = Cast<UBlueprintDelegateNodeSpawner const>(SampleAction))
	{
		SampleProperty = DelegateSpawner->GetDelegateProperty();
	}
	else if (UBlueprintVariableNodeSpawner const* VariableSpawner = Cast<UBlueprintVariableNodeSpawner const>(SampleAction))
	{
		SampleProperty = VariableSpawner->GetVarProperty();
	}

	if (SampleProperty != nullptr)
	{
		bool const bShowFriendlyNames = GetDefault<UEditorStyleSettings>()->bShowFriendlyNames;
		MenuDescription = bShowFriendlyNames ? FText::FromString(UEditorEngine::GetFriendlyName(SampleProperty)) : FText::FromName(SampleProperty->GetFName());

		TooltipDescription = SampleProperty->GetToolTipText().ToString();
		Category = FObjectEditorUtils::GetCategory(SampleProperty);

		bool const bIsDelegateProperty = SampleProperty->IsA<UMulticastDelegateProperty>();
		if (bIsDelegateProperty && Category.IsEmpty())
		{
			Category = FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Delegates).ToString();
		}
		else if (!bIsDelegateProperty)
		{
			check(Context.Blueprints.Num() > 0);
			UBlueprint const* Blueprint = Context.Blueprints[0];
			UClass const*     BlueprintClass = (Blueprint->SkeletonGeneratedClass != nullptr) ? Blueprint->SkeletonGeneratedClass : Blueprint->ParentClass;

			UClass const* PropertyClass = SampleProperty->GetOwnerClass();
			checkSlow(PropertyClass != nullptr);
			bool const bIsMemberProperty = BlueprintClass->IsChildOf(PropertyClass);

			FText TextCategory;
			if (Category.IsEmpty())
			{
				TextCategory = FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Variables);
			}
			else if (bIsMemberProperty)
			{
				TextCategory = FEditorCategoryUtils::BuildCategoryString(FCommonEditorCategory::Variables, FText::FromString(Category));
			}
			
			if (!bIsMemberProperty)
			{
				TextCategory = FText::Format(LOCTEXT("NonMemberVarCategory", "Class|{0}|{1}"), PropertyClass->GetDisplayNameText(), TextCategory);
			}
			Category = TextCategory.ToString();
		}
	}
	else
	{
		UE_LOG(LogBlueprintDragDropMenuItem, Warning, TEXT("Unhandled (or invalid) spawner: '%s'"), *SampleAction->GetName());
	}
}

//------------------------------------------------------------------------------
UEdGraphNode* FBlueprintDragDropMenuItem::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, FVector2D const Location, bool bSelectNewNode/* = true*/)
{
	// we shouldn't get here (this should just be used for drag/drop ops), but just in case we do...
	FBlueprintActionMenuItem BlueprintActionItem(GetSampleAction());
	return BlueprintActionItem.PerformAction(ParentGraph, FromPin, Location, bSelectNewNode);
}

//------------------------------------------------------------------------------
UEdGraphNode* FBlueprintDragDropMenuItem::PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, FVector2D const Location, bool bSelectNewNode/* = true*/)
{
	UEdGraphPin* FromPin = nullptr;
	if (FromPins.Num() > 0)
	{
		FromPin = FromPins[0];
	}
	
	UEdGraphNode* SpawnedNode = PerformAction(ParentGraph, FromPin, Location, bSelectNewNode);
	// try auto-wiring the rest of the pins (if there are any)
	for (int32 PinIndex = 1; PinIndex < FromPins.Num(); ++PinIndex)
	{
		SpawnedNode->AutowireNewNode(FromPins[PinIndex]);
	}
	
	return SpawnedNode;
}

//------------------------------------------------------------------------------
void FBlueprintDragDropMenuItem::AddReferencedObjects(FReferenceCollector& Collector)
{
	FEdGraphSchemaAction::AddReferencedObjects(Collector);
	
	// these don't get saved to disk, but we want to make sure the objects don't
	// get GC'd while the action array is around
	for (UBlueprintNodeSpawner const* Action : ActionSet)
	{
		Collector.AddReferencedObject(Action);
	}
}

//------------------------------------------------------------------------------
void FBlueprintDragDropMenuItem::AppendAction(UBlueprintNodeSpawner const* Action)
{
	ActionSet.Add(Action);
}

//------------------------------------------------------------------------------
FSlateBrush const* FBlueprintDragDropMenuItem::GetMenuIcon(FSlateColor& ColorOut)
{
	FSlateBrush const* IconBrush = nullptr;
	ColorOut = FSlateColor(FLinearColor::White);

	UBlueprintNodeSpawner const* SampleAction = GetSampleAction();
	if (UBlueprintDelegateNodeSpawner const* DelegateSpawner = Cast<UBlueprintDelegateNodeSpawner const>(SampleAction))
	{
		IconBrush = FEditorStyle::GetBrush(TEXT("GraphEditor.Delegate_16x"));
	}
	else if (UBlueprintVariableNodeSpawner const* VariableSpawner = Cast<UBlueprintVariableNodeSpawner const>(SampleAction))
	{
		if (UProperty const* Property = VariableSpawner->GetVarProperty())
		{
			UStruct* const PropertyOwner = CastChecked<UStruct>(Property->GetOuterUField());
			IconBrush = FBlueprintEditor::GetVarIconAndColor(PropertyOwner, Property->GetFName(), ColorOut);
		}
	}
	return IconBrush;
}

//------------------------------------------------------------------------------
UBlueprintNodeSpawner const* FBlueprintDragDropMenuItem::GetSampleAction() const
{
	check(ActionSet.Num() > 0);
	return *ActionSet.CreateConstIterator();
}

//------------------------------------------------------------------------------
TSet<UBlueprintNodeSpawner const*> const& FBlueprintDragDropMenuItem::GetActionSet() const
{
	return ActionSet;
}

//------------------------------------------------------------------------------
TSharedPtr<FDragDropOperation> FBlueprintDragDropMenuItem::OnDragged(FNodeCreationAnalytic AnalyticsDelegate) const
{
	TSharedPtr<FDragDropOperation> DragDropAction = nullptr;

	UBlueprintNodeSpawner const* SampleAction = GetSampleAction();
	if (UBlueprintDelegateNodeSpawner const* DelegateSpawner = Cast<UBlueprintDelegateNodeSpawner const>(SampleAction))
	{
		if (UMulticastDelegateProperty const* Property = DelegateSpawner->GetDelegateProperty())
		{
			UStruct* const PropertyOwner = CastChecked<UStruct>(Property->GetOuterUField());
			TSharedRef<FDragDropOperation> DragDropOpRef = FKismetDelegateDragDropAction::New(Property->GetFName(), PropertyOwner, AnalyticsDelegate);
			DragDropAction = TSharedPtr<FDragDropOperation>(DragDropOpRef);
		}
	}
	else if (UBlueprintVariableNodeSpawner const* VariableSpawner = Cast<UBlueprintVariableNodeSpawner const>(SampleAction))
	{
		if (UProperty const* Property = VariableSpawner->GetVarProperty())
		{
			UStruct* const PropertyOwner = CastChecked<UStruct>(Property->GetOuterUField());
			TSharedRef<FDragDropOperation> DragDropOpRef = FKismetVariableDragDropAction::New(Property->GetFName(), PropertyOwner, AnalyticsDelegate);
			DragDropAction = TSharedPtr<FDragDropOperation>(DragDropOpRef);
		}
		// @TODO: handle local variables as well
	}
	else
	{
		UE_LOG(LogBlueprintDragDropMenuItem, Warning, TEXT("Unhandled spawner type: '%s'"), *SampleAction->GetClass()->GetName());
	}
	return DragDropAction;
}

#undef LOCTEXT_NAMESPACE
