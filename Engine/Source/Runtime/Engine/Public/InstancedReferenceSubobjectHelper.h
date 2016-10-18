// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/** 
 * Meant to represent a specific object property that is setup to reference a 
 * instanced sub-object. Tracks the property hierarchy used to reach the 
 * property, so that we can easily retrieve instanced sub-objects from a 
 * container object.
 */
struct FInstancedPropertyPath
{
private:
	struct FPropertyLink
	{
		FPropertyLink(const UProperty* Property, int32 ArrayIndexIn = INDEX_NONE)
			: PropertyPtr(Property), ArrayIndex(ArrayIndexIn)
		{}

		const UProperty* PropertyPtr;
		int32            ArrayIndex;
	};

public:
	//--------------------------------------------------------------------------
	FInstancedPropertyPath(UProperty* RootProperty)
	{
		PropertyChain.Add(FPropertyLink(RootProperty));
	}

	//--------------------------------------------------------------------------
	void Push(const UProperty* Property, int32 ArrayIndex = INDEX_NONE)
	{
		PropertyChain.Add(FPropertyLink(Property, ArrayIndex));		
	}

	//--------------------------------------------------------------------------
	void Pop()
	{
 		PropertyChain.RemoveAt(PropertyChain.Num() - 1);
	}

	//--------------------------------------------------------------------------
	const UProperty* Head() const
	{
		return PropertyChain.Last().PropertyPtr;
	}

	//--------------------------------------------------------------------------
	UObject* Resolve(const UObject* Container) const
	{
		UStruct* CurrentContainerType = Container->GetClass();

		const TArray<FPropertyLink>& PropChainRef = PropertyChain;
		auto GetProperty = [&CurrentContainerType, &PropChainRef](int32 ChainIndex)->UProperty*
		{
			const UProperty* SrcProperty = PropChainRef[ChainIndex].PropertyPtr;
			return FindField<UProperty>(CurrentContainerType, SrcProperty->GetFName());
		};

		UProperty* CurrentProp = GetProperty(0);
		const uint8* ValuePtr = (CurrentProp) ? CurrentProp->ContainerPtrToValuePtr<uint8>(Container) : nullptr;

		for (int32 ChainIndex = 1; CurrentProp && ChainIndex < PropertyChain.Num(); ++ChainIndex)
		{
			if (const UArrayProperty* ArrayProperty = Cast<UArrayProperty>(CurrentProp))
			{
				check(PropertyChain[ChainIndex].PropertyPtr == ArrayProperty->Inner);

				int32 TargetIndex = PropertyChain[ChainIndex].ArrayIndex;
				check(TargetIndex != INDEX_NONE);

				FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);
				if (TargetIndex >= ArrayHelper.Num())
				{
					CurrentProp = nullptr;
					break;
				}

				CurrentProp = ArrayProperty->Inner;
				ValuePtr    = ArrayHelper.GetRawPtr(TargetIndex);
			}
			else if (ensure(PropertyChain[ChainIndex].ArrayIndex <= 0))
			{
				if (const UStructProperty* StructProperty = Cast<UStructProperty>(CurrentProp))
				{
					CurrentContainerType = StructProperty->Struct;
				}

				CurrentProp = GetProperty(ChainIndex);
				ValuePtr    = (CurrentProp) ? CurrentProp->ContainerPtrToValuePtr<uint8>(ValuePtr) : nullptr;
			}
		}

		const UObjectProperty* TargetPropety = Cast<UObjectProperty>(CurrentProp);
		if ( ensure(TargetPropety && TargetPropety->HasAnyPropertyFlags(CPF_InstancedReference)) )
		{ 
			return TargetPropety->GetObjectPropertyValue(ValuePtr);
		}
		return nullptr;
	}

private:
	TArray<FPropertyLink> PropertyChain;
};

/** 
 * Can be used as a raw sub-object pointer, but also contains a 
 * FInstancedPropertyPath to identify the property that this sub-object is 
 * referenced by. Paired together for ease of use (so API users don't have to manage a map).
 */
struct FInstancedSubObjRef
{
	FInstancedSubObjRef(UObject* SubObj, const FInstancedPropertyPath& PropertyPathIn)
		: SubObjInstance(SubObj)
		, PropertyPath(PropertyPathIn)
	{}

	//--------------------------------------------------------------------------
	operator UObject*() const
	{
		return SubObjInstance;
	}

	//--------------------------------------------------------------------------
	UObject* operator->() const
	{
		return SubObjInstance;
	}

	//--------------------------------------------------------------------------
	friend uint32 GetTypeHash(const FInstancedSubObjRef& SubObjRef)
	{
		return GetTypeHash((UObject*)SubObjRef);
	}

	UObject* SubObjInstance;
	FInstancedPropertyPath PropertyPath;
};
 
/** 
 * Contains a set of utility functions useful for searching out and identifying
 * instanced sub-objects contained within a specific outer object.
 */
class FFindInstancedReferenceSubobjectHelper
{
public:
	//--------------------------------------------------------------------------
	template<typename T>
	static void GetInstancedSubObjects(const UObject* Container, T& OutObjects)
	{
		const UClass* ContainerClass = Container->GetClass();
		for (UProperty* Prop = ContainerClass->RefLink; Prop; Prop = Prop->NextRef)
		{
			FInstancedPropertyPath RootPropertyPath(Prop);
			GetInstancedSubObjects_Inner(RootPropertyPath, reinterpret_cast<const uint8*>(Container), OutObjects);
		}
	}

