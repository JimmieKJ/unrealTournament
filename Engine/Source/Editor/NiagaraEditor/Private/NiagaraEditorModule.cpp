// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "ModuleManager.h"
#include "Toolkits/ToolkitManager.h"

IMPLEMENT_MODULE( FNiagaraEditorModule, NiagaraEditor );


const FName FNiagaraEditorModule::NiagaraEditorAppIdentifier( TEXT( "NiagaraEditorApp" ) );

void FNiagaraEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);
}


void FNiagaraEditorModule::ShutdownModule()
{
	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();
}


TSharedRef<INiagaraEditor> FNiagaraEditorModule::CreateNiagaraEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UNiagaraScript* Script )
{
	TSharedRef< FNiagaraEditor > NewNiagaraEditor( new FNiagaraEditor() );
	NewNiagaraEditor->InitNiagaraEditor( Mode, InitToolkitHost, Script );
	return NewNiagaraEditor;
}

TSharedRef<INiagaraEffectEditor> FNiagaraEditorModule::CreateNiagaraEffectEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraEffect* Effect)
{
	TSharedRef< FNiagaraEffectEditor > NewNiagaraEditor(new FNiagaraEffectEditor());
	NewNiagaraEditor->InitNiagaraEffectEditor(Mode, InitToolkitHost, Effect);
	return NewNiagaraEditor;
}
