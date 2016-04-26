// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

/* Dependencies
*****************************************************************************/
#include "Engine.h"
#include "Messaging.h"
#include "ModuleManager.h"
#include "SlateBasics.h"
#include "SlateStyle.h"
#include "EditorStyle.h"
#include "SDockTab.h"

/* Private includes
*****************************************************************************/
#include "ILogVisualizer.h"
#include "VisualLogger/VisualLogger.h"
#include "LogVisualizerSettings.h"
#include "LogVisualizerSessionSettings.h"
#include "VisualLoggerDatabase.h"

DECLARE_DELEGATE_OneParam(FOnFiltersSearchChanged, const FText&);

//DECLARE_DELEGATE(FOnFiltersChanged);
DECLARE_MULTICAST_DELEGATE(FOnFiltersChanged);

DECLARE_DELEGATE_ThreeParams(FOnLogLineSelectionChanged, TSharedPtr<struct FLogEntryItem> /*SelectedItem*/, int64 /*UserData*/, FName /*TagName*/);
DECLARE_DELEGATE_RetVal_TwoParams(FReply, FOnKeyboardEvent, const FGeometry& /*MyGeometry*/, const FKeyEvent& /*InKeyEvent*/);
DECLARE_DELEGATE_RetVal(float, FGetAnimationOutlinerFillPercentageFunc);

struct FVisualLoggerEvents
{
	FOnFiltersChanged OnFiltersChanged;
	FOnLogLineSelectionChanged OnLogLineSelectionChanged;
	FOnKeyboardEvent OnKeyboardEvent;
	FGetAnimationOutlinerFillPercentageFunc GetAnimationOutlinerFillPercentageFunc;
};

class FVisualLoggerTimeSliderController;
struct LOGVISUALIZER_API FLogVisualizer
{
	/** LogVisualizer interface*/
	void Reset();

	FLinearColor GetColorForCategory(int32 Index) const;
	FLinearColor GetColorForCategory(const FString& InFilterName) const;
	TSharedPtr<FVisualLoggerTimeSliderController> GetTimeSliderController() { return TimeSliderController; }
	UWorld* GetWorld(UObject* OptionalObject = nullptr);
	FVisualLoggerEvents& GetEvents() { return VisualLoggerEvents; }

	void SetCurrentVisualizer(TSharedPtr<class SVisualLogger> Visualizer) { CurrentVisualizer = Visualizer; }

	void SetAnimationOutlinerFillPercentage(float FillPercentage) { AnimationOutlinerFillPercentage = FillPercentage; }
	float GetAnimationOutlinerFillPercentage() 
	{
		if (VisualLoggerEvents.GetAnimationOutlinerFillPercentageFunc.IsBound())
		{
			SetAnimationOutlinerFillPercentage(VisualLoggerEvents.GetAnimationOutlinerFillPercentageFunc.Execute());
		}
		return AnimationOutlinerFillPercentage; 
	}

	int32 GetNextItem(FName RowName, int32 MoveDistance = 1);
	int32 GetPreviousItem(FName RowName, int32 MoveDistance = 1);

	void GotoNextItem(FName RowName, int32 MoveDistance = 1);
	void GotoPreviousItem(FName RowName, int32 MoveDistance = 1);
	void GotoFirstItem(FName RowName);
	void GotoLastItem(FName RowName);

	void UpdateCameraPosition(FName Rowname, int32 ItemIndes);

	/** Static access */
	static void Initialize();
	static void Shutdown();
	static FLogVisualizer& Get();
protected:
	static TSharedPtr< struct FLogVisualizer > StaticInstance;
	
	TSharedPtr<FVisualLoggerTimeSliderController> TimeSliderController;
	FVisualLoggerEvents VisualLoggerEvents;
	TWeakPtr<class SVisualLogger> CurrentVisualizer;
	float AnimationOutlinerFillPercentage;
};

class SVisualLoggerTab : public SDockTab
{
public:
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		return FLogVisualizer::Get().GetEvents().OnKeyboardEvent.Execute(MyGeometry, InKeyEvent);
	}
};

class SVisualLoggerBaseWidget : public SCompoundWidget
{
public:
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		return FLogVisualizer::Get().GetEvents().OnKeyboardEvent.Execute(MyGeometry, InKeyEvent);
	}
};


#include "LogVisualizerStyle.h"
#include "SVisualLogger.h"
#include "SVisualLoggerToolbar.h"
#include "VisualLoggerCommands.h"
#include "SVisualLoggerFilters.h"
#include "SVisualLoggerView.h"
#include "SVisualLoggerLogsList.h"
#include "SVisualLoggerSectionOverlay.h"
#include "SVisualLoggerStatusView.h"
#include "SVisualLoggerTimeline.h"
#include "SVisualLoggerTimelineBar.h"
#include "SVisualLoggerTimelinesContainer.h"
#include "SVisualLoggerTimeSlider.h"
#include "VisualLoggerTimeSliderController.h"