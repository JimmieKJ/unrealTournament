// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EditorWidgetsPrivatePCH.h"
#include "STransportControl.h"

#define LOCTEXT_NAMESPACE "STransportControl"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void STransportControl::Construct( const STransportControl::FArguments& InArgs )
{
	TransportControlArgs = InArgs._TransportArgs;
	
	ChildSlot
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Center) 
		.VAlign(VAlign_Center) 
		[
			SNew(SButton)
			. OnClicked(TransportControlArgs.OnBackwardEnd)
			. Visibility(TransportControlArgs.OnBackwardEnd.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			. ToolTipText( LOCTEXT("ToFront", "To Front") )
			. ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			. ContentPadding(2.0f)
			[
				SNew(SImage)
				. Image( FEditorStyle::GetBrush("Animation.Backward_End") )
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Center) 
		.VAlign(VAlign_Center) 
		[
			SNew(SButton)
			. OnClicked(TransportControlArgs.OnBackwardStep)
			. Visibility(TransportControlArgs.OnBackwardStep.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			. ToolTipText( LOCTEXT("ToPrevious", "To Previous") )
			. ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			. ContentPadding(2.0f)
			[
				SNew(SImage)
				. Image( FEditorStyle::GetBrush("Animation.Backward_Step") )
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Center) 
		.VAlign(VAlign_Center) 
		[
			SNew(SButton)
			. OnClicked(TransportControlArgs.OnBackwardPlay)
			. Visibility(TransportControlArgs.OnBackwardPlay.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			. ToolTipText( LOCTEXT("Reverse", "Reverse") )
			. ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			. ContentPadding(2.0f)
			[
				SNew(SImage)
				. Image( this, &STransportControl::GetBackwardStatusIcon )
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Center) 
		.VAlign(VAlign_Center) 
		[
			SNew(SButton)
			. OnClicked(TransportControlArgs.OnRecord)
			. Visibility(TransportControlArgs.OnRecord.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			. ToolTipText( this, &STransportControl::GetRecordStatusTooltip )
			. ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			. ContentPadding(2.0f)
			[
				SNew(SImage)
				. Image( FEditorStyle::GetBrush("Animation.Record") )
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Center) 
		.VAlign(VAlign_Center) 
		[
			SNew(SButton)
			. OnClicked(TransportControlArgs.OnForwardPlay)
			. Visibility(TransportControlArgs.OnForwardPlay.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			. ToolTipText( this, &STransportControl::GetForwardStatusTooltip )
			. ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			. ContentPadding(2.0f)
			[
				SNew(SImage)
				. Image( this, &STransportControl::GetForwardStatusIcon )
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Center) 
		.VAlign(VAlign_Center) 
		[
			SNew(SButton)
			. OnClicked(TransportControlArgs.OnForwardStep)
			. Visibility(TransportControlArgs.OnForwardStep.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			. ToolTipText( LOCTEXT("ToNext", "To Next") )
			. ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			. ContentPadding(2.0f)
			[
				SNew(SImage)
				. Image( FEditorStyle::GetBrush("Animation.Forward_Step") )
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Center) 
		.VAlign(VAlign_Center) 
		[
			SNew(SButton)
			. OnClicked(TransportControlArgs.OnForwardEnd)
			. Visibility(TransportControlArgs.OnForwardEnd.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			. ToolTipText( LOCTEXT("ToEnd", "To End") )
			. ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			. ContentPadding(2.0f)
			[
				SNew(SImage)
				. Image( FEditorStyle::GetBrush("Animation.Forward_End") )
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Center) 
		.VAlign(VAlign_Center) 
		[
			SNew(SButton)
			. OnClicked(TransportControlArgs.OnToggleLooping)
			. Visibility(TransportControlArgs.OnGetLooping.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			. ToolTipText( LOCTEXT("Loop", "Loop") )
			. ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			. ContentPadding(2.0f)
			[
				SNew(SImage)
				.Image( FEditorStyle::GetBrush("Animation.Loop") )
				.ColorAndOpacity( this, &STransportControl::GetLoopStatusColor )
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

bool STransportControl::IsTickable() const
{
	// Only bother if an active timer delegate was provided
	return TransportControlArgs.OnTickPlayback.IsBound() && TransportControlArgs.OnGetPlaybackMode.IsBound();
}

void STransportControl::Tick( float DeltaTime )
{
	const auto PlaybackMode = TransportControlArgs.OnGetPlaybackMode.Execute();
	const bool bIsPlaying = PlaybackMode == EPlaybackMode::PlayingForward || PlaybackMode == EPlaybackMode::PlayingReverse || PlaybackMode == EPlaybackMode::Recording;

	if ( bIsPlaying && !ActiveTimerHandle.IsValid() )
	{
		ActiveTimerHandle = RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateSP( this, &STransportControl::TickPlayback ) );
	}
	else if ( !bIsPlaying && ActiveTimerHandle.IsValid() )
	{
		UnRegisterActiveTimer( ActiveTimerHandle.Pin().ToSharedRef() );
	}
}

const FSlateBrush* STransportControl::GetForwardStatusIcon() const
{
	const auto PlaybackMode = TransportControlArgs.OnGetPlaybackMode.Execute();
	if (TransportControlArgs.OnGetPlaybackMode.IsBound() &&
		 ( PlaybackMode == EPlaybackMode::PlayingForward || PlaybackMode == EPlaybackMode::Recording ) )
	{
		return FEditorStyle::GetBrush("Animation.Pause");
	}

	return FEditorStyle::GetBrush("Animation.Forward");
}

FText STransportControl::GetForwardStatusTooltip() const
{
	if (TransportControlArgs.OnGetPlaybackMode.IsBound() &&
		TransportControlArgs.OnGetPlaybackMode.Execute() == EPlaybackMode::PlayingForward)
	{
		return LOCTEXT("Pause", "Pause");
	}

	return LOCTEXT("Play", "Play");
}

FText STransportControl::GetRecordStatusTooltip() const
{
	if (TransportControlArgs.OnGetPlaybackMode.IsBound() &&
		TransportControlArgs.OnGetPlaybackMode.Execute() == EPlaybackMode::Recording)
	{
		return LOCTEXT("StopRecording", "Stop Recording");
	}

	return LOCTEXT("Record", "Record");
}

const FSlateBrush* STransportControl::GetBackwardStatusIcon() const
{
	if (TransportControlArgs.OnGetPlaybackMode.IsBound() &&
		TransportControlArgs.OnGetPlaybackMode.Execute() == EPlaybackMode::PlayingReverse)
	{
		return FEditorStyle::GetBrush("Animation.Pause");
	}

	return FEditorStyle::GetBrush("Animation.Backward");
}

FSlateColor STransportControl::GetLoopStatusColor() const
{
	if (TransportControlArgs.OnGetLooping.IsBound() &&
		TransportControlArgs.OnGetLooping.Execute())
	{
		return FSlateColor(FLinearColor(1.f, 0.f, 0.f));
	}

	return FSlateColor(FLinearColor(1.f, 1.f, 1.f));
}

EActiveTimerReturnType STransportControl::TickPlayback( double InCurrentTime, float InDeltaTime )
{
	TransportControlArgs.OnTickPlayback.Execute( InCurrentTime, InDeltaTime );
	return EActiveTimerReturnType::Continue;
}

#undef LOCTEXT_NAMESPACE
