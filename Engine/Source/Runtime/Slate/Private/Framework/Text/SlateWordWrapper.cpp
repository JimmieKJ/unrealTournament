// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "SlateWordWrapper.h"

/** Information about the substring as it would appear as a result of soft-wrapping a line of text. */
struct FWrappedStringSlice
{
	/** 
		* Create a substring info that sources InSourceString, begins at InFirstCharIndex and ends at InLastCharIndex (exclusive).
		* The substring measures InSize.X by InSize.Y.
		*/
	FWrappedStringSlice( FString& InSourceString, int32 InFirstCharIndex, int32 InLastCharIndex, FVector2D InSize )
	: SourceString( InSourceString )
	, FirstCharIndex( InFirstCharIndex )
	, LastCharIndex( InLastCharIndex )
	, Size( InSize )
	{
	}

	/** The substring sources this string. */
	FString& SourceString;
	/** Index of the first character in the substring */
	int32 FirstCharIndex;
	/** Index of the last character (exclusive) */
	int32 LastCharIndex;
	/** The measured size of this substring given the current font and point size. */
	FVector2D Size;
};

namespace
{
	class FSlateWordWrapperBase : public FWordWrapper
	{
	protected:
		FSlateWordWrapperBase(const FString& InString, const FSlateFontInfo& InFontInfo, const int32 InWrapWidth, const float InFontScale, FWrappedLineData* const OutWrappedLineData);
		bool DoesSubstringFit(const int32 StartIndex, const int32 EndIndex);
		int32 FindIndexAtOrAfterWrapWidth(const int32 StartIndex);
	protected:
		const FSlateFontInfo& FontInfo;
		const int32 WrapWidth;
		const float FontScale;
	};

	bool FSlateWordWrapperBase::DoesSubstringFit(const int32 StartIndex, const int32 EndIndex)
	{
		const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		FVector2D Size( FontMeasureService->Measure( String, StartIndex, EndIndex, FontInfo, false, FontScale ) );
		return Size.X <= WrapWidth;
	}

	int32 FSlateWordWrapperBase::FindIndexAtOrAfterWrapWidth(const int32 StartIndex)
	{
		FString StringToMeasure(String + StartIndex);
		const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

		int32 Index = FontMeasureService->FindLastWholeCharacterIndexBeforeOffset(StringToMeasure, FontInfo, WrapWidth, FontScale);

		if (Index == INDEX_NONE)
		{
			return StartIndex;
		}

		//This index is inclusive but we need to return an exclusive index so we increment.
		return StartIndex + Index + 1; 
	}

	FSlateWordWrapperBase::FSlateWordWrapperBase(const FString& InString, const FSlateFontInfo& InFontInfo, const int32 InWrapWidth, const float InFontScale, FWrappedLineData* const OutWrappedLineData)
		: FWordWrapper(*InString, InString.Len(), OutWrappedLineData), FontInfo(InFontInfo), WrapWidth(InWrapWidth), FontScale(InFontScale)
	{
	}
}

namespace
{
	class FSlateWordWrapperSingle : public FSlateWordWrapperBase
	{
	public:
		/**
		* Used to generate multi-line/wrapped text.
		*
		* @param InString The unwrapped text.
		* @param InFontInfo The font used to render the text.
		* @param InWrapWidth The width available.
		* @param OutWrappedLineData An optional array to fill with the indices from the source string marking the begin and end points of the wrapped lines
		*
		* @return The index of the character closest to the specified horizontal offset
		*/
		static FString Execute(const FString& InString, const FSlateFontInfo& InFontInfo, const int32 InWrapWidth, const float InFontScale, FWrappedLineData* const OutWrappedLineData);
	protected:
		void AddLine(const int32 StartIndex, const int32 EndIndex);
	private:
		FSlateWordWrapperSingle(const FString& InString, const FSlateFontInfo& InFontInfo, const int32 InWrapWidth, const float InFontScale, FWrappedLineData* const OutWrappedLineData);
	private:
		FString Result;
		bool bPendingNewLine;
	};

	FString FSlateWordWrapperSingle::Execute(const FString& InString, const FSlateFontInfo& InFontInfo, const int32 InWrapWidth, const float InFontScale, FWrappedLineData* const OutWrappedLineData)
	{
		FSlateWordWrapperSingle WordWrapper(InString, InFontInfo, InWrapWidth, InFontScale, OutWrappedLineData);
		for(int i = 0; i < InString.Len(); ++i) // Sanity check: Doesn't seem valid to have more lines than code units.
		{
			if( !WordWrapper.ProcessLine() )
			{
				break;
			}
		}
		return WordWrapper.Result;
	}

