// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "EnumEditorUtils.h"
#include "ScopedTransaction.h"
#include "BlueprintGraphDefinitions.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/UserDefinedEnum.h"

#define LOCTEXT_NAMESPACE "Enum"

struct FEnumEditorUtilsHelper
{
	 static const TCHAR* DisplayName() { return TEXT("DisplayName"); }
	 static const TCHAR* InvalidName() { return TEXT("(INVALID)"); }
};

//////////////////////////////////////////////////////////////////////////
// FEnumEditorManager
FEnumEditorUtils::FEnumEditorManager& FEnumEditorUtils::FEnumEditorManager::Get()
{
	static TSharedRef< FEnumEditorManager > EnumEditorManager( new FEnumEditorManager() );
	return *EnumEditorManager;
}

//////////////////////////////////////////////////////////////////////////
// User defined enumerations

UEnum* FEnumEditorUtils::CreateUserDefinedEnum(UObject* InParent, FName EnumName, EObjectFlags Flags)
{
	ensure(0 != (RF_Public & Flags));

	UEnum* Enum = NewObject<UUserDefinedEnum>(InParent, EnumName, Flags);

	if (NULL != Enum)
	{
		TArray<TPair<FName, uint8>> EmptyNames;
		Enum->SetEnums(EmptyNames, UEnum::ECppForm::Namespaced);
		Enum->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
	}

	return Enum;
}

bool FEnumEditorUtils::IsNameAvailebleForUserDefinedEnum(FName Name)
{
	return true;
}

void FEnumEditorUtils::UpdateAfterPathChanged(UEnum* Enum)
{
	check(NULL != Enum);

	TArray<TPair<FName, uint8>> NewEnumeratorsNames;
	const int32 EnumeratorsToCopy = Enum->NumEnums() - 1; // skip _MAX
	for (int32 Index = 0; Index < EnumeratorsToCopy; Index++)
	{
		const FString ShortName = Enum->GetEnumName(Index);
		const FString NewFullName = Enum->GenerateFullEnumName(*ShortName);
		NewEnumeratorsNames.Add(TPairInitializer<FName, uint8>(FName(*NewFullName), Index));
	}

	Enum->SetEnums(NewEnumeratorsNames, UEnum::ECppForm::Namespaced);
}

//////////////////////////////////////////////////////////////////////////
// Enumerators

void FEnumEditorUtils::CopyEnumeratorsWithoutMax(const UEnum* Enum, TArray<TPair<FName, uint8>>& OutEnumNames)
{
	if (Enum == nullptr)
	{
		return;
	}

	const int32 EnumeratorsToCopy = Enum->NumEnums() - 1;
	for	(int32 Index = 0; Index < EnumeratorsToCopy; Index++)
	{
		OutEnumNames.Add(TPairInitializer<FName, int8>(Enum->GetNameByIndex(Index), Enum->GetValueByIndex(Index)));
	}
}

/** adds new enumerator (with default unique name) for user defined enum */
void FEnumEditorUtils::AddNewEnumeratorForUserDefinedEnum(UUserDefinedEnum* Enum)
{
	if (!Enum)
	{
		return;
	}

	PrepareForChange(Enum);

	TArray<TPair<FName, uint8>> OldNames, Names;
	CopyEnumeratorsWithoutMax(Enum, OldNames);
	Names = OldNames;

	FString EnumNameString = Enum->GenerateNewEnumeratorName();
	const FString FullNameStr = Enum->GenerateFullEnumName(*EnumNameString);
	Names.Add(TPairInitializer<FName, uint8>(FName(*FullNameStr), Enum->GetMaxEnumValue()));

	// Clean up enum values.
	for (int32 i = 0; i < Names.Num(); ++i)
	{
		Names[i].Value = i;
	}
	const UEnum::ECppForm EnumType = Enum->GetCppForm();
	Enum->SetEnums(Names, EnumType);
	EnsureAllDisplayNamesExist(Enum);

	BroadcastChanges(Enum, OldNames);
		
	Enum->MarkPackageDirty();
}

void FEnumEditorUtils::RemoveEnumeratorFromUserDefinedEnum(UUserDefinedEnum* Enum, int32 EnumeratorIndex)
{
	if (!(Enum && (Enum->GetNameByIndex(EnumeratorIndex) != NAME_None)))
	{
		return;
	}

	PrepareForChange(Enum);

	TArray<TPair<FName, uint8>> OldNames, Names;
	CopyEnumeratorsWithoutMax(Enum, OldNames);
	Names = OldNames;

	Names.RemoveAt(EnumeratorIndex);

	Enum->RemoveMetaData(FEnumEditorUtilsHelper::DisplayName(), EnumeratorIndex);

	// Clean up enum values.
	for (int32 i = 0; i < Names.Num(); ++i)
	{
		Names[i].Value = i;
	}
	const UEnum::ECppForm EnumType = Enum->GetCppForm();
	Enum->SetEnums(Names, EnumType);
	EnsureAllDisplayNamesExist(Enum);
	BroadcastChanges(Enum, OldNames);

	Enum->MarkPackageDirty();
}

