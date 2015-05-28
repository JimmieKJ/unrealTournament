// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LocalizationDashboardPrivatePCH.h"
#include "SCulturePicker.h"
#include "STableRow.h"
#include "SSearchBox.h"

#define LOCTEXT_NAMESPACE "CulturePicker"

void SCulturePicker::Construct( const FArguments& InArgs )
{
	OnCultureSelectionChanged = InArgs._OnSelectionChanged;
	IsCulturePickable = InArgs._IsCulturePickable;

	TArray<FString> AllCultureNames;
	FInternationalization::Get().GetCultureNames(AllCultureNames);
	for (const auto& CultureName : AllCultureNames)
	{
		const FCulturePtr Culture = FInternationalization::Get().GetCulture(CultureName);
		if (Culture.IsValid())
		{
			Cultures.AddUnique(Culture);
			const TArray<FString> ParentCultureNames = Culture->GetPrioritizedParentCultureNames();
			for (const auto& ParentCultureName : ParentCultureNames)
			{
				const FCulturePtr ParentCulture = FInternationalization::Get().GetCulture(ParentCultureName);
				Cultures.AddUnique(ParentCulture);
			}
		}
	}

	BuildStockEntries();
	RebuildEntries();

	ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSearchBox)
				.HintText( LOCTEXT("SearchHintText", "Name/Abbreviation") )
				.OnTextChanged(this, &SCulturePicker::OnFilterStringChanged)
				.DelayChangeNotificationsWhileTyping(true)
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(TreeView, STreeView<TSharedPtr<FCultureEntry>>)
				.SelectionMode(ESelectionMode::Single)
				.TreeItemsSource(&RootEntries)
				.OnGenerateRow(this, &SCulturePicker::OnGenerateRow)
				.OnGetChildren(this, &SCulturePicker::OnGetChildren)
				.OnSelectionChanged(this, &SCulturePicker::OnSelectionChanged)
			]
		];


	const auto& IsInitialSelection = [&](const TSharedPtr<FCultureEntry>& Entry) -> bool
	{
		return Entry->Culture == InArgs._InitialSelection;
	};

	const TSharedPtr<FCultureEntry>* InitialSelection = RootEntries.FindByPredicate(IsInitialSelection);
	if (InitialSelection)
	{
		TreeView->SetSelection(*InitialSelection);
	}
}

void SCulturePicker::RequestTreeRefresh()
{
	RebuildEntries();
	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}

void SCulturePicker::BuildStockEntries()
{
	TArray<FCulturePtr> StockCultures;
	TArray<FString> StockCultureNames;
	FInternationalization::Get().GetCultureNames(StockCultureNames);
	for (const auto& CultureName : StockCultureNames)
	{
		const FCulturePtr Culture = FInternationalization::Get().GetCulture(CultureName);
		if (Culture.IsValid())
		{
			StockCultures.AddUnique(Culture);
			const TArray<FString> ParentCultureNames = Culture->GetPrioritizedParentCultureNames();
			for (const auto& ParentCultureName : ParentCultureNames)
			{
				const FCulturePtr ParentCulture = FInternationalization::Get().GetCulture(ParentCultureName);
				StockCultures.AddUnique(ParentCulture);
			}
		}
	}

	TMap< FString, TSharedPtr<FCultureEntry> > StockCultureEntries;
	StockCultureEntries.Reserve(StockCultures.Num());

	StockEntries.Empty(StockCultures.Num());
	StockEntries.Reserve(StockCultureEntries.Num());
	for (const FCulturePtr& Culture : StockCultures)
	{
		const FString Name = Culture->GetName();

		TSharedRef<FCultureEntry> CultureEntry = MakeShareable( new FCultureEntry(Culture) );

		// Add culture to entries.
		StockCultureEntries.Add( Name, CultureEntry );

		// Add entry to list of root stock entries - it will be removed if someone has it as a child.
		StockEntries.Add( CultureEntry );
	}

	// Update all parent entries to know about their child entries.
	for (auto& Pair : StockCultureEntries)
	{
		const TSharedPtr<FCultureEntry> Entry = Pair.Value;

		const TArray<FString> ParentCultureNames = Entry->Culture->GetPrioritizedParentCultureNames();

		TArray<FString> InvalidAncestors;
		for (const FString& ParentName : ParentCultureNames)
		{
			// Find existing parent entry.
			TSharedPtr<FCultureEntry> ParentEntry;
			{
				const TSharedPtr<FCultureEntry>* const PossibleParentEntry = StockCultureEntries.Find(ParentName);
				if (PossibleParentEntry)
				{
					ParentEntry = *PossibleParentEntry;
				}
			}

			// Skip unfound parents.
			if (!ParentEntry.IsValid())
			{
				continue;
			}

			// Don't add self as child of self.
			if (Entry == ParentEntry)
			{
				continue;
			}

			// Invalidate ancestors of this parent entry.
			TArray<FString> ParentAncestorCultureNames = ParentEntry->Culture->GetPrioritizedParentCultureNames();
			for (const auto& AncestorCultureName : ParentAncestorCultureNames)
			{
				// Don't treat this parent entry as a parent of itself.
				if (AncestorCultureName != ParentName)
				{
					InvalidAncestors.Add(AncestorCultureName);
				}
			}

			// Don't add ancestors of ancestors.
			if (InvalidAncestors.Contains(ParentName))
			{
				continue;
			}

			// Add current entry to parent entry.
			ParentEntry->Children.Add( MakeShareable( new FCultureEntry(*Entry) ) );

			// Remove the current entry from the base entries, as it is a child.
			StockEntries.Remove(Entry);
		}
	}

	// Sort entries.
	auto CultureEntryComparator = [](const TSharedPtr<FCultureEntry>& LHS, const TSharedPtr<FCultureEntry>& RHS) -> bool
	{
		return LHS->Culture->GetDisplayName() < RHS->Culture->GetDisplayName();
	};
	StockEntries.Sort(CultureEntryComparator);
}

