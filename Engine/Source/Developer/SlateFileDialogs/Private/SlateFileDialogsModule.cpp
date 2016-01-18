// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateFileDialogsPrivatePCH.h"
#include "SlateBasics.h"
#include "SlateFileDialogs.h"
#include "SlateFileDlgWindow.h"
#include "SlateFileDialogsStyles.h"

void FSlateFileDialogsModule::StartupModule()
{
	SlateFileDialog = new FSlateFileDialogsModule();

	FileDialogStyle = new FSlateFileDialogsStyle;
	FileDialogStyle->Initialize();
}


void FSlateFileDialogsModule::ShutdownModule()
{
	if (SlateFileDialog != NULL)
	{
		FileDialogStyle->Shutdown();
		delete FileDialogStyle;

		delete SlateFileDialog;
		SlateFileDialog = NULL;
	}
}


bool FSlateFileDialogsModule::OpenFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath,
		const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames, int32& OutFilterIndex)
{
	FSlateFileDlgWindow dialog(FileDialogStyle);
	return dialog.OpenFileDialog(ParentWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypes, Flags, OutFilenames, OutFilterIndex);
}


bool FSlateFileDialogsModule::OpenFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath,
		const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames)
{
	FSlateFileDlgWindow dialog(FileDialogStyle);
	return dialog.OpenFileDialog(ParentWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypes, Flags, OutFilenames);
}


bool FSlateFileDialogsModule::OpenDirectoryDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath,
		FString& OutFoldername)
{
	FSlateFileDlgWindow dialog(FileDialogStyle);
	return dialog.OpenDirectoryDialog(ParentWindowHandle, DialogTitle, DefaultPath, OutFoldername);
}


bool FSlateFileDialogsModule::SaveFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath,
	const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames)
{
	FSlateFileDlgWindow dialog(FileDialogStyle);
	return dialog.SaveFileDialog(ParentWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypes, Flags, OutFilenames);
}


ISlateFileDialogsModule* FSlateFileDialogsModule::Get()
{
	return SlateFileDialog;
}

IMPLEMENT_MODULE(FSlateFileDialogsModule, SlateFileDialogs);
