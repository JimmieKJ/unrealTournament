// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "NiagaraPrivate.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraEffect.h"
#include "NiagaraSimulation.h"


UNiagaraFunctionLibrary::UNiagaraFunctionLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}


/**
* Spawns a Niagara effect at the specified world location/rotation
* @return			The spawned UNiagaraComponent
*/
UNiagaraComponent* UNiagaraFunctionLibrary::SpawnEffectAtLocation(UObject* WorldContextObject, UNiagaraEffect* EffectTemplate, FVector SpawnLocation, FRotator SpawnRotation, bool bAutoDestroy)
{
	UNiagaraComponent* PSC = NULL;
	if (EffectTemplate)
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
		if (World != nullptr)
		{
			AActor* Actor = World->GetWorldSettings();
			PSC = NewObject<UNiagaraComponent>((Actor ? Actor : (UObject*)World));
			PSC->SetAsset(EffectTemplate);
			PSC->RegisterComponentWithWorld(World);

			PSC->SetAbsolute(true, true, true);
			PSC->SetWorldLocationAndRotation(SpawnLocation, SpawnRotation);
			PSC->SetRelativeScale3D(FVector(1.f));
			//PSC->ActivateSystem(true);
		}
	}
	return PSC;
}





/**
* Spawns a Niagara effect attached to a component
* @return			The spawned UNiagaraComponent
*/
UNiagaraComponent* UNiagaraFunctionLibrary::SpawnEffectAttached(UNiagaraEffect* EffectTemplate, USceneComponent* AttachToComponent, FName AttachPointName, FVector Location, FRotator Rotation, EAttachLocation::Type LocationType, bool bAutoDestroy)
{
	UNiagaraComponent* PSC = nullptr;
	if (EffectTemplate)
	{
		if (AttachToComponent == NULL)
		{
			UE_LOG(LogScript, Warning, TEXT("UNiagaraFunctionLibrary::SpawnEffectAttached: NULL AttachComponent specified!"));
		}
		else
		{
			AActor* Actor = AttachToComponent->GetOwner();
			PSC = NewObject<UNiagaraComponent>((Actor ? Actor : (UObject*)AttachToComponent->GetWorld()));
			PSC->SetAsset(EffectTemplate);
			PSC->RegisterComponentWithWorld(AttachToComponent->GetWorld());

			PSC->AttachTo(AttachToComponent, AttachPointName);
			if (LocationType == EAttachLocation::KeepWorldPosition)
			{
				PSC->SetWorldLocationAndRotation(Location, Rotation);
			}
			else
			{
				PSC->SetRelativeLocationAndRotation(Location, Rotation);
			}
			PSC->SetRelativeScale3D(FVector(1.f));
		}
	}
	return PSC;
}


/**
* Set a constant in an emitter of a Niagara effect
*/
void UNiagaraFunctionLibrary::SetUpdateScriptConstant(UNiagaraComponent* Component, FName EmitterName, FName ConstantName, FVector Value)
{
	TArray<TSharedPtr<FNiagaraSimulation>> &Emitters = Component->GetEffectInstance()->GetEmitters();

	for (TSharedPtr<FNiagaraSimulation> &Emitter : Emitters)
	{
		FName CurName = *Emitter->GetProperties()->Name;
		if (CurName == EmitterName)
		{
			Emitter->GetProperties()->ExternalConstants.SetOrAdd(ConstantName, Value);
			break;
		}
	}
}