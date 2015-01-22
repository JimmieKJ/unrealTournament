// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class SDocumentationAnchor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SDocumentationAnchor )
	{}
		SLATE_ARGUMENT( FString, PreviewLink )
		SLATE_ARGUMENT( FString, PreviewExcerptName )

		/** The string for the link to follow when clicked  */
		SLATE_ATTRIBUTE( FString, Link )

	SLATE_END_ARGS()


	void Construct(const FArguments& InArgs);

private:
	
	const FSlateBrush* GetButtonImage() const;

	FReply OnClicked() const;

private:

	TAttribute<FString> Link;
	TSharedPtr< SButton > Button;
	TSharedPtr< SImage > ButtonImage;
	const FSlateBrush* Default; 
	const FSlateBrush* Hovered; 
	const FSlateBrush* Pressed; 

	TSharedPtr< IDocumentationPage > DocumentationPage;
};