// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraScript.h"
#include "NiagaraDataSet.h"
#include "NiagaraEmitterProperties.generated.h"

class NiagaraEffectRenderer;
class UNiagaraEventReceiverEmitterAction;

UENUM()
enum EEmitterRenderModuleType
{
	RMT_None = 0,
	RMT_Sprites,
	RMT_Ribbon,
	RMT_Trails,
	RMT_Meshes
};

//TODO: Event action that spawns other whole effects?
//One that calls a BP exposed delegate?

USTRUCT()
struct FNiagaraEventReceiverProperties
{
	GENERATED_BODY()

	FNiagaraEventReceiverProperties()
	: Name(NAME_None)
	, SourceEventGenerator(NAME_None)
	, SourceEmitter(NAME_None)
	{

	}

	FNiagaraEventReceiverProperties(FName InName, FName InEventGenerator, FName InSourceEmitter)
		: Name(InName)
		, SourceEventGenerator(InEventGenerator)
		, SourceEmitter(InSourceEmitter)
	{

	}

	/** The name of this receiver. */
	UPROPERTY(EditAnywhere, Category = "Event Receiver")
	FName Name;

	/** The name of the EventGenerator to bind to. */
	UPROPERTY(EditAnywhere, Category = "Event Receiver")
	FName SourceEventGenerator;

	/** The name of the emitter from which the Event Generator is taken. */
	UPROPERTY(EditAnywhere, Category = "Event Receiver")
	FName SourceEmitter;

	UPROPERTY(EditAnywhere, Instanced, Category = "Event Receiver")
	TArray<UNiagaraEventReceiverEmitterAction*> EmitterActions;
};

USTRUCT()
struct FNiagaraEventGeneratorProperties : public FNiagaraDataSetProperties
{
	GENERATED_BODY()

	FNiagaraEventGeneratorProperties()
	: MaxEventsPerFrame(64)
	{

	}

	FNiagaraEventGeneratorProperties(FNiagaraDataSetProperties& Props, int32 InMaxEventsPerFrame)
	: FNiagaraDataSetProperties(Props)
	, MaxEventsPerFrame(InMaxEventsPerFrame)
	{

	}

	/** Max Number of Events that can be generated per frame. */
	UPROPERTY(EditAnywhere, Category = "Event Receiver")
	int32 MaxEventsPerFrame; //TODO - More complex allocation so that we can grow dynamically if more space is needed ?
};

USTRUCT()
struct FNiagaraEmitterScriptProperties
{
	GENERATED_BODY()
	
 	UPROPERTY(EditAnywhere, Category = "Script")
 	UNiagaraScript *Script;

	UPROPERTY(EditAnywhere, Category = "Script")
 	FNiagaraConstants ExternalConstants;

	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Script")
	TArray<FNiagaraEventReceiverProperties> EventReceivers;

	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Script")
	TArray<FNiagaraEventGeneratorProperties> EventGenerators;

	NIAGARA_API void Init(UNiagaraEmitterProperties* EmitterProps);
};

/** 
 *	UNiagaraEmitterProperties stores the attributes of an FNiagaraSimulation
 *	that need to be serialized and are used for its initialization 
 */
UCLASS(MinimalAPI)
class UNiagaraEmitterProperties : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	NIAGARA_API void Init();

	//Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//End UObject Interface

	UPROPERTY(EditAnywhere, Category = "Emitter")
	FString EmitterName;
	UPROPERTY(EditAnywhere, Category = "Emitter")
	bool bIsEnabled;
	UPROPERTY(EditAnywhere, Category = "Emitter")
	float SpawnRate;
	UPROPERTY(EditAnywhere, Category = "Emitter")
	UMaterial *Material;
	UPROPERTY(EditAnywhere, Category = "Emitter")
	TEnumAsByte<EEmitterRenderModuleType> RenderModuleType;

	UPROPERTY(EditAnywhere, Category = "Emitter")
	float StartTime;
	UPROPERTY(EditAnywhere, Category = "Emitter")
	float EndTime;

	//Can get rid of the enum and just have users select a class for this directly in the UI?
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Emitter")
	class UNiagaraEffectRendererProperties *RendererProperties;

	UPROPERTY(EditAnywhere, Category = "Emitter")
	int32 NumLoops;

	UPROPERTY(EditAnywhere, Category = "Emitter", meta = (ShowOnlyInnerProperties))
	FNiagaraEmitterScriptProperties UpdateScriptProps;

	UPROPERTY(EditAnywhere, Category = "Emitter", meta = (ShowOnlyInnerProperties))
	FNiagaraEmitterScriptProperties SpawnScriptProps;
};


