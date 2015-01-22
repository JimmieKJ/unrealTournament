// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "ComponentVisualizer.h"

IMPLEMENT_HIT_PROXY(HComponentVisProxy, HHitProxy);


FName FComponentVisualizer::GetComponentPropertyName(const UActorComponent* Component)
{
	FName PropertyName = NAME_None;
	if (Component)
	{
		const AActor* CompOwner = Component->GetOwner();
		if(CompOwner)
		{
			// Iterate over fields of this actor
			UClass* ActorClass = CompOwner->GetClass();
			for (TFieldIterator<UObjectProperty> It(ActorClass); It; ++It)
			{
				// See if this property points to the component in question
				UObjectProperty* ObjectProp = *It;
				UObject* Object = ObjectProp->GetObjectPropertyValue(ObjectProp->ContainerPtrToValuePtr<void>(CompOwner));
				if (Object == Component)
				{
					// It does! Remember this name
					PropertyName = ObjectProp->GetFName();
				}
			}
		}
	}
	return PropertyName;
}

UActorComponent* FComponentVisualizer::GetComponentFromPropertyName(const AActor* CompOwner, FName PropertyName)
{
	UActorComponent* ResultComp = NULL;
	if(CompOwner && PropertyName != NAME_None)
	{
		UClass* ActorClass = CompOwner->GetClass();
		UObjectProperty* ObjectProp = FindField<UObjectProperty>(ActorClass, PropertyName);
		if(ObjectProp)
		{
			UObject* Object = ObjectProp->GetObjectPropertyValue(ObjectProp->ContainerPtrToValuePtr<void>(CompOwner));
			ResultComp = Cast<UActorComponent>(Object);
		}

	}
	return ResultComp;
}
