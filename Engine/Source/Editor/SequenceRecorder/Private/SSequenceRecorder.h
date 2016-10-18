// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorRecording.h"

class SSequenceRecorder : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SSequenceRecorder)
	{
	}

	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

private:

	void BindCommands();

	TSharedRef<ITableRow> MakeListViewWidget(class UActorRecording* Recording, const TSharedRef<STableViewBase>& OwnerTable) const;

	FText GetRecordingActorName(class UActorRecording* Recording) const;

	void OnSelectionChanged(UActorRecording* Recording, ESelectInfo::Type SelectionType) const;

	void HandleRecord();

	EActiveTimerReturnType StartDelayedRecord(double InCurrentTime, float InDeltaTime);

	bool CanRecord() const;

	bool IsRecordVisible() const;

	void HandleStopAll();

	bool CanStopAll() const;

	bool IsStopAllVisible() const;

	void HandleAddRecording();

	bool CanAddRecording() const;

	void HandleRemoveRecording();

	bool CanRemoveRecording() const;

	EActiveTimerReturnType HandleRefreshItems(double InCurrentTime, float InDeltaTime);

	TOptional<float> GetDelayPercent() const;

	void OnDelayChanged(float NewValue);

	EVisibility GetDelayProgressVisibilty() const;

	FText GetTargetSequenceName() const;

private:
	TSharedPtr<IDetailsView> SequenceRecordingDetailsView;

	TSharedPtr<IDetailsView> ActorRecordingDetailsView;

	TSharedPtr<SListView<UActorRecording*>> ListView;

	TSharedPtr<FUICommandList> CommandList;

	/** The handle to the refresh tick timer */
	TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;

	TSharedPtr<SProgressBar> DelayProgressBar;
};
