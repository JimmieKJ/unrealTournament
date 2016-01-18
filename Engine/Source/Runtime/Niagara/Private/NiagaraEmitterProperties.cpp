// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "NiagaraPrivate.h"
#include "NiagaraEmitterProperties.h"
#include "NiagaraScriptSourceBase.h"

void FNiagaraEmitterScriptProperties::Init(UNiagaraEmitterProperties* EmitterProps)
{
	if (Script)
	{
		//TODO - Don't need to do this for every init. Only when the script has changed.
		//Store a guid or something

		ExternalConstants.Init(EmitterProps, this);

		TArray<FNiagaraEventGeneratorProperties> NewEventGenerators;
		TArray<FNiagaraDataSetProperties>& EventGeneratorDataSets = Script->EventGenerators;
		for (int32 i = 0; i < EventGeneratorDataSets.Num(); ++i)
		{
			FNiagaraDataSetProperties& CurrGeneratorSet = EventGeneratorDataSets[i];

			//If we already had customized data for this generator then use that, otherwise add a new defaulted one.
			FNiagaraEventGeneratorProperties* ExistingEventGenerator = EventGenerators.FindByPredicate(
				[&](FNiagaraEventGeneratorProperties& Generator)
			{
				return Generator.ID == CurrGeneratorSet.ID;
			}
			);

			if (ExistingEventGenerator)
			{
				NewEventGenerators.Add(*ExistingEventGenerator);
			}
			else
			{
				FNiagaraEventGeneratorProperties NewGenerator;
				NewGenerator.ID = CurrGeneratorSet.ID;
				NewGenerator.Variables = CurrGeneratorSet.Variables;
				NewEventGenerators.Add(NewGenerator);
			}
		}
		EventGenerators = NewEventGenerators;

		TArray<FNiagaraEventReceiverProperties> NewEventReceivers;
		TArray<FNiagaraDataSetProperties>& EventReceiverDataSets = Script->EventReceivers;
		for (int32 i = 0; i < EventReceiverDataSets.Num(); ++i)
		{
			FNiagaraDataSetProperties& CurrReceiverSet = EventReceiverDataSets[i];

			//If we already had customized data for this receiver then use that, otherwise add a new defaulted one.
			FNiagaraEventReceiverProperties* ExistingEventReceiver = EventReceivers.FindByPredicate(
				[&](FNiagaraEventReceiverProperties& Receiver)
			{
				return Receiver.Name == CurrReceiverSet.ID.Name;
			}
			);

			if (ExistingEventReceiver)
			{
				NewEventReceivers.Add(*ExistingEventReceiver);
			}
			else
			{
				FNiagaraEventReceiverProperties NewEventReceiver;
				NewEventReceiver.Name = CurrReceiverSet.ID.Name;
				NewEventReceivers.Add(NewEventReceiver);
			}
		}

		EventReceivers = NewEventReceivers;
	}
	else
	{
		ExternalConstants.Empty();
		EventReceivers.Empty();
		EventGenerators.Empty();
	}
}

//////////////////////////////////////////////////////////////////////////

UNiagaraEmitterProperties::UNiagaraEmitterProperties(const FObjectInitializer& Initiilaizer)
: Super(Initiilaizer)
, EmitterName(TEXT("New Emitter"))
, bIsEnabled(true)
, SpawnRate(50)
, Material(nullptr)
, RenderModuleType(RMT_Sprites)
, StartTime(0.0f)
, EndTime(0.0f)
, RendererProperties(nullptr)
, NumLoops(0)
{
}

void UNiagaraEmitterProperties::Init()
{
	SpawnScriptProps.Init(this);
	UpdateScriptProps.Init(this);
}

void UNiagaraEmitterProperties::InitFromOldStruct(FDeprecatedNiagaraEmitterProperties& OldStruct)
{
	EmitterName = OldStruct.Name;
	bIsEnabled = OldStruct.bIsEnabled;
	SpawnRate = OldStruct.SpawnRate;
	UpdateScriptProps.Script = OldStruct.UpdateScript;
	SpawnScriptProps.Script = OldStruct.SpawnScript;
	Material = OldStruct.Material;
	RenderModuleType = OldStruct.RenderModuleType;
	StartTime = OldStruct.StartTime;
	EndTime = OldStruct.EndTime;
	RendererProperties = OldStruct.RendererProperties;
	NumLoops = OldStruct.NumLoops;

#if WITH_EDITORONLY_DATA
	//Ensure both scripts are post loaded and compile them so that their constant arrays are correct.
	if (SpawnScriptProps.Script)
	{
		SpawnScriptProps.Script->ConditionalPostLoad();
		if (SpawnScriptProps.Script->Source)
			SpawnScriptProps.Script->Source->Compile();
	}
	if (UpdateScriptProps.Script)
	{
		UpdateScriptProps.Script->ConditionalPostLoad();
		if (UpdateScriptProps.Script->Source)
			UpdateScriptProps.Script->Source->Compile();
	}
#endif

	UpdateScriptProps.ExternalConstants.InitFromOldMap(OldStruct.ExternalConstants);
	SpawnScriptProps.ExternalConstants.InitFromOldMap(OldStruct.ExternalSpawnConstants);
	//Also init the spawn constants with the update constants as they used to combined
	SpawnScriptProps.ExternalConstants.InitFromOldMap(OldStruct.ExternalConstants);
	UpdateScriptProps.ExternalConstants.InitFromOldMap(OldStruct.ExternalSpawnConstants);

	Init();
}

#if WITH_EDITOR
void UNiagaraEmitterProperties::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	Init();
}
#endif