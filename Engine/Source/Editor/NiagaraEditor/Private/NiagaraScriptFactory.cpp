// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "NiagaraScript.h"
#include "NiagaraEditorModule.h"
#include "NiagaraScriptFactoryNew.h"
#include "NiagaraNodeOutput.h"

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
	UNiagaraScript* NewScript = NewObject<UNiagaraScript>(InParent, Class, Name, Flags);
	if(NewScript != NULL)
	{
		// Then allocate editor-only 'source' object
		UNiagaraScriptSource* Source = NewObject<UNiagaraScriptSource>(NewScript, NAME_None, RF_Transactional);
		if(Source)
		{
			// Create 'update graph'
			UNiagaraGraph* CreatedGraph = NewObject<UNiagaraGraph>(Source, NAME_None, RF_Transactional);
			Source->NodeGraph = CreatedGraph;

			FGraphNodeCreator<UNiagaraNodeOutput> OutputNodeCreator(*CreatedGraph);
			UNiagaraNodeOutput* OutputNode = OutputNodeCreator.CreateNode();

			//Automatically add attributes that the current render modules rely on. Not sure what we'll want to do here in the end.
			OutputNode->Outputs.Add(FNiagaraVariableInfo(FName(TEXT("Position")), ENiagaraDataType::Vector));
			OutputNode->Outputs.Add(FNiagaraVariableInfo(FName(TEXT("Velocity")), ENiagaraDataType::Vector));
			OutputNode->Outputs.Add(FNiagaraVariableInfo(FName(TEXT("Acceleration")), ENiagaraDataType::Vector));
			OutputNode->Outputs.Add(FNiagaraVariableInfo(FName(TEXT("Rotation")), ENiagaraDataType::Vector));
			OutputNode->Outputs.Add(FNiagaraVariableInfo(FName(TEXT("Color")), ENiagaraDataType::Vector));
			OutputNode->Outputs.Add(FNiagaraVariableInfo(FName(TEXT("Age")), ENiagaraDataType::Vector));

			OutputNodeCreator.Finalize();

			// Set pointer in script to source
			NewScript->Source = Source;

			FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::Get().LoadModuleChecked<FNiagaraEditorModule>(TEXT("NiagaraEditor"));
			NiagaraEditorModule.CompileScript(NewScript);
		}
	}

	return NewScript;
}

#undef LOCTEXT_NAMESPACE