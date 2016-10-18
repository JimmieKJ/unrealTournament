// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "BlueprintGraphPrivatePCH.h"
#include "Kismet/KismetSystemLibrary.h"
#include "K2Node_ClassDynamicCast.h"
#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "K2Node_LoadAsset.h"

#define LOCTEXT_NAMESPACE "K2Node_LoadAsset"

void UK2Node_LoadAsset::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(GetSchema());

	if (K2Schema)
	{
		CreatePin(EGPD_Input, K2Schema->PC_Exec, TEXT(""), nullptr, false, false, K2Schema->PN_Execute);
		UEdGraphPin* OutputExecPin = CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), nullptr, false, false, K2Schema->PN_Then);
		if (OutputExecPin)
		{
			OutputExecPin->PinFriendlyName = FText::FromString(K2Schema->PN_Completed);
		}

		CreatePin(EGPD_Input, GetInputCategory(), TEXT(""), UObject::StaticClass(), false, false, GetInputPinName());
		CreatePin(EGPD_Output, GetOutputCategory(), TEXT(""), UObject::StaticClass(), false, false, GetOutputPinName());
	}
}

void UK2Node_LoadAsset::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	check(Schema);
	bool bIsErrorFree = true;

	// Create LoadAsset function call
	auto CallLoadAssetNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallLoadAssetNode->FunctionReference.SetExternalMember(NativeFunctionName(), UKismetSystemLibrary::StaticClass());
	CallLoadAssetNode->AllocateDefaultPins(); 

	// connect to input exe
	{
		auto InputExePin = GetExecPin();
		auto CallFunctionInputExePin = CallLoadAssetNode->GetExecPin();
		bIsErrorFree &= InputExePin && CallFunctionInputExePin && CompilerContext.MovePinLinksToIntermediate(*InputExePin, *CallFunctionInputExePin).CanSafeConnect();
	}

	// Create Local Variable
	UK2Node_TemporaryVariable* TempVarOutput = CompilerContext.SpawnInternalVariable(this, GetOutputCategory(), FString(), UObject::StaticClass(), false);

	// Create assign node
	auto AssignNode = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	AssignNode->AllocateDefaultPins();

	auto LoadedObjectVariablePin = TempVarOutput->GetVariablePin();

	// connect local variable to assign node
	{
		auto AssignLHSPPin = AssignNode->GetVariablePin();
		bIsErrorFree &= AssignLHSPPin && LoadedObjectVariablePin && Schema->TryCreateConnection(AssignLHSPPin, LoadedObjectVariablePin);
	}

	// connect local variable to output
	{
		auto OutputObjectPinPin = FindPin(GetOutputPinName());
		bIsErrorFree &= LoadedObjectVariablePin && OutputObjectPinPin && CompilerContext.MovePinLinksToIntermediate(*OutputObjectPinPin, *LoadedObjectVariablePin).CanSafeConnect();
	}

	// connect assign exec input to function output
	{
		auto CallFunctionOutputExePin = CallLoadAssetNode->FindPin(Schema->PN_Then);
		auto AssignInputExePin = AssignNode->GetExecPin();
		bIsErrorFree &= AssignInputExePin && CallFunctionOutputExePin && Schema->TryCreateConnection(AssignInputExePin, CallFunctionOutputExePin);
	}

	// connect assign exec output to output
	{
		auto OutputExePin = FindPin(Schema->PN_Then);
		auto AssignOutputExePin = AssignNode->GetThenPin();
		bIsErrorFree &= OutputExePin && AssignOutputExePin && CompilerContext.MovePinLinksToIntermediate(*OutputExePin, *AssignOutputExePin).CanSafeConnect();
	}

	// connect to asset
	auto CallFunctionAssetPin = CallLoadAssetNode->FindPin(GetInputPinName());
	{
		auto AssetPin = FindPin(GetInputPinName());
		ensure(CallFunctionAssetPin);
		bIsErrorFree &= AssetPin && CallFunctionAssetPin && CompilerContext.MovePinLinksToIntermediate(*AssetPin, *CallFunctionAssetPin).CanSafeConnect();
	}

	// Create OnLoadEvent
	const FString DelegateOnLoadedParamName(TEXT("OnLoaded"));
	auto OnLoadEventNode = CompilerContext.SpawnIntermediateEventNode<UK2Node_CustomEvent>(this, CallFunctionAssetPin, SourceGraph);
	OnLoadEventNode->CustomFunctionName = *FString::Printf(TEXT("OnLoaded_%s"), *CompilerContext.GetGuid(this));
	OnLoadEventNode->AllocateDefaultPins();
	{
		UFunction* LoadAssetFunction = CallLoadAssetNode->GetTargetFunction();
		UDelegateProperty* OnLoadDelegateProperty = LoadAssetFunction ? FindField<UDelegateProperty>(LoadAssetFunction, *DelegateOnLoadedParamName) : nullptr;
		UFunction* OnLoadedSignature = OnLoadDelegateProperty ? OnLoadDelegateProperty->SignatureFunction : nullptr;
		ensure(OnLoadedSignature);
		for (TFieldIterator<UProperty> PropIt(OnLoadedSignature); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			const UProperty* Param = *PropIt;
			if (!Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm))
			{
				FEdGraphPinType PinType;
				bIsErrorFree &= Schema->ConvertPropertyToPinType(Param, /*out*/ PinType);
				bIsErrorFree &= (NULL != OnLoadEventNode->CreateUserDefinedPin(Param->GetName(), PinType, EGPD_Output));
			}
		}
	}

	// connect delegate
	{
		auto CallFunctionDelegatePin = CallLoadAssetNode->FindPin(DelegateOnLoadedParamName);
		ensure(CallFunctionDelegatePin);
		auto EventDelegatePin = OnLoadEventNode->FindPin(UK2Node_CustomEvent::DelegateOutputName);
		bIsErrorFree &= CallFunctionDelegatePin && EventDelegatePin && Schema->TryCreateConnection(CallFunctionDelegatePin, EventDelegatePin);
	}

	// connect loaded object from event to assign
	{
		auto LoadedAssetEventPin = OnLoadEventNode->FindPin(TEXT("Loaded"));
		ensure(LoadedAssetEventPin);
		auto AssignRHSPPin = AssignNode->GetValuePin();
		bIsErrorFree &= AssignRHSPPin && LoadedAssetEventPin && Schema->TryCreateConnection(LoadedAssetEventPin, AssignRHSPPin);
	}

	if (!bIsErrorFree)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("InternalConnectionError", "K2Node_LoadAsset: Internal connection error. @@").ToString(), this);
	}

	BreakAllNodeLinks();
}

