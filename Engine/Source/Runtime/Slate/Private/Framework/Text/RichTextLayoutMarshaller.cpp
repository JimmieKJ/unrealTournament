// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "RichTextLayoutMarshaller.h"
#include "RichTextMarkupProcessing.h"
#include "SlateTextLayout.h"

#if WITH_FANCY_TEXT

TSharedRef< FRichTextLayoutMarshaller > FRichTextLayoutMarshaller::Create(TArray< TSharedRef< ITextDecorator > > InDecorators, const ISlateStyle* const InDecoratorStyleSet)
{
	return MakeShareable(new FRichTextLayoutMarshaller(FDefaultRichTextMarkupParser::Create(), FDefaultRichTextMarkupWriter::Create(), MoveTemp(InDecorators), InDecoratorStyleSet));
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

	// Iterate through parsed line results and create processed lines with runs.
	for (const FTextLineParseResults& LineParseResults : LineParseResultsArray)
	{
		TSharedRef<FString> ModelString = MakeShareable(new FString());
		TArray< TSharedRef< IRun > > Runs;

		for (const FTextRunParseResults& RunParseResult : LineParseResults.Runs)
		{
			TSharedPtr< ISlateRun > Run;

			TSharedPtr< ITextDecorator > Decorator = TryGetDecorator(ProcessedString, RunParseResult);

			if (Decorator.IsValid())
			{
				// Create run and update model string.
				Run = Decorator->Create(TargetTextLayout.AsShared(), RunParseResult, ProcessedString, ModelString, DecoratorStyleSet);
			}
			else
			{
				FRunInfo RunInfo(RunParseResult.Name);
				for (const TPair<FString, FTextRange>& Pair : RunParseResult.MetaData)
				{
					RunInfo.MetaData.Add(Pair.Key, ProcessedString.Mid( Pair.Value.BeginIndex, Pair.Value.EndIndex - Pair.Value.BeginIndex));
				}

				const FTextBlockStyle* TextBlockStyle;
				FTextRange ModelRange;
				ModelRange.BeginIndex = ModelString->Len();
				if (!(RunParseResult.Name.IsEmpty()) && DecoratorStyleSet->HasWidgetStyle< FTextBlockStyle >(FName(*RunParseResult.Name)))
				{
					*ModelString += ProcessedString.Mid(RunParseResult.ContentRange.BeginIndex, RunParseResult.ContentRange.EndIndex - RunParseResult.ContentRange.BeginIndex);
					TextBlockStyle = &(DecoratorStyleSet->GetWidgetStyle< FTextBlockStyle >( FName(*RunParseResult.Name) ));
				}
				else
				{
					*ModelString += ProcessedString.Mid(RunParseResult.OriginalRange.BeginIndex, RunParseResult.OriginalRange.EndIndex - RunParseResult.OriginalRange.BeginIndex);
					TextBlockStyle = &DefaultTextStyle;
				}
				ModelRange.EndIndex = ModelString->Len();

				// Create run.
				Run = FSlateTextRun::Create(RunInfo, ModelString, *TextBlockStyle, ModelRange);
			}

			Runs.Add( Run.ToSharedRef() );
		}

		TargetTextLayout.AddLine(ModelString, Runs);
	}
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

#endif //WITH_FANCY_TEXT
