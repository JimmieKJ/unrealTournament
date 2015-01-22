// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EngineUtils.h"
#include "BehaviorTree/Blackboard/BlackboardKeyAllTypes.h"
#include "BehaviorTree/BlackboardComponent.h"

UBlackboardComponent::UBlackboardComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
	bPausedNotifies = false;
	bSynchronizedKeyPopulated = false;
}

void UBlackboardComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// cache blackboard component if owner has one
	// note that it's a valid scenario for this component to not have an owner at all (at least in terms of unittesting)
	AActor* Owner = GetOwner();
	if (Owner)
	{
		BrainComp = GetOwner()->FindComponentByClass<UBrainComponent>();
		if (BrainComp)
		{
			BrainComp->CacheBlackboardComponent(this);
		}
	}

	if (BlackboardAsset)
	{
		InitializeBlackboard(*BlackboardAsset);
	}
}

void UBlackboardComponent::UninitializeComponent()
{
	if (BlackboardAsset && BlackboardAsset->HasSyncronizedKeys())
	{
		UAISystem* AISystem = UAISystem::GetCurrentSafe(GetWorld());
		if (AISystem)
		{
			AISystem->UnregisterBlackboardComponent(*BlackboardAsset, *this);
		}
	}
	Super::UninitializeComponent();
}

void UBlackboardComponent::CacheBrainComponent(UBrainComponent& BrainComponent)
{
	if (&BrainComponent != BrainComp)
	{
		BrainComp = &BrainComponent;
	}
}

struct FBlackboardInitializationData
{
	FBlackboard::FKey KeyID;
	uint16 DataSize;

	FBlackboardInitializationData() {}
	FBlackboardInitializationData(FBlackboard::FKey InKeyID, uint16 InDataSize) : KeyID(InKeyID)
	{
		DataSize = (InDataSize <= 2) ? InDataSize : ((InDataSize + 3) & ~3);
	}

	struct FMemorySort
	{
		FORCEINLINE bool operator()(const FBlackboardInitializationData& A, const FBlackboardInitializationData& B) const
		{
			return A.DataSize > B.DataSize;
		}
	};
};

void UBlackboardComponent::InitializeParentChain(UBlackboardData* NewAsset)
{
	if (NewAsset)
	{
		InitializeParentChain(NewAsset->Parent);
		NewAsset->UpdateKeyIDs();
	}
}

bool UBlackboardComponent::InitializeBlackboard(UBlackboardData& NewAsset)
{
	// if we re-initialize with the same asset then there's no point
	// in reseting, since we'd loose all the accumulated knowledge
	if (&NewAsset == BlackboardAsset)
	{
		return false;
	}

	UAISystem* AISystem = UAISystem::GetCurrentSafe(GetWorld());
	if (AISystem == nullptr)
	{
		return false;
	}

	if (BlackboardAsset)
	{
		AISystem->UnregisterBlackboardComponent(*BlackboardAsset, *this);
	}

	BlackboardAsset = &NewAsset;
	ValueMemory.Reset();
	ValueOffsets.Reset();
	bSynchronizedKeyPopulated = false;

	bool bSuccess = true;
	
	if (BlackboardAsset->IsValid())
	{
		InitializeParentChain(BlackboardAsset);

		TArray<FBlackboardInitializationData> InitList;
		const int32 NumKeys = BlackboardAsset->GetNumKeys();
		InitList.Reserve(NumKeys);
		ValueOffsets.AddZeroed(NumKeys);

		for (UBlackboardData* It = BlackboardAsset; It; It = It->Parent)
		{
			for (int32 KeyIndex = 0; KeyIndex < It->Keys.Num(); KeyIndex++)
			{
				if (It->Keys[KeyIndex].KeyType)
				{
					InitList.Add(FBlackboardInitializationData(KeyIndex + It->GetFirstKeyID(), It->Keys[KeyIndex].KeyType->GetValueSize()));
				}
			}
		}

		// sort key values by memory size, so they can be packed better
		// it still won't protect against structures, that are internally misaligned (-> uint8, uint32)
		// but since all Engine level keys are good... 
		InitList.Sort(FBlackboardInitializationData::FMemorySort());
		uint16 MemoryOffset = 0;
		for (int32 Index = 0; Index < InitList.Num(); Index++)
		{
			ValueOffsets[InitList[Index].KeyID] = MemoryOffset;
			MemoryOffset += InitList[Index].DataSize;
		}

		ValueMemory.AddZeroed(MemoryOffset);

		// initialize memory
		for (int32 Index = 0; Index < InitList.Num(); Index++)
		{
			const FBlackboardEntry* KeyData = BlackboardAsset->GetKey(InitList[Index].KeyID);
			uint8* RawData = GetKeyRawData(InitList[Index].KeyID);

			KeyData->KeyType->Initialize(RawData);
		}
	
		// naive initial synchronization with one of already instantiated blackboards using the same BB asset
		if (BlackboardAsset->HasSyncronizedKeys())
		{
			PopulateSynchronizedKeys();
		}
	}
	else
	{
		bSuccess = false;
		UE_LOG(LogBehaviorTree, Error, TEXT("Blackboard asset (%s) has errors and can't be used!"), *GetNameSafe(BlackboardAsset));
	}

	return bSuccess;
}

