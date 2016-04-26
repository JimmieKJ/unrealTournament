// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "BlueprintGraphPrivatePCH.h"
#include "Kismet/KismetSystemLibrary.h"
#include "K2Node_ClassDynamicCast.h"
#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "K2Node_ConvertAsset.h"

#define LOCTEXT_NAMESPACE "K2Node_ConvertAsset"

namespace UK2Node_ConvertAssetImpl
{
	static const FString InputPinName("Asset");
	static const FString OutputPinName("Object");
}

UClass* UK2Node_ConvertAsset::GetTargetClass() const
{
	auto InutPin = FindPin(UK2Node_ConvertAssetImpl::InputPinName);
	bool bIsConnected = InutPin && InutPin->LinkedTo.Num() && InutPin->LinkedTo[0];
	auto SourcePin = bIsConnected ? InutPin->LinkedTo[0] : nullptr;
	return SourcePin
		? Cast<UClass>(SourcePin->PinType.PinSubCategoryObject.Get())
		: nullptr;
}

bool UK2Node_ConvertAsset::IsAssetClassType() const
{
	const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(GetSchema());

	// get first input, return if class asset
	auto InutPin = FindPin(UK2Node_ConvertAssetImpl::InputPinName);
	bool bIsConnected = InutPin && InutPin->LinkedTo.Num() && InutPin->LinkedTo[0];
	auto SourcePin = bIsConnected ? InutPin->LinkedTo[0] : nullptr;
	return (SourcePin && K2Schema)
		? (SourcePin->PinType.PinCategory == K2Schema->PC_AssetClass)
		: false;
}

bool UK2Node_ConvertAsset::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(GetSchema());
	auto InutPin = FindPin(UK2Node_ConvertAssetImpl::InputPinName);
	if (K2Schema && InutPin && OtherPin && (InutPin == MyPin) && (MyPin->PinType.PinCategory == K2Schema->PC_Wildcard))
	{
		if ((OtherPin->PinType.PinCategory != K2Schema->PC_Asset) &&
			(OtherPin->PinType.PinCategory != K2Schema->PC_AssetClass))
		{
			return true;
		}
	}
	return false;
}

void UK2Node_ConvertAsset::RefreshPinTypes()
{
	const UEdGraphSchema_K2* K2Schema = CastChecked<UEdGraphSchema_K2>(GetSchema());
	auto InutPin = FindPin(UK2Node_ConvertAssetImpl::InputPinName);
	auto OutputPin = FindPin(UK2Node_ConvertAssetImpl::OutputPinName);
	ensure(InutPin && OutputPin);
	if (InutPin && OutputPin)
	{
		const bool bIsConnected = InutPin->LinkedTo.Num() > 0;
		UClass* TargetType = bIsConnected ? GetTargetClass() : nullptr;
		const bool bIsAssetClass = bIsConnected ? IsAssetClassType() : false;

		const FString InputCategory = bIsConnected
			? (bIsAssetClass ? K2Schema->PC_AssetClass : K2Schema->PC_Asset)
			: K2Schema->PC_Wildcard;
		InutPin->PinType = FEdGraphPinType(InputCategory, FString(), TargetType, false, false);

		const FString OutputCategory = bIsConnected
			? (bIsAssetClass ? K2Schema->PC_Class : K2Schema->PC_Object)
			: K2Schema->PC_Wildcard;
		OutputPin->PinType = FEdGraphPinType(OutputCategory, FString(), TargetType, false, false);

		PinTypeChanged(InutPin);
		PinTypeChanged(OutputPin);

		if (OutputPin->LinkedTo.Num())
		{
			UClass const* CallingContext = NULL;
			if (UBlueprint const* Blueprint = GetBlueprint())
			{
				CallingContext = Blueprint->GeneratedClass;
				if (CallingContext == NULL)
				{
					CallingContext = Blueprint->ParentClass;
				}
			}

			for (auto TargetPin : OutputPin->LinkedTo)
			{
				if (TargetPin && !K2Schema->ArePinsCompatible(OutputPin, TargetPin, CallingContext))
				{
					OutputPin->BreakLinkTo(TargetPin);
				}
			}
		}
	}
}

void UK2Node_ConvertAsset::PostReconstructNode()
{
	RefreshPinTypes();

	Super::PostReconstructNode();
}

