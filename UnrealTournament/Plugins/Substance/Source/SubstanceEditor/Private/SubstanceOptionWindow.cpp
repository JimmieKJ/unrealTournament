//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceEditorPrivatePCH.h"
#include "SubstanceOptionWindow.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "SubstanceOption"


void SSubstanceOptionWindow::Construct(const FArguments& InArgs)
{
	ImportUI = InArgs._ImportUI;
	WidgetWindow = InArgs._WidgetWindow;
	check (ImportUI);
	
	Substance::InstancePath = ImportUI->InstanceDestinationPath;
	Substance::MaterialPath = ImportUI->MaterialDestinationPath;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FPathPickerConfig InstancePathPickerConfig;
	InstancePathPickerConfig.bAllowContextMenu = true;
	InstancePathPickerConfig.DefaultPath = ImportUI->InstanceDestinationPath;
	InstancePathPickerConfig.bFocusSearchBoxWhenOpened = false;
	InstancePathPickerConfig.OnPathSelected = FOnPathSelected::CreateRaw(this, &SSubstanceOptionWindow::InstancePathSelected);
	InstancePathPicker = ContentBrowserModule.Get().CreatePathPicker(InstancePathPickerConfig);

	
	FPathPickerConfig MaterialPathPickerConfig;
	MaterialPathPickerConfig.bAllowContextMenu = true;
	MaterialPathPickerConfig.DefaultPath = ImportUI->MaterialDestinationPath;
	MaterialPathPickerConfig.bFocusSearchBoxWhenOpened = false;
	MaterialPathPickerConfig.OnPathSelected = FOnPathSelected::CreateRaw(this, &SSubstanceOptionWindow::MaterialPathSelected);
	MaterialPathPicker = ContentBrowserModule.Get().CreatePathPicker(MaterialPathPickerConfig);

	this->ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot().AutoHeight().Padding(5)
			[
				SNew(SCheckBox)
				.IsEnabled( !ImportUI->bForceCreateInstance )
				.IsChecked( this, &SSubstanceOptionWindow::GetCreateInstance )
				.OnCheckStateChanged( this, &SSubstanceOptionWindow::OnCreateInstanceChanged )
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SubstanceOptions_dflt_inst", "Create default Instance"))
				]
			]

			+SVerticalBox::Slot().AutoHeight().Padding(5)
			[
				SNew(SCheckBox)
				.IsEnabled( this, &SSubstanceOptionWindow::CreateInstance )
				.OnCheckStateChanged( this, &SSubstanceOptionWindow::OnOverrideInstancePathChanged )
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SubstanceOptions_dflt_inst_path", "Specify Default Instance path"))
				]
			]

			+SVerticalBox::Slot().Padding(5)
			[
				SNew(SBorder)
				[
					SNew(SBox)
					.HeightOverride(200)
					.IsEnabled(this, &SSubstanceOptionWindow::ShowInstancePathPicker)
					[
						InstancePathPicker.ToSharedRef()
					]
				]
			]

			+SVerticalBox::Slot().AutoHeight().Padding(5)
			[
				SAssignNew(InstanceNameWidget, SEditableTextBox)
				.Text(FText::FromString(ImportUI->InstanceName))
				.IsEnabled(this, &SSubstanceOptionWindow::CreateInstance)
				.OnTextChanged(this, &SSubstanceOptionWindow::OnInstanceNameChanged)
			]

			+SVerticalBox::Slot().AutoHeight().Padding(5)
			[
				SNew(SSeparator)
			]

			+SVerticalBox::Slot().AutoHeight().Padding(5)
			[
				SNew(SCheckBox)
				.IsChecked( this, &SSubstanceOptionWindow::GetCreateMaterial )	
				.IsEnabled(this, &SSubstanceOptionWindow::CreateInstance)
				.OnCheckStateChanged( this, &SSubstanceOptionWindow::OnCreateMaterialChanged )
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SubstanceOptions_create_mat", "Create Material"))
				]
			]

			+SVerticalBox::Slot().AutoHeight().Padding(5)
			[
				SNew(SCheckBox)
				.IsEnabled( this, &SSubstanceOptionWindow::CreateMaterial )
				.OnCheckStateChanged( this, &SSubstanceOptionWindow::OnOverrideMaterialPathChanged)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SubstanceOptions_mat_path", "Specify Default Material path"))
				]
			]

			+SVerticalBox::Slot().Padding(5)
			[
				SNew(SBorder)
				[
					SNew(SBox)
					.HeightOverride(200)
					.IsEnabled(this, &SSubstanceOptionWindow::ShowMaterialPathPicker)
					[
						MaterialPathPicker.ToSharedRef()
					]
				]
			]

			+SVerticalBox::Slot().AutoHeight().Padding(5)
			[
				SAssignNew(MaterialNameWidget, SEditableTextBox)
				.Text(FText::FromString(ImportUI->MaterialName))
				.IsEnabled(this, &SSubstanceOptionWindow::CreateMaterial)
				.OnTextChanged(this, &SSubstanceOptionWindow::OnMaterialNameChanged)
			]

			+SVerticalBox::Slot().AutoHeight() .Padding(5)
			[
				SNew(SSeparator)
			]

			+SVerticalBox::Slot().AutoHeight() .Padding(5) .HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot() .FillWidth(1)
				[
					SAssignNew( ImportButton, SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("SubstanceOptionWindow_Import", "Import"))
					.OnClicked_Raw( this, &SSubstanceOptionWindow::OnImport )
				]

				+SHorizontalBox::Slot() .FillWidth(1)
				[
					SNew(SButton) .HAlign(HAlign_Center)
					.Text(LOCTEXT("SubstanceOptionWindow_Cancel", "Cancel"))
					.OnClicked_Raw( this, &SSubstanceOptionWindow::OnCancel )
				]
			]
		]
	];

	if(WidgetWindow.IsValid())
	{
		WidgetWindow.Pin()->SetWidgetToFocusOnActivate(ImportButton);
	}
}