void UBlackboardComponent::PopulateSynchronizedKeys()
{
	ensure(bSynchronizedKeyPopulated == false);
	
	UAISystem* AISystem = UAISystem::GetCurrentSafe(GetWorld());
	check(AISystem);
	AISystem->RegisterBlackboardComponent(*BlackboardAsset, *this);

	for (auto Iter = AISystem->CreateBlackboardDataToComponentsIterator(*BlackboardAsset); Iter; ++Iter)
	{
		UBlackboardComponent* OtherBlackboard = Iter.Value();
		if (OtherBlackboard != nullptr && ShouldSyncWithBlackboard(*OtherBlackboard))
		{
			for (const auto& Key : BlackboardAsset->Keys)
			{
				if (Key.bInstanceSynced)
				{
					const int32 KeyID = BlackboardAsset->GetKeyID(Key.EntryName);
					check(BlackboardAsset->GetKeyID(Key.EntryName) == OtherBlackboard->BlackboardAsset->GetKeyID(Key.EntryName));
					check(BlackboardAsset->GetKeyType(KeyID) == OtherBlackboard->BlackboardAsset->GetKeyType(KeyID));

					const TSubclassOf<UBlackboardKeyType> ValueType = BlackboardAsset->GetKeyType(KeyID);
					const UBlackboardKeyType* ValueTypeInstance = GetDefault<UBlackboardKeyType>(*ValueType);
					ValueTypeInstance->CopyValue(GetKeyRawData(KeyID), OtherBlackboard->GetKeyRawData(KeyID));
				}
			}
			break;
		}
	}

	bSynchronizedKeyPopulated = true;
}

bool UBlackboardComponent::ShouldSyncWithBlackboard(UBlackboardComponent& OtherBlackboardComponent) const
{
	return &OtherBlackboardComponent != this && (
		(BrainComp == nullptr || (BrainComp->GetAIOwner() != nullptr && BrainComp->GetAIOwner()->ShouldSyncBlackboardWith(OtherBlackboardComponent) == true))
		|| (OtherBlackboardComponent.BrainComp == nullptr || (OtherBlackboardComponent.BrainComp->GetAIOwner() != nullptr && OtherBlackboardComponent.BrainComp->GetAIOwner()->ShouldSyncBlackboardWith(OtherBlackboardComponent) == true)));
}

UBrainComponent* UBlackboardComponent::GetBrainComponent() const
{
	return BrainComp;
}

UBlackboardData* UBlackboardComponent::GetBlackboardAsset() const
{
	return BlackboardAsset;
}

FName UBlackboardComponent::GetKeyName(FBlackboard::FKey KeyID) const
{
	return BlackboardAsset ? BlackboardAsset->GetKeyName(KeyID) : NAME_None;
}

FBlackboard::FKey UBlackboardComponent::GetKeyID(const FName& KeyName) const
{
	return BlackboardAsset ? BlackboardAsset->GetKeyID(KeyName) : FBlackboard::InvalidKey;
}

TSubclassOf<UBlackboardKeyType> UBlackboardComponent::GetKeyType(FBlackboard::FKey KeyID) const
{
	return BlackboardAsset ? BlackboardAsset->GetKeyType(KeyID) : NULL;
}

bool UBlackboardComponent::IsKeyInstanceSynced(FBlackboard::FKey KeyID) const
{
	return BlackboardAsset ? BlackboardAsset->IsKeyInstanceSynced(KeyID) : false;
}

int32 UBlackboardComponent::GetNumKeys() const
{
	return BlackboardAsset ? BlackboardAsset->GetNumKeys() : 0;
}

void UBlackboardComponent::RegisterObserver(FBlackboard::FKey KeyID, FOnBlackboardChange ObserverDelegate)
{
	Observers.AddUnique(KeyID, ObserverDelegate);
}

void UBlackboardComponent::UnregisterObserver(FBlackboard::FKey KeyID, FOnBlackboardChange ObserverDelegate)
{
	Observers.RemoveSingle(KeyID, ObserverDelegate);
}

void UBlackboardComponent::PauseUpdates()
{
	bPausedNotifies = true;
}

void UBlackboardComponent::ResumeUpdates()
{
	bPausedNotifies = false;

	for (int32 UpdateIndex = 0; UpdateIndex < QueuedUpdates.Num(); UpdateIndex++)
	{
		NotifyObservers(QueuedUpdates[UpdateIndex]);
	}

	QueuedUpdates.Empty();
}

void UBlackboardComponent::NotifyObservers(FBlackboard::FKey KeyID) const
{
	if (bPausedNotifies)
	{
		QueuedUpdates.AddUnique(KeyID);
	}
	else
	{
		for (TMultiMap<uint8, FOnBlackboardChange>::TConstKeyIterator KeyIt(Observers, KeyID); KeyIt; ++KeyIt)
		{
			const FOnBlackboardChange& ObserverDelegate = KeyIt.Value();
			ObserverDelegate.ExecuteIfBound(*this, KeyID);
		}
	}
}

bool UBlackboardComponent::IsCompatibleWith(UBlackboardData* TestAsset) const
{
	for (UBlackboardData* It = BlackboardAsset; It; It = It->Parent)
	{
		if (It == TestAsset)
		{
			return true;
		}

		if (It->Keys == TestAsset->Keys)
		{
			return true;
		}
	}

	return false;
}

EBlackboardCompare::Type UBlackboardComponent::CompareKeyValues(TSubclassOf<UBlackboardKeyType> KeyType, FBlackboard::FKey KeyA, FBlackboard::FKey KeyB) const
{	
	return GetDefault<UBlackboardKeyType>()->Compare(GetKeyRawData(KeyA), GetKeyRawData(KeyB));
}

