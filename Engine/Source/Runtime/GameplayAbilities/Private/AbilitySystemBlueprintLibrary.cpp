// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitMovementModeChange.h"
#include "Abilities/Tasks/AbilityTask_WaitOverlap.h"
#include "Abilities/Tasks/AbilityTask_WaitConfirmCancel.h"
#include "LatentActions.h"

UAbilitySystemBlueprintLibrary::UAbilitySystemBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

UAbilitySystemComponent* UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AActor *Actor)
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
}

void UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(AActor* Actor, FGameplayTag EventTag, FGameplayEventData Payload)
{
	if (Actor && !Actor->IsPendingKill())
	{
		IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Actor);
		if (AbilitySystemInterface != NULL)
		{
			UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemInterface->GetAbilitySystemComponent();
			if (AbilitySystemComponent != NULL)
			{
				FScopedPredictionWindow NewScopedWindow(AbilitySystemComponent, true);
				AbilitySystemComponent->HandleGameplayEvent(EventTag, &Payload);
			}
		}
	}
}

float UAbilitySystemBlueprintLibrary::GetFloatAttribute(const class AActor* Actor, FGameplayAttribute Attribute, bool& bSuccessfullyFoundAttribute)
{
	bSuccessfullyFoundAttribute = true;

	const UAbilitySystemComponent* const AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!AbilitySystem || !AbilitySystem->HasAttributeSetForAttribute(Attribute))
	{
		bSuccessfullyFoundAttribute = false;
		return 0.f;
	}

	const float Result = AbilitySystem->GetNumericAttribute(Attribute);
	return Result;
}

FGameplayAbilityTargetDataHandle UAbilitySystemBlueprintLibrary::AppendTargetDataHandle(FGameplayAbilityTargetDataHandle TargetHandle, FGameplayAbilityTargetDataHandle HandleToAdd)
{
	TargetHandle.Append(&HandleToAdd);
	return TargetHandle;
}

FGameplayAbilityTargetDataHandle UAbilitySystemBlueprintLibrary::AbilityTargetDataFromLocations(const FGameplayAbilityTargetingLocationInfo& SourceLocation, const FGameplayAbilityTargetingLocationInfo& TargetLocation)
{
	// Construct TargetData
	FGameplayAbilityTargetData_LocationInfo*	NewData = new FGameplayAbilityTargetData_LocationInfo();
	NewData->SourceLocation = SourceLocation;
	NewData->TargetLocation = TargetLocation;

	// Give it a handle and return
	FGameplayAbilityTargetDataHandle	Handle;
	Handle.Data.Add(TSharedPtr<FGameplayAbilityTargetData_LocationInfo>(NewData));
	return Handle;
}

FGameplayAbilityTargetDataHandle UAbilitySystemBlueprintLibrary::AbilityTargetDataFromActor(AActor* Actor)
{
	// Construct TargetData
	FGameplayAbilityTargetData_ActorArray*	NewData = new FGameplayAbilityTargetData_ActorArray();
	NewData->TargetActorArray.Add(Actor);
	FGameplayAbilityTargetDataHandle		Handle(NewData);
	return Handle;
}
FGameplayAbilityTargetDataHandle UAbilitySystemBlueprintLibrary::AbilityTargetDataFromActorArray(TArray<TWeakObjectPtr<AActor>> ActorArray, bool OneTargetPerHandle)
{
	// Construct TargetData
	if (OneTargetPerHandle)
	{
		FGameplayAbilityTargetDataHandle		Handle;
		for (int32 i = 0; i < ActorArray.Num(); ++i)
		{
			if (ActorArray[i].IsValid())
			{
				FGameplayAbilityTargetDataHandle TempHandle = AbilityTargetDataFromActor(ActorArray[i].Get());
				Handle.Append(&TempHandle);
			}
		}
		return Handle;
	}
	else
	{
		FGameplayAbilityTargetData_ActorArray*	NewData = new FGameplayAbilityTargetData_ActorArray();
		NewData->TargetActorArray = ActorArray;
		FGameplayAbilityTargetDataHandle		Handle(NewData);
		return Handle;
	}
}

FGameplayAbilityTargetDataHandle UAbilitySystemBlueprintLibrary::FilterTargetData(FGameplayAbilityTargetDataHandle TargetDataHandle, FGameplayTargetDataFilterHandle FilterHandle)
{
	FGameplayAbilityTargetDataHandle ReturnDataHandle;
	
	for (int32 i = 0; TargetDataHandle.IsValid(i); ++i)
	{
		FGameplayAbilityTargetData* UnfilteredData = TargetDataHandle.Get(i);
		check(UnfilteredData);
		if (UnfilteredData->GetActors().Num() > 0)
		{
			TArray<TWeakObjectPtr<AActor>> FilteredActors = UnfilteredData->GetActors().FilterByPredicate(FilterHandle);
			if (FilteredActors.Num() > 0)
			{
				//Copy the data first, since we don't understand the internals of it
				UScriptStruct* ScriptStruct = UnfilteredData->GetScriptStruct();
				FGameplayAbilityTargetData* NewData = (FGameplayAbilityTargetData*)FMemory::Malloc(ScriptStruct->GetCppStructOps()->GetSize());
				ScriptStruct->InitializeStruct(NewData);
				ScriptStruct->CopyScriptStruct(NewData, UnfilteredData);
				ReturnDataHandle.Data.Add(TSharedPtr<FGameplayAbilityTargetData>(NewData));
				if (FilteredActors.Num() < UnfilteredData->GetActors().Num())
				{
					//We have lost some, but not all, of our actors, so replace the array. This should only be possible with targeting types that permit actor-array setting.
					if (!NewData->SetActors(FilteredActors))
					{
						//This is an error, though we could ignore it. We somehow filtered out part of a list, but the class doesn't support changing the list, so now it's all or nothing.
						check(false);
					}
				}
			}
		}
	}

	return ReturnDataHandle;
}

FGameplayTargetDataFilterHandle UAbilitySystemBlueprintLibrary::MakeFilterHandle(FGameplayTargetDataFilter Filter, AActor* FilterActor)
{
	FGameplayTargetDataFilterHandle FilterHandle;
	FGameplayTargetDataFilter* NewFilter = new FGameplayTargetDataFilter(Filter);
	NewFilter->InitializeFilterContext(FilterActor);
	FilterHandle.Filter = TSharedPtr<FGameplayTargetDataFilter>(NewFilter);
	return FilterHandle;
}

FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::MakeSpecHandle(UGameplayEffect* InGameplayEffect, AActor* InInstigator, AActor* InEffectCauser, float InLevel)
{
	FGameplayEffectContext* EffectContext = new FGameplayEffectContext(InInstigator, InEffectCauser);
	return FGameplayEffectSpecHandle(new FGameplayEffectSpec(InGameplayEffect, FGameplayEffectContextHandle(EffectContext), InLevel));
}

FGameplayAbilityTargetDataHandle UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(FHitResult HitResult)
{
	// Construct TargetData
	FGameplayAbilityTargetData_SingleTargetHit* TargetData = new FGameplayAbilityTargetData_SingleTargetHit(HitResult);

	// Give it a handle and return
	FGameplayAbilityTargetDataHandle	Handle;
	Handle.Data.Add(TSharedPtr<FGameplayAbilityTargetData>(TargetData));

	return Handle;
}

int32 UAbilitySystemBlueprintLibrary::GetDataCountFromTargetData(FGameplayAbilityTargetDataHandle TargetData)
{
	return TargetData.Data.Num();
}

TArray<AActor*> UAbilitySystemBlueprintLibrary::GetActorsFromTargetData(FGameplayAbilityTargetDataHandle TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
		TArray<AActor*>	ResolvedArray;
		if (Data)
		{
			TArray<TWeakObjectPtr<AActor> > WeakArray = Data->GetActors();
			for (TWeakObjectPtr<AActor> WeakPtr : WeakArray)
			{
				ResolvedArray.Add(WeakPtr.Get());
			}
		}
		return ResolvedArray;
	}
	return TArray<AActor*>();
}

