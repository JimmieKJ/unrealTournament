// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Engine/NiagaraScript.h"
#include "Engine/NiagaraConstants.h"
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


//////////////////////////////////////////////////////////////////////////
// UNiagraScriptSource

UNiagaraScriptSource::UNiagaraScriptSource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNiagaraScriptSource::PostLoad()
{
	Super::PostLoad();
	UNiagaraScript* ScriptOwner = Cast<UNiagaraScript>(GetOuter());
	if (ScriptOwner)
	{
		ScriptOwner->ConditionalPostLoad();
		//FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::Get().LoadModuleChecked<FNiagaraEditorModule>(TEXT("NiagaraEditor"));
		//NiagaraEditorModule.CompileScript(ScriptOwner);
		Compile();
	}

}


void UNiagaraScriptSource::Compile()
{
	UNiagaraScript* ScriptOwner = Cast<UNiagaraScript>(GetOuter());
	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::Get().LoadModuleChecked<FNiagaraEditorModule>(TEXT("NiagaraEditor"));
	NiagaraEditorModule.CompileScript(ScriptOwner);

	ExposedVectorConstants.Empty();

	// grab all constant nodes that are exposed to the editor
	TArray<UNiagaraNodeConstant*> ConstNodes;
	UpdateGraph->GetNodesOfClass<UNiagaraNodeConstant>(ConstNodes);
	for (UNiagaraNodeConstant *Node : ConstNodes)
	{
		if (Node->bExposeToEffectEditor)
		{
			EditorExposedVectorConstant *Const = new EditorExposedVectorConstant();
			Const->ConstName = Node->ConstName;
			ExposedVectorConstants.Add( MakeShareable(Const) );
		}
	}
}


void UNiagaraScriptSource::GetParticleAttributes(TArray<FName>& VectorOutputs)
{
	VectorOutputs.Empty();

	VectorOutputs.Add(FName(TEXT("Particle Position")));
	VectorOutputs.Add(FName(TEXT("Particle Velocity")));
	VectorOutputs.Add(FName(TEXT("Particle Color")));
	VectorOutputs.Add(FName(TEXT("Particle Rotation")));
	VectorOutputs.Add(FName(TEXT("Particle Age")));
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

