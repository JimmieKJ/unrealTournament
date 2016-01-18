// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScriptEventExecution.h"

//////////////////////////////////////////////////////////////////////////
// SProfilerStatRow

/** Enum for each column type in the execution tree widget */
namespace EBlueprintProfilerStat
{
	enum Type
	{
		Name = 0,
		TotalTime,
		InclusiveTime,
		ExclusiveTime,
		MaxTime,
		MinTime,
		Samples
	};
};

class SProfilerStatRow : public SMultiColumnTableRow<FBPProfilerStatPtr>
{
protected:

	/** The profiler statistic associated with this stat row */
	FBPProfilerStatPtr ItemToEdit;

public:

	SLATE_BEGIN_ARGS(SProfilerStatRow){}
	SLATE_END_ARGS()

	/** Constructs the statistic row item widget */
	void Construct(const FArguments& InArgs, TSharedRef<STableViewBase> OwnerTableView, FBPProfilerStatPtr InItemToEdit);

	/** Generates and returns a widget for the specified column name and the associated statistic */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

	/** Converts the profiler stat enum into the associated column FName */
	static const FName GetStatName(const EBlueprintProfilerStat::Type StatId);

	/** Converts the profiler stat enum into the associated column FText */
	static const FText GetStatText(const EBlueprintProfilerStat::Type StatId);

};

//////////////////////////////////////////////////////////////////////////
// FBPPerformanceData

struct FBPPerformanceData
{
public:

	FBPPerformanceData()
		: Samples(StatSampleSize)
		, CachedExclusiveTime(0.0)
		, MaxTime(-MAX_dbl)
		, MinTime(MAX_dbl)
		, TotalTime(0.0)
		, NumSamples(0)
		, bDirty(false)
	{
	}

	/** Add a single event node timing into the dataset */
	void AddEventTiming(const double NodeTiming);

	/** Update and average the current timings from the buffer */
	void Update();

	/** Reset the current data buffer and all derived data stats */
	void Reset();

	/** Returns if this data structure has any valid data */
	bool IsDataValid() const { return MaxTime != -MAX_dbl && MinTime != MAX_dbl; }

	/** Marks the data is dirty and signals an update is required */
	void MarkDataDirty() { bDirty = true; }

	/** Signals that the data is dirty and requires and update */
	bool IsDataDirty() const { return bDirty; }

	/** Returns the various stats this container holds */
	double GetMaxTime() const { return MaxTime; }
	double GetMinTime() const { return MinTime; }
	double GetTotalTime() const { return TotalTime; }
	double GetExclusiveTime() const { return CachedExclusiveTime; }
	int32 GetSampleCount() const { return NumSamples; }
	int32 GetSafeSampleCount() const { return Samples.Num() ? Samples.Num() : 1; }

public:

	/** Controls how many samples are used to generate the averaged samples */
	static int32 StatSampleSize;

private:

	/** Sample buffer */
	TSimpleRingBuffer<double> Samples;
	/** Cached exclusive timing */
	double CachedExclusiveTime;
	/** Max exclusive timing */
	double MaxTime;
	/** Min exclusive timing */
	double MinTime;
	/** Total time accrued */
	double TotalTime;
	/** The current inclusive sample count */
	int32 NumSamples;
	/** Flag to indicate the data wants the average recalculated */
	bool bDirty;

};

//////////////////////////////////////////////////////////////////////////
// FBPProfilerStat

class FBPProfilerStat : public TSharedFromThis<FBPProfilerStat>
{
public:

	FBPProfilerStat(FBPProfilerStatPtr InParentStat)
		: InclusiveTime(0.0)
		, MinInclusiveTime(MAX_dbl)
		, MaxInclusiveTime(-MAX_dbl)
		, InclusiveTotalTime(0.0)
		, InclusiveSamples(0)
		, ParentStat(InParentStat)
	{
	}

	FBPProfilerStat(FBPProfilerStatPtr InParentStat, FScriptExecEvent& Context)
		: InclusiveTime(0.0)
		, MinInclusiveTime(MAX_dbl)
		, MaxInclusiveTime(-MAX_dbl)
		, InclusiveTotalTime(0.0)
		, InclusiveSamples(0)
		, ObjectContext(Context)
		, ParentStat(InParentStat)
	{
		UpdateStatName();
	}

	virtual ~FBPProfilerStat() {}

	/** Release Data */
	void ReleaseData()
	{
		// Need to investigate why this needs to be called manually.
		for (auto Iter : CachedChildren)
		{
			Iter.Value->ReleaseData();
			Iter.Value.Reset();
		}
		ParentStat.Reset();
	}

