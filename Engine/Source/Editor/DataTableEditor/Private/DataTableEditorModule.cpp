// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DataTableEditorPrivatePCH.h"
#include "ModuleManager.h"
#include "DataTableEditor.h"
#include "Toolkits/ToolkitManager.h"
#include "Engine/DataTable.h"

IMPLEMENT_MODULE( FDataTableEditorModule, DataTableEditor );


const FName FDataTableEditorModule::DataTableEditorAppIdentifier( TEXT( "DataTableEditorApp" ) );

void FDataTableEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
}


void FDataTableEditorModule::ShutdownModule()
{
	MenuExtensibilityManager.Reset();
}


TSharedRef<IDataTableEditor> FDataTableEditorModule::CreateDataTableEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UDataTable* Table )
{
	TSharedRef< FDataTableEditor > NewDataTableEditor( new FDataTableEditor() );
	NewDataTableEditor->InitDataTableEditor( Mode, InitToolkitHost, Table );
	return NewDataTableEditor;
}

