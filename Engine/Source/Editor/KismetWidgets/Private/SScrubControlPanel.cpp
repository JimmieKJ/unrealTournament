// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "KismetWidgetsPrivatePCH.h"
#include "SScrubControlPanel.h"
#include "Editor/EditorWidgets/Public/EditorWidgetsModule.h"


#define LOCTEXT_NAMESPACE "SScrubControlPanel"

void SScrubControlPanel::Construct( const SScrubControlPanel::FArguments& InArgs )
{
	ScrubWidget = NULL;

	IsRealtimeStreamingMode = InArgs._IsRealtimeStreamingMode;
	
	FEditorWidgetsModule& EditorWidgetsModule = FModuleManager::Get().LoadModuleChecked<FEditorWidgetsModule>( "EditorWidgets" );
	
	FTransportControlArgs TransportControlArgs;
	TransportControlArgs.OnForwardPlay = InArgs._OnClickedForwardPlay;
	TransportControlArgs.OnRecord = InArgs._OnClickedRecord;
	TransportControlArgs.OnBackwardPlay = InArgs._OnClickedBackwardPlay;
	TransportControlArgs.OnForwardStep = InArgs._OnClickedForwardStep;
	TransportControlArgs.OnBackwardStep = InArgs._OnClickedBackwardStep;
	TransportControlArgs.OnForwardEnd = InArgs._OnClickedForwardEnd;
	TransportControlArgs.OnBackwardEnd = InArgs._OnClickedBackwardEnd;
	TransportControlArgs.OnToggleLooping = InArgs._OnClickedToggleLoop;
	TransportControlArgs.OnGetLooping = InArgs._OnGetLooping;
	TransportControlArgs.OnGetPlaybackMode = InArgs._OnGetPlaybackMode;
	TransportControlArgs.OnTickPlayback = InArgs._OnTickPlayback;
	
	FTransportControlArgs TransportControlArgsForRealtimeStreamingMode;
	TransportControlArgsForRealtimeStreamingMode.OnForwardPlay = TransportControlArgs.OnForwardPlay;
	TransportControlArgsForRealtimeStreamingMode.OnForwardStep = TransportControlArgs.OnForwardStep;
	TransportControlArgsForRealtimeStreamingMode.OnGetPlaybackMode = TransportControlArgs.OnGetPlaybackMode;

	this->ChildSlot
	.Padding( FMargin( 0.0f, 1.0f) )
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Fill) 
		.VAlign(VAlign_Center)
		.FillWidth(1)
		.Padding( FMargin( 0.0f, 0.0f) )
		[
			SNew( SBorder )
			[
				SAssignNew(ScrubWidget, SScrubWidget)
				.Value(InArgs._Value)
				.NumOfKeys(InArgs._NumOfKeys)
				.SequenceLength(InArgs._SequenceLength)
				.OnValueChanged(InArgs._OnValueChanged)
				.OnBeginSliderMovement(InArgs._OnBeginSliderMovement)
				.OnEndSliderMovement(InArgs._OnEndSliderMovement)
				.ViewInputMin(InArgs._ViewInputMin)
				.ViewInputMax(InArgs._ViewInputMax)
				.OnSetInputViewRange(InArgs._OnSetInputViewRange)
				.OnCropAnimSequence(InArgs._OnCropAnimSequence)
				.OnAddAnimSequence(InArgs._OnAddAnimSequence)
				.OnAppendAnimSequence(InArgs._OnAppendAnimSequence)
				.OnReZeroAnimSequence(InArgs._OnReZeroAnimSequence)
				.bAllowZoom(InArgs._bAllowZoom)
				.DraggableBars(InArgs._DraggableBars)
				.OnBarDrag(InArgs._OnBarDrag)
			]
		]

		// Padding
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			// Padding to make controls line up with the track label widths.
			// note: a more robust way to accomplish this would be nice.
			SNew(SSpacer)
			.Size(FVector2D(16.0f, 16.0f))
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Padding(0)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.Visibility(this, &SScrubControlPanel::GetRealtimeControlVisibility, false)
			[
				EditorWidgetsModule.CreateTransportControl(TransportControlArgs)
			]
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Padding(0)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.Visibility(this, &SScrubControlPanel::GetRealtimeControlVisibility, true)
			[
				EditorWidgetsModule.CreateTransportControl(TransportControlArgsForRealtimeStreamingMode)
			]
		]
	];
}

EVisibility SScrubControlPanel::GetRealtimeControlVisibility(bool bIsControlForRealtimeStreamingMode) const
{
	return (IsRealtimeStreamingMode.Get() == bIsControlForRealtimeStreamingMode)
		? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
