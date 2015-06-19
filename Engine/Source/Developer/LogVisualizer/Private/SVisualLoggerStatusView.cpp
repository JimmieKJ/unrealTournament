// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "SVisualLoggerView.h"
#include "TimeSliderController.h"
#include "ITimeSlider.h"
#include "STimeSlider.h"
#include "SSearchBox.h"
#include "SSequencerSectionOverlay.h"
#include "STimelinesContainer.h"
#include "SVisualLoggerStatusView.h"

#define LOCTEXT_NAMESPACE "SVisualLoggerStatusView"

struct FLogStatusItem
{
	FString ItemText;
	FString ValueText;

	TArray< TSharedPtr< FLogStatusItem > > Children;

	FLogStatusItem(const FString& InItemText) : ItemText(InItemText) {}
	FLogStatusItem(const FString& InItemText, const FString& InValueText) : ItemText(InItemText), ValueText(InValueText) {}
};


void SVisualLoggerStatusView::Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList)
{
	ChildSlot
		[
			SNew(SBorder)
			.Padding(1)
			.BorderImage(FLogVisualizerStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(StatusItemsView, STreeView<TSharedPtr<FLogStatusItem> >)
				.ItemHeight(40.0f)
				.TreeItemsSource(&StatusItems)
				.OnGenerateRow(this, &SVisualLoggerStatusView::HandleGenerateLogStatus)
				.OnGetChildren(this, &SVisualLoggerStatusView::OnLogStatusGetChildren)
				.SelectionMode(ESelectionMode::None)
			]
		];
}

void GenerateChildren(TSharedPtr<FLogStatusItem> StatusItem, const FVisualLogStatusCategory LogCategory)
{
	for (int32 LineIndex = 0; LineIndex < LogCategory.Data.Num(); LineIndex++)
	{
		FString KeyDesc, ValueDesc;
		const bool bHasValue = LogCategory.GetDesc(LineIndex, KeyDesc, ValueDesc);
		if (bHasValue)
		{
			StatusItem->Children.Add(MakeShareable(new FLogStatusItem(KeyDesc, ValueDesc)));
		}
	}

	for (const FVisualLogStatusCategory& Child : LogCategory.Children)
	{
		TSharedPtr<FLogStatusItem> ChildCategory = MakeShareable(new FLogStatusItem(Child.Category));
		StatusItem->Children.Add(ChildCategory);
		GenerateChildren(ChildCategory, Child);
	}
}
void SVisualLoggerStatusView::OnItemSelectionChanged(const FVisualLogDevice::FVisualLogEntryItem& LogEntry)
{
	TArray<FString> ExpandedCategories;
	for (int32 ItemIndex = 0; ItemIndex < StatusItems.Num(); ItemIndex++)
	{
		const bool bIsExpanded = StatusItemsView->IsItemExpanded(StatusItems[ItemIndex]);
		if (bIsExpanded)
		{
			ExpandedCategories.Add(StatusItems[ItemIndex]->ItemText);
		}
	}

	StatusItems.Empty();

	FString TimestampDesc = FString::Printf(TEXT("%.2fs"), LogEntry.Entry.TimeStamp);
	StatusItems.Add(MakeShareable(new FLogStatusItem(LOCTEXT("VisLogTimestamp", "Time").ToString(), TimestampDesc)));

	for (int32 CategoryIndex = 0; CategoryIndex < LogEntry.Entry.Status.Num(); CategoryIndex++)
	{
		if (LogEntry.Entry.Status[CategoryIndex].Data.Num() <= 0 && LogEntry.Entry.Status[CategoryIndex].Children.Num() == 0)
		{
			continue;
		}

		TSharedRef<FLogStatusItem> StatusItem = MakeShareable(new FLogStatusItem(LogEntry.Entry.Status[CategoryIndex].Category));
		for (int32 LineIndex = 0; LineIndex < LogEntry.Entry.Status[CategoryIndex].Data.Num(); LineIndex++)
		{
			FString KeyDesc, ValueDesc;
			const bool bHasValue = LogEntry.Entry.Status[CategoryIndex].GetDesc(LineIndex, KeyDesc, ValueDesc);
			if (bHasValue)
			{
				StatusItem->Children.Add(MakeShareable(new FLogStatusItem(KeyDesc, ValueDesc)));
			}
		}

		for (const FVisualLogStatusCategory& Child : LogEntry.Entry.Status[CategoryIndex].Children)
		{
			TSharedPtr<FLogStatusItem> ChildCategory = MakeShareable(new FLogStatusItem(Child.Category));
			StatusItem->Children.Add(ChildCategory);
			GenerateChildren(ChildCategory, Child);
		}

		StatusItems.Add(StatusItem);
	}

	StatusItemsView->RequestTreeRefresh();

	for (int32 ItemIndex = 0; ItemIndex < StatusItems.Num(); ItemIndex++)
	{
		for (const FString& Category : ExpandedCategories)
		{
			if (StatusItems[ItemIndex]->ItemText == Category)
			{
				StatusItemsView->SetItemExpansion(StatusItems[ItemIndex], true);
				break;
			}
		}
	}
}

void SVisualLoggerStatusView::OnLogStatusGetChildren(TSharedPtr<FLogStatusItem> InItem, TArray< TSharedPtr<FLogStatusItem> >& OutItems)
{
	OutItems = InItem->Children;
}

TSharedRef<ITableRow> SVisualLoggerStatusView::HandleGenerateLogStatus(TSharedPtr<FLogStatusItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (InItem->Children.Num() > 0)
	{
		return SNew(STableRow<TSharedPtr<FLogStatusItem> >, OwnerTable)
			[
				SNew(STextBlock)
				.Text(FText::FromString(InItem->ItemText))
			];
	}

	FString TooltipText = FString::Printf(TEXT("%s: %s"), *InItem->ItemText, *InItem->ValueText);
	return SNew(STableRow<TSharedPtr<FLogStatusItem> >, OwnerTable)
		[
			SNew(SBorder)
			.BorderImage(FLogVisualizerStyle::Get().GetBrush("NoBorder"))
			.ToolTipText(FText::FromString(TooltipText))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::FromString(InItem->ItemText))
					.ColorAndOpacity(FColorList::Aquamarine)
				]
				+ SHorizontalBox::Slot()
				.Padding(4.0f, 0, 0, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(InItem->ValueText))
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
