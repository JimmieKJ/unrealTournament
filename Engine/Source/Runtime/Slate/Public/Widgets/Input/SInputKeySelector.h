// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * A widget for selecting keys or input chords.
 */
class SLATE_API SInputKeySelector : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam( FOnKeySelected, FInputChord )
	DECLARE_DELEGATE( FOnIsSelectingKeyChanged )

	SLATE_BEGIN_ARGS( SInputKeySelector )
		: _SelectedKey( FInputChord(EKeys::Invalid) )
		, _ButtonStyle( &FCoreStyle::Get().GetWidgetStyle<FButtonStyle>( "Button" ) )
		, _KeySelectionText( NSLOCTEXT("DefaultKeySelectionText", "InputKeySelector", "...") )
		, _AllowModifierKeys( true )
		, _EscapeCancelsSelection( true )
	{}
		/** The currently selected key */
		SLATE_ATTRIBUTE( FInputChord, SelectedKey )

		/** The font used to display the currently selected key. */
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )

		/** The margin around the selected key text. */
		SLATE_ATTRIBUTE( FMargin, Margin )

		/** The color of the selected key text. */
		SLATE_ATTRIBUTE( FLinearColor, ColorAndOpacity )

		/** The style of the button used to enable key selection. */
		SLATE_STYLE_ARGUMENT( FButtonStyle, ButtonStyle )

		/** The text to display while selecting a new key. */
		SLATE_ARGUMENT( FText, KeySelectionText )

		/** When true modifier keys are captured in the selected key chord, otherwise they are ignored. */
		SLATE_ARGUMENT( bool, AllowModifierKeys )

		/** When true, pressing escape will cancel the key selection, when false, pressing escape will select the escape key. */
		SLATE_ARGUMENT( bool, EscapeCancelsSelection )

		/** Occurs whenever a new key is selected. */
		SLATE_EVENT( FOnKeySelected, OnKeySelected )

		/** Occurs whenever key selection mode starts and stops. */
		SLATE_EVENT( FOnIsSelectingKeyChanged, OnIsSelectingKeyChanged )

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

public:

	/** Gets the currently selected key chord. */
	FInputChord GetSelectedKey() const;

	/** Sets the currently selected key chord. */
	void SetSelectedKey( TAttribute<FInputChord> InSelectedKey );

	/** Sets the font which is used to display the currently selected key. */
	void SetFont( TAttribute<FSlateFontInfo> InFont );

	/** Sets the margin around the text used to display the currently selected key */
	void SetMargin( TAttribute<FMargin> InMargin );

	/** Sets the color of the text used to display the currently selected key. */
	void SetColorAndOpacity( TAttribute<FLinearColor> InColorAndOpacity );

	/** Sets the style of the button which is used enter key selection mode. */
	void SetButtonStyle( const FButtonStyle* ButtonStyle );

	/** Sets the text which is displayed when selecting a key. */
	void SetKeySelectionText( FText InKeySelectionText );

	/** When true modifier keys are captured in the selected key chord, otherwise they are ignored. */
	void SetAllowModifierKeys( bool bInAllowModifierKeys );

	/** Returns true whenever key selection mode is active, otherwise returns false. */
	bool GetIsSelectingKey() const;

public:

	virtual FReply OnPreviewKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnPreviewMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual void OnFocusLost( const FFocusEvent& InFocusEvent ) override;
	virtual bool SupportsKeyboardFocus() const { return true; }

private:

	/** Gets the display text for the currently selected key. */
	FText GetSelectedKeyText() const;

	/** Gets the font used to display the currently selected key. */
	FSlateFontInfo GetFont() const;

	/**  Gets the margin around the text used to display the currently selected key. */
	FMargin GetMargin() const;

	/** Gets the color of the text used to display the currently selected key. */
	FSlateColor GetSlateColorAndOpacity() const;

	/** Handles the OnClicked event from the button which enables key selection mode. */
	FReply OnClicked();

	/** Sets the currently selected key and invokes the associated events. */
	void SelectKey( FKey Key, bool bShiftDown, bool bControllDown, bool bAltDown, bool bCommandDown );

	/** Sets bIsSelectingKey and invokes the associated events. */
	void SetIsSelectingKey(bool bInIsSelectingKey);

private:

	/** True when key selection mode is active. */
	bool bIsSelectingKey;

	/** The currently selected key chord. */
	TAttribute<FInputChord> SelectedKey;

	/** The font for the text used to display the currently selected key. */
	TAttribute<FSlateFontInfo> Font;

	/** The margin around the text used to display the currently selected key. */
	TAttribute<FMargin> Margin;

	/** The text to display when selecting keys. */
	FText KeySelectionText;

	/** When true modifier keys are recorded on the selected key chord, otherwise they are ignored. */
	bool bAllowModifierKeys;

	/** When true, pressing escape will cancel the key selection, when false, pressing escape will select the escape key. */ 
	bool bEscapeCancelsSelection;

	/** The default font used for the text which displays the currently selected key. */
	FSlateFontInfo DefaultFont;

	/** Delegate which is run any time a new key is selected. */
	FOnKeySelected OnKeySelected;

	/** Delegate which is run when key selection mode starts and stops. */
	FOnIsSelectingKeyChanged OnIsSelectingKeyChanged;

	/** The button which starts key key selection mode. */
	TSharedPtr<SButton> Button;
};