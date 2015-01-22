// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "INiagaraCompiler.h"


UNiagaraNodeGetAttr::UNiagaraNodeGetAttr(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNiagaraNodeGetAttr::AllocateDefaultPins()
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	CreatePin(EGPD_Output, Schema->PC_Vector, TEXT(""), NULL, false, false, AttrName.ToString());
}

FText UNiagaraNodeGetAttr::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Attribute"), FText::FromName(AttrName));
	return FText::Format(NSLOCTEXT("Niagara", "GetAttribute", "Get {Attribute}"), Args);
}

FLinearColor UNiagaraNodeGetAttr::GetNodeTitleColor() const
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	if(Pins.Num() > 0 && Pins[0] != NULL)
	{
		return Schema->GetPinTypeColor(Pins[0]->PinType);
	}
	else
	{
		return Super::GetNodeTitleColor();
	}
}

void UNiagaraNodeGetAttr::Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& Outputs)
{
	Outputs.Add(FNiagaraNodeResult(Compiler->GetAttribute(AttrName), Pins[0]));
}