bool UAbilitySystemBlueprintLibrary::DoesTargetDataContainActor(FGameplayAbilityTargetDataHandle TargetData, int32 Index, AActor* Actor)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			for (auto DataActorIter : Data->GetActors())
			{
				if (DataActorIter == Actor)
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool UAbilitySystemBlueprintLibrary::TargetDataHasActor(FGameplayAbilityTargetDataHandle TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			return (Data->GetActors().Num() > 0);
		}
	}
	return false;
}

bool UAbilitySystemBlueprintLibrary::TargetDataHasHitResult(FGameplayAbilityTargetDataHandle TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			return Data->HasHitResult();
		}
	}
	return false;
}

FHitResult UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(FGameplayAbilityTargetDataHandle TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			const FHitResult* HitResultPtr = Data->GetHitResult();
			if (HitResultPtr)
			{
				return *HitResultPtr;
			}
		}
	}

	return FHitResult();
}

bool UAbilitySystemBlueprintLibrary::TargetDataHasOrigin(FGameplayAbilityTargetDataHandle TargetData, int32 Index)
{
	FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
	if (Data)
	{
		return (Data->HasHitResult() || Data->HasOrigin());
	}
	return false;
}

FTransform UAbilitySystemBlueprintLibrary::GetTargetDataOrigin(FGameplayAbilityTargetDataHandle TargetData, int32 Index)
{
	FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
	if (Data)
	{
		if (Data->HasOrigin())
		{
			return Data->GetOrigin();
		}
		if (Data->HasHitResult())
		{
			const FHitResult* HitResultPtr = Data->GetHitResult();
			FTransform ReturnTransform;
			ReturnTransform.SetLocation(HitResultPtr->TraceStart);
			ReturnTransform.SetRotation((HitResultPtr->Location - HitResultPtr->TraceStart).GetSafeNormal().Rotation().Quaternion());
			return ReturnTransform;
		}
	}

	return FTransform::Identity;
}

bool UAbilitySystemBlueprintLibrary::TargetDataHasEndPoint(FGameplayAbilityTargetDataHandle TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			return (Data->HasHitResult() || Data->HasEndPoint());
		}
	}
	return false;
}

FVector UAbilitySystemBlueprintLibrary::GetTargetDataEndPoint(FGameplayAbilityTargetDataHandle TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			const FHitResult* HitResultPtr = Data->GetHitResult();
			if (HitResultPtr)
			{
				return HitResultPtr->Location;
			}
			else if (Data->HasEndPoint())
			{
				return Data->GetEndPoint();
			}
		}
	}

	return FVector::ZeroVector;
}

FTransform UAbilitySystemBlueprintLibrary::GetTargetDataEndPointTransform(FGameplayAbilityTargetDataHandle TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			return Data->GetEndPointTransform();
		}
	}

	return FTransform::Identity;
}


// -------------------------------------------------------------------------------------


bool UAbilitySystemBlueprintLibrary::EffectContextIsInstigatorLocallyControlled(FGameplayEffectContextHandle EffectContext)
{
	return EffectContext.IsLocallyControlled();
}

FHitResult UAbilitySystemBlueprintLibrary::EffectContextGetHitResult(FGameplayEffectContextHandle EffectContext)
{
	if (EffectContext.GetHitResult())
	{
		return *EffectContext.GetHitResult();
	}

	return FHitResult();
}

bool UAbilitySystemBlueprintLibrary::EffectContextHasHitResult(FGameplayEffectContextHandle EffectContext)
{
	return EffectContext.GetHitResult() != NULL;
}

AActor*	UAbilitySystemBlueprintLibrary::EffectContextGetInstigatorActor(FGameplayEffectContextHandle EffectContext)
{
	return EffectContext.GetInstigator();
}

