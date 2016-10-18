// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


struct FCommandDesc
{
	FString Name;
	FString Desc;
	FString EndText;
};

/**
 * Implements the launcher's worker thread.
 */
class FLauncherWorker
	: public FRunnable
	, public ILauncherWorker
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InProfile - The profile to process.
	 * @param InDeviceProxyManager - The target device proxy manager to use.
	 */
	FLauncherWorker( const ITargetDeviceProxyManagerRef& InDeviceProxyManager, const ILauncherProfileRef& InProfile );

	~FLauncherWorker()
	{
		TaskChain.Reset();
	}

public:

	virtual bool Init( ) override;

	virtual uint32 Run( ) override;

	virtual void Stop( ) override;

	virtual void Exit( ) override { }

public:

	virtual void Cancel( ) override;

	virtual void CancelAndWait( ) override;

	virtual ELauncherWorkerStatus::Type GetStatus( ) const  override
	{
		return Status;
	}

	virtual int32 GetTasks( TArray<ILauncherTaskPtr>& OutTasks ) const override;

	virtual FOutputMessageReceivedDelegate& OnOutputReceived() override
	{
		return OutputMessageReceived;
	}

	virtual FOnStageStartedDelegate& OnStageStarted() override
	{
		return StageStarted;
	}

	virtual FOnStageCompletedDelegate& OnStageCompleted() override
	{
		return StageCompleted;
	}

	virtual FOnLaunchCompletedDelegate& OnCompleted() override
	{
		return LaunchCompleted;
	}

	virtual FOnLaunchCanceledDelegate& OnCanceled() override
	{
		return LaunchCanceled;
	}

protected:

	/**
	 * Creates the tasks for the specified profile.
	 *
	 * @param InProfile - The profile to create tasks for.
	 */
	void CreateAndExecuteTasks( const ILauncherProfileRef& InProfile );

	void OnTaskStarted(const FString& TaskName);
	void OnTaskCompleted(const FString& TaskName);

	FString CreateUATCommand( const ILauncherProfileRef& InProfile, const TArray<FString>& InPlatforms, TArray<FCommandDesc>& OutCommands, FString& CommandStart );

private:

	// Holds a critical section to lock access to selected properties.
	FCriticalSection CriticalSection;

	// Holds a pointer  to the device proxy manager.
	ITargetDeviceProxyManagerPtr DeviceProxyManager;

	// Holds a pointer to the launcher profile.
	ILauncherProfilePtr Profile;

	// Holds the worker's current status.
	ELauncherWorkerStatus::Type Status;

	// Holds the first task in the task chain.
	TSharedPtr<FLauncherTask> TaskChain;

	// holds the read and write pipes and the running UAT process
	void* ReadPipe;
	void* WritePipe;
	FProcHandle ProcHandle;

	double StageStartTime;
	double LaunchStartTime;

	// message delegate
	FOutputMessageReceivedDelegate OutputMessageReceived;
	FOnStageStartedDelegate StageStarted;
	FOnStageCompletedDelegate StageCompleted;
	FOnLaunchCompletedDelegate LaunchCompleted;
	FOnLaunchCanceledDelegate LaunchCanceled;
};
