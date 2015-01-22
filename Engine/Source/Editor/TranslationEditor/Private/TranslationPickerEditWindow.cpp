// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "TranslationEditorPrivatePCH.h"
#include "TranslationPickerEditWindow.h"
#include "Editor/Documentation/Public/SDocumentationToolTip.h"
#include "TranslationUnit.h"

#define LOCTEXT_NAMESPACE "TranslationPicker"

void STranslationPickerEditWindow::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow;
	PickedTexts = InArgs._PickedTexts;

	WindowContents = SNew(SBox);

	auto TextsBox = SNew(SVerticalBox);

	// Add a new Translation Picker Edit Widget for each picked text
	for (FText PickedText : PickedTexts)
	{
		TSharedPtr<SEditableTextBox> TextBox;
		int32 DefaultPadding = 0.0f;

		TextsBox->AddSlot()
			.FillHeight(1)
			.Padding(FMargin(5))
			[
				SNew(SBorder)
				[
					SNew(STranslationPickerEditWidget)
					.PickedText(PickedText)
				]
			];
	}

	TSharedPtr<SEditableTextBox> TextBox;
	int32 DefaultPadding = 0.0f;

	// Layout the Translation Picker Edit Widgets and some save/close buttons below them
	WindowContents->SetContent(
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1)
			[
				SNew(SBox)
				.Padding(FMargin(8, 5, 8, 5))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.Padding(DefaultPadding)
					.AutoHeight()
					[
						TextsBox
					]
					+ SVerticalBox::Slot()
					.Padding(DefaultPadding)
					.AutoHeight()
					.HAlign(HAlign_Right)
					[
						SNew(SUniformGridPanel)
						.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
						.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
						.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
						+ SUniformGridPanel::Slot(0, 0)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
							.OnClicked(this, &STranslationPickerEditWindow::Close)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.Padding(FMargin(0, 0, 3, 0))
								.VAlign(VAlign_Center)
								.AutoWidth()
								[
									SNew(STextBlock).Text(LOCTEXT("SaveAllAndClose", "Save all and close"))
								]
							]
						]
						+ SUniformGridPanel::Slot(1, 0)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
							.OnClicked(this, &STranslationPickerEditWindow::Close)
							.Text(LOCTEXT("CancelButton", "Cancel"))
						]
					]
				]
			]
		]

	);

	ChildSlot
	[
		WindowContents.ToSharedRef()
	];
}

FReply STranslationPickerEditWindow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		Close();
		
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply STranslationPickerEditWindow::Close()
{
	if (ParentWindow.IsValid())
	{
		FSlateApplication::Get().RequestDestroyWindow(ParentWindow.Pin().ToSharedRef());
		ParentWindow.Reset();
	}

	return FReply::Handled();
}

void STranslationPickerEditWidget::Construct(const FArguments& InArgs)
{
	PickedText = InArgs._PickedText;
	int32 DefaultPadding = 0.0f;

	// Get all the data we need and format it properly
	const FString* NamespaceString = FTextInspector::GetNamespace(PickedText);
	const FString* KeyString = FTextInspector::GetKey(PickedText);
	const FString* SourceString = FTextInspector::GetSourceString(PickedText);
	const FString& TranslationString = FTextInspector::GetDisplayString(PickedText);
	FString LocresFullPath;

	FString ManifestAndArchiveNameString;
	if (NamespaceString && KeyString)
	{
		TSharedPtr<FString, ESPMode::ThreadSafe> TextTableName = TSharedPtr<FString, ESPMode::ThreadSafe>(nullptr);
		TextTableName = FTextLocalizationManager::Get().GetTableName(*NamespaceString, *KeyString);
		LocresFullPath = *TextTableName;
		if (TextTableName.IsValid())
		{
			ManifestAndArchiveNameString = FPaths::GetBaseFilename(*TextTableName);
		}
	}

	FText Namespace = NamespaceString != nullptr ? FText::FromString(*NamespaceString) : FText::GetEmpty();
	FText Key = KeyString != nullptr ? FText::FromString(*KeyString) : FText::GetEmpty();
	FText Source = SourceString != nullptr ? FText::FromString(*SourceString) : FText::GetEmpty();
	FText ManifestAndArchiveName = FText::FromString(ManifestAndArchiveNameString);
	FText Translation = FText::FromString(TranslationString);

	FText SourceWithLabel = FText::Format(LOCTEXT("SourceLabel", "Source: {0}"), Source);
	FText NamespaceWithLabel = FText::Format(LOCTEXT("NamespaceLabel", "Namespace: {0}"), Namespace);
	FText KeyWithLabel = FText::Format(LOCTEXT("KeyLabel", "Key: {0}"), Key);
	FText ManifestAndArchiveNameWithLabel = FText::Format(LOCTEXT("LocresFileLabel", "Manifest/Archive File: {0}"), FText::FromString(ManifestAndArchiveNameString));

	// Save the necessary data in UTranslationUnit for later.  This is what we pass to TranslationDataManager to save our edits
	TranslationUnit = NewObject<UTranslationUnit>();
	TranslationUnit->Namespace = NamespaceString != nullptr ? *NamespaceString : "";
	TranslationUnit->Source = SourceString != nullptr ? *SourceString : "";
	TranslationUnit->Translation = TranslationString;
	TranslationUnit->LocresPath = LocresFullPath;

	// Layout all our data
	ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1)
			.Padding(FMargin(5))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1)
				.Padding(FMargin(5))
				[
					SNew(STextBlock)
					.Text(SourceWithLabel)
				]
				+ SVerticalBox::Slot()
				.FillHeight(1)
				.Padding(FMargin(5))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TranslationLabel", "Translation: "))
					]
					+ SHorizontalBox::Slot()
					[
						SAssignNew(TextBox, SEditableTextBox)
						.Text(Translation)
						.HintText(LOCTEXT("TranslationEditTextBox_HintText", "Enter/edit translation here."))
					]
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1)
			.Padding(FMargin(5))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					[
						SNew(STextBlock)
						.Text(NamespaceWithLabel)
					]
					+ SVerticalBox::Slot()
					[
						SNew(STextBlock)
						.Text(KeyWithLabel)
					]
					+ SVerticalBox::Slot()
					[
						SNew(STextBlock)
						.Text(ManifestAndArchiveNameWithLabel)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5))
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &STranslationPickerEditWidget::SaveAndPreview)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.Padding(FMargin(0, 0, 3, 0))
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SaveAndPreviewButtonText", "Save and preview"))
						]
					]
				]
			]
		];
}

FReply STranslationPickerEditWidget::SaveAndPreview()
{
	// Update translation string from entered text
	TranslationUnit->Translation = TextBox->GetText().ToString();

	// Save the data via translation data manager
	TArray<UTranslationUnit*> TempArray;
	TempArray.Add(TranslationUnit);
	FTranslationDataManager::SaveSelectedTranslations(TempArray);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE