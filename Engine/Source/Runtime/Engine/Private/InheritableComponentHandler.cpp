// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/SCS_Node.h"
#include "Engine/InheritableComponentHandler.h"

#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#endif // WITH_EDITOR

// UInheritableComponentHandler

void UInheritableComponentHandler::PostLoad()
{
	Super::PostLoad();

	for (int32 Index = Records.Num() - 1; Index >= 0; --Index)
	{
		FComponentOverrideRecord& Record = Records[Index];
		if (Record.ComponentTemplate && !CastChecked<UActorComponent>(Record.ComponentTemplate->GetArchetype())->IsEditableWhenInherited())
		{
			Record.ComponentTemplate->MarkPendingKill(); // hack needed to be able to identify if NewObject returns this back to us in the future
			Records.RemoveAtSwap(Index);
		}
	}
}

#if WITH_EDITOR
UActorComponent* UInheritableComponentHandler::CreateOverridenComponentTemplate(FComponentKey Key)
{
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		FComponentOverrideRecord& Record = Records[Index];
		if (Record.ComponentKey.Match(Key))
		{
			if (Record.ComponentTemplate)
			{
				return Record.ComponentTemplate;
			}
			Records.RemoveAtSwap(Index);
			break;
		}
	}

	auto BestArchetype = FindBestArchetype(Key);
	if (!BestArchetype)
	{
		UE_LOG(LogBlueprint, Warning, TEXT("CreateOverridenComponentTemplate '%s': cannot find archetype for component '%s' from '%s'"),
			*GetPathNameSafe(this), *Key.GetSCSVariableName().ToString(), *GetPathNameSafe(Key.GetComponentOwner()));
		return NULL;
	}
	ensure(Cast<UBlueprintGeneratedClass>(GetOuter()));
	auto NewComponentTemplate = NewObject<UActorComponent>(
		GetOuter(), BestArchetype->GetClass(), BestArchetype->GetFName(), RF_ArchetypeObject | RF_Public | RF_InheritableComponentTemplate, BestArchetype);

	// HACK: NewObject can return a pre-existing object which will not have been initialized to the archetype.  When we remove the old handlers, we mark them pending
	//       kill so we can identify that situation here (see UE-13987/UE-13990)
	if (NewComponentTemplate->IsPendingKill())
	{
		NewComponentTemplate->ClearPendingKill();
		UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
		CopyParams.bDoDelta = false;
		UEngine::CopyPropertiesForUnrelatedObjects(BestArchetype, NewComponentTemplate, CopyParams);
	}

	FComponentOverrideRecord NewRecord;
	NewRecord.ComponentKey = Key;
	NewRecord.ComponentTemplate = NewComponentTemplate;
	Records.Add(NewRecord);

	return NewComponentTemplate;
}

void UInheritableComponentHandler::RemoveOverridenComponentTemplate(FComponentKey Key)
{
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		FComponentOverrideRecord& Record = Records[Index];
		if (Record.ComponentKey.Match(Key))
		{
			Record.ComponentTemplate->MarkPendingKill(); // hack needed to be able to identify if NewObject returns this back to us in the future
			Records.RemoveAtSwap(Index);
			break;
		}
	}
}

void UInheritableComponentHandler::UpdateOwnerClass(UBlueprintGeneratedClass* OwnerClass)
{
	for (auto& Record : Records)
	{
		auto OldComponentTemplate = Record.ComponentTemplate;
		if (OldComponentTemplate && (OwnerClass != OldComponentTemplate->GetOuter()))
		{
			Record.ComponentTemplate = DuplicateObject(OldComponentTemplate, OwnerClass, OldComponentTemplate->GetFName());
		}
	}
}

