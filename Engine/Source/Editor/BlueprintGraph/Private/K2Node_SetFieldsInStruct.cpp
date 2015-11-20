// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "K2Node_SetFieldsInStruct.h"
#include "MakeStructHandler.h"
#include "CompilerResultsLog.h"
#include "KismetCompiler.h"
#include "Editor/PropertyEditor/Public/PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "K2Node_MakeStruct"

struct SetFieldsInStructHelper
{
	static const TCHAR* StructRefPinName()
	{
		return TEXT("StructRef");
	}

	static const TCHAR* StructOutPinName()
	{
		return TEXT("StructOut");
	}
};

class FKCHandler_SetFieldsInStruct : public FKCHandler_MakeStruct
{
public:
	FKCHandler_SetFieldsInStruct(FKismetCompilerContext& InCompilerContext) : FKCHandler_MakeStruct(InCompilerContext) {}

	virtual UEdGraphPin* FindStructPinChecked(UEdGraphNode* InNode) const override
	{
		check(CastChecked<UK2Node_SetFieldsInStruct>(InNode));
		UEdGraphPin* FoundPin = InNode->FindPinChecked(SetFieldsInStructHelper::StructRefPinName());
		check(EGPD_Input == FoundPin->Direction);
		return FoundPin;
	}
};

UK2Node_SetFieldsInStruct::UK2Node_SetFieldsInStruct(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bMadeAfterOverridePinRemoval(false)
{
}

void UK2Node_SetFieldsInStruct::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (Schema && StructType)
	{
		CreatePin(EGPD_Input, Schema->PC_Exec, TEXT(""), NULL, false, false, Schema->PN_Execute);
		CreatePin(EGPD_Output, Schema->PC_Exec, TEXT(""), NULL, false, false, Schema->PN_Then);

		CreatePin(EGPD_Input, Schema->PC_Struct, TEXT(""), StructType, false, true, SetFieldsInStructHelper::StructRefPinName());

		auto OutPin = CreatePin(EGPD_Output, Schema->PC_Struct, TEXT(""), StructType, false, true, SetFieldsInStructHelper::StructOutPinName());
		OutPin->PinToolTip = LOCTEXT("SetFieldsInStruct_OutPinTooltip", "Reference to the input struct").ToString();
		{
			FStructOnScope StructOnScope(Cast<UScriptStruct>(StructType));
			FSetFieldsInStructPinManager OptionalPinManager(StructOnScope.GetStructMemory());
			OptionalPinManager.RebuildPropertyList(ShowPinForProperties, StructType);
			OptionalPinManager.CreateVisiblePins(ShowPinForProperties, StructType, EGPD_Input, this);
		}
	}
}

FText UK2Node_SetFieldsInStruct::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (StructType == nullptr)
	{
		return LOCTEXT("SetFieldsInNullStructNodeTitle", "Set members in <unknown struct>");
	}
	else if (CachedNodeTitle.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("StructName"), FText::FromName(StructType->GetFName()));
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("SetFieldsInStructNodeTitle", "Set members in {StructName}"), Args), this);
	}
	return CachedNodeTitle;
}

FText UK2Node_SetFieldsInStruct::GetTooltipText() const
{
	if (StructType == nullptr)
	{
		return LOCTEXT("SetFieldsInStruct_NullTooltip", "Adds a node that modifies an '<unknown struct>'");
	}
	else if (CachedTooltip.IsOutOfDate(this))
	{
		// FText::Format() is slow, so we cache this to save on performance
		CachedTooltip.SetCachedText(FText::Format(
			LOCTEXT("SetFieldsInStruct_Tooltip", "Adds a node that modifies a '{0}'"),
			FText::FromName(StructType->GetFName())
		), this);
	}
	return CachedTooltip;
}

FName UK2Node_SetFieldsInStruct::GetPaletteIcon(FLinearColor& OutColor) const
{ 
	return UK2Node_Variable::GetPaletteIcon(OutColor);
}

void UK2Node_SetFieldsInStruct::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	UEdGraphPin* FoundPin = FindPin(SetFieldsInStructHelper::StructRefPinName());
	if (!ensure(FoundPin) || (FoundPin->LinkedTo.Num() <= 0))
	{
		FText ErrorMessage = LOCTEXT("SetStructFields_NoStructRefError", "The @@ pin must be connected to the struct that you wish to set.");
		MessageLog.Error(*ErrorMessage.ToString(), FoundPin);
	}

	if (!bMadeAfterOverridePinRemoval)
	{
		MessageLog.Warning(*NSLOCTEXT("K2Node", "OverridePinRemoval", "Override pins have been removed from @@, please verify the Blueprint works as expected! See tooltips for enabling pin visibility for more details. This warning will go away after you resave the asset!").ToString(), this);
	}
}

FNodeHandlingFunctor* UK2Node_SetFieldsInStruct::CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_SetFieldsInStruct(CompilerContext);
}

