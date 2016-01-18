// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "LinkerPlaceholderBase.h"
#include "Blueprint/BlueprintSupport.h"

/*******************************************************************************
 *FPlaceholderContainerTracker
 ******************************************************************************/

/**  */
struct FPlaceholderContainerTracker : TThreadSingleton<FPlaceholderContainerTracker>
{
	TArray<UObject*> PerspectiveReferencerStack;
};

//------------------------------------------------------------------------------
FScopedPlaceholderContainerTracker::FScopedPlaceholderContainerTracker(UObject* InPlaceholderContainerCandidate)
	: PlaceholderReferencerCandidate(InPlaceholderContainerCandidate)
{
	FPlaceholderContainerTracker::Get().PerspectiveReferencerStack.Add(InPlaceholderContainerCandidate);
}

//------------------------------------------------------------------------------
FScopedPlaceholderContainerTracker::~FScopedPlaceholderContainerTracker()
{
	UObject* StackTop = FPlaceholderContainerTracker::Get().PerspectiveReferencerStack.Pop();
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	check(StackTop == PlaceholderReferencerCandidate);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
}

/*******************************************************************************
 * FLinkerPlaceholderBase
 ******************************************************************************/

namespace LinkerPlaceholderObjectImpl
{
	/**
	 * Traces the specified property up its outer chain, all the way up to the 
	 * OwnerClass. Constructs a UProperty path along the way, so that we can use
	 * traverse a property tree down, from the class owner down.
	 * 
	 * @param  LeafProperty	The property you want a path for.
	 * @param  ChainOut		Output array that tracks the specified property back up its outer chain (first entry should be what was passed in LeafProperty). 
	 */
	static void BuildPropertyChain(const UProperty* LeafProperty, TArray<const UProperty*>& ChainOut);

	/**
	 * A recursive method that replaces all leaf references to this object with 
	 * the supplied ReplacementValue.
	 *
	 * This function recurses the property chain (from class owner down) because 
	 * at the time of AddReferencingPropertyValue() we cannot know/record the 
	 * address/index of array properties (as they may change during array 
	 * re-allocation or compaction). So we must follow the property chain and 
	 * check every UArrayProperty member for references to this (hence, the need
	 * for this recursive function).
	 * 
	 * @param  PropertyChain    An ascending outer chain, where the property at index zero is the leaf (referencer) property.
	 * @param  ChainIndex		An index into the PropertyChain that this call should start at and iterate DOWN to zero.
	 * @param  ValueAddress     The memory address of the value corresponding to the property at ChainIndex.
	 * @param  OldValue
	 * @param  ReplacementValue	The new object to replace all references to this with.
	 * @return The number of references that were replaced.
	 */
	static int32 ResolvePlaceholderValues(const TArray<const UProperty*>& PropertyChain, int32 ChainIndex, uint8* ValueAddress, UObject* OldValue, UObject* ReplacementValue);
}

//------------------------------------------------------------------------------
static void LinkerPlaceholderObjectImpl::BuildPropertyChain(const UProperty* LeafProperty, TArray<const UProperty*>& ChainOut)
{
	ChainOut.Empty();
	ChainOut.Add(LeafProperty);

	UClass* ClassOwner = LeafProperty->GetOwnerClass();

	UObject* PropertyOuter = LeafProperty->GetOuter();
	while ((PropertyOuter != nullptr) && (PropertyOuter != ClassOwner))
	{
		if (const UProperty* PropertyOwner = Cast<const UProperty>(PropertyOuter))
		{
			ChainOut.Add(PropertyOwner);
		}
		PropertyOuter = PropertyOuter->GetOuter();
	}
}

