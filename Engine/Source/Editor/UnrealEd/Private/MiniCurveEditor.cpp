// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "MiniCurveEditor.h"
#include "SCurveEditor.h"


void SMiniCurveEditor::Construct(const FArguments& InArgs)
{
	ViewMinInput=0.f;
	ViewMaxInput=5.f;

	this->ChildSlot
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
		.Padding(0.0f)
		[
			SAssignNew(TrackWidget, SCurveEditor)
			.ViewMinInput(this, &SMiniCurveEditor::GetViewMinInput)
			.ViewMaxInput(this, &SMiniCurveEditor::GetViewMaxInput)
			.TimelineLength(this, &SMiniCurveEditor::GetTimelineLength)
			.OnSetInputViewRange(this, &SMiniCurveEditor::SetInputViewRange)
			.HideUI(false)
			.AlwaysDisplayColorCurves(true)
		]
	];

	check(TrackWidget.IsValid());
	TrackWidget->SetCurveOwner(InArgs._CurveOwner);

	WidgetWindow = InArgs._ParentWindow;

	UObject* SelectedCurve = InArgs._CurveOwner->GetOwner();
	FAssetEditorManager::Get().NotifyAssetOpened(SelectedCurve, this);	
}

SMiniCurveEditor::~SMiniCurveEditor()
{
	FAssetEditorManager::Get().NotifyEditorClosed(this);
}

float SMiniCurveEditor::GetTimelineLength() const
{
	return 0.f;
}


void SMiniCurveEditor::SetInputViewRange(float InViewMinInput, float InViewMaxInput)
{
	ViewMaxInput = InViewMaxInput;
	ViewMinInput = InViewMinInput;
}

FName SMiniCurveEditor::GetEditorName() const
{
	return FName("MiniCurveEditor");
}

void SMiniCurveEditor::FocusWindow(UObject* ObjectToFocusOn)
{	
	if(WidgetWindow.IsValid())
	{
		//WidgetWindow.Pin()->ShowWindow();
		WidgetWindow.Pin()->BringToFront(true);
	}
}

bool SMiniCurveEditor::CloseWindow()
{
	if(WidgetWindow.IsValid())
	{
		WidgetWindow.Pin()->RequestDestroyWindow();
	}

	return true;
}

