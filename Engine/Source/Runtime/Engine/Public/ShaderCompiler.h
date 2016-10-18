// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCompiler.h: Platform independent shader compilation definitions.
=============================================================================*/

#pragma once

#include "ShaderCore.h"

DECLARE_LOG_CATEGORY_EXTERN(LogShaderCompilers, Log, All);

class FShaderCompileJob;
class FShaderPipelineCompileJob;

/** Stores all of the common information used to compile a shader or pipeline. */
class FShaderCommonCompileJob : public FRefCountedObject
{
public:
	/** Id of the shader map this shader belongs to. */
	uint32 Id;
	/** true if the results of the shader compile have been processed. */
	bool bFinalized;
	/** Output of the shader compile */
	bool bSucceeded;
	bool bOptimizeForLowLatency;

	FShaderCommonCompileJob(uint32 InId) :
		Id(InId),
		bFinalized(false),
		bSucceeded(false),
		bOptimizeForLowLatency(false)
	{
	}

	virtual FShaderCompileJob* GetSingleShaderJob() { return nullptr; }
	virtual const FShaderCompileJob* GetSingleShaderJob() const { return nullptr; }
	virtual FShaderPipelineCompileJob* GetShaderPipelineJob() { return nullptr; }
	virtual const FShaderPipelineCompileJob* GetShaderPipelineJob() const { return nullptr; }
};


/** Stores all of the input and output information used to compile a single shader. */
class FShaderCompileJob : public FShaderCommonCompileJob
{
public:
	/** Vertex factory type that this shader belongs to, may be NULL */
	FVertexFactoryType* VFType;
	/** Shader type that this shader belongs to, must be valid */
	FShaderType* ShaderType;
	/** Input for the shader compile */
	FShaderCompilerInput Input;
	FShaderCompilerOutput Output;

	// List of pipelines that are sharing this job.
	TMap<const FVertexFactoryType*, TArray<const FShaderPipelineType*>> SharingPipelines;

	FShaderCompileJob(uint32 InId, FVertexFactoryType* InVFType, FShaderType* InShaderType) :
		FShaderCommonCompileJob(InId),
		VFType(InVFType),
		ShaderType(InShaderType)
	{
	}

	virtual FShaderCompileJob* GetSingleShaderJob() override { return this; }
	virtual const FShaderCompileJob* GetSingleShaderJob() const override { return this; }
};

class FShaderPipelineCompileJob : public FShaderCommonCompileJob
{
public:
	TArray<FShaderCommonCompileJob*> StageJobs;
	bool bFailedRemovingUnused;

	/** Shader pipeline that this shader belongs to, may (currently) be NULL */
	const FShaderPipelineType* ShaderPipeline;

	FShaderPipelineCompileJob(uint32 InId, const FShaderPipelineType* InShaderPipeline, int32 NumStages) :
		FShaderCommonCompileJob(InId),
		bFailedRemovingUnused(false),
		ShaderPipeline(InShaderPipeline)
	{
		check(InShaderPipeline && InShaderPipeline->GetName());
		check(NumStages > 0);
		StageJobs.Empty(NumStages);
	}

	~FShaderPipelineCompileJob()
	{
		for (int32 Index = 0; Index < StageJobs.Num(); ++Index)
		{
			delete StageJobs[Index];
		}
		StageJobs.Reset();
	}

	virtual FShaderPipelineCompileJob* GetShaderPipelineJob() override { return this; }
	virtual const FShaderPipelineCompileJob* GetShaderPipelineJob() const override { return this; }
};

class FShaderCompileThreadRunnableBase : public FRunnable
{
	friend class FShaderCompilingManager;

protected:
	/** The manager for this thread */
	class FShaderCompilingManager* Manager;
	/** The runnable thread */
	FRunnableThread* Thread;
	
	/** If the thread has been terminated by an unhandled exception, this contains the error message. */
	FString ErrorMessage;
	/** true if the thread has been terminated by an unhandled exception. */
	bool bTerminatedByError;

	volatile bool bForceFinish;

public:
	FShaderCompileThreadRunnableBase(class FShaderCompilingManager* InManager);
	virtual ~FShaderCompileThreadRunnableBase()
	{}

