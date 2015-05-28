// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node_Variable.h"
#include "K2Node_StructOperation.generated.h"

UCLASS(MinimalAPI, abstract)
class UK2Node_StructOperation : public UK2Node_Variable
{
	GENERATED_UCLASS_BODY()

	/** Class that this variable is defined in.  */
	UPROPERTY()
	UScriptStruct* StructType;

	// Begin UEdGraphNode interface
	virtual FString GetPinMetaData(FString InPinName, FName InKey) override;
	// End UEdGraphNode interface

	// UK2Node interface
	//virtual bool DrawNodeAsVariable() const override { return true; }
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override {}
	virtual bool HasExternalUserDefinedStructDependencies(TArray<class UStruct*>* OptionalOutput) const override;
	// End of UK2Node interface

protected:
	// Updater for subclasses that allow hiding pins
	struct FStructOperationOptionalPinManager : public FOptionalPinManager
	{
		// FOptionalPinsUpdater interface
		virtual void GetRecordDefaults(UProperty* TestProperty, FOptionalPinFromProperty& Record) const override
		{
			Record.bCanToggleVisibility = true;
			Record.bShowPin = true;
		}

		virtual void CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex, UProperty* Property) const override;
		// End of FOptionalPinsUpdater interfac
	};
#if WITH_EDITOR
	static bool DoRenamedPinsMatch(const UEdGraphPin* NewPin, const UEdGraphPin* OldPin, bool bStructInVaraiablesOut);
#endif
};

