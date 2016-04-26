// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraPrivate.h"
#include "NiagaraEvents.h"
#include "NiagaraSimulation.h"
#include "NiagaraEffect.h"

DECLARE_DWORD_COUNTER_STAT(TEXT("Num Death Events"), STAT_NiagaraNumDeathEvents, STATGROUP_Niagara);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Spawn Events"), STAT_NiagaraNumSpawnEvents, STATGROUP_Niagara);

void UNiagaraEventReceiverEmitterAction_SpawnParticles::PerformAction(FNiagaraSimulation& OwningSim, FNiagaraEventReceiverProperties& OwningEventReceiver)
{
	//Find the generator set we're bound to and spawn NumParticles particles for every event it generated last frame.
	FNiagaraDataSet* GeneratorSet = OwningSim.GetParentEffectInstance()->GetDataSet(FNiagaraDataSetID(OwningEventReceiver.SourceEventGenerator, ENiagaraDataSetType::Event), OwningEventReceiver.SourceEmitter);
	if (GeneratorSet)
	{
		OwningSim.SpawnBurst(GeneratorSet->GetPrevNumInstances()*NumParticles);
	}
}
//////////////////////////////////////////////////////////////////////////

void FNiagaraDeathEventGenerator::BeginTrackingDeaths()
{
	FNiagaraDataSet& Data = OwnerSim->GetData();
	TempStorage.Empty(Data.GetNumInstances());
	TempStorage.SetNum(Data.GetNumVariables());
	NumDead = 0;
}

void FNiagaraDeathEventGenerator::OnDeath(int32 DeadIndex)
{
	FNiagaraDataSet& Data = OwnerSim->GetData();
	int32 AttrIdx = 0;
	for (const TPair<FNiagaraVariableInfo, uint32>& Pair : Data.GetVariables())
	{
		FVector4* AttrPtr = Data.GetVariableData(Pair.Key);
		TempStorage[AttrIdx].Add(AttrPtr[DeadIndex]);
		++AttrIdx;
	}
	++NumDead;
}

void FNiagaraDeathEventGenerator::EndTrackingDeaths()
{
	FNiagaraDataSet& Data = OwnerSim->GetData();
	int32 AttrIdx = 0;
	DataSet.SetNumInstances(NumDead);
	DataSet.Allocate(NumDead);
	if (NumDead > 0)
	{
		INC_DWORD_STAT_BY(STAT_NiagaraNumDeathEvents, NumDead);
		for (const TPair<FNiagaraVariableInfo, uint32>& Pair : Data.GetVariables())
		{
			FVector4* AttrPtr = DataSet.GetVariableData(Pair.Key);
			FMemory::Memcpy(AttrPtr, TempStorage[AttrIdx].GetData(), sizeof(FVector4)* NumDead);
			++AttrIdx;
		}
	}
}

void FNiagaraSpawnEventGenerator::OnSpawned(int32 SpawnIndex, int32 NumSpawned)
{
	INC_DWORD_STAT_BY(STAT_NiagaraNumSpawnEvents, NumSpawned);

	FNiagaraDataSet& Data = OwnerSim->GetData();
	DataSet.Allocate(NumSpawned);
	DataSet.SetNumInstances(NumSpawned);

	for (const TPair<FNiagaraVariableInfo, uint32>& Pair : Data.GetVariables())
	{
		FVector4* DestPtr = DataSet.GetVariableData(Pair.Key, 0);
		FVector4* AttrPtr = Data.GetVariableData(Pair.Key, SpawnIndex);
		FMemory::Memcpy(DestPtr, AttrPtr, sizeof(FVector4)* NumSpawned);
	}
}
