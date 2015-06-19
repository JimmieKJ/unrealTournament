// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "AbilitySystemEditorPrivatePCH.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetArrayLibrary.h"
#include "AbilityTask.h"
#include "KismetCompiler.h"
#include "BlueprintEditorUtils.h"
#include "K2Node_LatentAbilityCall.h"
#include "GameplayAbilityGraphSchema.h"
#include "K2Node_EnumLiteral.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_LatentAbilityCall::UK2Node_LatentAbilityCall(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyActivateFunctionName = GET_FUNCTION_NAME_CHECKED(UAbilityTask, Activate);
}

bool UK2Node_LatentAbilityCall::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const
{
	return Super::CanCreateUnderSpecifiedSchema(DesiredSchema);
}

bool UK2Node_LatentAbilityCall::IsCompatibleWithGraph(UEdGraph const* TargetGraph) const
{
	bool bIsCompatible = false;
	
	EGraphType GraphType = TargetGraph->GetSchema()->GetGraphType(TargetGraph);
	bool const bAllowLatentFuncs = (GraphType == GT_Ubergraph || GraphType == GT_Macro);
	
	if (bAllowLatentFuncs)
	{
		UBlueprint* MyBlueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
		if (MyBlueprint && MyBlueprint->GeneratedClass)
		{
			if (MyBlueprint->GeneratedClass->IsChildOf(UGameplayAbility::StaticClass()))
			{
				bIsCompatible = true;
			}
		}
	}
	
	return bIsCompatible;
}

void UK2Node_LatentAbilityCall::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// these nested loops are combing over the same classes/functions the
	// FBlueprintActionDatabase does; ideally we save on perf and fold this in
	// with FBlueprintActionDatabase, but we want to keep the modules separate
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (!Class->IsChildOf<UAbilityTask>() || Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		
		for (TFieldIterator<UFunction> FuncIt(Class, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* Function = *FuncIt;
			if (!Function->HasAnyFunctionFlags(FUNC_Static))
			{
				continue;
			}

			// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
			// check to make sure that the registrar is looking for actions of this type
			// (could be regenerating actions for a specific asset, and therefore the 
			// registrar would only accept actions corresponding to that asset)
			if (!ActionRegistrar.IsOpenForRegistration(Function))
			{
				continue;
			}

			UObjectProperty* ReturnProperty = Cast<UObjectProperty>(Function->GetReturnProperty());
			// see if the function is a static factory method for online proxies
			bool const bIsProxyFactoryMethod = (ReturnProperty != nullptr) && ReturnProperty->PropertyClass->IsChildOf<UAbilityTask>();
			
			if (bIsProxyFactoryMethod)
			{
				UBlueprintNodeSpawner* NodeSpawner = UBlueprintFunctionNodeSpawner::Create(Function);
				check(NodeSpawner != nullptr);
				NodeSpawner->NodeClass = GetClass();
				
				auto CustomizeAcyncNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, TWeakObjectPtr<UFunction> FunctionPtr)
				{
					UK2Node_LatentAbilityCall* AsyncTaskNode = CastChecked<UK2Node_LatentAbilityCall>(NewNode);
					if (FunctionPtr.IsValid())
					{
						UFunction* Func = FunctionPtr.Get();
						UObjectProperty* ReturnProp = CastChecked<UObjectProperty>(Func->GetReturnProperty());
						
						AsyncTaskNode->ProxyFactoryFunctionName = Func->GetFName();
						AsyncTaskNode->ProxyFactoryClass        = Func->GetOuterUClass();
						AsyncTaskNode->ProxyClass               = ReturnProp->PropertyClass;
					}
				};
				
				TWeakObjectPtr<UFunction> FunctionPtr = Function;
				NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeAcyncNodeLambda, FunctionPtr);
				
				// @TODO: since this can't be folded into FBlueprintActionDatabase, we
				//        need a way to associate these spawners with a certain class
				ActionRegistrar.AddBlueprintAction(Function, NodeSpawner);
			}
		}
	}
}

// -------------------------------------------------

struct FK2Node_LatentAbilityCallHelper
{
	static FString WorldContextPinName;
	static FString ClassPinName;
	static FString BeginSpawnFuncName;
	static FString FinishSpawnFuncName;
	static FString BeginSpawnArrayFuncName;
	static FString FinishSpawnArrayFuncName;
	static FString SpawnedActorPinName;
};

FString FK2Node_LatentAbilityCallHelper::WorldContextPinName(TEXT("WorldContextObject"));
FString FK2Node_LatentAbilityCallHelper::ClassPinName(TEXT("Class"));
FString FK2Node_LatentAbilityCallHelper::BeginSpawnFuncName(TEXT("BeginSpawningActor"));
FString FK2Node_LatentAbilityCallHelper::FinishSpawnFuncName(TEXT("FinishSpawningActor"));
FString FK2Node_LatentAbilityCallHelper::BeginSpawnArrayFuncName(TEXT("BeginSpawningActorArray"));
FString FK2Node_LatentAbilityCallHelper::FinishSpawnArrayFuncName(TEXT("FinishSpawningActorArray"));
FString FK2Node_LatentAbilityCallHelper::SpawnedActorPinName(TEXT("SpawnedActor"));

