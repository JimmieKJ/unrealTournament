// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 
#pragma once

class SLATE_API SErrorHint
	: public SCompoundWidget
	, public IErrorReportingWidget
{
public:

	SLATE_BEGIN_ARGS( SErrorHint )
		: _ErrorText()
		{}
		SLATE_TEXT_ARGUMENT(ErrorText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:

	// IErrorReportingWidget interface

	virtual void SetError( const FText& InErrorText ) override;
	virtual void SetError( const FString& InErrorText ) override;
	virtual bool HasError() const override;
	virtual TSharedRef<SWidget> AsWidget() override;

private:

	TAttribute<EVisibility> CustomVisibility;
	EVisibility MyVisibility() const;

	FVector2D GetDesiredSizeScale() const;
	FCurveSequence ExpandAnimation;

	TSharedPtr<SWidget> ImageWidget;
	FText ErrorText;
	FText GetErrorText() const;
};
