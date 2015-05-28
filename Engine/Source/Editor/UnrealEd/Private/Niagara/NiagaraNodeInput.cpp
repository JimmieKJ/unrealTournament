// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "BlueprintGraphDefinitions.h"
#include "GraphEditorSettings.h"

UNiagaraNodeInput::UNiagaraNodeInput(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
, Input(NAME_None, ENiagaraDataType::Vector)
, FloatDefault(1.0f)
, VectorDefault(1.0f, 1.0f, 1.0f, 1.0f)
, MatrixDefault(FMatrix::Identity)
, bCanBeExposed(true)
, bExposeWhenConstant(true)
{
}

void UNiagaraNodeInput::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	ReallocatePins();
}

void UNiagaraNodeInput::ReallocatePins()
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	Modify();

	// Break any links to 'orphan' pins
	for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* Pin = Pins[PinIndex];
		TArray<class UEdGraphPin*>& LinkedToRef = Pin->LinkedTo;
		for (int32 LinkIdx = 0; LinkIdx < LinkedToRef.Num(); LinkIdx++)
		{
			UEdGraphPin* OtherPin = LinkedToRef[LinkIdx];
			// If we are linked to a pin that its owner doesn't know about, break that link
			if (!OtherPin->GetOwningNode()->Pins.Contains(OtherPin))
			{
				Pin->LinkedTo.Remove(OtherPin);
			}
		}
	}

	// Store the old Input and Output pins
	TArray<UEdGraphPin*> OldInputPins;
	TArray<UEdGraphPin*> OldOutputPins;
	GetInputPins(OldInputPins);
	GetOutputPins(OldOutputPins);

	// Move the existing pins to a saved array
	TArray<UEdGraphPin*> OldPins(Pins);
	Pins.Empty();


	switch (Input.Type)
	{
	case ENiagaraDataType::Scalar:
	{
		CreatePin(EGPD_Output, Schema->PC_Float, TEXT(""), NULL, false, false, Input.Name.ToString());
	}
		break;
	case ENiagaraDataType::Vector:
	{
		CreatePin(EGPD_Output, Schema->PC_Vector, TEXT(""), NULL, false, false, Input.Name.ToString());
	}
		break;
	case ENiagaraDataType::Matrix:
	{
		CreatePin(EGPD_Output, Schema->PC_Matrix, TEXT(""), NULL, false, false, Input.Name.ToString());
	}
		break;
	case ENiagaraDataType::Curve:
	{
		CreatePin(EGPD_Output, Schema->PC_Curve, TEXT(""), NULL, false, false, Input.Name.ToString());
	}
		break;
	};

	// Get new Input and Output pins
	TArray<UEdGraphPin*> NewInputPins;
	TArray<UEdGraphPin*> NewOutputPins;
	GetInputPins(NewInputPins);
	GetOutputPins(NewOutputPins);

	for (int32 PinIndex = 0; PinIndex < OldInputPins.Num(); PinIndex++)
	{
		if (PinIndex < NewInputPins.Num())
		{
			NewInputPins[PinIndex]->CopyPersistentDataFromOldPin(*OldInputPins[PinIndex]);
		}
	}

	for (int32 PinIndex = 0; PinIndex < OldOutputPins.Num(); PinIndex++)
	{
		if (PinIndex < NewOutputPins.Num())
		{
			NewOutputPins[PinIndex]->CopyPersistentDataFromOldPin(*OldOutputPins[PinIndex]);
		}
	}

	OldInputPins.Empty();
	OldOutputPins.Empty();

	// Throw away the original pins
	for (int32 OldPinIndex = 0; OldPinIndex < OldPins.Num(); ++OldPinIndex)
	{
		UEdGraphPin* OldPin = OldPins[OldPinIndex];
		OldPin->Modify();
		OldPin->BreakAllPinLinks();

#if 0
		UEdGraphNode::ReturnPinToPool(OldPin);
#else
		OldPin->Rename(NULL, GetTransientPackage(), REN_None);
		OldPin->RemoveFromRoot();
		OldPin->MarkPendingKill();
#endif
	}
	OldPins.Empty();

	GetGraph()->NotifyGraphChanged();
}

void UNiagaraNodeInput::AllocateDefaultPins()
{
	ReallocatePins();
}

FText UNiagaraNodeInput::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return Input.Name == NAME_None ? NSLOCTEXT("NiagaraNodeInput", "Input", "Input") : FText::FromName(Input.Name);
}

FLinearColor UNiagaraNodeInput::GetNodeTitleColor() const
{
	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
	if (IsConstant())
	{
		return Schema->IsSystemConstant(Input) ? 
			CastChecked<UEdGraphSchema_Niagara>(GetSchema())->NodeTitleColor_SystemConstant :
			CastChecked<UEdGraphSchema_Niagara>(GetSchema())->NodeTitleColor_Constant ;
	}
	else
	{
		return CastChecked<UEdGraphSchema_Niagara>(GetSchema())->NodeTitleColor_Attribute;
	}
}

void UNiagaraNodeInput::Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& Outputs)
{
	//Input nodes in scripts being used as functions by other scripts will actually never get this far.
	//The compiler will find them and replace them with direct links between the pins going in and the nodes connected to the corresponding input.

	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
	if (IsConstant())
	{
		if (IsExposedConstant() || Schema->IsSystemConstant(Input))
		{
			//treat this input as an external constant.
			switch (Input.Type)
			{
			case ENiagaraDataType::Scalar:	Outputs.Add(FNiagaraNodeResult(Compiler->GetExternalConstant(Input, FloatDefault), Pins[0]));	break;
			case ENiagaraDataType::Vector:	Outputs.Add(FNiagaraNodeResult(Compiler->GetExternalConstant(Input, VectorDefault), Pins[0]));	break;
			case ENiagaraDataType::Matrix:	Outputs.Add(FNiagaraNodeResult(Compiler->GetExternalConstant(Input, MatrixDefault), Pins[0]));	break;
			case ENiagaraDataType::Curve:	Outputs.Add(FNiagaraNodeResult(Compiler->GetExternalCurveConstant(Input), Pins[0]));	break;
			}
		}
		else
		{
			//treat this input as an external constant.
			switch (Input.Type)
			{
			case ENiagaraDataType::Scalar:	Outputs.Add(FNiagaraNodeResult(Compiler->GetInternalConstant(Input, FloatDefault), Pins[0]));	break;
			case ENiagaraDataType::Vector:	Outputs.Add(FNiagaraNodeResult(Compiler->GetInternalConstant(Input, VectorDefault), Pins[0]));	break;
			case ENiagaraDataType::Matrix:	Outputs.Add(FNiagaraNodeResult(Compiler->GetInternalConstant(Input, MatrixDefault), Pins[0]));	break;
			case ENiagaraDataType::Curve:	Outputs.Add(FNiagaraNodeResult(Compiler->GetExternalCurveConstant(Input), Pins[0]));	break;//Internal curve Constant?
			}
		}
	}
	else
	{
		//This input is an attribute.
		Outputs.Add(FNiagaraNodeResult(Compiler->GetAttribute(Input), Pins[0]));
	}
}

bool UNiagaraNodeInput::IsConstant()const
{
	//This input is a constant if it does not match an attribute for the current script.
	return GetNiagaraGraph()->GetAttributeIndex(Input) == INDEX_NONE;
}

bool UNiagaraNodeInput::IsExposedConstant()const
{
	return bExposeWhenConstant && IsConstant();
}