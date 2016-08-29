// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EditorWidgetsPrivatePCH.h"
#include "STextPropertyEditableTextBox.h"
#include "TextPackageNamespaceUtil.h"
#include "TextReferenceCollector.h"

#define LOCTEXT_NAMESPACE "STextPropertyEditableTextBox"

FText STextPropertyEditableTextBox::MultipleValuesText(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"));

#if USE_STABLE_LOCALIZATION_KEYS

void IEditableTextProperty::StaticStableTextId(UObject* InObject, const ETextPropertyEditAction InEditAction, const FString& InTextSource, const FString& InProposedNamespace, const FString& InProposedKey, FString& OutStableNamespace, FString& OutStableKey)
{
	UPackage* Package = InObject ? InObject->GetOutermost() : nullptr;
	StaticStableTextId(Package, InEditAction, InTextSource, InProposedNamespace, InProposedKey, OutStableNamespace, OutStableKey);
}

void IEditableTextProperty::StaticStableTextId(UPackage* InPackage, const ETextPropertyEditAction InEditAction, const FString& InTextSource, const FString& InProposedNamespace, const FString& InProposedKey, FString& OutStableNamespace, FString& OutStableKey)
{
	bool bPersistKey = false;

	const FString PackageNamespace = TextNamespaceUtil::EnsurePackageNamespace(InPackage);
	if (!PackageNamespace.IsEmpty())
	{
		// Make sure the proposed namespace is using the correct namespace for this package
		OutStableNamespace = TextNamespaceUtil::BuildFullNamespace(InProposedNamespace, PackageNamespace, /*bAlwaysApplyPackageNamespace*/true);

		if (InProposedNamespace.Equals(OutStableNamespace, ESearchCase::CaseSensitive) || InEditAction == ETextPropertyEditAction::EditedNamespace)
		{
			// If the proposal was already using the correct namespace (or we just set the namespace), attempt to persist the proposed key too
			if (!InProposedKey.IsEmpty())
			{
				// If we changed the source text, then we can persist the key if this text is the *only* reference using that ID
				// If we changed the identifier, then we can persist the key only if doing so won't cause an identify conflict
				const FTextReferenceCollector::EComparisonMode ReferenceComparisonMode = InEditAction == ETextPropertyEditAction::EditedSource ? FTextReferenceCollector::EComparisonMode::MatchId : FTextReferenceCollector::EComparisonMode::MismatchSource;
				const int32 RequiredReferenceCount = InEditAction == ETextPropertyEditAction::EditedSource ? 1 : 0;

				int32 ReferenceCount = 0;
				FTextReferenceCollector(InPackage, ReferenceComparisonMode, OutStableNamespace, InProposedKey, InTextSource, ReferenceCount);

				if (ReferenceCount == RequiredReferenceCount)
				{
					bPersistKey = true;
					OutStableKey = InProposedKey;
				}
			}
		}
		else if (InEditAction != ETextPropertyEditAction::EditedNamespace)
		{
			// If our proposed namespace wasn't correct for our package, and we didn't just set it (which doesn't include the package namespace)
			// then we should clear out any user specified part of it
			OutStableNamespace = TextNamespaceUtil::BuildFullNamespace(FString(), PackageNamespace, /*bAlwaysApplyPackageNamespace*/true);
		}
	}

	if (!bPersistKey)
	{
		OutStableKey = FGuid::NewGuid().ToString();
	}
}

#endif // USE_STABLE_LOCALIZATION_KEYS

void STextPropertyEditableTextBox::Construct(const FArguments& InArgs, const TSharedRef<IEditableTextProperty>& InEditableTextProperty)
{
	EditableTextProperty = InEditableTextProperty;

	TSharedPtr<SHorizontalBox> HorizontalBox;

	const bool bIsPassword = EditableTextProperty->IsPassword();
	bIsMultiLine = EditableTextProperty->IsMultiLineText();
	if (bIsMultiLine)
	{
		ChildSlot
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SBox)
				.MinDesiredWidth(InArgs._MinDesiredWidth)
				.MaxDesiredHeight(InArgs._MaxDesiredHeight)
				[
					SAssignNew(MultiLineWidget, SMultiLineEditableTextBox)
					.Text(this, &STextPropertyEditableTextBox::GetTextValue)
					.ToolTipText(this, &STextPropertyEditableTextBox::GetToolTipText)
					.Style(InArgs._Style)
					.Font(InArgs._Font)
					.ForegroundColor(InArgs._ForegroundColor)
					.SelectAllTextWhenFocused(false)
					.ClearKeyboardFocusOnCommit(false)
					.OnTextCommitted(this, &STextPropertyEditableTextBox::OnTextCommitted)
					.SelectAllTextOnCommit(false)
					.IsReadOnly(this, &STextPropertyEditableTextBox::IsReadOnly)
					.AutoWrapText(InArgs._AutoWrapText)
					.WrapTextAt(InArgs._WrapTextAt)
					.ModiferKeyForNewLine(EModifierKey::Shift)
					.IsPassword(bIsPassword)
				]
			]
		];

		PrimaryWidget = MultiLineWidget;
	}
	else
	{
		ChildSlot
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SBox)
				.MinDesiredWidth(InArgs._MinDesiredWidth)
				[
					SAssignNew(SingleLineWidget, SEditableTextBox)
					.Text(this, &STextPropertyEditableTextBox::GetTextValue)
					.ToolTipText(this, &STextPropertyEditableTextBox::GetToolTipText)
					.Style(InArgs._Style)
					.Font(InArgs._Font)
					.ForegroundColor(InArgs._ForegroundColor)
					.SelectAllTextWhenFocused(true)
					.ClearKeyboardFocusOnCommit(false)
					.OnTextCommitted(this, &STextPropertyEditableTextBox::OnTextCommitted)
					.SelectAllTextOnCommit(true)
					.IsReadOnly(this, &STextPropertyEditableTextBox::IsReadOnly)
					.IsPassword(bIsPassword)
				]
			]
		];

		PrimaryWidget = SingleLineWidget;
	}

	HorizontalBox->AddSlot()
		.AutoWidth()
		[
			SNew(SComboButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(FMargin(4, 0))
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.ForegroundColor(FSlateColor::UseForeground())
			.ToolTipText(LOCTEXT("AdvancedTextSettingsComboToolTip", "Edit advanced text settings."))
			.MenuContent()
			[
				SNew(SBox)
				.WidthOverride(340)
				.Padding(4)
				[
					SNew(SGridPanel)
					.FillColumn(1, 1.0f)

					// Localizable?
					+SGridPanel::Slot(0, 0)
					.Padding(2)
					.HAlign(HAlign_Right)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TextLocalizableLabel", "Localizable:"))
					]
					+SGridPanel::Slot(1, 0)
					.Padding(2)
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0)
						[
							SNew(SUniformGridPanel)
							.SlotPadding(FMargin(0, 0, 4, 0))

							+SUniformGridPanel::Slot(0, 0)
							[
								SNew(SCheckBox)
								.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
								.ToolTipText(LOCTEXT("TextLocalizableToggleYesToolTip", "Assign this text a key and allow it to be gathered for localization."))
								.Padding(FMargin(4, 2))
								.HAlign(HAlign_Center)
								.IsChecked(this, &STextPropertyEditableTextBox::GetLocalizableCheckState, true/*bActiveState*/)
								.OnCheckStateChanged(this, &STextPropertyEditableTextBox::HandleLocalizableCheckStateChanged, true/*bActiveState*/)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("TextLocalizableToggleYes", "Yes"))
								]
							]

							+SUniformGridPanel::Slot(1, 0)
							[
								SNew(SCheckBox)
								.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
								.ToolTipText(LOCTEXT("TextLocalizableToggleNoToolTip", "Mark this text as 'culture invariant' to prevent it being gathered for localization."))
								.Padding(FMargin(4, 2))
								.HAlign(HAlign_Center)
								.IsChecked(this, &STextPropertyEditableTextBox::GetLocalizableCheckState, false/*bActiveState*/)
								.OnCheckStateChanged(this, &STextPropertyEditableTextBox::HandleLocalizableCheckStateChanged, false/*bActiveState*/)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("TextLocalizableToggleNo", "No"))
								]
							]
						]
					]

