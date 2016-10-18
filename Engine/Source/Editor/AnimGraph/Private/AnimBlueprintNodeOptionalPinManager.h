// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "K2Node.h"

struct FAnimBlueprintNodeOptionalPinManager : public FOptionalPinManager
{
protected:
	class UAnimGraphNode_Base* BaseNode;
	TArray<UEdGraphPin*>* OldPins;

	TMap<FString, UEdGraphPin*> OldPinMap;

public:
	FAnimBlueprintNodeOptionalPinManager(class UAnimGraphNode_Base* Node, TArray<UEdGraphPin*>* InOldPins);

	/** FOptionalPinManager interface */
	virtual void GetRecordDefaults(UProperty* TestProperty, FOptionalPinFromProperty& Record) const override;
	virtual void CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex, UProperty* Property) const override;
	virtual void PostInitNewPin(UEdGraphPin* Pin, FOptionalPinFromProperty& Record, int32 ArrayIndex, UProperty* Property, uint8* PropertyAddress) const override;
	virtual void PostRemovedOldPin(FOptionalPinFromProperty& Record, int32 ArrayIndex, UProperty* Property, uint8* PropertyAddress) const override;

	void AllocateDefaultPins(UStruct* SourceStruct, uint8* StructBasePtr);
};