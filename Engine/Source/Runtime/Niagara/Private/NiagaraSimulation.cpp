// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraPrivate.h"
#include "NiagaraConstants.h"
#include "NiagaraSimulation.h"
#include "NiagaraEffectRenderer.h"
#include "VectorVM.h"
#include "NiagaraEffect.h"

DECLARE_DWORD_COUNTER_STAT(TEXT("Num Custom Events"), STAT_NiagaraNumCustomEvents, STATGROUP_Niagara);

//////////////////////////////////////////////////////////////////////////

FNiagaraSimulation::FNiagaraSimulation(TWeakObjectPtr<UNiagaraEmitterProperties> InProps, FNiagaraEffectInstance* InParentEffectInstance)
: Age(0.0f)
, Loops(0)
, bIsEnabled(true)
, Data(FNiagaraDataSetID(*InProps->EmitterName, ENiagaraDataSetType::ParticleData))
, SpawnRemainder(0.0f)
, CachedBounds(ForceInit)
, EffectRenderer(nullptr)
, ParentEffectInstance(InParentEffectInstance)
, SpawnEventGenerator(this)
, bGenerateSpawnEvents(false)
, DeathEventGenerator(this)
, bGenerateDeathEvents(false)
{
	Props = InProps;

	Init();
}

FNiagaraSimulation::FNiagaraSimulation(TWeakObjectPtr<UNiagaraEmitterProperties> InProps, FNiagaraEffectInstance* InParentEffectInstance, ERHIFeatureLevel::Type InFeatureLevel)
: Age(0.0f)
, Loops(0)
, bIsEnabled(true)
, Data(FNiagaraDataSetID(*InProps->EmitterName, ENiagaraDataSetType::ParticleData))
, SpawnRemainder(0.0f)
, CachedBounds(ForceInit)
, EffectRenderer(nullptr)
, ParentEffectInstance(InParentEffectInstance)
, SpawnEventGenerator(this)
, bGenerateSpawnEvents(false)
, DeathEventGenerator(this)
, bGenerateDeathEvents(false)
{
	Props = InProps;

	Init();
	SetRenderModuleType(Props->RenderModuleType, InFeatureLevel);
}

void FNiagaraSimulation::Init()
{
	Data.Reset();
	DataSets.Empty();
	DataSetMap.Empty();
	SpawnEventGenerator.Reset();
	DeathEventGenerator.Reset();
	bGenerateSpawnEvents = false;
	bGenerateDeathEvents = false;

	if (!bIsEnabled)
	{
		return;
	}

	UNiagaraEmitterProperties* PinnedProps = Props.Get();
	if (!PinnedProps)
	{
		UE_LOG(LogNiagara, Error, TEXT("Unknown Error creating Niagara Simulation. Properties were null."));
		bIsEnabled = false;
		return;
	}

	//Check for various failure conditions and bail.
	if (!PinnedProps || !PinnedProps->UpdateScriptProps.Script || !PinnedProps->SpawnScriptProps.Script)
	{
		//TODO - Arbitrary named scripts. Would need some base functionality for Spawn/Udpate to be called that can be overriden in BPs for emitters with custom scripts.
		UE_LOG(LogNiagara, Error, TEXT("This emitter cannot be enabled because it's doesn't have both an update and spawn script."));
		bIsEnabled = false;
		return;
	}
	if (PinnedProps->SpawnScriptProps.Script->Usage.bReadsAttriubteData)
	{
		UE_LOG(LogNiagara, Error, TEXT("%s reads attribute data and so cannot be used as a spawn script. The data being read would be invalid."), *Props->SpawnScriptProps.Script->GetName());
		bIsEnabled = false;
		return;
	}
	if (PinnedProps->UpdateScriptProps.Script->Attributes.Num() == 0 || Props->SpawnScriptProps.Script->Attributes.Num() == 0)
	{
		UE_LOG(LogNiagara, Error, TEXT("This emitter cannot be enabled because it's spawn or update script doesn't have any attriubtes.."));
		bIsEnabled = false;
		return;
	}
	
	//Add the particle data to the data set map.
	//Currently just used for the tick loop but will also allow access directly to the particle data from other emitters.
	DataSetMap.Add(Data.GetID(), &Data);
	//Warn the user if there are any attributes used in the update script that are not initialized in the spawn script.
	//TODO: We need some window in the effect editor and possibly the graph editor for warnings and errors.
	for (FNiagaraVariableInfo& Attr : PinnedProps->UpdateScriptProps.Script->Attributes)
	{
		int32 FoundIdx;
		if (!PinnedProps->SpawnScriptProps.Script->Attributes.Find(Attr, FoundIdx))
		{
			UE_LOG(LogNiagara, Warning, TEXT("Attribute %s is used in the Update script for %s but it is not initialised in the Spawn script!"), *Attr.Name.ToString(), *Props->EmitterName);
		}
	}
	Data.AddVariables(PinnedProps->UpdateScriptProps.Script->Attributes);
	Data.AddVariables(PinnedProps->SpawnScriptProps.Script->Attributes);

//	ExternalConstantsMap.Empty();
//	ExternalConstantsMap.Merge(PinnedProps->UpdateScriptProps.ExternalConstants);
//	ExternalConstantsMap.Merge(PinnedProps->SpawnScriptProps.ExternalConstants);

	ExternalConstants.Empty();
	ExternalConstants.Merge(PinnedProps->UpdateScriptProps.ExternalConstants);
	ExternalConstants.Merge(PinnedProps->SpawnScriptProps.ExternalConstants);

	//Create data storage for event generators.
	for (FNiagaraEventGeneratorProperties& Generator : Props->SpawnScriptProps.EventGenerators)
	{
		FNiagaraDataSet* DataSet = DataSetMap.FindOrAdd(Generator.ID);
		if (!DataSet)
		{
			int32 Index = DataSets.Add(FNiagaraDataSet(Generator.ID));
			DataSet = &DataSets[Index];
			DataSetMap.Add(Generator.ID, DataSet);
		}

		check(DataSet);
		DataSet->AddVariables(Generator.Variables);
		DataSet->Allocate(FMath::Max(DataSet->GetDataAllocation(), (uint32)Generator.MaxEventsPerFrame));
	}

	for (FNiagaraEventGeneratorProperties& Generator : Props->UpdateScriptProps.EventGenerators)
	{
		FNiagaraDataSet* DataSet = DataSetMap.FindOrAdd(Generator.ID);
		if (!DataSet)
		{
			int32 Index = DataSets.Add(FNiagaraDataSet(Generator.ID));
			DataSet = &DataSets[Index];
			DataSetMap.Add(Generator.ID, DataSet);
		}

		check(DataSet);
		DataSet->AddVariables(Generator.Variables);
		DataSet->Allocate(FMath::Max(DataSet->GetDataAllocation(), (uint32)Generator.MaxEventsPerFrame));
	}
}

void FNiagaraSimulation::PostInit()
{
	if (bIsEnabled)
	{
		check(ParentEffectInstance);
		//Go through all our receivers and grab their generator sets so that the source emitters can do any init work they need to do.
		for (FNiagaraEventReceiverProperties& Receiver : Props->SpawnScriptProps.EventReceivers)
		{
			FNiagaraDataSet* ReceiverSet = ParentEffectInstance->GetDataSet(FNiagaraDataSetID(Receiver.SourceEventGenerator, ENiagaraDataSetType::Event), Receiver.SourceEmitter);
		}

		for (FNiagaraEventReceiverProperties& Receiver : Props->UpdateScriptProps.EventReceivers)
		{
			FNiagaraDataSet* ReceiverSet = ParentEffectInstance->GetDataSet(FNiagaraDataSetID(Receiver.SourceEventGenerator, ENiagaraDataSetType::Event), Receiver.SourceEmitter);
		}
	}
}

void FNiagaraSimulation::SetProperties(TWeakObjectPtr<UNiagaraEmitterProperties> InProps)
{ 
	Props = InProps; 
	Init();
}

void FNiagaraSimulation::SetEnabled(bool bEnabled)
{
	bIsEnabled = bEnabled;
	Init();
}

FNiagaraDataSet* FNiagaraSimulation::GetDataSet(FNiagaraDataSetID SetID)
{
	FNiagaraDataSet** SetPtr = DataSetMap.Find(SetID);
	FNiagaraDataSet* Ret = NULL;
	if (SetPtr)
	{
		Ret = *SetPtr;
	}
	else
	{
		//If the set was not found and we're looking for a system event generator then init them here.
		if (SetID == FNiagaraDataSetID::SpawnEvent)
		{
			bGenerateSpawnEvents = true;
			SpawnEventGenerator.GetDataSet().AddVariables(Props->SpawnScriptProps.Script->Attributes);
			SpawnEventGenerator.GetDataSet().AddVariables(Props->UpdateScriptProps.Script->Attributes);
			DataSetMap.Add(SpawnEventGenerator.GetDataSet().GetID(), &SpawnEventGenerator.GetDataSet());
			Ret = &SpawnEventGenerator.GetDataSet();
		}
		else if (SetID == FNiagaraDataSetID::DeathEvent)
		{
			bGenerateDeathEvents = true;
			DeathEventGenerator.GetDataSet().AddVariables(Props->SpawnScriptProps.Script->Attributes);
			DeathEventGenerator.GetDataSet().AddVariables(Props->UpdateScriptProps.Script->Attributes);
			DataSetMap.Add(DeathEventGenerator.GetDataSet().GetID(), &DeathEventGenerator.GetDataSet());
			Ret = &DeathEventGenerator.GetDataSet();
		}
		//TODO: Scene Collision Event Generator.
	}

	return Ret;
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
	int32 BytesUsed = Data.GetBytesUsed();
	for (FNiagaraDataSet& Set : DataSets)
	{
		BytesUsed += Set.GetBytesUsed();
	}
	BytesUsed += DeathEventGenerator.GetDataSet().GetBytesUsed();
	BytesUsed += SpawnEventGenerator.GetDataSet().GetBytesUsed();
	return BytesUsed;
}


/** Look for dead particles and move from the end of the list to the dead location, compacting in the process
  */
void FNiagaraSimulation::KillParticles()
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraKill);
	int32 OrigNumParticles = Data.GetNumInstances();
	int32 CurNumParticles = OrigNumParticles;
	int32 ParticleIndex = OrigNumParticles - 1;

	if (bGenerateDeathEvents)
	{
		DeathEventGenerator.BeginTrackingDeaths();
	}

	const FVector4* ParticleRelativeTimes = Data.GetVariableData(FNiagaraVariableInfo(BUILTIN_VAR_PARTICLEAGE, ENiagaraDataType::Vector));
	if (ParticleRelativeTimes)
	{
		while (ParticleIndex >= 0)
		{
			if (ParticleRelativeTimes[ParticleIndex].X > 1.0f)
			{
				if (bGenerateDeathEvents)
				{
					DeathEventGenerator.OnDeath(ParticleIndex);
				}

				check(CurNumParticles > ParticleIndex);

				// Particle is dead, move one from the end here. 
				MoveParticleToIndex(--CurNumParticles, ParticleIndex);
			}
			--ParticleIndex;
		}
	}

	Data.SetNumInstances(CurNumParticles);

	if (bGenerateDeathEvents)
	{
		DeathEventGenerator.EndTrackingDeaths();
	}

	// check if the emitter has officially died
	if (GetTickState() == NTS_Dieing && CurNumParticles == 0)
	{
		SetTickState(NTS_Dead);
	}
}

/** 
  * PreTick - handles killing dead particles, emitter death, and buffer swaps
  */
void FNiagaraSimulation::PreTick()
{
	UNiagaraEmitterProperties* PinnedProps = Props.Get();

	if (!PinnedProps || !bIsEnabled
		|| TickState == NTS_Suspended || TickState == NTS_Dead
		|| Data.GetNumInstances() == 0)
	{
		return;
	}

	check(Data.GetNumVariables() > 0);
	check(PinnedProps->SpawnScriptProps.Script);
	check(PinnedProps->UpdateScriptProps.Script);

	KillParticles();

	//Swap all data set buffers before doing the main tick on any simulation.
	for (TPair<FNiagaraDataSetID, FNiagaraDataSet*> SetPair : DataSetMap)
	{
		SetPair.Value->Tick();
	}
}

void FNiagaraSimulation::Tick(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraTick);
	SimpleTimer TickTime;

	UNiagaraEmitterProperties* PinnedProps = Props.Get();
	if (!PinnedProps || !bIsEnabled || TickState == NTS_Suspended || TickState == NTS_Dead)
	{
		return;
	}

	Age += DeltaSeconds;

	check(Data.GetNumVariables() > 0);
	check(PinnedProps->SpawnScriptProps.Script);
	check(PinnedProps->UpdateScriptProps.Script);
	
	TickEvents(DeltaSeconds);

	// Figure out how many we will spawn.
	int32 OrigNumParticles = Data.GetNumInstances();
	int32 NumToSpawn = CalcNumToSpawn(DeltaSeconds);
	int32 MaxNewParticles = OrigNumParticles + NumToSpawn;
	Data.Allocate(MaxNewParticles);

	ExternalConstants.SetOrAdd(BUILTIN_CONST_EMITTERAGE, FVector4(Age, Age, Age, Age));
	ExternalConstants.SetOrAdd(BUILTIN_CONST_DELTATIME, FVector4(DeltaSeconds, DeltaSeconds, DeltaSeconds, DeltaSeconds));

	// Simulate particles forward by DeltaSeconds.
	if (TickState==NTS_Running || TickState==NTS_Dieing)
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSimulate);
		RunVMScript(PinnedProps->UpdateScriptProps, EUnusedAttributeBehaviour::PassThrough);
	}

	//Init new particles with the spawn script.
	if (TickState==NTS_Running)
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSpawn);
		Data.SetNumInstances(MaxNewParticles);
		//For now, zero any unused attributes here. But as this is really uninitialized data we should maybe make this a more serious error.
		RunVMScript(PinnedProps->SpawnScriptProps, EUnusedAttributeBehaviour::Zero, OrigNumParticles, NumToSpawn);

		if (bGenerateSpawnEvents)
		{
			SpawnEventGenerator.OnSpawned(OrigNumParticles, NumToSpawn);
		}
	}


	CPUTimeMS = TickTime.GetElapsedMilliseconds();

	INC_DWORD_STAT_BY(STAT_NiagaraNumParticles, Data.GetNumInstances());
}


void FNiagaraSimulation::TickEvents(float DeltaSeconds)
{
	//Handle Event Actions.
	auto CallEventActions = [&](FNiagaraEventReceiverProperties& Receiver)
	{
		for (auto& Action : Receiver.EmitterActions)
		{
			if (Action)
			{
				Action->PerformAction(*this, Receiver);
			}
		}
	};

	for (auto& Receiver : Props->SpawnScriptProps.EventReceivers) { CallEventActions(Receiver); }
	for (auto& Receiver : Props->UpdateScriptProps.EventReceivers) { CallEventActions(Receiver); }
}


void FNiagaraSimulation::RunVMScript(FNiagaraEmitterScriptProperties& ScriptProps, EUnusedAttributeBehaviour UnusedAttribBehaviour)
{
	RunVMScript(ScriptProps, UnusedAttribBehaviour, 0, Data.GetNumInstances());
}

void FNiagaraSimulation::RunVMScript(FNiagaraEmitterScriptProperties& ScriptProps, EUnusedAttributeBehaviour UnusedAttribBehaviour, uint32 StartParticle)
{
	RunVMScript(ScriptProps, UnusedAttribBehaviour, StartParticle, Data.GetNumInstances() - StartParticle);
}

void FNiagaraSimulation::RunVMScript(FNiagaraEmitterScriptProperties& ScriptProps, EUnusedAttributeBehaviour UnusedAttribBehaviour, uint32 StartParticle, uint32 NumParticles)
{
	if (NumParticles == 0)
	{
		return;
	}
	
	UNiagaraScript* Script = ScriptProps.Script;

	check(Script);
	check(Script->ByteCode.Num() > 0);
	check(Script->Attributes.Num() > 0);
	check(Data.GetNumVariables() > 0);

	uint32 CurrNumParticles = Data.GetNumInstances();

	check(StartParticle + NumParticles <= CurrNumParticles);

	VectorRegister* InputRegisters[VectorVM::MaxInputRegisters] = { 0 };
	VectorRegister* OutputRegisters[VectorVM::MaxOutputRegisters] = { 0 };
	
	//The current script run will be using NumScriptAttrs. We must pick out the Attributes form the simulation data in the order that they appear in the script.
	//The remaining attributes being handled according to UnusedAttribBehaviour
	const int32 NumScriptAttrs = Script->Attributes.Num();

	check(NumScriptAttrs < VectorVM::MaxInputRegisters);
	check(NumScriptAttrs < VectorVM::MaxOutputRegisters);

	int32 NumInputRegisters = 0;
	int32 NumOutputRegitsers = 0;

	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSimRegisterSetup);

		// Setup input and output registers.
		for (TPair<FNiagaraVariableInfo, uint32> AttrIndexPair : Data.GetVariables())
		{
			const FNiagaraVariableInfo& AttrInfo = AttrIndexPair.Key;

			FVector4* InBuff;
			FVector4* OutBuff;
			Data.GetVariableData(AttrInfo, InBuff, OutBuff, StartParticle);

			int32 AttrIndex = Script->Attributes.Find(AttrInfo);
			if (AttrIndex != INDEX_NONE)
			{
				//The input buffer can be NULL but only if this script never accesses it.
				check(InBuff || !Script->Usage.bReadsAttriubteData);

				//This attribute is used in this script so set it to the correct Input and Output buffer locations.
				InputRegisters[AttrIndex] = (VectorRegister*)InBuff;
				OutputRegisters[AttrIndex] = (VectorRegister*)OutBuff;

				NumInputRegisters += InBuff ? 1 : 0;
				NumOutputRegitsers += OutBuff ? 1 : 0;
			}
			else
			{
				//This attribute is not used in this script so handle it accordingly.
				check(OutBuff);

				if (UnusedAttribBehaviour == EUnusedAttributeBehaviour::PassThrough)
				{
					Data.GetVariableBuffer(AttrInfo).EnablePassThrough();
				}
				else if (UnusedAttribBehaviour == EUnusedAttributeBehaviour::Copy)
				{
					check(InBuff);
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
	}

	// Fill constant table with required emitter constants and internal script constants.
	TArray<FVector4> ConstantTable;
	//Script->ConstantData.FillConstantTable(ExternalConstantsMap, ConstantTable);
	Script->ConstantData.FillConstantTable(ExternalConstants, ConstantTable);


	// Fill in the shared data table
	TArray<FVectorVMSharedDataView, TInlineAllocator<64>> SharedDataTable;
	
	for (FNiagaraEventReceiverProperties Receiver : ScriptProps.EventReceivers)
	{
		//Look up to the effect for receiver sets as they can come from other emitters.
		FNiagaraDataSetID ReceiverID = FNiagaraDataSetID(Receiver.Name, ENiagaraDataSetType::Event);
		FNiagaraDataSetID SourceID = FNiagaraDataSetID(Receiver.SourceEventGenerator, ENiagaraDataSetType::Event);
		
		FNiagaraDataSet* SourceSet = ParentEffectInstance->GetDataSet(SourceID, Receiver.SourceEmitter);
		FNiagaraDataSetProperties* ScriptEventReceiver = Script->EventReceivers.FindByPredicate([&](const FNiagaraDataSetProperties& R){ return ReceiverID == R.ID; });

		check(ScriptEventReceiver);

		uint32 NumInstances = SourceSet ? SourceSet->GetPrevNumInstances() : 0;
		int32 ViewIdx = SharedDataTable.Add(FVectorVMSharedDataView(NumInstances, NumInstances - 1));
		FVectorVMSharedDataView& View = SharedDataTable[ViewIdx];
		for (FNiagaraVariableInfo& Var : ScriptEventReceiver->Variables)
		{
			FVector4* ReadData = SourceSet ? SourceSet->GetPrevVariableData(Var) : nullptr;
			View.AddBuffer(ReadData);			
		}
	}

	//Keep track of all the generator sets so that we can set their instance numbers after the VM run.
	int32 StartOfGeneratorSets = SharedDataTable.Num();
	TArray<FNiagaraDataSet*, TInlineAllocator<64>> GeneratorSets;

	//TODO: Allow growing arrays for shared data / events in VM? HMMMM
	for (FNiagaraEventGeneratorProperties Generator : ScriptProps.EventGenerators)
	{
		FNiagaraDataSet* GeneratorSet = DataSetMap.FindChecked(Generator.ID);
		GeneratorSets.Add(GeneratorSet);
	
		check(GeneratorSet);

		uint32 Allocation = GeneratorSet->GetDataAllocation();
		uint32 NumInstances = GeneratorSet->GetNumInstances();

		int32 ViewIdx = SharedDataTable.Add(FVectorVMSharedDataView(Allocation, NumInstances));
		FVectorVMSharedDataView& View = SharedDataTable[ViewIdx];

		for (FNiagaraVariableInfo& Var : Generator.Variables)
		{
			FVector4* WriteData = GeneratorSet->GetVariableData(Var);
			View.AddBuffer(WriteData);
		}
	}
	
	VectorVM::Exec(
		Script->ByteCode.GetData(),
		InputRegisters,
		NumInputRegisters,
		OutputRegisters,
		NumOutputRegitsers,
		ConstantTable.GetData(),
		SharedDataTable.Num() > 0 ? SharedDataTable.GetData() : NULL,
		NumParticles
		);

	//Go through our generators and set the num instances that were actually written ready for them to be read by receivers.
	for (int32 i = 0; i < GeneratorSets.Num(); ++i)
	{
		FVectorVMSharedDataView& View = SharedDataTable[StartOfGeneratorSets + i];
		FNiagaraDataSet* GeneratorSet = GeneratorSets[i];
		check(GeneratorSet);
		GeneratorSet->SetNumInstances(View.GetCounter());
		INC_DWORD_STAT_BY(STAT_NiagaraNumCustomEvents, View.GetCounter());
	}
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
				UE_LOG(LogNiagara, Error, TEXT("Cannot render %s because it does not define attriubte %s."), *Props->EmitterName, *Attr.Name.ToString());
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
		case RMT_Meshes: EffectRenderer = new NiagaraEffectRendererMeshes(FeatureLevel, Props->RendererProperties);
			break;
		default:	EffectRenderer = new NiagaraEffectRendererSprites(FeatureLevel, Props->RendererProperties);
					Props->RenderModuleType = RMT_Sprites;
					break;
		}

		EffectRenderer->SetMaterial(Material, FeatureLevel);

		CheckAttriubtesForRenderer();
	}
}