#if USE_STABLE_LOCALIZATION_KEYS
					// Package
					+SGridPanel::Slot(0, 1)
					.Padding(2)
					.HAlign(HAlign_Right)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TextPackageLabel", "Package:"))
					]
					+SGridPanel::Slot(1, 1)
					.Padding(2)
					[
						SNew(SEditableTextBox)
						.Text(this, &STextPropertyEditableTextBox::GetPackageValue)
						.IsReadOnly(true)
					]
#endif // USE_STABLE_LOCALIZATION_KEYS

					// Namespace
					+SGridPanel::Slot(0, 2)
					.Padding(2)
					.HAlign(HAlign_Right)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TextNamespaceLabel", "Namespace:"))
					]
					+SGridPanel::Slot(1, 2)
					.Padding(2)
					[
						SAssignNew(NamespaceEditableTextBox, SEditableTextBox)
						.Text(this, &STextPropertyEditableTextBox::GetNamespaceValue)
						.SelectAllTextWhenFocused(true)
						.ClearKeyboardFocusOnCommit(false)
						.OnTextChanged(this, &STextPropertyEditableTextBox::OnNamespaceChanged)
						.OnTextCommitted(this, &STextPropertyEditableTextBox::OnNamespaceCommitted)
						.SelectAllTextOnCommit(true)
						.IsReadOnly(this, &STextPropertyEditableTextBox::IsIdentityReadOnly)
					]

					// Key
					+SGridPanel::Slot(0, 3)
					.Padding(2)
					.HAlign(HAlign_Right)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TextKeyLabel", "Key:"))
					]
					+SGridPanel::Slot(1, 3)
					.Padding(2)
					[
						SAssignNew(KeyEditableTextBox, SEditableTextBox)
						.Text(this, &STextPropertyEditableTextBox::GetKeyValue)
#if USE_STABLE_LOCALIZATION_KEYS
						.SelectAllTextWhenFocused(true)
						.ClearKeyboardFocusOnCommit(false)
						.OnTextChanged(this, &STextPropertyEditableTextBox::OnKeyChanged)
						.OnTextCommitted(this, &STextPropertyEditableTextBox::OnKeyCommitted)
						.SelectAllTextOnCommit(true)
						.IsReadOnly(this, &STextPropertyEditableTextBox::IsIdentityReadOnly)
#else	// USE_STABLE_LOCALIZATION_KEYS
						.IsReadOnly(true)
#endif	// USE_STABLE_LOCALIZATION_KEYS
					]
				]
			]
		];

	HorizontalBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.AutoWidth()
		[
			SNew(SImage)
			.Image(FCoreStyle::Get().GetBrush("Icons.Warning"))
			.Visibility(this, &STextPropertyEditableTextBox::GetTextWarningImageVisibility)
			.ToolTipText(LOCTEXT("TextNotLocalizedWarningToolTip", "This text is marked as 'culture invariant' and won't be gathered for localization.\nYou can change this by editing the advanced text settings."))
		];

	SetEnabled(TAttribute<bool>(this, &STextPropertyEditableTextBox::CanEdit));
}