void SCulturePicker::RebuildEntries()
{
	RootEntries.Reset();

	TFunctionRef<void (const TArray< TSharedPtr<FCultureEntry> >&, TArray< TSharedPtr<FCultureEntry> >&)> DeepCopyAndFilter
		= [&](const TArray< TSharedPtr<FCultureEntry> >& InEntries, TArray< TSharedPtr<FCultureEntry> >& OutEntries)
	{
		for (const TSharedPtr<FCultureEntry>& InEntry : InEntries)
		{
			// Set pickable flag.
			bool IsPickable = true;
			if (IsCulturePickable.IsBound())
			{
				IsPickable = IsCulturePickable.Execute(InEntry->Culture);
			}

			TSharedRef<FCultureEntry> OutEntry = MakeShareable(new FCultureEntry(InEntry->Culture, IsPickable));

			// Recurse to children.
			DeepCopyAndFilter(InEntry->Children, OutEntry->Children);

			bool IsFilteredOut = false;
			if (!FilterString.IsEmpty())
			{
				const FString Name = OutEntry->Culture->GetName();
				const FString DisplayName = OutEntry->Culture->GetDisplayName();
				IsFilteredOut = !Name.Contains(FilterString) && !DisplayName.Contains(FilterString);
			}

			// If has children, must be added. If it is not filtered and it is pickable, should be added.
			if ( OutEntry->Children.Num() != 0 || (!IsFilteredOut && IsPickable) )
			{
				OutEntries.Add(OutEntry);
			}
		}	
	};
	DeepCopyAndFilter(StockEntries, RootEntries);
}

void SCulturePicker::OnFilterStringChanged(const FText& InFilterString)
{
	FilterString = InFilterString.ToString();
	RequestTreeRefresh();
}

TSharedRef<ITableRow> SCulturePicker::OnGenerateRow(TSharedPtr<FCultureEntry> Entry, const TSharedRef<STableViewBase>& Table)
{
	return	SNew(STableRow< TSharedPtr<FCultureEntry> >, Table)
			[
				SNew(STextBlock)
				.Text( FText::FromString(Entry->Culture->GetDisplayName()) )
				.ToolTip(
						SNew(SToolTip)
						.Content()
						[
							SNew(STextBlock)
							.Text( FText::FromString(Entry->Culture->GetName()) )
							.HighlightText( FText::FromString(FilterString) )
						]
						)
				.HighlightText( FText::FromString(FilterString) )
				
				.ColorAndOpacity( Entry->IsSelectable ? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground() )
				// TODO: Appear differently if not selectable but present because it's part of the hierarchy.
			];
}

void SCulturePicker::OnGetChildren(TSharedPtr<FCultureEntry> Entry, TArray< TSharedPtr<FCultureEntry> >& Children)
{
	// Add entries from children array.
	for (const auto& Child : Entry->Children)
	{
		Children.Add(Child);

		// Sort entries.
		auto CultureEntryComparator = [](const TSharedPtr<FCultureEntry>& LHS, const TSharedPtr<FCultureEntry>& RHS) -> bool
		{
			return LHS->Culture->GetDisplayName() < RHS->Culture->GetDisplayName();
		};
		Children.Sort(CultureEntryComparator);
	}
}

void SCulturePicker::OnSelectionChanged(TSharedPtr<FCultureEntry> Entry, ESelectInfo::Type SelectInfo)
{
	// Don't count as selection if the entry isn't actually selectable but is part of the hierarchy.
	if (Entry.IsValid() && Entry->IsSelectable)
	{
		OnCultureSelectionChanged.ExecuteIfBound( Entry->Culture, SelectInfo );
	}
}

#undef LOCTEXT_NAMESPACE