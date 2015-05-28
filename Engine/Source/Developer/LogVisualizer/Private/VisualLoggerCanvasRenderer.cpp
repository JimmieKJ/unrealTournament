// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "LogVisualizer.h"
#include "Engine.h"
#include "SlateBasics.h"
#include "EditorStyle.h"
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#include "VisualLoggerCanvasRenderer.h"
#include "Debug/ReporterGraph.h"
#include "TimeSliderController.h"

namespace LogVisualizer
{
	FORCEINLINE bool PointInFrustum(UCanvas* Canvas, const FVector& Location)
	{
		return Canvas->SceneView->ViewFrustum.IntersectBox(Location, FVector::ZeroVector);
	}

	FORCEINLINE void DrawText(UCanvas* Canvas, UFont* Font, const FString& TextToDraw, const FVector& WorldLocation)
	{
		if (PointInFrustum(Canvas, WorldLocation))
		{
			const FVector ScreenLocation = Canvas->Project(WorldLocation);
			Canvas->DrawText(Font, *TextToDraw, ScreenLocation.X, ScreenLocation.Y);
		}
	}

	FORCEINLINE void DrawTextCentered(UCanvas* Canvas, UFont* Font, const FString& TextToDraw, const FVector& WorldLocation)
	{
		if (PointInFrustum(Canvas, WorldLocation))
		{
			const FVector ScreenLocation = Canvas->Project(WorldLocation);
			float TextXL = 0.f;
			float TextYL = 0.f;
			Canvas->StrLen(Font, TextToDraw, TextXL, TextYL);
			Canvas->DrawText(Font, *TextToDraw, ScreenLocation.X - TextXL / 2.0f, ScreenLocation.Y - TextYL / 2.0f);
		}
	}

	FORCEINLINE void DrawTextShadowed(UCanvas* Canvas, UFont* Font, const FString& TextToDraw, const FVector& WorldLocation)
	{
		if (PointInFrustum(Canvas, WorldLocation))
		{
			const FVector ScreenLocation = Canvas->Project(WorldLocation);
			float TextXL = 0.f;
			float TextYL = 0.f;
			Canvas->StrLen(Font, TextToDraw, TextXL, TextYL);
			Canvas->SetDrawColor(FColor::Black);
			Canvas->DrawText(Font, *TextToDraw, 1 + ScreenLocation.X - TextXL / 2.0f, 1 + ScreenLocation.Y - TextYL / 2.0f);
			Canvas->SetDrawColor(FColor::White);
			Canvas->DrawText(Font, *TextToDraw, ScreenLocation.X - TextXL / 2.0f, ScreenLocation.Y - TextYL / 2.0f);
		}
	}
}

void FVisualLoggerCanvasRenderer::OnItemSelectionChanged(const struct FVisualLogEntry& EntryItem)
{
	SelectedEntry = EntryItem;
}

void FVisualLoggerCanvasRenderer::ObjectSelectionChanged(TSharedPtr<class STimeline> TimeLine)
{
	CurrentTimeLine = TimeLine;
}

void FVisualLoggerCanvasRenderer::DrawOnCanvas(class UCanvas* Canvas, class APlayerController*)
{
	if (GEngine == NULL || CurrentTimeLine.IsValid() == false)
	{
		return;
	}

	UWorld* World = FLogVisualizer::Get().GetWorld();
	if (World == NULL)
	{
		return;
	}

	UFont* Font = GEngine->GetSmallFont();
	FCanvasTextItem TextItem(FVector2D::ZeroVector, FText::GetEmpty(), Font, FLinearColor::White);

	const FString TimeStampString = FString::Printf(TEXT("%.2f"), SelectedEntry.TimeStamp);
	LogVisualizer::DrawTextShadowed(Canvas, Font, TimeStampString, SelectedEntry.Location);

	for (const auto CurrentData : SelectedEntry.DataBlocks)
	{
		const FName TagName = CurrentData.TagName;
		const bool bIsValidByFilter = FCategoryFiltersManager::Get().MatchCategoryFilters(CurrentData.Category.ToString(), ELogVerbosity::All) && FCategoryFiltersManager::Get().MatchCategoryFilters(CurrentData.TagName.ToString(), ELogVerbosity::All);
		FVisualLogExtensionInterface* Extension = FVisualLogger::Get().GetExtensionForTag(TagName);
		if (!Extension)
		{
			continue;
		}

		if (!bIsValidByFilter)
		{
			Extension->DisableDrawingForData(FLogVisualizer::Get().GetWorld(), Canvas, NULL, TagName, CurrentData, SelectedEntry.TimeStamp);
		}
		else
		{
			Extension->DrawData(FLogVisualizer::Get().GetWorld(), Canvas, NULL, TagName, CurrentData, SelectedEntry.TimeStamp);
		}
	}

	if (ULogVisualizerSessionSettings::StaticClass()->GetDefaultObject<ULogVisualizerSessionSettings>()->bEnableGraphsVisualization)
	{
		DrawHistogramGraphs(Canvas, NULL);
	}
}

