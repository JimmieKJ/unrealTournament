// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "INiagaraCompiler.h"


UDEPRECATED_NiagaraNodeGetAttr::UDEPRECATED_NiagaraNodeGetAttr(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDEPRECATED_NiagaraNodeGetAttr::AllocateDefaultPins()
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	CreatePin(EGPD_Output, Schema->PC_Vector, TEXT(""), NULL, false, false, AttrName.ToString());
}

FText UDEPRECATED_NiagaraNodeGetAttr::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Attribute"), FText::FromName(AttrName));
	return FText::Format(NSLOCTEXT("Niagara", "GetAttribute", "Get {Attribute}"), Args);
}

FLinearColor UDEPRECATED_NiagaraNodeGetAttr::GetNodeTitleColor() const
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

void UDEPRECATED_NiagaraNodeGetAttr::Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& Outputs)
{
	check(0);
	//UE_LOG(LogNiagara, Fatal, TEXT("DEPRECATED_UNiagaraNodeGetAttr is deprecated. All instance of it should have been replaced in UNiagaraGraph::PostLoad()"));

	return;

	//TODO REMOVE THIS NODE ENTIRELY
	//Outputs.Add(FNiagaraNodeResult(Compiler->GetAttribute(AttrName), Pins[0]));
}

