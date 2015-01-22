// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Engine/NiagaraScript.h"
#include "NiagaraEditorModule.h"

#define LOCTEXT_NAMESPACE "NiagaraScriptFactory"

UNiagaraScriptFactoryNew::UNiagaraScriptFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	SupportedClass = UNiagaraScript::StaticClass();
	bCreateNew = false;
	bEditAfterNew = true;

	// Check ini to see if we should enable creation
	bool bEnableNiagara = false;
	GConfig->GetBool(TEXT("Niagara"), TEXT("EnableNiagara"), bEnableNiagara, GEngineIni);
	bCreateNew = bEnableNiagara;
}

UObject* UNiagaraScriptFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UNiagaraScript::StaticClass()));

	// First allocate runtime script 
	UNiagaraScript* NewScript = ConstructObject<UNiagaraScript>(Class, InParent, Name, Flags);
	if(NewScript != NULL)
	{
		// Then allocate editor-only 'source' object
		UNiagaraScriptSource* Source = ConstructObject<UNiagaraScriptSource>(UNiagaraScriptSource::StaticClass(), NewScript, NAME_None, RF_Transactional);
		if(Source)
		{
			// Create 'update graph'
			UNiagaraGraph* CreatedGraph  = ConstructObject<UNiagaraGraph>(UNiagaraGraph::StaticClass(), Source, NAME_None, RF_Transactional);
			Source->UpdateGraph = CreatedGraph;

			FGraphNodeCreator<UNiagaraNodeOutputUpdate> OutputNodeCreator(*CreatedGraph);
			UNiagaraNodeOutputUpdate* OutputNode = OutputNodeCreator.CreateNode();
			OutputNodeCreator.Finalize();

// 			FGraphNodeCreator<UNiagaraNodeGetAttr> TimeNodeCreator(*CreatedGraph);
// 			UNiagaraNodeGetAttr* DeltaTimeNode = TimeNodeCreator.CreateNode();
// 			DeltaTimeNode->AttrName = FName(TEXT("DeltaTime"));
// 			DeltaTimeNode->bIsEmitterAttribute = true;
// 			TimeNodeCreator.Finalize();
			//UEdGraphPin* DeltaTimePin = DeltaTimeNode->FindPinChecked(DeltaTimeNode->AttrName.ToString());

			TArray<FName> OutputNames;
			Source->GetParticleAttributes(OutputNames);

			// Simple Euler integration.
			/*
			for(int32 i=0; i<3; i++)
			{
				FGraphNodeCreator<UNiagaraNodeGetAttr> PosNodeCreator(*CreatedGraph);
				UNiagaraNodeGetAttr* PosAttrNode = PosNodeCreator.CreateNode();
				PosAttrNode->AttrName = OutputNames[i];
				PosNodeCreator.Finalize();

				FGraphNodeCreator<UNiagaraNodeGetAttr> VelNodeCreator(*CreatedGraph);
				UNiagaraNodeGetAttr* VelAttrNode = VelNodeCreator.CreateNode();
				VelAttrNode->AttrName = OutputNames[i+3];
				VelNodeCreator.Finalize();


				UEdGraphPin* PosAttrOutputPin = PosAttrNode->FindPinChecked(OutputNames[i].ToString());
				UEdGraphPin* VelAttrOutputPin = VelAttrNode->FindPinChecked(OutputNames[i+3].ToString());
				UEdGraphPin* OutputPin = OutputNode->FindPinChecked(OutputNames[i].ToString());

				UNiagaraNodeOp* MulNode = NULL;
				{
					FGraphNodeCreator<UNiagaraNodeOp> NodeCreator(*CreatedGraph);
					MulNode = NodeCreator.CreateNode();
					MulNode->OpIndex = VectorVM::EOp::mul;
					NodeCreator.Finalize();

					UEdGraphPin* APin = MulNode->FindPinChecked(TEXT("A"));
					UEdGraphPin* BPin = MulNode->FindPinChecked(TEXT("B"));

					APin->MakeLinkTo(VelAttrOutputPin);
					BPin->MakeLinkTo(DeltaTimePin);
				}

				{
					FGraphNodeCreator<UNiagaraNodeOp> NodeCreator(*CreatedGraph);
					UNiagaraNodeOp* AddNode = NodeCreator.CreateNode();
					AddNode->OpIndex = VectorVM::EOp::add;
					NodeCreator.Finalize();

					UEdGraphPin* APin = AddNode->FindPinChecked(TEXT("A"));
					UEdGraphPin* BPin = AddNode->FindPinChecked(TEXT("B"));
					UEdGraphPin* AddResultPin = AddNode->FindPinChecked(TEXT("Result"));
					UEdGraphPin* MulResultPin = MulNode->FindPinChecked(TEXT("Result"));

					APin->MakeLinkTo(PosAttrOutputPin);
					BPin->MakeLinkTo(MulResultPin);
					OutputPin->MakeLinkTo(AddResultPin);
				}
			}

			// Update relative time.
			{
				FGraphNodeCreator<UNiagaraNodeGetAttr> RelTimeNodeCreator(*Source->UpdateGraph);
				UNiagaraNodeGetAttr* RelTimeAttrNode = RelTimeNodeCreator.CreateNode();
				RelTimeAttrNode->AttrName = FName(TEXT("RelativeTime"));
				RelTimeNodeCreator.Finalize();

				FGraphNodeCreator<UNiagaraNodeOp> MadNodeCreator(*Source->UpdateGraph);
				UNiagaraNodeOp* MadNode = MadNodeCreator.CreateNode();
				MadNode->OpIndex = VectorVM::EOp::mad;
				MadNodeCreator.Finalize();

				MadNode->FindPinChecked(TEXT("A"))->MakeLinkTo(DeltaTimePin);
				MadNode->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("0.2");
				MadNode->FindPinChecked(TEXT("C"))->MakeLinkTo(RelTimeAttrNode->FindPinChecked(TEXT("RelativeTime")));

				OutputNode->FindPinChecked(TEXT("RelativeTime"))->MakeLinkTo(MadNode->FindPinChecked(TEXT("Result")));
			}
			*/

			// Set pointer in script to source
			NewScript->Source = Source;

			FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::Get().LoadModuleChecked<FNiagaraEditorModule>(TEXT("NiagaraEditor"));
			NiagaraEditorModule.CompileScript(NewScript);
		}
	}

	return NewScript;
}

#undef LOCTEXT_NAMESPACE