// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "RichCurveEditorCommands.h"

void SSequencerCurveEditorToolBar::Construct( const FArguments& InArgs, TSharedPtr<FUICommandList> CurveEditorCommandList )
{
	FToolBarBuilder ToolBarBuilder( CurveEditorCommandList, FMultiBoxCustomization::None, TSharedPtr<FExtender>(), Orient_Horizontal, true );
	ToolBarBuilder.BeginSection( "Curve" );
	{
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().ZoomToFitHorizontal );
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().ZoomToFitVertical );
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().ZoomToFitAll );
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().ZoomToFitSelected );
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.BeginSection( "Interpolation" );
	{
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().InterpolationCubicAuto );
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().InterpolationCubicUser );
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().InterpolationCubicBreak );
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().InterpolationLinear );
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().InterpolationConstant );
	}
	ToolBarBuilder.EndSection();

	ChildSlot
	[
		ToolBarBuilder.MakeWidget()
	];
}