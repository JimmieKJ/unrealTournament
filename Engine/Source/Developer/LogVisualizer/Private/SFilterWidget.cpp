// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "SFilterWidget.h"

#define LOCTEXT_NAMESPACE "SFilterWidget"
/** Constructs this widget with InArgs */
void SFilterWidget::Construct(const FArguments& InArgs)
{
	bEnabled = true;
	OnFilterChanged = InArgs._OnFilterChanged;
	OnRequestRemove = InArgs._OnRequestRemove;
	OnRequestEnableOnly = InArgs._OnRequestEnableOnly;
	OnRequestDisableAll = InArgs._OnRequestDisableAll;
	OnRequestRemoveAll = InArgs._OnRequestRemoveAll;
	FilterName = InArgs._FilterName;
	ColorCategory = InArgs._ColorCategory;
	Verbosity = ELogVerbosity::All;

	// Get the tooltip and color of the type represented by this filter
	FilterColor = ColorCategory;

	ChildSlot
		[
			SNew(SBorder)
			.Padding(0)
			.BorderBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.2f))
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
	const FString VerbosityStr = FOutputDevice::VerbosityToString(Verbosity);
	if (Verbosity != ELogVerbosity::VeryVerbose)
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
	return FText::FromString(
		IsEnabled() ?
		FString::Printf(TEXT("Enabled '%s' category for '%s' verbosity and lower\nRight click to change verbosity"), *GetFilterNameAsString(), FOutputDevice::VerbosityToString(Verbosity))
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
#undef LOCTEXT_NAMESPACE
