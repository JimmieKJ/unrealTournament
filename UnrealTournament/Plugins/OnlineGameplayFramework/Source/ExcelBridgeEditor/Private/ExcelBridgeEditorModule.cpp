// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ExcelBridgeEditorPrivatePCH.h"
#include "ExcelBridgeEditorModule.h"
#include "ExcelBridge.h"
#include "ExcelImportUtilCustomization.h"

DEFINE_LOG_CATEGORY(LogExcelBridgeEditor);
IMPLEMENT_MODULE(FExcelBridgeEditorModule, ExcelBridgeEditor);

// sorry win32 editor muahahahaa
static const TCHAR* DllFilename = TEXT("Plugins/OnlineGameplayFramework/Dependencies/ExcelBridge.dll");

FExcelBridgeEditorModule::FExcelBridgeEditorModule()
: DllHandle(nullptr)
, NewExcelBridgeDllFunc(nullptr)
, DeleteExcelBridgeDllFunc(nullptr)
{
}

FExcelBridgeEditorModule::~FExcelBridgeEditorModule()
{
	// should have been freed in shutdown
	check(DllHandle == nullptr);
}

FExcelBridge* FExcelBridgeEditorModule::NewExcelBridge()
{
	// see if we need to load the DLL
	if (DllHandle == nullptr)
	{
		FString FullPath = FPaths::GameDir() / DllFilename;
		DllHandle = FPlatformProcess::GetDllHandle(*FullPath);
		if (DllHandle == nullptr)
		{
			// log an error
			UE_LOG(LogExcelBridgeEditor, Error, TEXT("Unable to load DLL at %s"), *FullPath);
			return nullptr;
		}

		NewExcelBridgeDllFunc = (NewExcelBridgeSignature)FPlatformProcess::GetDllExport(DllHandle, TEXT("NewExcelBridge"));
		DeleteExcelBridgeDllFunc = (DeleteExcelBridgeSignature)FPlatformProcess::GetDllExport(DllHandle, TEXT("DeleteExcelBridge"));

		if (NewExcelBridgeDllFunc == nullptr || DeleteExcelBridgeDllFunc == nullptr)
		{
			// log an error
			UE_LOG(LogExcelBridgeEditor, Error, TEXT("Unable to map NewExcelBridge/DeleteExcelBridge from %s"), *FullPath);

			// give up
			FPlatformProcess::FreeDllHandle(DllHandle);
			NewExcelBridgeDllFunc = nullptr;
			DeleteExcelBridgeDllFunc = nullptr;
			DllHandle = nullptr;
			return nullptr;
		}
	}

	check(NewExcelBridgeDllFunc);
	return NewExcelBridgeDllFunc();
}

void FExcelBridgeEditorModule::DeleteExcelBridge(FExcelBridge*& Bridge)
{
	if (Bridge == nullptr)
		return;
	check(DllHandle);
	check(DeleteExcelBridgeDllFunc);
	DeleteExcelBridgeDllFunc(Bridge);
	Bridge = nullptr;
}

bool FExcelBridgeEditorModule::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Ignore any execs that don't start with ExcelBridgeEditor
	if (FParse::Command(&Cmd, TEXT("ExcelBridgeEditor")))
	{
		return false;
	}

	return false;
}

void FExcelBridgeEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// register details customization
	PropertyModule.RegisterCustomPropertyTypeLayout("ExcelImportUtil", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FExcelImportUtilCustomization::MakeInstance));

	// notify customization module changed
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FExcelBridgeEditorModule::ShutdownModule()
{
	if (DllHandle)
	{
		FPlatformProcess::FreeDllHandle(DllHandle);
		DllHandle = nullptr;
		NewExcelBridgeDllFunc = nullptr;
		DeleteExcelBridgeDllFunc = nullptr;
	}
}
