// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "InternationalizationSettingsModulePrivatePCH.h"
#include "EdGraph/EdGraphSchema.h"

#define LOCTEXT_NAMESPACE "InternationalizationSettingsModelDetails"

/** Functions for sorting the languages */
struct FCompareCultureByNativeLanguage
{
	static FText GetCultureNativeLanguageText( const FCulturePtr Culture )
	{
		check( Culture.IsValid() );
		const FString Language = Culture->GetNativeLanguage();
		return FText::FromString(Language);
	}

	FORCEINLINE bool operator()( const FCulturePtr A, const FCulturePtr B ) const
	{
		check( A.IsValid() );
		check( B.IsValid() );
		return( GetCultureNativeLanguageText( A ).CompareToCaseIgnored( GetCultureNativeLanguageText( B ) ) ) < 0;
	}
};


/** Functions for sorting the regions */
struct FCompareCultureByNativeRegion
{
	static FText GetCultureNativeRegionText( const FCulturePtr Culture )
	{
		check( Culture.IsValid() );
		FString Region = Culture->GetNativeRegion();
		if ( Region.IsEmpty() )
		{
			// Fallback to displaying the language, if no region is available
			return LOCTEXT("NoSpecificRegionOption", "Non-Specific Region");
		}
		return FText::FromString(Region);
	}

	FORCEINLINE bool operator()( const FCulturePtr A, const FCulturePtr B ) const
	{
		check( A.IsValid() );
		check( B.IsValid() );
		// Non-Specific Region should appear before all else.
		if(A->GetNativeRegion().IsEmpty())
		{
			return true;
		}
		// Non-Specific Region should appear before all else.
		if(B->GetNativeRegion().IsEmpty())
		{
			return false;
		}
		// Compare native region strings.
		return( GetCultureNativeRegionText( A ).CompareToCaseIgnored( GetCultureNativeRegionText( B ) ) ) < 0;
	}
};

TSharedRef<IDetailCustomization> FInternationalizationSettingsModelDetails::MakeInstance()
{
	TSharedRef<FInternationalizationSettingsModelDetails> InternationalizationSettingsModelDetails = MakeShareable(new FInternationalizationSettingsModelDetails());
	return InternationalizationSettingsModelDetails;
}

FInternationalizationSettingsModelDetails::~FInternationalizationSettingsModelDetails()
{
	check(Model.IsValid());
	Model->OnSettingChanged().RemoveAll(this);
}

void FInternationalizationSettingsModelDetails::OnSettingsChanged()
{
	FInternationalization& I18N = FInternationalization::Get();

	check(Model.IsValid());
	FString SavedCultureName = Model->GetCultureName();
	if ( !SavedCultureName.IsEmpty() && SavedCultureName != I18N.GetCurrentCulture()->GetName() )
	{
		SelectedCulture = I18N.GetCulture(SavedCultureName);
		RequiresRestart = true;
	}
	else
	{
		SelectedCulture = I18N.GetCurrentCulture();
		RequiresRestart = false;
	}
	RefreshAvailableLanguages();
	LanguageComboBox->SetSelectedItem(SelectedLanguage);
	RefreshAvailableRegions();
	RegionComboBox->SetSelectedItem(SelectedCulture);
}

void FInternationalizationSettingsModelDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	FInternationalization& I18N = FInternationalization::Get();

	TArray< TWeakObjectPtr<UObject> > ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	check(ObjectsBeingCustomized.Num() == 1);

	if(ObjectsBeingCustomized[0].IsValid())
	{
		Model = Cast<UInternationalizationSettingsModel>(ObjectsBeingCustomized[0].Get());
	}
	check(Model.IsValid());

	Model->OnSettingChanged().AddRaw(this, &FInternationalizationSettingsModelDetails::OnSettingsChanged);

	// If the saved culture is not the same as the actual current culture, a restart is needed to sync them fully and properly.
	FString SavedCultureName = Model->GetCultureName();
	if ( !SavedCultureName.IsEmpty() && SavedCultureName != I18N.GetCurrentCulture()->GetName() )
	{
		SelectedCulture = I18N.GetCulture(SavedCultureName);
		RequiresRestart = true;
	}
	else
	{
		SelectedCulture = I18N.GetCurrentCulture();
		RequiresRestart = false;
	}

	// Populate all our cultures
	RefreshAvailableCultures();

	IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory("Internationalization");

	const FText LanguageToolTipText = LOCTEXT("EditorLanguageTooltip", "Change the Editor language (requires restart to take effect)");

	// For use in the Slate macros below, the type must be typedef'd to compile.
	typedef FCulturePtr ThreadSafeCulturePtr;

	CategoryBuilder.AddCustomRow(LOCTEXT("EditorLanguageLabel", "Language"))
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("EditorLanguageLabel", "Language"))
			.Font(DetailBuilder.GetDetailFont())
			.ToolTipText(LanguageToolTipText)
		]
	]
	.ValueContent()
	.MaxDesiredWidth(300.0f)
	[
		SAssignNew(LanguageComboBox, SComboBox< ThreadSafeCulturePtr > )
		.OptionsSource( &AvailableLanguages )
		.InitiallySelectedItem(SelectedLanguage)
		.OnGenerateWidget(this, &FInternationalizationSettingsModelDetails::OnLanguageGenerateWidget, &DetailBuilder)
		.ToolTipText(LanguageToolTipText)
		.OnSelectionChanged(this, &FInternationalizationSettingsModelDetails::OnLanguageSelectionChanged)
		.Content()
		[
			SNew(STextBlock)
			.Text(this, &FInternationalizationSettingsModelDetails::GetCurrentLanguageText)
			.Font(DetailBuilder.GetDetailFont())
		]
	];

	const FText RegionToolTipText = LOCTEXT("EditorRegionTooltip", "Change the Editor region (requires restart to take effect)");

	CategoryBuilder.AddCustomRow(LOCTEXT("EditorRegionLabel", "Region"))
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("EditorRegionLabel", "Region"))
			.Font(DetailBuilder.GetDetailFont())
			.ToolTipText(RegionToolTipText)
		]
	]
	.ValueContent()
	.MaxDesiredWidth(300.0f)
	[
		SAssignNew(RegionComboBox, SComboBox< ThreadSafeCulturePtr > )
		.OptionsSource( &AvailableRegions )
		.InitiallySelectedItem(SelectedCulture)
		.OnGenerateWidget(this, &FInternationalizationSettingsModelDetails::OnRegionGenerateWidget, &DetailBuilder)
		.ToolTipText(RegionToolTipText)
		.OnSelectionChanged(this, &FInternationalizationSettingsModelDetails::OnRegionSelectionChanged)
		.IsEnabled(this, &FInternationalizationSettingsModelDetails::IsRegionSelectionAllowed)
		.Content()
		[
			SNew(STextBlock)
			.Text(this, &FInternationalizationSettingsModelDetails::GetCurrentRegionText)
			.Font(DetailBuilder.GetDetailFont())
		]
	];

	const FText FieldNamesToolTipText = LOCTEXT("EditorFieldNamesTooltip", "Toggle showing localized field names (requires restart to take effect)");

	CategoryBuilder.AddCustomRow(LOCTEXT("EditorFieldNamesLabel", "Use Localized Field Names"))
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("EditorFieldNamesLabel", "Use Localized Field Names"))
			.Font(DetailBuilder.GetDetailFont())
			.ToolTipText(FieldNamesToolTipText)
		]
	]
	.ValueContent()
	.MaxDesiredWidth(300.0f)
	[
		SAssignNew(FieldNamesCheckBox, SCheckBox)
		.IsChecked(Model->ShouldLoadLocalizedPropertyNames() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
		.ToolTipText(FieldNamesToolTipText)
		.OnCheckStateChanged(this, &FInternationalizationSettingsModelDetails::ShouldLoadLocalizedFieldNamesCheckChanged)
	];

	const FText NodeAndPinsNamesToolTipText = LOCTEXT("GraphEditorNodesAndPinsLocalized_Tooltip", "Toggle localized node and pin titles in all graph editors");

	CategoryBuilder.AddCustomRow(LOCTEXT("GraphEditorNodesAndPinsLocalized", "Use Localized Graph Editor Nodes and Pins"))
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GraphEditorNodesAndPinsLocalized", "Use Localized Graph Editor Nodes and Pins"))
			.Font(DetailBuilder.GetDetailFont())
			.ToolTipText(NodeAndPinsNamesToolTipText)
		]
	]
	.ValueContent()
	.MaxDesiredWidth(300.0f)
	[
		SAssignNew(FieldNamesCheckBox, SCheckBox)
		.IsChecked(Model->ShouldShowNodesAndPinsUnlocalized() ? ECheckBoxState::Unchecked : ECheckBoxState::Checked)
		.ToolTipText(NodeAndPinsNamesToolTipText)
		.OnCheckStateChanged(this, &FInternationalizationSettingsModelDetails::ShouldShowNodesAndPinsUnlocalized)
	];

	CategoryBuilder.AddCustomRow(LOCTEXT("EditorRestartWarningLabel", "RestartWarning"))
	.Visibility( TAttribute<EVisibility>(this, &FInternationalizationSettingsModelDetails::GetInternationalizationRestartRowVisibility) )
	.WholeRowContent()
	.HAlign(HAlign_Center)
	[
		SNew( SHorizontalBox )
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.0f, 0.0f)
		[
			SNew( SImage )
			.Image( FCoreStyle::Get().GetBrush("Icons.Warning") )
		]
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew( STextBlock )
			.Text( LOCTEXT("RestartWarningText", "Changes require restart to take effect.") )
			.Font(DetailBuilder.GetDetailFont())
		]
	];
}