void SSubstanceOptionWindow::OnInstanceNameChanged(const FText& InNewName)
{
	TArray<FAssetData> AssetData;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().GetAssetsByPath(FName(*Substance::InstancePath), AssetData);

	FText ErrorText;
	if(!FEditorFileUtils::IsFilenameValidForSaving(InNewName.ToString(), ErrorText) || !FName(*InNewName.ToString()).IsValidObjectName( ErrorText ))
	{
		InstanceNameWidget.Pin()->SetError(ErrorText);
		bIsReportingError = true;
		return;
	}
	else
	{
		// Check to see if the name conflicts
		for( auto Iter = AssetData.CreateConstIterator(); Iter; ++Iter)
		{
			if(Iter->AssetName.ToString() == InNewName.ToString())
			{
				InstanceNameWidget.Pin()->SetError(LOCTEXT("AssetInUseError", "Asset name already in use!"));
				bIsReportingError = true;
				return;
			}
		}
	}

	InstanceNameWidget.Pin()->SetError(FText::FromString(TEXT("")));
	bIsReportingError = false;
}


void SSubstanceOptionWindow::OnMaterialNameChanged(const FText& InNewName)
{
	TArray<FAssetData> AssetData;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().GetAssetsByPath(FName(*Substance::InstancePath), AssetData);

	FText ErrorText;
	if(!FEditorFileUtils::IsFilenameValidForSaving(InNewName.ToString(), ErrorText) || !FName(*InNewName.ToString()).IsValidObjectName( ErrorText ))
	{
		InstanceNameWidget.Pin()->SetError(ErrorText);
		bIsReportingError = true;
		return;
	}
	else
	{
		// Check to see if the name conflicts
		for( auto Iter = AssetData.CreateConstIterator(); Iter; ++Iter)
		{
			if(Iter->AssetName.ToString() == InNewName.ToString())
			{
				InstanceNameWidget.Pin()->SetError(LOCTEXT("AssetInUseError", "Asset name already in use!"));
				bIsReportingError = true;
				return;
			}
		}
	}

	InstanceNameWidget.Pin()->SetError(FText::FromString(TEXT("")));
	bIsReportingError = false;
}

#undef LOCTEXT_NAMESPACE
