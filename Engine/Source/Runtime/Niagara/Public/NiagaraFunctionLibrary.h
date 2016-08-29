// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine.h" // needed for access to UBlueprintFunctionLibrary
#include "NiagaraFunctionLibrary.generated.h"


/**
* A C++ and Blueprint accessible library of utility functions for accessing Niagara simulations
* All positions & orientations are returned in Unreal reference frame & units, assuming the Leap device is located at the origin.
*/
UCLASS()
class NIAGARA_API UNiagaraFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/**
	* Spawns a Niagara effect at the specified world location/rotation
	* @return			The spawned UNiagaraComponent
	*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (Keywords = "niagara effect", WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static UNiagaraComponent* SpawnEffectAtLocation(UObject* WorldContextObject, class UNiagaraEffect* EffectTemplate, FVector Location, FRotator Rotation = FRotator::ZeroRotator, bool bAutoDestroy = true);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (Keywords = "niagara effect", UnsafeDuringActorConstruction = "true"))
	static UNiagaraComponent* SpawnEffectAttached(UNiagaraEffect* EffectTemplate, USceneComponent* AttachToComponent, FName AttachPointName, FVector Location, FRotator Rotation, EAttachLocation::Type LocationType, bool bAutoDestroy);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (Keywords = "niagara effect", UnsafeDuringActorConstruction = "true"))
	static void SetUpdateScriptConstant(UNiagaraComponent* Component, FName EmitterName, FName ConstantName, FVector Value);
};