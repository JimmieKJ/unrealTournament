// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Enumerates possible states of slow running tasks.
 */
enum class ESlowTaskStates
{
	/** Task has been aborted. */
	Aborted,

	/** Task has completed execution. */
	Completed,

	/** Task execution is still pending. */
	Pending,

	/** Task is executing. */
	Running,
};


/**
 * Interface for slow running tasks.
 *
 * A slow running task is a unit of work that may take a considerable amount of time to
 * complete, i.e. several seconds, minutes or even hours. This interface provides mechanisms
 * for tracking and aborting such tasks.
 *
 * Classes that implement this interface should make a best effort to provide a completion
 * percentage via the GetPercentageComplete() method. If this value is expensive to compute,
 * implementors should regularly cache the values. If a completion percentage cannot be
 * computed for some reason, the return value should be unset.
 *
 * Slow running tasks may also provide a status text that can be polled by the user interface.
 */
class ISlowTask
{
public:

	/** Abort this task. */
	virtual void Abort() = 0;

	/**
	 * Gets the task's completion percentage, if available (0.0 to 1.0, or unset).
	 *
	 * @return Completion percentage value.
	 * @see IsComplete
	 */
	virtual TOptional<float> GetPercentageComplete() = 0;

	/**
	 * Gets the task's current status text.
	 *
	 * @return Status text.
	 */
	virtual FText GetStatusText() = 0;

	/**
	 * Gets the current state of the task.
	 *
	 * @return Task state.
	 */
	virtual ESlowTaskStates GetTaskState() = 0;

public:

	/** A delegate that is executed after the task is complete. */
	DECLARE_EVENT_OneParam(ISlowTask, FOnCompleted, bool /*Success*/)
	virtual FOnCompleted& OnComplete() = 0;

	/** A delegate that is executed after the task is complete. */
	DECLARE_EVENT_OneParam(ISlowTask, FOnStatusChanged, FText /*StatusText*/)
	virtual FOnStatusChanged& OnStatusChanged() = 0;
};