	//--------------------------------------------------------------------------
	static void Duplicate(UObject* OldObject, UObject* NewObject, TMap<UObject*, UObject*>& ReferenceReplacementMap, TArray<UObject*>& DuplicatedObjects)
	{
		if (OldObject->GetClass()->HasAnyClassFlags(CLASS_HasInstancedReference) &&
			NewObject->GetClass()->HasAnyClassFlags(CLASS_HasInstancedReference))
		{
			TArray<FInstancedSubObjRef> OldEditInlineObjects;
			GetInstancedSubObjects(OldObject, OldEditInlineObjects);
			if (OldEditInlineObjects.Num())
			{
				TArray<FInstancedSubObjRef> NewEditInlineObjects;
				GetInstancedSubObjects(NewObject, NewEditInlineObjects);
				for (auto Obj : NewEditInlineObjects)
				{
					const bool bProperOuter = (Obj->GetOuter() == OldObject);
					const bool bEditInlineNew = Obj->GetClass()->HasAnyClassFlags(CLASS_EditInlineNew | CLASS_DefaultToInstanced);
					if (bProperOuter && bEditInlineNew)
					{
						const bool bKeptByOld = OldEditInlineObjects.Contains(Obj);
						const bool bNotHandledYet = !ReferenceReplacementMap.Contains(Obj);
						if (bKeptByOld && bNotHandledYet)
						{
							UObject* NewEditInlineSubobject = StaticDuplicateObject(Obj, NewObject);
							ReferenceReplacementMap.Add(Obj, NewEditInlineSubobject);

							// NOTE: we cannot patch OldObject's linker table here, since we don't 
							//       know the relation between the  two objects (one could be of a 
							//       super class, and the other a child)

							// We also need to make sure to fixup any properties here
							DuplicatedObjects.Add(NewEditInlineSubobject);
						}
					}
				}
			}
		}
	}

private:
	//--------------------------------------------------------------------------
	template<typename T>
	static void GetInstancedSubObjects_Inner(FInstancedPropertyPath& PropertyPath, const uint8* ContainerAddress, T& OutObjects)
	{
		check(ContainerAddress);
		const UProperty* TargetProp = PropertyPath.Head();

		const UArrayProperty* ArrayProperty = Cast<const UArrayProperty>(TargetProp);
		if (ArrayProperty && ArrayProperty->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_ContainsInstancedReference))
		{
			const UStructProperty* InnerStructProperty = Cast<const UStructProperty>(ArrayProperty->Inner);
			const UObjectProperty* InnerObjectProperty = Cast<const UObjectProperty>(ArrayProperty->Inner);
			if (InnerStructProperty && InnerStructProperty->Struct)
			{
				FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, ContainerAddress);
				for (int32 ElementIndex = 0; ElementIndex < ArrayHelper.Num(); ++ElementIndex)
				{
					const uint8* ValueAddress = ArrayHelper.GetRawPtr(ElementIndex);
					if (ValueAddress)
					{
						for (UProperty* StructProp = InnerStructProperty->Struct->RefLink; StructProp; StructProp = StructProp->NextRef)
						{
							PropertyPath.Push(InnerStructProperty, ElementIndex);
							GetInstancedSubObjects_Inner(PropertyPath, ValueAddress, OutObjects);
							PropertyPath.Pop();
						}
					}
				}
			}
			else if (InnerObjectProperty && InnerObjectProperty->HasAllPropertyFlags(CPF_PersistentInstance))
			{
				ensure(InnerObjectProperty->HasAllPropertyFlags(CPF_InstancedReference));
				FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, ContainerAddress);
				for (int32 ElementIndex = 0; ElementIndex < ArrayHelper.Num(); ++ElementIndex)
				{
					UObject* ObjectValue = InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetRawPtr(ElementIndex));
					if (ObjectValue)
					{
						PropertyPath.Push(InnerObjectProperty, ElementIndex);
						OutObjects.Add(FInstancedSubObjRef(ObjectValue, PropertyPath));
						PropertyPath.Pop();
					}
					
				}
			}
		}
		else if (TargetProp->HasAllPropertyFlags(CPF_PersistentInstance))
		{
			ensure(TargetProp->HasAllPropertyFlags(CPF_InstancedReference));
			const UObjectProperty* ObjectProperty = Cast<const UObjectProperty>(TargetProp);
			if (ObjectProperty)
			{
				for (int32 ArrayIdx = 0; ArrayIdx < ObjectProperty->ArrayDim; ++ArrayIdx)
				{
					UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue_InContainer(ContainerAddress, ArrayIdx);
					if (ObjectValue)
					{
						// don't need to push to PropertyPath, since this property is already at its head
						OutObjects.Add(FInstancedSubObjRef(ObjectValue, PropertyPath));	
					}
				}
			}
		}
		else if (TargetProp->HasAnyPropertyFlags(CPF_ContainsInstancedReference))
		{
			const UStructProperty* StructProperty = Cast<const UStructProperty>(TargetProp);
			if (StructProperty && StructProperty->Struct)
			{
				for (int32 ArrayIdx = 0; ArrayIdx < StructProperty->ArrayDim; ++ArrayIdx)
				{
					const uint8* ValueAddress = StructProperty->ContainerPtrToValuePtr<uint8>(ContainerAddress, ArrayIdx);
					for (UProperty* StructProp = StructProperty->Struct->RefLink; StructProp; StructProp = StructProp->NextRef)
					{
						PropertyPath.Push(StructProp, ArrayIdx);
						GetInstancedSubObjects_Inner(PropertyPath, ValueAddress, OutObjects);
						PropertyPath.Pop();
					}
				}
			}
		}
	}
};