void STextPropertyEditableTextBox::GetDesiredWidth(float& OutMinDesiredWidth, float& OutMaxDesiredWidth)
{
	if (bIsMultiLine)
	{
		OutMinDesiredWidth = 250.0f;
	}
	else
	{
		OutMinDesiredWidth = 125.0f;
	}

	OutMaxDesiredWidth = 600.0f;
}

bool STextPropertyEditableTextBox::SupportsKeyboardFocus() const
{
	return PrimaryWidget.IsValid() && PrimaryWidget->SupportsKeyboardFocus();
}

FReply STextPropertyEditableTextBox::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	// Forward keyboard focus to our editable text widget
	return FReply::Handled().SetUserFocus(PrimaryWidget.ToSharedRef(), InFocusEvent.GetCause());
}

void STextPropertyEditableTextBox::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	const float CurrentHeight = AllottedGeometry.GetLocalSize().Y;
	if (bIsMultiLine && PreviousHeight.IsSet() && PreviousHeight.GetValue() != CurrentHeight)
	{
		EditableTextProperty->RequestRefresh();
	}
	PreviousHeight = CurrentHeight;
}

bool STextPropertyEditableTextBox::CanEdit() const
{
	return !EditableTextProperty->IsReadOnly();
}

bool STextPropertyEditableTextBox::IsReadOnly() const
{
	return EditableTextProperty->IsReadOnly();
}

bool STextPropertyEditableTextBox::IsIdentityReadOnly() const
{
	if (EditableTextProperty->IsReadOnly())
	{
		return true;
	}

	// We can't edit the identity of culture invariant texts
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText TextValue = EditableTextProperty->GetText(0);
		if (TextValue.IsCultureInvariant())
		{
			return true;
		}
	}

	return false;
}