	void StartThread();

	// FRunnable interface.
	virtual void Stop() { bForceFinish = true; }
	virtual uint32 Run();
	inline void WaitForCompletion() const
	{
		if( Thread )
		{
			Thread->WaitForCompletion();
		}
	}

	/** Checks the thread's health, and passes on any errors that have occured.  Called by the main thread. */
	void CheckHealth() const;

	/** Main work loop. */
	virtual int32 CompilingLoop() = 0;
};

/** 
 * Shader compiling thread
 * This runs in the background while UE4 is running, launches shader compile worker processes when necessary, and feeds them inputs and reads back the outputs.
 */
class FShaderCompileThreadRunnable : public FShaderCompileThreadRunnableBase
{
	friend class FShaderCompilingManager;
private:

	/** Information about the active workers that this thread is tracking. */
	TArray<struct FShaderCompileWorkerInfo*> WorkerInfos;
	/** Tracks the last time that this thread checked if the workers were still active. */
	double LastCheckForWorkersTime;

public:
	/** Initialization constructor. */
	FShaderCompileThreadRunnable(class FShaderCompilingManager* InManager);
	virtual ~FShaderCompileThreadRunnable();

private:

	/** 
	 * Grabs tasks from Manager->CompileQueue in a thread safe way and puts them into QueuedJobs of available workers. 
	 * Also writes completed jobs to Manager->ShaderMapJobs.
	 */
	int32 PullTasksFromQueue();

	/** Used when compiling through workers, writes out the worker inputs for any new tasks in WorkerInfos.QueuedJobs. */
	void WriteNewTasks();

	/** Used when compiling through workers, launches worker processes if needed. */
	bool LaunchWorkersIfNeeded();

	/** Used when compiling through workers, attempts to open the worker output file if the worker is done and read the results. */
	void ReadAvailableResults();

	/** Used when compiling directly through the console tools dll. */
	void CompileDirectlyThroughDll();

	/** Main work loop. */
	virtual int32 CompilingLoop() override;
};

class FShaderCompileXGEThreadRunnable : public FShaderCompileThreadRunnableBase
{
private:
	/** The handle referring to the XGE console process, if a build is in progress. */
	FProcHandle BuildProcessHandle;
	
	/** Process ID of the XGE console, if a build is in progress. */
	uint32 BuildProcessID;

	/**
	 * A map of directory paths to shader jobs contained within that directory.
	 * One entry per XGE task.
	 */
	class FShaderBatch
	{
		TArray<FShaderCommonCompileJob*> Jobs;
		bool bTransferFileWritten;

	public:
		const FString& DirectoryBase;
		const FString& InputFileName;
		const FString& SuccessFileName;
		const FString& OutputFileName;

		int32 BatchIndex;
		int32 DirectoryIndex;

		FString WorkingDirectory;
		FString OutputFileNameAndPath;
		FString SuccessFileNameAndPath;
		FString InputFileNameAndPath;
		
		FShaderBatch(const FString& InDirectoryBase, const FString& InInputFileName, const FString& InSuccessFileName, const FString& InOutputFileName, int32 InDirectoryIndex, int32 InBatchIndex)
			: bTransferFileWritten(false)
			, DirectoryBase(InDirectoryBase)
			, InputFileName(InInputFileName)
			, SuccessFileName(InSuccessFileName)
			, OutputFileName(InOutputFileName)
		{
			SetIndices(InDirectoryIndex, InBatchIndex);
		}

		void SetIndices(int32 InDirectoryIndex, int32 InBatchIndex);

		void CleanUpFiles(bool keepInputFile);

		inline int32 NumJobs()
		{
			return Jobs.Num();
		}
		inline const TArray<FShaderCommonCompileJob*>& GetJobs() const
		{
			return Jobs;
		}

		void AddJob(FShaderCommonCompileJob* Job);
		
		void WriteTransferFile();
	};
	TArray<FShaderBatch*> ShaderBatchesInFlight;
	TArray<FShaderBatch*> ShaderBatchesFull;
	TSparseArray<FShaderBatch*> ShaderBatchesIncomplete;

