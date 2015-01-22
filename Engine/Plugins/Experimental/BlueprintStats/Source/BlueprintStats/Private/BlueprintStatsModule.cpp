// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintStatsPrivatePCH.h"
#include "IBlueprintStatsModule.h"
#include "BlueprintStats.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBlueprintStats, Verbose, All);

DEFINE_LOG_CATEGORY(LogBlueprintStats);

class FBlueprintStatsModule : public IBlueprintStatsModule
{
public:
	FBlueprintStatsModule()
		: DumpBlueprintStatsCmd(NULL)
	{
	}

	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End of IModuleInterface interface

private:
	IConsoleObject* DumpBlueprintStatsCmd;

private:
	static void DumpBlueprintStats();
};

IMPLEMENT_MODULE(FBlueprintStatsModule, BlueprintStats)


void FBlueprintStatsModule::StartupModule()
{
	if (!IsRunningCommandlet())
	{
		DumpBlueprintStatsCmd = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("DumpBlueprintStats"),
			TEXT("Dumps statistics about blueprint node usage to the log."),
			FConsoleCommandDelegate::CreateStatic(DumpBlueprintStats),
			ECVF_Default
			);
	}
}

void FBlueprintStatsModule::ShutdownModule()
{
	if (DumpBlueprintStatsCmd != NULL)
	{
		IConsoleManager::Get().UnregisterConsoleObject(DumpBlueprintStatsCmd);
	}
}

void FBlueprintStatsModule::DumpBlueprintStats()
{
	TArray<FBlueprintStatRecord> Records;
	for (TObjectIterator<UBlueprint> BlueprintIt; BlueprintIt; ++BlueprintIt)
	{
		UBlueprint* Blueprint = *BlueprintIt;

		new (Records) FBlueprintStatRecord(Blueprint);
	}


	// Now merge them
	FBlueprintStatRecord Aggregate(NULL);
	for (const FBlueprintStatRecord& SourceRecord : Records)
	{
		Aggregate.MergeAnotherRecordIn(SourceRecord);
	}

	// Sort the lists
	Aggregate.NodeCount.ValueSort(TGreater<int32>());
	Aggregate.FunctionCount.ValueSort(TGreater<int32>());
	Aggregate.FunctionOwnerCount.ValueSort(TGreater<int32>());
	Aggregate.RemoteMacroCount.ValueSort(TGreater<int32>());

	// Print out the merged record
	UE_LOG(LogBlueprintStats, Log, TEXT("Blueprint stats for %d blueprints in %s"), Records.Num(), FApp::GetGameName());
	UE_LOG(LogBlueprintStats, Log, TEXT("%s"), *Aggregate.ToString(true));
	UE_LOG(LogBlueprintStats, Log, TEXT("%s"), *Aggregate.ToString(false));
	UE_LOG(LogBlueprintStats, Log, TEXT("\n"));

	// Print out the node list
	UE_LOG(LogBlueprintStats, Log, TEXT("NodeClass,NumInstances"));
	for (const auto& NodePair : Aggregate.NodeCount)
	{
		UE_LOG(LogBlueprintStats, Log, TEXT("%s,%d"), *(NodePair.Key->GetName()), NodePair.Value);
	}
	UE_LOG(LogBlueprintStats, Log, TEXT("\n"));

	// Print out the function list
	UE_LOG(LogBlueprintStats, Log, TEXT("FunctionPath,ClassName,FunctionName,NumInstances"));
	for (const auto& FunctionPair : Aggregate.FunctionCount)
	{
		UFunction* Function = FunctionPair.Key;
		UE_LOG(LogBlueprintStats, Log, TEXT("%s,%s,%s,%d"), *(Function->GetPathName()), *(Function->GetOuterUClass()->GetName()), *(Function->GetName()), FunctionPair.Value);
	}
	UE_LOG(LogBlueprintStats, Log, TEXT("\n"));

	// Print out the macro list
	UE_LOG(LogBlueprintStats, Log, TEXT("MacroPath,MacroName,NumInstances"));
	for (const auto& MacroPair : Aggregate.RemoteMacroCount)
	{
		UEdGraph* MacroGraph = MacroPair.Key;
		UE_LOG(LogBlueprintStats, Log, TEXT("%s,%s,%d"), *(MacroGraph->GetPathName()), *(MacroGraph->GetName()), MacroPair.Value);
	}
	UE_LOG(LogBlueprintStats, Log, TEXT("\n"));
}