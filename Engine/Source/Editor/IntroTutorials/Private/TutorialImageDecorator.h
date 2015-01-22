// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "TextDecorators.h"

class FTutorialImageDecorator : public ITextDecorator
{
public:

	static TSharedRef< FTutorialImageDecorator > Create();
	virtual ~FTutorialImageDecorator() {}

public:

	virtual bool Supports( const FTextRunParseResults& RunParseResult, const FString& Text ) const override;

	virtual TSharedRef< ISlateRun > Create(const TSharedRef<class FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResult, const FString& OriginalText, const TSharedRef< FString >& InOutModelText, const ISlateStyle* Style) override;

	static FString GetPathToImage(const FString& InSrcMetaData);

private:

	FTutorialImageDecorator();
};