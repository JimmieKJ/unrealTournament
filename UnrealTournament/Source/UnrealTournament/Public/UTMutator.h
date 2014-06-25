// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTMutator.generated.h"

UCLASS(Blueprintable, Abstract, Meta = (ChildCanTick))
class AUTMutator : public AInfo
{
	GENERATED_UCLASS_BODY()

	/** next in linked list of mutators */
	UPROPERTY(BlueprintReadOnly, Category = Mutator)
	AUTMutator* NextMutator;

	/** only one mutator of a given group is allowed to be active */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Mutator)
	TArray<FName> GroupNames;

	/** menu display name */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Mutator)
	FText DisplayName;

	/** adds a mutator to the end of the linked list */
	virtual void AddMutator(AUTMutator* OtherMutator)
	{
		if (NextMutator == NULL)
		{
			NextMutator = OtherMutator;
		}
		else
		{
			NextMutator->AddMutator(OtherMutator);
		}
	}

	/** called when the mutator is added; at game startup this will be before BeginPlay() is called and before the first PlayerController is spawned on listen servers
	 * so it is a better place to modify certain game properties than BeginPlay()
	 * Options is the URL parameters, same as GameMode::InitGame()
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void Init(const FString& Options);

	/** allows changing or reacting to player login URL options */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void ModifyLogin(FString& Portal, FString& Options);

	/** if any mutator returns true for AlwaysKeep() then an Actor won't be destroyed by CheckRelevance() returning false
	* when returning true, set bPreventModify to true as well to prevent CheckRelevance() from even being called;
	* otherwise it is called but return value ignored so other mutators can modify even if they can't delete
	* MAKE SURE TO CALL SUPER TO PROCESS ADDITIONAL MUTATORS
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	bool AlwaysKeep(AActor* Other, bool& bPreventModify);

	/** entry point for mutators modifying, replacing, or destroying Actors
	 * return false to destroy Other
	 * note that certain critical Actors such as PlayerControllers can't be destroyed, but we'll still call this code path to allow mutators
	 * to change properties on them
	 * MAKE SURE TO CALL SUPER TO PROCESS ADDITIONAL MUTATORS
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	bool CheckRelevance(AActor* Other);

	/** called when a player pawn is spawned and to reset its parameters (like movement speed and such)*/
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void ModifyPlayer(APawn* Other);
};