	/** The full path to the two working directories for XGE shader builds. */
	const FString XGEWorkingDirectory;
	uint32 XGEDirectoryIndex;

	uint64 LastAddTime;
	uint64 StartTime;
	int32 BatchIndexToCreate;
	int32 BatchIndexToFill;

	FDateTime ScriptFileCreationTime;

	void PostCompletedJobsForBatch(FShaderBatch* Batch);

	void GatherResultsFromXGE();

public:
	/** Initialization constructor. */
	FShaderCompileXGEThreadRunnable(class FShaderCompilingManager* InManager);
	virtual ~FShaderCompileXGEThreadRunnable();

	/** Main work loop. */
	virtual int32 CompilingLoop() override;

	static bool IsSupported();
};

/** Results for a single compiled shader map. */
struct FShaderMapCompileResults
{
	FShaderMapCompileResults() :
		NumJobsQueued(0),
		bAllJobsSucceeded(true),
		bApplyCompletedShaderMapForRendering(true),
		bRecreateComponentRenderStateOnCompletion(false)
	{}

	int32 NumJobsQueued;
	bool bAllJobsSucceeded;
	bool bApplyCompletedShaderMapForRendering;
	bool bRecreateComponentRenderStateOnCompletion;
	TArray<FShaderCommonCompileJob*> FinishedJobs;
};

/** Results for a single compiled and finalized shader map. */
struct FShaderMapFinalizeResults : public FShaderMapCompileResults
{
	/** Tracks finalization progress on this shader map. */
	int32 FinalizeJobIndex;

	// List of pipelines with shared shaders; nullptr for non mesh pipelines
	TMap<const FVertexFactoryType*, TArray<const FShaderPipelineType*> > SharedPipelines;

	FShaderMapFinalizeResults(const FShaderMapCompileResults& InCompileResults) :
		FShaderMapCompileResults(InCompileResults),
		FinalizeJobIndex(0)
	{}
};

/**  
 * Manager of asynchronous and parallel shader compilation.
 * This class contains an interface to enqueue and retreive asynchronous shader jobs, and manages a FShaderCompileThreadRunnable.
 */
class FShaderCompilingManager
{
	friend class FShaderCompileThreadRunnableBase;
	friend class FShaderCompileThreadRunnable;
	friend class FShaderCompileXGEThreadRunnable;
private:

	//////////////////////////////////////////////////////
	// Thread shared properties: These variables can only be read from or written to when a lock on CompileQueueSection is obtained, since they are used by both threads.

	/** Tracks whether we are compiling while the game is running.  If true, we need to throttle down shader compiling CPU usage to avoid starving the runtime threads. */
	bool bCompilingDuringGame;
	/** Queue of tasks that haven't been assigned to a worker yet. */
	TArray<FShaderCommonCompileJob*> CompileQueue;
	/** Map from shader map Id to the compile results for that map, used to gather compiled results. */
	TMap<int32, FShaderMapCompileResults> ShaderMapJobs;
	/** Number of jobs currently being compiled.  This includes CompileQueue and any jobs that have been assigned to workers but aren't complete yet. */
	int32 NumOutstandingJobs;

	/** Critical section used to gain access to the variables above that are shared by both the main thread and the FShaderCompileThreadRunnable. */
	FCriticalSection CompileQueueSection;

	//////////////////////////////////////////////////////
	// Main thread state - These are only accessed on the main thread and used to track progress

	/** Map from shader map id to results being finalized.  Used to track shader finalizations over multiple frames. */
	TMap<int32, FShaderMapFinalizeResults> PendingFinalizeShaderMaps;

	/** The threads spawned for shader compiling. */
	TScopedPointer<FShaderCompileThreadRunnableBase> Thread;

	//////////////////////////////////////////////////////
	// Configuration properties - these are set only on initialization and can be read from either thread

