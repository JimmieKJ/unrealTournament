// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "AssetImportDataCustomization.h"
#include "SEditableText.h"
#include "EditorReimportHandler.h"
#include "ObjectEditorUtils.h"

#define LOCTEXT_NAMESPACE "AssetImportDataCustomization"

TSharedRef<IPropertyTypeCustomization> FAssetImportDataCustomization::MakeInstance()
{
	return MakeShareable( new FAssetImportDataCustomization );
}

void FAssetImportDataCustomization::CustomizeHeader( TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils )
{
}

void FAssetImportDataCustomization::CustomizeChildren( TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils )
{
	PropertyHandle = InPropertyHandle;

	FAssetImportInfo* Info = GetEditStruct();
	if (!Info)
	{
		return;
	}

	auto Font = IDetailLayoutBuilder::GetDetailFont();

	const FText SourceFileText = LOCTEXT("SourceFile", "Source File");
	for (int32 Index = 0; Index < Info->SourceFiles.Num(); ++Index)
	{
		ChildBuilder.AddChildContent(SourceFileText)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(SourceFileText)
			.Font(Font)
		]
		.ValueContent()
		.HAlign(HAlign_Fill)
		.MaxDesiredWidth(TOptional<float>())
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				[
					SNew(SEditableText)
					.IsReadOnly(true)
					.Text(this, &FAssetImportDataCustomization::GetFilenameText, Index)
					.ToolTipText(this, &FAssetImportDataCustomization::GetFilenameText, Index)
					.Font(Font)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(this, &FAssetImportDataCustomization::OnChangePathClicked, Index)
					.ToolTipText(LOCTEXT("ChangePath_Tooltip", "Browse for a new source file path"))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("...", "..."))
						.Font(Font)
					]
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
					.OnClicked(this, &FAssetImportDataCustomization::OnClearPathClicked, Index)
					.ToolTipText(LOCTEXT("ClearPath_Tooltip", "Clear this source file information from the asset"))
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("Cross"))
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SEditableText)
				.IsReadOnly(true)
				.Text(this, &FAssetImportDataCustomization::GetTimestampText, Index)
				.Font(Font)
			]
		];
	}
}

FText FAssetImportDataCustomization::GetFilenameText(int32 Index) const
{
	if (FAssetImportInfo* Info = GetEditStruct())
	{
		if (Info->SourceFiles.IsValidIndex(Index))
		{
			return FText::FromString(Info->SourceFiles[Index].RelativeFilename);
		}
	}
	return FText();
}

FText FAssetImportDataCustomization::GetTimestampText(int32 Index) const
{
	if (FAssetImportInfo* Info = GetEditStruct())
	{
		if (Info->SourceFiles.IsValidIndex(Index))
		{
			return FText::FromString(Info->SourceFiles[Index].Timestamp.ToString());
		}
	}
	return FText();
}


FAssetImportInfo* FAssetImportDataCustomization::GetEditStruct() const
{
	static TArray<FAssetImportInfo*> AssetImportInfo;
	AssetImportInfo.Reset();

	if(PropertyHandle->IsValidHandle())
	{
		PropertyHandle->AccessRawData(reinterpret_cast<TArray<void*>&>(AssetImportInfo));
	}

	if (AssetImportInfo.Num() == 1)
	{
		return AssetImportInfo[0];
	}
	return nullptr;
}

UAssetImportData* FAssetImportDataCustomization::GetOuterClass() const
{
	static TArray<UObject*> OuterObjects;
	OuterObjects.Reset();

	PropertyHandle->GetOuterObjects(OuterObjects);

	return OuterObjects.Num() ? Cast<UAssetImportData>(OuterObjects[0]) : nullptr;
}

FReply FAssetImportDataCustomization::OnChangePathClicked(int32 Index) const
{
	UAssetImportData* ImportData = GetOuterClass();
	UObject* Obj = ImportData ? ImportData->GetOuter() : nullptr;

	if (!Obj)
	{
		return FReply::Handled();
	}

	TArray<FString> OpenFilenames;
	FReimportManager::Instance()->GetNewReimportPath(Obj, OpenFilenames);
	if (OpenFilenames.Num() == 1)
	{
		ImportData->UpdateFilenameOnly(FPaths::ConvertRelativePathToFull(OpenFilenames[0]), Index);
		ImportData->MarkPackageDirty();
	}
	return FReply::Handled();
}

FReply FAssetImportDataCustomization::OnClearPathClicked(int32 Index) const
{
	UAssetImportData* ImportData = GetOuterClass();
	if (ImportData && ImportData->SourceData.SourceFiles.IsValidIndex(Index))
	{
		ImportData->SourceData.SourceFiles[Index] = FAssetImportInfo::FSourceFile(FString());
		ImportData->MarkPackageDirty();
	}

	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE
