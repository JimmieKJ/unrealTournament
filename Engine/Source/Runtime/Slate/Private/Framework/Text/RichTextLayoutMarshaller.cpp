// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "RichTextLayoutMarshaller.h"
#include "RichTextMarkupProcessing.h"
#include "SlateTextLayout.h"

#if WITH_FANCY_TEXT

TSharedRef< FRichTextLayoutMarshaller > FRichTextLayoutMarshaller::Create(TArray< TSharedRef< ITextDecorator > > InDecorators, const ISlateStyle* const InDecoratorStyleSet)
{
	return MakeShareable(new FRichTextLayoutMarshaller(MoveTemp(InDecorators), InDecoratorStyleSet));
}

TSharedRef< FRichTextLayoutMarshaller > FRichTextLayoutMarshaller::Create(TSharedPtr< IRichTextMarkupParser > InParser, TSharedPtr< IRichTextMarkupWriter > InWriter, TArray< TSharedRef< ITextDecorator > > InDecorators, const ISlateStyle* const InDecoratorStyleSet)
{
	return MakeShareable(new FRichTextLayoutMarshaller(MoveTemp(InParser), MoveTemp(InWriter), MoveTemp(InDecorators), InDecoratorStyleSet));
}

FRichTextLayoutMarshaller::~FRichTextLayoutMarshaller()
{
}

void FRichTextLayoutMarshaller::SetText(const FString& SourceString, FTextLayout& TargetTextLayout)
{
	const FTextBlockStyle& DefaultTextStyle = static_cast<FSlateTextLayout&>(TargetTextLayout).GetDefaultTextStyle();

	TArray<FTextLineParseResults> LineParseResultsArray;
	FString ProcessedString;
	Parser->Process(LineParseResultsArray, SourceString, ProcessedString);

	TArray<FTextLayout::FNewLineData> LinesToAdd;
	LinesToAdd.Reserve(LineParseResultsArray.Num());

	// Iterate through parsed line results and create processed lines with runs.
	for (const FTextLineParseResults& LineParseResults : LineParseResultsArray)
	{
		TSharedRef<FString> ModelString = MakeShareable(new FString());
		TArray< TSharedRef< IRun > > Runs;

		for (const FTextRunParseResults& RunParseResult : LineParseResults.Runs)
		{
			AppendRunsForText(RunParseResult, ProcessedString, DefaultTextStyle, ModelString, TargetTextLayout, Runs);
		}

		LinesToAdd.Emplace(MoveTemp(ModelString), MoveTemp(Runs));
	}

	TargetTextLayout.AddLines(LinesToAdd);
}

void FRichTextLayoutMarshaller::GetText(FString& TargetString, const FTextLayout& SourceTextLayout)
{
	const auto& SourceLineModels = SourceTextLayout.GetLineModels();

	TArray<IRichTextMarkupWriter::FRichTextLine> WriterLines;
	WriterLines.Reserve(SourceLineModels.Num());

	for (const FTextLayout::FLineModel& LineModel : SourceLineModels)
	{
		IRichTextMarkupWriter::FRichTextLine WriterLine;
		WriterLine.Runs.Reserve(LineModel.Runs.Num());

		for (const FTextLayout::FRunModel& RunModel : LineModel.Runs)
		{
			FString RunText;
			RunModel.AppendTextTo(RunText);
			WriterLine.Runs.Emplace(IRichTextMarkupWriter::FRichTextRun(RunModel.GetRun()->GetRunInfo(), RunText));
		}

		WriterLines.Add(WriterLine);
	}

	Writer->Write(WriterLines, TargetString);
}

FRichTextLayoutMarshaller::FRichTextLayoutMarshaller(TArray< TSharedRef< ITextDecorator > > InDecorators, const ISlateStyle* const InDecoratorStyleSet)
	: Parser(MoveTemp(FDefaultRichTextMarkupParser::Create()))
	, Writer(MoveTemp(FDefaultRichTextMarkupWriter::Create()))
	, Decorators(MoveTemp(InDecorators))
	, InlineDecorators()
	, DecoratorStyleSet(InDecoratorStyleSet)
{
}

FRichTextLayoutMarshaller::FRichTextLayoutMarshaller(TSharedPtr< IRichTextMarkupParser > InParser, TSharedPtr< IRichTextMarkupWriter > InWriter, TArray< TSharedRef< ITextDecorator > > InDecorators, const ISlateStyle* const InDecoratorStyleSet)
	: Parser(MoveTemp(InParser))
	, Writer(MoveTemp(InWriter))
	, Decorators(MoveTemp(InDecorators))
	, InlineDecorators()
	, DecoratorStyleSet(InDecoratorStyleSet)
{
}

TSharedPtr< ITextDecorator > FRichTextLayoutMarshaller::TryGetDecorator(const FString& Line, const FTextRunParseResults& TextRun) const
{
	for (auto DecoratorIter = InlineDecorators.CreateConstIterator(); DecoratorIter; ++DecoratorIter)
	{
		if ((*DecoratorIter)->Supports(TextRun, Line))
		{
			return *DecoratorIter;
		}
	}

	for (auto DecoratorIter = Decorators.CreateConstIterator(); DecoratorIter; ++DecoratorIter)
	{
		if ((*DecoratorIter)->Supports(TextRun, Line))
		{
			return *DecoratorIter;
		}
	}

	return nullptr;
}

void FRichTextLayoutMarshaller::AppendRunsForText(const FTextRunParseResults& TextRun,
	const FString& ProcessedString,
	const FTextBlockStyle& DefaultTextStyle,
	const TSharedRef<FString>& InOutModelText,
	FTextLayout& TargetTextLayout,
	TArray<TSharedRef<IRun>>& Runs)
{
	TSharedPtr< ISlateRun > Run;
	TSharedPtr< ITextDecorator > Decorator = TryGetDecorator(ProcessedString, TextRun);

	if (Decorator.IsValid())
	{
		// Create run and update model string.
		Run = Decorator->Create(TargetTextLayout.AsShared(), TextRun, ProcessedString, InOutModelText, DecoratorStyleSet);
	}
	else
	{
		FRunInfo RunInfo(TextRun.Name);
		for (const TPair<FString, FTextRange>& Pair : TextRun.MetaData)
		{
			RunInfo.MetaData.Add(Pair.Key, ProcessedString.Mid(Pair.Value.BeginIndex, Pair.Value.EndIndex - Pair.Value.BeginIndex));
		}

		const FTextBlockStyle* TextBlockStyle;
		FTextRange ModelRange;
		ModelRange.BeginIndex = InOutModelText->Len();
		if (!(TextRun.Name.IsEmpty()) && DecoratorStyleSet->HasWidgetStyle< FTextBlockStyle >(FName(*TextRun.Name)))
		{
			*InOutModelText += ProcessedString.Mid(TextRun.ContentRange.BeginIndex, TextRun.ContentRange.EndIndex - TextRun.ContentRange.BeginIndex);
			TextBlockStyle = &(DecoratorStyleSet->GetWidgetStyle< FTextBlockStyle >(FName(*TextRun.Name)));
		}
		else
		{
			*InOutModelText += ProcessedString.Mid(TextRun.OriginalRange.BeginIndex, TextRun.OriginalRange.EndIndex - TextRun.OriginalRange.BeginIndex);
			TextBlockStyle = &DefaultTextStyle;
		}
		ModelRange.EndIndex = InOutModelText->Len();

		// Create run.
		Run = FSlateTextRun::Create(RunInfo, InOutModelText, *TextBlockStyle, ModelRange);
	}

	Runs.Add(Run.ToSharedRef());
}

#endif //WITH_FANCY_TEXT
