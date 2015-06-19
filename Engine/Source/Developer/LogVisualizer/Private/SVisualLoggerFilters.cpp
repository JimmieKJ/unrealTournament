// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "SFilterWidget.h"
#include "SSearchBox.h"
#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#endif

/////////////////////
// SLogFilterList
/////////////////////

#define LOCTEXT_NAMESPACE "SVisualLoggerFilters"
void SVisualLoggerFilters::Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList)
{
	ChildSlot
		[
			SAssignNew(FilterBox, SWrapBox)
			.UseAllottedWidth(true)
		];

	GraphsFilterCombo =
		SNew(SComboButton)
		.ComboButtonStyle(FLogVisualizerStyle::Get(), "Filters.Style")
		.ForegroundColor(FLinearColor::White)
		.ContentPadding(0)
		.OnGetMenuContent(this, &SVisualLoggerFilters::MakeGraphsFilterMenu)
		.ToolTipText(LOCTEXT("AddFilterToolTip", "Add an asset filter."))
		.HasDownArrow(true)
		.ContentPadding(FMargin(1, 0))
		.ButtonContent()
		[
			SNew(STextBlock)
			.TextStyle(FLogVisualizerStyle::Get(), "Filters.Text")
			.Text(LOCTEXT("GraphFilters", "Graph Filters"))
		];

	GraphsFilterCombo->SetVisibility(GraphFilters.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed);

	FilterBox->AddSlot()
		.Padding(3, 3)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0))
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Visibility_Lambda([this]()->EVisibility{ return this->GraphsFilter.Len() > 0 ? EVisibility::Visible : EVisibility::Hidden; })
				.Image(FLogVisualizerStyle::Get().GetBrush("Filters.FilterIcon"))
			]
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0))
			[
				GraphsFilterCombo.ToSharedRef()
			]
		];
}

bool SVisualLoggerFilters::GraphSubmenuVisibility(const FName MenuName)
{
	if (GraphsFilter.Len() == 0)
	{
		return true;
	}

	if (GraphFilters.Find(MenuName) != NULL)
	{
		const TArray<FString> DataNames = *GraphFilters.Find(MenuName);
		for (const auto& CurrentData : DataNames)
		{
			if (CurrentData.Find(GraphsFilter) != INDEX_NONE)
			{
				return true;
			}
		}
	}

	return false;
}

TSharedRef<SWidget> SVisualLoggerFilters::MakeGraphsFilterMenu()
{
	FMenuBuilder MenuBuilder(true, NULL);

	MenuBuilder.BeginSection(TEXT("Graphs"));
	{
		TSharedRef<SSearchBox> FiltersSearchBox = SNew(SSearchBox)
			.InitialText(FText::FromString(GraphsFilter))
			.HintText(LOCTEXT("GraphsFilterSearchHint", "Quick filter"))
			.OnTextChanged(this, &SVisualLoggerFilters::OnSearchChanged);

		MenuBuilder.AddWidget(FiltersSearchBox, LOCTEXT("FiltersSearchMenuWidget", ""));

		for (auto Iter = GraphFilters.CreateIterator(); Iter; ++Iter)
		{
			const FText& LabelText = FText::FromString(Iter->Key.ToString());
			MenuBuilder.AddSubMenu(
				LabelText,
				FText::Format(LOCTEXT("FilterByTooltipPrefix", "Filter by {0}"), LabelText),
				FNewMenuDelegate::CreateSP(this, &SVisualLoggerFilters::CreateFiltersMenuCategoryForGraph, Iter->Key),
				FUIAction(
				FExecuteAction::CreateSP(this, &SVisualLoggerFilters::GraphFilterCategoryClicked, Iter->Key),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SVisualLoggerFilters::IsGraphFilterCategoryInUse, Iter->Key),
				FIsActionButtonVisible::CreateLambda([this, LabelText]()->bool{return GraphSubmenuVisibility(*LabelText.ToString()); })
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
				);
		}
	}
	MenuBuilder.EndSection(); //ContentBrowserFilterBasicAsset


	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);

	const FVector2D DisplaySize(
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Right - DisplayMetrics.PrimaryDisplayWorkAreaRect.Left,
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - DisplayMetrics.PrimaryDisplayWorkAreaRect.Top);

	return
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.MaxHeight(DisplaySize.Y * 0.5)
		[
			MenuBuilder.MakeWidget()
		];
}

void SVisualLoggerFilters::GraphFilterCategoryClicked(FName MenuCategory)
{
	const bool bNewSet = !IsGraphFilterCategoryInUse(MenuCategory);

	if (GraphFilters.Contains(MenuCategory))
	{
		bool bChanged = false;
		for (const FString &Filter : GraphFilters[MenuCategory])
		{
			const FString GraphFilterName = MenuCategory.ToString() + TEXT("$") + Filter;
			FCategoryFiltersManager::Get().GetCategory(GraphFilterName).Enabled = bNewSet;
			bChanged = true;
		}

		if (bChanged)
		{
			InvalidateCanvas();
		}
	}
}

bool SVisualLoggerFilters::IsGraphFilterCategoryInUse(FName MenuCategory) const
{
	bool bInUse = false;
	if (GraphFilters.Contains(MenuCategory))
	{
		for (const FString& Filter : GraphFilters[MenuCategory])
		{
			const FString GraphFilterName = MenuCategory.ToString() + TEXT("$") + Filter;
			bInUse |= FCategoryFiltersManager::Get().GetCategory(GraphFilterName).Enabled;
		}
	}

	return bInUse;
}


void SVisualLoggerFilters::CreateFiltersMenuCategoryForGraph(FMenuBuilder& MenuBuilder, FName MenuCategory) const
{
	auto FiltersFromGraph = GraphFilters[MenuCategory];
	for (auto Iter = FiltersFromGraph.CreateIterator(); Iter; ++Iter)
	{
		FName Name = **Iter;
		const FText& LabelText = FText::FromString(Name.ToString());
		MenuBuilder.AddMenuEntry(
			LabelText,
			FText::Format(LOCTEXT("FilterByTooltipPrefix", "Filter by {0}"), LabelText),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateSP(this, &SVisualLoggerFilters::FilterByTypeClicked, MenuCategory, Name),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SVisualLoggerFilters::IsAssetTypeActionsInUse, MenuCategory, Name),
			FIsActionButtonVisible::CreateLambda([this, LabelText]()->bool{return this->GraphsFilter.Len() == 0 || LabelText.ToString().Find(this->GraphsFilter) != INDEX_NONE; })),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
			);
	}
}

void SVisualLoggerFilters::FilterByTypeClicked(FName InGraphName, FName InDataName)
{
	if (GraphFilters.Contains(InGraphName))
	{
		bool bChanged = false;
		for (const FString& Filter : GraphFilters[InGraphName])
		{
			if (Filter == InDataName.ToString())
			{
				const FString GraphFilterName = InGraphName.ToString() + TEXT("$") + Filter;
				FCategoryFilter& CategoryFilter = FCategoryFiltersManager::Get().GetCategory(GraphFilterName);
				CategoryFilter.Enabled = !CategoryFilter.Enabled;
				bChanged = true;
			}
		}

		if (bChanged)
		{
			InvalidateCanvas();
		}
	}
}

bool SVisualLoggerFilters::IsAssetTypeActionsInUse(FName InGraphName, FName InDataName) const
{
	if (GraphFilters.Contains(InGraphName))
	{
		for (const FString& Filter : GraphFilters[InGraphName])
		{
			if (Filter == InDataName.ToString())
			{
				const FString GraphFilterName = InGraphName.ToString() + TEXT("$") + Filter;
				return FCategoryFiltersManager::Get().GetCategory(GraphFilterName).Enabled;
			}
		}
	}

	return false;
}

void SVisualLoggerFilters::OnSearchChanged(const FText& Filter)
{
	GraphsFilter = Filter.ToString();

	InvalidateCanvas();
}

void SVisualLoggerFilters::InvalidateCanvas()
{
#if WITH_EDITOR
	UEditorEngine *EEngine = Cast<UEditorEngine>(GEngine);
	if (GIsEditor && EEngine != NULL)
	{
		for (int32 Index = 0; Index < EEngine->AllViewportClients.Num(); Index++)
		{
			FEditorViewportClient* ViewportClient = EEngine->AllViewportClients[Index];
			if (ViewportClient)
			{
				ViewportClient->Invalidate();
			}
		}
	}
#endif
}

uint32 SVisualLoggerFilters::GetCategoryIndex(const FString& InFilterName) const
{
	uint32 CategoryIndex = 0;
	for (auto& CurrentFilter : Filters)
	{
		if (CurrentFilter->GetFilterNameAsString() == InFilterName)
		{
			return CategoryIndex;
		}
		CategoryIndex++;
	}

	return INDEX_NONE;
}

void SVisualLoggerFilters::AddFilter(const FString& InFilterName)
{
	for (auto& CurrentFilter : Filters)
	{
		if (CurrentFilter->GetFilterNameAsString() == InFilterName)
		{
			return;
		}
	}

	const FLinearColor Color = FLogVisualizer::Get().GetColorForCategory(Filters.Num());
	TSharedRef<SFilterWidget> NewFilter =
		SNew(SFilterWidget)
		.FilterName(*InFilterName)
		.ColorCategory(Color)
		.OnFilterChanged(SFilterWidget::FOnSimpleRequest::CreateRaw(this, &SVisualLoggerFilters::OnFiltersChanged))
		//.OnRequestRemove(this, &SLogFilterList::RemoveFilter)
		//.OnRequestDisableAll(this, &SLogFilterList::DisableAllFilters)
		//.OnRequestRemoveAll(this, &SLogFilterList::RemoveAllFilters);
		;

	bool bIsSet = true;

	if (FCategoryFiltersManager::Get().IsValidCategory(InFilterName))
	{
		bIsSet = FCategoryFiltersManager::Get().GetCategory(InFilterName).Enabled;
	}
	FCategoryFiltersManager::Get().AddCategory(InFilterName, ELogVerbosity::All);
	Filters.Add(NewFilter);

	FilterBox->AddSlot()
		.Padding(2, 2)
		[
			NewFilter
		];

	GraphsFilterCombo->SetVisibility(GraphFilters.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed);
}

void SVisualLoggerFilters::ResetData()
{
	for (auto& CurrentFilter : Filters)
	{
		FilterBox->RemoveSlot(CurrentFilter);
	}
	Filters.Reset();
}


void SVisualLoggerFilters::OnFiltersChanged()
{
	TArray<FString> EnabledFilters;
	for (TSharedRef<SFilterWidget> CurrentFilter : Filters)
	{
		if (CurrentFilter->IsEnabled())
		{
			EnabledFilters.AddUnique(CurrentFilter->GetFilterName().ToString());
		}
	}

	FLogVisualizer::Get().GetVisualLoggerEvents().OnFiltersChanged.ExecuteIfBound();
}

void SVisualLoggerFilters::AddFilter(const FString& GraphName, const FString& DataName)
{
	GraphFilters.FindOrAdd(*GraphName).AddUnique(DataName);

	FString GraphFilterName = GraphName + TEXT("$") + DataName;
	FCategoryFiltersManager::Get().AddCategory(GraphFilterName, ELogVerbosity::All);
}

void SVisualLoggerFilters::OnFiltersSearchChanged(const FText& Filter)
{

}

void SVisualLoggerFilters::OnItemSelectionChanged(const struct FVisualLogEntry& EntryItem)
{
	TArray<FVisualLoggerCategoryVerbosityPair> Categories;
	FVisualLoggerHelpers::GetCategories(EntryItem, Categories);
	for (int32 Index = 0; Index < Filters.Num(); ++Index)
	{
		SFilterWidget& Filter = Filters[Index].Get();
		Filter.SetBorderBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.2f));
		for (const FVisualLoggerCategoryVerbosityPair& Category : Categories)
		{
			if (Filter.GetFilterName() == Category.CategoryName)
			{
				Filter.SetBorderBackgroundColor(FLinearColor(0.3f, 0.3f, 0.3f, 0.8f));
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
