// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "PropertyHandle.h"

void PropertyHandleToPropertyPath(const UClass* OwnerClass, const IPropertyHandle& InPropertyHandle, TArray<UProperty*>& PropertyPath)
{
	PropertyPath.Add(InPropertyHandle.GetProperty());
	TSharedPtr<IPropertyHandle> CurrentHandle = InPropertyHandle.GetParentHandle();
	while (CurrentHandle.IsValid() && CurrentHandle->GetProperty() != nullptr)
	{
		PropertyPath.Insert(CurrentHandle->GetProperty(), 0);
		CurrentHandle = CurrentHandle->GetParentHandle();
	}
}

FCanKeyPropertyParams::FCanKeyPropertyParams()
{
	ObjectClass = nullptr;
}

FCanKeyPropertyParams::FCanKeyPropertyParams(UClass* InObjectClass, TArray<UProperty*> InPropertyPath)
{
	ObjectClass = InObjectClass;
	PropertyPath = InPropertyPath;
}

FCanKeyPropertyParams::FCanKeyPropertyParams(UClass* InObjectClass, const IPropertyHandle& InPropertyHandle)
{
	ObjectClass = InObjectClass;
	PropertyHandleToPropertyPath(ObjectClass, InPropertyHandle, PropertyPath);
}

FKeyPropertyParams::FKeyPropertyParams()
{
}

FKeyPropertyParams::FKeyPropertyParams(TArray<UObject*> InObjectsToKey, TArray<UProperty*> InPropertyPath)
{
	ObjectsToKey = InObjectsToKey;
	PropertyPath = InPropertyPath;
}

FKeyPropertyParams::FKeyPropertyParams(TArray<UObject*> InObjectsToKey, const IPropertyHandle& InPropertyHandle)
{
	ObjectsToKey = InObjectsToKey;
	PropertyHandleToPropertyPath(InObjectsToKey[0]->GetClass(), InPropertyHandle, PropertyPath);
	KeyParams.bCreateKeyOnlyWhenAutoKeying = true;
}

FString FPropertyChangedParams::GetPropertyPathString() const
{
	TArray<FString> PropertyNames;
	for (UProperty* Property : PropertyPath)
	{
		PropertyNames.Add(Property->GetName());
	}
	return FString::Join(PropertyNames, TEXT("."));
}