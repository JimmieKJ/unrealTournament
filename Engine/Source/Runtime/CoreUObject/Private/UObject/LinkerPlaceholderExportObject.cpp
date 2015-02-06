// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "LinkerPlaceholderExportObject.h"
#include "Blueprint/BlueprintSupport.h"

//------------------------------------------------------------------------------
ULinkerPlaceholderExportObject::ULinkerPlaceholderExportObject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	, bWasResolved(false)
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
{
}

//------------------------------------------------------------------------------
IMPLEMENT_CORE_INTRINSIC_CLASS(ULinkerPlaceholderExportObject, UObject,
	{
	}
);

//------------------------------------------------------------------------------
void ULinkerPlaceholderExportObject::BeginDestroy()
{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	check(bWasResolved || HasAnyFlags(RF_ClassDefaultObject));
	check(ReferencingProperties.Num() == 0);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

	Super::BeginDestroy();
}

//------------------------------------------------------------------------------
void ULinkerPlaceholderExportObject::AddReferencingProperty(const UObjectProperty* ReferencingProperty)
{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	check(bWasResolved != true);
	check(!ReferencingProperty->GetLinker() || ReferencingProperty->GetLinker()->LinkerRoot == GetOuter());
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

	ReferencingProperties.Add(ReferencingProperty);
}

//------------------------------------------------------------------------------
bool ULinkerPlaceholderExportObject::IsReferencedBy(const UObjectProperty* Property)
{
	return ReferencingProperties.Contains(Property);
}

//------------------------------------------------------------------------------
void ULinkerPlaceholderExportObject::ReplaceReferencingObjectValues(UClass* Container, UObject* NewObjectValue)
{
	UClass* ClassOwner = Container->GetClass();
	for (TWeakObjectPtr<UObjectProperty> PropertyPtr : ReferencingProperties)
	{
		if (!PropertyPtr.IsValid())
		{
			continue;
		}
		const UObjectProperty* Property = PropertyPtr.Get();

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		check(Property->GetOwnerClass() == ClassOwner);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

		TArray<const UProperty*> PropertyChain;
		PropertyChain.Add(Property);
		const UProperty* OutermostProperty = Property;

		UObject* PropertyOuter = Property->GetOuter();
		while ((PropertyOuter != nullptr) && (PropertyOuter != ClassOwner))
		{
			if (const UProperty* PropertyOwner = Cast<const UProperty>(PropertyOuter))
			{
				OutermostProperty = PropertyOwner;
				PropertyChain.Add(PropertyOwner);
			}
			PropertyOuter = PropertyOuter->GetOuter();
		}

		uint8* OutermostAddress = OutermostProperty->ContainerPtrToValuePtr<uint8>((uint8*)Container, /*ArrayIndex =*/0);
		int32 ResolvedCount = ResolvePlaceholderValues(PropertyChain, PropertyChain.Num() - 1, OutermostAddress, NewObjectValue);

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		// we expect that (because we have had ReferencingProperties added) 
		// there should be at least one reference that is resolved... if this 
		// there were none, then a property could have changed its value after
		// it was set to this
		// 
		// NOTE: this may seem it can be resolved by properties removing 
		//       themselves from ReferencingProperties, but certain properties
		//       may be the inner of a UArrayProperty (meaning there could be
		//       multiple references per property)... we'd have to inc/decrement
		//       a property ref-count to resolve that scenario
		check(ResolvedCount > 0);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	}

	ReferencingProperties.Empty();
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	bWasResolved = true;
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
}

//------------------------------------------------------------------------------
int32 ULinkerPlaceholderExportObject::ResolvePlaceholderValues(const TArray<const UProperty*>& PropertyChain, int32 ChainIndex, uint8* ValueAddress, UObject* ReplacementValue)
{
	int32 ReplacementCount = 0;

	for (int32 PropertyIndex = ChainIndex; PropertyIndex >= 0; --PropertyIndex)
	{
		const UProperty* Property = PropertyChain[PropertyIndex];
		if (PropertyIndex == 0)
		{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
			check(Property->IsA<UObjectProperty>());
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

			const UObjectProperty* ReferencingProperty = (const UObjectProperty*)Property;
			if (ReferencingProperty->GetObjectPropertyValue(ValueAddress) == this)
			{
				ReferencingProperty->SetObjectPropertyValue(ValueAddress, ReplacementValue);
				// @TODO: unfortunately, this is currently protected
				//ReferencingProperty->CheckValidObject(ValueAddress);

				++ReplacementCount;
			}
		}
		else if (const UArrayProperty* ArrayProperty = Cast<const UArrayProperty>(Property))
		{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
			const UProperty* NextProperty = PropertyChain[PropertyIndex - 1];
			check(NextProperty == ArrayProperty->Inner);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

			// because we can't know which array entry was set with a reference 
			// to this object, we have to comb through them all
			FScriptArrayHelper ArrayHelper(ArrayProperty, ValueAddress);
			for (int32 ArrayIndex = 0; ArrayIndex < ArrayHelper.Num(); ++ArrayIndex)
			{
				uint8* MemberAddress = ArrayHelper.GetRawPtr(ArrayIndex);
				ReplacementCount += ResolvePlaceholderValues(PropertyChain, PropertyIndex - 1, MemberAddress, ReplacementValue);
			}

			// the above recursive call chewed through the rest of the
			// PropertyChain, no need to keep on here
			break;
		}
		else
		{
			const UProperty* NextProperty = PropertyChain[PropertyIndex - 1];
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
			check(NextProperty->GetOuter() == Property);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
			ValueAddress = Property->ContainerPtrToValuePtr<uint8>(ValueAddress, /*ArrayIndex =*/0);
		}
	}

	return ReplacementCount;
}
