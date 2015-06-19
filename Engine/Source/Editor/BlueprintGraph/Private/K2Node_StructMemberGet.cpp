// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "StructMemberNodeHandlers.h"

//////////////////////////////////////////////////////////////////////////
// UK2Node_StructMemberGet

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_StructMemberGet::UK2Node_StructMemberGet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_StructMemberGet::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if ((PropertyName == TEXT("bShowPin")))
	{
		GetSchema()->ReconstructNode(*this);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UK2Node_StructMemberGet::AllocateDefaultPins()
{
	//@TODO: Create a context pin

	// Display any currently visible optional pins
	{
		FStructOperationOptionalPinManager OptionalPinManager;
		OptionalPinManager.RebuildPropertyList(ShowPinForProperties, StructType);
		OptionalPinManager.CreateVisiblePins(ShowPinForProperties, StructType, EGPD_Output, this);
	}
}

void UK2Node_StructMemberGet::AllocatePinsForSingleMemberGet(FName MemberName)
{
	//@TODO: Create a context pin

	// Updater for subclasses that allow hiding pins
	struct FSingleVariablePinManager : public FOptionalPinManager
	{
		FName MatchName;

		FSingleVariablePinManager(FName InMatchName)
			: MatchName(InMatchName)
		{
		}

		// FOptionalPinsUpdater interface
		virtual void GetRecordDefaults(UProperty* TestProperty, FOptionalPinFromProperty& Record) const override
		{
			Record.bCanToggleVisibility = false;
			Record.bShowPin = TestProperty->GetFName() == MatchName;
		}
		// End of FOptionalPinsUpdater interface
	};


	// Display any currently visible optional pins
	{
		FSingleVariablePinManager PinManager(MemberName);
		PinManager.RebuildPropertyList(ShowPinForProperties, StructType);
		PinManager.CreateVisiblePins(ShowPinForProperties, StructType, EGPD_Output, this);
	}
}

FText UK2Node_StructMemberGet::GetTooltipText() const
{
	if (CachedTooltip.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("VariableName"), FText::FromString(GetVarNameString()));
		// FText::Format() is slow, so we cache this to save on performance
		CachedTooltip.SetCachedText(FText::Format(LOCTEXT("K2Node_StructMemberGet_Tooltip", "Get member variables of {VariableName}"), Args), this);
	}
	return CachedTooltip;
}

FText UK2Node_StructMemberGet::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (CachedNodeTitle.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("VariableName"), FText::FromString(GetVarNameString()));
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("GetMembersInVariable", "Get members in {VariableName}"), Args), this);
	}
	return CachedNodeTitle;
}

FNodeHandlingFunctor* UK2Node_StructMemberGet::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_StructMemberVariableGet(CompilerContext);
}

#undef LOCTEXT_NAMESPACE