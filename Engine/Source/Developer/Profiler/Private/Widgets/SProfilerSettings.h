// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Widget used to modify settings for the profiler, created on demand and destroyed on close. */
class SProfilerSettings : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SProfilerSettings ){}
		SLATE_EVENT( FSimpleDelegate, OnClose )
		SLATE_ARGUMENT( FProfilerSettings*, SettingPtr )
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

private:
	void AddTitle( const FText& TitleText, const TSharedRef<SGridPanel>& Grid, int32& RowPos );

	void AddSeparator( const TSharedRef<SGridPanel>& Grid, int32& RowPos );

	void AddHeader( const FText& HeaderText, const TSharedRef<SGridPanel>& Grid, int32& RowPos);

	void AddOption( const FText& OptionName, const FText& OptionDesc, bool& Value, const bool& Default, const TSharedRef<SGridPanel>& Grid, int32& RowPos );

	void AddFooter( const TSharedRef<SGridPanel>& Grid, int32& RowPos );

	FReply SaveAndClose_OnClicked();
	FReply ResetToDefaults_OnClicked();

	EVisibility OptionDefault_GetDiffersFromDefaultAsVisibility( const bool* ValuePtr, const bool* DefaultPtr ) const;
	FReply OptionDefault_OnClicked( bool* ValuePtr, const bool* DefaultPtr );

	void OptionValue_OnCheckStateChanged( ECheckBoxState CheckBoxState, bool* ValuePtr );
	ECheckBoxState OptionValue_IsChecked( const bool* ValuePtr ) const;

private:
	/** Delegate to call when this profiler settings is closed. */
	FSimpleDelegate OnClose;

	/** Pointer to the profiler settings. */
	FProfilerSettings* SettingPtr;
};