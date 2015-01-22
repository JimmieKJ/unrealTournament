// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Vector.h"
#include "Math/Rotator.h"

class UNREALED_API FComponentEditorUtils
{
public:

	static class USceneComponent* GetSceneComponent(class UObject* Object, class UObject* SubObject = NULL);

	static void GetArchetypeInstances(class UObject* Object, TArray<class UObject*>& ArchetypeInstances);

	struct FTransformData
	{
		FTransformData(const class USceneComponent& Component)
			: Trans(Component.RelativeLocation)
			, Rot(Component.RelativeRotation)
			, Scale(Component.RelativeScale3D)
		{}

		FVector Trans; 
		FRotator Rot;
		FVector Scale;
	};

	// Given a template, propagates a default transform change to all instances of the template
	static void PropagateTransformPropertyChange(class USceneComponent* InSceneComponentTemplate, const FTransformData& OldDefaultTransform, const FTransformData& NewDefaultTransform, TSet<class USceneComponent*>& UpdatedComponents);

	// Given an instance of a template and a property, propagates a default value change to the instance (only if applicable)
	template<typename T>
	static void PropagateTransformPropertyChange(class USceneComponent* InSceneComponent, const class UProperty* InProperty, const T& OldDefaultValue, const T& NewDefaultValue, TSet<class USceneComponent*>& UpdatedComponents)
	{
		check(InProperty != nullptr);
		check(InSceneComponent != nullptr);

		T* CurrentValue = InProperty->ContainerPtrToValuePtr<T>(InSceneComponent);
		if(CurrentValue != nullptr)
		{
			PropagateTransformPropertyChange(InSceneComponent, *CurrentValue, OldDefaultValue, NewDefaultValue, UpdatedComponents);
		}
	}

	// Given an instance of a template and a current value, propagates a default value change to the instance (only if applicable)
	template<typename T>
	static void PropagateTransformPropertyChange(class USceneComponent* InSceneComponent, T& CurrentValue, const T& OldDefaultValue, const T& NewDefaultValue, TSet<class USceneComponent*>& UpdatedComponents)
	{
		check(InSceneComponent != nullptr);

		// Propagate the change only if the current instanced value matches the previous default value (otherwise this could overwrite any per-instance override)
		if(NewDefaultValue != OldDefaultValue && CurrentValue == OldDefaultValue)
		{
			// Ensure that this instance will be included in any undo/redo operations, and record it into the transaction buffer.
			// Note: We don't do this for components that originate from script, because they will be re-instanced from the template after an undo, so there is no need to record them.
			if(!InSceneComponent->bCreatedByConstructionScript)
			{
				InSceneComponent->SetFlags(RF_Transactional);
				InSceneComponent->Modify();
			}

			// We must also modify the owner, because we'll need script components to be reconstructed as part of an undo operation.
			AActor* Owner = InSceneComponent->GetOwner();
			if(Owner != nullptr)
			{
				Owner->Modify();
			}

			// Modify the value
			CurrentValue = NewDefaultValue;

			// Re-register the component with the scene so that transforms are updated for display
			InSceneComponent->ReregisterComponent();

			// Add the component to the set of updated instances
			UpdatedComponents.Add(InSceneComponent);
		}
	}
};