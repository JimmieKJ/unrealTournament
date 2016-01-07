// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseTextLayoutMarshaller.h"

class FChatItemTransparency
	: public TSharedFromThis<FChatItemTransparency>
{
public:
	FChatItemTransparency(TSharedRef<SWidget> InOwningWidget, TSharedRef<class FChatDisplayService> InChatDisplayService);

	void FadeIn()
	{
		FadeCurveSequence.Play(OwningWidget);
	}

	void FadeOut(bool AutoFade = false);

	float GetTransparency();

private:

	TSharedRef<SWidget> OwningWidget;
	FCurveSequence FadeCurveSequence;
	FCurveHandle FadeCurve;
	bool ShouldFadeOut;
	bool CachedMinimize;
	bool CachedAutoFaded;

	TSharedRef<class FChatDisplayService> ChatDisplayService;
};

class FChatTextLayoutMarshaller
	: public FBaseTextLayoutMarshaller
{
public:
	virtual ~FChatTextLayoutMarshaller() {}

	virtual void AddUserNameHyperlinkDecorator(const TSharedRef<class FChatHyperlinkDecorator>& InNameHyperlinkDecorator) = 0;
	virtual bool AppendMessage(const TSharedPtr<class FChatItemViewModel>& NewMessage, bool GroupText) = 0;
	virtual bool AppendLineBreak() = 0;
	virtual void SetOwningWidget(TSharedRef<SWidget> InOwningWidget) = 0;
};

/**
 * Creates the implementation for an FChatDisplayService.
 *
 * @return the newly created FChatDisplayService implementation.
 */
FACTORY(TSharedRef< FChatTextLayoutMarshaller >, FChatTextLayoutMarshaller,
	const FFriendsAndChatStyle* const InDecoratorStyleSet,
	const struct FTextBlockStyle& TextStyle,
	bool InIsMultiChat,
	FVector2D TimeStampMeasure,
	const TSharedRef<class FChatDisplayService>& ChatDisplayService);