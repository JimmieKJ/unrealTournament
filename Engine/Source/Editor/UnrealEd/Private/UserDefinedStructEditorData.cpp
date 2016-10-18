// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "StructureEditorUtils.h"
#include "Engine/UserDefinedStruct.h"
#include "BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "UserDefinedStructEditorData"

bool FStructVariableDescription::SetPinType(const FEdGraphPinType& VarType)
{
	Category = VarType.PinCategory;
	SubCategory = VarType.PinSubCategory;
	SubCategoryObject = VarType.PinSubCategoryObject.Get();
	bIsArray = VarType.bIsArray;

	return !VarType.bIsReference && !VarType.bIsWeakPointer;
}

FEdGraphPinType FStructVariableDescription::ToPinType() const
{
	return FEdGraphPinType(Category, SubCategory, SubCategoryObject.Get(), bIsArray, false);
}

UUserDefinedStructEditorData::UUserDefinedStructEditorData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UUserDefinedStruct* ScriptStruct = GetOwnerStruct();
	if (ScriptStruct)
	{
		DefaultStructInstance.SetPackage(ScriptStruct->GetOutermost());
	}
}

uint32 UUserDefinedStructEditorData::GenerateUniqueNameIdForMemberVariable()
{
	const uint32 Result = UniqueNameId;
	++UniqueNameId;
	return Result;
}

UUserDefinedStruct* UUserDefinedStructEditorData::GetOwnerStruct() const
{
	return Cast<UUserDefinedStruct>(GetOuter());
}

void UUserDefinedStructEditorData::PostEditUndo()
{
	Super::PostEditUndo();
	FStructureEditorUtils::OnStructureChanged(GetOwnerStruct());
}

class FStructureTransactionAnnotation : public ITransactionObjectAnnotation
{
public:
	FStructureTransactionAnnotation(FStructureEditorUtils::EStructureEditorChangeInfo ChangeInfo)
		: ActiveChange(ChangeInfo)
	{
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { /** Don't need this functionality for now */ }

	FStructureEditorUtils::EStructureEditorChangeInfo GetActiveChange()
	{
		return ActiveChange;
	}

protected:
	FStructureEditorUtils::EStructureEditorChangeInfo ActiveChange;
};

TSharedPtr<ITransactionObjectAnnotation> UUserDefinedStructEditorData::GetTransactionAnnotation() const
{
	return MakeShareable(new FStructureTransactionAnnotation(FStructureEditorUtils::FStructEditorManager::ActiveChange));
}

void UUserDefinedStructEditorData::PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation)
{
	Super::PostEditUndo();
	FStructureEditorUtils::EStructureEditorChangeInfo ActiveChange = FStructureEditorUtils::Unknown;

	if (TransactionAnnotation.IsValid())
	{
		TSharedPtr<FStructureTransactionAnnotation> StructAnnotation = StaticCastSharedPtr<FStructureTransactionAnnotation>(TransactionAnnotation);
		if (StructAnnotation.IsValid())
		{
			ActiveChange = StructAnnotation->GetActiveChange();
		}
	}
	FStructureEditorUtils::OnStructureChanged(GetOwnerStruct(), ActiveChange);
}

void UUserDefinedStructEditorData::PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph)
{
	Super::PostLoadSubobjects(OuterInstanceGraph);

	for (auto& VarDesc : VariablesDescriptions)
	{
		VarDesc.bInvalidMember = !FStructureEditorUtils::CanHaveAMemberVariableOfType(GetOwnerStruct(), VarDesc.ToPinType());
	}
}

const uint8* UUserDefinedStructEditorData::GetDefaultInstance() const
{
	ensure(DefaultStructInstance.IsValid() && DefaultStructInstance.GetStruct() == GetOwnerStruct());
	return DefaultStructInstance.GetStructMemory();
}

void UUserDefinedStructEditorData::RecreateDefaultInstance(FString* OutLog)
{
	UStruct* ScriptStruct = GetOwnerStruct();
	DefaultStructInstance.Recreate(ScriptStruct);
	uint8* StructData = DefaultStructInstance.GetStructMemory();
	ensure(DefaultStructInstance.IsValid() && DefaultStructInstance.GetStruct() == ScriptStruct);
	if (DefaultStructInstance.IsValid() && StructData && ScriptStruct)
	{
		DefaultStructInstance.SetPackage(ScriptStruct->GetOutermost());

		for (TFieldIterator<UProperty> It(ScriptStruct); It; ++It)
		{
			UProperty* Property = *It;
			if (Property)
			{
				auto VarDesc = VariablesDescriptions.FindByPredicate(FStructureEditorUtils::FFindByNameHelper<FStructVariableDescription>(Property->GetFName()));
				if (VarDesc && !VarDesc->CurrentDefaultValue.IsEmpty())
				{
					if (!FBlueprintEditorUtils::PropertyValueFromString(Property, VarDesc->CurrentDefaultValue, StructData))
					{
						const FString Message = FString::Printf(TEXT("Cannot parse value. Property: %s String: \"%s\" ")
							, (Property ? *Property->GetDisplayNameText().ToString() : TEXT("None"))
							, *VarDesc->CurrentDefaultValue);
						UE_LOG(LogClass, Warning, TEXT("UUserDefinedStructEditorData::RecreateDefaultInstance %s Struct: %s "), *Message, *GetPathNameSafe(ScriptStruct));
						if (OutLog)
						{
							OutLog->Append(Message);
						}
					}
				}
			}
		}
	}
}

void UUserDefinedStructEditorData::CleanDefaultInstance()
{
	ensure(!DefaultStructInstance.IsValid() || DefaultStructInstance.GetStruct() == GetOwnerStruct());
	DefaultStructInstance.Destroy();
}

void UUserDefinedStructEditorData::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UUserDefinedStructEditorData* This = CastChecked<UUserDefinedStructEditorData>(InThis);

	UStruct* ScriptStruct = This->GetOwnerStruct();
	ensure(!This->DefaultStructInstance.IsValid() || This->DefaultStructInstance.GetStruct() == ScriptStruct);
	uint8* StructData = This->DefaultStructInstance.GetStructMemory();
	if (StructData)
	{
		FSimpleObjectReferenceCollectorArchive ObjectReferenceCollector(This, Collector);
		ScriptStruct->SerializeBin(ObjectReferenceCollector, StructData);
	}

	Super::AddReferencedObjects(This, Collector);
}

#undef LOCTEXT_NAMESPACE