// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** 
* FDlgAnimCompression
* 
* Wrapper class for SAnimationCompressionPanel. This class creates and launches a dialog then awaits the
* result to return to the user. 
*/
class UNREALED_API FDlgAnimCompression
{
public:
	FDlgAnimCompression( TArray<TWeakObjectPtr<UAnimSequence>>& AnimSequences );

	/**  Shows the dialog box and waits for the user to respond. */
	void ShowModal();

private:

	/** Cached pointer to the modal window */
	TSharedPtr<SWindow> DialogWindow;

	/** Cached pointer to the animation compression widget */
	TSharedPtr<class SAnimationCompressionPanel> DialogWidget;
};

/** 
 * Slate panel for Choosing animation compression
 */
class SAnimationCompressionPanel : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SAnimationCompressionPanel)
		{
		}
		/** The anim sequences to compress */
		SLATE_ARGUMENT( TArray<TWeakObjectPtr<UAnimSequence>>, AnimSequences )

		/** Window in which this widget resides */
		SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)

	SLATE_END_ARGS()	

	/**
	 * Constructs this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	virtual ~SAnimationCompressionPanel();

	/**
	 * Handler for the panels "Apply" button
	 */
	FReply					ApplyClicked();

private:
	/**
	 * Apply the selected compression Algorithm to animation sequences supplied to the panel.
	 *
	 * @param	Algorithm	The type of compression to apply to the animation sequences
	 */
	void					ApplyAlgorithm(class UAnimCompress* Algorithm);

	/**
	 * Creates a Slate radio button 
	 *
	 * @param	RadioText	Text label for the radio button
	 * @param	ButtonId	The ID for the radio button
	 * @return				The created radio button widget
	 */
	TSharedRef<SWidget> CreateRadioButton( const FText& RadioText, int32 ButtonId )
	{
		return
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "RadioButton")
			.IsChecked( this, &SAnimationCompressionPanel::IsRadioChecked, ButtonId )
			.OnCheckStateChanged( this, &SAnimationCompressionPanel::OnRadioChanged, ButtonId )
			[
				SNew(STextBlock).Text(RadioText)
			];
	}

	/**
	 * Returns the state of the radio button
	 *
	 * @param	ButtonId	The ID for the radio button
	 * @return				The status of the radio button
	 */
	ECheckBoxState IsRadioChecked( int32 ButtonId ) const
	{
		return (CurrentCompressionChoice == ButtonId)
			? ECheckBoxState::Checked
			: ECheckBoxState::Unchecked;
	}

	/**
	 * Handler for all radio button clicks
	 *
	 * @param	NewRadioState	The new state of the radio button
	 * @param	RadioThatChanged	The ID of the radio button that has changed. 
	 */
	void OnRadioChanged( ECheckBoxState NewRadioState, int32 RadioThatChanged )
	{
		if (NewRadioState == ECheckBoxState::Checked)
		{
			CurrentCompressionChoice = RadioThatChanged;
		}
	}

	TArray<class UAnimCompress*>			AnimationCompressionAlgorithms;
	TArray< TWeakObjectPtr<UAnimSequence> >	AnimSequences;

	int32 CurrentCompressionChoice;

	/** Pointer to the window which holds this Widget, required for modal control */
	TSharedPtr<SWindow> ParentWindow;
};