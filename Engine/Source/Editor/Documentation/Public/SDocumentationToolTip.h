// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "IDocumentationPage.h"

class SDocumentationToolTip : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS( SDocumentationToolTip )
		: _Text()
		, _Style( TEXT("Documentation.SDocumentationTooltip") )
		, _ColorAndOpacity( FLinearColor( 1.0f, 1.0f, 1.0f, 1.0f ) )
		, _Content()
		{}

		/** The text displayed in this tool tip */
		SLATE_ATTRIBUTE( FText, Text )

		/** The text style to use for this tool tip */
		SLATE_ARGUMENT( FName, Style )
		
		/** Font color and opacity */
		SLATE_ATTRIBUTE( FSlateColor, ColorAndOpacity )

		/**  */
		SLATE_ARGUMENT( FString, DocumentationLink )

		/**  */
		SLATE_ARGUMENT( FString, ExcerptName )

		/** Arbitrary content to be displayed in the tool tip; overrides any text that may be set. */
		SLATE_DEFAULT_SLOT( FArguments, Content )

	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	bool IsInteractive() const;

	virtual const FText& GetTextTooltip() const
	{
		return TextContent.Get();
	}

private:

	void ConstructSimpleTipContent();

	void ConstructFullTipContent();

	FReply ReloadDocumentation();

	void CreateExcerpt( FString FileSource, FString ExcerptName );

private:

	/** Text block widget */
	TAttribute< FText > TextContent;
	TSharedPtr< SWidget > OverrideContent;
	FTextBlockStyle StyleInfo;
	TAttribute< FSlateColor > ColorAndOpacity;

	/** The link to the documentation */
	FString DocumentationLink;
	FString ExcerptName;

	/** Content widget */
	TSharedPtr< SBorder > WidgetContent;

	TSharedPtr< SWidget > SimpleTipContent;
	bool IsDisplayingDocumentationLink;

	TSharedPtr< SWidget > FullTipContent;

	TSharedPtr< IDocumentationPage > DocumentationPage;
	bool IsShowingFullTip;
};