FString UBlackboardComponent::GetDebugInfoString(EBlackboardDescription::Type Mode) const 
{
	FString DebugString = FString::Printf(TEXT("Blackboard (asset: %s)\n"), *GetNameSafe(BlackboardAsset));

	TArray<FString> KeyDesc;
	uint8 Offset = 0;
	for (UBlackboardData* It = BlackboardAsset; It; It = It->Parent)
	{
		for (int32 KeyIndex = 0; KeyIndex < It->Keys.Num(); KeyIndex++)
		{
			KeyDesc.Add(DescribeKeyValue(KeyIndex + Offset, Mode));
		}
		Offset += It->Keys.Num();
	}
	
	KeyDesc.Sort();
	for (int32 KeyDescIndex = 0; KeyDescIndex < KeyDesc.Num(); KeyDescIndex++)
	{
		DebugString += TEXT("  ");
		DebugString += KeyDesc[KeyDescIndex];
		DebugString += TEXT('\n');
	}

	if (Mode == EBlackboardDescription::Full && BlackboardAsset)
	{
		DebugString += TEXT("Observed Keys:\n");

		TArray<uint8> ObserversKeys;
		if (Observers.GetKeys(ObserversKeys) > 0)
		{
			DebugString += TEXT("Observed Keys:\n");
		
			for (int32 KeyIndex = 0; KeyIndex < ObserversKeys.Num(); ++KeyIndex)
			{
				const FBlackboard::FKey KeyID = ObserversKeys[KeyIndex];
				//@todo shouldn't be using a localized value?; GetKeyName() [10/11/2013 justin.sargent]
				DebugString += FString::Printf(TEXT("  %s:\n"), *BlackboardAsset->GetKeyName(KeyID).ToString());
			}
		}
		else
		{
			DebugString += TEXT("  NONE\n");
		}
	}

	return DebugString;
}

FString UBlackboardComponent::DescribeKeyValue(const FName& KeyName, EBlackboardDescription::Type Mode) const
{
	return DescribeKeyValue(GetKeyID(KeyName), Mode);
}

FString UBlackboardComponent::DescribeKeyValue(FBlackboard::FKey KeyID, EBlackboardDescription::Type Mode) const
{
	FString Description;

	const FBlackboardEntry* Key = BlackboardAsset ? BlackboardAsset->GetKey(KeyID) : NULL;
	if (Key)
	{
		const uint8* ValueData = GetKeyRawData(KeyID);
		FString ValueDesc = Key->KeyType ? *(Key->KeyType->DescribeValue(ValueData)) : TEXT("empty");

		if (Mode == EBlackboardDescription::OnlyValue)
		{
			Description = ValueDesc;
		}
		else if (Mode == EBlackboardDescription::KeyWithValue)
		{
			Description = FString::Printf(TEXT("%s: %s"), *(Key->EntryName.ToString()), *ValueDesc);
		}
		else
		{
			const FString CommonTypePrefix = UBlackboardKeyType::StaticClass()->GetName().AppendChar(TEXT('_'));
			const FString FullKeyType = Key->KeyType ? GetNameSafe(Key->KeyType->GetClass()) : FString();
			const FString DescKeyType = FullKeyType.StartsWith(CommonTypePrefix) ? FullKeyType.RightChop(CommonTypePrefix.Len()) : FullKeyType;

			Description = FString::Printf(TEXT("%s [%s]: %s"), *(Key->EntryName.ToString()), *DescKeyType, *ValueDesc);
		}
	}

	return Description;
}

#if ENABLE_VISUAL_LOG

void UBlackboardComponent::DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const
{
	FVisualLogStatusCategory Category;
	Category.Category = FString::Printf(TEXT("Blackboard (asset: %s)"), *GetNameSafe(BlackboardAsset));

	for (UBlackboardData* It = BlackboardAsset; It; It = It->Parent)
	{
		for (int32 KeyIndex = 0; KeyIndex < It->Keys.Num(); KeyIndex++)
		{
			const FBlackboardEntry& Key = It->Keys[KeyIndex];

			const uint8* ValueData = GetKeyRawData(It->GetFirstKeyID() + KeyIndex);
			FString ValueDesc = Key.KeyType ? *(Key.KeyType->DescribeValue(ValueData)) : TEXT("empty");

			Category.Add(Key.EntryName.ToString(), ValueDesc);
		}
	}

	Category.Data.Sort();
	Snapshot->Status.Add(Category);
}

#endif

UObject* UBlackboardComponent::GetValueAsObject(const FName& KeyName) const
{
	return GetValue<UBlackboardKeyType_Object>(KeyName);
}

UClass* UBlackboardComponent::GetValueAsClass(const FName& KeyName) const
{
	return GetValue<UBlackboardKeyType_Class>(KeyName);
}

uint8 UBlackboardComponent::GetValueAsEnum(const FName& KeyName) const
{
	FBlackboard::FKey KeyID = GetKeyID(KeyName);
	if (GetKeyType(KeyID) != UBlackboardKeyType_Enum::StaticClass() &&
		GetKeyType(KeyID) != UBlackboardKeyType_NativeEnum::StaticClass())
	{
		return UBlackboardKeyType_Enum::InvalidValue;
	}

	return GetValue<UBlackboardKeyType_Enum>(KeyName);
}

int32 UBlackboardComponent::GetValueAsInt(const FName& KeyName) const
{
	return GetValue<UBlackboardKeyType_Int>(KeyName);
}

float UBlackboardComponent::GetValueAsFloat(const FName& KeyName) const
{
	return GetValue<UBlackboardKeyType_Float>(KeyName);
}

bool UBlackboardComponent::GetValueAsBool(const FName& KeyName) const
{
	return GetValue<UBlackboardKeyType_Bool>(KeyName);
}

FString UBlackboardComponent::GetValueAsString(const FName& KeyName) const
{
	return GetValue<UBlackboardKeyType_String>(KeyName);
}

FName UBlackboardComponent::GetValueAsName(const FName& KeyName) const
{
	return GetValue<UBlackboardKeyType_Name>(KeyName);
}

FVector UBlackboardComponent::GetValueAsVector(const FName& KeyName) const
{
	return GetValue<UBlackboardKeyType_Vector>(KeyName);
}

FRotator UBlackboardComponent::GetValueAsRotator(const FName& KeyName) const
{
	return GetValue<UBlackboardKeyType_Rotator>(KeyName);
}

void UBlackboardComponent::SetValueAsObject(const FName& KeyName, UObject* ObjectValue)
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	SetValue<UBlackboardKeyType_Object>(KeyID, ObjectValue);
}

void UBlackboardComponent::SetValueAsClass(const FName& KeyName, UClass* ClassValue)
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	SetValue<UBlackboardKeyType_Class>(KeyID, ClassValue);
}

void UBlackboardComponent::SetValueAsEnum(const FName& KeyName, uint8 EnumValue)
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	SetValue<UBlackboardKeyType_Enum>(KeyID, EnumValue);
}

void UBlackboardComponent::SetValueAsInt(const FName& KeyName, int32 IntValue)
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	SetValue<UBlackboardKeyType_Int>(KeyID, IntValue);
}

void UBlackboardComponent::SetValueAsFloat(const FName& KeyName, float FloatValue)
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	SetValue<UBlackboardKeyType_Float>(KeyID, FloatValue);
}

void UBlackboardComponent::SetValueAsBool(const FName& KeyName, bool BoolValue)
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	SetValue<UBlackboardKeyType_Bool>(KeyID, BoolValue);
}

void UBlackboardComponent::SetValueAsString(const FName& KeyName, FString StringValue)
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	SetValue<UBlackboardKeyType_String>(KeyID, StringValue);
}

void UBlackboardComponent::SetValueAsName(const FName& KeyName, FName NameValue)
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	SetValue<UBlackboardKeyType_Name>(KeyID, NameValue);
}

void UBlackboardComponent::SetValueAsVector(const FName& KeyName, FVector VectorValue)
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	SetValue<UBlackboardKeyType_Vector>(KeyID, VectorValue);
}

void UBlackboardComponent::SetValueAsRotator(const FName& KeyName, FRotator RotatorValue)
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	SetValue<UBlackboardKeyType_Rotator>(KeyID, RotatorValue);
}

void UBlackboardComponent::ClearValueAsVector(const FName& KeyName)
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	ClearValue(KeyID);
}

void UBlackboardComponent::ClearValueAsVector(FBlackboard::FKey KeyID)
{
	ensure(false && "This function is deprecated. Use more generic ClearValue() instead");
	ClearValue(KeyID);
}

bool UBlackboardComponent::IsVectorValueSet(const FName& KeyName) const
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	return IsVectorValueSet(KeyID);
}

bool UBlackboardComponent::IsVectorValueSet(FBlackboard::FKey KeyID) const
{
	FVector VectorValue = GetValue<UBlackboardKeyType_Vector>(KeyID);
	return (VectorValue != FAISystem::InvalidLocation);
}

void UBlackboardComponent::ClearValueAsRotator(const FName& KeyName)
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	ClearValue(KeyID);
}

void UBlackboardComponent::ClearValueAsRotator(uint8 KeyID)
{
	ensure(false && "This function is deprecated. Use more generic ClearValue() instead");
	ClearValue(KeyID);
}

void UBlackboardComponent::ClearValue(const FName& KeyName)
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	ClearValue(KeyID);
}

void UBlackboardComponent::ClearValue(FBlackboard::FKey KeyID)
{
	check(BlackboardAsset);

	uint8* RawData = GetKeyRawData(KeyID);
	if (RawData)
	{
		TSubclassOf<UBlackboardKeyType> ValueType = GetKeyType(KeyID);
		check(ValueType);
		
		const UBlackboardKeyType* ValueTypeInstance = GetDefault<UBlackboardKeyType>(*ValueType);

		const bool bChanged = ValueTypeInstance->Clear(RawData);
		if (bChanged)
		{
			NotifyObservers(KeyID);

			if (BlackboardAsset->HasSyncronizedKeys() && IsKeyInstanceSynced(KeyID))
			{
				// grab the value set and apply the same to synchronized keys
				// to avoid virtual function call overhead
				UAISystem* AISystem = UAISystem::GetCurrentSafe(GetWorld());
				for (auto Iter = AISystem->CreateBlackboardDataToComponentsIterator(*BlackboardAsset); Iter; ++Iter)
				{
					UBlackboardComponent* OtherBlackboard = Iter.Value();
					if (OtherBlackboard != nullptr && ShouldSyncWithBlackboard(*OtherBlackboard))
					{
						ValueTypeInstance->CopyValue(OtherBlackboard->GetKeyRawData(KeyID), RawData);
						OtherBlackboard->NotifyObservers(KeyID);
					}
				}
			}
		}
	}
}

bool UBlackboardComponent::GetLocationFromEntry(const FName& KeyName, FVector& ResultLocation) const
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	return GetLocationFromEntry(KeyID, ResultLocation);
}

bool UBlackboardComponent::GetLocationFromEntry(FBlackboard::FKey KeyID, FVector& ResultLocation) const
{
	if (BlackboardAsset && ValueOffsets.IsValidIndex(KeyID))
	{
		const FBlackboardEntry* EntryInfo = BlackboardAsset->GetKey(KeyID);
		if (EntryInfo && EntryInfo->KeyType)
		{
			const uint8* ValueData = ValueMemory.GetData() + ValueOffsets[KeyID];
			return EntryInfo->KeyType->GetLocation(ValueData, ResultLocation);
		}
	}

	return false;
}

bool UBlackboardComponent::GetRotationFromEntry(const FName& KeyName, FRotator& ResultRotation) const
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	return GetRotationFromEntry(KeyID, ResultRotation);
}

bool UBlackboardComponent::GetRotationFromEntry(FBlackboard::FKey KeyID, FRotator& ResultRotation) const
{
	if (BlackboardAsset && ValueOffsets.IsValidIndex(KeyID))
	{
		const FBlackboardEntry* EntryInfo = BlackboardAsset->GetKey(KeyID);
		if (EntryInfo && EntryInfo->KeyType)
		{
			const uint8* ValueData = ValueMemory.GetData() + ValueOffsets[KeyID];
			return EntryInfo->KeyType->GetRotation(ValueData, ResultRotation);
		}
	}

	return false;
}

//----------------------------------------------------------------------//
// Deprecated
//----------------------------------------------------------------------//
void UBlackboardComponent::SetValueAsObject(FBlackboard::FKey KeyID, UObject* ObjectValue)
{
	SetValue<UBlackboardKeyType_Object>(KeyID, ObjectValue);
}

void UBlackboardComponent::SetValueAsClass(FBlackboard::FKey KeyID, UClass* ClassValue)
{
	SetValue<UBlackboardKeyType_Class>(KeyID, ClassValue);
}

void UBlackboardComponent::SetValueAsEnum(FBlackboard::FKey KeyID, uint8 EnumValue)
{
	SetValue<UBlackboardKeyType_Enum>(KeyID, EnumValue);
}

void UBlackboardComponent::SetValueAsInt(FBlackboard::FKey KeyID, int32 IntValue)
{
	SetValue<UBlackboardKeyType_Int>(KeyID, IntValue);
}

void UBlackboardComponent::SetValueAsFloat(FBlackboard::FKey KeyID, float FloatValue)
{
	SetValue<UBlackboardKeyType_Float>(KeyID, FloatValue);
}

void UBlackboardComponent::SetValueAsBool(FBlackboard::FKey KeyID, bool BoolValue)
{
	SetValue<UBlackboardKeyType_Bool>(KeyID, BoolValue);
}

void UBlackboardComponent::SetValueAsString(FBlackboard::FKey KeyID, FString StringValue)
{
	SetValue<UBlackboardKeyType_String>(KeyID, StringValue);
}

void UBlackboardComponent::SetValueAsName(FBlackboard::FKey KeyID, FName NameValue)
{
	SetValue<UBlackboardKeyType_Name>(KeyID, NameValue);
}

void UBlackboardComponent::SetValueAsVector(FBlackboard::FKey KeyID, FVector VectorValue)
{
	SetValue<UBlackboardKeyType_Vector>(KeyID, VectorValue);
}

UObject* UBlackboardComponent::GetValueAsObject(FBlackboard::FKey KeyID) const
{
	return GetValue<UBlackboardKeyType_Object>(KeyID);
}

UClass* UBlackboardComponent::GetValueAsClass(FBlackboard::FKey KeyID) const
{
	return GetValue<UBlackboardKeyType_Class>(KeyID);
}

uint8 UBlackboardComponent::GetValueAsEnum(FBlackboard::FKey KeyID) const
{
	return GetValue<UBlackboardKeyType_Enum>(KeyID);
}

int32 UBlackboardComponent::GetValueAsInt(FBlackboard::FKey KeyID) const
{
	return GetValue<UBlackboardKeyType_Int>(KeyID);
}

float UBlackboardComponent::GetValueAsFloat(FBlackboard::FKey KeyID) const
{
	return GetValue<UBlackboardKeyType_Float>(KeyID);
}

bool UBlackboardComponent::GetValueAsBool(FBlackboard::FKey KeyID) const
{
	return GetValue<UBlackboardKeyType_Bool>(KeyID);
}

FString UBlackboardComponent::GetValueAsString(FBlackboard::FKey KeyID) const
{
	return GetValue<UBlackboardKeyType_String>(KeyID);
}

FName UBlackboardComponent::GetValueAsName(FBlackboard::FKey KeyID) const
{
	return GetValue<UBlackboardKeyType_Name>(KeyID);
}

FVector UBlackboardComponent::GetValueAsVector(FBlackboard::FKey KeyID) const
{
	return GetValue<UBlackboardKeyType_Vector>(KeyID);
}

FRotator UBlackboardComponent::GetValueAsRotator(FBlackboard::FKey KeyID) const
{
	return GetValue<UBlackboardKeyType_Rotator>(KeyID);
}
