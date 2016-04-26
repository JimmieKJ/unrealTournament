// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraPrivate.h"
#include "NiagaraSimulation.h"
#include "NiagaraEffectRenderer.h"
#include "VectorVM.h"

#if ENABLE_NIAGARA_SIMULATION_DEBUGGING

FNiagaraSimulationDebugger& FNiagaraSimulationDebugger::Get()
{
	static FNiagaraSimulationDebugger Instance;
	return Instance;
}

FNiagaraSimulationDebugger::FNiagaraSimulationDebugger()
: DebugSim(NULL)
, NumParticlesToDebugNextSpawn(0)
, NumSpawnsToSkip(0)
{
}

void FNiagaraSimulationDebugger::DebugNextNParticles(int32 NumParticlesToDebug, int32 SpawnsToSkip)
{
	NumParticlesToDebugNextSpawn = NumParticlesToDebug;
	NumSpawnsToSkip = SpawnsToSkip;
}

void FNiagaraSimulationDebugger::InitForDebugging(FNiagaraSimulation* Sim, FString InDebugSimName)
{
	EndDebugging();

	DebugSim = Sim;
	DebugSimName = InDebugSimName;
	UE_LOG(LogNiagara, Log, TEXT("======================================"));
	UE_LOG(LogNiagara, Log, TEXT("Begun Debugging %s"), *DebugSimName);
	UE_LOG(LogNiagara, Log, TEXT("======================================"));
}

void FNiagaraSimulationDebugger::EndDebugging()
{
	if (DebugSim)
	{
		UE_LOG(LogNiagara, Log, TEXT("======================================"));
		UE_LOG(LogNiagara, Log, TEXT("Stopped Debugging %s"), *DebugSimName);
		UE_LOG(LogNiagara, Log, TEXT("======================================"));
		DebugSim = NULL;
		DebugSimName.Empty();
		CurrDebugInstances.Empty();
	}
}

void FNiagaraSimulationDebugger::PreSpawn(FNiagaraSimulation* Sim, int32 CurrNumParticles, int32 NumToSpawn)
{
	if (DebugSim == Sim)
	{
		int32 CurrParticleToDebug = CurrNumParticles;
		while (--NumToSpawn >= 0 && NumSpawnsToSkip-- == 0 && NumParticlesToDebugNextSpawn-- > 0)
		{
			UE_LOG(LogNiagara, Log, TEXT("Begun Debugging Particle #%d"), CurrParticleToDebug);
			CurrDebugInstances.Add(CurrParticleToDebug++);
		}
	}
}

void FNiagaraSimulationDebugger::OnDeath(FNiagaraSimulation* Sim, int32 DyingParticle, int32 ReplacementParticle)
{
	if (IsDebugging(Sim))
	{
		if (CurrDebugInstances.Remove(DyingParticle) > 0)
		{
			UE_LOG(LogNiagara, Log, TEXT("PARTICLE #%d DIED!"), DyingParticle);
		}
		if (CurrDebugInstances.Remove(ReplacementParticle) > 0)
		{
			//If we're debugging the particle being moved into the dead particles position then we need to remove that and begin tracking it's new index.
			UE_LOG(LogNiagara, Log, TEXT("PARTICLE MOVE! Stopped debugging %d. Now debugging #%d"), ReplacementParticle, DyingParticle);
			CurrDebugInstances.Add(DyingParticle);
		}
	}
}

void FNiagaraSimulationDebugger::PreScriptRun(FNiagaraSimulation* Sim, UNiagaraScript* Script, int32 TotalNumParticles, int32 NumParticlesProcessed, int32 StartingParticle)
{
	if (IsDebugging(Sim))
	{
		UE_LOG(LogNiagara, Log, TEXT("=== %s ==================================="), *Script->GetName());
		UE_LOG(LogNiagara, Log, TEXT("Particles: %d - ToProcess: %d - Start: %d"), TotalNumParticles, NumParticlesProcessed, StartingParticle);

		VMDebugger.InitForScriptRun(StartingParticle, CurrDebugInstances);
		VectorVM::AttachDebugger(&VMDebugger);
	}
}

FString FNiagaraSimulationDebugger::GetValueString(FVector4 Value)
{
	return FString::Printf(TEXT("{%.5f, %.5f, %.5f, %.5f}"), Value.X, Value.Y, Value.Z, Value.W);
}

FString FNiagaraSimulationDebugger::GetOpArgString(const VectorVM::FOpDebugInfo& DebugInfo, int32 ArgIndex)
{
	const VectorVM::FDataLocationInfo& LocInfo = DebugInfo.LocationInfo[ArgIndex];
	FString PreOpStr, PostOpStr;
	// 		if (Type == VectorVM::EVMType::float)
	// 		{
	// 		}
	// 		else if (Type == VectorVM::EVMType::int)
	// 		{
	// 		}
	// 		else 
	if (DebugInfo.OpType == VectorVM::EVMType::Vector4)
	{
		PreOpStr = GetValueString(DebugInfo.PreOpValues[ArgIndex].v);
		PostOpStr = GetValueString(DebugInfo.PostOpValues[ArgIndex].v);
	}
	FString LocNameStr = VectorVM::GetOperandLocationName(LocInfo.Location);
	FString LocStr = FString::Printf(TEXT("%s%d"), *LocNameStr, LocInfo.PrimaryLocationIndex);
	if (LocInfo.SecondaryLocationIndex != INDEX_NONE)
		LocStr.Append(FString::Printf(TEXT("-%d"), LocInfo.SecondaryLocationIndex));
	if (LocInfo.TertiaryLocationIndex != INDEX_NONE)
		LocStr.Append(FString::Printf(TEXT("-%d"), LocInfo.TertiaryLocationIndex));

	return FString::Printf(TEXT("[%s-%p] %s --> %s"), *LocStr, LocInfo.MemoryAddress, *PreOpStr, *PostOpStr);
}

