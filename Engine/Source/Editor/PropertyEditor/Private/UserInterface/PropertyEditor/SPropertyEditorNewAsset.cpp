// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "SPropertyEditorNewAsset.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "AssetRegistryModule.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "SPropertyEditorNewAsset"

UObject* SPropertyEditorNewAsset::Create(TWeakObjectPtr<UFactory> FactoryPtr)
{
	if (!FactoryPtr.IsValid())
	{
		return nullptr;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetType"), FactoryPtr->GetDisplayName());

	TSharedPtr<SWindow> PropertyEditorNewAssetWindow =
		SNew(SWindow)
		.Title(FText::Format(LOCTEXT("SelectPath", "Select Path for {AssetType}"), Args))
		.ToolTipText(FText::Format(LOCTEXT("SelectPathTooltip", "Select the path where the {AssetType} will be created."), Args))
		.ClientSize(FVector2D(400, 400));

	TSharedPtr<SPropertyEditorNewAsset> PropertyEditorNewAssetWidget;
	PropertyEditorNewAssetWindow->SetContent(SAssignNew(PropertyEditorNewAssetWidget, SPropertyEditorNewAsset, PropertyEditorNewAssetWindow, FactoryPtr));

	GEditor->EditorAddModalWindow(PropertyEditorNewAssetWindow.ToSharedRef());

	check(PropertyEditorNewAssetWidget.IsValid());
	return PropertyEditorNewAssetWidget->NewAsset;
}

void SPropertyEditorNewAsset::Construct(const FArguments& InArgs, TSharedPtr<SWindow> InParentWindow, TWeakObjectPtr<UFactory> InFactoryPtr)
{
	bIsReportingError = false;
	ParentWindowPtr = InParentWindow;
	FactoryPtr = InFactoryPtr;
	NewAsset = nullptr;
	AssetPath = FString("/Game");

	check(FactoryPtr.IsValid());

	FString AssetName = FactoryPtr->GetDefaultNewAssetName();

	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = AssetPath;
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateRaw(this, &SPropertyEditorNewAsset::OnSelectAssetPath);

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 2.0f, 0.0f, 4.0f)
		[
			SNew(SSeparator)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		.Padding(2.0f, 0.0f, 2.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 16.0f, 0.0f)
			.FillWidth(0.6f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TextboxLabel", "Asset Name:"))
				]
				+ SHorizontalBox::Slot()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				.FillWidth(1.0f)
				[
					SAssignNew(FilenameWidget, SEditableTextBox)
					.Text(FText::FromString(AssetName))
					.OnTextChanged(this, &SPropertyEditorNewAsset::OnFilenameChanged)
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.FillWidth(0.4f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				.AutoWidth()
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.ContentPadding(FMargin(8, 2, 8, 2))
					.OnClicked(this, &SPropertyEditorNewAsset::OnCreateNewAssetClicked)
					.IsEnabled(this, &SPropertyEditorNewAsset::IsCreateNewAssetEnabled)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CreateButtonText", "Create"))
					]
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.ContentPadding(FMargin(8, 2, 8, 2))
					.OnClicked(this, &SPropertyEditorNewAsset::OnCancelCreateNewAsset)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CancelButtonText", "Cancel"))
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 10.0f, 0.0f, 0.0f)
		[
			SNullWidget::NullWidget
		]
	];

	OnFilenameChanged(FText::FromString(AssetName));
}

FReply SPropertyEditorNewAsset::OnCreateNewAssetClicked()
{
	TSharedPtr<SWindow> ParentWindow = ParentWindowPtr.Pin();
	if (ParentWindow.IsValid())
	{
		ParentWindow->RequestDestroyWindow();
	}

	FString AssetName = FilenameWidget->GetText().ToString();
	FString AssetFullName = AssetPath / AssetName;

	if (FactoryPtr.IsValid())
	{
		UFactory* FactoryInstance = DuplicateObject<UFactory>(FactoryPtr.Get(), GetTransientPackage());

		FEditorDelegates::OnConfigureNewAssetProperties.Broadcast(FactoryInstance);
		if (FactoryInstance->ConfigureProperties())
		{
			FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
			NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, AssetPath, FactoryInstance->GetSupportedClass(), FactoryInstance);
		}
	}

	return FReply::Handled();
}

FReply SPropertyEditorNewAsset::OnCancelCreateNewAsset()
{
	TSharedPtr<SWindow> ParentWindow = ParentWindowPtr.Pin();
	if (ParentWindow.IsValid())
	{
		ParentWindow->RequestDestroyWindow();
	}

	return FReply::Handled();
}

void SPropertyEditorNewAsset::OnSelectAssetPath(const FString& Path)
{
	AssetPath = Path;
	OnFilenameChanged(FilenameWidget->GetText());
}

void SPropertyEditorNewAsset::OnFilenameChanged(const FText& InNewName)
{
	TArray<FAssetData> AssetData;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().GetAssetsByPath(FName(*AssetPath), AssetData);

	FText ErrorText;
	if (!FEditorFileUtils::IsFilenameValidForSaving(InNewName.ToString(), ErrorText) || !FName(*InNewName.ToString()).IsValidObjectName(ErrorText))
	{
		FilenameWidget->SetError(LOCTEXT("AssetNameInvalid", "Asset name is invalid"));
		bIsReportingError = true;
		return;
	}
	else
	{
		// Check to see if the name conflicts
		for (auto Iter = AssetData.CreateConstIterator(); Iter; ++Iter)
		{
			if (Iter->AssetName.ToString() == InNewName.ToString())
			{
				FilenameWidget->SetError(LOCTEXT("AssetInUseError", "Asset name already in use!"));
				bIsReportingError = true;
				return;
			}
		}
	}

	FilenameWidget->SetError(FText::FromString(TEXT("")));
	bIsReportingError = false;
}

bool SPropertyEditorNewAsset::IsCreateNewAssetEnabled() const
{
	return !bIsReportingError;
}

#undef LOCTEXT_NAMESPACE