void UK2Node_ConvertAsset::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);
	if (Pin && (UK2Node_ConvertAssetImpl::InputPinName == Pin->PinName))
	{
		RefreshPinTypes();
	}
}

void UK2Node_ConvertAsset::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = CastChecked<UEdGraphSchema_K2>(GetSchema());
	CreatePin(EGPD_Input, K2Schema->PC_Wildcard, TEXT(""), nullptr, false, false, UK2Node_ConvertAssetImpl::InputPinName);
	CreatePin(EGPD_Output, K2Schema->PC_Wildcard, TEXT(""), nullptr, false, false, UK2Node_ConvertAssetImpl::OutputPinName);
}

void UK2Node_ConvertAsset::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
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

void UK2Node_ConvertAsset::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	
	UClass* TargetType = GetTargetClass();
	if (TargetType && Schema && (2 == Pins.Num()))
	{
		const bool bIsAssetClass = IsAssetClassType();

		//Create Convert Function
		auto ConvertToObjectFunc = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		const FName ConvertFunctionName = bIsAssetClass 
			? GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Conv_AssetClassToClass) 
			: GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Conv_AssetToObject);
		ConvertToObjectFunc->FunctionReference.SetExternalMember(ConvertFunctionName, UKismetSystemLibrary::StaticClass());
		ConvertToObjectFunc->AllocateDefaultPins();

		//Connect input to convert
		auto InputPin = FindPin(UK2Node_ConvertAssetImpl::InputPinName);
		const FString ConvertInputName = bIsAssetClass
			? FString(TEXT("AssetClass"))
			: FString(TEXT("Asset"));
		auto ConvertInput = ConvertToObjectFunc->FindPin(ConvertInputName);
		bool bIsErrorFree = InputPin && ConvertInput && CompilerContext.MovePinLinksToIntermediate(*InputPin, *ConvertInput).CanSafeConnect();

		auto ConvertOutput = ConvertToObjectFunc->GetReturnValuePin();
		UEdGraphPin* InnerOutput = nullptr;
		if (UObject::StaticClass() != TargetType)
		{
			//Create Cast Node
			UK2Node_DynamicCast* CastNode = bIsAssetClass
				? CompilerContext.SpawnIntermediateNode<UK2Node_ClassDynamicCast>(this, SourceGraph)
				: CompilerContext.SpawnIntermediateNode<UK2Node_DynamicCast>(this, SourceGraph);
			CastNode->SetPurity(true);
			CastNode->TargetType = TargetType;
			CastNode->AllocateDefaultPins();

			// Connect Object/Class to Cast
			auto CastInput = CastNode->GetCastSourcePin();
			bIsErrorFree &= ConvertOutput && CastInput && Schema->TryCreateConnection(ConvertOutput, CastInput);

			// Connect output to cast
			InnerOutput = CastNode->GetCastResultPin();
		}
		else
		{
			InnerOutput = ConvertOutput;
		}

		auto OutputPin = FindPin(UK2Node_ConvertAssetImpl::OutputPinName);
		bIsErrorFree &= OutputPin && InnerOutput && CompilerContext.MovePinLinksToIntermediate(*OutputPin, *InnerOutput).CanSafeConnect();

		if (!bIsErrorFree)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("InternalConnectionError", "K2Node_ConvertAsset: Internal connection error. @@").ToString(), this);
		}

		BreakAllNodeLinks();
	}
}

FText UK2Node_ConvertAsset::GetCompactNodeTitle() const
{
	return FText::FromString(TEXT("\x2022"));
}

FText UK2Node_ConvertAsset::GetMenuCategory() const
{
	return FText(LOCTEXT("UK2Node_LoadAssetGetMenuCategory", "Utilities"));
}

FText UK2Node_ConvertAsset::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText(LOCTEXT("UK2Node_ConvertAssetGetNodeTitle", "Resolve Asset ID"));
}

FText UK2Node_ConvertAsset::GetTooltipText() const
{
	return FText(LOCTEXT("UK2Node_ConvertAssetGetTooltipText", "Resolves an Asset ID or Class Asset ID into an object/class. If the asset wasn't loaded it returns none."));
}

#undef LOCTEXT_NAMESPACE