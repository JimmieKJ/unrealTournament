// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "ComponentVisualizer.h"

IMPLEMENT_HIT_PROXY(HComponentVisProxy, HHitProxy);


FComponentVisualizer::FPropertyNameAndIndex FComponentVisualizer::GetComponentPropertyName(const UActorComponent* Component)
{
	if (Component)
	{
		const AActor* CompOwner = Component->GetOwner();
		if (CompOwner)
		{
			// Iterate over UObject* fields of this actor
			UClass* ActorClass = CompOwner->GetClass();
			for (TFieldIterator<UObjectProperty> It(ActorClass); It; ++It)
			{
				// See if this property points to the component in question
				UObjectProperty* ObjectProp = *It;
				for (int32 Index = 0; Index < ObjectProp->ArrayDim; ++Index)
				{
					UObject* Object = ObjectProp->GetObjectPropertyValue(ObjectProp->ContainerPtrToValuePtr<void>(CompOwner, Index));
					if (Object == Component)
					{
						// It does! Return this name
						return FPropertyNameAndIndex(ObjectProp->GetFName(), Index);
					}
				}
			}

			// If nothing found, look in TArray<UObject*> fields
			for (TFieldIterator<UArrayProperty> It(ActorClass); It; ++It)
			{
				UArrayProperty* ArrayProp = *It;
				if (UObjectProperty* InnerProp = Cast<UObjectProperty>(It->Inner))
				{
					FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(CompOwner));
					for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
					{
						UObject* Object = InnerProp->GetObjectPropertyValue(ArrayHelper.GetRawPtr(Index));
						if (Object == Component)
						{
							return FPropertyNameAndIndex(ArrayProp->GetFName(), Index);
						}
					}
				}
			}
		}
	}

	// Didn't find actor property referencing this component
	return FPropertyNameAndIndex();
}

UActorComponent* FComponentVisualizer::GetComponentFromPropertyName(const AActor* CompOwner, const FPropertyNameAndIndex& Property)
{
	UActorComponent* ResultComp = NULL;
	if(CompOwner && Property.IsValid())
	{
		UClass* ActorClass = CompOwner->GetClass();
		UProperty* Prop = FindField<UProperty>(ActorClass, Property.Name);
		if (UObjectProperty* ObjectProp = Cast<UObjectProperty>(Prop))
		{
			UObject* Object = ObjectProp->GetObjectPropertyValue(ObjectProp->ContainerPtrToValuePtr<void>(CompOwner, Property.Index));
			ResultComp = Cast<UActorComponent>(Object);
		}
		else if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(Prop))
		{
			if (UObjectProperty* InnerProp = Cast<UObjectProperty>(ArrayProp->Inner))
			{
				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(CompOwner));
				UObject* Object = InnerProp->GetObjectPropertyValue(ArrayHelper.GetRawPtr(Property.Index));
				ResultComp = Cast<UActorComponent>(Object);
			}
		}
	}

	return ResultComp;
}