	/** Number of busy threads to use for shader compiling while loading. */
	uint32 NumShaderCompilingThreads;
	/** Number of busy threads to use for shader compiling while in game. */
	uint32 NumShaderCompilingThreadsDuringGame;
	/** Largest number of jobs that can be put in the same batch. */
	int32 MaxShaderJobBatchSize;
	/** Process Id of UE4. */
	uint32 ProcessId;
	/** Whether to allow compiling shaders through the worker application, which allows multiple cores to be used. */
	bool bAllowCompilingThroughWorkers;
	/** Whether to allow shaders to compile in the background or to block after each material. */
	bool bAllowAsynchronousShaderCompiling;
	/** Whether to ask to retry a failed shader compile error. */
	bool bPromptToRetryFailedShaderCompiles;
	/** Whether to log out shader job completion times on the worker thread.  Useful for tracking down which global shader is taking a long time. */
	bool bLogJobCompletionTimes;
	/** Target execution time for ProcessAsyncResults.  Larger values speed up async shader map processing but cause more hitchiness while async compiling is happening. */
	float ProcessGameThreadTargetTime;
	/** Base directory where temporary files are written out during multi core shader compiling. */
	FString ShaderBaseWorkingDirectory;
	/** Absolute version of ShaderBaseWorkingDirectory. */
	FString AbsoluteShaderBaseWorkingDirectory;
	/** Absolute path to the directory to dump shader debug info to. */
	FString AbsoluteShaderDebugInfoDirectory;
	/** Name of the shader worker application. */
	FString ShaderCompileWorkerName;
	/** Whether the SCW has crashed and we should fall back to calling the compiler dll's directly. */
	bool bFallBackToDirectCompiles;
	/** Whether a recreate should be done when compiling is finished. */
	bool bRecreateComponentRenderStateOutstanding;

	/** 
	 * Tracks the total time that shader compile workers have been busy since startup.  
	 * Useful for profiling the shader compile worker thread time.
	 */
	double WorkersBusyTime;

	/** 
	 * Tracks which opt-in shader platforms have their warnings suppressed.
	 */
	uint64 SuppressedShaderPlatforms;

	/** Launches the worker, returns the launched process handle. */
	FProcHandle LaunchWorker(const FString& WorkingDirectory, uint32 ProcessId, uint32 ThreadId, const FString& WorkerInputFile, const FString& WorkerOutputFile);

	/** Blocks on completion of the given shader maps. */
	void BlockOnShaderMapCompletion(const TArray<int32>& ShaderMapIdsToFinishCompiling, TMap<int32, FShaderMapFinalizeResults>& CompiledShaderMaps);

	/** Blocks on completion of all shader maps. */
	void BlockOnAllShaderMapCompletion(TMap<int32, FShaderMapFinalizeResults>& CompiledShaderMaps);

	/** Finalizes the given shader map results and optionally assigns the affected shader maps to materials, while attempting to stay within an execution time budget. */
	void ProcessCompiledShaderMaps(TMap<int32, FShaderMapFinalizeResults>& CompiledShaderMaps, float TimeBudget, bool bRecreateComponentRenderState);

	/** Recompiles shader jobs with errors if requested, and returns true if a retry was needed. */
	bool HandlePotentialRetryOnError(TMap<int32, FShaderMapFinalizeResults>& CompletedShaderMaps);

public:
	
	ENGINE_API FShaderCompilingManager();

	/** 
	 * Returns whether to display a notification that shader compiling is happening in the background. 
	 * Note: This is dependent on NumOutstandingJobs which is updated from another thread, so the results are non-deterministic.
	 */
	bool ShouldDisplayCompilingNotification() const 
	{ 
		// Heuristic based on the number of jobs outstanding
		return NumOutstandingJobs > 80; 
	}

	bool AllowAsynchronousShaderCompiling() const 
	{
		return bAllowAsynchronousShaderCompiling;
	}

	/** 
	 * Returns whether async compiling is happening.
	 * Note: This is dependent on NumOutstandingJobs which is updated from another thread, so the results are non-deterministic.
	 */
	bool IsCompiling() const
	{
		return NumOutstandingJobs > 0 || PendingFinalizeShaderMaps.Num() > 0;
	}

	/** 
	 * Returns the number of outstanding compile jobs.
	 * Note: This is dependent on NumOutstandingJobs which is updated from another thread, so the results are non-deterministic.
	 */
	int32 GetNumRemainingJobs() const
	{
		return NumOutstandingJobs;
	}

