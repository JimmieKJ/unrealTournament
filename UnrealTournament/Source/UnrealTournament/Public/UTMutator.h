// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTMutator.generated.h"

UCLASS(Blueprintable, Abstract, Meta = (ChildCanTick), Config = Game)
class UNREALTOURNAMENT_API AUTMutator : public AInfo
{
	GENERATED_UCLASS_BODY()

	/** next in linked list of mutators */
	UPROPERTY(BlueprintReadOnly, Category = Mutator)
	AUTMutator* NextMutator;

	/** only one mutator of a given group is allowed to be active */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Mutator)
	TArray<FName> GroupNames;

	/** menu display name
	 * if this is empty the mutator is not shown in menus (assumed meant for internal use only)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Mutator)
	FText DisplayName;

	/** UMG menu for configuring the mutator (optional) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = UI)
	TSubclassOf<class UUserWidget> ConfigMenu;

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

	/** called when a player joins the game initially or is rebuilt due to seamless travel, after the Controller is created and fully initialized with its local viewport or remote connection (so RPCs are OK here)
	 * this function is also called for bots
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void PostPlayerInit(AController* C);

	/** called when a player (human or bot) exits the game */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void NotifyLogout(AController* C);

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

	/** allows mutators to modify/react to damage */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void ModifyDamage(int32& Damage, FVector& Momentum, APawn* Injured, AController* InstigatedBy, const FHitResult& HitInfo, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType);

	/** score a kill (or suicide) */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void ScoreKill(AController* Killer, AController* Other, TSubclassOf<UDamageType> DamageType);

	/** score a game object event (flag capture, return, etc) */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason);

	/** OverridePickupQuery()
	* when pawn wants to pickup something, mutators are given a chance to modify it. If this function
	* returns true, bAllowPickup will determine if the object can be picked up.
	* Note that overriding bAllowPickup to false from this function without disabling the item in some way will have detrimental effects on bots,
	* since the pickup's AI interface will assume the default behavior and keep telling the bot to pick the item up.
	* @param Other the Pawn that wants the item
	* @param ItemClass the Inventory class the Pawn can pick up
	* @param Pickup the Actor containing that item (this may be a PickupFactory or it may be a DroppedPickup)
	* @param bAllowPickup (out) whether or not the Pickup actor should give its item to Other
	* @return whether or not to override the default behavior with the value of bAllowPickup
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	bool OverridePickupQuery(APawn* Other, TSubclassOf<AUTInventory> ItemClass, AActor* Pickup, bool& bAllowPickup);

	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void ScoreDamage(int32 DamageAmount, AController* Victim, AController* Attacker);

	/** notification that the game's state has changed (e.g. pre-match -> in progress -> half time -> in progress -> game over) */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void NotifyMatchStateChange(FName NewState);

	/** allows preserving Actors during travel
	 * this function does not need to invoke NextMutator; the GameMode guarantees everyone gets a shot
	 */
	virtual void GetSeamlessTravelActorList(bool bToEntry, TArray<AActor*>& ActorList)
	{
	}
};