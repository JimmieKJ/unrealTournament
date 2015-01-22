// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

/* Dependencies
*****************************************************************************/
#include "Engine.h"
#include "Messaging.h"
#include "ModuleManager.h"
#include "SlateBasics.h"
#include "SlateStyle.h"
#include "EditorStyle.h"

/* Private includes
*****************************************************************************/
#include "VisualLogger/VisualLogger.h"

DECLARE_DELEGATE_OneParam(FOnItemSelectionChanged, const FVisualLogDevice::FVisualLogEntryItem&);
DECLARE_DELEGATE_OneParam(FOnObjectSelectionChanged, TSharedPtr<class STimeline>);
DECLARE_DELEGATE_OneParam(FOnFiltersSearchChanged, const FText&);
DECLARE_DELEGATE(FOnFiltersChanged);

struct FVisualLoggerEvents
{
	FVisualLoggerEvents() 
	{
	
	}

	FOnItemSelectionChanged OnItemSelectionChanged;
	FOnFiltersChanged OnFiltersChanged;
	FOnObjectSelectionChanged OnObjectSelectionChanged;
};

struct IVisualLoggerInterface
{
	virtual ~IVisualLoggerInterface() {}
	virtual bool HasValidCategories(TArray<FVisualLoggerCategoryVerbosityPair> Categories) = 0;
	virtual bool IsValidCategory(const FString& InCategoryName, TEnumAsByte<ELogVerbosity::Type> Verbosity = ELogVerbosity::All) = 0;
	virtual bool IsValidCategory(const FString& InGraphName, const FString& InDataName, TEnumAsByte<ELogVerbosity::Type> Verbosity = ELogVerbosity::All) = 0;
	virtual FLinearColor GetCategoryColor(FName Category) = 0;
	virtual UWorld* GetWorld() const = 0;
	virtual class AActor* GetVisualLoggerHelperActor() = 0;

	const FVisualLoggerEvents& GetVisualLoggerEvents() { return VisualLoggerEvents; }
	class FSequencerTimeSliderController* GetTimeSliderController() { return TimeSliderController.Get(); }
	void SetTimeSliderController(TSharedPtr<class FSequencerTimeSliderController> InTimeSliderController) { TimeSliderController = InTimeSliderController; }

protected:
	FVisualLoggerEvents VisualLoggerEvents;
	TSharedPtr<class FSequencerTimeSliderController> TimeSliderController;
};


#include "LogVisualizerStyle.h"
#include "SVisualLogger.h"
#include "SVisualLoggerToolbar.h"
#include "VisualLoggerCommands.h"
#include "SVisualLoggerFilters.h"
#include "SVisualLoggerView.h"
#include "SVisualLoggerLogsList.h"
#include "SVisualLoggerStatusView.h"
#include "STimeline.h"
#include "LogVisualizerSettings.h"
#include "LogVisualizerSessionSettings.h"