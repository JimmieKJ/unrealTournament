// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_FANCY_TEXT

#include "BaseTextLayoutMarshaller.h"
#include "ITextDecorator.h"
#include "IRichTextMarkupParser.h"
#include "IRichTextMarkupWriter.h"

/**
 * Get/set the raw text to/from a text layout as rich text
 */
class SLATE_API FRichTextLayoutMarshaller : public FBaseTextLayoutMarshaller
{
public:

	static TSharedRef< FRichTextLayoutMarshaller > Create(TArray< TSharedRef< ITextDecorator > > InDecorators, const ISlateStyle* const InDecoratorStyleSet);
	static TSharedRef< FRichTextLayoutMarshaller > Create(TSharedPtr< IRichTextMarkupParser > InParser, TSharedPtr< IRichTextMarkupWriter > InWriter, TArray< TSharedRef< ITextDecorator > > InDecorators, const ISlateStyle* const InDecoratorStyleSet);

	virtual ~FRichTextLayoutMarshaller();

	// ITextLayoutMarshaller
	virtual void SetText(const FString& SourceString, FTextLayout& TargetTextLayout) override;
	virtual void GetText(FString& TargetString, const FTextLayout& SourceTextLayout) override;

	/**
	 * Append an inline decorator to this marshaller
	 */
	inline FRichTextLayoutMarshaller& AppendInlineDecorator(const TSharedRef< ITextDecorator >& DecoratorToAdd)
	{
		InlineDecorators.Add(DecoratorToAdd);
		return *this;
	}

protected:

	FRichTextLayoutMarshaller(TSharedPtr< IRichTextMarkupParser > InParser, TSharedPtr< IRichTextMarkupWriter > InWriter, TArray< TSharedRef< ITextDecorator > > InDecorators, const ISlateStyle* const InDecoratorStyleSet);

	TSharedPtr< ITextDecorator > TryGetDecorator(const FString& Line, const FTextRunParseResults& TextRun) const;

	/** The parser used to resolve any markup used in the provided string. */
	TSharedPtr< IRichTextMarkupParser > Parser;

	/** The writer used to recreate any markup needed to preserve the styled runs. */
	TSharedPtr< IRichTextMarkupWriter > Writer;

	/** Any decorators that should be used while parsing the text. */
	TArray< TSharedRef< ITextDecorator > > Decorators;

	/** Additional decorators can be appended inline. Inline decorators get precedence over decorators not specified inline. */
	TArray< TSharedRef< ITextDecorator > > InlineDecorators;

	/** The style set used for looking up styles used by decorators */
	const ISlateStyle* DecoratorStyleSet;

};

#endif //WITH_FANCY_TEXT
