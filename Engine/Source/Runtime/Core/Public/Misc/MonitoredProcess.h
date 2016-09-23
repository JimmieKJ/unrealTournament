// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Declares a delegate that is executed when a monitored process completed.
 *
 * The first parameter is the process return code.
 */
DECLARE_DELEGATE_OneParam(FOnMonitoredProcessCompleted, int32)

/**
 * Declares a delegate that is executed when a monitored process produces output.
 *
 * The first parameter is the produced output.
 */
DECLARE_DELEGATE_OneParam(FOnMonitoredProcessOutput, FString)


/**
 * Implements an external process that can be monitored.
 */
class CORE_API FMonitoredProcess
	: public FRunnable
{
public:

	/**
	 * Creates a new monitored process.
	 *
	 * @param InURL The URL of the executable to launch.
	 * @param InParams The command line parameters.
	 * @param InHidden Whether the window of the process should be hidden.
	 */
	FMonitoredProcess( const FString& InURL, const FString& InParams, bool InHidden, bool InCreatePipes = true );

	/** Destructor. */
	~FMonitoredProcess();

public:

	/**
	 * Cancels the process.
	 *
	 * @param InKillTree Whether to kill the entire process tree when canceling this process.
	 */
	void Cancel( bool InKillTree = false )
	{
		Canceling = true;
		KillTree = InKillTree;
	}

	/**
	 * Gets the duration of time that the task has been running.
	 *
	 * @return Time duration.
	 */
	FTimespan GetDuration() const;

	/**
	 * Checks whether the process is still running.
	 *
	 * @return true if the process is running, false otherwise.
	 */
	bool IsRunning() const
	{
		return bIsRunning;
	}

	/** Launches the process. */
	bool Launch();

	/**
	 * Sets the sleep interval to be used in the main thread loop.
	 *
	 * @param InSleepInterval The Sleep interval to use.
	 */
	void SetSleepInterval( float InSleepInterval )
	{
		SleepInterval = InSleepInterval;
	}

public:

	/**
	 * Returns a delegate that is executed when the process has been canceled.
	 *
	 * @return The delegate.
	 */
	FSimpleDelegate& OnCanceled()
	{
		return CanceledDelegate;
	}

	/**
	 * Returns a delegate that is executed when a monitored process completed.
	 *
	 * @return The delegate.
	 */
	FOnMonitoredProcessCompleted& OnCompleted()
	{
		return CompletedDelegate;
	}

	/**
	 * Returns a delegate that is executed when a monitored process produces output.
	 *
	 * @return The delegate.
	 */
	FOnMonitoredProcessOutput& OnOutput()
	{
		return OutputDelegate;
	}

	/**
	 * Returns the return code from the exited process
	 *
	 * @return Process return code
	 */
	int GetReturnCode() const
	{
		return ReturnCode;
	}

public:

	// FRunnable interface

	virtual bool Init() override
	{
		return true;
	}

	virtual uint32 Run() override;

	virtual void Stop() override
	{
		Cancel();
	}

	virtual void Exit() override { }

protected:

	/**
	 * Processes the given output string.
	 *
	 * @param Output The output string to process.
	 */
	void ProcessOutput( const FString& Output );

private:

	// Whether the process is being canceled. */
	bool Canceling;

	// Holds the time at which the process ended. */
	FDateTime EndTime;

	// Whether the window of the process should be hidden. */
	bool Hidden;

	// Whether to kill the entire process tree when cancelling this process. */
	bool KillTree;

	// Holds the command line parameters. */
	FString Params;

	// Holds the handle to the process. */
	FProcHandle ProcessHandle;

	// Holds the read pipe. */
	void* ReadPipe;

	// Holds the return code. */
	int ReturnCode;

	// Holds the time at which the process started. */
	FDateTime StartTime;

	// Holds the monitoring thread object. */
	FRunnableThread* Thread;

	// Is the thread running? 
	bool bIsRunning;

	// Holds the URL of the executable to launch. */
	FString URL;

	// Holds the write pipe. */
	void* WritePipe;

	// Holds if we should create pipes
	bool bCreatePipes;

	// Sleep interval to use
	float SleepInterval;

private:

	// Holds a delegate that is executed when the process has been canceled. */
	FSimpleDelegate CanceledDelegate;

	// Holds a delegate that is executed when a monitored process completed. */
	FOnMonitoredProcessCompleted CompletedDelegate;

	// Holds a delegate that is executed when a monitored process produces output. */
	FOnMonitoredProcessOutput OutputDelegate;
};
