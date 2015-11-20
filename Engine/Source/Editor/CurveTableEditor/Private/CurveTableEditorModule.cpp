// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CurveTableEditorPrivatePCH.h"
#include "ModuleManager.h"
#include "CurveTableEditor.h"
#include "Toolkits/ToolkitManager.h"
#include "Engine/CurveTable.h"
#include "EditorFramework/AssetImportData.h"

IMPLEMENT_MODULE( FCurveTableEditorModule, CurveTableEditor );


const FName FCurveTableEditorModule::CurveTableEditorAppIdentifier( TEXT( "CurveTableEditorApp" ) );

void FCurveTableEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
}


void FCurveTableEditorModule::ShutdownModule()
{
	MenuExtensibilityManager.Reset();
}


TSharedRef<ICurveTableEditor> FCurveTableEditorModule::CreateCurveTableEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UCurveTable* Table )
{
	TSharedRef< FCurveTableEditor > NewCurveTableEditor( new FCurveTableEditor() );
	NewCurveTableEditor->InitCurveTableEditor( Mode, InitToolkitHost, Table );
	return NewCurveTableEditor;
}