// -------------------------------------------------

void UK2Node_LatentAbilityCall::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	AllocateDefaultPins();
	UClass* UseSpawnClass = GetClassToSpawn(&OldPins);
	if (UseSpawnClass != NULL)
	{
		CreatePinsForClass(UseSpawnClass);
	}
}

UEdGraphPin* UK2Node_LatentAbilityCall::GetClassPin(const TArray<UEdGraphPin*>* InPinsToSearch /*= NULL*/) const
{
	const TArray<UEdGraphPin*>* PinsToSearch = InPinsToSearch ? InPinsToSearch : &Pins;

	UEdGraphPin* Pin = NULL;
	for (auto PinIt = PinsToSearch->CreateConstIterator(); PinIt; ++PinIt)
	{
		UEdGraphPin* TestPin = *PinIt;
		if (TestPin && TestPin->PinName == FK2Node_LatentAbilityCallHelper::ClassPinName)
		{
			Pin = TestPin;
			break;
		}
	}
	check(Pin == NULL || Pin->Direction == EGPD_Input);
	return Pin;
}

UClass* UK2Node_LatentAbilityCall::GetClassToSpawn(const TArray<UEdGraphPin*>* InPinsToSearch) const
{
	UClass* UseSpawnClass = NULL;
	const TArray<UEdGraphPin*>* PinsToSearch = InPinsToSearch ? InPinsToSearch : &Pins;

	UEdGraphPin* ClassPin = GetClassPin(PinsToSearch);
	if (ClassPin && ClassPin->DefaultObject != NULL && ClassPin->LinkedTo.Num() == 0)
	{
		UseSpawnClass = CastChecked<UClass>(ClassPin->DefaultObject);
	}

	return UseSpawnClass;
}

void UK2Node_LatentAbilityCall::CreatePinsForClass(UClass* InClass)
{
	check(InClass != NULL);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	const UObject* const ClassDefaultObject = InClass->GetDefaultObject(false);

	SpawnParmPins.Empty();

	// Tasks can hide spawn parameters by doing meta = (HideSpawnParms="PropertyA,PropertyB")
	// (For example, hide Instigator in situations where instigator is not relevant to your task)
	
	TArray<FString> IgnorePropertyList;
	{
		UFunction* ProxyFunction = ProxyFactoryClass->FindFunctionByName(ProxyFactoryFunctionName);

		FString IgnorePropertyListStr = ProxyFunction->GetMetaData(FName(TEXT("HideSpawnParms")));
	
		if (!IgnorePropertyListStr.IsEmpty())
		{
			IgnorePropertyListStr.ParseIntoArray(IgnorePropertyList, TEXT(","), true);
		}
	}

	for (TFieldIterator<UProperty> PropertyIt(InClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* Property = *PropertyIt;
		UClass* PropertyClass = CastChecked<UClass>(Property->GetOuter());
		const bool bIsDelegate = Property->IsA(UMulticastDelegateProperty::StaticClass());
		const bool bIsExposedToSpawn = UEdGraphSchema_K2::IsPropertyExposedOnSpawn(Property);
		const bool bIsSettableExternally = !Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance);

		if (bIsExposedToSpawn &&
			!Property->HasAnyPropertyFlags(CPF_Parm) &&
			bIsSettableExternally &&
			Property->HasAllPropertyFlags(CPF_BlueprintVisible) &&
			!bIsDelegate && 
			!IgnorePropertyList.Contains(Property->GetName()) &&
			(FindPin(Property->GetName()) == nullptr) )
		{


			UEdGraphPin* Pin = CreatePin(EGPD_Input, TEXT(""), TEXT(""), NULL, false, false, Property->GetName());
			const bool bPinGood = (Pin != NULL) && K2Schema->ConvertPropertyToPinType(Property, /*out*/ Pin->PinType);
			SpawnParmPins.Add(Pin);

			if (ClassDefaultObject && Pin && K2Schema->PinDefaultValueIsEditable(*Pin))
			{
				FString DefaultValueAsString;
				const bool bDefaultValueSet = FBlueprintEditorUtils::PropertyValueToString(Property, reinterpret_cast<const uint8*>(ClassDefaultObject), DefaultValueAsString);
				check(bDefaultValueSet);
				K2Schema->TrySetDefaultValue(*Pin, DefaultValueAsString);
			}

			// Copy tooltip from the property.
			if (Pin != nullptr)
			{
				K2Schema->ConstructBasicPinTooltip(*Pin, Property->GetToolTipText(), Pin->PinToolTip);
			}
		}
	}
}

