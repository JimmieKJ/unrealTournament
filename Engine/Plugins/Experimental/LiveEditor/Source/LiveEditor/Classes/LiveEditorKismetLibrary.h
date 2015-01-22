// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveEditorTypes.h"

#include "LiveEditorKismetLibrary.generated.h"

UCLASS()
class ULiveEditorKismetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintPure, Category="Development|LiveEditor")
	static float LiveModifyFloat( float InFloat );

	UFUNCTION(BlueprintCallable, Category="Development|LiveEditor", meta=(HidePin="Target", DefaultToSelf="Target"))
	static void RegisterForLiveEditEvent( UObject *Target, FName EventName, FName Description, const TArray< TEnumAsByte<ELiveEditControllerType::Type> > &PermittedBindings );

	UFUNCTION(BlueprintCallable, Category="Development|LiveEditor", meta=(HidePin="Target", DefaultToSelf="Target"))
	static void UnRegisterLiveEditableVariable( UObject *Target, FName VarName );

	UFUNCTION(BlueprintCallable, Category="Development|LiveEditor", meta=(HidePin="Target", DefaultToSelf="Target"))
	static void UnRegisterAllLiveEditableVariables( UObject *Target );

	UFUNCTION(BlueprintCallable, Category="Development|LiveEditor")
	static AActor *GetSelectedEditorObject();

	UFUNCTION(BlueprintCallable, Category="Development|LiveEditor")
	static void GetAllSelectedEditorObjects( UClass *OfClass, TArray<UObject*> &SelectedObjects );

	UFUNCTION(BlueprintCallable, Category="Development|LiveEditor")
	static UObject *GetSelectedContentBrowserObject();

	UFUNCTION(BlueprintCallable, Category="Development|LiveEditor")
	static AActor *GetActorArchetype( class AActor *Actor );

	UFUNCTION(BlueprintCallable, Category="Development|LiveEditor")
	static void MarkDirty( AActor *Target );

	UFUNCTION(BlueprintCallable, Category="Development|LiveEditor")
	static void ReplicateChangesToChildren( FName PropertyName, UObject *Archetype );

	UFUNCTION(BlueprintPure, Category="Development|LiveEditor")
	static UObject *GetBlueprintClassDefaultObject( UBlueprint *Blueprint );

	UFUNCTION(BlueprintCallable, Category="Development|LiveEditor")
	static void ModifyPropertyByName( UObject *Target, const FString &PropertyName, TEnumAsByte<ELiveEditControllerType::Type> ControlType, float Delta, int32 MidiValue, bool bShouldClamp, float ClampMin=0.0f, float ClampMax=0.0f );
};
