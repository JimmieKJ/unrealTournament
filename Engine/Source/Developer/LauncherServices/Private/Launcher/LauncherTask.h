// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Abstract base class for launcher tasks.
 */
class FLauncherTask
	: public FRunnable
	, public ILauncherTask
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InName - The name of the task.
	 * @param InDependency - An optional task that this task depends on.
	 */
	FLauncherTask( const FString& InName, const FString& InDesc, void* InReadPipe, void* InWritePipe)
		: Name(InName)
		, Desc(InDesc)
		, Status(ELauncherTaskStatus::Pending)
		, ReadPipe(InReadPipe)
		, WritePipe(InWritePipe)
		, Result(0)
	{ }

public:

	/**
	 * Adds a task that will execute after this task completed.
	 *
	 * Continuations must be added before this task starts.
	 *
	 * @param Task - The task to add.
	 */
	void AddContinuation( const TSharedPtr<FLauncherTask>& Task )
	{
		if (Status == ELauncherTaskStatus::Pending)
		{
			Continuations.AddUnique(Task);
		}	
	}

	/**
	 * Executes the tasks.
	 *
	 * @param ChainState - State data of the task chain.
	 */
	void Execute( const FLauncherTaskChainState& ChainState )
	{
		check(Status == ELauncherTaskStatus::Pending);

		LocalChainState = ChainState;

		Thread = FRunnableThread::Create(this, TEXT("FLauncherTask"));

		if (Thread == nullptr)
		{

		}
	}

	/**
	 * Gets the list of tasks to be executed after this task.
	 *
	 * @return Continuation task list.
	 */
	virtual const TArray<TSharedPtr<FLauncherTask> >& GetContinuations( ) const
	{
		return Continuations;
	}

	/**
	 * Checks whether the task chain has finished execution.
	 *
	 * A task chain is finished when this task and all its continuations are finished.
	 *
	 * @return true if the chain is finished, false otherwise.
	 */
	bool IsChainFinished( ) const
	{
		if (IsFinished())
		{
			for (int32 ContinuationIndex = 0; ContinuationIndex < Continuations.Num(); ++ContinuationIndex)
			{
				if (!Continuations[ContinuationIndex]->IsChainFinished())
				{
					return false;
				}
			}

			return true;
		}

		return false;
	}

	bool Succeeded() const
	{
		if (IsChainFinished())
		{
			for (int32 ContinuationIndex = 0; ContinuationIndex < Continuations.Num(); ++ContinuationIndex)
			{
				if (!Continuations[ContinuationIndex]->Succeeded())
				{
					return false;
				}
			}
		}

		return Status == ELauncherTaskStatus::Completed;
	}

	int32 ReturnCode() const override
	{
		if (IsChainFinished())
		{
			for (int32 ContinuationIndex = 0; ContinuationIndex < Continuations.Num(); ++ContinuationIndex)
			{
				if (Continuations[ContinuationIndex]->ReturnCode() != 0)
				{
					return Continuations[ContinuationIndex]->ReturnCode();
				}
			}
		}

		return Result;
	}

public:

	// Begin FRunnable interface

	virtual bool Init( ) override
	{
		return true;
	}

	virtual uint32 Run( ) override
	{
		Status = ELauncherTaskStatus::Busy;

		StartTime = FDateTime::UtcNow();

		FPlatformProcess::Sleep(0.5f);

		TaskStarted.Broadcast(Name);
		if (PerformTask(LocalChainState))
		{
			Status = ELauncherTaskStatus::Completed;

			TaskCompleted.Broadcast(Name);
			ExecuteContinuations();
		}
		else
		{
			if (Status == ELauncherTaskStatus::Canceling)
			{
				Status = ELauncherTaskStatus::Canceled;
			}
			else
			{
				Status = ELauncherTaskStatus::Failed;
			}

			TaskCompleted.Broadcast(Name);

			CancelContinuations();
		}

		EndTime = FDateTime::UtcNow();
		
		return 0;
	}

	virtual void Stop( ) override
	{
		Cancel();
	}

	virtual void Exit( ) override { }

	// End FRunnable interface

public:

	// Begin ILauncherTask interface

	virtual void Cancel( ) override
	{
		if (Status == ELauncherTaskStatus::Busy)
		{
			Status = ELauncherTaskStatus::Canceling;
		}
		else if (Status == ELauncherTaskStatus::Pending)
		{
			Status = ELauncherTaskStatus::Canceled;
		}

		CancelContinuations();
	}

	virtual FTimespan GetDuration( ) const override
	{
		if (Status == ELauncherTaskStatus::Pending)
		{
			return FTimespan::Zero();
		}

		if (Status == ELauncherTaskStatus::Busy)
		{
			return (FDateTime::UtcNow() - StartTime);
		}

		return (EndTime - StartTime);
	}

	virtual const FString& GetName( ) const override
	{
		return Name;
	}

	virtual const FString& GetDesc( ) const override
	{
		return Desc;
	}

	virtual ELauncherTaskStatus::Type GetStatus( ) const override
	{
		return Status;
	}

	virtual bool IsFinished( ) const override
	{
		return ((Status == ELauncherTaskStatus::Canceled) ||
				(Status == ELauncherTaskStatus::Completed) ||
				(Status == ELauncherTaskStatus::Failed));
	}

	virtual FOnTaskStartedDelegate& OnStarted() override
	{
		return TaskStarted;
	}

	virtual FOnTaskCompletedDelegate& OnCompleted() override
	{
		return TaskCompleted;
	}

	// End ILauncherTask interface

protected:

	/**
	 * Performs the actual task.
	 *
	 * @param ChainState - Holds the state of the task chain.
	 *
	 * @return true if the task completed successfully, false otherwise.
	 */
	virtual bool PerformTask( FLauncherTaskChainState& ChainState ) = 0;

protected:

	/**
	 * Cancels all continuation tasks.
	 */
	void CancelContinuations( ) const
	{
		for (int32 ContinuationIndex = 0; ContinuationIndex < Continuations.Num(); ++ContinuationIndex)
		{
			Continuations[ContinuationIndex]->Cancel();
		}
	}

	/**
	 * Executes all continuation tasks.
	 */
	void ExecuteContinuations( ) const
	{
		for (int32 ContinuationIndex = 0; ContinuationIndex < Continuations.Num(); ++ContinuationIndex)
		{
			Continuations[ContinuationIndex]->Execute(LocalChainState);
		}
	}

private:

	// Holds the tasks that execute after this task completed.
	TArray<TSharedPtr<FLauncherTask> > Continuations;

	// Holds the time at which the task ended.
	FDateTime EndTime;

	// Holds the local state of this task chain.
	FLauncherTaskChainState LocalChainState;

	// Holds the task's name.
	FString Name;

	// Holds the task's description
	FString Desc;

	// Holds the time at which the task started.
	FDateTime StartTime;

	// Holds the status of this task
	ELauncherTaskStatus::Type Status;

	// Holds the thread that's running this task.
	FRunnableThread* Thread;

	// message delegates
	FOnTaskStartedDelegate TaskStarted;
	FOnTaskCompletedDelegate TaskCompleted;

protected:

	// read and write pipe
	void* ReadPipe;
	void* WritePipe;

	// result
	int32 Result;
};