void FInternationalizationSettingsModelDetails::RefreshAvailableCultures()
{
	AvailableCultures.Empty();
	TArray<FCultureRef> LocalizedCultures;
	FInternationalization::Get().GetCulturesWithAvailableLocalization(FPaths::GetEditorLocalizationPaths(), LocalizedCultures, true);
	for (const FCultureRef& Culture : LocalizedCultures)
	{
		AvailableCultures.Add(Culture);
	}

	// Update our selected culture based on the available choices
	if ( !AvailableCultures.Contains( SelectedCulture ) )
	{
		SelectedCulture = NULL;
	}

	RefreshAvailableLanguages();
}

void FInternationalizationSettingsModelDetails::RefreshAvailableLanguages()
{
	AvailableLanguages.Empty();
	
	FString SelectedLanguageName;
	if ( SelectedCulture.IsValid() )
	{
		SelectedLanguageName = SelectedCulture->GetNativeLanguage();
	}

	// Setup the language list
	for( const auto& Culture : AvailableCultures )
	{
		FCulturePtr LanguageCulture = FInternationalization::Get().GetCulture(Culture->GetTwoLetterISOLanguageName());
		if (LanguageCulture.IsValid() && AvailableLanguages.Find(LanguageCulture) == INDEX_NONE)
		{
			AvailableLanguages.Add(LanguageCulture);
				
			// Do we have a match for the base language
			const FString CultureLanguageName = Culture->GetNativeLanguage();
			if ( SelectedLanguageName == CultureLanguageName)
			{
				SelectedLanguage = Culture;
			}
		}
	}

	AvailableLanguages.Sort( FCompareCultureByNativeLanguage() );

	RefreshAvailableRegions();
}

void FInternationalizationSettingsModelDetails::RefreshAvailableRegions()
{
	AvailableRegions.Empty();

	FCulturePtr DefaultCulture;
	if ( SelectedLanguage.IsValid() )
	{
		const FString SelectedLanguageName = SelectedLanguage->GetTwoLetterISOLanguageName();

		// Setup the region list
		for( const auto& Culture : AvailableCultures )
		{
			const FString CultureLanguageName = Culture->GetTwoLetterISOLanguageName();
			if ( SelectedLanguageName == CultureLanguageName)
			{
				AvailableRegions.Add( Culture );

				// If this doesn't have a valid region... assume it's the default
				const FString CultureRegionName = Culture->GetNativeRegion();
				if ( CultureRegionName.IsEmpty() )
				{	
					DefaultCulture = Culture;
				}
			}
		}

		AvailableRegions.Sort( FCompareCultureByNativeRegion() );
	}

	// If we have a preferred default (or there's only one in the list), select that now
	if ( !DefaultCulture.IsValid() )
	{
		if(AvailableRegions.Num() == 1)
		{
			DefaultCulture = AvailableRegions.Last();
		}
	}

	if ( DefaultCulture.IsValid() )
	{
		// Set it as our default region, if one hasn't already been chosen
		if ( !SelectedCulture.IsValid() && RegionComboBox.IsValid() )
		{
			// We have to update the combo box like this, otherwise it'll do a null selection when we next click on it
			RegionComboBox->SetSelectedItem( DefaultCulture );
		}
	}

	if ( RegionComboBox.IsValid() )
	{
		RegionComboBox->RefreshOptions();
	}
}

