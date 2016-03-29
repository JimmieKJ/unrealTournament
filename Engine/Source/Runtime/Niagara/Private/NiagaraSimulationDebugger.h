// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraScript.h"

//#define ENABLE_NIAGARA_SIMULATION_DEBUGGING (0)
#define ENABLE_NIAGARA_SIMULATION_DEBUGGING (1) && WITH_EDITOR && ENABLE_VM_DEBUGGING

namespace LexicalConversion
{
	FString ToString(VectorRegister Register);
};

#if ENABLE_NIAGARA_SIMULATION_DEBUGGING

class FNiagaraSimulationDebugger
{
	VectorVM::FVectorVMDebugger VMDebugger;
	FNiagaraSimulation* DebugSim;
	FString DebugSimName;

	TArray<int32> CurrDebugInstances;

	int32 NumParticlesToDebugNextSpawn;
	int32 NumSpawnsToSkip;

	FString GetValueString(FVector4 Value);
	FString GetOpArgString(const VectorVM::FOpDebugInfo& DebugInfo, int32 ArgIndex);
public:

	static FNiagaraSimulationDebugger& Get();

	FNiagaraSimulationDebugger();

	FORCEINLINE bool IsDebugging(FNiagaraSimulation* InSim){ return DebugSim == InSim && CurrDebugInstances.Num() > 0; }

	void DebugNextNParticles(int32 NumParticlesToDebug, int32 SpawnsToSkip);
	void InitForDebugging(FNiagaraSimulation* Sim, FString InDebugSimName);
	void EndDebugging();

	// Hooks for simulations
	void PreSpawn(FNiagaraSimulation* Sim, int32 CurrNumParticles, int32 NumToSpawn);
	void OnDeath(FNiagaraSimulation* Sim, int32 DyingParticle, int32 ReplacementParticle);
	void PreScriptRun(FNiagaraSimulation* Sim, UNiagaraScript* Script, int32 TotalNumParticles, int32 NumParticlesProcessed, int32 StartingParticle);
	void PostScriptRun(FNiagaraSimulation* Sim, UNiagaraScript* Script);
};

#endif
