// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InternationalizationSettingsModel.h"

class INTERNATIONALIZATIONSETTINGS_API FInternationalizationSettingsModelDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** Default Constructor */
	FInternationalizationSettingsModelDetails()
		:	RequiresRestart(false)
	{
	}

	/** Destructor */
	~FInternationalizationSettingsModelDetails();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

private:

	void OnSettingsChanged();

	/** Prompt the user to restart the editor */
	void PromptForRestart() const;

	/** Look for available cultures */
	void RefreshAvailableCultures();

	/** Look for available languages */
	void RefreshAvailableLanguages();

	/** Look for available regions */
	void RefreshAvailableRegions();

	/** Delegate called to display the current language */
	FText GetCurrentLanguageText() const;

	/** Generate a widget for the language combo */
	TSharedRef<SWidget> OnLanguageGenerateWidget( FCulturePtr Culture, IDetailLayoutBuilder* DetailBuilder ) const;

	/** Delegate called when the language selection has changed */
	void OnLanguageSelectionChanged( FCulturePtr Culture, ESelectInfo::Type SelectionType );

	/** Generate a widget for the region combo */
	TSharedRef<SWidget> OnRegionGenerateWidget( FCulturePtr Culture, IDetailLayoutBuilder* DetailBuilder ) const;

	/** Delegate called to display the current region */
	FText GetCurrentRegionText() const;

	/** Delegate called when the region selection has changed */
	void OnRegionSelectionChanged( FCulturePtr Culture, ESelectInfo::Type SelectionType );

	/** Delegate called to see if we're allowed to change the region */
	bool IsRegionSelectionAllowed() const;

	/** Delegate called to determine whether to collapse or display the restart row. */
	EVisibility GetInternationalizationRestartRowVisibility() const;

	/** Delegate called when the the checked state of whether or not field names should be localized has changed. */
	void ShouldLoadLocalizedFieldNamesCheckChanged(ECheckBoxState CheckState);

	/** Write to config now the Editor is shutting down (all packages are saved) */
	void HandleShutdownPostPackagesSaved();

	/** Delegate called when the the checked state of whether or not nodes and pins in graph editors should be localized has changed. */
	void ShouldShowNodesAndPinsUnlocalized(ECheckBoxState CheckState);

private:
	TWeakObjectPtr<UInternationalizationSettingsModel> Model;

	/** The culture selected at present */
	FCulturePtr SelectedCulture;

	/** The language selected at present */
	FCulturePtr SelectedLanguage;

	/** The cultures we have available to us */
	TArray< FCulturePtr > AvailableCultures;

	/** The languages we have available to us */
	TArray< FCulturePtr > AvailableLanguages;

	/** The dropdown for the languages */
	TSharedPtr<SComboBox< FCulturePtr > > LanguageComboBox;

	/** The regions we have available to us */
	TArray< FCulturePtr > AvailableRegions;

	/** The dropdown for the regions */
	TSharedPtr<SComboBox< FCulturePtr > > RegionComboBox;

	/** The check box for using localized field names */
	TSharedPtr<SCheckBox> FieldNamesCheckBox;

	bool RequiresRestart;
};