FText STextPropertyEditableTextBox::GetToolTipText() const
{
	FText LocalizedTextToolTip;
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText TextValue = EditableTextProperty->GetText(0);

		bool bIsLocalized = false;
		FString Namespace;
		FString Key;
		const FString* SourceString = FTextInspector::GetSourceString(TextValue);

		if (SourceString && TextValue.ShouldGatherForLocalization())
		{
			bIsLocalized = FTextLocalizationManager::Get().FindNamespaceAndKeyFromDisplayString(FTextInspector::GetSharedDisplayString(TextValue), Namespace, Key);
		}

		if (bIsLocalized)
		{
			LocalizedTextToolTip = FText::Format(LOCTEXT("LocalizedTextToolTipFmt", "--- Localized Text ---\nNamespace: {0}\nKey: {1}\nSource: {2}"), FText::FromString(Namespace), FText::FromString(Key), FText::FromString(*SourceString));
		}
	}
	
	const FText BaseToolTipText = EditableTextProperty->GetToolTipText();
	if (LocalizedTextToolTip.IsEmptyOrWhitespace())
	{
		return BaseToolTipText;
	}
	if (BaseToolTipText.IsEmptyOrWhitespace())
	{
		return LocalizedTextToolTip;
	}

	return FText::Format(LOCTEXT("ToolTipCompleteFmt", "{0}\n\n{1}"), BaseToolTipText, LocalizedTextToolTip);
}

FText STextPropertyEditableTextBox::GetTextValue() const
{
	FText TextValue;

	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		TextValue = EditableTextProperty->GetText(0);
	}
	else if (NumTexts > 1)
	{
		TextValue = MultipleValuesText;
	}

	return TextValue;
}

void STextPropertyEditableTextBox::OnTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();

	// Don't commit the Multiple Values text if there are multiple properties being set
	if (NumTexts > 0 && (NumTexts == 1 || NewText.ToString() != MultipleValuesText.ToString()))
	{
		const FString& SourceString = NewText.ToString();
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			const FText PropertyValue = EditableTextProperty->GetText(TextIndex);

			// Only apply the change if the new text is different
			if (PropertyValue.ToString().Equals(NewText.ToString(), ESearchCase::CaseSensitive))
			{
				continue;
			}

			// Maintain culture invariance when editing the text
			if (PropertyValue.IsCultureInvariant())
			{
				EditableTextProperty->SetText(TextIndex, FText::AsCultureInvariant(NewText.ToString()));
				continue;
			}

			FString NewNamespace;
			FString NewKey;
#if USE_STABLE_LOCALIZATION_KEYS
			{
				// Get the stable namespace and key that we should use for this property
				const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
				EditableTextProperty->GetStableTextId(
					TextIndex, 
					IEditableTextProperty::ETextPropertyEditAction::EditedSource, 
					TextSource ? *TextSource : FString(), 
					FTextInspector::GetNamespace(PropertyValue).Get(FString()), 
					FTextInspector::GetKey(PropertyValue).Get(FString()), 
					NewNamespace, 
					NewKey
					);
			}
#else	// USE_STABLE_LOCALIZATION_KEYS
			{
				// We want to preserve the namespace set on this property if it's *not* the default value
				if (!EditableTextProperty->IsDefaultValue())
				{
					// Some properties report that they're not the default, but still haven't been set from a property, so we also check the property key to see if it's a valid GUID before allowing the namespace to persist
					FGuid TmpGuid;
					if (FGuid::Parse(FTextInspector::GetKey(PropertyValue).Get(FString()), TmpGuid))
					{
						NewNamespace = FTextInspector::GetNamespace(PropertyValue).Get(FString());
					}
				}

				NewKey = FGuid::NewGuid().ToString();
			}
#endif	// USE_STABLE_LOCALIZATION_KEYS

			EditableTextProperty->SetText(TextIndex, FText::ChangeKey(NewNamespace, NewKey, NewText));
		}
	}
}

FText STextPropertyEditableTextBox::GetNamespaceValue() const
{
	FText NamespaceValue;

	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);
		TOptional<FString> FoundNamespace = FTextInspector::GetNamespace(PropertyValue);
		if (FoundNamespace.IsSet())
		{
			NamespaceValue = FText::FromString(TextNamespaceUtil::StripPackageNamespace(FoundNamespace.GetValue()));
		}
	}
	else if (NumTexts > 1)
	{
		NamespaceValue = MultipleValuesText;
	}

	return NamespaceValue;
}

void STextPropertyEditableTextBox::OnNamespaceChanged(const FText& NewText)
{
	FText ErrorMessage;
	const FText ErrorCtx = LOCTEXT("TextNamespaceErrorCtx", "Namespace");
	IsValidIdentity(NewText, &ErrorMessage, &ErrorCtx);

	NamespaceEditableTextBox->SetError(ErrorMessage);
}

