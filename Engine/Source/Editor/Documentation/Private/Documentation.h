// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IDocumentation.h"

class FDocumentation : public IDocumentation
{
public:

	static TSharedRef< IDocumentation > Create();

public:

	virtual ~FDocumentation();

	virtual bool OpenHome() const override;

	virtual bool OpenHome(const FCultureRef& Culture) const override;

	virtual bool OpenAPIHome() const override;

	virtual bool Open( const FString& Link ) const override;

	virtual bool Open(const FString& Link, const FCultureRef& Culture) const override;

	virtual TSharedRef< SWidget > CreateAnchor( const TAttribute<FString>& Link, const FString& PreviewLink = FString(), const FString& PreviewExcerptName = FString() ) const override;

	virtual TSharedRef< IDocumentationPage > GetPage( const FString& Link, const TSharedPtr< FParserConfiguration >& Config, const FDocumentationStyle& Style = FDocumentationStyle() ) override;

	virtual bool PageExists(const FString& Link) const override;

	virtual bool PageExists(const FString& Link, const FCultureRef& Culture) const override;

	virtual TSharedRef< class SToolTip > CreateToolTip( const TAttribute<FText>& Text, const TSharedPtr<SWidget>& OverrideContent, const FString& Link, const FString& ExcerptName ) const override;

private:

	FDocumentation();

private:

	TMap< FString, TWeakPtr< IDocumentationPage > > LoadedPages;
};