bool FEnumEditorUtils::IsEnumeratorBitflagsType(UUserDefinedEnum* Enum)
{
	return Enum && Enum->HasMetaData(*FBlueprintMetadata::MD_Bitflags.ToString());
}

void FEnumEditorUtils::SetEnumeratorBitflagsTypeState(UUserDefinedEnum* Enum, bool bBitflagsType)
{
	if (Enum)
	{
		PrepareForChange(Enum);

		if (bBitflagsType)
		{
			Enum->SetMetaData(*FBlueprintMetadata::MD_Bitflags.ToString(), TEXT(""));
		}
		else
		{
			Enum->RemoveMetaData(*FBlueprintMetadata::MD_Bitflags.ToString());
		}

		TArray<TPair<FName, uint8>> Names;
		CopyEnumeratorsWithoutMax(Enum, Names);
		BroadcastChanges(Enum, Names);

		Enum->MarkPackageDirty();
	}
}

/** Reorder enumerators in enum. Swap an enumerator with given name, with previous or next (based on bDirectionUp parameter) */
void FEnumEditorUtils::MoveEnumeratorInUserDefinedEnum(UUserDefinedEnum* Enum, int32 EnumeratorIndex, bool bDirectionUp)
{
	if (!(Enum && (Enum->GetNameByIndex(EnumeratorIndex) != NAME_None)))
	{
		return;
	}

	PrepareForChange(Enum);

	TArray<TPair<FName, uint8>> OldNames, Names;
	CopyEnumeratorsWithoutMax(Enum, OldNames);
	Names = OldNames;

	const bool bIsLast = ((Names.Num() - 1) == EnumeratorIndex);
	const bool bIsFirst = (0 == EnumeratorIndex);

	if (bDirectionUp && !bIsFirst)
	{
		Names.Swap(EnumeratorIndex, EnumeratorIndex - 1);
	}
	else if (!bDirectionUp && !bIsLast)
	{
		Names.Swap(EnumeratorIndex, EnumeratorIndex + 1);
	}

	// Clean up enum values.
	for (int32 i = 0; i < Names.Num(); ++i)
	{
		Names[i].Value = i;
	}
	const UEnum::ECppForm EnumType = Enum->GetCppForm();
	Enum->SetEnums(Names, EnumType);
	EnsureAllDisplayNamesExist(Enum);
	BroadcastChanges(Enum, OldNames);

	Enum->MarkPackageDirty();
}

bool FEnumEditorUtils::IsProperNameForUserDefinedEnumerator(const UEnum* Enum, FString NewName)
{
	if (Enum && !UEnum::IsFullEnumName(*NewName))
	{
		check(Enum->GetFName().IsValidXName());

		const FName ShortName(*NewName);
		const bool bValidName = ShortName.IsValidXName(INVALID_OBJECTNAME_CHARACTERS);

		const FString TrueNameStr = Enum->GenerateFullEnumName(*NewName);
		const FName TrueName(*TrueNameStr);
		check(!bValidName || TrueName.IsValidXName());

		const bool bNameNotUsed = (INDEX_NONE == Enum->FindEnumIndex(TrueName));
		return bNameNotUsed && bValidName;
	}
	return false;
}

class FArchiveEnumeratorResolver : public FArchiveUObject
{
public:
	const UEnum* Enum;
	const TArray<TPair<FName, uint8>>& OldNames;

	FArchiveEnumeratorResolver(const UEnum* InEnum, const TArray<TPair<FName, uint8>>& InOldNames)
		: FArchiveUObject(), Enum(InEnum), OldNames(InOldNames)
	{
	}

	virtual bool UseToResolveEnumerators() const override
	{
		return true;
	}
};

void FEnumEditorUtils::PrepareForChange(const UUserDefinedEnum* Enum)
{
	FEnumEditorManager::Get().PreChange(Enum, EEnumEditorChangeInfo::Changed);
}

void FEnumEditorUtils::BroadcastChanges(const UUserDefinedEnum* Enum, const TArray<TPair<FName, uint8>>& OldNames, bool bResolveData)
{
	check(NULL != Enum);
	if (bResolveData)
	{
		FArchiveEnumeratorResolver EnumeratorResolver(Enum, OldNames);

		TArray<UClass*> ClassesToCheck;
		for (TObjectIterator<UByteProperty> PropertyIter; PropertyIter; ++PropertyIter)
		{
			const UByteProperty* ByteProperty = *PropertyIter;
			if (ByteProperty && (Enum == ByteProperty->GetIntPropertyEnum()))
			{
				UClass* OwnerClass = ByteProperty->GetOwnerClass();
				if (OwnerClass)
				{
					ClassesToCheck.Add(OwnerClass);
				}
			}
		}

		for (FObjectIterator ObjIter; ObjIter; ++ObjIter)
		{
			for (auto ClassIter = ClassesToCheck.CreateConstIterator(); ClassIter; ++ClassIter)
			{
				if (ObjIter->IsA(*ClassIter))
				{
					ObjIter->Serialize(EnumeratorResolver);
					break;
				}
			}
		}
	}

	struct FNodeValidatorHelper
	{
		static bool IsValid(UK2Node* Node)
		{
			return Node
				&& (NULL != Cast<UEdGraph>(Node->GetOuter()))
				&& !Node->HasAnyFlags(RF_Transient) && !Node->IsPendingKill();
		}
	};

	TSet<UBlueprint*> BlueprintsToRefresh;

	{
		//CUSTOM NODES DEPENTENT ON ENUM

		for (TObjectIterator<UK2Node> It(RF_Transient); It; ++It)
		{
			UK2Node* Node = *It;
			INodeDependingOnEnumInterface* NodeDependingOnEnum = Cast<INodeDependingOnEnumInterface>(Node);
			if (FNodeValidatorHelper::IsValid(Node) && NodeDependingOnEnum && (Enum == NodeDependingOnEnum->GetEnum()))
			{
				if (UBlueprint* Blueprint = Node->GetBlueprint())
				{
					if (NodeDependingOnEnum->ShouldBeReconstructedAfterEnumChanged())
					{
						Node->ReconstructNode();
					}
					BlueprintsToRefresh.Add(Blueprint);
				}
			}
		}
	}

	for (TObjectIterator<UEdGraphNode> It(RF_Transient); It; ++It)
	{
		for (UEdGraphPin* Pin : It->Pins)
		{
			if (Pin && (Pin->PinType.PinSubCategory != UEdGraphSchema_K2::PSC_Bitmask) && (Enum == Pin->PinType.PinSubCategoryObject.Get()) && (EEdGraphPinDirection::EGPD_Input == Pin->Direction))
			{
				UK2Node* Node = Cast<UK2Node>(Pin->GetOuter());
				if (FNodeValidatorHelper::IsValid(Node))
				{
					if (UBlueprint* Blueprint = Node->GetBlueprint())
					{
						if (INDEX_NONE == Enum->FindEnumIndex(*Pin->DefaultValue))
						{
							Pin->Modify();
							if (Blueprint->BlueprintType == BPTYPE_Interface)
							{
								Pin->DefaultValue = Enum->GetEnumName(0);
							}
							else
							{
								Pin->DefaultValue = FEnumEditorUtilsHelper::InvalidName();
							}
							Node->PinDefaultValueChanged(Pin);
							BlueprintsToRefresh.Add(Blueprint);
						}
					}
				}
			}
		}
	}

	// Modify any properties that are using the enum as a bitflags type for bitmask values inside a Blueprint class.
	for (TObjectIterator<UIntProperty> PropertyIter; PropertyIter; ++PropertyIter)
	{
		const UIntProperty* IntProperty = *PropertyIter;
		if (IntProperty && IntProperty->HasMetaData(*FBlueprintMetadata::MD_Bitmask.ToString()))
		{
			UClass* OwnerClass = IntProperty->GetOwnerClass();
			if (OwnerClass)
			{
				// Note: We only need to consider the skeleton class here.
				UBlueprint* Blueprint = Cast<UBlueprint>(OwnerClass->ClassGeneratedBy);
				if (Blueprint && OwnerClass == Blueprint->SkeletonGeneratedClass)
				{
					const FString& BitmaskEnumName = IntProperty->GetMetaData(FBlueprintMetadata::MD_BitmaskEnum);
					if (BitmaskEnumName == Enum->GetName() && !Enum->HasMetaData(*FBlueprintMetadata::MD_Bitflags.ToString()))
					{
						FName VarName = IntProperty->GetFName();

						// This will remove the metadata key from both the skeleton & full class.
						FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(Blueprint, VarName, nullptr, FBlueprintMetadata::MD_BitmaskEnum);

						// Need to reassign the property since the skeleton class will have been regenerated at this point.
						IntProperty = FindFieldChecked<UIntProperty>(Blueprint->SkeletonGeneratedClass, VarName);

						// Reconstruct any nodes that reference the variable that was just modified.
						for (TObjectIterator<UK2Node_Variable> VarNodeIt; VarNodeIt; ++VarNodeIt)
						{
							UK2Node_Variable* VarNode = *VarNodeIt;
							if (VarNode && VarNode->GetPropertyForVariable() == IntProperty)
							{
								VarNode->ReconstructNode();

								BlueprintsToRefresh.Add(VarNode->GetBlueprint());
							}
						}

						BlueprintsToRefresh.Add(Blueprint);
					}
				}
			}
		}
	}

	for (auto It = BlueprintsToRefresh.CreateIterator(); It; ++It)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(*It);
		(*It)->BroadcastChanged();
	}

	FEnumEditorManager::Get().PostChange(Enum, EEnumEditorChangeInfo::Changed);
}

int32 FEnumEditorUtils::ResolveEnumerator(const UEnum* Enum, FArchive& Ar, int32 EnumeratorValue)
{
	check(Ar.UseToResolveEnumerators());
	const FArchiveEnumeratorResolver* EnumeratorResolver = (FArchiveEnumeratorResolver*)(&Ar);
	if(Enum == EnumeratorResolver->Enum)
	{
		for (TPair<FName, uint8> OldName : EnumeratorResolver->OldNames)
		{
			if (OldName.Value == EnumeratorValue)
			{
				const FName EnumeratorName = OldName.Key;
				const int32 NewEnumValue = Enum->GetValueByName(EnumeratorName);
				if(INDEX_NONE != NewEnumValue)
				{
					return NewEnumValue;
				}
			}
		}
		return Enum->GetMaxEnumValue();
	}
	return EnumeratorValue;
}

FString FEnumEditorUtils::GetEnumeratorDisplayName(const UUserDefinedEnum* Enum, const int32 EnumeratorIndex)
{
	if (Enum && Enum->DisplayNames.IsValidIndex(EnumeratorIndex))
	{
		return Enum->DisplayNames[EnumeratorIndex].ToString();
	}
	return FString();
}

bool FEnumEditorUtils::SetEnumeratorDisplayName(UUserDefinedEnum* Enum, int32 EnumeratorIndex, FString NewDisplayName)
{
	if (Enum && (EnumeratorIndex >= 0) && (EnumeratorIndex < Enum->NumEnums()))
	{
		if (IsEnumeratorDisplayNameValid(Enum, NewDisplayName))
		{
			PrepareForChange(Enum);
			Enum->SetMetaData(FEnumEditorUtilsHelper::DisplayName(), *NewDisplayName, EnumeratorIndex);
			EnsureAllDisplayNamesExist(Enum);
			BroadcastChanges(Enum, TArray<TPair<FName, uint8>>(), false);
			return true;
		}
	}
	return false;
}

bool FEnumEditorUtils::IsEnumeratorDisplayNameValid(const UUserDefinedEnum* Enum, const FString NewDisplayName)
{
	if (!Enum || 
		NewDisplayName.IsEmpty() || 
		(NewDisplayName == FEnumEditorUtilsHelper::InvalidName()))
	{
		return false;
	}

	for	(int32 Index = 0; Index < Enum->NumEnums(); Index++)
	{
		if (NewDisplayName == GetEnumeratorDisplayName(Enum, Index))
		{
			return false;
		}
	}

	return true;
}

void FEnumEditorUtils::EnsureAllDisplayNamesExist(UUserDefinedEnum* Enum)
{
	if (Enum)
	{
		const int32 EnumeratorsToEnsure = (Enum->NumEnums() > 0) ? (Enum->NumEnums() - 1) : 0;
		if (Enum->DisplayNames.Num() != EnumeratorsToEnsure)
		{
			Enum->DisplayNames.Empty(EnumeratorsToEnsure);
		}
		for	(int32 Index = 0; Index < EnumeratorsToEnsure; Index++)
		{
			FText DisplayNameMetaData = Enum->UEnum::GetEnumText(Index);
			if (DisplayNameMetaData.IsEmpty())
			{
				const FString EnumName = Enum->GetEnumName(Index);
				DisplayNameMetaData = FText::FromString(EnumName);
				for(int32 AddIndex = 0; !IsEnumeratorDisplayNameValid(Enum, DisplayNameMetaData.ToString()); ++AddIndex)
				{
					DisplayNameMetaData = FText::FromString(FString::Printf(TEXT("%s%d"), *EnumName, AddIndex));
				}
				Enum->SetMetaData(FEnumEditorUtilsHelper::DisplayName(), *DisplayNameMetaData.ToString(), Index);
			}
			if (!Enum->DisplayNames.IsValidIndex(Index))
			{
				Enum->DisplayNames.Add(DisplayNameMetaData);
			}
			else if (!Enum->DisplayNames[Index].EqualTo(DisplayNameMetaData))
			{
				Enum->DisplayNames[Index] = DisplayNameMetaData;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
