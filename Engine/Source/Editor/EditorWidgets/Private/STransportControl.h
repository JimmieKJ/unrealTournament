// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class STransportControl : public ITransportControl
{
public:
	SLATE_BEGIN_ARGS(STransportControl)
		: _TransportArgs() {}

		SLATE_ARGUMENT(FTransportControlArgs, TransportArgs)
	SLATE_END_ARGS()
		
	/**
	 * Construct the widget
	 * 
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs );

private:
	const FSlateBrush* GetForwardStatusIcon() const;
	FText GetForwardStatusTooltip() const;
	FText GetRecordStatusTooltip() const;
	const FSlateBrush* GetBackwardStatusIcon() const;
	FSlateColor GetLoopStatusColor() const;

	FReply OnToggleLooping();

private:
	FTransportControlArgs TransportControlArgs;
};