	void FSlateWordWrapperSingle::AddLine(const int32 StartIndex, const int32 EndIndex)
	{
		if(bPendingNewLine)
		{
			Result += TEXT("\n");
		}
		FString NewString(EndIndex - StartIndex, String + StartIndex);
		Result += NewString;
		bPendingNewLine = NewString.EndsWith( FString( TEXT("\n") ) ) ? false : true; // Add a new line if the last new line didn't end with a line break.
	}

	FSlateWordWrapperSingle::FSlateWordWrapperSingle(const FString& InString, const FSlateFontInfo& InFontInfo, const int32 InWrapWidth, const float InFontScale, FWrappedLineData* const OutWrappedLineData)
		: FSlateWordWrapperBase(InString, InFontInfo, InWrapWidth, InFontScale, OutWrappedLineData), bPendingNewLine(false)
	{
	}
}

FString SlateWordWrapper::WrapText( const FString& InString, const FSlateFontInfo& InFontInfo, const int32 InWrapWidth, const float InFontScale, FWordWrapper::FWrappedLineData* const OutWrappedLineData )
{
	return FSlateWordWrapperSingle::Execute(InString, InFontInfo, InWrapWidth, InFontScale, OutWrappedLineData);
}

namespace
{
	class FSlateWordWrapperMulti : public FSlateWordWrapperBase
	{
	public:
		/**
		* Used to generate multi-line/wrapped text.
		*
		* @param InString The unwrapped text.
		* @param InFontInfo The font used to render the text.
		* @param InWrapWidth The width available.
		* @param OutString The resulting string with newlines inserted.
		* @param OutWrappedLineData An optional array to fill with the indices from the source string marking the begin and end points of the wrapped lines
		*/
		static void Execute(FString& InString, const FSlateFontInfo& InFontInfo, const int32 InWrapWidth, const float InFontScale, TArray< FWrappedStringSlice >& OutWrappedText, FWrappedLineData* const OutWrappedLineData);
	protected:
		void AddLine(const int32 StartIndex, const int32 EndIndex);
	private:
		FSlateWordWrapperMulti(FString& InString, const FSlateFontInfo& InFontInfo, const int32 InWrapWidth, const float InFontScale, TArray< FWrappedStringSlice >& OutWrappedText, FWrappedLineData* const OutWrappedLineData);
	private:
		FString& StringReference;
		TArray< struct FWrappedStringSlice >& Result;
	};

	void FSlateWordWrapperMulti::Execute(FString& InString, const FSlateFontInfo& InFontInfo, const int32 InWrapWidth, const float InFontScale, TArray< FWrappedStringSlice >& OutWrappedText, FWrappedLineData* const OutWrappedLineData)
	{
		FSlateWordWrapperMulti WordWrapper(InString, InFontInfo, InWrapWidth, InFontScale, OutWrappedText, OutWrappedLineData);
		for(int i = 0; i < InString.Len(); ++i) // Sanity check: Doesn't seem valid to have more lines than code units.
		{
			if( !WordWrapper.ProcessLine() )
			{
				break;
			}
		}
	}

	void FSlateWordWrapperMulti::AddLine(const int32 StartIndex, const int32 EndIndex)
	{
		const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		FVector2D StringSize( FontMeasureService->Measure( StringReference.Mid(StartIndex, EndIndex - StartIndex), FontInfo, FontScale ) );
		Result.Add( FWrappedStringSlice(StringReference, StartIndex, EndIndex, StringSize) );
	}

	FSlateWordWrapperMulti::FSlateWordWrapperMulti(FString& InString, const FSlateFontInfo& InFontInfo, const int32 InWrapWidth, const float InFontScale, TArray< FWrappedStringSlice >& OutWrappedText, FWrappedLineData* const OutWrappedLineData)
		: FSlateWordWrapperBase(InString, InFontInfo, InWrapWidth, InFontScale, OutWrappedLineData), StringReference(InString), Result(OutWrappedText)
	{
	}
}

void SlateWordWrapper::WrapText( const TArray< TSharedRef<FString> >& InTextLines, const FSlateFontInfo& InFontInfo, const float InWrapWidth, const float InFontScale, TArray< FWrappedStringSlice >& OutWrappedText, FWordWrapper::FWrappedLineData* const OutWrappedLineData )
{
	OutWrappedText.Empty();

	// Iterate over every source line, generating wrapping information into OutWrappedText.
	for (int32 LineIndex=0; LineIndex < InTextLines.Num(); ++LineIndex )
	{
		FString& CurrentSourceLine = InTextLines[LineIndex].Get();
		FSlateWordWrapperMulti::Execute(CurrentSourceLine, InFontInfo, InFontScale, InWrapWidth, OutWrappedText, OutWrappedLineData);
	}
}