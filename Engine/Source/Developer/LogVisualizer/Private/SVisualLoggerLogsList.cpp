// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "SVisualLoggerView.h"
#include "TimeSliderController.h"
#include "ITimeSlider.h"
#include "STimeSlider.h"
#include "SSearchBox.h"
#include "SSequencerSectionOverlay.h"
#include "STimelinesContainer.h"
#include "SVisualLoggerLogsList.h"
#include "LogVisualizerSettings.h"

#define LOCTEXT_NAMESPACE "SVisualLoggerLogsList"

struct FLogEntryItem
{
	FString Category;
	FLinearColor CategoryColor;
	ELogVerbosity::Type Verbosity;
	FString Line;
	int64 UserData;
	FName TagName;
};

namespace ELogsSortMode
{
	enum Type
	{
		ByName,
		ByStartTime,
		ByEndTime,
	};
}

void SVisualLoggerLogsList::Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList, TSharedPtr<IVisualLoggerInterface> InVisualLoggerInterface)
{
	VisualLoggerInterface = InVisualLoggerInterface;

	ChildSlot
		[
			SAssignNew(LogsLinesWidget, SListView<TSharedPtr<FLogEntryItem> >)
			.ItemHeight(20)
			.ListItemsSource(&LogEntryLines)
			.SelectionMode(ESelectionMode::Multi)
			.OnSelectionChanged(this, &SVisualLoggerLogsList::LogEntryLineSelectionChanged)
			.OnGenerateRow(this, &SVisualLoggerLogsList::LogEntryLinesGenerateRow)
		];
}

FReply SVisualLoggerLogsList::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::C && (InKeyEvent.IsLeftCommandDown() || InKeyEvent.IsLeftControlDown()))
	{
		FString ClipboardString;
		for (const TSharedPtr<struct FLogEntryItem>& CurrentItem : LogsLinesWidget->GetSelectedItems())
		{
			ClipboardString += CurrentItem->Category + FString(TEXT(" (")) + FString(FOutputDevice::VerbosityToString(CurrentItem->Verbosity)) + FString(TEXT(") ")) + CurrentItem->Line;
			ClipboardString += TEXT("\n");
		}
		FPlatformMisc::ClipboardCopy(*ClipboardString);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SVisualLoggerLogsList::OnFiltersChanged()
{
	FVisualLogDevice::FVisualLogEntryItem LogEntry = CurrentLogEntry;
	OnItemSelectionChanged(LogEntry);
}

void SVisualLoggerLogsList::LogEntryLineSelectionChanged(TSharedPtr<FLogEntryItem> SelectedItem, ESelectInfo::Type SelectInfo)
{
	TMap<FName, FVisualLogExtensionInterface*>& AllExtensions = FVisualLogger::Get().GetAllExtensions();
	for (auto Iterator = AllExtensions.CreateIterator(); Iterator; ++Iterator)
	{
		FVisualLogExtensionInterface* Extension = (*Iterator).Value;
		if (Extension != NULL)
		{
			if (SelectedItem.IsValid() == true)
			{
				Extension->LogEntryLineSelectionChanged(SelectedItem, SelectedItem->UserData, SelectedItem->TagName);
			}
			else
			{
				Extension->LogEntryLineSelectionChanged(SelectedItem, 0, NAME_None);
			}
		}
	}
}

TSharedRef<ITableRow> SVisualLoggerLogsList::LogEntryLinesGenerateRow(TSharedPtr<FLogEntryItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(5.0f, 0.0f))
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor(Item->CategoryColor))
				.Text(Item->Category)
				.HighlightText(this, &SVisualLoggerLogsList::GetFilterText)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(5.0f, 0.0f))
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor(FLinearColor::Gray))
				.Text(FString(TEXT("(")) + FString(FOutputDevice::VerbosityToString(Item->Verbosity)) + FString(TEXT(")")))
			]
			+ SHorizontalBox::Slot()
			.Padding(FMargin(5.0f, 0.0f))
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(Item->Line)
				.HighlightText(this, &SVisualLoggerLogsList::GetFilterText)
			]
		];
}

FText SVisualLoggerLogsList::GetFilterText() const
{
	static FText NoText;
	const bool bSearchInsideLogs = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->bSearchInsideLogs;
	return bSearchInsideLogs ? FilterText : NoText;
}

void SVisualLoggerLogsList::OnFiltersSearchChanged(const FText& Filter)
{
	FilterText = Filter;
	OnFiltersChanged();
}

void SVisualLoggerLogsList::OnItemSelectionChanged(const FVisualLogDevice::FVisualLogEntryItem& LogEntry)
{
	CurrentLogEntry.Entry.Reset();
	LogEntryLines.Reset();

	TArray<FVisualLoggerCategoryVerbosityPair> OutCategories;
	FVisualLoggerHelpers::GetCategories(LogEntry.Entry, OutCategories);
	if (VisualLoggerInterface->HasValidCategories(OutCategories) == false)
	{
		return;
	}

	CurrentLogEntry = LogEntry;
	const FVisualLogLine* LogLine = LogEntry.Entry.LogLines.GetData();
	for (int LineIndex = 0; LineIndex < LogEntry.Entry.LogLines.Num(); ++LineIndex, ++LogLine)
	{
		bool bShowLine = true;

		FString CurrentCategory = LogLine->Category.ToString();
		bShowLine = VisualLoggerInterface->IsValidCategory(CurrentCategory, LogLine->Verbosity) /*&& (bHistogramGraphsFilter || (QuickFilterText.Len() == 0 || CurrentCategory.Find(QuickFilterText) != INDEX_NONE))*/;

		if (bShowLine)
		{
			FLogEntryItem EntryItem;
			EntryItem.Category = LogLine->Category.ToString();
			EntryItem.CategoryColor = VisualLoggerInterface->GetCategoryColor(LogLine->Category);
			EntryItem.Verbosity = LogLine->Verbosity;
			EntryItem.Line = LogLine->Line;
			EntryItem.UserData = LogLine->UserData;
			EntryItem.TagName = LogLine->TagName;

			LogEntryLines.Add(MakeShareable(new FLogEntryItem(EntryItem)));
		}
	}

	for (auto& Event : LogEntry.Entry.Events)
	{
		bool bShowLine = true;

		FString CurrentCategory = Event.Name;
		bShowLine = VisualLoggerInterface->IsValidCategory(Event.Name, Event.Verbosity) /*&& (bHistogramGraphsFilter || (QuickFilterText.Len() == 0 || CurrentCategory.Find(QuickFilterText) != INDEX_NONE))*/;

		if (bShowLine)
		{
			FLogEntryItem EntryItem;
			EntryItem.Category = Event.Name;
			EntryItem.CategoryColor = VisualLoggerInterface->GetCategoryColor(*EntryItem.Category);
			EntryItem.Verbosity = Event.Verbosity;
			EntryItem.Line = FString::Printf(TEXT("Registered event: '%s' (%d times)%s"), *Event.Name, Event.Counter, Event.EventTags.Num() ? TEXT("\n") : TEXT(""));
			for (auto& EventTag : Event.EventTags)
			{
				EntryItem.Line += FString::Printf(TEXT("%d times for tag: '%s'\n"), EventTag.Value, *EventTag.Key.ToString());
			}
			EntryItem.UserData = Event.UserData;
			EntryItem.TagName = Event.TagName;

			LogEntryLines.Add(MakeShareable(new FLogEntryItem(EntryItem)));
		}

	}

	//SetCurrentViewedTime(LogEntry.Entry.TimeStamp);
	LogsLinesWidget->RequestListRefresh();
}
#undef LOCTEXT_NAMESPACE
