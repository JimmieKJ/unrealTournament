// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "KeyPropertyParams.h"
#include "PropertyHandle.h"

TArray<UProperty*> PropertyHandleToPropertyPath(const UClass* OwnerClass, const IPropertyHandle& InPropertyHandle)
{
	TArray<UProperty*> PropertyPath;

	PropertyPath.Add(InPropertyHandle.GetProperty());
	TSharedPtr<IPropertyHandle> CurrentHandle = InPropertyHandle.GetParentHandle();
	while (CurrentHandle.IsValid() && CurrentHandle->GetProperty() != nullptr)
	{
		PropertyPath.Insert(CurrentHandle->GetProperty(), 0);
		CurrentHandle = CurrentHandle->GetParentHandle();
	}

	return PropertyPath;
}

FCanKeyPropertyParams::FCanKeyPropertyParams(UClass* InObjectClass, TArray<UProperty*> InPropertyPath)
	: ObjectClass(InObjectClass)
	, PropertyPath(InPropertyPath)
{
}

FCanKeyPropertyParams::FCanKeyPropertyParams(UClass* InObjectClass, const IPropertyHandle& InPropertyHandle)
	: ObjectClass(InObjectClass)
	, PropertyPath(PropertyHandleToPropertyPath(InObjectClass, InPropertyHandle))
{
}

FKeyPropertyParams::FKeyPropertyParams(TArray<UObject*> InObjectsToKey, TArray<UProperty*> InPropertyPath, ESequencerKeyMode InKeyMode)
	: ObjectsToKey(InObjectsToKey)
	, PropertyPath(InPropertyPath)
	, KeyMode(InKeyMode)

{
}

FKeyPropertyParams::FKeyPropertyParams(TArray<UObject*> InObjectsToKey, const IPropertyHandle& InPropertyHandle, ESequencerKeyMode InKeyMode)
	: ObjectsToKey(InObjectsToKey)
	, PropertyPath(PropertyHandleToPropertyPath(InObjectsToKey[0]->GetClass(), InPropertyHandle))
	, KeyMode(InKeyMode)
{
}

FPropertyChangedParams::FPropertyChangedParams(TArray<UObject*> InObjectsThatChanged, TArray<UProperty*> InPropertyPath, FName InStructPropertyNameToKey, ESequencerKeyMode InKeyMode)
	: ObjectsThatChanged(InObjectsThatChanged)
	, PropertyPath(InPropertyPath)
	, StructPropertyNameToKey(InStructPropertyNameToKey)
	, KeyMode(InKeyMode)
{
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