void STextPropertyEditableTextBox::OnNamespaceCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (!IsValidIdentity(NewText))
	{
		return;
	}

	const int32 NumTexts = EditableTextProperty->GetNumTexts();

	// Don't commit the Multiple Values text if there are multiple properties being set
	if (NumTexts > 0 && (NumTexts == 1 || NewText.ToString() != MultipleValuesText.ToString()))
	{
		const FString& TextNamespace = NewText.ToString();
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			const FText PropertyValue = EditableTextProperty->GetText(TextIndex);

			// Only apply the change if the new namespace is different - we want to keep the keys stable where possible
			const FString CurrentTextNamespace = TextNamespaceUtil::StripPackageNamespace(FTextInspector::GetNamespace(PropertyValue).Get(FString()));
			if (CurrentTextNamespace.Equals(TextNamespace, ESearchCase::CaseSensitive))
			{
				continue;
			}

			// Get the stable namespace and key that we should use for this property
			FString NewNamespace;
			FString NewKey;
#if USE_STABLE_LOCALIZATION_KEYS
			{
				const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
				EditableTextProperty->GetStableTextId(
					TextIndex, 
					IEditableTextProperty::ETextPropertyEditAction::EditedNamespace, 
					TextSource ? *TextSource : FString(), 
					TextNamespace, 
					FTextInspector::GetKey(PropertyValue).Get(FString()), 
					NewNamespace, 
					NewKey
					);
			}
#else	// USE_STABLE_LOCALIZATION_KEYS
			{
				NewNamespace = TextNamespace;

				// If the current key is a GUID, then we can preserve that when setting the new namespace
				NewKey = FTextInspector::GetKey(PropertyValue).Get(FString());
				{
					FGuid TmpGuid;
					if (!FGuid::Parse(NewKey, TmpGuid))
					{
						NewKey = FGuid::NewGuid().ToString();
					}
				}
			}
#endif	// USE_STABLE_LOCALIZATION_KEYS

			EditableTextProperty->SetText(TextIndex, FText::ChangeKey(NewNamespace, NewKey, PropertyValue));
		}
	}
}

FText STextPropertyEditableTextBox::GetKeyValue() const
{
	FText KeyValue;

	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);
		TOptional<FString> FoundKey = FTextInspector::GetKey(PropertyValue);
		if (FoundKey.IsSet())
		{
			KeyValue = FText::FromString(FoundKey.GetValue());
		}
	}
	else if (NumTexts > 1)
	{
		KeyValue = MultipleValuesText;
	}

	return KeyValue;
}

#if USE_STABLE_LOCALIZATION_KEYS

void STextPropertyEditableTextBox::OnKeyChanged(const FText& NewText)
{
	FText ErrorMessage;
	const FText ErrorCtx = LOCTEXT("TextKeyErrorCtx", "Key");
	const bool bIsValidName = IsValidIdentity(NewText, &ErrorMessage, &ErrorCtx);

	if (NewText.IsEmptyOrWhitespace())
	{
		ErrorMessage = LOCTEXT("TextKeyEmptyErrorMsg", "Key cannot be empty so a new key will be assigned");
	}
	else if (bIsValidName)
	{
		// Valid name, so check it won't cause an identity conflict (only test if we have a single text selected to avoid confusion)
		const int32 NumTexts = EditableTextProperty->GetNumTexts();
		if (NumTexts == 1)
		{
			const FText PropertyValue = EditableTextProperty->GetText(0);

			const FString TextNamespace = FTextInspector::GetNamespace(PropertyValue).Get(FString());
			const FString TextKey = NewText.ToString();

			// Get the stable namespace and key that we should use for this property
			// If it comes back with the same namespace but a different key then it means there was an identity conflict
			FString NewNamespace;
			FString NewKey;
			const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
			EditableTextProperty->GetStableTextId(
				0,
				IEditableTextProperty::ETextPropertyEditAction::EditedKey,
				TextSource ? *TextSource : FString(),
				TextNamespace,
				TextKey,
				NewNamespace,
				NewKey
				);

			if (TextNamespace.Equals(NewNamespace, ESearchCase::CaseSensitive) && !TextKey.Equals(NewKey, ESearchCase::CaseSensitive))
			{
				ErrorMessage = LOCTEXT("TextKeyConflictErrorMsg", "Identity (namespace & key) is being used by a different text within this package so a new key will be assigned");
			}
		}
	}

	KeyEditableTextBox->SetError(ErrorMessage);
}

