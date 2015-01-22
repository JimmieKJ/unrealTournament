// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

struct FVisualLoggerCanvasRenderer
{
	FVisualLoggerCanvasRenderer(TSharedPtr<IVisualLoggerInterface> InVisualLoggerInterface) 
		: VisualLoggerInterface(InVisualLoggerInterface) 
	{}

	void DrawOnCanvas(class UCanvas* Canvas, class APlayerController*);
	void OnItemSelectionChanged(const struct FVisualLogEntry& EntryItem);
	void ObjectSelectionChanged(TSharedPtr<class STimeline> TimeLine);

protected:
	void DrawHistogramGraphs(class UCanvas* Canvas, class APlayerController* );

private:
	FVisualLogEntry SelectedEntry;
	TSharedPtr<IVisualLoggerInterface> VisualLoggerInterface;
	TMap<FString, TArray<FString> > UsedGraphCategories;
	TWeakPtr<class STimeline> CurrentTimeLine;
};