	const FString& GetAbsoluteShaderDebugInfoDirectory() const
	{
		return AbsoluteShaderDebugInfoDirectory;
	}

	bool AreWarningsSuppressed(const EShaderPlatform Platform) const
	{
		return (SuppressedShaderPlatforms & (static_cast<uint64>(1) << Platform)) != 0;
	}

	void SuppressWarnings(const EShaderPlatform Platform)
	{
		SuppressedShaderPlatforms |= static_cast<uint64>(1) << Platform;
	}

	/** 
	 * Adds shader jobs to be asynchronously compiled. 
	 * FinishCompilation or ProcessAsyncResults must be used to get the results.
	 */
	ENGINE_API void AddJobs(TArray<FShaderCommonCompileJob*>& NewJobs, bool bApplyCompletedShaderMapForRendering, bool bOptimizeForLowLatency, bool bRecreateComponentRenderStateOnCompletion);

	/**
	* Removes all outstanding compile jobs for the passed shader maps.
	*/
	ENGINE_API void CancelCompilation(const TCHAR* MaterialName, const TArray<int32>& ShaderMapIdsToCancel);

	/** 
	 * Blocks until completion of the requested shader maps.  
	 * This will not assign the shader map to any materials, the caller is responsible for that.
	 */
	ENGINE_API void FinishCompilation(const TCHAR* MaterialName, const TArray<int32>& ShaderMapIdsToFinishCompiling);

	/** 
	 * Blocks until completion of all async shader compiling, and assigns shader maps to relevant materials.
	 * This should be called before exit if the DDC needs to be made up to date. 
	 */
	ENGINE_API void FinishAllCompilation();


	/** 
	 * Shutdown the shader compiler manager, this will shutdown immediately and not process any more shader compile requests. 
	 */
	ENGINE_API void Shutdown();


	/** 
	 * Processes completed asynchronous shader maps, and assigns them to relevant materials.
	 * @param bLimitExecutionTime - When enabled, ProcessAsyncResults will be bandwidth throttled by ProcessGameThreadTargetTime, to limit hitching.
	 *		ProcessAsyncResults will then have to be called often to finish all shader maps (eg from Tick).  Otherwise, all compiled shader maps will be processed.
	 * @param bBlockOnGlobalShaderCompletion - When enabled, ProcessAsyncResults will block until global shader maps are complete.
	 *		This must be done before using global shaders for rendering.
	 */
	ENGINE_API void ProcessAsyncResults(bool bLimitExecutionTime, bool bBlockOnGlobalShaderCompletion);

	/**
	 * Returns true if the given shader compile worker is still running.
	 */
	static bool IsShaderCompilerWorkerRunning(FProcHandle & WorkerHandle);
};

/** The global shader compiling thread manager. */
extern ENGINE_API FShaderCompilingManager* GShaderCompilingManager;

/** The shader precompilers for each platform.  These are only set during the console shader compilation while cooking or in the PrecompileShaders commandlet. */
extern class FConsoleShaderPrecompiler* GConsoleShaderPrecompilers[SP_NumPlatforms];

/** Enqueues a shader compile job with GShaderCompilingManager. */
extern void GlobalBeginCompileShader(
	const FString& DebugGroupName,
	class FVertexFactoryType* VFType,
	class FShaderType* ShaderType,
	const class FShaderPipelineType* ShaderPipelineType,
	const TCHAR* SourceFilename,
	const TCHAR* FunctionName,
	FShaderTarget Target,
	FShaderCompileJob* NewJob,
	TArray<FShaderCommonCompileJob*>& NewJobs,
	bool bAllowDevelopmentShaderCompile = true
	);

/** Implementation of the 'recompileshaders' console command.  Recompiles shaders at runtime based on various criteria. */
extern bool RecompileShaders(const TCHAR* Cmd, FOutputDevice& Ar);

/** Returns whether all global shader types containing the substring are complete and ready for rendering. if type name is null, check everything */
extern ENGINE_API bool IsGlobalShaderMapComplete(const TCHAR* TypeNameSubstring = nullptr);
