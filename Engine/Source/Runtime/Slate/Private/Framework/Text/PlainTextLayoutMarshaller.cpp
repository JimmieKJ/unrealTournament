// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "PlainTextLayoutMarshaller.h"
#include "SlateTextLayout.h"
#include "SlatePasswordRun.h"

TSharedRef< FPlainTextLayoutMarshaller > FPlainTextLayoutMarshaller::Create()
{
	return MakeShareable(new FPlainTextLayoutMarshaller());
}

FPlainTextLayoutMarshaller::~FPlainTextLayoutMarshaller()
{
}

void FPlainTextLayoutMarshaller::SetIsPassword(const TAttribute<bool>& InIsPassword)
{
	bIsPassword = InIsPassword;
}

void FPlainTextLayoutMarshaller::SetText(const FString& SourceString, FTextLayout& TargetTextLayout)
{
	const FTextBlockStyle& DefaultTextStyle = static_cast<FSlateTextLayout&>(TargetTextLayout).GetDefaultTextStyle();

	TArray<FTextRange> LineRanges;
	FTextRange::CalculateLineRangesFromString(SourceString, LineRanges);

	TArray<FTextLayout::FNewLineData> LinesToAdd;
	LinesToAdd.Reserve(LineRanges.Num());

	const bool bUsePasswordRun = bIsPassword.Get(false);
	for (const FTextRange& LineRange : LineRanges)
	{
		TSharedRef<FString> LineText = MakeShareable(new FString(SourceString.Mid(LineRange.BeginIndex, LineRange.Len())));

		TArray<TSharedRef<IRun>> Runs;
		if (bUsePasswordRun)
		{
			Runs.Add(FSlatePasswordRun::Create(FRunInfo(), LineText, DefaultTextStyle));
		}
		else
		{
			Runs.Add(FSlateTextRun::Create(FRunInfo(), LineText, DefaultTextStyle));
		}

		LinesToAdd.Emplace(MoveTemp(LineText), MoveTemp(Runs));
	}

	TargetTextLayout.AddLines(LinesToAdd);
}

void FPlainTextLayoutMarshaller::GetText(FString& TargetString, const FTextLayout& SourceTextLayout)
{
	SourceTextLayout.GetAsText(TargetString);
}

FPlainTextLayoutMarshaller::FPlainTextLayoutMarshaller()
{
	bIsPassword = false;
}