//------------------------------------------------------------------------------
static int32 LinkerPlaceholderObjectImpl::ResolvePlaceholderValues(const TArray<const UProperty*>& PropertyChain, int32 ChainIndex, uint8* ValueAddress, UObject* OldValue, UObject* ReplacementValue)
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
			if (ReferencingProperty->GetObjectPropertyValue(ValueAddress) == OldValue)
			{
				// @TODO: use an FArchiver with ReferencingProperty->SerializeItem() 
				//        so that we can utilize CheckValidObject()
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
				ReplacementCount += ResolvePlaceholderValues(PropertyChain, PropertyIndex - 1, MemberAddress, OldValue, ReplacementValue);
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

//------------------------------------------------------------------------------
FLinkerPlaceholderBase::FLinkerPlaceholderBase()
	: bResolveWasInvoked(false)
{
}

//------------------------------------------------------------------------------
FLinkerPlaceholderBase::~FLinkerPlaceholderBase()
{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	check(!HasKnownReferences());
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
}

//------------------------------------------------------------------------------
bool FLinkerPlaceholderBase::AddReferencingPropertyValue(const UObjectProperty* ReferencingProperty, void* DataPtr)
{
	TArray<UObject*>& PossibleReferencers = FPlaceholderContainerTracker::Get().PerspectiveReferencerStack;

	UObject* FoundReferencer = nullptr;
	// iterate backwards because this is meant to act as a stack, where the last
	// entry is most likely the one we're looking for
	for (int32 CandidateIndex = PossibleReferencers.Num() - 1; CandidateIndex >= 0; --CandidateIndex)
	{
		UObject* ReferencerCandidate = PossibleReferencers[CandidateIndex];

		UClass* CandidateClass = ReferencerCandidate->GetClass();
		UClass* PropOwnerClass = ReferencingProperty->GetOwnerClass();

		if (CandidateClass->IsChildOf(PropOwnerClass))
		{
			FoundReferencer = ReferencerCandidate;
			break;
		}
	}

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	check(ReferencingProperty->GetObjectPropertyValue(DataPtr) == GetPlaceholderAsUObject());
	check(FoundReferencer != nullptr);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

	if (FoundReferencer != nullptr)
	{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		// @TODO: verify that DataPtr belongs to FoundReferencer
// 		uint8* ContainerStart = (uint8*)FoundReferencer;
// 		uint8* ContainerEnd   = ContainerStart + FoundReferencer->GetClass()->GetStructureSize();
// 		// check that we picked the right container object, and that DataPtr exists within it 
// 		check(DataPtr >= ContainerStart && DataPtr <= ContainerEnd);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

		ReferencingContainers.FindOrAdd(FoundReferencer).Add(ReferencingProperty);
	}
	return (FoundReferencer != nullptr);
}

//------------------------------------------------------------------------------
bool FLinkerPlaceholderBase::HasKnownReferences() const
{
	return (ReferencingContainers.Num() > 0);
}

//------------------------------------------------------------------------------
int32 FLinkerPlaceholderBase::ResolveAllPlaceholderReferences(UObject* ReplacementObj)
{
	int32 ReplacementCount = ResolvePlaceholderPropertyValues(ReplacementObj);
	ReferencingContainers.Empty();

	MarkAsResolved();
	return ReplacementCount;
}

//------------------------------------------------------------------------------
bool FLinkerPlaceholderBase::HasBeenFullyResolved() const
{
	return IsMarkedResolved() && !HasKnownReferences();
}

//------------------------------------------------------------------------------
bool FLinkerPlaceholderBase::IsMarkedResolved() const
{
	return bResolveWasInvoked;
}

//------------------------------------------------------------------------------
void FLinkerPlaceholderBase::MarkAsResolved()
{
	bResolveWasInvoked = true;
}
 
//------------------------------------------------------------------------------
int32 FLinkerPlaceholderBase::ResolvePlaceholderPropertyValues(UObject* NewObjectValue)
{
	int32 ResolvedTotal = 0;

	UObject* ThisAsUObject = GetPlaceholderAsUObject();
	for (auto& ReferencingPair : ReferencingContainers)
	{
		TWeakObjectPtr<UObject> ContainerPtr = ReferencingPair.Key;
		if (!ContainerPtr.IsValid())
		{
			continue;
		}
		UObject* Container = ContainerPtr.Get();

		for (const UObjectProperty* Property : ReferencingPair.Value)
		{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
			check(Container->GetClass()->IsChildOf(Property->GetOwnerClass()));
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

			TArray<const UProperty*> PropertyChain;
			LinkerPlaceholderObjectImpl::BuildPropertyChain(Property, PropertyChain);
			const UProperty* OutermostProperty = PropertyChain.Last();

			uint8* OutermostAddress = OutermostProperty->ContainerPtrToValuePtr<uint8>((uint8*)Container, /*ArrayIndex =*/0);
			int32 ResolvedCount = LinkerPlaceholderObjectImpl::ResolvePlaceholderValues(PropertyChain, PropertyChain.Num() - 1, OutermostAddress, ThisAsUObject, NewObjectValue);
			ResolvedTotal += ResolvedCount;

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
			// we expect that (because we have had ReferencingProperties added) 
			// there should be at least one reference that is resolved... if 
			// there were none, then a property could have changed its value 
			// after it was set to this
			// 
			// NOTE: this may seem it can be resolved by properties removing 
			//       themselves from ReferencingProperties, but certain 
			//       properties may be the inner of a UArrayProperty (meaning 
			//       there could be multiple references per property)... we'd 
			//       have to inc/decrement a property ref-count to resolve that 
			//       scenario
			check(ResolvedCount > 0);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		}
	}

	return ResolvedTotal;
}

/*******************************************************************************
 * TLinkerImportPlaceholder<UClass>
 ******************************************************************************/

//------------------------------------------------------------------------------
template<>
int32 TLinkerImportPlaceholder<UClass>::ResolvePropertyReferences(UClass* ReplacementClass)
{
	int32 ReplacementCount = 0;
	UClass* PlaceholderClass = CastChecked<UClass>(GetPlaceholderAsUObject());

	for (UProperty* Property : ReferencingProperties)
	{
		if (UObjectPropertyBase* BaseObjProperty = Cast<UObjectPropertyBase>(Property))
		{
			if (BaseObjProperty->PropertyClass == PlaceholderClass)
			{
				BaseObjProperty->PropertyClass = ReplacementClass;
				++ReplacementCount;
			}

			if (UClassProperty* ClassProperty = Cast<UClassProperty>(BaseObjProperty))
			{
				if (ClassProperty->MetaClass == PlaceholderClass)
				{
					ClassProperty->MetaClass = ReplacementClass;
					++ReplacementCount;
				}
			}
			else if (UAssetClassProperty* AssetClassProperty = Cast<UAssetClassProperty>(BaseObjProperty))
			{
				if (AssetClassProperty->MetaClass == PlaceholderClass)
				{
					AssetClassProperty->MetaClass = ReplacementClass;
					++ReplacementCount;
				}	
			}
		}	
		else if (UInterfaceProperty* InterfaceProp = Cast<UInterfaceProperty>(Property))
		{
			if (InterfaceProp->InterfaceClass == PlaceholderClass)
			{
				InterfaceProp->InterfaceClass = ReplacementClass;
				++ReplacementCount;
			}
		}
		else
		{
			ensureMsgf(false, TEXT("Unhandled property type: %s"), *Property->GetClass()->GetName());
		}
	}

	ReferencingProperties.Empty();
	return ReplacementCount;
}

/*******************************************************************************
 * TLinkerImportPlaceholder<UFunction>
 ******************************************************************************/

//------------------------------------------------------------------------------
template<>
int32 TLinkerImportPlaceholder<UFunction>::ResolvePropertyReferences(UFunction* ReplacementFunc)
{
	int32 ReplacementCount = 0;
	UFunction* PlaceholderFunc = CastChecked<UFunction>(GetPlaceholderAsUObject());

	for (UProperty* Property : ReferencingProperties)
	{
		if (UDelegateProperty* DelegateProperty = Cast<UDelegateProperty>(Property))
		{
			if (DelegateProperty->SignatureFunction == PlaceholderFunc)
			{
				DelegateProperty->SignatureFunction = ReplacementFunc;
				++ReplacementCount;
			}
		}
		else if (UMulticastDelegateProperty* MulticastDelegateProperty = Cast<UMulticastDelegateProperty>(Property))
		{
			if (MulticastDelegateProperty->SignatureFunction == PlaceholderFunc)
			{
				MulticastDelegateProperty->SignatureFunction = ReplacementFunc;
				++ReplacementCount;
			}
		}
		else
		{
			ensureMsgf(false, TEXT("Unhandled property type: %s"), *Property->GetClass()->GetName());
		}
	}

	ReferencingProperties.Empty();
	return ReplacementCount;
}
