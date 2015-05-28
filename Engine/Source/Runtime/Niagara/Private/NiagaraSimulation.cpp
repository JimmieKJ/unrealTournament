// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NiagaraPrivate.h"
#include "NiagaraSimulation.h"
#include "NiagaraEffectRenderer.h"
#include "VectorVM.h"


FNiagaraSimulation::FNiagaraSimulation(FNiagaraEmitterProperties *InProps) 
: Age(0.0f)
, bIsEnabled(true)
, SpawnRemainder(0.0f)
, CachedBounds(ForceInit)
, EffectRenderer(nullptr)
{
	Props = InProps;

	Init();
}

FNiagaraSimulation::FNiagaraSimulation(FNiagaraEmitterProperties *InProps, ERHIFeatureLevel::Type InFeatureLevel)
	: Age(0.0f)
	, bIsEnabled(true)
	, SpawnRemainder(0.0f)
	, CachedBounds(ForceInit)
	, EffectRenderer(nullptr)
{
	Props = InProps;

	Init();
	SetRenderModuleType(Props->RenderModuleType, InFeatureLevel);
}

void FNiagaraSimulation::Init()
{
	if (Props && Props->UpdateScript && Props->SpawnScript )
	{
		if (Props->UpdateScript->Attributes.Num() == 0 || Props->SpawnScript->Attributes.Num() == 0)
		{
			Data.Reset();
			bIsEnabled = false;
			UE_LOG(LogNiagara, Error, TEXT("This emitter cannot be enabled because it's spawn or update script doesn't have any attriubtes.."));
		}
		else
		{
			//Warn the user if there are any attributes used in the update script that are not initialized in the spawn script.
			//TODO: We need some window in the effect editor and possibly the graph editor for warnings and errors.
			for (FNiagaraVariableInfo& Attr : Props->UpdateScript->Attributes)
			{
				int32 FoundIdx;
				if (!Props->SpawnScript->Attributes.Find(Attr, FoundIdx))
				{
					UE_LOG(LogNiagara, Warning, TEXT("Attribute %s is used in the Update script for %s but it is not initialised in the Spawn script!"), *Attr.Name.ToString(), *Props->Name);
				}
			}
			Data.AddAttributes(Props->UpdateScript->Attributes);
			Data.AddAttributes(Props->SpawnScript->Attributes);

			Constants.Empty();
			Constants.Merge(Props->ExternalConstants);
		}
	}
	else
	{
		Data.Reset();//Clear the attributes to mark the sim as disabled independatly of the user set Enabled flag.
		bIsEnabled = false;//Also force the user flag to give an indication to the user.
		//TODO - Arbitrary named scripts. Would need some base functionality for Spawn/Udpate to be called that can be overriden in BPs for emitters with custom scripts.
		UE_LOG(LogNiagara, Error, TEXT("This emitter cannot be enabled because it's doesn't have both an update and spawn script."));
	}
}

void FNiagaraSimulation::SetProperties(FNiagaraEmitterProperties *InProps)	
{ 
	Props = InProps; 
	Init();
}

void FNiagaraSimulation::SetSpawnScript(UNiagaraScript* Script)
{
	Props->SpawnScript = Script;
	Init();
}

void FNiagaraSimulation::SetUpdateScript(UNiagaraScript* Script)
{
	Props->UpdateScript = Script;
	Init();
}

void FNiagaraSimulation::SetEnabled(bool bEnabled)
{
	bIsEnabled = bEnabled;
	Init();
}


float FNiagaraSimulation::GetTotalCPUTime()
{
	float Total = CPUTimeMS;
	if (EffectRenderer)
	{
		Total += EffectRenderer->GetCPUTimeMS();
	}

	return Total;
}

int FNiagaraSimulation::GetTotalBytesUsed()
{
	return Data.GetBytesUsed();
}


void FNiagaraSimulation::Tick(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraTick);

	if (!bIsEnabled)
		return;

	SimpleTimer TickTime;

	check(Data.GetNumAttributes() > 0);
	check(Props->SpawnScript);
	check(Props->UpdateScript);

	// Cache the ComponentToWorld transform.
//	CachedComponentToWorld = Component.GetComponentToWorld();

	Data.SwapBuffers();
	Data.SetNumParticles(Data.GetPrevNumParticles());

	int32 OrigNumParticles = Data.GetNumParticles();
	int32 NumToSpawn = 0;

	// Figure out how many we will spawn.
	NumToSpawn = CalcNumToSpawn(DeltaSeconds);
	int32 MaxNewParticles = OrigNumParticles + NumToSpawn;
	Data.Allocate(MaxNewParticles);

	Age += DeltaSeconds;
	Constants.SetOrAdd(TEXT("Emitter Age"), FVector4(Age, Age, Age, Age));
	Constants.SetOrAdd(TEXT("Delta Time"), FVector4(DeltaSeconds, DeltaSeconds, DeltaSeconds, DeltaSeconds));

	// Simulate particles forward by DeltaSeconds.
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSimulate);
		RunVMScript(Props->UpdateScript, EUnusedAttributeBehaviour::Copy);
	}
	
	//Init new particles with the spawn script.
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSpawn);
		Data.SetNumParticles(MaxNewParticles);
		//For now, zero any unused attributes here. But as this is really uninitialized data we should maybe make this a more serious error.
		RunVMScript(Props->SpawnScript, EUnusedAttributeBehaviour::Zero, OrigNumParticles, NumToSpawn);
	}

	// Iterate over looking for dead particles and move from the end of the list to the dead location, compacting in the process
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraKill);
		int32 CurNumParticles = OrigNumParticles = Data.GetNumParticles();
		int32 ParticleIndex = 0;
		const FVector4* ParticleRelativeTimes = Data.GetAttributeData(FNiagaraVariableInfo(FName(TEXT("Age")), ENiagaraDataType::Vector));
		if (ParticleRelativeTimes)
		{
			while (ParticleIndex < OrigNumParticles)
			{
				if (ParticleRelativeTimes[ParticleIndex].X > 1.0f)
				{
					// Particle is dead, move one from the end here. 
					MoveParticleToIndex(--CurNumParticles, ParticleIndex);
				}
				ParticleIndex++;
			}
		}
		Data.SetNumParticles(CurNumParticles);
	}

	CPUTimeMS = TickTime.GetElapsedMilliseconds();

	DECLARE_DWORD_COUNTER_STAT(TEXT("NumParticles"), STAT_NiagaraNumParticles, STATGROUP_Niagara);
	INC_DWORD_STAT_BY(STAT_NiagaraNumParticles, Data.GetNumParticles());

}

void FNiagaraSimulation::RunVMScript(UNiagaraScript* Script, EUnusedAttributeBehaviour UnusedAttribBehaviour)
{
	RunVMScript(Script, UnusedAttribBehaviour, 0, Data.GetNumParticles());
}

void FNiagaraSimulation::RunVMScript(UNiagaraScript* Script, EUnusedAttributeBehaviour UnusedAttribBehaviour, uint32 StartParticle)
{
	RunVMScript(Script, UnusedAttribBehaviour, StartParticle, Data.GetNumParticles() - StartParticle);
}

void FNiagaraSimulation::RunVMScript(UNiagaraScript* Script, EUnusedAttributeBehaviour UnusedAttribBehaviour, uint32 StartParticle, uint32 NumParticles)
{
	if (NumParticles == 0)
		return;

	check(Script);
	check(Script->ByteCode.Num() > 0);
	check(Script->Attributes.Num() > 0);
	check(Data.GetNumAttributes() > 0);

	uint32 CurrNumParticles = Data.GetNumParticles();

	check(StartParticle + NumParticles <= CurrNumParticles);

	const FVector4* InputBuffer = Data.GetPreviousBuffer();
	const FVector4* OutputBuffer = Data.GetCurrentBuffer();

	uint32 InputAttributeStride = Data.GetPrevParticleAllocation();
	uint32 OutputAttributeStride = Data.GetParticleAllocation();

	VectorRegister* InputRegisters[VectorVM::MaxInputRegisters] = { 0 };
	VectorRegister* OutputRegisters[VectorVM::MaxOutputRegisters] = { 0 };
	
	//The current script run will be using NumScriptAttrs. We must pick out the Attributes form the simulation data in the order that they appear in the script.
	//The remaining attributes being handled according to UnusedAttribBehaviour
	const int32 NumSimulationAttrs = Data.GetNumAttributes();
	const int32 NumScriptAttrs = Script->Attributes.Num();

	check(NumScriptAttrs < VectorVM::MaxInputRegisters);
	check(NumScriptAttrs < VectorVM::MaxOutputRegisters);

	// Setup input and output registers.
	for (TPair<FNiagaraVariableInfo, uint32> AttrIndexPair : Data.GetAttributes())
	{
		const FNiagaraVariableInfo& AttrInfo = AttrIndexPair.Key;

		FVector4* InBuff;
		FVector4* OutBuff;
		Data.GetAttributeData(AttrInfo, InBuff, OutBuff, StartParticle);

		int32 AttrIndex = Script->Attributes.Find(AttrInfo);
		if (AttrIndex != INDEX_NONE)
		{
			//This attribute is used in this script so set it to the correct Input and Output buffer locations.
			InputRegisters[AttrIndex] = (VectorRegister*)InBuff;
			OutputRegisters[AttrIndex] = (VectorRegister*)OutBuff;
		}	
		else
		{
			//This attribute is not used in this script so handle it accordingly.
			check(OutBuff != NULL);
			if (UnusedAttribBehaviour == EUnusedAttributeBehaviour::Copy)
			{
				check(InBuff != NULL);
				FMemory::Memcpy(OutBuff, InBuff, NumParticles * sizeof(FVector4));
			}
			else if (UnusedAttribBehaviour == EUnusedAttributeBehaviour::Zero)
			{
				FMemory::Memzero(OutBuff, NumParticles * sizeof(FVector4));
			}
			else if (UnusedAttribBehaviour == EUnusedAttributeBehaviour::MarkInvalid)
			{
				FMemory::Memset(OutBuff, NIAGARA_INVALID_MEMORY, NumParticles * sizeof(FVector4));
			}
		}
	}

	//Fill constant table with required emitter constants and internal script constants.
	TArray<FVector4> ConstantTable;
	TArray<FNiagaraDataObject *>DataObjTable;
	Script->ConstantData.FillConstantTable(Constants, ConstantTable, DataObjTable);

	VectorVM::Exec(
		Script->ByteCode.GetData(),
		InputRegisters,
		NumScriptAttrs,
		OutputRegisters,
		NumScriptAttrs,
		ConstantTable.GetData(),
		DataObjTable.GetData(),
		NumParticles
		);
}

bool FNiagaraSimulation::CheckAttriubtesForRenderer()
{
	bool bOk = true;
	if (EffectRenderer)
	{
		const TArray<FNiagaraVariableInfo>& RequiredAttrs = EffectRenderer->GetRequiredAttributes();

		for (auto& Attr : RequiredAttrs)
		{
			if (!Data.HasAttriubte(Attr))
			{
				bOk = false;
				UE_LOG(LogNiagara, Error, TEXT("Cannot render %s because it does not define attriubte %s."), *Props->Name, *Attr.Name.ToString());
			}
		}
	}
	return bOk;
}

/** Replace the current effect renderer with a new one of Type, transferring the existing material over
 *	to the new renderer. Don't forget to call RenderModuleUpdate on the SceneProxy after calling this! 
 */
void FNiagaraSimulation::SetRenderModuleType(EEmitterRenderModuleType Type, ERHIFeatureLevel::Type FeatureLevel)
{
	if (Type != Props->RenderModuleType || EffectRenderer==nullptr)
	{
		UMaterial *Material = UMaterial::GetDefaultMaterial(MD_Surface);

		if (EffectRenderer)
		{
			Material = EffectRenderer->GetMaterial();
			delete EffectRenderer;
		}
		else
		{
			if (Props->Material)
			{
				Material = Props->Material;
			}
		}

		Props->RenderModuleType = Type;
		switch (Type)
		{
		case RMT_Sprites: EffectRenderer = new NiagaraEffectRendererSprites(FeatureLevel, Props->RendererProperties);
			break;
		case RMT_Ribbon: EffectRenderer = new NiagaraEffectRendererRibbon(FeatureLevel, Props->RendererProperties);
			break;
		default:	EffectRenderer = new NiagaraEffectRendererSprites(FeatureLevel, Props->RendererProperties);
					Props->RenderModuleType = RMT_Sprites;
					break;
		}

		EffectRenderer->SetMaterial(Material, FeatureLevel);
		CheckAttriubtesForRenderer();
	}
}
