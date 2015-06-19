// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "K2Node_CallArrayFunction.h"

UK2Node_CallArrayFunction::UK2Node_CallArrayFunction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_CallArrayFunction::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* TargetArrayPin = GetTargetArrayPin();
	check(TargetArrayPin);
	TargetArrayPin->PinType.bIsArray = true;
	TargetArrayPin->PinType.bIsReference = true;
	TargetArrayPin->PinType.PinCategory = Schema->PC_Wildcard;
	TargetArrayPin->PinType.PinSubCategory = TEXT("");
	TargetArrayPin->PinType.PinSubCategoryObject = NULL;

	TArray< FArrayPropertyPinCombo > ArrayPins;
	GetArrayPins(ArrayPins);
	for(auto Iter = ArrayPins.CreateConstIterator(); Iter; ++Iter)
	{
		if(Iter->ArrayPropPin)
		{
			Iter->ArrayPropPin->bHidden = true;
			Iter->ArrayPropPin->bNotConnectable = true;
			Iter->ArrayPropPin->bDefaultValueIsReadOnly = true;
		}
	}

	PropagateArrayTypeInfo(TargetArrayPin);
}

void UK2Node_CallArrayFunction::PostReconstructNode()
{
	// cannot use a ranged for here, as PinConnectionListChanged() might end up 
	// collapsing split pins (subtracting elements from Pins)
	for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* Pin = Pins[PinIndex];
		if (Pin->LinkedTo.Num() > 0)
		{
			PinConnectionListChanged(Pin);
		}
	}
}

void UK2Node_CallArrayFunction::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	TArray<UEdGraphPin*> PinsToCheck;
	GetArrayTypeDependentPins(PinsToCheck);

	for (int32 Index = 0; Index < PinsToCheck.Num(); ++Index)
	{
		UEdGraphPin* PinToCheck = PinsToCheck[Index];
		if (PinToCheck->SubPins.Num() > 0)
		{
			PinsToCheck.Append(PinToCheck->SubPins);
		}
	}

	PinsToCheck.Add(GetTargetArrayPin());

	if (PinsToCheck.Contains(Pin))
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		bool bNeedToPropagate = false;

		if( Pin->LinkedTo.Num() > 0 )
		{
			if (Pin->PinType.PinCategory == Schema->PC_Wildcard)
			{
				UEdGraphPin* LinkedTo = Pin->LinkedTo[0];
				check(LinkedTo);
				check(Pin->PinType.bIsArray == LinkedTo->PinType.bIsArray);

				Pin->PinType.PinCategory = LinkedTo->PinType.PinCategory;
				Pin->PinType.PinSubCategory = LinkedTo->PinType.PinSubCategory;
				Pin->PinType.PinSubCategoryObject = LinkedTo->PinType.PinSubCategoryObject;

				bNeedToPropagate = true;
			}
		}
		else
		{
			bNeedToPropagate = true;

			for (UEdGraphPin* PinToCheck : PinsToCheck)
			{
				if (PinToCheck->LinkedTo.Num() > 0)
				{
					bNeedToPropagate = false;
					break;
				}
			}

			if (bNeedToPropagate)
			{
				Pin->PinType.PinCategory = Schema->PC_Wildcard;
				Pin->PinType.PinSubCategory = TEXT("");
				Pin->PinType.PinSubCategoryObject = NULL;
			}
		}

		if (bNeedToPropagate)
		{
			PropagateArrayTypeInfo(Pin);
		}
	}
}

UEdGraphPin* UK2Node_CallArrayFunction::GetTargetArrayPin() const
{
	TArray< FArrayPropertyPinCombo > ArrayParmPins;

	GetArrayPins(ArrayParmPins);

	if(ArrayParmPins.Num())
	{
		return ArrayParmPins[0].ArrayPin;
	}
	return nullptr;
}

void UK2Node_CallArrayFunction::GetArrayPins(TArray< FArrayPropertyPinCombo >& OutArrayPinInfo ) const
{
	OutArrayPinInfo.Empty();

	UFunction* TargetFunction = GetTargetFunction();
	check(TargetFunction);
	FString ArrayPointerMetaData = TargetFunction->GetMetaData(FBlueprintMetadata::MD_ArrayParam);
	TArray<FString> ArrayPinComboNames;
	ArrayPointerMetaData.ParseIntoArray(ArrayPinComboNames, TEXT(","), true);

	for(auto Iter = ArrayPinComboNames.CreateConstIterator(); Iter; ++Iter)
	{
		TArray<FString> ArrayPinNames;
		Iter->ParseIntoArray(ArrayPinNames, TEXT("|"), true);

		FArrayPropertyPinCombo ArrayInfo;
		ArrayInfo.ArrayPin = FindPin(ArrayPinNames[0]);
		if(ArrayPinNames.Num() > 1)
		{
			ArrayInfo.ArrayPropPin = FindPin(ArrayPinNames[1]);
		}

		if(ArrayInfo.ArrayPin)
		{
			OutArrayPinInfo.Add(ArrayInfo);
		}
	}
}

bool UK2Node_CallArrayFunction::IsWildcardProperty(UFunction* InArrayFunction, const UProperty* InProperty)
{
	if(InArrayFunction && InProperty)
	{
		FString ArrayPointerMetaData = InArrayFunction->GetMetaData(FBlueprintMetadata::MD_ArrayParam);
		TArray<FString> ArrayPinComboNames;
		ArrayPointerMetaData.ParseIntoArray(ArrayPinComboNames, TEXT(","), true);

		for(auto Iter = ArrayPinComboNames.CreateConstIterator(); Iter; ++Iter)
		{
			TArray<FString> ArrayPinNames;
			Iter->ParseIntoArray(ArrayPinNames, TEXT("|"), true);

			if(ArrayPinNames[0] == InProperty->GetName())
			{
				return true;
			}
		}
	}
	return false;
}

void UK2Node_CallArrayFunction::GetArrayTypeDependentPins(TArray<UEdGraphPin*>& OutPins) const
{
	OutPins.Empty();

	UFunction* TargetFunction = GetTargetFunction();
	check(TargetFunction);

	const FString DependentPinMetaData = TargetFunction->GetMetaData(FBlueprintMetadata::MD_ArrayDependentParam);
	TArray<FString> TypeDependentPinNames;
	DependentPinMetaData.ParseIntoArray(TypeDependentPinNames, TEXT(","), true);

	for(TArray<UEdGraphPin*>::TConstIterator it(Pins); it; ++it)
	{
		UEdGraphPin* CurrentPin = *it;
		int32 ItemIndex = 0;
		if( CurrentPin && TypeDependentPinNames.Find(CurrentPin->PinName, ItemIndex) )
		{
			OutPins.Add(CurrentPin);
		}
	}
}

void UK2Node_CallArrayFunction::PropagateArrayTypeInfo(const UEdGraphPin* SourcePin)
{
	if( SourcePin )
	{
		const UEdGraphSchema_K2* Schema = CastChecked<UEdGraphSchema_K2>(GetSchema());
		const FEdGraphPinType& SourcePinType = SourcePin->PinType;

		TArray<UEdGraphPin*> DependentPins;
		GetArrayTypeDependentPins(DependentPins);
		DependentPins.Add(GetTargetArrayPin());
	
		// Propagate pin type info (except for array info!) to pins with dependent types
		for (UEdGraphPin* CurrentPin : DependentPins)
		{
			if (CurrentPin != SourcePin)
			{
				FEdGraphPinType& CurrentPinType = CurrentPin->PinType;

				bool const bHasTypeMismatch = (CurrentPinType.PinCategory != SourcePinType.PinCategory) ||
					(CurrentPinType.PinSubCategory != SourcePinType.PinSubCategory) ||
					(CurrentPinType.PinSubCategoryObject != SourcePinType.PinSubCategoryObject);

				if (!bHasTypeMismatch)
				{
					continue;
				}

				if (CurrentPin->SubPins.Num() > 0)
				{
					Schema->RecombinePin(CurrentPin->SubPins[0]);
				}

				CurrentPinType.PinCategory          = SourcePinType.PinCategory;
				CurrentPinType.PinSubCategory       = SourcePinType.PinSubCategory;
				CurrentPinType.PinSubCategoryObject = SourcePinType.PinSubCategoryObject;

				// Reset default values
				if (!Schema->IsPinDefaultValid(CurrentPin, CurrentPin->DefaultValue, CurrentPin->DefaultObject, CurrentPin->DefaultTextValue).IsEmpty())
				{
					CurrentPin->ResetDefaultValue();
				}

				// Verify that all previous connections to this pin are still valid with the new type
				for (TArray<UEdGraphPin*>::TIterator ConnectionIt(CurrentPin->LinkedTo); ConnectionIt; ++ConnectionIt)
				{
					UEdGraphPin* ConnectedPin = *ConnectionIt;
					if (!Schema->ArePinsCompatible(CurrentPin, ConnectedPin))
					{
						CurrentPin->BreakLinkTo(ConnectedPin);
					}
				}
			}
		}
	}
}
