// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "Runtime/Engine/Classes/Engine/UserDefinedStruct.h"
#include "Editor/UnrealEd/Public/Kismet2/StructureEditorUtils.h"
#include "K2Node_StructOperation.h"

//////////////////////////////////////////////////////////////////////////
// UK2Node_StructOperation

UK2Node_StructOperation::UK2Node_StructOperation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
bool UK2Node_StructOperation::HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const
{
	const bool bResult = nullptr != StructType;
	if (bResult && OptionalOutput)
	{
		OptionalOutput->AddUnique(StructType);
	}

	const bool bSuperResult = Super::HasExternalDependencies(OptionalOutput);
	return bSuperResult || bResult;
}

void UK2Node_StructOperation::FStructOperationOptionalPinManager::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex, UProperty* Property) const
{
	FOptionalPinManager::CustomizePinData(Pin, SourcePropertyName, ArrayIndex, Property);

	if (Pin && Property)
	{
		const auto UDStructure = Cast<const UUserDefinedStruct>(Property->GetOwnerStruct());
		if (UDStructure)
		{
			const auto VarDesc = FStructureEditorUtils::GetVarDesc(UDStructure).FindByPredicate(
				FStructureEditorUtils::FFindByNameHelper<FStructVariableDescription>(Property->GetFName()));
			if (VarDesc)
			{
				Pin->PersistentGuid = VarDesc->VarGuid;
			}
		}
	}
}

bool UK2Node_StructOperation::DoRenamedPinsMatch(const UEdGraphPin* NewPin, const UEdGraphPin* OldPin, bool bStructInVaraiablesOut)
{
	bool bResult = false;
	if (NewPin && OldPin && (OldPin->Direction == NewPin->Direction))
	{
		const EEdGraphPinDirection StructDirection = bStructInVaraiablesOut ? EGPD_Input : EGPD_Output;
		const EEdGraphPinDirection VariablesDirection = bStructInVaraiablesOut ? EGPD_Output : EGPD_Input;
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>(); 
		const bool bCompatible = K2Schema && K2Schema->ArePinTypesCompatible(NewPin->PinType, OldPin->PinType);

		if (bCompatible && (StructDirection == OldPin->Direction))
		{
			// Struct name was changed
			bResult = true;
		}
		else if (bCompatible && (VariablesDirection == OldPin->Direction))
		{
			// name of a member variable was changed
			if ((NewPin->PersistentGuid == OldPin->PersistentGuid) && OldPin->PersistentGuid.IsValid())
			{
				bResult = true;
			}
		}
	}
	return bResult;
}

FString UK2Node_StructOperation::GetPinMetaData(FString InPinName, FName InKey)
{
	FString ReturnValue;

	for (TFieldIterator<UProperty> It(StructType); It; ++It)
	{
		const UProperty* Property = *It;
		if(Property && Property->GetName() == InPinName)
		{
			ReturnValue = Property->GetMetaData(InKey);
			break;
		}
	}
	return ReturnValue;
}