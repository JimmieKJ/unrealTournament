// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTInventory.generated.h"

UCLASS(Blueprintable)
class AUTInventory : public AActor
{
	GENERATED_UCLASS_BODY()

	friend void AUTCharacter::AddInventory(AUTInventory*, bool);
	friend void AUTCharacter::RemoveInventory(AUTInventory*);

protected:
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Inventory")
	AUTInventory* NextInventory;

	UPROPERTY()
	AUTCharacter* UTOwner;

	/** called when this inventory item has been given to the specified character */
	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly)
	void eventGivenTo(AUTCharacter* NewOwner, bool bAutoActivate);
	virtual void GivenTo(AUTCharacter* NewOwner, bool bAutoActivate);
	/** called when this inventory item has been removed from its owner */
	UFUNCTION(BlueprintImplementableEvent)
	void eventRemoved();
	virtual void Removed();

	/** client side handling of owner transition */
	UFUNCTION(Client, Reliable)
	void ClientGivenTo(bool bAutoActivate);
	virtual void ClientGivenTo_Internal(bool bAutoActivate);
	/** called only on the client that is given the item */
	UFUNCTION(BlueprintImplementableEvent)
	void eventClientGivenTo(bool bAutoActivate);
	UFUNCTION(Client, Reliable)
	virtual void ClientRemoved();
	UFUNCTION(BlueprintImplementableEvent)
	void eventClientRemoved();

	virtual void OnRep_Instigator();

	uint32 bPendingClientGivenTo : 1;
	uint32 bPendingAutoActivate : 1;

public:
	AUTInventory* GetNext() const
	{
		return NextInventory;
	}
	AUTCharacter* GetUTOwner() const
	{
		checkSlow(UTOwner == GetOwner());
		return UTOwner;
	}
	virtual void Destroyed();
};