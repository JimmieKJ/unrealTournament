// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "HAL/OutputDevices.h"
#include "AbilitySystemGlobals.h"
#include "VisualLogger.h"

#include "ComponentReregisterContext.h"

FGameplayAttribute::FGameplayAttribute(UProperty *NewProperty)
{
	// Only numeric properties are allowed right now
	Attribute = Cast<UNumericProperty>(NewProperty);
}

void FGameplayAttribute::SetNumericValueChecked(float NewValue, class UAttributeSet* Dest) const
{
	UNumericProperty *NumericProperty = CastChecked<UNumericProperty>(Attribute);
	void* ValuePtr = NumericProperty->ContainerPtrToValuePtr<void>(Dest);
	float OldValue = *static_cast<float*>(ValuePtr);
	Dest->PreAttributeChange(*this, NewValue);
	NumericProperty->SetFloatingPointPropertyValue(ValuePtr, NewValue);

#if ENABLE_VISUAL_LOG
	// draw a graph of the changes to the attribute in the visual logger
	AActor* OwnerActor = Dest->GetOwningAbilitySystemComponent()->OwnerActor;
	if (OwnerActor)
	{
		ABILITY_VLOG_ATTRIBUTE_GRAPH(OwnerActor, Log, GetName(), OldValue, NewValue);
	}
#endif
}

float FGameplayAttribute::GetNumericValue(const UAttributeSet* Src) const
{
	const UNumericProperty* const NumericPropertyOrNull = Cast<UNumericProperty>(Attribute);
	if (!NumericPropertyOrNull)
	{
		return 0.f;
	}

	const void* ValuePtr = NumericPropertyOrNull->ContainerPtrToValuePtr<void>(Src);
	return NumericPropertyOrNull->GetFloatingPointPropertyValue(ValuePtr);
}

float FGameplayAttribute::GetNumericValueChecked(const UAttributeSet* Src) const
{
	UNumericProperty* NumericProperty = CastChecked<UNumericProperty>(Attribute);
	const void* ValuePtr = NumericProperty->ContainerPtrToValuePtr<void>(Src);
	return NumericProperty->GetFloatingPointPropertyValue(ValuePtr);
}

bool FGameplayAttribute::IsSystemAttribute() const
{
	return GetAttributeSetClass()->IsChildOf(UAbilitySystemComponent::StaticClass());
}

UAttributeSet::UAttributeSet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool UAttributeSet::IsNameStableForNetworking() const
{
	/** 
	 * IsNameStableForNetworking means an attribute set can be referred to its path name (relative to owning AActor*) over the network
	 *
	 * Attribute sets are net addressable if:
	 *	-They are Default Subobjects (created in C++ constructor)
	 *	-They were loaded directly from a package (placed in map actors)
	 *	-They were explicitly set to bNetAddressable
	 */

	return bNetAddressable || Super::IsNameStableForNetworking();
}

void UAttributeSet::SetNetAddressable()
{
	bNetAddressable = true;
}

void UAttributeSet::InitFromMetaDataTable(const UDataTable* DataTable)
{
	static const FString Context = FString(TEXT("UAttribute::BindToMetaDataTable"));

	for( TFieldIterator<UProperty> It(GetClass(), EFieldIteratorFlags::IncludeSuper) ; It ; ++It )
	{
		UProperty* Property = *It;
		UNumericProperty *NumericProperty = Cast<UNumericProperty>(Property);
		if (NumericProperty)
		{
			FString RowNameStr = FString::Printf(TEXT("%s.%s"), *Property->GetOuter()->GetName(), *Property->GetName());
		
			FAttributeMetaData * MetaData = DataTable->FindRow<FAttributeMetaData>(FName(*RowNameStr), Context, false);
			if (MetaData)
			{
				void *Data = NumericProperty->ContainerPtrToValuePtr<void>(this);
				NumericProperty->SetFloatingPointPropertyValue(Data, MetaData->BaseValue);
			}
		}
	}

	PrintDebug();
}

UAbilitySystemComponent* UAttributeSet::GetOwningAbilitySystemComponent() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CastChecked<AActor>(GetOuter()));
}

FGameplayAbilityActorInfo* UAttributeSet::GetActorInfo() const
{
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (ASC)
	{
		return ASC->AbilityActorInfo.Get();
	}

	return nullptr;
}

void UAttributeSet::PrintDebug()
{
	
}

void UAttributeSet::PreNetReceive()
{
	// During the scope of this entire actor's network update, we need to lock our attribute aggregators.
	FScopedAggregatorOnDirtyBatch::BeginNetReceiveLock();
}
	
void UAttributeSet::PostNetReceive()
{
	// Once we are done receiving properties, we can unlock the attribute aggregators and flag them that the 
	// current property values are from the server.
	FScopedAggregatorOnDirtyBatch::EndNetReceiveLock();
}

FAttributeMetaData::FAttributeMetaData()
	: MinValue(0.f)
	, MaxValue(1.f)
{

}

float FScalableFloat::GetValueAtLevel(float Level) const
{
	if (Curve.CurveTable != nullptr)
	{
		if (FinalCurve == nullptr)
		{
			static const FString ContextString = TEXT("FScalableFloat::GetValueAtLevel");
			FinalCurve = Curve.GetCurve(ContextString);

			RegisterOnCurveTablePostReimport();
		}

		if (FinalCurve != nullptr)
		{
			return Value * FinalCurve->Eval(Level);
		}
		else
		{
			ABILITY_LOG(Error, TEXT("Unable to find RowName: %s for FScalableFloat."), *Curve.RowName.ToString());
		}
	}

	return Value;
}

void FScalableFloat::SetValue(float NewValue)
{
	UnRegisterOnCurveTablePostReimport();

	Value = NewValue;
	Curve.CurveTable = nullptr;
	Curve.RowName = NAME_None;
	FinalCurve = nullptr;
}

void FScalableFloat::SetScalingValue(float InCoeffecient, FName InRowName, UCurveTable * InTable)
{
	UnRegisterOnCurveTablePostReimport();

	Value = InCoeffecient;
	Curve.RowName = InRowName;
	Curve.CurveTable = InTable;
	FinalCurve = nullptr;
}

void FScalableFloat::RegisterOnCurveTablePostReimport() const
{
#if WITH_EDITOR
	if (!OnCurveTablePostReimportHandle.IsValid())
	{
		// Register our interest in knowing when our referenced curve table is changed, so that we can update FinalCurve appropriately
		OnCurveTablePostReimportHandle = FReimportManager::Instance()->OnPostReimport().AddRaw(this, &FScalableFloat::OnCurveTablePostReimport);
	}
#endif // WITH_EDITOR
}

void FScalableFloat::UnRegisterOnCurveTablePostReimport() const
{
#if WITH_EDITOR
	if (OnCurveTablePostReimportHandle.IsValid())
	{
		FReimportManager::Instance()->OnPostReimport().Remove(OnCurveTablePostReimportHandle);
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void FScalableFloat::OnCurveTablePostReimport(UObject* InObject, bool)
{
	if (Curve.CurveTable && Curve.CurveTable == InObject)
	{
		// Reset FinalCurve so that GetValueAtLevel will re-cache it the next time it gets called
		FinalCurve = nullptr;
	}
}
#endif // WITH_EDITOR

bool FGameplayAttribute::operator==(const FGameplayAttribute& Other) const
{
	return ((Other.Attribute == Attribute));
}

bool FGameplayAttribute::operator!=(const FGameplayAttribute& Other) const
{
	return ((Other.Attribute != Attribute));
}

bool FScalableFloat::operator==(const FScalableFloat& Other) const
{
	return ((Other.Curve == Curve) && (Other.Value == Value));
}

bool FScalableFloat::operator!=(const FScalableFloat& Other) const
{
	return ((Other.Curve != Curve) || (Other.Value != Value));
}

// ------------------------------------------------------------------------------------
//
// ------------------------------------------------------------------------------------
TSubclassOf<UAttributeSet> FindBestAttributeClass(TArray<TSubclassOf<UAttributeSet> >& ClassList, FString PartialName)
{
	for (auto Class : ClassList)
	{
		if (Class->GetName().Contains(PartialName))
		{
			return Class;
		}
	}

	return nullptr;
}

/**
 *	Transforms CurveTable data into format more effecient to read at runtime.
 *	UCurveTable requires string parsing to map to GroupName/AttributeSet/Attribute
 *	Each curve in the table represents a *single attribute's values for all levels*.
 *	At runtime, we want *all attribute values at given level*.
 */
void FAttributeSetInitter::PreloadAttributeSetData(UCurveTable* CurveData)
{
	if(!ensure(CurveData))
	{
		return;
	}

	/**
	 *	Get list of AttributeSet classes loaded
	 */

	TArray<TSubclassOf<UAttributeSet> >	ClassList;
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* TestClass = *ClassIt;
		if (TestClass->IsChildOf(UAttributeSet::StaticClass()))
		{
			ClassList.Add(TestClass);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// This can only work right now on POD attribute sets. If we ever support FStrings or TArrays in AttributeSets
			// we will need to update this code to not use memcpy etc.
			for (TFieldIterator<UProperty> PropIt(TestClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
			{
				if (!PropIt->HasAllPropertyFlags(CPF_IsPlainOldData))
				{
					ABILITY_LOG(Error, TEXT("FAttributeSetInitter::PreloadAttributeSetData Unable to Handle AttributeClass %s because it has a non POD property: %s"),
						*TestClass->GetName(), *PropIt->GetName());
					return;
				}
			}
#endif
		}
	}

	/**
	 *	Loop through CurveData table and build sets of Defaults that keyed off of Name + Level
	 */

	for (auto It = CurveData->RowMap.CreateConstIterator(); It; ++It)
	{
		FString RowName = It.Key().ToString();
		FString ClassName;
		FString SetName;
		FString AttributeName;
		FString Temp;

		RowName.Split(TEXT("."), &ClassName, &Temp);
		Temp.Split(TEXT("."), &SetName, &AttributeName);

		if (!ensure(!ClassName.IsEmpty() && !SetName.IsEmpty() && !AttributeName.IsEmpty()))
		{
			ABILITY_LOG(Warning, TEXT("FAttributeSetInitter::PreloadAttributeSetData Unable to parse row %s in %s"), *RowName, *CurveData->GetName());
			continue;
		}

		// Find the AttributeSet

		TSubclassOf<UAttributeSet> Set = FindBestAttributeClass(ClassList, SetName);
		if (!Set)
		{
			// This is ok, we may have rows in here that don't correspond directly to attributes
			ABILITY_LOG(Verbose, TEXT("FAttributeSetInitter::PreloadAttributeSetData Unable to match AttributeSet from %s (row: %s)"), *SetName, *RowName);
			continue;
		}

		// Find the UProperty

		UNumericProperty* Property = FindField<UNumericProperty>(*Set, *AttributeName);
		if (!Property)
		{
			ABILITY_LOG(Warning, TEXT("FAttributeSetInitter::PreloadAttributeSetData Unable to match Attribute from %s (row: %s)"), *AttributeName, *RowName);
			continue;
		}

		FRichCurve* Curve = It.Value();
		FName ClassFName = FName(*ClassName);
		FAttributeSetDefaulsCollection& DefaultCollection = Defaults.FindOrAdd(ClassFName);

		int32 LastLevel = Curve->GetLastKey().Time;
		DefaultCollection.LevelData.SetNum(FMath::Max(LastLevel, DefaultCollection.LevelData.Num()));

		
		//At this point we know the Name of this "class"/"group", the AttributeSet, and the Property Name. Now loop through the values on the curve to get the attribute default value at each level.
		for (auto KeyIter = Curve->GetKeyIterator(); KeyIter; ++KeyIter)
		{
			const FRichCurveKey& CurveKey = *KeyIter;

			int32 Level = CurveKey.Time;
			float Value = CurveKey.Value;

			FAttributeSetDefaults& SetDefaults = DefaultCollection.LevelData[Level-1];

			FAttributeDefaultValueList* DefaultDataList = SetDefaults.DataMap.Find(Set);
			if (DefaultDataList == nullptr)
			{
				ABILITY_LOG(Verbose, TEXT("Initializing new default set for %s[%d]. PropertySize: %d.. DefaultSize: %d"), *Set->GetName(), Level, Set->GetPropertiesSize(), UAttributeSet::StaticClass()->GetPropertiesSize());
				
				DefaultDataList = &SetDefaults.DataMap.Add(Set);
			}

			// Import curve value into default data

			check(DefaultDataList);
			DefaultDataList->AddPair(Property, Value);
		}


	}
}

void FAttributeSetInitter::InitAttributeSetDefaults(UAbilitySystemComponent* AbilitySystemComponent, FName GroupName, int32 Level, bool bInitialInit) const
{
	SCOPE_CYCLE_COUNTER(STAT_InitAttributeSetDefaults);
	
	const FAttributeSetDefaulsCollection* Collection = Defaults.Find(GroupName);
	if (!Collection)
	{
		ABILITY_LOG(Warning, TEXT("Unable to find DefaultAttributeSet Group %s. Failing back to Defaults"), *GroupName.ToString());
		Collection = Defaults.Find(FName(TEXT("Default")));
		if (!Collection)
		{
			ABILITY_LOG(Error, TEXT("FAttributeSetInitter::InitAttributeSetDefaults Default DefaultAttributeSet not found! Skipping Initialization"));
			return;
		}
	}

	if (!Collection->LevelData.IsValidIndex(Level - 1))
	{
		// We could eventually extrapolate values outside of the max defined levels
		ABILITY_LOG(Warning, TEXT("Attribute defaults for Level %d are not defined! Skipping"), Level);
		return;
	}

	const FAttributeSetDefaults& SetDefaults = Collection->LevelData[Level - 1];
	for (const UAttributeSet* Set : AbilitySystemComponent->SpawnedAttributes)
	{
		const FAttributeDefaultValueList* DefaultDataList = SetDefaults.DataMap.Find(Set->GetClass());
		if (DefaultDataList)
		{
			ABILITY_LOG(Log, TEXT("Initializing Set %s"), *Set->GetName());

			for (auto& DataPair : DefaultDataList->List)
			{
				check(DataPair.Property);

				if (Set->ShouldInitProperty(bInitialInit, DataPair.Property))
				{
					FGameplayAttribute AttributeToModify(DataPair.Property);
					AbilitySystemComponent->SetNumericAttributeBase(AttributeToModify, DataPair.Value);
				}
			}
		}		
	}
	
	AbilitySystemComponent->ForceReplication();
}
