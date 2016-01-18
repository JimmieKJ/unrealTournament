// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ExcelBridgeEditorPrivatePCH.h"
#include "ExcelImportUtilCustomization.h"
#include "ExcelImportUtil.h"
#include "SButton.h"
#include "DesktopPlatformModule.h"

#define LOCTEXT_NAMESPACE "ExcelImportUtilCustomization"

void FExcelImportUtilCustomization::CustomizeHeader(
			TSharedRef<class IPropertyHandle> InStructPropertyHandle, 
			class FDetailWidgetRow& HeaderRow, 
			IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	PropertyHandle = InStructPropertyHandle;

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
			.MaxDesiredWidth(0.0f)
			.MinDesiredWidth(125.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.IsEnabled(!PropertyHandle->IsEditConst())
				.Text(LOCTEXT("ImportFromInstance", "Import From Active Excel Document"))
				.OnClicked(this, &FExcelImportUtilCustomization::OnImportFromInstance)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.IsEnabled(!PropertyHandle->IsEditConst())
				.Text(LOCTEXT("ImportFromFile", "Import From Excel File..."))
				.OnClicked(this, &FExcelImportUtilCustomization::OnImportFromFile)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.IsEnabled(!PropertyHandle->IsEditConst())
				.Text(LOCTEXT("Reimport", "Re-Import"))
				.OnClicked(this, &FExcelImportUtilCustomization::OnReimport)
			]
		];
}

void FExcelImportUtilCustomization::CustomizeChildren(
			TSharedRef<class IPropertyHandle> InStructPropertyHandle, 
			class IDetailChildrenBuilder& StructBuilder, 
			IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	/* do nothing */
}

FExcelImportUtil* FExcelImportUtilCustomization::GetValue()
{
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);
	if (RawData.Num() != 1)
	{
		UE_LOG(LogExcelBridgeEditor, Error, TEXT("Unexpected raw data count of %d"), RawData.Num());
		return nullptr;
	}
	return static_cast<FExcelImportUtil*>(RawData[0]);
}

void FExcelImportUtilCustomization::Error(const FText& Message) const
{
	// log to console
	UE_LOG(LogExcelBridgeEditor, Error, TEXT("%s"), *Message.ToString());

	// display a message box
	FMessageDialog::Open(EAppMsgType::Ok, Message);
}

FReply FExcelImportUtilCustomization::OnImportFromInstance()
{
	FExcelImportUtil* Value = GetValue();
	if (ensure(Value))
	{
		// run the import 
		if (!Value->ImportFromRunningExcelInstance())
		{
			Error(FText::Format(LOCTEXT("ImportFailedFmt", "Import from Excel failed.\n{0}"), FText::FromString(Value->LastErrorMessage)));
		}
	}
	return FReply::Handled();
}

FReply FExcelImportUtilCustomization::OnImportFromFile()
{
	// get value and call ImportFromXlsFile
	FExcelImportUtil* Value = GetValue();
	if (ensure(Value))
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (ensure(DesktopPlatform))
		{
			const FString Filter = TEXT("Excel files (*.xlsm;*.xlsx)|*.xlsm;*.xlsx|All files (*.*)|*.*");
			//TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
			//void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

			const FString& LastFile = Value->GetLastFilePath();
			const FString DefaultPath = LastFile.IsEmpty() ? FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN) : FPaths::GetPath(LastFile);

			// display open file prompt
			TArray<FString> OutFiles;
			if (DesktopPlatform->OpenFileDialog(nullptr, //ParentWindowHandle, 
				LOCTEXT("ImagePickerDialogTitle", "Choose an Excel file").ToString(), 
				DefaultPath, LastFile, Filter, EFileDialogFlags::None, OutFiles ))
			{
				check(OutFiles.Num() == 1);
				FString FilePath = OutFiles[0];

				// standardize this path name
				FPaths::MakeStandardFilename(FilePath);

				// make sure file exists!
				if (FPaths::FileExists(FilePath))
				{					
					// run the import 
					if (!Value->ImportFromXlsFile(FilePath))
					{
						Error(FText::Format(LOCTEXT("ImportFailedFmt", "Import from Excel failed.\n{0}"), FText::FromString(Value->LastErrorMessage)));
					}
				}
				else
				{
					Error(FText::Format(LOCTEXT("OpenFileFailed", "File {0} does not exist."), FText::FromString(FilePath)));
				}
			}
		}
	}
	return FReply::Handled();
}

FReply FExcelImportUtilCustomization::OnReimport()
{
	FExcelImportUtil* Value = GetValue();
	if (ensure(Value))
	{
		// run the import 
		if (!Value->Reimport())
		{
			Error(FText::Format(LOCTEXT("ImportFailedFmt", "Re-Import from Excel failed.\n{0}"), FText::FromString(Value->LastErrorMessage)));
		}
	}
	return FReply::Handled();
}
