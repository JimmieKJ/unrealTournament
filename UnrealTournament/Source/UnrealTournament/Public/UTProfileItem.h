// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProfileItem.generated.h"

USTRUCT()
struct FProfileItemEntry
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class UUTProfileItem* Item;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Count;

	FProfileItemEntry()
		: Item(NULL), Count(0)
	{}
	FProfileItemEntry(UUTProfileItem* InItem, uint32 InCount)
	: Item(InItem), Count(int32(FMath::Min<uint32>(InCount, MAX_int32)))
	{}
};

/** collectable/consumable item that a player owns in their profile, such as a hat */
UCLASS()
class UUTProfileItem : public UDataAsset
{
	GENERATED_BODY()
public:

	UUTProfileItem(const FObjectInitializer& OI)
	: Super(OI)
	{
		bTradable = true;
	}

	/** whether this item can be traded to others */
	UPROPERTY(EditAnywhere)
	bool bTradable;
	/** if true, player can only have one of this item */
	UPROPERTY(EditAnywhere)
	bool bUnique;
	/** human readable name */
	UPROPERTY(EditAnywhere)
	FText DisplayName;
	/** art for UI */
	UPROPERTY(EditAnywhere)
	FCanvasIcon Image;
	/** player can construct this item by combining these other items */
	UPROPERTY(EditAnywhere)
	TArray<FProfileItemEntry> Recipe;
	
	/** when this item is in the player's inventory, they have access to these cosmetic items */
	UPROPERTY(EditAnywhere, Meta = (MetaClass = "UTCosmetic"))
	TArray<FStringClassReference> GrantedCosmetics;
	/** when this item is in the player's inventory, they have access to these player characters */
	UPROPERTY(EditAnywhere, Meta = (MetaClass = "UTCharacterContent"))
	TArray<FStringClassReference> GrantedCharacters;
	/** when this item is in the player's inventory, they have access to these BOT characters */
	UPROPERTY(EditAnywhere, Meta = (AllowedClasses = "UTBotCharacter"))
	TArray<FStringAssetReference> GrantedBots;

	/** returns whether this item grants access to the object with the specified path */
	inline bool Grants(const FString& Path) const
	{
		FString BPClassPath = Path + TEXT("_C");
		return GrantedCosmetics.ContainsByPredicate([&](const FStringClassReference& TestItem) { return TestItem.ToString() == Path || TestItem.ToString() == BPClassPath; }) ||
			GrantedCharacters.ContainsByPredicate([&](const FStringClassReference& TestItem) { return TestItem.ToString() == Path || TestItem.ToString() == BPClassPath; }) ||
			GrantedBots.ContainsByPredicate([&](const FStringAssetReference& TestItem) { return TestItem.ToString().Contains(Path); });
	}
};