void UK2Node_LatentAbilityCall::PinDefaultValueChanged(UEdGraphPin* ChangedPin)
{
	if (ChangedPin->PinName == FK2Node_LatentAbilityCallHelper::ClassPinName)
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		// Because the archetype has changed, we break the output link as the output pin type will change
		//UEdGraphPin* ResultPin = GetResultPin();
		//ResultPin->BreakAllPinLinks();

		// Remove all pins related to archetype variables
		for (auto OldPin : SpawnParmPins)
		{
			OldPin->BreakAllPinLinks();
			Pins.Remove(OldPin);
		}
		SpawnParmPins.Empty();

		UClass* UseSpawnClass = GetClassToSpawn();
		if (UseSpawnClass != NULL)
		{
			CreatePinsForClass(UseSpawnClass);
		}

		// Refresh the UI for the graph so the pin changes show up
		UEdGraph* Graph = GetGraph();
		Graph->NotifyGraphChanged();

		// Mark dirty
		FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprint());
	}
}

UEdGraphPin* UK2Node_LatentAbilityCall::GetResultPin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPinChecked(K2Schema->PN_ReturnValue);
	check(Pin->Direction == EGPD_Output);
	return Pin;
}

bool UK2Node_LatentAbilityCall::IsSpawnVarPin(UEdGraphPin* Pin)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	return (Pin->Direction == EEdGraphPinDirection::EGPD_Input &&
			Pin->PinName != K2Schema->PN_Execute &&
			Pin->PinName != K2Schema->PN_Then &&
			Pin->PinName != K2Schema->PN_ReturnValue &&
			Pin->PinName != FK2Node_LatentAbilityCallHelper::ClassPinName &&
			Pin->PinName != FK2Node_LatentAbilityCallHelper::WorldContextPinName);

}

bool UK2Node_LatentAbilityCall::ValidateActorSpawning(class FKismetCompilerContext& CompilerContext, bool bGenerateErrors)
{
	FName ProxyPrespawnFunctionName = *FK2Node_LatentAbilityCallHelper::BeginSpawnFuncName;
	UFunction* PreSpawnFunction = ProxyFactoryClass->FindFunctionByName(ProxyPrespawnFunctionName);

	FName ProxyPostpawnFunctionName = *FK2Node_LatentAbilityCallHelper::FinishSpawnFuncName;
	UFunction* PostSpawnFunction = ProxyFactoryClass->FindFunctionByName(ProxyPostpawnFunctionName);

	FName ProxyPrespawnArrayFunctionName = *FK2Node_LatentAbilityCallHelper::BeginSpawnArrayFuncName;
	UFunction* PreSpawnArrayFunction = ProxyFactoryClass->FindFunctionByName(ProxyPrespawnArrayFunctionName);

	FName ProxyPostpawnArrayFunctionName = *FK2Node_LatentAbilityCallHelper::FinishSpawnArrayFuncName;
	UFunction* PostSpawnArrayFunction = ProxyFactoryClass->FindFunctionByName(ProxyPostpawnArrayFunctionName);

	bool HasClassParameter = GetClassPin() != nullptr;
	bool HasPreSpawnFunc = PreSpawnFunction != nullptr;
	bool HasPostSpawnFunc = PostSpawnFunction != nullptr;
	bool HasPreSpawnArrayFunc = PreSpawnArrayFunction != nullptr;
	bool HasPostSpawnArrayFunc = PostSpawnArrayFunction != nullptr;

	if (HasClassParameter || HasPreSpawnFunc || HasPostSpawnFunc)
	{
		// They are trying to use ActorSpawning. If any of the above are NOT true, then we have a problem
		if (!HasClassParameter)
		{
			if (bGenerateErrors)
			{
				CompilerContext.MessageLog.Error(*LOCTEXT("MissingClassParameter", "UK2Node_LatentAbilityCall: Attempting to use ActorSpawning but Proxy Factory Function missing a Class parameter. @@").ToString(), this);
			}
			return false;
		}
		if (!HasPreSpawnFunc)
		{
			if (bGenerateErrors)
			{
				CompilerContext.MessageLog.Error(*LOCTEXT("MissingBeginSpawningFunc", "UK2Node_LatentAbilityCall: Attempting to use ActorSpawning but Missing a BeginSpawningActor function. @@").ToString(), this);
			}
			return false;
		}
		if (!HasPostSpawnFunc)
		{
			if (bGenerateErrors)
			{
				CompilerContext.MessageLog.Error(*LOCTEXT("MissingFinishSpawningFunc", "UK2Node_LatentAbilityCall: Attempting to use ActorSpawning but Missing a FinishSpawningActor function. @@").ToString(), this);
			}
			return false;
		}
		if ((HasPreSpawnArrayFunc || HasPostSpawnArrayFunc))
		{
			if (bGenerateErrors)
			{
				CompilerContext.MessageLog.Error(*LOCTEXT("SpawnFuncAmbiguous", "UK2Node_LatentAbilityCall: Both ActorSpawning and ActorArraySpawning are at least partially implemented. These are mutually exclusive. @@").ToString(), this);
			}
			return false;
		}
	}
	
	return true;
}

bool UK2Node_LatentAbilityCall::ValidateActorArraySpawning(class FKismetCompilerContext& CompilerContext, bool bGenerateErrors)
{
	FName ProxyPrespawnFunctionName = *FK2Node_LatentAbilityCallHelper::BeginSpawnFuncName;
	UFunction* PreSpawnFunction = ProxyFactoryClass->FindFunctionByName(ProxyPrespawnFunctionName);

	FName ProxyPostpawnFunctionName = *FK2Node_LatentAbilityCallHelper::FinishSpawnFuncName;
	UFunction* PostSpawnFunction = ProxyFactoryClass->FindFunctionByName(ProxyPostpawnFunctionName);

	FName ProxyPrespawnArrayFunctionName = *FK2Node_LatentAbilityCallHelper::BeginSpawnArrayFuncName;
	UFunction* PreSpawnArrayFunction = ProxyFactoryClass->FindFunctionByName(ProxyPrespawnArrayFunctionName);

	FName ProxyPostpawnArrayFunctionName = *FK2Node_LatentAbilityCallHelper::FinishSpawnArrayFuncName;
	UFunction* PostSpawnArrayFunction = ProxyFactoryClass->FindFunctionByName(ProxyPostpawnArrayFunctionName);

	bool HasClassParameter = GetClassToSpawn() != nullptr;
	bool HasPreSpawnFunc = PreSpawnFunction != nullptr;
	bool HasPostSpawnFunc = PostSpawnFunction != nullptr;
	bool HasPreSpawnArrayFunc = PreSpawnArrayFunction != nullptr;
	bool HasPostSpawnArrayFunc = PostSpawnArrayFunction != nullptr;

	if (HasClassParameter || HasPreSpawnFunc || HasPostSpawnFunc || HasPreSpawnArrayFunc || HasPostSpawnArrayFunc)
	{
		// They are trying to use ActorSpawning. If any of the above are NOT true, then we have a problem
		if (!HasClassParameter)
		{
			if (bGenerateErrors)
			{
				CompilerContext.MessageLog.Error(*LOCTEXT("MissingClassParameter", "UK2Node_LatentAbilityCall: Attempting to use ActorSpawning but Proxy Factory Function missing a Class parameter. @@").ToString(), this);
			}
			return false;
		}
		if (!HasPreSpawnArrayFunc)
		{
			if (bGenerateErrors)
			{
				CompilerContext.MessageLog.Error(*LOCTEXT("MissingBeginSpawningArrayFunc", "UK2Node_LatentAbilityCall: Attempting to use ActorArraySpawning but Missing a BeginSpawningActorArray function. @@").ToString(), this);
			}
			return false;
		}
		if (!HasPostSpawnArrayFunc)
		{
			if (bGenerateErrors)
			{
				CompilerContext.MessageLog.Error(*LOCTEXT("MissingFinishSpawningArrayFunc", "UK2Node_LatentAbilityCall: Attempting to use ActorArraySpawning but Missing a FinishSpawningActorArray function. @@").ToString(), this);
			}
			return false;
		}
		if (HasPreSpawnFunc || HasPostSpawnFunc)
		{
			if (bGenerateErrors)
			{
				CompilerContext.MessageLog.Error(*LOCTEXT("SpawnFuncAmbiguous", "UK2Node_LatentAbilityCall: Both ActorSpawning and ActorArraySpawning are at least partially implemented. These are mutually exclusive. @@").ToString(), this);
			}
			return false;
		}
	}

	return true;
}

bool UK2Node_LatentAbilityCall::ConnectSpawnProperties(UClass* ClassToSpawn, const UEdGraphSchema_K2* Schema, class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphPin*& LastThenPin, UEdGraphPin* SpawnedActorReturnPin)
{
	bool bIsErrorFree = true;
	for (auto SpawnVarPin : SpawnParmPins)
	{
		const bool bHasDefaultValue = !SpawnVarPin->DefaultValue.IsEmpty() || !SpawnVarPin->DefaultTextValue.IsEmpty() || SpawnVarPin->DefaultObject;
		if (SpawnVarPin->LinkedTo.Num() > 0 || bHasDefaultValue)
		{
			if (SpawnVarPin->LinkedTo.Num() == 0)
			{
				UProperty* Property = FindField<UProperty>(ClassToSpawn, *SpawnVarPin->PinName);
				// NULL property indicates that this pin was part of the original node, not the 
				// class we're assigning to:
				if (!Property)
				{
					continue;
				}

				if (ClassToSpawn->ClassDefaultObject != nullptr)
				{
					// We don't want to generate an assignment node unless the default value 
					// differs from the value in the CDO:
					FString DefaultValueAsString;
					FBlueprintEditorUtils::PropertyValueToString(Property, (uint8*)ClassToSpawn->ClassDefaultObject, DefaultValueAsString);
					if (DefaultValueAsString == SpawnVarPin->DefaultValue)
					{
						continue;
					}
				}
			}


			UFunction* SetByNameFunction = Schema->FindSetVariableByNameFunction(SpawnVarPin->PinType);
			if (SetByNameFunction)
			{
				UK2Node_CallFunction* SetVarNode = NULL;
				if (SpawnVarPin->PinType.bIsArray)
				{
					SetVarNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallArrayFunction>(this, SourceGraph);
				}
				else
				{
					SetVarNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
				}
				SetVarNode->SetFromFunction(SetByNameFunction);
				SetVarNode->AllocateDefaultPins();

				// Connect this node into the exec chain
				bIsErrorFree &= Schema->TryCreateConnection(LastThenPin, SetVarNode->GetExecPin());
				LastThenPin = SetVarNode->GetThenPin();

				static FString ObjectParamName = FString(TEXT("Object"));
				static FString ValueParamName = FString(TEXT("Value"));
				static FString PropertyNameParamName = FString(TEXT("PropertyName"));

				// Connect the new actor to the 'object' pin
				UEdGraphPin* ObjectPin = SetVarNode->FindPinChecked(ObjectParamName);
				SpawnedActorReturnPin->MakeLinkTo(ObjectPin);

				// Fill in literal for 'property name' pin - name of pin is property name
				UEdGraphPin* PropertyNamePin = SetVarNode->FindPinChecked(PropertyNameParamName);
				PropertyNamePin->DefaultValue = SpawnVarPin->PinName;

				UEdGraphPin* ValuePin = SetVarNode->FindPinChecked(ValueParamName);
				if (SpawnVarPin->LinkedTo.Num() == 0 &&
					SpawnVarPin->DefaultValue != FString() &&
					SpawnVarPin->PinType.PinCategory == Schema->PC_Byte &&
					SpawnVarPin->PinType.PinSubCategoryObject.IsValid() &&
					SpawnVarPin->PinType.PinSubCategoryObject->IsA<UEnum>())
				{
					// Pin is an enum, we need to alias the enum value to an int:
					UK2Node_EnumLiteral* EnumLiteralNode = CompilerContext.SpawnIntermediateNode<UK2Node_EnumLiteral>(this, SourceGraph);
					EnumLiteralNode->Enum = CastChecked<UEnum>(SpawnVarPin->PinType.PinSubCategoryObject.Get());
					EnumLiteralNode->AllocateDefaultPins();
					EnumLiteralNode->FindPinChecked(Schema->PN_ReturnValue)->MakeLinkTo(ValuePin);

					UEdGraphPin* InPin = EnumLiteralNode->FindPinChecked(UK2Node_EnumLiteral::GetEnumInputPinName());
					InPin->DefaultValue = SpawnVarPin->DefaultValue;
				}
				else
				{
					// Move connection from the variable pin on the spawn node to the 'value' pin
					CompilerContext.MovePinLinksToIntermediate(*SpawnVarPin, *ValuePin);
					SetVarNode->PinConnectionListChanged(ValuePin);
				}
			}
		}
	}
	return bIsErrorFree;
}

/**
 *	This is essentially a mix of K2Node_BaseAsyncTask::ExpandNode and K2Node_SpawnActorFromClass::ExpandNode.
 *	Several things are going on here:
 *		-Factory call to create proxy object (K2Node_BaseAsyncTask)
 *		-Task return delegates are created and hooked up (K2Node_BaseAsyncTask)
 *		-A BeginSpawn function is called on proxyu object (similiar to K2Node_SpawnActorFromClass)
 *		-BeginSpawn can choose to spawn or not spawn an actor (and return it)
 *			-If spawned:
 *				-SetVars are run on the newly spawned object (set expose on spawn variables - K2Node_SpawnActorFromClass)
 *				-FinishSpawn is called on the proxy object
 *				
 *				
 *	Also, a K2Node_SpawnActorFromClass could not be used directly here, since we want the proxy object to implement its own
 *	BeginSpawn/FinishSpawn function (custom game logic will often be performed in the native implementation). K2Node_SpawnActorFromClass also
 *	requires a SpawnTransform be wired into it, and in most ability task cases, the spawn transform is implied or not necessary.
 *	
 *	
 */
void UK2Node_LatentAbilityCall::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	bool validatedActorSpawn = ValidateActorSpawning(CompilerContext, false);
	bool validatedActorArraySpawn = ValidateActorArraySpawning(CompilerContext, false);

	UEdGraphPin* ClassPin = GetClassPin();
	if (ClassPin == nullptr)
	{
		// Nothing special about this task, just call super
		Super::ExpandNode(CompilerContext, SourceGraph);
		return;
	}

	UK2Node::ExpandNode(CompilerContext, SourceGraph);

	if (!validatedActorSpawn && !validatedActorArraySpawn)
	{
		ValidateActorSpawning(CompilerContext, true);
		ValidateActorArraySpawning(CompilerContext, true);
	}

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	check(SourceGraph && Schema);
	bool bIsErrorFree = true;


	// ------------------------------------------------------------------------------------------
	// CREATE A CALL TO FACTORY THE PROXY OBJECT
	// ------------------------------------------------------------------------------------------
	UK2Node_CallFunction* const CallCreateProxyObjectNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallCreateProxyObjectNode->FunctionReference.SetExternalMember(ProxyFactoryFunctionName, ProxyFactoryClass);
	CallCreateProxyObjectNode->AllocateDefaultPins();
	bIsErrorFree &= CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(Schema->PN_Execute), *CallCreateProxyObjectNode->FindPinChecked(Schema->PN_Execute)).CanSafeConnect();
	for (auto CurrentPin : Pins)
	{
		if (FBaseAsyncTaskHelper::ValidDataPin(CurrentPin, EGPD_Input, Schema))
		{
			UEdGraphPin* DestPin = CallCreateProxyObjectNode->FindPin(CurrentPin->PinName); // match function inputs, to pass data to function from CallFunction node

			// NEW: if no DestPin, assume it is a Class Spawn PRoperty - not an error
			if (DestPin)
			{
				bIsErrorFree &= CompilerContext.CopyPinLinksToIntermediate(*CurrentPin, *DestPin).CanSafeConnect();
			}
		}
	}
	// ------------------------------------------------------------------------------------------
	// GATHER OUTPUT PARAMETERS AND PAIR THEM WITH LOCAL VARIABLES
	// ------------------------------------------------------------------------------------------
	TArray<FBaseAsyncTaskHelper::FOutputPinAndLocalVariable> VariableOutputs;
	for (auto CurrentPin : Pins)
	{
		if (FBaseAsyncTaskHelper::ValidDataPin(CurrentPin, EGPD_Output, Schema))
		{
			const FEdGraphPinType& PinType = CurrentPin->PinType;
			UK2Node_TemporaryVariable* TempVarOutput = CompilerContext.SpawnInternalVariable(
				this, PinType.PinCategory, PinType.PinSubCategory, PinType.PinSubCategoryObject.Get(), PinType.bIsArray);
			bIsErrorFree &= TempVarOutput->GetVariablePin() && CompilerContext.MovePinLinksToIntermediate(*CurrentPin, *TempVarOutput->GetVariablePin()).CanSafeConnect();
			VariableOutputs.Add(FBaseAsyncTaskHelper::FOutputPinAndLocalVariable(CurrentPin, TempVarOutput));
		}
	}

	// ------------------------------------------------------------------------------------------
	// FOR EACH DELEGATE DEFINE EVENT, CONNECT IT TO DELEGATE AND IMPLEMENT A CHAIN OF ASSIGMENTS
	// ------------------------------------------------------------------------------------------
	UEdGraphPin* LastThenPin = CallCreateProxyObjectNode->FindPinChecked(Schema->PN_Then);
	UEdGraphPin* const ProxyObjectPin = CallCreateProxyObjectNode->GetReturnValuePin();
	for (TFieldIterator<UMulticastDelegateProperty> PropertyIt(ProxyClass, EFieldIteratorFlags::ExcludeSuper); PropertyIt && bIsErrorFree; ++PropertyIt)
	{
		bIsErrorFree &= FBaseAsyncTaskHelper::HandleDelegateImplementation(*PropertyIt, VariableOutputs, ProxyObjectPin, LastThenPin, this, SourceGraph, CompilerContext);
	}

	if (CallCreateProxyObjectNode->FindPinChecked(Schema->PN_Then) == LastThenPin)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("MissingDelegateProperties", "BaseAsyncTask: Proxy has no delegates defined. @@").ToString(), this);
		return;
	}


	// ------------------------------------------------------------------------------------------
	// NEW: CREATE A CALL TO THE PRESPAWN FUNCTION, IF IT RETURNS TRUE, THEN WE WILL SPAWN THE NEW ACTOR
	// ------------------------------------------------------------------------------------------

	FName ProxyPrespawnFunctionName = validatedActorArraySpawn ? *FK2Node_LatentAbilityCallHelper::BeginSpawnArrayFuncName : *FK2Node_LatentAbilityCallHelper::BeginSpawnFuncName;
	UFunction* PreSpawnFunction = ProxyFactoryClass->FindFunctionByName(ProxyPrespawnFunctionName);

	FName ProxyPostpawnFunctionName = validatedActorArraySpawn ? *FK2Node_LatentAbilityCallHelper::FinishSpawnArrayFuncName : *FK2Node_LatentAbilityCallHelper::FinishSpawnFuncName;
	UFunction* PostSpawnFunction = ProxyFactoryClass->FindFunctionByName(ProxyPostpawnFunctionName);

	if (PreSpawnFunction == nullptr)
	{
		if (validatedActorArraySpawn)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("MissingBeginSpawningActorArrayFunction", "AbilityTask: Proxy is missing BeginSpawningActorArray native function. @@").ToString(), this);
		}
		else
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("MissingBeginSpawningActorFunction", "AbilityTask: Proxy is missing BeginSpawningActor native function. @@").ToString(), this);
		}
		return;
	}

	if (PostSpawnFunction == nullptr)
	{
		if (validatedActorArraySpawn)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("MissingFinishSpawningActorArrayFunction", "AbilityTask: Proxy is missing FinishSpawningActorArray native function. @@").ToString(), this);
		}
		else
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("MissingFinishSpawningActorFunction", "AbilityTask: Proxy is missing FinishSpawningActor native function. @@").ToString(), this);
		}
		return;
	}


	UK2Node_CallFunction* const CallPrespawnProxyObjectNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallPrespawnProxyObjectNode->FunctionReference.SetExternalMember(ProxyPrespawnFunctionName, ProxyClass);
	CallPrespawnProxyObjectNode->AllocateDefaultPins();

	// Hook up the self connection
	UEdGraphPin* PrespawnCallSelfPin = Schema->FindSelfPin(*CallPrespawnProxyObjectNode, EGPD_Input);
	check(PrespawnCallSelfPin);

	bIsErrorFree &= Schema->TryCreateConnection(ProxyObjectPin, PrespawnCallSelfPin);

	// Hook up input parameters to PreSpawn
	for (auto CurrentPin : Pins)
	{
		if (FBaseAsyncTaskHelper::ValidDataPin(CurrentPin, EGPD_Input, Schema))
		{
			UEdGraphPin* DestPin = CallPrespawnProxyObjectNode->FindPin(CurrentPin->PinName);
			if (DestPin)
			{
				bIsErrorFree &= CompilerContext.CopyPinLinksToIntermediate(*CurrentPin, *DestPin).CanSafeConnect();
			}
		}
	}		

	// Hook the activate node up in the exec chain
	UEdGraphPin* PrespawnExecPin = CallPrespawnProxyObjectNode->FindPinChecked(Schema->PN_Execute);
	UEdGraphPin* PrespawnThenPin = CallPrespawnProxyObjectNode->FindPinChecked(Schema->PN_Then);
	UEdGraphPin* PrespawnReturnPin = CallPrespawnProxyObjectNode->FindPinChecked(Schema->PN_ReturnValue);
	UEdGraphPin* SpawnedActorReturnPin = CallPrespawnProxyObjectNode->FindPinChecked(FK2Node_LatentAbilityCallHelper::SpawnedActorPinName);

	bIsErrorFree &= Schema->TryCreateConnection(LastThenPin, PrespawnExecPin);

	LastThenPin = PrespawnThenPin;

	// -------------------------------------------
	// Branch based on return value of Prespawn
	// -------------------------------------------
		
	UK2Node_IfThenElse* BranchNode = SourceGraph->CreateBlankNode<UK2Node_IfThenElse>();
	BranchNode->AllocateDefaultPins();
	CompilerContext.MessageLog.NotifyIntermediateObjectCreation(BranchNode, this);

	// Link return value of prespawn with the branch condtional
	bIsErrorFree &= Schema->TryCreateConnection(PrespawnReturnPin, BranchNode->GetConditionPin());

	// Link our Prespawn call to the branch node
	bIsErrorFree &= Schema->TryCreateConnection(LastThenPin, BranchNode->GetExecPin());

	UEdGraphPin* BranchElsePin = BranchNode->GetElsePin();

	LastThenPin = BranchNode->GetThenPin();

	UClass* ClassToSpawn = GetClassToSpawn();
	if (validatedActorArraySpawn && ClassToSpawn)
	{
		//Branch for main loop control
		UK2Node_IfThenElse* Branch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
		Branch->AllocateDefaultPins();

		//Create int Iterator
		UK2Node_TemporaryVariable* IteratorVar = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
		IteratorVar->VariableType.PinCategory = Schema->PC_Int;
		IteratorVar->AllocateDefaultPins();

		//Iterator assignment (initialization to zero)
		UK2Node_AssignmentStatement* IteratorInitialize = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
		IteratorInitialize->AllocateDefaultPins();
		IteratorInitialize->GetValuePin()->DefaultValue = TEXT("0");

		//Iterator assignment (incrementing)
		UK2Node_AssignmentStatement* IteratorAssign = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
		IteratorAssign->AllocateDefaultPins();

		//Increment iterator command
		UK2Node_CallFunction* Increment = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		Increment->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Add_IntInt")));
		Increment->AllocateDefaultPins();
		Increment->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("1");

		//Array length
		UK2Node_CallArrayFunction* ArrayLength = CompilerContext.SpawnIntermediateNode<UK2Node_CallArrayFunction>(this, SourceGraph);
		ArrayLength->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(TEXT("Array_Length")));
		ArrayLength->AllocateDefaultPins();

		//Array element retrieval
		UK2Node_CallArrayFunction* GetElement = CompilerContext.SpawnIntermediateNode<UK2Node_CallArrayFunction>(this, SourceGraph);
		GetElement->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(TEXT("Array_Get")));
		GetElement->AllocateDefaultPins();

		//Check node for iterator versus array length
		UK2Node_CallFunction* Condition = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		Condition->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Less_IntInt")));
		Condition->AllocateDefaultPins();

		//Connections to set up the loop
		UEdGraphSchema_K2 const* const K2Schema = Cast<const UEdGraphSchema_K2>(Schema);
		bIsErrorFree &= Schema->TryCreateConnection(LastThenPin, IteratorInitialize->GetExecPin());
		bIsErrorFree &= Schema->TryCreateConnection(IteratorVar->GetVariablePin(), IteratorInitialize->GetVariablePin());
		bIsErrorFree &= Schema->TryCreateConnection(IteratorInitialize->GetThenPin(), Branch->GetExecPin());
		bIsErrorFree &= Schema->TryCreateConnection(SpawnedActorReturnPin, ArrayLength->GetTargetArrayPin());
		bIsErrorFree &= Schema->TryCreateConnection(Condition->GetReturnValuePin(), Branch->GetConditionPin());
		bIsErrorFree &= Schema->TryCreateConnection(IteratorVar->GetVariablePin(), Condition->FindPinChecked(TEXT("A")));
		bIsErrorFree &= Schema->TryCreateConnection(ArrayLength->FindPin(K2Schema->PN_ReturnValue), Condition->FindPinChecked(TEXT("B")));

		//Connections to establish loop iteration
		bIsErrorFree &= Schema->TryCreateConnection(IteratorVar->GetVariablePin(), Increment->FindPinChecked(TEXT("A")));
		bIsErrorFree &= Schema->TryCreateConnection(IteratorVar->GetVariablePin(), IteratorAssign->GetVariablePin());
		bIsErrorFree &= Schema->TryCreateConnection(Increment->GetReturnValuePin(), IteratorAssign->GetValuePin());
		bIsErrorFree &= Schema->TryCreateConnection(IteratorAssign->GetThenPin(), Branch->GetExecPin());

		//This is the inner loop
		LastThenPin = Branch->GetThenPin();		//Connect the loop branch to the spawn-assignment code block
		bIsErrorFree &= Schema->TryCreateConnection(SpawnedActorReturnPin, GetElement->GetTargetArrayPin());
		bIsErrorFree &= Schema->TryCreateConnection(IteratorVar->GetVariablePin(), GetElement->FindPinChecked(K2Schema->PN_Index));
		bIsErrorFree &= ConnectSpawnProperties(ClassToSpawn, Schema, CompilerContext, SourceGraph, LastThenPin, GetElement->FindPinChecked(K2Schema->PN_Item));		//Last argument is the array element
		bIsErrorFree &= Schema->TryCreateConnection(LastThenPin, IteratorAssign->GetExecPin());		//Connect the spawn-assignment code block to the iterator increment
		
		//Finish by providing the proper path out
		LastThenPin = Branch->GetElsePin();
	}

	// -------------------------------------------
	// Set spawn variables
	//  Borrowed heavily from FKismetCompilerUtilities::GenerateAssignmentNodes
	// -------------------------------------------
	
	if (validatedActorSpawn && ClassToSpawn)
	{
		bIsErrorFree &= ConnectSpawnProperties(ClassToSpawn, Schema, CompilerContext, SourceGraph, LastThenPin, SpawnedActorReturnPin);
	}
	
	// -------------------------------------------
	// Call FinishSpawning
	// -------------------------------------------

	UK2Node_CallFunction* const CallPostSpawnnProxyObjectNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallPostSpawnnProxyObjectNode->FunctionReference.SetExternalMember(ProxyPostpawnFunctionName, ProxyClass);
	CallPostSpawnnProxyObjectNode->AllocateDefaultPins();

	// Hook up the self connection
	UEdGraphPin* PostspawnCallSelfPin = Schema->FindSelfPin(*CallPostSpawnnProxyObjectNode, EGPD_Input);
	check(PostspawnCallSelfPin);

	bIsErrorFree &= Schema->TryCreateConnection(ProxyObjectPin, PostspawnCallSelfPin);

	// Link our Postspawn call in
	bIsErrorFree &= Schema->TryCreateConnection(LastThenPin, CallPostSpawnnProxyObjectNode->FindPinChecked(Schema->PN_Execute));

	// Hook up any other input parameters to PostSpawn
	for (auto CurrentPin : Pins)
	{
		if (FBaseAsyncTaskHelper::ValidDataPin(CurrentPin, EGPD_Input, Schema))
		{
			UEdGraphPin* DestPin = CallPostSpawnnProxyObjectNode->FindPin(CurrentPin->PinName);
			if (DestPin)
			{
				bIsErrorFree &= CompilerContext.CopyPinLinksToIntermediate(*CurrentPin, *DestPin).CanSafeConnect();
			}
		}
	}


	UEdGraphPin* InSpawnedActorPin = CallPostSpawnnProxyObjectNode->FindPin(TEXT("SpawnedActor"));
	if (InSpawnedActorPin == nullptr)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("MissingSpawnedActorInputPin", "AbilityTask: Proxy is missing SpawnedActor input pin in FinishSpawningActor. @@").ToString(), this);
		return;
	}

	bIsErrorFree &= Schema->TryCreateConnection(SpawnedActorReturnPin, InSpawnedActorPin);

	LastThenPin = CallPostSpawnnProxyObjectNode->FindPinChecked(Schema->PN_Then);


	// Move the connections from the original node then pin to the last internal then pin
	bIsErrorFree &= CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(Schema->PN_Then), *LastThenPin).CanSafeConnect();
	bIsErrorFree &= CompilerContext.CopyPinLinksToIntermediate(*LastThenPin, *BranchElsePin).CanSafeConnect();
	
	if (!bIsErrorFree)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("InternalConnectionError", "BaseAsyncTask: Internal connection error. @@").ToString(), this);
	}

	// Make sure we caught everything
	BreakAllNodeLinks();
}


#undef LOCTEXT_NAMESPACE
