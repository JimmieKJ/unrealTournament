// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFindInstancedReferenceSubobjectHelper
{
public:
	template<typename T>
	static void Get(const UStruct* Struct, const uint8* ContainerAddress, T& OutObjects)
	{
		check(ContainerAddress);
		for (UProperty* Prop = (Struct ? Struct->RefLink : NULL); Prop; Prop = Prop->NextRef)
		{
			auto ArrayProperty = Cast<const UArrayProperty>(Prop);
			if (ArrayProperty && Prop->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_ContainsInstancedReference))
			{
				auto InnerStructProperty = Cast<const UStructProperty>(ArrayProperty->Inner);
				auto InnerObjectProperty = Cast<const UObjectProperty>(ArrayProperty->Inner);
				if (InnerStructProperty && InnerStructProperty->Struct)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, ContainerAddress);
					for (int32 ElementIndex = 0; ElementIndex < ArrayHelper.Num(); ++ElementIndex)
					{
						const uint8* ValueAddress = ArrayHelper.GetRawPtr(ElementIndex);
						if (ValueAddress)
						{
							Get(InnerStructProperty->Struct, ValueAddress, OutObjects);
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
							OutObjects.Add(ObjectValue);
						}
					}
				}
			}
			else if (Prop->HasAllPropertyFlags(CPF_PersistentInstance))
			{
				ensure(Prop->HasAllPropertyFlags(CPF_InstancedReference));
				auto ObjectProperty = Cast<const UObjectProperty>(Prop);
				if (ObjectProperty)
				{
					for (int32 ArrayIdx = 0; ArrayIdx < Prop->ArrayDim; ++ArrayIdx)
					{
						auto ObjectValue = ObjectProperty->GetObjectPropertyValue_InContainer(ContainerAddress, ArrayIdx);
						if (ObjectValue)
						{
							OutObjects.Add(ObjectValue);
						}
					}
				}
			}
			else if (Prop->HasAnyPropertyFlags(CPF_ContainsInstancedReference))
			{
				auto StructProperty = Cast<const UStructProperty>(Prop);
				if (StructProperty && StructProperty->Struct)
				{
					for (int32 ArrayIdx = 0; ArrayIdx < Prop->ArrayDim; ++ArrayIdx)
					{
						const uint8* ValueAddress = StructProperty->ContainerPtrToValuePtr<uint8>(ContainerAddress, ArrayIdx);
						Get(StructProperty->Struct, ValueAddress, OutObjects);
					}
				}
			}
		}
	}

	static void Duplicate(UObject* OldObject, UObject* NewObject, TMap<UObject*, UObject*>& ReferenceReplacementMap, TArray<UObject*>& DuplicatedObjects)
	{
		if (OldObject->GetClass()->HasAnyClassFlags(CLASS_HasInstancedReference) &&
			NewObject->GetClass()->HasAnyClassFlags(CLASS_HasInstancedReference))
		{
			TSet<UObject*> OldEditInlineObjects;
			Get(OldObject->GetClass(), reinterpret_cast<uint8*>(OldObject), OldEditInlineObjects);
			if (OldEditInlineObjects.Num())
			{
				TSet<UObject*> NewEditInlineObjects;
				Get(NewObject->GetClass(), reinterpret_cast<uint8*>(NewObject), NewEditInlineObjects);
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
							UObject* NewEditInlineSubobject = StaticDuplicateObject(Obj, NewObject, NULL);
							ReferenceReplacementMap.Add(Obj, NewEditInlineSubobject);

							TArray<UObject*> OutDefaultOuters;
							GetObjectsWithOuter(NewEditInlineSubobject, OutDefaultOuters, false);

							// We also need to make sure to fixup any properties here
							DuplicatedObjects.Add(NewEditInlineSubobject);
						}
					}
				}
			}
		}
	}
};