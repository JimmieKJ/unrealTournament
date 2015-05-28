// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Future.h"
#include "ISlowTask.h"


/**
 * Template for slow asynchronous results.
 */
template<typename ResultType>
struct TSlowResult
{
	/** Holds the asynchronous return value. */
	TFuture<ResultType> Future;

	/** Pointer to the asynchronous task that is computing the result, or nullptr if the result was returned synchronously. */
	TSharedPtr<ISlowTask> Task;

	/** Default constructor (no initialization). */
	TSlowResult() { }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InFuture The future object to set.
	 * @param InTask The slow task to set.
	 */
	TSlowResult(TFuture<ResultType>&& InFuture, const TSharedRef<ISlowTask>& InTask)
		: Future(InFuture)
		, Task(InTask)
	{ }
};
