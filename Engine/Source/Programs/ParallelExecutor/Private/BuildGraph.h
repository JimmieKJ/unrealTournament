// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FBuildAction
{
public:
	int32 SortIndex;

	FString Caption;
	FString GroupPrefix;
	FString OutputPrefix;
	FString ToolPath;
	FString ToolArguments;
	FString WorkingDirectory;
	bool bSkipIfProjectFailed;

	TArray<FBuildAction*> Dependants;
	TArray<FBuildAction*> Dependencies;

	int32 TotalDependants;
	int32 MissingDependencyCount;

	TMap<FString, FString> Variables;

	FBuildAction();
	~FBuildAction();
};

class FBuildActionExecutor : public FRunnable
{
public:
	FBuildAction* Action;
	TArray<FString> Log;
	int32 ExitCode;

	FBuildActionExecutor(FBuildAction* InAction, FCriticalSection& InCriticalSection, FEvent* InCompletedEvent, TArray<FBuildActionExecutor*>& InCompletedActions, const TMap<FString, FString>& InEnvironment, void* InJobObject);
	virtual uint32 Run() override;

private:
	FCriticalSection& CriticalSection;
	FEvent* CompletedEvent;
	TArray<FBuildActionExecutor*>& CompletedActions;
	const TMap<FString, FString>& Environment;
	void* JobObject;

	int32 ExecuteAction();
	static FString ExpandEnvironmentVariables(const FString& Input);
	static FProcHandle CreateChildProcess(const FString& FileName, const FString& Arguments, const FString& WorkingDir, const TMap<FString, FString>& Environment, void* WritePipe, void* JobObject);
	static void WaitForOutput(FProcHandle ProcessHandle, void* ReadPipe);
};

class FBuildGraph
{
public:
	FBuildGraph();
	~FBuildGraph();
	bool ReadFromFile(const FString& InputPath);
	int32 ExecuteInParallel(int32 MaxProcesses);

private:
	TArray<FBuildAction*> Actions;

	FBuildAction* FindOrAddAction(TMap<FString, FBuildAction*>& NameToAction, const FString& Name);
	static void RecursiveIncDependents(FBuildAction* Action, TSet<FBuildAction*>& VisitedActions);

	static void* CreateJobObject();
};