void UInheritableComponentHandler::ValidateTemplates()
{
	for (int32 Index = 0; Index < Records.Num();)
	{
		bool bIsValidAndNecessary = false;
		{
			FComponentOverrideRecord& Record = Records[Index];
			FComponentKey& ComponentKey = Record.ComponentKey;
			
			FName VarName = ComponentKey.GetSCSVariableName();
			if (ComponentKey.RefreshVariableName())
			{
				FName NewName = ComponentKey.GetSCSVariableName();
				UE_LOG(LogBlueprint, Log, TEXT("ValidateTemplates '%s': variable old name '%s' new name '%s'"),
					*GetPathNameSafe(this), *VarName.ToString(), *NewName.ToString());
				VarName = NewName;

				MarkPackageDirty();
			}

			if (IsRecordValid(Record))
			{
				if (IsRecordNecessary(Record))
				{
					bIsValidAndNecessary = true;
				}
				else
				{
					UE_LOG(LogBlueprint, Log, TEXT("ValidateTemplates '%s': overridden template is unnecessary - component '%s' from '%s'"),
						*GetPathNameSafe(this), *VarName.ToString(), *GetPathNameSafe(ComponentKey.GetComponentOwner()));
				}
			}
			else
			{
				UE_LOG(LogBlueprint, Warning, TEXT("ValidateTemplates '%s': overridden template is invalid - component '%s' from '%s'"),
					*GetPathNameSafe(this), *VarName.ToString(), *GetPathNameSafe(ComponentKey.GetComponentOwner()));
			}
		}

		if (bIsValidAndNecessary)
		{
			++Index;
		}
		else
		{
			Records.RemoveAtSwap(Index);
		}
	}
}

bool UInheritableComponentHandler::IsValid() const
{
	for (auto& Record : Records)
	{
		if (!IsRecordValid(Record))
		{
			return false;
		}
	}
	return true;
}

bool UInheritableComponentHandler::IsRecordValid(const FComponentOverrideRecord& Record) const
{
	UClass* OwnerClass = Cast<UClass>(GetOuter());
	ensure(OwnerClass);

	if (!Record.ComponentTemplate)
	{
		return false;
	}

	if (Record.ComponentTemplate->GetOuter() != OwnerClass)
	{
		return false;
	}

	if (!Record.ComponentKey.IsValid())
	{
		return false;
	}

	auto OriginalTemplate = Record.ComponentKey.GetOriginalTemplate();
	if (!OriginalTemplate)
	{
		return false;
	}

	if (OriginalTemplate->GetClass() != Record.ComponentTemplate->GetClass())
	{
		return false;
	}
	
	return true;
}

struct FComponentComparisonHelper
{
	static bool AreIdentical(UObject* ObjectA, UObject* ObjectB)
	{
		if (!ObjectA || !ObjectB || (ObjectA->GetClass() != ObjectB->GetClass()))
		{
			return false;
		}

		bool Result = true;
		for (UProperty* Prop = ObjectA->GetClass()->PropertyLink; Prop && Result; Prop = Prop->PropertyLinkNext)
		{
			bool bConsiderProperty = Prop->ShouldDuplicateValue(); //Should the property be compared at all?
			if (bConsiderProperty)
			{
				for (int32 Idx = 0; (Idx < Prop->ArrayDim) && Result; Idx++)
				{
					if (!Prop->Identical_InContainer(ObjectA, ObjectB, Idx, PPF_DeepComparison))
					{
						Result = false;
					}
				}
			}
		}
		if (Result)
		{
			// Allow the component to compare its native/ intrinsic properties.
			Result = ObjectA->AreNativePropertiesIdenticalTo(ObjectB);
		}
		return Result;
	}
};

bool UInheritableComponentHandler::IsRecordNecessary(const FComponentOverrideRecord& Record) const
{
	auto ChildComponentTemplate = Record.ComponentTemplate;
	auto ParentComponentTemplate = FindBestArchetype(Record.ComponentKey);
	check(ChildComponentTemplate && ParentComponentTemplate && (ParentComponentTemplate != ChildComponentTemplate));
	return !FComponentComparisonHelper::AreIdentical(ChildComponentTemplate, ParentComponentTemplate);
}

UActorComponent* UInheritableComponentHandler::FindBestArchetype(FComponentKey Key) const
{
	UActorComponent* ClosestArchetype = nullptr;

	auto ActualBPGC = Cast<UBlueprintGeneratedClass>(GetOuter());
	if (ActualBPGC && Key.GetComponentOwner() && (ActualBPGC != Key.GetComponentOwner()))
	{
		ActualBPGC = Cast<UBlueprintGeneratedClass>(ActualBPGC->GetSuperClass());
		while (!ClosestArchetype && ActualBPGC)
		{
			if (ActualBPGC->InheritableComponentHandler)
			{
				ClosestArchetype = ActualBPGC->InheritableComponentHandler->GetOverridenComponentTemplate(Key);
			}
			ActualBPGC = Cast<UBlueprintGeneratedClass>(ActualBPGC->GetSuperClass());
		}

		if (!ClosestArchetype)
		{
			ClosestArchetype = Key.GetOriginalTemplate();
		}
	}

	return ClosestArchetype;
}