	/** Find context entry point based on the passed in exec event */
	FBPProfilerStatPtr FindEntryPoint(FScriptExecEvent& EventEntry);

	/** Returns the parent container statistic */
	FBPProfilerStatPtr GetParent() { return ParentStat; }

	/** Find or add a child stat based in exec context */
	virtual FBPProfilerStatPtr FindOrAddChildByContext(FScriptExecEvent& Context);

	/** Gather children for list/tree widget */
	void GatherChildren(TArray<FBPProfilerStatPtr>& OutChildren);

	/** Update the current profiling statistics */
	virtual void UpdateProfilingStats();

	/** Returns if this stat has valid data */
	bool HasValidData() { return PerformanceStats.IsDataValid(); }

	/** Generate name widget */
	TSharedRef<SWidget> GenerateNameWidget();

	/** Create text attribute for column */
	TAttribute<FText> GetTextAttribute(FName ColumnName) const;

	/** Returns the exec path object context for this statistic */
	FScriptExecEvent& GetObjectContext() { return ObjectContext; }

	/** Submits new event data to this profiler statistic */
	virtual void SubmitData(class FScriptExecEvent& EventData);

	/** Returns the call depth for this statistic */
	int32 GetCallDepth() const { return ObjectContext.IsValid() ? ObjectContext.GetCallDepth() : -1; }

	// Return current statisic values with safety checking */
	virtual double GetMaxTime() const;
	virtual double GetMinTime() const;
	float GetTotalTime() const;
	float GetInclusiveTotalTime() const;
	float GetInclusiveTime() const;
	float GetExclusiveTime() const;
	float GetSampleCount() const;
	float GetSafeSampleCount() const;
	int32 GetInclusiveSampleCount() const { return InclusiveSamples; }

protected:

	friend class FBPProfilerBranchStat;

	/** Cache the statistic text from the exec event context */
	virtual void UpdateStatName();

	/** Returns the slate icon from the node referenced by the exec event */
	const FSlateBrush* GetIcon();

	/** Returns the icon color override */
	FLinearColor GetIconColor() const;

	/** Returns the statistic tool tip */
	const FText GetToolTip();

	/** Navigates to the exec event node in the graph */
	void OnNavigateToObject();

	/** Returns the various statistic text values for the stats */
	virtual FText GetInclusiveTimeText() const;
	virtual FText GetExclusiveTimeText() const;
	virtual FText GetTotalTimeText() const;
	virtual FText GetMaxTimeText() const;
	virtual FText GetMinTimeText() const;
	virtual FText GetSamplesText() const;

protected:

	/** Local profiling statistics */
	FBPPerformanceData PerformanceStats;
	/** Container profiling statistics */
	double InclusiveTime;
	double MinInclusiveTime;
	double MaxInclusiveTime;
	double InclusiveTotalTime;
	int32 InclusiveSamples;
	/** Script execution event context */
	FScriptExecEvent ObjectContext;
	/** Cached statistic name */
	FText StatName;
	/** Statistic icon color */
	FLinearColor IconColor;
	/** Parent statistic */
	FBPProfilerStatPtr ParentStat;
	/** Child statistics */
	TMap<TWeakObjectPtr<const UObject>, FBPProfilerStatPtr> CachedChildren;

};

//////////////////////////////////////////////////////////////////////////
// FBPProfilerBranchStat

class FBPProfilerBranchStat : public FBPProfilerStat
{
public:

	FBPProfilerBranchStat(FBPProfilerStatPtr InParentStat, FScriptExecEvent& Context)
		: FBPProfilerStat(InParentStat, Context)
		, bAreExecutionPathsSequential(false)
	{
	}

	virtual ~FBPProfilerBranchStat() {}

	//~ FBPProfilerStat Start
	virtual FBPProfilerStatPtr FindOrAddChildByContext(FScriptExecEvent& Context) override;
	void CreateChildrenForPins();
	virtual void UpdateProfilingStats() override;
	virtual double GetMaxTime() const override;
	virtual double GetMinTime() const override;
	virtual void SubmitData(class FScriptExecEvent& EventData) override;
	//~ FBPProfilerStat End

protected:

	/** Returns whether different branches execute in sequence rather exclusively */
	bool IsBranchSequential();

private:

	/** Cached exec pin */
	TWeakObjectPtr<const UObject> CachedExecPin;

	/** Signals that execution paths are sequential */
	bool bAreExecutionPathsSequential;

};