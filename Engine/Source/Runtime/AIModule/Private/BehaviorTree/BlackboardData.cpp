// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/BlackboardData.h"

UBlackboardData::FKeyUpdate UBlackboardData::OnUpdateKeys;

static void UpdatePersistentKeys(UBlackboardData* Asset)
{
	UBlackboardKeyType_Object* SelfKeyType = Asset->UpdatePersistentKey<UBlackboardKeyType_Object>(FBlackboard::KeySelf);
	if (SelfKeyType)
	{
		SelfKeyType->BaseClass = AActor::StaticClass();
	}
}

bool FBlackboardEntry::operator==(const FBlackboardEntry& Other) const
{
	return (EntryName == Other.EntryName) &&
		((KeyType && Other.KeyType && KeyType->GetClass() == Other.KeyType->GetClass()) || (KeyType == NULL && Other.KeyType == NULL));
}

UBlackboardData::UBlackboardData(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		OnUpdateKeys.AddStatic(&UpdatePersistentKeys);
	}
}

FBlackboard::FKey UBlackboardData::GetKeyID(const FName& KeyName) const
{
	return InternalGetKeyID(KeyName, CheckParentKeys);
}

FName UBlackboardData::GetKeyName(FBlackboard::FKey KeyID) const
{
	const FBlackboardEntry* KeyEntry = GetKey(KeyID);
	return KeyEntry ? KeyEntry->EntryName : NAME_None;
}

TSubclassOf<UBlackboardKeyType> UBlackboardData::GetKeyType(FBlackboard::FKey KeyID) const
{
	const FBlackboardEntry* KeyEntry = GetKey(KeyID);
	return KeyEntry ? KeyEntry->KeyType->GetClass() : NULL;
}

bool UBlackboardData::IsKeyInstanceSynced(FBlackboard::FKey KeyID) const
{
	const FBlackboardEntry* KeyEntry = GetKey(KeyID);
	return KeyEntry ? KeyEntry->bInstanceSynced : false;
}

const FBlackboardEntry* UBlackboardData::GetKey(FBlackboard::FKey KeyID) const
{
	if (KeyID != FBlackboard::InvalidKey)
	{
		if (KeyID >= FirstKeyID)
		{
			return &Keys[KeyID - FirstKeyID];
		}
		else if (Parent)
		{
			return Parent->GetKey(KeyID);
		}
	}

	return NULL;
}

int32 UBlackboardData::GetNumKeys() const
{
	return FirstKeyID + Keys.Num();
}

FBlackboard::FKey UBlackboardData::InternalGetKeyID(const FName& KeyName, EKeyLookupMode LookupMode) const
{
	for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); KeyIndex++)
	{
		if (Keys[KeyIndex].EntryName == KeyName)
		{
			return KeyIndex + FirstKeyID;
		}
	}

	return Parent && (LookupMode == CheckParentKeys) ? Parent->InternalGetKeyID(KeyName, LookupMode) : FBlackboard::InvalidKey;
}

bool UBlackboardData::IsValid() const
{
	if (Parent)
	{
		for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); KeyIndex++)
		{
			const FBlackboard::FKey KeyID = Parent->InternalGetKeyID(Keys[KeyIndex].EntryName, CheckParentKeys);
			if (KeyID != FBlackboard::InvalidKey)
			{
				UE_LOG(LogBehaviorTree, Warning, TEXT("Blackboard asset (%s) has duplicated key (%s) in parent chain!"),
					*GetName(), *Keys[KeyIndex].EntryName.ToString());

				return false;
			}
		}
	}

	return true;
}

void UBlackboardData::UpdateIfHasSynchronizedKeys()
{
	bHasSynchronizedKeys = Parent != nullptr && Parent->bHasSynchronizedKeys;
	for (int32 KeyIndex = 0; KeyIndex < Keys.Num() && bHasSynchronizedKeys == false; ++KeyIndex)
	{
		bHasSynchronizedKeys |= Keys[KeyIndex].bInstanceSynced;
	}
}

void UBlackboardData::PostLoad()
{
	Super::PostLoad();

	UpdateParentKeys();
	UpdateIfHasSynchronizedKeys();
}

#if WITH_EDITOR
void UBlackboardData::PreEditChange(class FEditPropertyChain& PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);
}

void UBlackboardData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_Parent = GET_MEMBER_NAME_CHECKED(UBlackboardData, Parent);
	static const FName NAME_InstanceSynced = GET_MEMBER_NAME_CHECKED(FBlackboardEntry, bInstanceSynced);
	static const FName NAME_Keys = GET_MEMBER_NAME_CHECKED(UBlackboardData, Keys);

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		if (PropertyChangedEvent.Property->GetFName() == NAME_Parent)
		{
			// look for cycles
			if (Parent && Parent->HasParent(this))
			{
				UE_LOG(LogBehaviorTree, Warning, TEXT("Blackboard asset (%s) has (%s) in parent chain! Clearing value to avoid cycle."),
					*GetNameSafe(Parent), *GetNameSafe(this));

				Parent = NULL;
			}

			UpdateParentKeys();
		}

		if (PropertyChangedEvent.Property->GetFName() == NAME_InstanceSynced || PropertyChangedEvent.Property->GetFName() == NAME_Parent)
		{
			UpdateIfHasSynchronizedKeys();
		}
	}
	if (PropertyChangedEvent.MemberProperty)
	{
		if (PropertyChangedEvent.MemberProperty->GetFName() == NAME_Keys)
		{
			// look for BB assets using this one as a parent and update them as well
			PropagateKeyChangesToDerivedBlackboardAssets();
		}
	}
}

void UBlackboardData::PropagateKeyChangesToDerivedBlackboardAssets()
{
	for (TObjectIterator<UBlackboardData> It; It; ++It)
	{
		if (It->Parent == this)
		{
			It->UpdateParentKeys();
			It->UpdateIfHasSynchronizedKeys();
			It->PropagateKeyChangesToDerivedBlackboardAssets();
		}
	}
}

#endif // WITH_EDITOR

static bool ContainsKeyName(FName KeyName, const TArray<FBlackboardEntry>& Keys, const TArray<FBlackboardEntry>& ParentKeys)
{
	for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); KeyIndex++)
	{
		if (Keys[KeyIndex].EntryName == KeyName)
		{
			return true;
		}
	}

	for (int32 KeyIndex = 0; KeyIndex < ParentKeys.Num(); KeyIndex++)
	{
		if (ParentKeys[KeyIndex].EntryName == KeyName)
		{
			return true;
		}
	}

	return false;
}

bool UBlackboardData::HasParent(const UBlackboardData* TestParent) const
{
	if (Parent == TestParent)
	{
		return true;
	}

	return Parent ? Parent->HasParent(TestParent) : false;
}

void UBlackboardData::UpdateParentKeys()
{
	if (Parent == this)
	{
		Parent = NULL;
	}

#if WITH_EDITORONLY_DATA
	ParentKeys.Reset();

	for (UBlackboardData* It = Parent; It; It = It->Parent)
	{
		for (int32 KeyIndex = 0; KeyIndex < It->Keys.Num(); KeyIndex++)
		{
			const bool bAlreadyExist = ContainsKeyName(It->Keys[KeyIndex].EntryName, Keys, ParentKeys);
			if (!bAlreadyExist)
			{
				ParentKeys.Add(It->Keys[KeyIndex]);
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

	UpdateKeyIDs();
	OnUpdateKeys.Broadcast(this);
}

void UBlackboardData::UpdateKeyIDs()
{
	FirstKeyID = Parent ? Parent->GetNumKeys() : 0;
}

void UBlackboardData::UpdateDeprecatedKeys()
{
	for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); KeyIndex++)
	{
		FBlackboardEntry& Entry = Keys[KeyIndex];
		if (Entry.KeyType)
		{
			UBlackboardKeyType* UpdatedKey = Entry.KeyType->UpdateDeprecatedKey();
			if (UpdatedKey)
			{
				Entry.KeyType = UpdatedKey;
			}
		}
	}
}
