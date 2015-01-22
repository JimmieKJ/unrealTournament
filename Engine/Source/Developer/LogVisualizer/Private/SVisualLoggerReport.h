// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
class SVisualLoggerReport : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVisualLoggerReport) {}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TArray< TSharedPtr<class STimeline> >&, TSharedPtr<class SVisualLoggerView> VisualLoggerView);

	virtual ~SVisualLoggerReport();

protected:
	void GenerateReportText();

protected:
	TArray< TSharedPtr<class STimeline> > SelectedItems;
	TArray<TSharedRef<ITextDecorator>> Decorators;
	TSharedPtr<class SRichTextBlock> InteractiveRichText;
	FText ReportText;
	TArray<FString> CollectedEvents;
};