bool UK2Node_SetFieldsInStruct::ShowCustomPinActions(const UEdGraphPin* Pin, bool bIgnorePinsNum)
{
	const int32 MinimalPinsNum = 5;
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	const auto Node = Pin ? Cast<const UK2Node_SetFieldsInStruct>(Pin->GetOwningNodeUnchecked()) : NULL;
	return Node
		&& ((Node->Pins.Num() > MinimalPinsNum) || bIgnorePinsNum)
		&& (EGPD_Input == Pin->Direction)
		&& (Pin->PinName != SetFieldsInStructHelper::StructRefPinName())
		&& !Schema->IsMetaPin(*Pin);
}

void UK2Node_SetFieldsInStruct::RemoveFieldPins(const UEdGraphPin* Pin, EPinsToRemove Selection)
{
	if (ShowCustomPinActions(Pin, false) && (Pin->GetOwningNodeUnchecked() == this))
	{
		// Pretend that the action was done on the hidden parent pin if the pin is split
		while (Pin->ParentPin != nullptr)
		{
			Pin = Pin->ParentPin;
		}

		const bool bHideSelected = (Selection == EPinsToRemove::GivenPin);
		const bool bHideNotSelected = (Selection == EPinsToRemove::AllOtherPins);
		bool bWasChanged = false;
		for (FOptionalPinFromProperty& OptionalProperty : ShowPinForProperties)
		{
			const bool bSelected = (Pin->PinName == OptionalProperty.PropertyName.ToString());
			const bool bHide = (bSelected && bHideSelected) || (!bSelected && bHideNotSelected);
			if (OptionalProperty.bShowPin && bHide)
			{
				bWasChanged = true;
				OptionalProperty.bShowPin = false;
			}
		}

		if (bWasChanged)
		{
			ReconstructNode();
		}
	}
}

bool UK2Node_SetFieldsInStruct::AllPinsAreShown() const
{
	for (const FOptionalPinFromProperty& OptionalProperty : ShowPinForProperties)
	{
		if (!OptionalProperty.bShowPin)
		{
			return false;
		}
	}

	return true;
}

void UK2Node_SetFieldsInStruct::RestoreAllPins()
{
	bool bWasChanged = false;
	for (FOptionalPinFromProperty& OptionalProperty : ShowPinForProperties)
	{
		if (!OptionalProperty.bShowPin)
		{
			bWasChanged = true;
			OptionalProperty.bShowPin = true;
		}
	}

	if (bWasChanged)
	{
		ReconstructNode();
	}
}

void UK2Node_SetFieldsInStruct::FSetFieldsInStructPinManager::GetRecordDefaults(UProperty* TestProperty, FOptionalPinFromProperty& Record) const
{
	FMakeStructPinManager::GetRecordDefaults(TestProperty, Record);

	Record.bShowPin = false;
}

void UK2Node_SetFieldsInStruct::PostPlacedNewNode()
{
	// New nodes automatically have this set.
	bMadeAfterOverridePinRemoval = true;
}

void UK2Node_SetFieldsInStruct::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UEdGraphPin* OutPin = FindPin(SetFieldsInStructHelper::StructOutPinName());
	if (OutPin && OutPin->LinkedTo.Num())
	{
		UEdGraphPin* InPin = FindPin(SetFieldsInStructHelper::StructRefPinName());
		UEdGraphPin* SourcePin = (InPin && (1 == InPin->LinkedTo.Num())) ? InPin->LinkedTo[0] : nullptr;
		auto Schema = CompilerContext.GetSchema();
		const bool bCopied = SourcePin && Schema->MovePinLinks(*OutPin, *SourcePin, true).CanSafeConnect();
		
		if (!bCopied)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ExpansionError", "Cannot copy links from @@").ToString(), OutPin);
		}
	}
	Pins.Remove(OutPin);
}

void UK2Node_SetFieldsInStruct::Serialize(FArchive& Ar)
{
	UK2Node_StructOperation::Serialize(Ar);

	if (Ar.IsLoading() && !bMadeAfterOverridePinRemoval)
	{
		// Check if this node actually requires warning the user that functionality has changed.

		bMadeAfterOverridePinRemoval = true;
		FOptionalPinManager PinManager;

		// Have to check if this node is even in danger.
		for (TFieldIterator<UProperty> It(StructType, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			UProperty* TestProperty = *It;
			if (PinManager.CanTreatPropertyAsOptional(TestProperty))
			{
				bool bNegate = false;
				if (UProperty* OverrideProperty = PropertyCustomizationHelpers::GetEditConditionProperty(TestProperty, bNegate))
				{
					// We have confirmed that there is a property that uses an override variable to enable it, so set it to true.
					bMadeAfterOverridePinRemoval = false;
					break;
				}
			}
		}
	}
	else if (Ar.IsSaving() && !Ar.IsTransacting())
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this);

		if (Blueprint && !Blueprint->bBeingCompiled)
		{
			bMadeAfterOverridePinRemoval = true;
		}
	}
}

#undef LOCTEXT_NAMESPACE