void STextPropertyEditableTextBox::OnKeyCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (!IsValidIdentity(NewText))
	{
		return;
	}

	const int32 NumTexts = EditableTextProperty->GetNumTexts();

	// Don't commit the Multiple Values text if there are multiple properties being set
	if (NumTexts > 0 && (NumTexts == 1 || NewText.ToString() != MultipleValuesText.ToString()))
	{
		const FString& TextKey = NewText.ToString();
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			const FText PropertyValue = EditableTextProperty->GetText(TextIndex);

			// Only apply the change if the new key is different - we want to keep the keys stable where possible
			const FString CurrentTextKey = FTextInspector::GetKey(PropertyValue).Get(FString());
			if (CurrentTextKey.Equals(TextKey, ESearchCase::CaseSensitive))
			{
				continue;
			}

			// Get the stable namespace and key that we should use for this property
			FString NewNamespace;
			FString NewKey;
			const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
			EditableTextProperty->GetStableTextId(
				TextIndex, 
				IEditableTextProperty::ETextPropertyEditAction::EditedKey, 
				TextSource ? *TextSource : FString(), 
				FTextInspector::GetNamespace(PropertyValue).Get(FString()), 
				TextKey, 
				NewNamespace, 
				NewKey
				);

			EditableTextProperty->SetText(TextIndex, FText::ChangeKey(NewNamespace, NewKey, PropertyValue));
		}
	}
}

FText STextPropertyEditableTextBox::GetPackageValue() const
{
	FText PackageValue;

	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);
		TOptional<FString> FoundNamespace = FTextInspector::GetNamespace(PropertyValue);
		if (FoundNamespace.IsSet())
		{
			PackageValue = FText::FromString(TextNamespaceUtil::ExtractPackageNamespace(FoundNamespace.GetValue()));
		}
	}
	else if (NumTexts > 1)
	{
		PackageValue = MultipleValuesText;
	}

	return PackageValue;
}

#endif // USE_STABLE_LOCALIZATION_KEYS

ECheckBoxState STextPropertyEditableTextBox::GetLocalizableCheckState(bool bActiveState) const
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);

		const bool bIsLocalized = !PropertyValue.IsCultureInvariant();
		return bIsLocalized == bActiveState ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Undetermined;
}

void STextPropertyEditableTextBox::HandleLocalizableCheckStateChanged(ECheckBoxState InCheckboxState, bool bActiveState)
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();

	if (bActiveState)
	{
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			const FText PropertyValue = EditableTextProperty->GetText(TextIndex);

			// Assign a key to any currently culture invariant texts
			if (PropertyValue.IsCultureInvariant())
			{
				// Get the stable namespace and key that we should use for this property
				FString NewNamespace;
				FString NewKey;
				EditableTextProperty->GetStableTextId(
					TextIndex,
					IEditableTextProperty::ETextPropertyEditAction::EditedKey,
					PropertyValue.ToString(),
					FString(),
					FString(),
					NewNamespace,
					NewKey
					);

				EditableTextProperty->SetText(TextIndex, FInternationalization::Get().ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(*PropertyValue.ToString(), *NewNamespace, *NewKey));
			}
		}
	}
	else
	{
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			const FText PropertyValue = EditableTextProperty->GetText(TextIndex);

			// Clear the identity from any non-culture invariant texts
			if (!PropertyValue.IsCultureInvariant())
			{
				const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
				EditableTextProperty->SetText(TextIndex, FText::AsCultureInvariant(PropertyValue.ToString()));
			}
		}
	}
}

EVisibility STextPropertyEditableTextBox::GetTextWarningImageVisibility() const
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	
	if (NumTexts == 1)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);
		return PropertyValue.IsCultureInvariant() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return EVisibility::Collapsed;
}

bool STextPropertyEditableTextBox::IsValidIdentity(const FText& InIdentity, FText* OutReason, const FText* InErrorCtx) const
{
	const FString InvalidIdentityChars = FString::Printf(TEXT("%s%c%c"), INVALID_NAME_CHARACTERS, TextNamespaceUtil::PackageNamespaceStartMarker, TextNamespaceUtil::PackageNamespaceEndMarker);
	return FName::IsValidXName(InIdentity.ToString(), InvalidIdentityChars, OutReason, InErrorCtx);
}

#undef LOCTEXT_NAMESPACE
