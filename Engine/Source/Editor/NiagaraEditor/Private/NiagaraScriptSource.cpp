// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "NiagaraScript.h"
#include "NiagaraConstants.h"
#include "NiagaraEditorModule.h"
#include "ComponentReregisterContext.h"
#include "NiagaraComponent.h"
#include "NiagaraSimulation.h"
#include "NiagaraEffect.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeWriteDataSet.h"
#include "NiagaraNodeReadDataSet.h"

#include "EdGraphSchema_Niagara.h"

//////////////////////////////////////////////////////////////////////////
// NiagaraGraph

UNiagaraGraph::UNiagaraGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	Schema = UEdGraphSchema_Niagara::StaticClass();
}

void UNiagaraGraph::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	NotifyGraphChanged();
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

void UNiagaraGraph::FindReadDataSetNodes(TArray<class UNiagaraNodeReadDataSet*>& OutReadNodes) const
{
	for (UEdGraphNode* Node : Nodes)
	{
		if (UNiagaraNodeReadDataSet* InNode = Cast<UNiagaraNodeReadDataSet>(Node))
		{
			OutReadNodes.Add(InNode);
		}
	}
}

void UNiagaraGraph::FindWriteDataSetNodes(TArray<class UNiagaraNodeWriteDataSet*>& OutWriteNodes) const
{
	for (UEdGraphNode* Node : Nodes)
	{
		if (UNiagaraNodeWriteDataSet* InNode = Cast<UNiagaraNodeWriteDataSet>(Node))
		{
			OutWriteNodes.Add(InNode);
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

struct FNiagaraComponentReregisterContext : FComponentReregisterContext
{
	FNiagaraEmitterScriptProperties* ScriptProps;
	UNiagaraEmitterProperties* EmitterProps;
	FNiagaraComponentReregisterContext(UNiagaraComponent* Comp, FNiagaraEmitterScriptProperties* InScriptProps, UNiagaraEmitterProperties* InEmitterProps)
		: FComponentReregisterContext(Comp)
		, ScriptProps(InScriptProps)
		, EmitterProps(InEmitterProps)
	{

	}
	~FNiagaraComponentReregisterContext()
	{
		ScriptProps->Init(EmitterProps);
	}
};

class FNiagaraScriptCompileContext
{
public:
	/** Initialization constructor. */
	FNiagaraScriptCompileContext(UNiagaraScript* Script)
	{
		// wait until resources are released
		FlushRenderingCommands();

		// Reregister all components usimg Script.
		for (TObjectIterator<UNiagaraComponent> ComponentIt; ComponentIt; ++ComponentIt)
		{
			UNiagaraComponent* Comp = *ComponentIt;
			TSharedPtr<FNiagaraEffectInstance> Inst = Comp->GetEffectInstance();
			if (Inst.IsValid())
			{
				TArray<TSharedPtr<FNiagaraSimulation>>& Emitters = Inst->GetEmitters();
				for (TSharedPtr<FNiagaraSimulation> Sim : Emitters)
				{
					if (Sim.IsValid())
					{
						if (UNiagaraEmitterProperties* Props = Sim->GetProperties().Get())
						{
							if (Props->UpdateScriptProps.Script == Script)
							{
								new(ComponentContexts)FNiagaraComponentReregisterContext(Comp, &Props->UpdateScriptProps, Props);
							}
							else if (Props->SpawnScriptProps.Script == Script)
							{
								new(ComponentContexts)FNiagaraComponentReregisterContext(Comp, &Props->SpawnScriptProps, Props);
							}
						}						
					}
				}
			}
		}
	}
	
private:
	/** The recreate contexts for the individual components. */
	TIndirectArray<FNiagaraComponentReregisterContext> ComponentContexts;
};

void UNiagaraScriptSource::Compile()
{
	UNiagaraScript* ScriptOwner = Cast<UNiagaraScript>(GetOuter());
	
	FNiagaraScriptCompileContext ScriptCompileContext(ScriptOwner);

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::Get().LoadModuleChecked<FNiagaraEditorModule>(TEXT("NiagaraEditor"));
	NiagaraEditorModule.CompileScript(ScriptOwner);

	FNiagaraConstants& ExternalConsts = ScriptOwner->ConstantData.GetExternalConstants();

	//Build the constant list. 
	//This is mainly just jumping through some hoops for the custom UI. Should be removed and have the UI just read directly from the constants stored in the UScript.
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

}

void UNiagaraScriptSource::GetEmitterAttributes(TArray<FName>& VectorInputs, TArray<FName>& MatrixInputs)
{
	for (uint32 i=0; i < NiagaraConstants::NumBuiltinConstants; i++)
	{
		VectorInputs.Add(NiagaraConstants::GConstantNames[i]);
	}

	MatrixInputs.Empty();
	MatrixInputs.Add(FName(TEXT("Emitter Transform")));
}

