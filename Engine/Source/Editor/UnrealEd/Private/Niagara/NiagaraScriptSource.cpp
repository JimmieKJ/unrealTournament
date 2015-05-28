// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "NiagaraScript.h"
#include "NiagaraConstants.h"
#include "NiagaraEditorModule.h"

//////////////////////////////////////////////////////////////////////////
// NiagaraGraph

UNiagaraGraph::UNiagaraGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	Schema = UEdGraphSchema_Niagara::StaticClass();
}

class UNiagaraScriptSource* UNiagaraGraph::GetSource() const
{
	return CastChecked<UNiagaraScriptSource>(GetOuter());
}

UNiagaraNodeOutput* UNiagaraGraph::FindOutputNode() const
{
	for (UEdGraphNode* Node : Nodes)
	{
		if (UNiagaraNodeOutput* OutNode = Cast<UNiagaraNodeOutput>(Node))
		{
			return OutNode;
		}
	}
	check(0);
	return NULL;
}

void UNiagaraGraph::FindInputNodes(TArray<class UNiagaraNodeInput*>& OutInputNodes) const
{
	for (UEdGraphNode* Node : Nodes)
	{
		if (UNiagaraNodeInput* InNode = Cast<UNiagaraNodeInput>(Node))
		{
			OutInputNodes.Add(InNode);
		}
	}
}

int32 UNiagaraGraph::GetAttributeIndex(const FNiagaraVariableInfo& Attr)const
{
	const UNiagaraNodeOutput* OutNode = FindOutputNode();
	check(OutNode);
	for (int32 i = 0; i < OutNode->Outputs.Num(); ++i)
	{
		if (OutNode->Outputs[i] == Attr)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void UNiagaraGraph::GetAttributes(TArray< FNiagaraVariableInfo >& OutAttributes)const
{
	const UNiagaraNodeOutput* OutNode = FindOutputNode();
	check(OutNode);

	for (const FNiagaraVariableInfo& Attr : OutNode->Outputs)
	{
		check(!OutAttributes.Find(Attr));
		OutAttributes.Add(Attr);
	}
}

//////////////////////////////////////////////////////////////////////////
// UNiagraScriptSource

UNiagaraScriptSource::UNiagaraScriptSource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNiagaraScriptSource::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	UNiagaraScript* ScriptOwner = Cast<UNiagaraScript>(GetOuter());
	if (ScriptOwner && ScriptOwner->ByteCode.Num() == 0)
	{
		ScriptOwner->ConditionalPostLoad();
		//FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::Get().LoadModuleChecked<FNiagaraEditorModule>(TEXT("NiagaraEditor"));
		//NiagaraEditorModule.CompileScript(ScriptOwner);	
	}
	Compile();
#endif
}


void UNiagaraScriptSource::Compile()
{
	UNiagaraScript* ScriptOwner = Cast<UNiagaraScript>(GetOuter());
	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::Get().LoadModuleChecked<FNiagaraEditorModule>(TEXT("NiagaraEditor"));
	NiagaraEditorModule.CompileScript(ScriptOwner);

	FNiagaraConstants& ExternalConsts = ScriptOwner->ConstantData.ExternalConstants;

	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(NodeGraph->GetSchema());
	ExposedVectorConstants.Empty();
	for (int32 ConstIdx = 0; ConstIdx < ExternalConsts.GetNumVectorConstants(); ConstIdx++)
	{
		FNiagaraVariableInfo Info;
		FVector4 Value;
		ExternalConsts.GetVectorConstant(ConstIdx, Value, Info);
		if (Schema->IsSystemConstant(Info))
		{
			continue;//System constants are "external" but should not be exposed to the editor.
		}
			
		EditorExposedVectorConstant *Const = new EditorExposedVectorConstant();
		Const->ConstName = Info.Name;
		Const->Value = Value;
		ExposedVectorConstants.Add(MakeShareable(Const));
	}

	for (int32 ConstIdx = 0; ConstIdx < ExternalConsts.GetNumDataObjectConstants(); ConstIdx++)
	{
		EditorExposedVectorCurveConstant *Const = new EditorExposedVectorCurveConstant();
		FNiagaraVariableInfo Info;
		FNiagaraDataObject* Dummy;
		ExternalConsts.GetDataObjectConstant(ConstIdx, Dummy, Info);
		Const->ConstName = Info.Name;
		//Set default value for this too?
		ExposedVectorCurveConstants.Add(MakeShareable(Const));
	}
}

void UNiagaraScriptSource::GetEmitterAttributes(TArray<FName>& VectorInputs, TArray<FName>& MatrixInputs)
{
	for (uint32 i=0; i < NiagaraConstants::NumBuiltinConstants; i++)
	{
		VectorInputs.Add(NiagaraConstants::ConstantNames[i]);
	}

	MatrixInputs.Empty();
	MatrixInputs.Add(FName(TEXT("Emitter Transform")));
}