FText UK2Node_LoadAsset::GetTooltipText() const
{
	return FText(LOCTEXT("UK2Node_LoadAssetGetTooltipText", "Async Load Asset"));
}

FText UK2Node_LoadAsset::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText(LOCTEXT("UK2Node_LoadAssetGetNodeTitle", "Load Asset"));
}

bool UK2Node_LoadAsset::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	bool bIsCompatible = false;
	// Can only place events in ubergraphs and macros (other code will help prevent macros with latents from ending up in functions), and basicasync task creates an event node:
	EGraphType GraphType = TargetGraph->GetSchema()->GetGraphType(TargetGraph);
	if (GraphType == EGraphType::GT_Ubergraph || GraphType == EGraphType::GT_Macro)
	{
		bIsCompatible = true;
	}
	return bIsCompatible && Super::IsCompatibleWithGraph(TargetGraph);
}

FName UK2Node_LoadAsset::GetCornerIcon() const
{
	return TEXT("Graph.Latent.LatentIcon");
}

void UK2Node_LoadAsset::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_LoadAsset::GetMenuCategory() const
{
	return FText(LOCTEXT("UK2Node_LoadAssetGetMenuCategory", "Utilities"));
}

const FString& UK2Node_LoadAsset::GetInputCategory() const
{
	const UEdGraphSchema_K2* K2Schema = CastChecked<const UEdGraphSchema_K2>(GetSchema());
	return K2Schema->PC_Asset;
}

const FString& UK2Node_LoadAsset::GetOutputCategory() const
{
	const UEdGraphSchema_K2* K2Schema = CastChecked<const UEdGraphSchema_K2>(GetSchema());
	return K2Schema->PC_Object;
}

const FString& UK2Node_LoadAsset::GetInputPinName() const
{
	static const FString InputAssetPinName("Asset");
	return InputAssetPinName;
}

const FString& UK2Node_LoadAsset::GetOutputPinName() const
{
	static const FString OutputObjectPinName("Object");
	return OutputObjectPinName;
}

FName UK2Node_LoadAsset::NativeFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, LoadAsset);
}

// UK2Node_LoadAssetClass

FName UK2Node_LoadAssetClass::NativeFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, LoadAssetClass);
}


FText UK2Node_LoadAssetClass::GetTooltipText() const
{
	return FText(LOCTEXT("UK2Node_LoadAssetClassGetTooltipText", "Async Load Class Asset"));
}

FText UK2Node_LoadAssetClass::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText(LOCTEXT("UK2Node_LoadAssetClassGetNodeTitle", "Load Class Asset"));
}

const FString& UK2Node_LoadAssetClass::GetInputCategory() const
{
	const UEdGraphSchema_K2* K2Schema = CastChecked<const UEdGraphSchema_K2>(GetSchema());
	return K2Schema->PC_AssetClass;
}

const FString& UK2Node_LoadAssetClass::GetOutputCategory() const
{
	const UEdGraphSchema_K2* K2Schema = CastChecked<const UEdGraphSchema_K2>(GetSchema());
	return K2Schema->PC_Class;
}

const FString& UK2Node_LoadAssetClass::GetInputPinName() const
{
	static const FString InputAssetPinName("AssetClass");
	return InputAssetPinName;
}

const FString& UK2Node_LoadAssetClass::GetOutputPinName() const
{
	static const FString OutputObjectPinName("Class");
	return OutputObjectPinName;
}


#undef LOCTEXT_NAMESPACE