AActor*	UAbilitySystemBlueprintLibrary::EffectContextGetOriginalInstigatorActor(FGameplayEffectContextHandle EffectContext)
{
	return EffectContext.GetOriginalInstigator();
}

AActor*	UAbilitySystemBlueprintLibrary::EffectContextGetEffectCauser(FGameplayEffectContextHandle EffectContext)
{
	return EffectContext.GetEffectCauser();
}

UObject* UAbilitySystemBlueprintLibrary::EffectContextGetSourceObject(FGameplayEffectContextHandle EffectContext)
{
	return const_cast<UObject*>( EffectContext.GetSourceObject() );
}

FVector UAbilitySystemBlueprintLibrary::EffectContextGetOrigin(FGameplayEffectContextHandle EffectContext)
{
	if (EffectContext.HasOrigin())
	{
		return EffectContext.GetOrigin();
	}

	return FVector::ZeroVector;
}

bool UAbilitySystemBlueprintLibrary::IsInstigatorLocallyControlled(FGameplayCueParameters Parameters)
{
	return Parameters.EffectContext.IsLocallyControlled();
}

int32 UAbilitySystemBlueprintLibrary::GetActorCount(FGameplayCueParameters Parameters)
{
	return Parameters.EffectContext.GetActors().Num();
}

AActor* UAbilitySystemBlueprintLibrary::GetActorByIndex(FGameplayCueParameters Parameters, int32 Index)
{
	if (Parameters.EffectContext.GetActors().IsValidIndex(Index))
	{
		return Parameters.EffectContext.GetActors()[Index].Get();
	}
	return NULL;
}

FHitResult UAbilitySystemBlueprintLibrary::GetHitResult(FGameplayCueParameters Parameters)
{
	if (Parameters.EffectContext.GetHitResult())
	{
		return *Parameters.EffectContext.GetHitResult();
	}
	
	return FHitResult();
}

bool UAbilitySystemBlueprintLibrary::HasHitResult(FGameplayCueParameters Parameters)
{
	return Parameters.EffectContext.GetHitResult() != NULL;
}

void UAbilitySystemBlueprintLibrary::ForwardGameplayCueToTarget(TScriptInterface<IGameplayCueInterface> TargetCueInterface, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	AActor* ActorTarget = Cast<AActor>(TargetCueInterface.GetObject());
	if (TargetCueInterface && ActorTarget)
	{
		TargetCueInterface->HandleGameplayCue(ActorTarget, Parameters.OriginalTag, EventType, Parameters);
	}
}

AActor*	UAbilitySystemBlueprintLibrary::GetInstigatorActor(FGameplayCueParameters Parameters)
{
	return Parameters.EffectContext.GetInstigator();
}

FTransform UAbilitySystemBlueprintLibrary::GetInstigatorTransform(FGameplayCueParameters Parameters)
{
	AActor* InstigatorActor = GetInstigatorActor(Parameters);
	if (InstigatorActor)
	{
		return InstigatorActor->GetTransform();
	}

	ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::GetInstigatorTransform called on GameplayCue with no valid instigator"));
	return FTransform::Identity;
}

FVector UAbilitySystemBlueprintLibrary::GetOrigin(FGameplayCueParameters Parameters)
{
	if (Parameters.EffectContext.HasOrigin())
	{
		return Parameters.EffectContext.GetOrigin();
	}

	return FVector::ZeroVector;
}

bool UAbilitySystemBlueprintLibrary::GetGameplayCueEndLocationAndNormal(AActor* TargetActor, FGameplayCueParameters Parameters, FVector& Location, FVector& Normal)
{
	FGameplayEffectContext* Data = Parameters.EffectContext.Get();
	if (Data && Data->GetHitResult())
	{
		
		Location = Data->GetHitResult()->Location;
		Normal = Data->GetHitResult()->Normal;
		return true;
	}
	else if(TargetActor)
	{
		Location = TargetActor->GetActorLocation();
		Normal = TargetActor->GetActorForwardVector();
		return true;
	}

	return false;
}

bool UAbilitySystemBlueprintLibrary::GetGameplayCueDirection(AActor* TargetActor, FGameplayCueParameters Parameters, FVector& Direction)
{
	if (FGameplayEffectContext* Ctx = Parameters.EffectContext.Get())
	{
		if (Ctx->GetHitResult())
		{
			// Most projectiles and melee attacks will use this
			Direction = (-1.f * Ctx->GetHitResult()->Normal);
			return true;
		}
		else if (TargetActor && Ctx->HasOrigin())
		{
			// Fallback to trying to use the target location and the origin of the effect
			FVector NewVec = (TargetActor->GetActorLocation() - Ctx->GetOrigin());
			NewVec.Normalize();
			Direction = NewVec;
			return true;
		}
		else if (TargetActor && Ctx->GetEffectCauser())
		{
			// Finally, try to use the direction between the causer of the effect and the target of the effect
			FVector NewVec = (TargetActor->GetActorLocation() - Ctx->GetEffectCauser()->GetActorLocation());
			NewVec.Normalize();
			Direction = NewVec;
			return true;
		}
	}

	Direction = FVector::ZeroVector;
	return false;
}

bool UAbilitySystemBlueprintLibrary::DoesGameplayCueMeetTagRequirements(FGameplayCueParameters Parameters, FGameplayTagRequirements& SourceTagReqs, FGameplayTagRequirements& TargetTagReqs)
{
	return SourceTagReqs.RequirementsMet(Parameters.AggregatedSourceTags)
		&& TargetTagReqs.RequirementsMet(Parameters.AggregatedSourceTags);
}

// ---------------------------------------------------------------------------------------

FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::AssignSetByCallerMagnitude(FGameplayEffectSpecHandle SpecHandle, FName DataName, float Magnitude)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->SetSetByCallerMagnitude(DataName, Magnitude);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::AssignSetByCallerMagnitude called with invalid SpecHandle"));
	}

	return SpecHandle;
}

FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::SetDuration(FGameplayEffectSpecHandle SpecHandle, float Duration)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->SetDuration(Duration, true);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::SetDuration called with invalid SpecHandle"));
	}

	return SpecHandle;
}

FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::AddGrantedTag(FGameplayEffectSpecHandle SpecHandle, FGameplayTag NewGameplayTag)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->DynamicGrantedTags.AddTag(NewGameplayTag);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::AddGrantedTag called with invalid SpecHandle"));
	}

	return SpecHandle;
}

FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::AddGrantedTags(FGameplayEffectSpecHandle SpecHandle, FGameplayTagContainer NewGameplayTags)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->DynamicGrantedTags.AppendTags(NewGameplayTags);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::AddGrantedTags called with invalid SpecHandle"));
	}

	return SpecHandle;
}
	
FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::AddLinkedGameplayEffectSpec(FGameplayEffectSpecHandle SpecHandle, FGameplayEffectSpecHandle LinkedGameplayEffectSpec)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->TargetEffectSpecs.Add(LinkedGameplayEffectSpec);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::AddLinkedGameplayEffectSpec called with invalid SpecHandle"));
	}

	return SpecHandle;
}


FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::SetStackCount(FGameplayEffectSpecHandle SpecHandle, int32 StackCount)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->StackCount = StackCount;
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::AddLinkedGameplayEffectSpec called with invalid SpecHandle"));
	}
	return SpecHandle;
}
	
FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::SetStackCountToMax(FGameplayEffectSpecHandle SpecHandle)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec && Spec->Def)
	{
		Spec->StackCount = Spec->Def->StackLimitCount;
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::AddLinkedGameplayEffectSpec called with invalid SpecHandle"));
	}
	return SpecHandle;
}

int32 UAbilitySystemBlueprintLibrary::GetActiveGameplayEffectStackCount(FActiveGameplayEffectHandle ActiveHandle)
{
	UAbilitySystemComponent* ASC = ActiveHandle.GetOwningAbilitySystemComponent();
	if (ASC)
	{
		return ASC->GetCurrentStackCount(ActiveHandle);
	}
	return 0;
}