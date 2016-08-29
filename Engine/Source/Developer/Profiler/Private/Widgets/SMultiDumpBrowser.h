// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SMultiDumpBrowser"

/*-----------------------------------------------------------------------------
Declarations
-----------------------------------------------------------------------------*/


/** A custom widget used to display a histogram. */
class SMultiDumpBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMultiDumpBrowser) { }
	SLATE_END_ARGS()

	// descriptor for a single stats file; associated with a table row, so we can load on demand
	class FFileDescriptor
	{
	public:
		enum DescriptorStatus
		{
			EQueued = 0,
			ELoading,
			ELoaded,
			EAnalyzed
		};
		FFileDescriptor()
			:Status(EQueued)
		{
		}

		FString FullPath;
		FString DisplayName;
		FString TimeString;
		DescriptorStatus Status;
		FText GetDisplayNameString() const
		{
			return FText::FromString(DisplayName + " - " + TimeString);
		}
	};

	// Table row for the stats dump file list view
	class SFileTableRow : public STableRow<TSharedPtr<FFileDescriptor>>
	{
	public:
		SLATE_BEGIN_ARGS(SFileTableRow) { }
		SLATE_ARGUMENT(TSharedPtr<FFileDescriptor>, Desc)
			//		SLATE_EVENT(FOnCheckStateChanged, OnCheckStateChanged)
		SLATE_END_ARGS()

		TSharedPtr<FFileDescriptor> Desc;

		FText GetDisplayName() const
		{
			return Desc->GetDisplayNameString();
		}

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedPtr<FFileDescriptor>& InFileDesc)
		{
			STableRow<TSharedPtr<FFileDescriptor>>::Construct(STableRow<TSharedPtr<FFileDescriptor>>::FArguments(), OwnerTable);
			Desc = InFileDesc;

			ChildSlot
				[
					SNew(SBox)
					[
						SNew(STextBlock)
						.Text(this, &SFileTableRow::GetDisplayName)
					]
				];
		}
	};


	/**
	* Construct this widget
	*
	* @param	InArgs	The declaration data for this widget
	*/
	void Construct(const FArguments& InArgs)
	{
		this->SetEnabled(true);
		ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				.Padding(4)
				.VAlign(VAlign_Fill)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(2)
					[
						SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder")).Padding(4).HAlign(HAlign_Fill)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
								[
									SNew(STextBlock).Text(LOCTEXT("MultiDumpBrowserThreadTotals", "Show thread totals for:"))
								]
							+ SHorizontalBox::Slot().HAlign(HAlign_Fill)
								[
									SAssignNew(DisplayTotalsBox, SEditableTextBox).OnTextCommitted(this, &SMultiDumpBrowser::PrefilterTextCommitted).ToolTipText(LOCTEXT("MultiDumpBrowserTooltip", "Use \"Load Folder\" above to load a folder of stats dumps. GT/RT totals for stats matching text entered here will be displayed in the list below - e.g. enter \"Particle\" here to show total thread times for particle emitters."))
								]
						]
					]
					+ SVerticalBox::Slot().Padding(2).FillHeight(1.0f)
					[
						SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder")).Padding(4).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
						[
							SAssignNew(FileList, SListView<TSharedPtr<FFileDescriptor>>)
							.ListItemsSource(&StatsFiles)
							.ItemHeight(16)
							.OnGenerateRow(this, &SMultiDumpBrowser::GenerateFileRow)
							.OnSelectionChanged(this, &SMultiDumpBrowser::SelectionChanged)
						]
					]
				]
			];
	}

	TSharedRef<ITableRow> GenerateFileRow(TSharedPtr<FFileDescriptor> FileItem, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(SFileTableRow, OwnerTable, FileItem);
	}

	void SelectionChanged(TSharedPtr<FFileDescriptor> SelectedItem, ESelectInfo::Type SelType)
	{
		FProfilerManager::Get()->LoadProfilerCapture(SelectedItem->FullPath);
	}


	void Update()
	{
		FileList->RequestListRefresh();
	}

	void AddFile(FFileDescriptor *InFileDesc)
	{
		StatsFiles.Add(MakeShareable(InFileDesc));
	}

	void Clear()
	{
		StatsFiles.Empty();
	}


	/**
	* Tick the file browser; we update the thread totals we've filtered for here; this is necessary because 
	* the profiler window widgets process loading in their tick and we can't load more than a single file at a time, so we need to
	* trigger loading of each one, then analyze in order to display thread totals for specific search terms
	*/
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		static bool bCurrentlyLoading = false;

		// iterate over all stats files, load one at a time, analyze
		for (auto Desc : StatsFiles)
		{
			// if we're not currently loading one and we've found one in the list that was added but not analyzed, load it
			if (Desc->Status == FFileDescriptor::EQueued && bCurrentlyLoading == false)
			{
				FProfilerManager::Get()->LoadProfilerCapture(Desc->FullPath);
				Desc->Status = FFileDescriptor::ELoading;
				Desc->TimeString = "Getting timings, please wait...";
				bCurrentlyLoading = true;
			}

			// if the current load is completed, start summing up thread totals for the term we're looking for in the DisplayTotalsBox
			if (FProfilerManager::Get()->IsCaptureFileFullyProcessed() && Desc->Status == FFileDescriptor::ELoading)
			{
				Desc->Status = FFileDescriptor::ELoaded;
				bCurrentlyLoading = false;
				FindTotalsForPrefilter(Desc);

				Desc->Status = FFileDescriptor::EAnalyzed;
			}
		}

		if (bCurrentlyLoading == true)
		{
			this->SetEnabled(false);
		}
		else
		{
			this->SetEnabled(true);
		}

		SWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	}


	/**
	 * Recursively searches starting at Event for stats matching the TotalsFilteringText
	 * and returns total times
	 * @param	Event			Start recursively accumulating at this event
	 * @param	OutTotalTime	Total accumulated inclusive time for all stats matching TotalsFilteringText
	 */
	void FindTotalsForPrefilterRecursive(FEventGraphSamplePtr Event, float &OutTotalTime)
	{
		FString EventName = Event->_StatName.ToString().ToUpper();
		if (EventName.Contains(TotalsFilteringText.ToUpper()))
		{
			OutTotalTime += Event->_InclusiveTimeMS;
		}
		else
		{
			for (auto Child : Event->GetChildren())
			{
				FindTotalsForPrefilterRecursive(Child, OutTotalTime);
			}
		}
	}

	/* Find total RT and GT times for stats in dump file Desc, matching TotalsFilteringText
 	 * @param	Desc	FileDescriptor for the stats dump file to traverse
	 */
	void FindTotalsForPrefilter(TSharedPtr<FFileDescriptor> Desc)
	{
		float TotalRenderThreadTime = 0;
		float TotalGameThreadTime = 0;

		// if we don't have a totals filtering text, display the entire RT and GT times
		bool bGetBaseThreadTimes = false;
		if (TotalsFilteringText == "")
		{
			bGetBaseThreadTimes = true;
		}

		FEventGraphSamplePtr RootPtr = FProfilerManager::Get()->GetProfilerSession()->GetEventGraphDataAverage()->GetRoot();

		// start at the root's children, which are the threads
		for (auto Child : RootPtr->GetChildren())
		{
			TArray<FString>EventNameTokens;
			Child->_ThreadName.ToString().ParseIntoArray(EventNameTokens, TEXT(" "));

			// get cumulative times for stats on the render thread
			if (EventNameTokens[0] == "RenderThread")
			{
				if (bGetBaseThreadTimes)
				{
					TotalsFilteringText = "RenderThread";
				}
				FindTotalsForPrefilterRecursive(Child, TotalRenderThreadTime);
			}
			// get cumulative times for stats on the game thread
			else if (EventNameTokens[0] == "GameThread")
			{
				if (bGetBaseThreadTimes)
				{
					TotalsFilteringText = "GameThread";
				}
				FindTotalsForPrefilterRecursive(Child, TotalGameThreadTime);
			}

			if (bGetBaseThreadTimes)
			{
				TotalsFilteringText = "";
			}
		}

		Desc->TimeString = "RT " + FString::Printf(TEXT("%.2f"), TotalRenderThreadTime) + " / GT " + FString::Printf(TEXT("%.2f"), TotalGameThreadTime);
	}

	void PrefilterTextCommitted(const FText& InText, const ETextCommit::Type InTextAction)
	{
		TotalsFilteringText = InText.ToString();
		for (auto Desc : StatsFiles)
		{
			Desc->Status = FFileDescriptor::EQueued;
		}
	}

protected:
	TArray<TSharedPtr<FFileDescriptor>> StatsFiles;
	TSharedPtr< SListView<TSharedPtr<FFileDescriptor>> > FileList;
	TSharedPtr<SEditableTextBox> DisplayTotalsBox;					// edit box determining for which stats names to show thread time totals in the file list
	FString TotalsFilteringText;
};

#undef LOCTEXT_NAMESPACE
