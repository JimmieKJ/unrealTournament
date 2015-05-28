// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "SFilterWidget.h"

#define LOCTEXT_NAMESPACE "SFilterWidget"
/** Constructs this widget with InArgs */
void SFilterWidget::Construct(const FArguments& InArgs)
{
	OnFilterChanged = InArgs._OnFilterChanged;
	OnRequestRemove = InArgs._OnRequestRemove;
	OnRequestEnableOnly = InArgs._OnRequestEnableOnly;
	OnRequestDisableAll = InArgs._OnRequestDisableAll;
	OnRequestRemoveAll = InArgs._OnRequestRemoveAll;
	FilterName = InArgs._FilterName;
	ColorCategory = InArgs._ColorCategory;
	BorderBackgroundColor = FLinearColor(0.2f, 0.2f, 0.2f, 0.2f);

	// Get the tooltip and color of the type represented by this filter
	FilterColor = ColorCategory;

	ChildSlot
		[
			SNew(SBorder)
			.Padding(2)
			.BorderBackgroundColor(this, &SFilterWidget::GetBorderBackgroundColor)
			.BorderImage(FLogVisualizerStyle::Get().GetBrush("ContentBrowser.FilterButtonBorder"))
			[
				SAssignNew(ToggleButtonPtr, SFilterCheckBox)
				.Style(FLogVisualizerStyle::Get(), "ContentBrowser.FilterButton")
				.ToolTipText(this, &SFilterWidget::GetTooltipString)
				.Padding(this, &SFilterWidget::GetFilterNamePadding)
				.IsChecked(this, &SFilterWidget::IsChecked)
				.OnCheckStateChanged(this, &SFilterWidget::FilterToggled)
				.OnGetMenuContent(this, &SFilterWidget::GetRightClickMenuContent)
				.ForegroundColor(this, &SFilterWidget::GetFilterForegroundColor)
				[
					SNew(STextBlock)
					.ColorAndOpacity(this, &SFilterWidget::GetFilterNameColorAndOpacity)
					.Font(FLogVisualizerStyle::Get().GetFontStyle("ContentBrowser.FilterNameFont"))
					.ShadowOffset(FVector2D(1.f, 1.f))
					.Text(this, &SFilterWidget::GetCaptionString)
				]
			]
		];

	ToggleButtonPtr->SetOnFilterDoubleClicked(FOnClicked::CreateSP(this, &SFilterWidget::FilterDoubleClicked));
	ToggleButtonPtr->SetOnFilterMiddleButtonClicked(FOnClicked::CreateSP(this, &SFilterWidget::FilterMiddleButtonClicked));
}

FText SFilterWidget::GetCaptionString() const
{
	FString CaptionString;
	FCategoryFilter& Filter = FCategoryFiltersManager::Get().GetCategory(GetFilterNameAsString());
	const FString VerbosityStr = FOutputDevice::VerbosityToString((ELogVerbosity::Type)Filter.LogVerbosity);
	if (Filter.LogVerbosity != ELogVerbosity::VeryVerbose)
	{
		CaptionString = FString::Printf(TEXT("%s [%s]"), *GetFilterNameAsString().Replace(TEXT("Log"), TEXT(""), ESearchCase::CaseSensitive), *VerbosityStr.Mid(0, 1));
	}
	else
	{
		CaptionString = FString::Printf(TEXT("%s [VV]"), *GetFilterNameAsString().Replace(TEXT("Log"), TEXT(""), ESearchCase::CaseSensitive));
	}
	return FText::FromString(CaptionString);
}

FText SFilterWidget::GetTooltipString() const
{
	FCategoryFilter& Filter = FCategoryFiltersManager::Get().GetCategory(GetFilterNameAsString());
	const FString VerbosityStr = FOutputDevice::VerbosityToString((ELogVerbosity::Type)Filter.LogVerbosity);

	return FText::FromString(
		IsEnabled() ?
		FString::Printf(TEXT("Enabled '%s' category for '%s' verbosity and lower\nRight click to change verbosity"), *GetFilterNameAsString(), *VerbosityStr)
		:
		FString::Printf(TEXT("Disabled '%s' category"), *GetFilterNameAsString())
		);
}

TSharedRef<SWidget> SFilterWidget::GetRightClickMenuContent()
{
	FMenuBuilder MenuBuilder(true, NULL);

	if (IsEnabled())
	{
		FText FiletNameAsText = FText::FromString(GetFilterNameAsString());
		MenuBuilder.BeginSection("VerbositySelection", LOCTEXT("VerbositySelection", "Current verbosity selection"));
		{
			for (int32 Index = ELogVerbosity::NoLogging + 1; Index <= ELogVerbosity::VeryVerbose; Index++)
			{
				const FString VerbosityStr = FOutputDevice::VerbosityToString((ELogVerbosity::Type)Index);
				MenuBuilder.AddMenuEntry(
					FText::Format(LOCTEXT("UseVerbosity", "Use: {0}"), FText::FromString(VerbosityStr)),
					LOCTEXT("UseVerbosityTooltip", "Applay verbosity to selected flter."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP(this, &SFilterWidget::SetVerbosityFilter, Index))
					);
			}
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

bool SFilterWidget::IsEnabled() const 
{ 
	FCategoryFilter& Filter = FCategoryFiltersManager::Get().GetCategory(GetFilterNameAsString());
	return Filter.Enabled;
}

void SFilterWidget::SetEnabled(bool InEnabled)
{
	FCategoryFilter& Filter = FCategoryFiltersManager::Get().GetCategory(GetFilterNameAsString());
	if (InEnabled != Filter.Enabled)
	{
		Filter.Enabled = InEnabled;
		OnFilterChanged.ExecuteIfBound();
	}
}

void SFilterWidget::FilterToggled(ECheckBoxState NewState)
{
	FCategoryFilter& Filter = FCategoryFiltersManager::Get().GetCategory(GetFilterNameAsString());
	SetEnabled(NewState == ECheckBoxState::Checked);
}

void SFilterWidget::SetVerbosityFilter(int32 SelectedVerbosityIndex)
{
	FCategoryFilter& Filter = FCategoryFiltersManager::Get().GetCategory(GetFilterNameAsString());
	Filter.LogVerbosity = (ELogVerbosity::Type)SelectedVerbosityIndex;
	OnFilterChanged.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE
