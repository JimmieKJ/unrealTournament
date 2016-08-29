// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GameplayTasksPrivatePCH.h"
#include "GameplayTaskResource.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
TArray<FString> UGameplayTaskResource::ResourceDescriptions;
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

UGameplayTaskResource::UGameplayTaskResource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bManuallySetID = false;
	ManualResourceID = INDEX_NONE;
	AutoResourceID = INDEX_NONE;
}

void UGameplayTaskResource::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject) && (GetClass()->HasAnyClassFlags(CLASS_Abstract) == false)
#if WITH_HOT_RELOAD
		&& !GIsHotReload
#endif // WITH_HOT_RELOAD
		)
	{
		if (bManuallySetID == false || ManualResourceID == INDEX_NONE)
		{
			UpdateAutoResourceID();
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const uint8 DebugId = GetResourceID();
		ResourceDescriptions.SetNum(FMath::Max(DebugId + 1, ResourceDescriptions.Num()));
		ResourceDescriptions[DebugId] = GenerateDebugDescription();
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	}
}

void UGameplayTaskResource::UpdateAutoResourceID()
{
	static uint16 NextAutoResID = 0;

	if (AutoResourceID == INDEX_NONE)
	{
		AutoResourceID = NextAutoResID++;
		if (AutoResourceID >= FGameplayResourceSet::MaxResources)
		{
			UE_LOG(LogGameplayTasks, Error, TEXT("AutoResourceID out of bounds (probably too much GameplayTaskResource classes, consider manually assigning values if you can split all classes into non-overlapping sets"));
		}
	}
}

#if WITH_EDITOR
void UGameplayTaskResource::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_ManuallySetID = GET_MEMBER_NAME_CHECKED(UGameplayTaskResource, bManuallySetID);

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property != NULL)
	{
		FName PropName = PropertyChangedEvent.Property->GetFName();

		if (PropName == NAME_ManuallySetID)
		{
			if (!bManuallySetID)
			{
				ManualResourceID = INDEX_NONE;
				// if we don't have ManualResourceID make sure AutoResourceID is valid
				UpdateAutoResourceID();
			}
		}
	}
}
#endif // WITH_EDITOR

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
FString UGameplayTaskResource::GenerateDebugDescription() const
{
	const FString ClassName = GetClass()->GetName();
	int32 SeparatorIdx = INDEX_NONE;
	if (ClassName.FindChar(TEXT('_'), SeparatorIdx))
	{
		return ClassName.Mid(SeparatorIdx + 1);
	}

	return ClassName;

}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