bool UInheritableComponentHandler::RefreshTemplateName(FComponentKey OldKey)
{
	for (auto& Record : Records)
	{
		if (Record.ComponentKey.Match(OldKey))
		{
			Record.ComponentKey.RefreshVariableName();
			return true;
		}
	}
	return false;
}

FComponentKey UInheritableComponentHandler::FindKey(UActorComponent* ComponentTemplate) const
{
	for (auto& Record : Records)
	{
		if (Record.ComponentTemplate == ComponentTemplate)
		{
			return Record.ComponentKey;
		}
	}
	return FComponentKey();
}

#endif

void UInheritableComponentHandler::PreloadAllTempates()
{
	for (auto Record : Records)
	{
		if (Record.ComponentTemplate && Record.ComponentTemplate->HasAllFlags(RF_NeedLoad))
		{
			auto Linker = Record.ComponentTemplate->GetLinker();
			if (Linker)
			{
				Linker->Preload(Record.ComponentTemplate);
			}
		}
	}
}

void UInheritableComponentHandler::PreloadAll()
{
	if (HasAllFlags(RF_NeedLoad))
	{
		auto Linker = GetLinker();
		if (Linker)
		{
			Linker->Preload(this);
		}
	}
	PreloadAllTempates();
}

FComponentKey UInheritableComponentHandler::FindKey(const FName VariableName) const
{
	for (const FComponentOverrideRecord& Record : Records)
	{
		if (Record.ComponentKey.GetSCSVariableName() == VariableName || (Record.ComponentTemplate && Record.ComponentTemplate->GetFName() == VariableName))
		{
			return Record.ComponentKey;
		}
	}
	return FComponentKey();
}

UActorComponent* UInheritableComponentHandler::GetOverridenComponentTemplate(FComponentKey Key) const
{
	auto Record = FindRecord(Key);
	return Record ? Record->ComponentTemplate : nullptr;
}

const FComponentOverrideRecord* UInheritableComponentHandler::FindRecord(const FComponentKey Key) const
{
	for (auto& Record : Records)
	{
		if (Record.ComponentKey.Match(Key))
		{
			return &Record;
		}
	}
	return nullptr;
}

// FComponentOverrideRecord

FComponentKey::FComponentKey(const USCS_Node* SCSNode) : OwnerClass(nullptr)
{
	if (SCSNode)
	{
		const USimpleConstructionScript* ParentSCS = SCSNode->GetSCS();
		OwnerClass      = ParentSCS ? Cast<UBlueprintGeneratedClass>(ParentSCS->GetOwnerClass()) : nullptr;
		AssociatedGuid  = SCSNode->VariableGuid;
		SCSVariableName = SCSNode->GetVariableName();
	}
}

#if WITH_EDITOR
FComponentKey::FComponentKey(UBlueprint* Blueprint, const FUCSComponentId& UCSComponentID)
{
	OwnerClass     = CastChecked<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
	AssociatedGuid = UCSComponentID.GetAssociatedGuid();
}
#endif // WITH_EDITOR

bool FComponentKey::Match(const FComponentKey OtherKey) const
{
	return (OwnerClass == OtherKey.OwnerClass) && (AssociatedGuid == OtherKey.AssociatedGuid);
}

USCS_Node* FComponentKey::FindSCSNode() const
{
	USimpleConstructionScript* ParentSCS = (OwnerClass && IsSCSKey()) ? OwnerClass->SimpleConstructionScript : nullptr;
	return ParentSCS ? ParentSCS->FindSCSNodeByGuid(AssociatedGuid) : nullptr;
}

UActorComponent* FComponentKey::GetOriginalTemplate() const
{
	UActorComponent* ComponentTemplate = nullptr;
	if (IsSCSKey())
	{
		if (USCS_Node* SCSNode = FindSCSNode())
		{
			ComponentTemplate = SCSNode->ComponentTemplate;
		}
	}
#if WITH_EDITOR
	else if (IsUCSKey())
	{
		ComponentTemplate = FBlueprintEditorUtils::FindUCSComponentTemplate(*this);
	}
#endif // WITH_EDITOR
	return ComponentTemplate;
}

bool FComponentKey::RefreshVariableName()
{
	if (IsValid() && IsSCSKey())
	{
		USCS_Node* SCSNode = FindSCSNode();
		const FName UpdatedName = SCSNode ? SCSNode->GetVariableName() : NAME_None;

		if (UpdatedName != SCSVariableName)
		{
			SCSVariableName = UpdatedName;
			return true;
		}
	}
	return false;
}
