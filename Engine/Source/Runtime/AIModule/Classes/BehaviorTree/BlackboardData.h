// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/DataAsset.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BlackboardData.generated.h"

class UBlackboardData;

/** blackboard entry definition */
USTRUCT()
struct FBlackboardEntry
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Blackboard)
	FName EntryName;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category=Blackboard, Meta=(ToolTip="Optional description to explain what this blackboard entry does."))
	FString EntryDescription;
#endif // WITH_EDITORONLY_DATA

	/** key type and additional properties */
	UPROPERTY(EditAnywhere, Instanced, Category=Blackboard)
	UBlackboardKeyType* KeyType;

	/** if set to true then this field will be synchronized across all instances of this blackboard */
	UPROPERTY(EditAnywhere, Category = Blackboard)
	uint32 bInstanceSynced : 1;

	bool operator==(const FBlackboardEntry& Other) const;
};

UCLASS(AutoExpandCategories=(Blackboard))
class AIMODULE_API UBlackboardData : public UDataAsset
{
	GENERATED_UCLASS_BODY()
	DECLARE_MULTICAST_DELEGATE_OneParam(FKeyUpdate, UBlackboardData* /*asset*/);

	/** parent blackboard (keys can be overridden) */
	UPROPERTY(EditAnywhere, Category=Parent)
	UBlackboardData* Parent;

#if WITH_EDITORONLY_DATA
	/** all keys inherited from parent chain */
	UPROPERTY(VisibleDefaultsOnly, Transient, Category=Parent)
	TArray<FBlackboardEntry> ParentKeys;
#endif

	/** blackboard keys */
	UPROPERTY(EditAnywhere, Category=Blackboard)
	TArray<FBlackboardEntry> Keys;

private:
	UPROPERTY()
	uint32 bHasSynchronizedKeys : 1;

public:

	FORCEINLINE bool HasSynchronizedKeys() const { return bHasSynchronizedKeys; }

	/** @return true if the key is instance synced */
	bool IsKeyInstanceSynced(FBlackboard::FKey KeyID) const;
	
	/** @return key ID from name */
	FBlackboard::FKey GetKeyID(const FName& KeyName) const;

	/** @return name of key */
	FName GetKeyName(FBlackboard::FKey KeyID) const;

	/** @return class of value for given key */
	TSubclassOf<UBlackboardKeyType> GetKeyType(FBlackboard::FKey KeyID) const;

	/** @return number of defined keys, including parent chain */
	int32 GetNumKeys() const;

	FORCEINLINE FBlackboard::FKey GetFirstKeyID() const { return FirstKeyID; }

	/** @return key data */
	const FBlackboardEntry* GetKey(FBlackboard::FKey KeyID) const;

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PreEditChange(class FEditPropertyChain& PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	void PropagateKeyChangesToDerivedBlackboardAssets();
#endif

	/** @return true if blackboard keys are not conflicting with parent key chain */
	bool IsValid() const;

	/** updates persistent key with given name, depending on currently defined entries and parent chain
	 *  @return key type of newly created entry for further setup
	 */
	template<class T>
	T* UpdatePersistentKey(const FName& KeyName)
	{
		T* CreatedKeyType = NULL;

		const FBlackboard::FKey KeyID = InternalGetKeyID(KeyName, DontCheckParentKeys);
		if (KeyID == FBlackboard::InvalidKey && Parent == NULL)
		{
			FBlackboardEntry Entry;
			Entry.EntryName = KeyName;

			CreatedKeyType = NewObject<T>(this);
			Entry.KeyType = CreatedKeyType;		

			Keys.Add(Entry);
			MarkPackageDirty();
		}
		else if (KeyID != FBlackboard::InvalidKey && Parent != NULL)
		{
			const FBlackboard::FKey KeyIndex = KeyID - FirstKeyID;
			Keys.RemoveAt(KeyIndex);
			MarkPackageDirty();
		}

		return CreatedKeyType;
	}

	/** delegate called for every loaded blackboard asset
	 *  meant for adding game specific persistent keys */
	static FKeyUpdate OnUpdateKeys;

	/** updates parent key cache for editor */
	void UpdateParentKeys();

	/** forces update of FirstKeyID, which depends on parent chain */
	void UpdateKeyIDs();

	void UpdateIfHasSynchronizedKeys();

	/** fix entries with deprecated key types */
	void UpdateDeprecatedKeys();

protected:

	enum EKeyLookupMode
	{
		CheckParentKeys,
		DontCheckParentKeys,
	};

	/** @return first ID for keys of this asset (parent keys goes first) */
	FBlackboard::FKey FirstKeyID;

	/** @return key ID from name */
	FBlackboard::FKey InternalGetKeyID(const FName& KeyName, EKeyLookupMode LookupMode) const;

	/** check if parent chain contains given blackboard data */
	bool HasParent(const UBlackboardData* TestParent) const;
};
