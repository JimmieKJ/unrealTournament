// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Interface to allow STextPropertyEditableTextBox to be used to edit both properties and Blueprint pins */
class IEditableTextProperty
{
public:
	enum class ETextPropertyEditAction : uint8
	{
		EditedNamespace,
		EditedKey,
		EditedSource,
	};

	virtual ~IEditableTextProperty() {}

	/** Are the text properties being edited marked as multi-line? */
	virtual bool IsMultiLineText() const = 0;

	/** Are the text properties being edited marked as password fields? */
	virtual bool IsPassword() const = 0;

	/** Are the text properties being edited read-only? */
	virtual bool IsReadOnly() const = 0;

	/** Is the value associated with the properties the default value? */
	virtual bool IsDefaultValue() const = 0;

	/** Get the tooltip text associated with the property being edited */
	virtual FText GetToolTipText() const = 0;

	/** Get the number of FText instances being edited by this property */
	virtual int32 GetNumTexts() const = 0;

	/** Get the text at the given index (check against GetNumTexts) */
	virtual FText GetText(const int32 InIndex) const = 0;

	/** Set the text at the given index (check against GetNumTexts) */
	virtual void SetText(const int32 InIndex, const FText& InText) = 0;

#if USE_STABLE_LOCALIZATION_KEYS
	/** Get the stable text ID for the given index (check against GetNumTexts) */
	virtual void GetStableTextId(const int32 InIndex, const ETextPropertyEditAction InEditAction, const FString& InTextSource, const FString& InProposedNamespace, const FString& InProposedKey, FString& OutStableNamespace, FString& OutStableKey) const = 0;
#endif // USE_STABLE_LOCALIZATION_KEYS

	/** Request a refresh of the property UI (eg, due to a size change) */
	virtual void RequestRefresh() = 0;

protected:
#if USE_STABLE_LOCALIZATION_KEYS
	/** Get the localization ID we should use for the given object, and the given text instance */
	EDITORWIDGETS_API static void StaticStableTextId(UObject* InObject, const ETextPropertyEditAction InEditAction, const FString& InTextSource, const FString& InProposedNamespace, const FString& InProposedKey, FString& OutStableNamespace, FString& OutStableKey);

	/** Get the localization ID we should use for the given package, and the given text instance */
	EDITORWIDGETS_API static void StaticStableTextId(UPackage* InPackage, const ETextPropertyEditAction InEditAction, const FString& InTextSource, const FString& InProposedNamespace, const FString& InProposedKey, FString& OutStableNamespace, FString& OutStableKey);
#endif // USE_STABLE_LOCALIZATION_KEYS
};

/** A widget that can be used for editing FText instances */
class EDITORWIDGETS_API STextPropertyEditableTextBox : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(STextPropertyEditableTextBox)
		: _Style(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
		, _Font()
		, _ForegroundColor()
		, _WrapTextAt(0.0f)
		, _AutoWrapText(false)
		, _MinDesiredWidth()
		, _MaxDesiredHeight(300.0f)
		{}
		/** The styling of the textbox */
		SLATE_STYLE_ARGUMENT(FEditableTextBoxStyle, Style)
		/** Font color and opacity (overrides Style) */
		SLATE_ATTRIBUTE(FSlateFontInfo, Font)
		/** Text color and opacity (overrides Style) */
		SLATE_ATTRIBUTE(FSlateColor, ForegroundColor)
		/** Whether text wraps onto a new line when it's length exceeds this width; if this value is zero or negative, no wrapping occurs */
		SLATE_ATTRIBUTE(float, WrapTextAt)
		/** Whether to wrap text automatically based on the widget's computed horizontal space */
		SLATE_ATTRIBUTE(bool, AutoWrapText)
		/** When specified, will report the MinDesiredWidth if larger than the content's desired width */
		SLATE_ATTRIBUTE(FOptionalSize, MinDesiredWidth)
		/** When specified, will report the MaxDesiredHeight if smaller than the content's desired height */
		SLATE_ATTRIBUTE(FOptionalSize, MaxDesiredHeight)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& Arguments, const TSharedRef<IEditableTextProperty>& InEditableTextProperty);
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	void GetDesiredWidth(float& OutMinDesiredWidth, float& OutMaxDesiredWidth);
	bool CanEdit() const;
	bool IsReadOnly() const;
	bool IsIdentityReadOnly() const;
	FText GetToolTipText() const;

	FText GetTextValue() const;
	void OnTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	FText GetNamespaceValue() const;
	void OnNamespaceChanged(const FText& NewText);
	void OnNamespaceCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	FText GetKeyValue() const;
#if USE_STABLE_LOCALIZATION_KEYS
	void OnKeyChanged(const FText& NewText);
	void OnKeyCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	FText GetPackageValue() const;
#endif // USE_STABLE_LOCALIZATION_KEYS

	ECheckBoxState GetLocalizableCheckState(bool bActiveState) const;

	void HandleLocalizableCheckStateChanged(ECheckBoxState InCheckboxState, bool bActiveState);

	EVisibility GetTextWarningImageVisibility() const;

	bool IsValidIdentity(const FText& InIdentity, FText* OutReason = nullptr, const FText* InErrorCtx = nullptr) const;

	TSharedPtr<IEditableTextProperty> EditableTextProperty;

	TSharedPtr<class SWidget> PrimaryWidget;

	TSharedPtr<SMultiLineEditableTextBox> MultiLineWidget;

	TSharedPtr<SEditableTextBox> SingleLineWidget;

	TSharedPtr<SEditableTextBox> NamespaceEditableTextBox;

	TSharedPtr<SEditableTextBox> KeyEditableTextBox;

	TOptional<float> PreviousHeight;

	bool bIsMultiLine;

	static FText MultipleValuesText;
};