void FNiagaraSimulationDebugger::PostScriptRun(FNiagaraSimulation* Sim, UNiagaraScript* Script)
{
	if (IsDebugging(Sim))
	{
		VectorVM::DetachDebugger(&VMDebugger);
		for (int32 Particle : CurrDebugInstances)
		{
			FString PrefixStr = FString::Printf(TEXT("[%s|%d]\t"), *(Script->GetName()), Particle);
			if (const TArray<VectorVM::FOpDebugInfo>* InfoArray = VMDebugger.GetDebugInfoForInstance(Particle))
			{
				for (const VectorVM::FOpDebugInfo& DebugInfo : *InfoArray)
				{
					FString OpHeader = *VectorVM::GetOpName(DebugInfo.Op);
					if (DebugInfo.Op == EVectorVMOp::output)
					{
						const VectorVM::FDataLocationInfo& DestLocInfo = DebugInfo.LocationInfo[(int32)VectorVM::EKernelArgs::Dest];
						if (DestLocInfo.PrimaryLocationIndex < Script->Attributes.Num() && DestLocInfo.PrimaryLocationIndex >= 0)
						{
							OpHeader.Append(FString::Printf(TEXT(" [Attrib: %s]"), *Script->Attributes[DestLocInfo.PrimaryLocationIndex].Name.ToString()));
						}
					}
					UE_LOG(LogNiagara, Log, TEXT("%s[%s] ----------------------------------"), *PrefixStr, *OpHeader);
					UE_LOG(LogNiagara, Log, TEXT("%sDest%s"), *PrefixStr, *GetOpArgString(DebugInfo, (int32)VectorVM::EKernelArgs::Dest));
					for (int32 Arg = 0; Arg < DebugInfo.NumArgs; ++Arg)
					{
						UE_LOG(LogNiagara, Log, TEXT("%sArg%d%s"), *PrefixStr, Arg, *GetOpArgString(DebugInfo, Arg + 1));
					}
				}
			}
			else
			{
				UE_LOG(LogNiagara, Log, TEXT("%s FAILED TO FIND DEBUG INFO FOR PARTICLE %d!"), *PrefixStr, Particle);
			}
		}
		UE_LOG(LogNiagara, Log, TEXT("======================================"));
	}
}

static void	BeginDebuggingNiagaraSimulation(const TArray<FString>& Args, UWorld* InWorld)
{
	if (Args.Num() < 2)
		return;

	FString ActorName = Args[0];
	FString EmitterName = Args[1];
	for (TActorIterator<AActor> It(InWorld, ANiagaraActor::StaticClass()); It; ++It)
	{
		if (It->GetName() == Args[0])
		{
			if (UNiagaraComponent* Comp = Cast<UNiagaraComponent>(It->FindComponentByClass(UNiagaraComponent::StaticClass())))
			{
				if (FNiagaraEffectInstance* EffectInst = Comp->GetEffectInstance().Get())
				{
					if (FNiagaraSimulation* Sim = EffectInst->GetEmitter(EmitterName))
					{
						FNiagaraSimulationDebugger::Get().InitForDebugging(Sim, EmitterName);
					}
				}
			}
		}
	}
}

FAutoConsoleCommandWithWorldAndArgs GBeginDebuggingSimulation(
	TEXT("Niagara.BeginDebuggingSimulation"),
	TEXT(""),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(BeginDebuggingNiagaraSimulation)
	);

void DebugNextNParticlesToSpawn(const TArray<FString>& Args)
{
	if (Args.Num() >= 1)
	{
		int32 N = 0;
		int32 ToSkip = 0;
		LexicalConversion::FromString(N, *Args[0]);
		if (Args.Num() > 1)
			LexicalConversion::FromString(ToSkip, *Args[1]);

		FNiagaraSimulationDebugger::Get().DebugNextNParticles(N, ToSkip);
	}
}

FAutoConsoleCommand GDebugNextNParticlesToSpawn(
	TEXT("Niagara.DebugNextNParticlesToSpawn"),
	TEXT(""),
	FConsoleCommandWithArgsDelegate::CreateStatic(DebugNextNParticlesToSpawn)
	);

#endif

FString LexicalConversion::ToString(VectorRegister Register)
{
	return FString::Printf(TEXT("{%.4f, %.4f, %.4f, %.4f}"), VectorGetComponent(Register, 0), VectorGetComponent(Register, 1), VectorGetComponent(Register, 2), VectorGetComponent(Register, 3));
}
