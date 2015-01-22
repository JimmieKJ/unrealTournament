// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_FANCY_TEXT

#include "BaseTextLayoutMarshaller.h"

/**
 * Get/set the raw text to/from a text layout as plain text
 */
class SLATE_API FPlainTextLayoutMarshaller : public FBaseTextLayoutMarshaller
{
public:

	static TSharedRef< FPlainTextLayoutMarshaller > Create();

	virtual ~FPlainTextLayoutMarshaller();
	
	// ITextLayoutMarshaller
	virtual void SetText(const FString& SourceString, FTextLayout& TargetTextLayout) override;
	virtual void GetText(FString& TargetString, const FTextLayout& SourceTextLayout) override;

protected:

	FPlainTextLayoutMarshaller();

};

#endif //WITH_FANCY_TEXT
