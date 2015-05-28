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

void SVisualLoggerLogsList::Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList)
{
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
	if (SelectedItem.IsValid() == true)
	{
		FLogVisualizer::Get().GetVisualLoggerEvents().OnLogLineSelectionChanged.ExecuteIfBound(SelectedItem, SelectedItem->UserData, SelectedItem->TagName);
	}
	else
	{
		FLogVisualizer::Get().GetVisualLoggerEvents().OnLogLineSelectionChanged.ExecuteIfBound(SelectedItem, 0, NAME_None);
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
				.Text( FText::FromString(Item->Category) )
				.HighlightText(this, &SVisualLoggerLogsList::GetFilterText)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(5.0f, 0.0f))
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor(Item->Verbosity == ELogVerbosity::Error ? FLinearColor::Red : (Item->Verbosity == ELogVerbosity::Warning ? FLinearColor::Yellow : FLinearColor::Gray)))
				.Text(FText::FromString(FString(TEXT("(")) + FString(FOutputDevice::VerbosityToString(Item->Verbosity)) + FString(TEXT(")"))))
			]
			+ SHorizontalBox::Slot()
			.Padding(FMargin(5.0f, 0.0f))
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(Item->Verbosity == ELogVerbosity::Error ? FLinearColor::Red : (Item->Verbosity == ELogVerbosity::Warning ? FLinearColor::Yellow : FLinearColor::Gray)))
				.Text(FText::FromString(Item->Line))
				.HighlightText(this, &SVisualLoggerLogsList::GetFilterText)
			]
		];
}

FText SVisualLoggerLogsList::GetFilterText() const
{
	static FText NoText;
	const bool bSearchInsideLogs = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->bSearchInsideLogs;
	return bSearchInsideLogs ? FText::FromString(FCategoryFiltersManager::Get().GetSearchString()) : NoText;
}

void SVisualLoggerLogsList::OnFiltersSearchChanged(const FText& Filter)
{
	OnFiltersChanged();
}

void SVisualLoggerLogsList::OnItemSelectionChanged(const FVisualLogDevice::FVisualLogEntryItem& LogEntry)
{
	CurrentLogEntry.Entry.Reset();
	LogEntryLines.Reset();

	TArray<FVisualLoggerCategoryVerbosityPair> OutCategories;
	FVisualLoggerHelpers::GetCategories(LogEntry.Entry, OutCategories);
	bool bHasValidCategory = false;
	for (auto& CurrentCategory : OutCategories)
	{
		bHasValidCategory |= FCategoryFiltersManager::Get().MatchCategoryFilters(CurrentCategory.CategoryName.ToString(), CurrentCategory.Verbosity);
	}
	
	if (!bHasValidCategory)
	{
		return;
	}

	CurrentLogEntry = LogEntry;
	const FVisualLogLine* LogLine = LogEntry.Entry.LogLines.GetData();
	const bool bSearchInsideLogs = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->bSearchInsideLogs;
	for (int LineIndex = 0; LineIndex < LogEntry.Entry.LogLines.Num(); ++LineIndex, ++LogLine)
	{
		bool bShowLine = FCategoryFiltersManager::Get().MatchCategoryFilters(LogLine->Category.ToString(), LogLine->Verbosity);
		if (bSearchInsideLogs)
		{
			FString String = FCategoryFiltersManager::Get().GetSearchString();
			if (String.Len() > 0)
			{
				bShowLine &= LogLine->Line.Find(String) != INDEX_NONE || LogLine->Category.ToString().Find(String) != INDEX_NONE;
			}
		}
		

		if (bShowLine)
		{
			FLogEntryItem EntryItem;
			EntryItem.Category = LogLine->Category.ToString();
			EntryItem.CategoryColor = FLogVisualizer::Get().GetColorForCategory(LogLine->Category.ToString());
			EntryItem.Verbosity = LogLine->Verbosity;
			EntryItem.Line = LogLine->Line;
			EntryItem.UserData = LogLine->UserData;
			EntryItem.TagName = LogLine->TagName;

			LogEntryLines.Add(MakeShareable(new FLogEntryItem(EntryItem)));
		}
	}

	for (auto& Event : LogEntry.Entry.Events)
	{
		bool bShowLine = FCategoryFiltersManager::Get().MatchCategoryFilters(Event.Name, Event.Verbosity);

		if (bShowLine)
		{
			FLogEntryItem EntryItem;
			EntryItem.Category = Event.Name;
			EntryItem.CategoryColor = FLogVisualizer::Get().GetColorForCategory(*EntryItem.Category);
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

	LogsLinesWidget->RequestListRefresh();
}
#undef LOCTEXT_NAMESPACE
