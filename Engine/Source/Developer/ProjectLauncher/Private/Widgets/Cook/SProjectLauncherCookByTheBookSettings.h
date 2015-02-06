// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


namespace EShowMapsChoices
{
	enum Type
	{
		/** Show all available maps. */
		ShowAllMaps,

		/** Only show maps that are to be cooked. */
		ShowCookedMaps
	};
}


/**
 * Implements the cook-by-the-book settings panel.
 */
class SProjectLauncherCookByTheBookSettings
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SProjectLauncherCookByTheBookSettings) { }
	SLATE_END_ARGS()

public:

	/** Destructor. */
	~SProjectLauncherCookByTheBookSettings( );

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param InModel The data model.
	 */
	void Construct(	const FArguments& InArgs, const FProjectLauncherModelRef& InModel, bool InShowSimple = false );

protected:

	/** Refreshes the list of available maps. */
	void RefreshMapList( );

	/** Refreshes the list of available cultures. */
	void RefreshCultureList( );

private:

	/** Handles clicking the 'Select All Cultures' button. */
	void HandleAllCulturesHyperlinkNavigate( bool AllPlatforms );

	/** Handles determining the visibility of the 'Select All Cultures' button. */
	EVisibility HandleAllCulturesHyperlinkVisibility( ) const;

	/** Handles clicking the 'Select All Maps' button. */
	void HandleAllMapsHyperlinkNavigate( bool AllPlatforms );

	/** Handles selecting a build configuration for the cooker. */
	void HandleCookConfigurationSelectorConfigurationSelected( EBuildConfigurations::Type );

	/** Handles getting the content text of the cooker build configuration selector. */
	FText HandleCookConfigurationSelectorText( ) const;

	/** Handles check state changes of the 'Incremental' check box. */
	void HandleIncrementalCheckBoxCheckStateChanged( ECheckBoxState NewState );

	/** Handles determining the checked state of the 'Incremental' check box. */
	ECheckBoxState HandleIncrementalCheckBoxIsChecked( ) const;

	/** Handles generating a row widget in the map list view. */
	TSharedRef<ITableRow> HandleMapListViewGenerateRow( TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable );

	/** Handles generating a row widget in the culture list view. */
	TSharedRef<ITableRow> HandleCultureListViewGenerateRow( TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable );

	/** Handles determining the visibility of the 'Select All Maps' button. */
	EVisibility HandleMapSelectionHyperlinkVisibility( ) const;

	/** Handles getting the visibility of the map selection warning message. */
	EVisibility HandleNoMapSelectedBoxVisibility( ) const;

	/** Handles getting the text in the 'No maps' text block. */
	FText HandleNoMapsTextBlockText( ) const;

	/** Handles changing the selected profile in the profile manager. */
	void HandleProfileManagerProfileSelected( const ILauncherProfilePtr& SelectedProfile, const ILauncherProfilePtr& PreviousProfile );

	/** Handles checking the specified 'Show maps' check box. */
	void HandleShowCheckBoxCheckStateChanged( ECheckBoxState NewState, EShowMapsChoices::Type Choice );

	/** Handles determining the checked state of the specified 'Show maps' check box. */
	ECheckBoxState HandleShowCheckBoxIsChecked( EShowMapsChoices::Type Choice ) const;

	/** Handles checking the specified 'Unversioned' check box. */
	void HandleUnversionedCheckBoxCheckStateChanged( ECheckBoxState NewState );

	/** Handles determining the checked state of the specified 'Unversioned' check box. */
	ECheckBoxState HandleUnversionedCheckBoxIsChecked( ) const;

	/** Handles determining the visibility of a validation error icon. */
	EVisibility HandleValidationErrorIconVisibility( ELauncherProfileValidationErrors::Type Error ) const;

	/** Callback for getting the cookers additional options. */
	FText HandleCookOptionsTextBlockText() const;

	/** Callback for changing the cookers additional options */
	void HandleCookerOptionsCommitted(const FText& NewText, ETextCommit::Type CommitType);

	/** Callback for updating any settings after the selected project has changed in the profile. */
	void HandleProfileProjectChanged();

	// Callback for check state changes of the 'UnrealPak' check box.
	void HandleUnrealPakCheckBoxCheckStateChanged( ECheckBoxState NewState );

	// Callback for determining the checked state of the 'UnrealPak' check box.
	ECheckBoxState HandleUnrealPakCheckBoxIsChecked( ) const;

	/** creates the complex widget. */
	TSharedRef<SWidget> MakeComplexWidget();

	/** creates the simple widget. */
	TSharedRef<SWidget> MakeSimpleWidget();

private:

	/** Holds the culture list. */
	TArray<TSharedPtr<FString> > CultureList;

	/** Holds the map list view. */
	TSharedPtr<SListView<TSharedPtr<FString> > > CultureListView;

	/** Holds the map list. */
	TArray<TSharedPtr<FString> > MapList;

	/** Holds the map list view. */
	TSharedPtr<SListView<TSharedPtr<FString> > > MapListView;

	/** Holds a pointer to the data model. */
	FProjectLauncherModelPtr Model;

	/** Holds the current 'Show maps' check box choice. */
	EShowMapsChoices::Type ShowMapsChoice;
};
