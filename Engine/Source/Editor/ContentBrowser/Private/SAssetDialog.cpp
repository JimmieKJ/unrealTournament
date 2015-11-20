// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserPCH.h"
#include "SAssetDialog.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

SAssetDialog::SAssetDialog()
	: DialogType(EAssetDialogType::Open)
	, ExistingAssetPolicy(ESaveAssetDialogExistingAssetPolicy::Disallow)
	, bLastInputValidityCheckSuccessful(false)
	, bPendingFocusNextFrame(true)
	, bValidAssetsChosen(false)
{
}

SAssetDialog::~SAssetDialog()
{
	if (!bValidAssetsChosen)
	{
		OnAssetDialogCancelled.ExecuteIfBound();
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SAssetDialog::Construct(const FArguments& InArgs, const FSharedAssetDialogConfig& InConfig)
{
	DialogType = InConfig.GetDialogType();

	AssetClassNames = InConfig.AssetClassNames;

	const FString DefaultPath = InConfig.DefaultPath;

	RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateSP( this, &SAssetDialog::SetFocusPostConstruct ) );

	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = DefaultPath;
	PathPickerConfig.bFocusSearchBoxWhenOpened = false;
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SAssetDialog::HandlePathSelected);
	PathPickerConfig.SetPathsDelegates.Add(&SetPathsDelegate);

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Append(AssetClassNames);
	AssetPickerConfig.Filter.PackagePaths.Add(FName(*DefaultPath));
	AssetPickerConfig.bAllowDragging = false;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::Tile;
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SAssetDialog::OnAssetSelected);
	AssetPickerConfig.OnAssetsActivated = FOnAssetsActivated::CreateSP(this, &SAssetDialog::OnAssetsActivated);
	AssetPickerConfig.SetFilterDelegates.Add(&SetFilterDelegate);
	AssetPickerConfig.GetCurrentSelectionDelegates.Add(&GetCurrentSelectionDelegate);
	AssetPickerConfig.SaveSettingsName = TEXT("AssetDialog");
	AssetPickerConfig.bCanShowFolders = true;
	AssetPickerConfig.bCanShowDevelopersFolder = true;
	AssetPickerConfig.OnFolderEntered = FOnPathSelected::CreateSP(this, &SAssetDialog::HandleAssetViewFolderEntered);

	SetCurrentlySelectedPath(DefaultPath);

	// Open and save specific configuration
	FText ConfirmButtonText;
	bool bIncludeNameBox = false;
	if ( DialogType == EAssetDialogType::Open )
	{
		const FOpenAssetDialogConfig& OpenAssetConfig = static_cast<const FOpenAssetDialogConfig&>(InConfig);
		PathPickerConfig.bAllowContextMenu = false;
		ConfirmButtonText = LOCTEXT("AssetDialogOpenButton", "Open");
		AssetPickerConfig.SelectionMode = OpenAssetConfig.bAllowMultipleSelection ? ESelectionMode::Multi : ESelectionMode::Single;
		AssetPickerConfig.bFocusSearchBoxWhenOpened = true;
		bIncludeNameBox = false;
	}
	else if ( DialogType == EAssetDialogType::Save )
	{
		const FSaveAssetDialogConfig& SaveAssetConfig = static_cast<const FSaveAssetDialogConfig&>(InConfig);
		PathPickerConfig.bAllowContextMenu = true;
		ConfirmButtonText = LOCTEXT("AssetDialogSaveButton", "Save");
		AssetPickerConfig.SelectionMode = ESelectionMode::Single;
		AssetPickerConfig.bFocusSearchBoxWhenOpened = false;
		bIncludeNameBox = true;
		ExistingAssetPolicy = SaveAssetConfig.ExistingAssetPolicy;
		SetCurrentlyEnteredAssetName(SaveAssetConfig.DefaultAssetName);
	}
	else
	{
		ensureMsgf(0, TEXT("AssetDialog type %d is not supported."), DialogType);
	}

	TSharedPtr<SWidget> PathPicker = FContentBrowserSingleton::Get().CreatePathPicker(PathPickerConfig);
	TSharedPtr<SWidget> AssetPicker = FContentBrowserSingleton::Get().CreateAssetPicker(AssetPickerConfig);

	// The root widget in this dialog.
	TSharedRef<SVerticalBox> MainVerticalBox = SNew(SVerticalBox);

	// Path/Asset view
	MainVerticalBox->AddSlot()
		.FillHeight(1)
		.Padding(0, 0, 0, 4)
		[
			SNew(SSplitter)
		
			+SSplitter::Slot()
			.Value(0.25f)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					PathPicker.ToSharedRef()
				]
			]

			+SSplitter::Slot()
			.Value(0.75f)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					AssetPicker.ToSharedRef()
				]
			]
		];

	// Input error strip, if we are using a name box
	if (bIncludeNameBox)
	{
		// Name Error label
		MainVerticalBox->AddSlot()
		.AutoHeight()
		[
			// Constant height, whether the label is visible or not
			SNew(SBox).HeightOverride(18)
			[
				SNew(SBorder)
				.Visibility( this, &SAssetDialog::GetNameErrorLabelVisibility )
				.BorderImage( FEditorStyle::GetBrush("AssetDialog.ErrorLabelBorder") )
				.Content()
				[
					SNew(STextBlock)
					.Text( this, &SAssetDialog::GetNameErrorLabelText )
					.TextStyle( FEditorStyle::Get(), "AssetDialog.ErrorLabelFont" )
				]
			]
		];
	}

	TSharedRef<SVerticalBox> LabelsBox = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1)
		.VAlign(VAlign_Center)
		.Padding(0, 2, 0, 2)
		[
			SNew(STextBlock).Text(LOCTEXT("PathBoxLabel", "Path:"))
		];

	TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1)
		.VAlign(VAlign_Center)
		.Padding(0, 2, 0, 2)
		[
			SAssignNew(PathText, STextBlock)
			.Text(this, &SAssetDialog::GetPathNameText)
		];

	if (bIncludeNameBox)
	{
		LabelsBox->AddSlot()
			.FillHeight(1)
			.VAlign(VAlign_Center)
			.Padding(0, 2, 0, 2)
			[
				SNew(STextBlock).Text(LOCTEXT("NameBoxLabel", "Name:"))
			];

		ContentBox->AddSlot()
			.FillHeight(1)
			.VAlign(VAlign_Center)
			.Padding(0, 2, 0, 2)
			[
				SAssignNew(NameEditableText, SEditableTextBox)
				.Text(this, &SAssetDialog::GetAssetNameText)
				.OnTextCommitted(this, &SAssetDialog::OnAssetNameTextCommited)
				.OnTextChanged(this, &SAssetDialog::OnAssetNameTextCommited, ETextCommit::Default)
				.SelectAllTextWhenFocused(true)
			];
	}

	// Buttons and asset name
	TSharedRef<SHorizontalBox> ButtonsAndNameBox = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(bIncludeNameBox ? 80 : 4, 20, 4, 3)
		[
			LabelsBox
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		.VAlign(VAlign_Bottom)
		.Padding(4, 3)
		[
			ContentBox
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Bottom)
		.Padding(4, 3)
		[
			SNew(SButton)
			.Text(ConfirmButtonText)
			.ContentPadding(FMargin(8, 2, 8, 2))
			.IsEnabled(this, &SAssetDialog::IsConfirmButtonEnabled)
			.OnClicked(this, &SAssetDialog::OnConfirmClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Bottom)
		.Padding(4, 3)
		[
			SNew(SButton)
			.ContentPadding(FMargin(8, 2, 8, 2))
			.Text(LOCTEXT("AssetDialogCancelButton", "Cancel"))
			.OnClicked(this, &SAssetDialog::OnCancelClicked)
		];

	MainVerticalBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding(0)
		[
			ButtonsAndNameBox
		];

	ChildSlot
	[
		MainVerticalBox
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FReply SAssetDialog::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if ( InKeyEvent.GetKey() == EKeys::Escape )
	{
		CloseDialog();
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

EActiveTimerReturnType SAssetDialog::SetFocusPostConstruct( double InCurrentTime, float InDeltaTime )
{
	FocusNameBox();
	return EActiveTimerReturnType::Stop;
}

void SAssetDialog::SetOnAssetsChosenForOpen(const FOnAssetsChosenForOpen& InOnAssetsChosenForOpen)
{
	OnAssetsChosenForOpen = InOnAssetsChosenForOpen;
}

void SAssetDialog::SetOnObjectPathChosenForSave(const FOnObjectPathChosenForSave& InOnObjectPathChosenForSave)
{
	OnObjectPathChosenForSave = InOnObjectPathChosenForSave;
}

void SAssetDialog::SetOnAssetDialogCancelled(const FOnAssetDialogCancelled& InOnAssetDialogCancelled)
{
	OnAssetDialogCancelled = InOnAssetDialogCancelled;
}

void SAssetDialog::FocusNameBox()
{
	if ( NameEditableText.IsValid() )
	{
		FSlateApplication::Get().SetKeyboardFocus(NameEditableText.ToSharedRef(), EFocusCause::SetDirectly);
	}
}

FText SAssetDialog::GetAssetNameText() const
{
	return FText::FromString(CurrentlyEnteredAssetName);
}

FText SAssetDialog::GetPathNameText() const
{
	return FText::FromString(CurrentlySelectedPath);
}

void SAssetDialog::OnAssetNameTextCommited(const FText& InText, ETextCommit::Type InCommitType)
{
	SetCurrentlyEnteredAssetName(InText.ToString());

	if ( InCommitType == ETextCommit::OnEnter )
	{
		CommitObjectPathForSave();
	}
}

EVisibility SAssetDialog::GetNameErrorLabelVisibility() const
{
	return GetNameErrorLabelText().IsEmpty() ? EVisibility::Hidden : EVisibility::Visible;
}

FText SAssetDialog::GetNameErrorLabelText() const
{
	if (!bLastInputValidityCheckSuccessful)
	{
		return LastInputValidityErrorText;
	}

	return FText::GetEmpty();
}

void SAssetDialog::HandlePathSelected(const FString& NewPath)
{
	FARFilter NewFilter;

	NewFilter.ClassNames.Append(AssetClassNames);
	NewFilter.PackagePaths.Add(FName(*NewPath));

	SetCurrentlySelectedPath(NewPath);

	SetFilterDelegate.ExecuteIfBound(NewFilter);
}

void SAssetDialog::HandleAssetViewFolderEntered(const FString& NewPath)
{
	SetCurrentlySelectedPath(NewPath);

	TArray<FString> NewPaths;
	NewPaths.Add(NewPath);
	SetPathsDelegate.Execute(NewPaths);
}

bool SAssetDialog::IsConfirmButtonEnabled() const
{
	if ( DialogType == EAssetDialogType::Open )
	{
		return CurrentlySelectedAssets.Num() > 0;
	}
	else if ( DialogType == EAssetDialogType::Save )
	{
		return bLastInputValidityCheckSuccessful;
	}
	else
	{
		ensureMsgf(0, TEXT("AssetDialog type %d is not supported."), DialogType);
	}

	return false;
}

FReply SAssetDialog::OnConfirmClicked()
{
	if ( DialogType == EAssetDialogType::Open )
	{
		TArray<FAssetData> SelectedAssets = GetCurrentSelectionDelegate.Execute();
		if (SelectedAssets.Num() > 0)
		{
			ChooseAssetsForOpen(SelectedAssets);
		}
	}
	else if ( DialogType == EAssetDialogType::Save )
	{
		// @todo save asset validation (e.g. "asset already exists" check)
		CommitObjectPathForSave();
	}
	else
	{
		ensureMsgf(0, TEXT("AssetDialog type %d is not supported."), DialogType);
	}

	return FReply::Handled();
}

FReply SAssetDialog::OnCancelClicked()
{
	CloseDialog();

	return FReply::Handled();
}

void SAssetDialog::OnAssetSelected(const FAssetData& AssetData)
{
	CurrentlySelectedAssets = GetCurrentSelectionDelegate.Execute();
	
	if ( AssetData.IsValid() )
	{
		SetCurrentlySelectedPath(AssetData.PackagePath.ToString());
		SetCurrentlyEnteredAssetName(AssetData.AssetName.ToString());
	}
}

void SAssetDialog::OnAssetsActivated(const TArray<FAssetData>& SelectedAssets, EAssetTypeActivationMethod::Type ActivationType)
{
	const bool bCorrectActivationMethod = (ActivationType == EAssetTypeActivationMethod::DoubleClicked || ActivationType == EAssetTypeActivationMethod::Opened);
	if (SelectedAssets.Num() > 0 && bCorrectActivationMethod)
	{
		if ( DialogType == EAssetDialogType::Open )
		{
			ChooseAssetsForOpen(SelectedAssets);
		}
		else if ( DialogType == EAssetDialogType::Save )
		{
			const FAssetData& AssetData = SelectedAssets[0];
			SetCurrentlySelectedPath(AssetData.PackagePath.ToString());
			SetCurrentlyEnteredAssetName(AssetData.AssetName.ToString());
			CommitObjectPathForSave();
		}
		else
		{
			ensureMsgf(0, TEXT("AssetDialog type %d is not supported."), DialogType);
		}
	}
}

void SAssetDialog::CloseDialog()
{
	FWidgetPath WidgetPath;
	TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(AsShared(), WidgetPath);

	if (ContainingWindow.IsValid())
	{
		ContainingWindow->RequestDestroyWindow();
	}
}

void SAssetDialog::SetCurrentlySelectedPath(const FString& NewPath)
{
	CurrentlySelectedPath = NewPath;
	UpdateInputValidity();
}

void SAssetDialog::SetCurrentlyEnteredAssetName(const FString& NewName)
{
	CurrentlyEnteredAssetName = NewName;
	UpdateInputValidity();
}

void SAssetDialog::UpdateInputValidity()
{
	bLastInputValidityCheckSuccessful = true;

	if ( CurrentlyEnteredAssetName.IsEmpty() )
	{
		// No error text for an empty name. Just fail validity.
		LastInputValidityErrorText = FText::GetEmpty();
		bLastInputValidityCheckSuccessful = false;
	}

	if (bLastInputValidityCheckSuccessful)
	{
		if ( CurrentlySelectedPath.IsEmpty() )
		{
			LastInputValidityErrorText = LOCTEXT("AssetDialog_NoPathSelected", "You must select a path.");
			bLastInputValidityCheckSuccessful = false;
		}
	}

	if ( DialogType == EAssetDialogType::Save )
	{
		if ( bLastInputValidityCheckSuccessful )
		{
			const FString ObjectPath = GetObjectPathForSave();
			FText ErrorMessage;
			const bool bAllowExistingAsset = (ExistingAssetPolicy == ESaveAssetDialogExistingAssetPolicy::AllowButWarn);
			if ( !ContentBrowserUtils::IsValidObjectPathForCreate(ObjectPath, ErrorMessage, bAllowExistingAsset) )
			{
				LastInputValidityErrorText = ErrorMessage;
				bLastInputValidityCheckSuccessful = false;
			}
		}
	}
}

void SAssetDialog::ChooseAssetsForOpen(const TArray<FAssetData>& SelectedAssets)
{
	if ( ensure(DialogType == EAssetDialogType::Open) )
	{
		if (SelectedAssets.Num() > 0)
		{
			bValidAssetsChosen = true;
			OnAssetsChosenForOpen.ExecuteIfBound(SelectedAssets);
			CloseDialog();
		}
	}
}

FString SAssetDialog::GetObjectPathForSave() const
{
	return CurrentlySelectedPath / CurrentlyEnteredAssetName + TEXT(".") + CurrentlyEnteredAssetName;
}

void SAssetDialog::CommitObjectPathForSave()
{
	if ( ensure(DialogType == EAssetDialogType::Save) )
	{
		if ( bLastInputValidityCheckSuccessful )
		{
			const FString ObjectPath = GetObjectPathForSave();

			bool bProceedWithSave = true;

			// If we were asked to warn on existing assets, do it now
			if ( ExistingAssetPolicy == ESaveAssetDialogExistingAssetPolicy::AllowButWarn )
			{
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				FAssetData ExistingAsset = AssetRegistryModule.Get().GetAssetByObjectPath(FName(*ObjectPath));
				if ( ExistingAsset.IsValid() && AssetClassNames.Contains(ExistingAsset.AssetClass) )
				{
					EAppReturnType::Type ShouldReplace = FMessageDialog::Open( EAppMsgType::YesNo, FText::Format(LOCTEXT("ReplaceAssetMessage", "{ExistingAsset} already exists. Do you want to replace it?"), FText::FromString(CurrentlyEnteredAssetName)) );
					bProceedWithSave = (ShouldReplace == EAppReturnType::Yes);
				}
			}

			if ( bProceedWithSave )
			{
				bValidAssetsChosen = true;
				OnObjectPathChosenForSave.ExecuteIfBound(ObjectPath);
				CloseDialog();
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE