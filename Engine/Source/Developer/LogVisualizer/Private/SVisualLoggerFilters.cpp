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

FColor SVisualLoggerFilters::ColorPalette[] = {
	FColor(0xff00A480),
	FColorList::Aquamarine,
	FColorList::Cyan,
	FColorList::Brown,
	FColorList::Green,
	FColorList::Orange,
	FColorList::Magenta,
	FColorList::BrightGold,
	FColorList::NeonBlue,
	FColorList::MediumSlateBlue,
	FColorList::SpicyPink,
	FColorList::SpringGreen,
	FColorList::SteelBlue,
	FColorList::SummerSky,
	FColorList::Violet,
	FColorList::VioletRed,
	FColorList::YellowGreen,
	FColor(0xff62E200),
	FColor(0xff1F7B67),
	FColor(0xff62AA2A),
	FColor(0xff70227E),
	FColor(0xff006B53),
	FColor(0xff409300),
	FColor(0xff5D016D),
	FColor(0xff34D2AF),
	FColor(0xff8BF13C),
	FColor(0xffBC38D3),
	FColor(0xff5ED2B8),
	FColor(0xffA6F16C),
	FColor(0xffC262D3),
	FColor(0xff0F4FA8),
	FColor(0xff00AE68),
	FColor(0xffDC0055),
	FColor(0xff284C7E),
	FColor(0xff21825B),
	FColor(0xffA52959),
	FColor(0xff05316D),
	FColor(0xff007143),
	FColor(0xff8F0037),
	FColor(0xff4380D3),
	FColor(0xff36D695),
	FColor(0xffEE3B80),
	FColor(0xff6996D3),
	FColor(0xff60D6A7),
	FColor(0xffEE6B9E)
};

#define LOCTEXT_NAMESPACE "SVisualLoggerFilters"
void SVisualLoggerFilters::Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList, TSharedPtr<IVisualLoggerInterface> InVisualLoggerInterface)
{
	VisualLoggerInterface = InVisualLoggerInterface;

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

	const TArray<FSimpleGraphFilter> DataNames = *GraphFilters.Find(MenuName);
	for (const auto& CurrentData : DataNames)
	{
		if (CurrentData.Name.ToString().Find(GraphsFilter) != INDEX_NONE)
		{
			return true;
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
		auto &Filters = GraphFilters[MenuCategory];
		for (int32 Index = 0; Index < Filters.Num(); ++Index)
		{
			Filters[Index].bEnabled = bNewSet;
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
	if (GraphFilters.Contains(MenuCategory))
	{
		auto &Filters = GraphFilters[MenuCategory];
		for (int32 Index = 0; Index < Filters.Num(); ++Index)
		{
			if (Filters[Index].bEnabled)
			{
				return true;
			}
		}
	}

	return false;
}


void SVisualLoggerFilters::CreateFiltersMenuCategoryForGraph(FMenuBuilder& MenuBuilder, FName MenuCategory) const
{
	auto FiltersFromGraph = GraphFilters[MenuCategory];
	for (auto Iter = FiltersFromGraph.CreateIterator(); Iter; ++Iter)
	{
		const FText& LabelText = FText::FromString(Iter->Name.ToString());
		MenuBuilder.AddMenuEntry(
			LabelText,
			FText::Format(LOCTEXT("FilterByTooltipPrefix", "Filter by {0}"), LabelText),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateSP(this, &SVisualLoggerFilters::FilterByTypeClicked, MenuCategory, Iter->Name),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SVisualLoggerFilters::IsAssetTypeActionsInUse, MenuCategory, Iter->Name),
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
		auto &Filters = GraphFilters[InGraphName];
		for (int32 Index = 0; Index < Filters.Num(); ++Index)
		{
			if (Filters[Index].Name == InDataName)
			{
				Filters[Index].bEnabled = !Filters[Index].bEnabled;
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
		auto &Filters = GraphFilters[InGraphName];
		for (int32 Index = 0; Index < Filters.Num(); ++Index)
		{
			if (Filters[Index].Name == InDataName)
			{
				return Filters[Index].bEnabled;
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

FLinearColor SVisualLoggerFilters::GetColorForUsedCategory(int32 Index) const
{
	if (Index >= 0 && Index < sizeof(ColorPalette) / sizeof(ColorPalette[0]))
	{
		return ColorPalette[Index];
	}

	static bool bReateColorList = false;
	static FColorList StaticColor;
	if (!bReateColorList)
	{
		bReateColorList = true;
		StaticColor.CreateColorMap();
	}
	return StaticColor.GetFColorByIndex(Index);
}

FLinearColor SVisualLoggerFilters::GetColorForUsedCategory(const FString& InFilterName) const
{
	int32 CategoryIndex = 0;
	for (auto& CurrentFilter : Filters)
	{
		if (CurrentFilter->GetFilterNameAsString() == InFilterName)
		{
			break;
		}
		CategoryIndex++;
	}

	return GetColorForUsedCategory(CategoryIndex);
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

	const FLinearColor Color = GetColorForUsedCategory(Filters.Num());
	TSharedRef<SFilterWidget> NewFilter =
		SNew(SFilterWidget)
		.FilterName(*InFilterName)
		.ColorCategory(Color)
		.OnFilterChanged(VisualLoggerInterface->GetVisualLoggerEvents().OnFiltersChanged)
		//.OnRequestRemove(this, &SLogFilterList::RemoveFilter)
		//.OnRequestDisableAll(this, &SLogFilterList::DisableAllFilters)
		//.OnRequestRemoveAll(this, &SLogFilterList::RemoveAllFilters);
		;
	NewFilter->SetEnabled(true);
	Filters.Add(NewFilter);

	FilterBox->AddSlot()
		.Padding(2, 2)
		[
			NewFilter
		];

	GraphsFilterCombo->SetVisibility(GraphFilters.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed);
}

void SVisualLoggerFilters::AddFilter(const FString& GraphName, const FString& DataName)
{
	TArray<FSimpleGraphFilter>& DataNames = GraphFilters.FindOrAdd(*GraphName);
	if (DataNames.Find(FSimpleGraphFilter(*DataName)) == INDEX_NONE)
	{
		DataNames.Add(FSimpleGraphFilter(*DataName));
	}
}

bool SVisualLoggerFilters::IsFilterEnabled(const FString& InFilterName, TEnumAsByte<ELogVerbosity::Type> Verbosity)
{
	const ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	const FName FilterName(*InFilterName);
	for (int32 Index = 0; Index < Filters.Num(); ++Index)
	{
		const SFilterWidget& Filter = Filters[Index].Get();
		if (Filter.GetFilterName() == FilterName)
		{
			return Filter.IsEnabled() && Filter.GetVerbosity() >= Verbosity.GetValue() && 
				(Settings->bSearchInsideLogs == true || (Settings->bSearchInsideLogs == false && (FiltersSearchString.Len() == 0 || Filter.GetFilterName().ToString().Find(FiltersSearchString) != INDEX_NONE)));
		}
	}

	return true;
}

bool SVisualLoggerFilters::IsFilterEnabled(const FString& InGraphName, const FString& InDataName, TEnumAsByte<ELogVerbosity::Type> Verbosity)
{
	if (GraphFilters.Contains(*InGraphName))
	{
		auto Filters = GraphFilters[*InGraphName];
		int32 Index = Filters.Find(FSimpleGraphFilter(*InDataName));
		if (Index != INDEX_NONE)
		{
			return Filters[Index].bEnabled && (GraphsFilter.Len() == 0 || Filters[Index].Name.ToString().Find(GraphsFilter) != INDEX_NONE);
		}
	}

	return true;
}

void SVisualLoggerFilters::OnFiltersSearchChanged(const FText& Filter)
{
	FiltersSearchString = Filter.ToString();
}

#undef LOCTEXT_NAMESPACE