FText FInternationalizationSettingsModelDetails::GetCurrentLanguageText() const
{
	if( SelectedLanguage.IsValid() )
	{
		return FCompareCultureByNativeLanguage::GetCultureNativeLanguageText(SelectedLanguage);
	}
	return FText::GetEmpty();
}

TSharedRef<SWidget> FInternationalizationSettingsModelDetails::OnLanguageGenerateWidget( FCulturePtr Culture, IDetailLayoutBuilder* DetailBuilder ) const
{
	return SNew(STextBlock)
		.Text(FCompareCultureByNativeLanguage::GetCultureNativeLanguageText(Culture))
		.Font(DetailBuilder->GetDetailFont());
}

void FInternationalizationSettingsModelDetails::OnLanguageSelectionChanged( FCulturePtr Culture, ESelectInfo::Type SelectionType )
{
	SelectedLanguage = Culture;
	SelectedCulture = NULL;
	RegionComboBox->ClearSelection();

	RefreshAvailableRegions();
}

FText FInternationalizationSettingsModelDetails::GetCurrentRegionText() const
{
	if( SelectedCulture.IsValid() )
	{
		return FCompareCultureByNativeRegion::GetCultureNativeRegionText(SelectedCulture);
	}
	return FText::GetEmpty();
}

TSharedRef<SWidget> FInternationalizationSettingsModelDetails::OnRegionGenerateWidget( FCulturePtr Culture, IDetailLayoutBuilder* DetailBuilder ) const
{
	return SNew(STextBlock)
		.Text(FCompareCultureByNativeRegion::GetCultureNativeRegionText(Culture))
		.Font(DetailBuilder->GetDetailFont());
}

void FInternationalizationSettingsModelDetails::OnRegionSelectionChanged( FCulturePtr Culture, ESelectInfo::Type SelectionType )
{
	SelectedCulture = Culture;

	HandleShutdownPostPackagesSaved();
}

bool FInternationalizationSettingsModelDetails::IsRegionSelectionAllowed() const
{
	return SelectedLanguage.IsValid();
}

EVisibility FInternationalizationSettingsModelDetails::GetInternationalizationRestartRowVisibility() const
{
	return RequiresRestart ? EVisibility::Visible : EVisibility::Collapsed;
}

void FInternationalizationSettingsModelDetails::ShouldLoadLocalizedFieldNamesCheckChanged(ECheckBoxState CheckState)
{
	HandleShutdownPostPackagesSaved();
}


void FInternationalizationSettingsModelDetails::HandleShutdownPostPackagesSaved()
{
	if ( SelectedCulture.IsValid() )
	{
		check(Model.IsValid());
		Model->SetCultureName(SelectedCulture->GetName());
		Model->ShouldLoadLocalizedPropertyNames(FieldNamesCheckBox->IsChecked());
		if(SelectedCulture != FInternationalization::Get().GetCurrentCulture())
		{
			RequiresRestart = true;
		}
		else
		{
			RequiresRestart = false;
		}
	}
}

void FInternationalizationSettingsModelDetails::ShouldShowNodesAndPinsUnlocalized(ECheckBoxState CheckState)
{
	check(Model.IsValid());
	Model->ShouldShowNodesAndPinsUnlocalized(CheckState == ECheckBoxState::Unchecked);

	// Find all Schemas and force a visualization cache clear
	for ( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
	{
		UClass* CurrentClass = *ClassIt;

		if (UEdGraphSchema* Schema = Cast<UEdGraphSchema>(CurrentClass->GetDefaultObject()))
		{
			Schema->ForceVisualizationCacheClear();
		}
	}
}

#undef LOCTEXT_NAMESPACE