void FVisualLoggerCanvasRenderer::DrawHistogramGraphs(class UCanvas* Canvas, class APlayerController*)
{
	struct FGraphLineData
	{
		FName DataName;
		FVector2D LeftExtreme, RightExtreme;
		TArray<FVector2D> Samples;
	};

	struct FGraphData
	{
		FGraphData() : Min(FVector2D(FLT_MAX, FLT_MAX)), Max(FVector2D(FLT_MIN, FLT_MIN)) {}

		FVector2D Min, Max;
		TMap<FName, FGraphLineData> GraphLines;
	};

	if (FLogVisualizer::Get().GetTimeSliderController().IsValid() == false)
	{
		return;
	}

	TMap<FName, FGraphData>	CollectedGraphs;

	const FVisualLoggerTimeSliderArgs&  TimeSliderArgs = FLogVisualizer::Get().GetTimeSliderController()->GetTimeSliderArgs();
	TRange<float> LocalViewRange = TimeSliderArgs.ViewRange.Get();
	const float LocalViewRangeMin = LocalViewRange.GetLowerBoundValue();
	const float LocalViewRangeMax = LocalViewRange.GetUpperBoundValue();
	const float LocalSequenceLength = LocalViewRangeMax - LocalViewRangeMin;
	const float WindowHalfWidth = LocalSequenceLength * TimeSliderArgs.CursorSize.Get() * 0.5f;
	const FVector2D TimeStampWindow(SelectedEntry.TimeStamp - WindowHalfWidth, SelectedEntry.TimeStamp + WindowHalfWidth);

	const TArray<FVisualLogDevice::FVisualLogEntryItem> &ObjectItems = CurrentTimeLine.Pin()->GetEntries();
	int32 ColorIndex = 0;
	int32 LeftSideOutsideIndex = INDEX_NONE;
	int32 RightSideOutsideIndex = INDEX_NONE;

	for (int32 EntryIndex = 0; EntryIndex < ObjectItems.Num(); ++EntryIndex)
	{
		const FVisualLogEntry* CurrentEntry = &(ObjectItems[EntryIndex].Entry);
		if (CurrentEntry->TimeStamp < TimeStampWindow.X)
		{
			LeftSideOutsideIndex = EntryIndex;
			continue;
		}

		if (CurrentEntry->TimeStamp > TimeStampWindow.Y)
		{
			RightSideOutsideIndex = EntryIndex;
			break;
		}

		const int32 SamplesNum = CurrentEntry->HistogramSamples.Num();
		for (int32 SampleIndex = 0; SampleIndex < SamplesNum; ++SampleIndex)
		{
			FVisualLogHistogramSample CurrentSample = CurrentEntry->HistogramSamples[SampleIndex];

			const FName CurrentCategory = CurrentSample.Category;
			const FName CurrentGraphName = CurrentSample.GraphName;
			const FName CurrentDataName = CurrentSample.DataName;

			FString GraphFilterName = CurrentSample.GraphName.ToString() +TEXT("$") + CurrentSample.DataName.ToString();
			const bool bIsValidByFilter = FCategoryFiltersManager::Get().MatchCategoryFilters(GraphFilterName, ELogVerbosity::All);

			if (bIsValidByFilter)
			{
				FGraphData &GraphData = CollectedGraphs.FindOrAdd(CurrentSample.GraphName);
				FGraphLineData &LineData = GraphData.GraphLines.FindOrAdd(CurrentSample.DataName);
				LineData.DataName = CurrentSample.DataName;
				LineData.Samples.Add(CurrentSample.SampleValue);

				GraphData.Min.X = FMath::Min(GraphData.Min.X, CurrentSample.SampleValue.X);
				GraphData.Min.Y = FMath::Min(GraphData.Min.Y, CurrentSample.SampleValue.Y);

				GraphData.Max.X = FMath::Max(GraphData.Max.X, CurrentSample.SampleValue.X);
				GraphData.Max.Y = FMath::Max(GraphData.Max.Y, CurrentSample.SampleValue.Y);
			}
		}
	}
	
	const int32 ExtremeValueIndexes[] = { LeftSideOutsideIndex != INDEX_NONE ? LeftSideOutsideIndex : 0, RightSideOutsideIndex != INDEX_NONE ? RightSideOutsideIndex : ObjectItems.Num() - 1 };
	for (int32 ObjectIndex = 0; ObjectIndex < 2; ++ObjectIndex)
	{
		const FVisualLogEntry* CurrentEntry = &(ObjectItems[ExtremeValueIndexes[ObjectIndex]].Entry);
		const int32 SamplesNum = CurrentEntry->HistogramSamples.Num();
		for (int32 SampleIndex = 0; SampleIndex < SamplesNum; ++SampleIndex)
		{
			FVisualLogHistogramSample CurrentSample = CurrentEntry->HistogramSamples[SampleIndex];

			const FName CurrentCategory = CurrentSample.Category;
			const FName CurrentGraphName = CurrentSample.GraphName;
			const FName CurrentDataName = CurrentSample.DataName;

			FString GraphFilterName = CurrentSample.GraphName.ToString() + TEXT("_") + CurrentSample.DataName.ToString();
			const bool bIsValidByFilter = FCategoryFiltersManager::Get().MatchCategoryFilters(GraphFilterName, ELogVerbosity::All);
			if (bIsValidByFilter)
			{
				FGraphData &GraphData = CollectedGraphs.FindOrAdd(CurrentSample.GraphName);
				FGraphLineData &LineData = GraphData.GraphLines.FindOrAdd(CurrentSample.DataName);
				LineData.DataName = CurrentSample.DataName;
				if (ObjectIndex == 0)
					LineData.LeftExtreme = CurrentSample.SampleValue;
				else
					LineData.RightExtreme = CurrentSample.SampleValue;
			}
		}
	}

	const float GoldenRatioConjugate = 0.618033988749895f;
	if (CollectedGraphs.Num() > 0)
	{
		const FColor GraphsBackgroundColor = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->GraphsBackgroundColor;
		const int NumberOfGraphs = CollectedGraphs.Num();
		const int32 NumberOfColumns = FMath::CeilToInt(FMath::Sqrt(NumberOfGraphs));
		int32 NumberOfRows = FMath::FloorToInt(NumberOfGraphs / NumberOfColumns);
		if (NumberOfGraphs - NumberOfRows * NumberOfColumns > 0)
		{
			NumberOfRows += 1;
		}

		const int32 MaxNumberOfGraphs = FMath::Max(NumberOfRows, NumberOfColumns);
		const float GraphWidth = 0.8f / NumberOfColumns;
		const float GraphHeight = 0.8f / NumberOfRows;

		const float XGraphSpacing = 0.2f / (MaxNumberOfGraphs + 1);
		const float YGraphSpacing = 0.2f / (MaxNumberOfGraphs + 1);

		const float StartX = XGraphSpacing;
		float StartY = 0.5 + (0.5 * NumberOfRows - 1) * (GraphHeight + YGraphSpacing);

		float CurrentX = StartX;
		float CurrentY = StartY;
		int32 GraphIndex = 0;
		int32 CurrentColumn = 0;
		int32 CurrentRow = 0;
		bool bDrawExtremesOnGraphs = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->bDrawExtremesOnGraphs;
		for (auto It(CollectedGraphs.CreateIterator()); It; ++It)
		{
			TWeakObjectPtr<UReporterGraph> HistogramGraph = Canvas->GetReporterGraph();
			if (!HistogramGraph.IsValid())
			{
				break;
			}
			HistogramGraph->SetNumGraphLines(It->Value.GraphLines.Num());
			int32 LineIndex = 0;
			UFont* Font = GEngine->GetSmallFont();
			int32 MaxStringSize = 0;
			float Hue = 0;

			auto& CategoriesForGraph = UsedGraphCategories.FindOrAdd(It->Key.ToString());

			It->Value.GraphLines.KeySort(TLess<FName>());
			for (auto LinesIt(It->Value.GraphLines.CreateConstIterator()); LinesIt; ++LinesIt)
			{
				const FString DataName = LinesIt->Value.DataName.ToString();
				int32 CategoryIndex = CategoriesForGraph.Find(DataName);
				if (CategoryIndex == INDEX_NONE)
				{
					CategoryIndex = CategoriesForGraph.AddUnique(DataName);
				}
				Hue = CategoryIndex * GoldenRatioConjugate;
				if (Hue > 1)
				{
					Hue -= FMath::FloorToFloat(Hue);
				}

				HistogramGraph->GetGraphLine(LineIndex)->Color = FLinearColor::FGetHSV(Hue * 255, 0, 244);
				HistogramGraph->GetGraphLine(LineIndex)->LineName = DataName;
				HistogramGraph->GetGraphLine(LineIndex)->Data.Append(LinesIt->Value.Samples);
				HistogramGraph->GetGraphLine(LineIndex)->LeftExtreme = LinesIt->Value.LeftExtreme;
				HistogramGraph->GetGraphLine(LineIndex)->RightExtreme = LinesIt->Value.RightExtreme;

				int32 DummyY, StringSizeX;
				StringSize(Font, StringSizeX, DummyY, *LinesIt->Value.DataName.ToString());
				MaxStringSize = StringSizeX > MaxStringSize ? StringSizeX : MaxStringSize;

				++LineIndex;
			}

			FVector2D GraphSpaceSize;
			GraphSpaceSize.Y = GraphSpaceSize.X = 0.8f / CollectedGraphs.Num();

			HistogramGraph->SetGraphScreenSize(CurrentX, CurrentX + GraphWidth, CurrentY, CurrentY + GraphHeight);
			CurrentX += GraphWidth + XGraphSpacing;
			HistogramGraph->SetAxesMinMax(FVector2D(TimeStampWindow.X, It->Value.Min.Y), FVector2D(TimeStampWindow.Y, It->Value.Max.Y));

			HistogramGraph->DrawCursorOnGraph(true);
			HistogramGraph->UseTinyFont(CollectedGraphs.Num() >= 5);
			HistogramGraph->SetCursorLocation(SelectedEntry.TimeStamp);
			HistogramGraph->SetNumThresholds(0);
			HistogramGraph->SetStyles(EGraphAxisStyle::Grid, EGraphDataStyle::Lines);
			HistogramGraph->SetBackgroundColor(GraphsBackgroundColor);
			HistogramGraph->SetLegendPosition(/*bShowHistogramLabelsOutside*/ false ? ELegendPosition::Outside : ELegendPosition::Inside);
			HistogramGraph->OffsetDataSets(/*bOffsetDataSet*/false);
			HistogramGraph->DrawExtremesOnGraph(bDrawExtremesOnGraphs);
			HistogramGraph->bVisible = true;
			HistogramGraph->Draw(Canvas);

			++GraphIndex;

			if (++CurrentColumn >= NumberOfColumns)
			{
				CurrentColumn = 0;
				CurrentRow++;

				CurrentX = StartX;
				CurrentY -= GraphHeight + YGraphSpacing;
			}
		}
	}
}
