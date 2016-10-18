// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"

#include "CompilerResultsLog.h"
#include "CallFunctionHandler.h"
#include "K2Node_SwitchEnum.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "K2Node_PureAssignmentStatement.h"
#include "GraphEditorSettings.h"
#include "BlueprintActionFilter.h"
#include "Editor/Kismet/Public/FindInBlueprintManager.h"

#define LOCTEXT_NAMESPACE "K2Node"

/*******************************************************************************
 *  FCustomStructureParamHelper
 ******************************************************************************/

struct FCustomStructureParamHelper
{
	static FName GetCustomStructureParamName()
	{
		static FName Name(TEXT("CustomStructureParam"));
		return Name;
	}

	static void FillCustomStructureParameterNames(const UFunction* Function, TArray<FString>& OutNames)
	{
		OutNames.Empty();
		if (Function)
		{
			FString MetaDataValue = Function->GetMetaData(GetCustomStructureParamName());
			if (!MetaDataValue.IsEmpty())
			{
				MetaDataValue.ParseIntoArray(OutNames, TEXT(","), true);
			}
		}
	}

	static void HandleSinglePin(UEdGraphPin* Pin)
	{
		if (Pin)
		{
			if (Pin->LinkedTo.Num() > 0)
			{
				UEdGraphPin* LinkedTo = Pin->LinkedTo[0];
				check(LinkedTo);
				ensure(!LinkedTo->PinType.bIsArray);

				Pin->PinType = LinkedTo->PinType;
			}
			else
			{
				const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
				Pin->PinType.PinCategory = Schema->PC_Wildcard;
				Pin->PinType.PinSubCategory = TEXT("");
				Pin->PinType.PinSubCategoryObject = NULL;
			}
		}
	}

	static void UpdateCustomStructurePins(const UFunction* Function, UK2Node* Node, UEdGraphPin* SinglePin = NULL)
	{
		if (Function && Node)
		{
			TArray<FString> Names;
			FCustomStructureParamHelper::FillCustomStructureParameterNames(Function, Names);
			if (SinglePin)
			{
				if (Names.Contains(SinglePin->PinName))
				{
					HandleSinglePin(SinglePin);
				}
			}
			else
			{
				for (auto& Name : Names)
				{
					if (auto Pin = Node->FindPin(Name))
					{
						HandleSinglePin(Pin);
					}
				}
			}
		}
	}
};

/*******************************************************************************
 *  FDynamicOutputUtils
 ******************************************************************************/

struct FDynamicOutputHelper
{
public:
	FDynamicOutputHelper(UEdGraphPin* InAlteredPin)
		: MutatingPin(InAlteredPin)
	{}

	/**
	 * Attempts to change the output pin's type so that it reflects the class 
	 * specified by the input class pin.
	 */
	void ConformOutputType() const;

	/**
	 * Retrieves the class pin that is used to determine the function's output type.
	 * 
	 * @return Null if the "DeterminesOutputType" metadata targets an invalid 
	 *         param (or if the metadata isn't present), otherwise a class picker pin.
	 */
	static UEdGraphPin* GetTypePickerPin(const UK2Node_CallFunction* FuncNode);

	/**
	 * Attempts to pull out class info from the supplied pin. Starts with the 
	 * pin's default, and then falls back onto the pin's native type. Will poll
	 * any connections that the pin may have.
	 * 
	 * @param  Pin	The pin you want a class from.
	 * @return A class that the pin represents (could be null if the pin isn't a class pin).
	 */
	static UClass* GetPinClass(UEdGraphPin* Pin);

	/**
	 * Intended to be used by ValidateNodeDuringCompilation(). Will check to 
	 * make sure the dynamic output's connections are still valid (they could
	 * become invalid as the the pin's type changes).
	 * 
	 * @param  FuncNode		The node you wish to validate.
	 * @param  MessageLog	The log to post errors/warnings to.
	 */
	static void VerifyNode(const UK2Node_CallFunction* FuncNode, FCompilerResultsLog& MessageLog);

private:
	/**
	 * 
	 * 
	 * @return 
	 */
	UK2Node_CallFunction* GetFunctionNode() const;

	/**
	 * 
	 * 
	 * @return 
	 */
	UFunction* GetTargetFunction() const;

	/**
	 * Checks if the supplied pin is the class picker that governs the 
	 * function's output type.
	 * 
	 * @param  Pin	The pin to test.
	 * @return True if the pin corresponds to the param that was flagged by the "DeterminesOutputType" metadata.
	 */
	bool IsTypePickerPin(UEdGraphPin* Pin) const;

	/**
	 * Retrieves the object output pin that is altered as the class input is 
	 * changed (favors params flagged by "DynamicOutputParam" metadata).
	 * 
	 * @return Null if the output param cannot be altered from the class input, 
	 *         otherwise a output pin that will mutate type as the class input is changed.
	 */
	static UEdGraphPin* GetDynamicOutPin(const UK2Node_CallFunction* FuncNode);

	/**
	 * Checks if the specified type is an object type that reflects the picker 
	 * pin's class.
	 * 
	 * @param  TypeToTest	The type you want to check.
	 * @return True if the type is likely the output governed by the class picker pin, otherwise false.
	 */
	static bool CanConformPinType(const UK2Node_CallFunction* FuncNode, const FEdGraphPinType& TypeToTest);

private:
	UEdGraphPin* MutatingPin;
};

void FDynamicOutputHelper::ConformOutputType() const
{
	if (IsTypePickerPin(MutatingPin))
	{
		UClass* PickedClass = GetPinClass(MutatingPin);
		UK2Node_CallFunction* FuncNode = GetFunctionNode();

		if (UEdGraphPin* DynamicOutPin = GetDynamicOutPin(FuncNode))
		{
			DynamicOutPin->PinType.PinSubCategoryObject = PickedClass;

			// leave the connection, and instead bring the user's attention to 
			// it via a ValidateNodeDuringCompilation() error
// 			const UEdGraphSchema* Schema = FuncNode->GetSchema();
// 			for (int32 LinkIndex = 0; LinkIndex < DynamicOutPin->LinkedTo.Num();)
// 			{
// 				UEdGraphPin* Link = DynamicOutPin->LinkedTo[LinkIndex];
// 				// if this can no longer be linked to the other pin, then we 
// 				// should disconnect it (because the pin's type just changed)
// 				if (Schema->CanCreateConnection(DynamicOutPin, Link).Response == CONNECT_RESPONSE_DISALLOW)
// 				{
// 					DynamicOutPin->BreakLinkTo(Link);
// 					// @TODO: warn/notify somehow
// 				}
// 				else
// 				{
// 					++LinkIndex;
// 				}
// 			}
		}
	}
}

UEdGraphPin* FDynamicOutputHelper::GetTypePickerPin(const UK2Node_CallFunction* FuncNode)
{
	UEdGraphPin* TypePickerPin = nullptr;

	if (UFunction* Function = FuncNode->GetTargetFunction())
	{
		FString TypeDeterminingPinName = Function->GetMetaData(FBlueprintMetadata::MD_DynamicOutputType);
		if (!TypeDeterminingPinName.IsEmpty())
		{
			TypePickerPin = FuncNode->FindPin(TypeDeterminingPinName);
		}
	}

	if (TypePickerPin && !ensure(TypePickerPin->Direction == EGPD_Input))
	{
		TypePickerPin = nullptr;
	}

	return TypePickerPin;
}

UClass* FDynamicOutputHelper::GetPinClass(UEdGraphPin* Pin)
{
	UClass* PinClass = UObject::StaticClass();

	bool const bIsClassOrObjectPin = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object);
	if (bIsClassOrObjectPin)
	{
		if (UClass* DefaultClass = Cast<UClass>(Pin->DefaultObject))
		{
			PinClass = DefaultClass;
		}
		else if (UClass* BaseClass = Cast<UClass>(Pin->PinType.PinSubCategoryObject.Get()))
		{
			PinClass = BaseClass;
		}

		if (Pin->LinkedTo.Num() > 0)
		{
			UClass* CommonInputClass = nullptr;
			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				const FEdGraphPinType& LinkedPinType = LinkedPin->PinType;

				UClass* LinkClass = Cast<UClass>(LinkedPinType.PinSubCategoryObject.Get());
				if (LinkClass == nullptr && LinkedPinType.PinSubCategory == UEdGraphSchema_K2::PSC_Self)
				{
					if (UK2Node* K2Node = Cast<UK2Node>(LinkedPin->GetOwningNode()))
					{
						LinkClass = K2Node->GetBlueprint()->GeneratedClass;
					}
				}

				if (LinkClass != nullptr)
				{
					if (CommonInputClass != nullptr)
					{
						while (!LinkClass->IsChildOf(CommonInputClass))
						{
							CommonInputClass = CommonInputClass->GetSuperClass();
						}
					}
					else
					{
						CommonInputClass = LinkClass;
					}
				}
			}

			PinClass = CommonInputClass;
		}
	}
	return PinClass;
}

void FDynamicOutputHelper::VerifyNode(const UK2Node_CallFunction* FuncNode, FCompilerResultsLog& MessageLog)
{
	if (UEdGraphPin* DynamicOutPin = GetDynamicOutPin(FuncNode))
	{
		const UEdGraphSchema* Schema = FuncNode->GetSchema();
		for (UEdGraphPin* Link : DynamicOutPin->LinkedTo)
		{
			if (Schema->CanCreateConnection(DynamicOutPin, Link).Response == CONNECT_RESPONSE_DISALLOW)
			{
				FText const ErrorFormat = LOCTEXT("BadConnection", "Invalid pin connection from '@@' to '@@'. You may have changed the type after the connections were made.");
				MessageLog.Error(*ErrorFormat.ToString(), DynamicOutPin, Link);
			}
		}
	}
}

UK2Node_CallFunction* FDynamicOutputHelper::GetFunctionNode() const
{
	return CastChecked<UK2Node_CallFunction>(MutatingPin->GetOwningNode());
}

UFunction* FDynamicOutputHelper::GetTargetFunction() const
{
	return GetFunctionNode()->GetTargetFunction();
}

bool FDynamicOutputHelper::IsTypePickerPin(UEdGraphPin* Pin) const
{
	bool bIsTypeDeterminingPin = false;

	if (UFunction* Function = GetTargetFunction())
	{
		FString TypeDeterminingPinName = Function->GetMetaData(FBlueprintMetadata::MD_DynamicOutputType);
		if (!TypeDeterminingPinName.IsEmpty())
		{
			bIsTypeDeterminingPin = (Pin->PinName == TypeDeterminingPinName);
		}
	}

	bool const bPinIsClassPicker = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class);
	bool const bPinIsObjectPicker = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object);
	return bIsTypeDeterminingPin && (bPinIsClassPicker || bPinIsObjectPicker) && (Pin->Direction == EGPD_Input);
}

UEdGraphPin* FDynamicOutputHelper::GetDynamicOutPin(const UK2Node_CallFunction* FuncNode)
{
	UProperty* TaggedOutputParam = nullptr;
	if (UFunction* Function = FuncNode->GetTargetFunction())
	{
		const FString& OutputPinName = Function->GetMetaData(FBlueprintMetadata::MD_DynamicOutputParam);
		// we sort through properties, instead of pins, because the pin's type 
		// could already be modified to some other class (for when we check CanConformPinType)
		for (TFieldIterator<UProperty> ParamIt(Function); ParamIt && (ParamIt->PropertyFlags & CPF_Parm); ++ParamIt)
		{
			if (OutputPinName.IsEmpty() && ParamIt->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				TaggedOutputParam = *ParamIt;
				break;
			}
			else if (OutputPinName == ParamIt->GetName())
			{
				TaggedOutputParam = *ParamIt;
				break;
			}
		}

		if (TaggedOutputParam != nullptr)
		{
			const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
			FEdGraphPinType PropertyPinType;

			if (!K2Schema->ConvertPropertyToPinType(TaggedOutputParam, /*out*/PropertyPinType) || !CanConformPinType(FuncNode, PropertyPinType))
			{
				TaggedOutputParam = nullptr;
			}
		}
	}

	UEdGraphPin* DynamicOutPin = nullptr;
	if (TaggedOutputParam != nullptr)
	{
		DynamicOutPin = FuncNode->FindPin(TaggedOutputParam->GetName());
		if (DynamicOutPin && (DynamicOutPin->Direction != EGPD_Output))
		{
			DynamicOutPin = nullptr;
		}
	}
	return DynamicOutPin;
}

bool FDynamicOutputHelper::CanConformPinType(const UK2Node_CallFunction* FuncNode, const FEdGraphPinType& TypeToTest)
{
	bool bIsProperType = false;
	if (UEdGraphPin* TypePickerPin = GetTypePickerPin(FuncNode))
	{
		UClass* BasePickerClass = CastChecked<UClass>(TypePickerPin->PinType.PinSubCategoryObject.Get());

		const FString& PinCategory = TypeToTest.PinCategory;
		if ((PinCategory == UEdGraphSchema_K2::PC_Object) ||
			(PinCategory == UEdGraphSchema_K2::PC_Interface) ||
			(PinCategory == UEdGraphSchema_K2::PC_Class))
		{
			if (UClass* TypeClass = Cast<UClass>(TypeToTest.PinSubCategoryObject.Get()))
			{
				bIsProperType = BasePickerClass->IsChildOf(TypeClass);
			}
		}
	}
	return bIsProperType;
}

/*******************************************************************************
 *  UK2Node_CallFunction
 ******************************************************************************/

UK2Node_CallFunction::UK2Node_CallFunction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bPinTooltipsValid(false)
{
}


bool UK2Node_CallFunction::IsDeprecated() const
{
	UFunction* Function = GetTargetFunction();
	return (Function != NULL) && Function->HasMetaData(FBlueprintMetadata::MD_DeprecatedFunction);
}

bool UK2Node_CallFunction::ShouldWarnOnDeprecation() const
{
	// TEMP:  Do not warn in the case of SpawnActor, as we have a special upgrade path for those nodes
	return (FunctionReference.GetMemberName() != FName(TEXT("BeginSpawningActorFromBlueprint")));
}

FString UK2Node_CallFunction::GetDeprecationMessage() const
{
	UFunction* Function = GetTargetFunction();
	if ((Function != NULL) && Function->HasMetaData(FBlueprintMetadata::MD_DeprecationMessage))
	{
		return FString::Printf(TEXT("%s %s"), *LOCTEXT("CallFunctionDeprecated_Warning", "@@ is deprecated;").ToString(), *Function->GetMetaData(FBlueprintMetadata::MD_DeprecationMessage));
	}

	return Super::GetDeprecationMessage();
}


FText UK2Node_CallFunction::GetFunctionContextString() const
{
	FText ContextString;

	// Don't show 'target is' if no target pin!
	UEdGraphPin* SelfPin = GetDefault<UEdGraphSchema_K2>()->FindSelfPin(*this, EGPD_Input);
	if(SelfPin != NULL && !SelfPin->bHidden)
	{
		const UFunction* Function = GetTargetFunction();
		UClass* CurrentSelfClass = (Function != NULL) ? Function->GetOwnerClass() : NULL;
		UClass const* TrueSelfClass = CurrentSelfClass;
		if (CurrentSelfClass && CurrentSelfClass->ClassGeneratedBy)
		{
			TrueSelfClass = CurrentSelfClass->GetAuthoritativeClass();
		}

		const FText TargetText = FBlueprintEditorUtils::GetFriendlyClassDisplayName(TrueSelfClass);

		FFormatNamedArguments Args;
		Args.Add(TEXT("TargetName"), TargetText);
		ContextString = FText::Format(LOCTEXT("CallFunctionOnDifferentContext", "Target is {TargetName}"), Args);
	}

	return ContextString;
}


FText UK2Node_CallFunction::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FText FunctionName;
	FText ContextString;
	FText RPCString;

	if (UFunction* Function = GetTargetFunction())
	{
		RPCString = UK2Node_Event::GetLocalizedNetString(Function->FunctionFlags, true);
		FunctionName = GetUserFacingFunctionName(Function);
		ContextString = GetFunctionContextString();
	}
	else
	{
		FunctionName = FText::FromName(FunctionReference.GetMemberName());
		if ((GEditor != NULL) && (GetDefault<UEditorStyleSettings>()->bShowFriendlyNames))
		{
			FunctionName = FText::FromString(FName::NameToDisplayString(FunctionName.ToString(), false));
		}
	}

	if(TitleType == ENodeTitleType::FullTitle)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("FunctionName"), FunctionName);
		Args.Add(TEXT("ContextString"), ContextString);
		Args.Add(TEXT("RPCString"), RPCString);

		if (ContextString.IsEmpty() && RPCString.IsEmpty())
		{
			return FText::Format(LOCTEXT("CallFunction_FullTitle", "{FunctionName}"), Args);
		}
		else if (ContextString.IsEmpty())
		{
			return FText::Format(LOCTEXT("CallFunction_FullTitle_WithRPCString", "{FunctionName}\n{RPCString}"), Args);
		}
		else if (RPCString.IsEmpty())
		{
			return FText::Format(LOCTEXT("CallFunction_FullTitle_WithContextString", "{FunctionName}\n{ContextString}"), Args);
		}
		else
		{
			return FText::Format(LOCTEXT("CallFunction_FullTitle_WithContextRPCString", "{FunctionName}\n{ContextString}\n{RPCString}"), Args);
		}
	}
	else
	{
		return FunctionName;
	}
}

void UK2Node_CallFunction::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	if (!bPinTooltipsValid)
	{
		for (auto& P : Pins)
		{
			P->PinToolTip.Empty();
			GeneratePinTooltip(*P);
		}

		bPinTooltipsValid = true;
	}

	return UK2Node::GetPinHoverText(Pin, HoverTextOut);
}

void UK2Node_CallFunction::AllocateDefaultPins()
{
	InvalidatePinTooltips();

	UBlueprint* MyBlueprint = GetBlueprint();
	
	UFunction* Function = GetTargetFunction();
	// favor the skeleton function if possible (in case the signature has 
	// changed, and not yet compiled).
	if (!FunctionReference.IsSelfContext())
	{
		UClass* FunctionClass = FunctionReference.GetMemberParentClass(MyBlueprint->GeneratedClass);
		if (UBlueprintGeneratedClass* BpClassOwner = Cast<UBlueprintGeneratedClass>(FunctionClass))
		{
			// this function could currently only be a part of some skeleton 
			// class (the blueprint has not be compiled with it yet), so let's 
			// check the skeleton class as well, see if we can pull pin data 
			// from there...
			UBlueprint* FunctionBlueprint = CastChecked<UBlueprint>(BpClassOwner->ClassGeneratedBy, ECastCheckedType::NullAllowed);
			if (FunctionBlueprint)
			{
				if (UFunction* SkelFunction = FindField<UFunction>(FunctionBlueprint->SkeletonGeneratedClass, FunctionReference.GetMemberName()))
				{
					Function = SkelFunction;
				}
			}
		}
	}

	// First try remap table
	if (Function == NULL)
	{
		UClass* ParentClass = FunctionReference.GetMemberParentClass(GetBlueprintClassFromNode());

		if (ParentClass != NULL)
		{
			if (UFunction* NewFunction = Cast<UFunction>(FMemberReference::FindRemappedField(ParentClass, FunctionReference.GetMemberName())))
			{
				// Found a remapped property, update the node
				Function = NewFunction;
				SetFromFunction(NewFunction);
			}
		}
	}

	if (Function == NULL)
	{
		// The function no longer exists in the stored scope
		// Try searching inside all function libraries, in case the function got refactored into one of them.
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			UClass* TestClass = *ClassIt;
			if (TestClass->IsChildOf(UBlueprintFunctionLibrary::StaticClass()))
			{
				Function = FindField<UFunction>(TestClass, FunctionReference.GetMemberName());
				if (Function != NULL)
				{
					UClass* OldClass = FunctionReference.GetMemberParentClass(GetBlueprintClassFromNode());
					Message_Note( FString::Printf(*LOCTEXT("FixedUpFunctionInLibrary", "UK2Node_CallFunction: Fixed up function '%s', originally in '%s', now in library '%s'.").ToString(),
						*FunctionReference.GetMemberName().ToString(),
						 (OldClass != NULL) ? *OldClass->GetName() : TEXT("(null)"), *TestClass->GetName()) );
					SetFromFunction(Function);
					break;
				}
			}
		}
	}

	// Now create the pins if we ended up with a valid function to call
	if (Function != NULL)
	{
		CreatePinsForFunctionCall(Function);
	}

	FCustomStructureParamHelper::UpdateCustomStructurePins(Function, this);

	Super::AllocateDefaultPins();
}

/** Util to find self pin in an array */
UEdGraphPin* FindSelfPin(TArray<UEdGraphPin*>& Pins)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	for(int32 PinIdx=0; PinIdx<Pins.Num(); PinIdx++)
	{
		if(Pins[PinIdx]->PinName == K2Schema->PN_Self)
		{
			return Pins[PinIdx];
		}
	}
	return NULL;
}

void UK2Node_CallFunction::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	// BEGIN TEMP
	// We had a bug where the class was being messed up by copy/paste, but the self pin class was still ok. This code fixes up those cases.
	UFunction* Function = GetTargetFunction();
	if (Function == NULL)
	{
		if (UEdGraphPin* SelfPin = FindSelfPin(OldPins))
		{
			if (UClass* SelfPinClass = Cast<UClass>(SelfPin->PinType.PinSubCategoryObject.Get()))
			{
				if (UFunction* NewFunction = FindField<UFunction>(SelfPinClass, FunctionReference.GetMemberName()))
				{
					SetFromFunction(NewFunction);
				}
			}
		}
	}
	// END TEMP

	Super::ReallocatePinsDuringReconstruction(OldPins);

	// Connect Execute and Then pins for functions, which became pure.
	ReconnectPureExecPins(OldPins);
}

UEdGraphPin* UK2Node_CallFunction::CreateSelfPin(const UFunction* Function)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Chase up the function's Super chain, the function can be called on any object that is at least that specific
	const UFunction* FirstDeclaredFunction = Function;
	while (FirstDeclaredFunction->GetSuperFunction() != NULL)
	{
		FirstDeclaredFunction = FirstDeclaredFunction->GetSuperFunction();
	}

	// Create the self pin
	UClass* FunctionClass = CastChecked<UClass>(FirstDeclaredFunction->GetOuter());
	// we don't want blueprint-function target pins to be formed from the
	// skeleton class (otherwise, they could be incompatible with other pins
	// that represent the same type)... this here could lead to a compiler 
	// warning (the GeneratedClass could not have the function yet), but in
	// that, the user would be reminded to compile the other blueprint
	if (FunctionClass->ClassGeneratedBy)
	{
		FunctionClass = FunctionClass->GetAuthoritativeClass();
	}

	UEdGraphPin* SelfPin = NULL;
	if (FunctionClass == GetBlueprint()->GeneratedClass)
	{
		// This means the function is defined within the blueprint, so the pin should be a true "self" pin
		SelfPin = CreatePin(EGPD_Input, K2Schema->PC_Object, K2Schema->PSC_Self, NULL, false, false, K2Schema->PN_Self);
	}
	else if (FunctionClass->IsChildOf(UInterface::StaticClass()))
	{
		SelfPin = CreatePin(EGPD_Input, K2Schema->PC_Interface, TEXT(""), FunctionClass, false, false, K2Schema->PN_Self);
	}
	else
	{
		// This means that the function is declared in an external class, and should reference that class
		SelfPin = CreatePin(EGPD_Input, K2Schema->PC_Object, TEXT(""), FunctionClass, false, false, K2Schema->PN_Self);
	}
	check(SelfPin != NULL);

	return SelfPin;
}

void UK2Node_CallFunction::CreateExecPinsForFunctionCall(const UFunction* Function)
{
	bool bCreateSingleExecInputPin = true;
	bool bCreateThenPin = true;
	
	// If not pure, create exec pins
	if (!bIsPureFunc)
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		// If we want enum->exec expansion, and it is not disabled, do it now
		if(bWantsEnumToExecExpansion)
		{
			const FString& EnumParamName = Function->GetMetaData(FBlueprintMetadata::MD_ExpandEnumAsExecs);
			UByteProperty* EnumProp = FindField<UByteProperty>(Function, FName(*EnumParamName));
			if(EnumProp != NULL && EnumProp->Enum != NULL)
			{
				const bool bIsFunctionInput = !EnumProp->HasAnyPropertyFlags(CPF_ReturnParm) &&
					(!EnumProp->HasAnyPropertyFlags(CPF_OutParm) ||
					 EnumProp->HasAnyPropertyFlags(CPF_ReferenceParm));
				const EEdGraphPinDirection Direction = bIsFunctionInput ? EGPD_Input : EGPD_Output;
				
				// yay, found it! Now create exec pin for each
				int32 NumExecs = (EnumProp->Enum->NumEnums() - 1);
				for(int32 ExecIdx=0; ExecIdx<NumExecs; ExecIdx++)
				{
					bool const bShouldBeHidden = EnumProp->Enum->HasMetaData(TEXT("Hidden"), ExecIdx) || EnumProp->Enum->HasMetaData(TEXT("Spacer"), ExecIdx);
					if (!bShouldBeHidden)
					{
						FString ExecName = EnumProp->Enum->GetEnumName(ExecIdx);
						CreatePin(Direction, K2Schema->PC_Exec, TEXT(""), NULL, false, false, ExecName);
					}
				}
				
				if (bIsFunctionInput)
				{
					// If using ExpandEnumAsExec for input, don't want to add a input exec pin
					bCreateSingleExecInputPin = false;
				}
				else
				{
					// If using ExpandEnumAsExec for output, don't want to add a "then" pin
					bCreateThenPin = false;
				}
			}
		}
		
		if (bCreateSingleExecInputPin)
		{
			// Single input exec pin
			CreatePin(EGPD_Input, K2Schema->PC_Exec, TEXT(""), NULL, false, false, K2Schema->PN_Execute);
		}

		if (bCreateThenPin)
		{
			UEdGraphPin* OutputExecPin = CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, K2Schema->PN_Then);
			// Use 'completed' name for output pins on latent functions
			if (Function->HasMetaData(FBlueprintMetadata::MD_Latent))
			{
				OutputExecPin->PinFriendlyName = FText::FromString(K2Schema->PN_Completed);
			}
		}
	}
}

void UK2Node_CallFunction::DetermineWantsEnumToExecExpansion(const UFunction* Function)
{
	bWantsEnumToExecExpansion = false;

	if (Function->HasMetaData(FBlueprintMetadata::MD_ExpandEnumAsExecs))
	{
		const FString& EnumParamName = Function->GetMetaData(FBlueprintMetadata::MD_ExpandEnumAsExecs);
		UByteProperty* EnumProp = FindField<UByteProperty>(Function, FName(*EnumParamName));
		if(EnumProp != NULL && EnumProp->Enum != NULL)
		{
			bWantsEnumToExecExpansion = true;
		}
		else
		{
			if (!bHasCompilerMessage)
			{
				//put in warning state
				bHasCompilerMessage = true;
				ErrorType = EMessageSeverity::Warning;
				ErrorMsg = FString::Printf(*LOCTEXT("EnumToExecExpansionFailed", "Unable to find enum parameter with name '%s' to expand for @@").ToString(), *EnumParamName);
			}
		}
	}
}

void UK2Node_CallFunction::GeneratePinTooltip(UEdGraphPin& Pin) const
{
	ensure(Pin.GetOwningNode() == this);

	UEdGraphSchema const* Schema = GetSchema();
	check(Schema != NULL);
	UEdGraphSchema_K2 const* const K2Schema = Cast<const UEdGraphSchema_K2>(Schema);

	if (K2Schema == NULL)
	{
		Schema->ConstructBasicPinTooltip(Pin, FText::GetEmpty(), Pin.PinToolTip);
		return;
	}
	
	// get the class function object associated with this node
	UFunction* Function = GetTargetFunction();
	if (Function == NULL)
	{
		Schema->ConstructBasicPinTooltip(Pin, FText::GetEmpty(), Pin.PinToolTip);
		return;
	}


	GeneratePinTooltipFromFunction(Pin, Function);
}

bool UK2Node_CallFunction::CreatePinsForFunctionCall(const UFunction* Function)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UClass* FunctionOwnerClass = Function->GetOuterUClass();

	bIsInterfaceCall = FunctionOwnerClass->HasAnyClassFlags(CLASS_Interface);
	bIsPureFunc = (Function->HasAnyFunctionFlags(FUNC_BlueprintPure) != false);
	bIsConstFunc = (Function->HasAnyFunctionFlags(FUNC_Const) != false);
	DetermineWantsEnumToExecExpansion(Function);

	// Create input pins
	CreateExecPinsForFunctionCall(Function);

	UEdGraphPin* SelfPin = CreateSelfPin(Function);

	// Renamed self pin to target
	SelfPin->PinFriendlyName = LOCTEXT("Target", "Target");

	const bool bIsProtectedFunc = Function->GetBoolMetaData(FBlueprintMetadata::MD_Protected);
	const bool bIsStaticFunc = Function->HasAllFunctionFlags(FUNC_Static);

	UEdGraph const* const Graph = GetGraph();
	UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	ensure(BP);
	if (BP != nullptr)
	{
		const bool bIsFunctionCompatibleWithSelf = BP->SkeletonGeneratedClass->IsChildOf(FunctionOwnerClass);

		if (bIsStaticFunc)
		{
			// For static methods, wire up the self to the CDO of the class if it's not us
			if (!bIsFunctionCompatibleWithSelf)
			{
				UClass* AuthoritativeClass = FunctionOwnerClass->GetAuthoritativeClass();
				SelfPin->DefaultObject = AuthoritativeClass->GetDefaultObject();
			}

			// Purity doesn't matter with a static function, we can always hide the self pin since we know how to call the method
			SelfPin->bHidden = true;
		}
		else
		{
			if (Function->GetBoolMetaData(FBlueprintMetadata::MD_HideSelfPin))
			{
				SelfPin->bHidden = true;
				SelfPin->bNotConnectable = true;
			}
			else
			{
				// Hide the self pin if the function is compatible with the blueprint class and pure (the !bIsConstFunc portion should be going away soon too hopefully)
				SelfPin->bHidden = (bIsFunctionCompatibleWithSelf && bIsPureFunc && !bIsConstFunc);
			}
		}
	}

	// Build a list of the pins that should be hidden for this function (ones that are automagically filled in by the K2 compiler)
	TSet<FString> PinsToHide;
	TSet<FString> InternalPins;
	FBlueprintEditorUtils::GetHiddenPinsForFunction(Graph, Function, PinsToHide, &InternalPins);

	const bool bShowWorldContextPin = ((PinsToHide.Num() > 0) && BP && BP->ParentClass && BP->ParentClass->HasMetaData(FBlueprintMetadata::MD_ShowWorldContextPin));

	// Create the inputs and outputs
	bool bAllPinsGood = true;
	for (TFieldIterator<UProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		UProperty* Param = *PropIt;
		const bool bIsFunctionInput = !Param->HasAnyPropertyFlags(CPF_ReturnParm) && (!Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm));
		const bool bIsRefParam = Param->HasAnyPropertyFlags(CPF_ReferenceParm) && bIsFunctionInput;

		const EEdGraphPinDirection Direction = bIsFunctionInput ? EGPD_Input : EGPD_Output;

		UEdGraphPin* Pin = CreatePin(Direction, TEXT(""), TEXT(""), NULL, false, bIsRefParam, Param->GetName());
		const bool bPinGood = (Pin != NULL) && K2Schema->ConvertPropertyToPinType(Param, /*out*/ Pin->PinType);

		if (bPinGood)
		{
			// Check for a display name override
			const FString PinDisplayName = Param->GetMetaData(FBlueprintMetadata::MD_DisplayName);
			if (!PinDisplayName.IsEmpty())
			{
				Pin->PinFriendlyName = FText::FromString(PinDisplayName);
			}

			//Flag pin as read only for const reference property
			Pin->bDefaultValueIsIgnored = Param->HasAllPropertyFlags(CPF_ConstParm | CPF_ReferenceParm) && (!Function->HasMetaData(FBlueprintMetadata::MD_AutoCreateRefTerm) || Pin->PinType.bIsArray);

			const bool bAdvancedPin = Param->HasAllPropertyFlags(CPF_AdvancedDisplay);
			Pin->bAdvancedView = bAdvancedPin;
			if(bAdvancedPin && (ENodeAdvancedPins::NoPins == AdvancedPinDisplay))
			{
				AdvancedPinDisplay = ENodeAdvancedPins::Hidden;
			}

			K2Schema->SetPinDefaultValue(Pin, Function, Param);
			
			if (PinsToHide.Contains(Pin->PinName))
			{
				FString const DefaultToSelfMetaValue = Function->GetMetaData(FBlueprintMetadata::MD_DefaultToSelf);
				FString const WorldContextMetaValue  = Function->GetMetaData(FBlueprintMetadata::MD_WorldContext);
				bool bIsSelfPin = ((Pin->PinName == DefaultToSelfMetaValue) || (Pin->PinName == WorldContextMetaValue));

				if (!bShowWorldContextPin || !bIsSelfPin)
				{
					Pin->bHidden = true;
					Pin->bNotConnectable = InternalPins.Contains(Pin->PinName);
				}
			}

			PostParameterPinCreated(Pin);
		}

		bAllPinsGood = bAllPinsGood && bPinGood;
	}

	// If we have an 'enum to exec' parameter, set its default value to something valid so we don't get warnings
	if(bWantsEnumToExecExpansion)
	{
		FString EnumParamName = Function->GetMetaData(FBlueprintMetadata::MD_ExpandEnumAsExecs);
		UEdGraphPin* EnumParamPin = FindPin(EnumParamName);
		if (UEnum* PinEnum = (EnumParamPin ? Cast<UEnum>(EnumParamPin->PinType.PinSubCategoryObject.Get()) : NULL))
		{
			EnumParamPin->DefaultValue = PinEnum->GetEnumName(0);
		}
	}

	return bAllPinsGood;
}

void UK2Node_CallFunction::PostReconstructNode()
{
	Super::PostReconstructNode();
	InvalidatePinTooltips();

	FCustomStructureParamHelper::UpdateCustomStructurePins(GetTargetFunction(), this);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	// Fixup self node, may have been overridden from old self node
	UFunction* Function = GetTargetFunction();
	const bool bIsStaticFunc = Function ? Function->HasAllFunctionFlags(FUNC_Static) : false;

	UEdGraphPin* SelfPin = FindPin(K2Schema->PN_Self);
	if (bIsStaticFunc && SelfPin)
	{
		// Wire up the self to the CDO of the class if it's not us
		if (UBlueprint* BP = GetBlueprint())
		{
			UClass* FunctionOwnerClass = Function->GetOuterUClass();
			if (!BP->SkeletonGeneratedClass->IsChildOf(FunctionOwnerClass))
			{
				SelfPin->DefaultObject = FunctionOwnerClass->GetDefaultObject();
			}
			else
			{
				// In case a non-NULL reference was previously serialized on load, ensure that it's set to NULL here to match what a new node's self pin would be initialized as (see CreatePinsForFunctionCall).
				SelfPin->DefaultObject = nullptr;
			}
		}
	}

	// Set the return type to the right class of component
	UActorComponent* TemplateComp = GetTemplateFromNode();
	UEdGraphPin* ReturnPin = GetReturnValuePin();
	if(TemplateComp && ReturnPin)
	{
		ReturnPin->PinType.PinSubCategoryObject = TemplateComp->GetClass();
	}

	if (UEdGraphPin* TypePickerPin = FDynamicOutputHelper::GetTypePickerPin(this))
	{
		FDynamicOutputHelper(TypePickerPin).ConformOutputType();
	}

	if (IsNodePure())
	{
		// Remove any pre-existing breakpoint on this node since pure nodes cannot have breakpoints
		if (UBreakpoint* ExistingBreakpoint = FKismetDebugUtilities::FindBreakpointForNode(GetBlueprint(), this))
		{
			// Remove the breakpoint
			FKismetDebugUtilities::StartDeletingBreakpoint(ExistingBreakpoint, GetBlueprint());
		}
	}
}

void UK2Node_CallFunction::DestroyNode()
{
	// See if this node has a template
	UActorComponent* Template = GetTemplateFromNode();
	if (Template != NULL)
	{
		// Get the blueprint so we can remove it from it
		UBlueprint* BlueprintObj = GetBlueprint();

		// remove it
		BlueprintObj->ComponentTemplates.Remove(Template);
	}

	Super::DestroyNode();
}

void UK2Node_CallFunction::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	if (Pin)
	{
		FCustomStructureParamHelper::UpdateCustomStructurePins(GetTargetFunction(), this, Pin);

		// Refresh the node to hide internal-only pins once the [invalid] connection has been broken
		if (Pin->bHidden && Pin->bNotConnectable && Pin->LinkedTo.Num() == 0)
		{
			GetGraph()->NotifyGraphChanged();
		}
	}

	if (bIsBeadFunction)
	{
		if (Pin->LinkedTo.Num() == 0)
		{
			// Commit suicide; bead functions must always have an input and output connection
			DestroyNode();
		}
	}

	InvalidatePinTooltips();
	FDynamicOutputHelper(Pin).ConformOutputType();
}

void UK2Node_CallFunction::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);
	InvalidatePinTooltips();
	FDynamicOutputHelper(Pin).ConformOutputType();
}

UFunction* UK2Node_CallFunction::GetTargetFunction() const
{
	UFunction* Function = FunctionReference.ResolveMember<UFunction>(GetBlueprintClassFromNode());
	return Function;
}

UFunction* UK2Node_CallFunction::GetTargetFunctionFromSkeletonClass() const
{
	UFunction* TargetFunction = nullptr;
	UClass* ParentClass = FunctionReference.GetMemberParentClass( GetBlueprintClassFromNode() );
	UBlueprint* OwningBP = ParentClass ? Cast<UBlueprint>( ParentClass->ClassGeneratedBy ) : nullptr;
	if( UClass* SkeletonClass = OwningBP ? OwningBP->SkeletonGeneratedClass : nullptr )
	{
		TargetFunction = SkeletonClass->FindFunctionByName( FunctionReference.GetMemberName() );
	}
	return TargetFunction;
}

UEdGraphPin* UK2Node_CallFunction::GetThenPin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPin(K2Schema->PN_Then);
	check(Pin == NULL || Pin->Direction == EGPD_Output); // If pin exists, it must be output
	return Pin;
}

UEdGraphPin* UK2Node_CallFunction::GetReturnValuePin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPin(K2Schema->PN_ReturnValue);
	check(Pin == NULL || Pin->Direction == EGPD_Output); // If pin exists, it must be output
	return Pin;
}

bool UK2Node_CallFunction::IsLatentFunction() const
{
	if (UFunction* Function = GetTargetFunction())
	{
		if (Function->HasMetaData(FBlueprintMetadata::MD_Latent))
		{
			return true;
		}
	}

	return false;
}

bool UK2Node_CallFunction::AllowMultipleSelfs(bool bInputAsArray) const
{
	if (UFunction* Function = GetTargetFunction())
	{
		return CanFunctionSupportMultipleTargets(Function);
	}

	return Super::AllowMultipleSelfs(bInputAsArray);
}

bool UK2Node_CallFunction::CanFunctionSupportMultipleTargets(UFunction const* Function)
{
	bool const bIsImpure = !Function->HasAnyFunctionFlags(FUNC_BlueprintPure);
	bool const bIsLatent = Function->HasMetaData(FBlueprintMetadata::MD_Latent);
	bool const bHasReturnParam = (Function->GetReturnProperty() != nullptr);

	return !bHasReturnParam && bIsImpure && !bIsLatent;
}

bool UK2Node_CallFunction::CanPasteHere(const UEdGraph* TargetGraph) const
{
	// Basic check for graph compatibility, etc.
	bool bCanPaste = Super::CanPasteHere(TargetGraph);

	// We check function context for placability only in the base class case; derived classes are typically bound to
	// specific functions that should always be placeable, but may not always be explicitly callable (e.g. InternalUseOnly).
	if(bCanPaste && GetClass() == StaticClass())
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		uint32 AllowedFunctionTypes = UEdGraphSchema_K2::EFunctionType::FT_Pure | UEdGraphSchema_K2::EFunctionType::FT_Const | UEdGraphSchema_K2::EFunctionType::FT_Protected;
		if(K2Schema->DoesGraphSupportImpureFunctions(TargetGraph))
		{
			AllowedFunctionTypes |= UEdGraphSchema_K2::EFunctionType::FT_Imperative;
		}
		UFunction* TargetFunction = GetTargetFunction();
		if( !TargetFunction )
		{
			TargetFunction = GetTargetFunctionFromSkeletonClass();
		}
		if (!TargetFunction)
		{
			// If the function doesn't exist and it is from self context, then it could be created from a CustomEvent node, that was also pasted (but wasn't compiled yet).
			bCanPaste = FunctionReference.IsSelfContext();
		}
		else
		{
			bCanPaste = K2Schema->CanFunctionBeUsedInGraph(FBlueprintEditorUtils::FindBlueprintForGraphChecked(TargetGraph)->GeneratedClass, TargetFunction, TargetGraph, AllowedFunctionTypes, false);
		}
	}
	
	return bCanPaste;
}

bool UK2Node_CallFunction::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
{
	bool bIsFilteredOut = false;
	for(UEdGraph* TargetGraph : Filter.Context.Graphs)
	{
		bIsFilteredOut |= !CanPasteHere(TargetGraph);
	}

	return bIsFilteredOut;
}

static FLinearColor GetPalletteIconColor(UFunction const* Function)
{
	bool const bIsPure = (Function != nullptr) && Function->HasAnyFunctionFlags(FUNC_BlueprintPure);
	if (bIsPure)
	{
		return GetDefault<UGraphEditorSettings>()->PureFunctionCallNodeTitleColor;
	}
	return GetDefault<UGraphEditorSettings>()->FunctionCallNodeTitleColor;
}

FSlateIcon UK2Node_CallFunction::GetPaletteIconForFunction(UFunction const* Function, FLinearColor& OutColor)
{
	static const FName NativeMakeFunc(TEXT("NativeMakeFunc"));
	static const FName NativeBrakeFunc(TEXT("NativeBreakFunc"));

	if (Function && Function->HasMetaData(NativeMakeFunc))
	{
		static FSlateIcon Icon("EditorStyle", "GraphEditor.MakeStruct_16x");
		return Icon;
	}
	else if (Function && Function->HasMetaData(NativeBrakeFunc))
	{
		static FSlateIcon Icon("EditorStyle", "GraphEditor.BreakStruct_16x");
		return Icon;
	}
	// Check to see if the function is calling an function that could be an event, display the event icon instead.
	else if (Function && UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Function))
	{
		static FSlateIcon Icon("EditorStyle", "GraphEditor.Event_16x");
		return Icon;
	}
	else
	{
		OutColor = GetPalletteIconColor(Function);

		static FSlateIcon Icon("EditorStyle", "Kismet.AllClasses.FunctionIcon");
		return Icon;
	}
}

FLinearColor UK2Node_CallFunction::GetNodeTitleColor() const
{
	return GetPalletteIconColor(GetTargetFunction());
}

FText UK2Node_CallFunction::GetTooltipText() const
{
	FText Tooltip;

	UFunction* Function = GetTargetFunction();
	if (Function == nullptr)
	{
		return FText::Format(LOCTEXT("CallUnknownFunction", "Call unknown function {0}"), FText::FromName(FunctionReference.GetMemberName()));
	}
	else if (CachedTooltip.IsOutOfDate(this))
	{
		FText BaseTooltip = FText::FromString(GetDefaultTooltipForFunction(Function));

		FFormatNamedArguments Args;
		Args.Add(TEXT("DefaultTooltip"), BaseTooltip);

		if (Function->HasAllFunctionFlags(FUNC_BlueprintAuthorityOnly))
		{
			Args.Add(
				TEXT("ClientString"),
				NSLOCTEXT("K2Node", "ServerFunction", "Authority Only. This function will only execute on the server.")
			);
			// FText::Format() is slow, so we cache this to save on performance
			CachedTooltip.SetCachedText(FText::Format(LOCTEXT("CallFunction_SubtitledTooltip", "{DefaultTooltip}\n\n{ClientString}"), Args), this);
		}
		else if (Function->HasAllFunctionFlags(FUNC_BlueprintCosmetic))
		{
			Args.Add(
				TEXT("ClientString"),
				NSLOCTEXT("K2Node", "ClientFunction", "Cosmetic. This event is only for cosmetic, non-gameplay actions.")
			);
			// FText::Format() is slow, so we cache this to save on performance
			CachedTooltip.SetCachedText(FText::Format(LOCTEXT("CallFunction_SubtitledTooltip", "{DefaultTooltip}\n\n{ClientString}"), Args), this);
		} 
		else
		{
			CachedTooltip.SetCachedText(BaseTooltip, this);
		}
	}
	return CachedTooltip;
}

void UK2Node_CallFunction::GeneratePinTooltipFromFunction(UEdGraphPin& Pin, const UFunction* Function)
{
	if (Pin.bWasTrashed)
	{
		return;
	}

	// figure what tag we should be parsing for (is this a return-val pin, or a parameter?)
	FString ParamName;
	FString TagStr = TEXT("@param");
	const bool bReturnPin = Pin.PinName == UEdGraphSchema_K2::PN_ReturnValue;
	if (bReturnPin)
	{
		TagStr = TEXT("@return");
	}
	else
	{
		ParamName = Pin.PinName.ToLower();
	}

	// grab the the function's comment block for us to parse
	FString FunctionToolTipText = Function->GetToolTipText().ToString();
	
	int32 CurStrPos = INDEX_NONE;
	int32 FullToolTipLen = FunctionToolTipText.Len();
	// parse the full function tooltip text, looking for tag lines
	do 
	{
		CurStrPos = FunctionToolTipText.Find(TagStr, ESearchCase::IgnoreCase, ESearchDir::FromStart, CurStrPos);
		if (CurStrPos == INDEX_NONE) // if the tag wasn't found
		{
			break;
		}

		// advance past the tag
		CurStrPos += TagStr.Len();

		// handle people having done @returns instead of @return
		if (bReturnPin && CurStrPos < FullToolTipLen && FunctionToolTipText[CurStrPos] == TEXT('s'))
		{
			++CurStrPos;
		}

		// advance past whitespace
		while(CurStrPos < FullToolTipLen && FChar::IsWhitespace(FunctionToolTipText[CurStrPos]))
		{
			++CurStrPos;
		}

		// if this is a parameter pin
		if (!ParamName.IsEmpty())
		{
			FString TagParamName;

			// copy the parameter name
			while (CurStrPos < FullToolTipLen && !FChar::IsWhitespace(FunctionToolTipText[CurStrPos]))
			{
				TagParamName.AppendChar(FunctionToolTipText[CurStrPos++]);
			}

			// if this @param tag doesn't match the param we're looking for
			if (TagParamName != ParamName)
			{
				continue;
			}
		}

		// advance past whitespace (get to the meat of the comment)
		// since many doxygen style @param use the format "@param <param name> - <comment>" we also strip - if it is before we get to any other non-whitespace
		while(CurStrPos < FullToolTipLen && (FChar::IsWhitespace(FunctionToolTipText[CurStrPos]) || FunctionToolTipText[CurStrPos] == '-'))
		{
			++CurStrPos;
		}


		FString ParamDesc;
		// collect the param/return-val description
		while (CurStrPos < FullToolTipLen && FunctionToolTipText[CurStrPos] != TEXT('@'))
		{
			// advance past newline
			while(CurStrPos < FullToolTipLen && FChar::IsLinebreak(FunctionToolTipText[CurStrPos]))
			{
				++CurStrPos;

				// advance past whitespace at the start of a new line
				while(CurStrPos < FullToolTipLen && FChar::IsWhitespace(FunctionToolTipText[CurStrPos]))
				{
					++CurStrPos;
				}

				// replace the newline with a single space
				if(!FChar::IsLinebreak(FunctionToolTipText[CurStrPos]))
				{
					ParamDesc.AppendChar(TEXT(' '));
				}
			}

			if (FunctionToolTipText[CurStrPos] != TEXT('@'))
			{
				ParamDesc.AppendChar(FunctionToolTipText[CurStrPos++]);
			}
		}

		// trim any trailing whitespace from the descriptive text
		ParamDesc.TrimTrailing();

		// if we came up with a valid description for the param/return-val
		if (!ParamDesc.IsEmpty())
		{
			Pin.PinToolTip += ParamDesc;
			break; // we found a match, so there's no need to continue
		}

	} while (CurStrPos < FullToolTipLen);

	GetDefault<UEdGraphSchema_K2>()->ConstructBasicPinTooltip(Pin, FText::FromString(Pin.PinToolTip), Pin.PinToolTip);
}

FText UK2Node_CallFunction::GetUserFacingFunctionName(const UFunction* Function)
{
	FText ReturnDisplayName;

	if (Function != NULL)
	{
		if (GEditor && GetDefault<UEditorStyleSettings>()->bShowFriendlyNames)
		{
			ReturnDisplayName = Function->GetDisplayNameText();
		}
		else
		{
			static const FString Namespace = TEXT("UObjectDisplayNames");
			const FString Key = Function->GetFullGroupName(false);

			ReturnDisplayName = Function->GetMetaDataText(TEXT("DisplayName"), Namespace, Key);
		}
	}
	return ReturnDisplayName;
}

FString UK2Node_CallFunction::GetDefaultTooltipForFunction(const UFunction* Function)
{
	FString Tooltip;

	if (Function != NULL)
	{
		Tooltip = Function->GetToolTipText().ToString();
	}

	if (!Tooltip.IsEmpty())
	{
		// Strip off the doxygen nastiness
		static const FString DoxygenParam(TEXT("@param"));
		static const FString DoxygenReturn(TEXT("@return"));
		static const FString DoxygenSee(TEXT("@see"));
		static const FString TooltipSee(TEXT("See:"));
		static const FString DoxygenNote(TEXT("@note"));
		static const FString TooltipNote(TEXT("Note:"));

		Tooltip.Split(DoxygenParam, &Tooltip, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		Tooltip.Split(DoxygenReturn, &Tooltip, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		Tooltip.ReplaceInline(*DoxygenSee, *TooltipSee);
		Tooltip.ReplaceInline(*DoxygenNote, *TooltipNote);

		Tooltip.Trim();
		Tooltip.TrimTrailing();

		UClass* CurrentSelfClass = (Function != NULL) ? Function->GetOwnerClass() : NULL;
		UClass const* TrueSelfClass = CurrentSelfClass;
		if (CurrentSelfClass && CurrentSelfClass->ClassGeneratedBy)
		{
			TrueSelfClass = CurrentSelfClass->GetAuthoritativeClass();
		}

		FText TargetDisplayText = (TrueSelfClass != NULL) ? TrueSelfClass->GetDisplayNameText() : LOCTEXT("None", "None");

		FFormatNamedArguments Args;
		Args.Add(TEXT("TargetName"), TargetDisplayText);
		Args.Add(TEXT("Tooltip"), FText::FromString(Tooltip));
		return FText::Format(LOCTEXT("CallFunction_Tooltip", "{Tooltip}\n\nTarget is {TargetName}"), Args).ToString();
	}
	else
	{
		return GetUserFacingFunctionName(Function).ToString();
	}
}

FText UK2Node_CallFunction::GetDefaultCategoryForFunction(const UFunction* Function, const FText& BaseCategory)
{
	FText NodeCategory = BaseCategory;
	if( Function->HasMetaData(FBlueprintMetadata::MD_FunctionCategory) )
	{
		FText FuncCategory;
		// If we are not showing friendly names, return the metadata stored, without localization
		if( GEditor && !GetDefault<UEditorStyleSettings>()->bShowFriendlyNames )
		{
			FuncCategory = FText::FromString(Function->GetMetaData(FBlueprintMetadata::MD_FunctionCategory));
		}
		else
		{
			// Look for localized metadata
			FuncCategory = Function->GetMetaDataText(FBlueprintMetadata::MD_FunctionCategory, TEXT("UObjectCategory"), Function->GetFullGroupName(false));

			// If the result is culture invariant, force it into a display string
			if (FuncCategory.IsCultureInvariant())
			{
				FuncCategory = FText::FromString(FName::NameToDisplayString(FuncCategory.ToString(), false));
			}
		}

		// Combine with the BaseCategory to form the full category, delimited by "|"
		if (!FuncCategory.IsEmpty() && !NodeCategory.IsEmpty())
		{
			NodeCategory = FText::Format(FText::FromString(TEXT("{0}|{1}")), NodeCategory, FuncCategory);
		}
		else if (NodeCategory.IsEmpty())
		{
			NodeCategory = FuncCategory;
		}
	}
	return NodeCategory;
}


FText UK2Node_CallFunction::GetKeywordsForFunction(const UFunction* Function)
{
	// If the friendly name and real function name do not match add the real function name friendly name as a keyword.
	FString Keywords;
	if( Function->GetName() != GetUserFacingFunctionName(Function).ToString() )
	{
		Keywords = Function->GetName();
	}

	if (ShouldDrawCompact(Function))
	{
		Keywords.AppendChar(TEXT(' '));
		Keywords += GetCompactNodeTitle(Function);
	}

	FText MetadataKeywords = Function->GetMetaDataText(FBlueprintMetadata::MD_FunctionKeywords, TEXT("UObjectKeywords"), Function->GetFullGroupName(false));
	FText ResultKeywords;

	if (!MetadataKeywords.IsEmpty())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Name"), FText::FromString(Keywords));
		Args.Add(TEXT("MetadataKeywords"), MetadataKeywords);
		ResultKeywords = FText::Format(FText::FromString("{Name} {MetadataKeywords}"), Args);
	}
	else
	{
		ResultKeywords = FText::FromString(Keywords);
	}

	return ResultKeywords;
}

void UK2Node_CallFunction::SetFromFunction(const UFunction* Function)
{
	if (Function != NULL)
	{
		bIsPureFunc = Function->HasAnyFunctionFlags(FUNC_BlueprintPure);
		bIsConstFunc = Function->HasAnyFunctionFlags(FUNC_Const);
		DetermineWantsEnumToExecExpansion(Function);

		FunctionReference.SetFromField<UFunction>(Function, GetBlueprintClassFromNode());
	}
}

FString UK2Node_CallFunction::GetDocumentationLink() const
{
	UClass* ParentClass = NULL;
	if (FunctionReference.IsSelfContext())
	{
		if (HasValidBlueprint())
		{
			UFunction* Function = FindField<UFunction>(GetBlueprint()->GeneratedClass, FunctionReference.GetMemberName());
			if (Function != NULL)
			{
				ParentClass = Function->GetOwnerClass();
			}
		}		
	}
	else 
	{
		ParentClass = FunctionReference.GetMemberParentClass(GetBlueprintClassFromNode());
	}
	
	if (ParentClass != NULL)
	{
		return FString::Printf(TEXT("Shared/GraphNodes/Blueprint/%s%s"), ParentClass->GetPrefixCPP(), *ParentClass->GetName());
	}

	return FString("Shared/GraphNodes/Blueprint/UK2Node_CallFunction");
}

FString UK2Node_CallFunction::GetDocumentationExcerptName() const
{
	return FunctionReference.GetMemberName().ToString();
}

FString UK2Node_CallFunction::GetDescriptiveCompiledName() const
{
	return FString(TEXT("CallFunc_")) + FunctionReference.GetMemberName().ToString();
}

bool UK2Node_CallFunction::ShouldDrawCompact(const UFunction* Function)
{
	return (Function != NULL) && Function->HasMetaData(FBlueprintMetadata::MD_CompactNodeTitle);
}

bool UK2Node_CallFunction::ShouldDrawCompact() const
{
	UFunction* Function = GetTargetFunction();

	return ShouldDrawCompact(Function);
}

bool UK2Node_CallFunction::ShouldDrawAsBead() const
{
	return bIsBeadFunction;
}

bool UK2Node_CallFunction::ShouldShowNodeProperties() const
{
	// Show node properties if this corresponds to a function graph
	if (FunctionReference.GetMemberName() != NAME_None)
	{
		return FindObject<UEdGraph>(GetBlueprint(), *(FunctionReference.GetMemberName().ToString())) != NULL;
	}
	return false;
}

FString UK2Node_CallFunction::GetCompactNodeTitle(const UFunction* Function)
{
	static const FString ProgrammerMultiplicationSymbol = TEXT("*");
	static const FString CommonMultiplicationSymbol = TEXT("\xD7");

	static const FString ProgrammerDivisionSymbol = TEXT("/");
	static const FString CommonDivisionSymbol = TEXT("\xF7");

	static const FString ProgrammerConversionSymbol = TEXT("->");
	static const FString CommonConversionSymbol = TEXT("\x2022");

	const FString OperatorTitle = Function->GetMetaData(FBlueprintMetadata::MD_CompactNodeTitle);
	if (!OperatorTitle.IsEmpty())
	{
		if (OperatorTitle == ProgrammerMultiplicationSymbol)
		{
			return CommonMultiplicationSymbol;
		}
		else if (OperatorTitle == ProgrammerDivisionSymbol)
		{
			return CommonDivisionSymbol;
		}
		else if (OperatorTitle == ProgrammerConversionSymbol)
		{
			return CommonConversionSymbol;
		}
		else
		{
			return OperatorTitle;
		}
	}
	
	return Function->GetName();
}

FText UK2Node_CallFunction::GetCompactNodeTitle() const
{
	UFunction* Function = GetTargetFunction();
	if (Function != NULL)
	{
		return FText::FromString(GetCompactNodeTitle(Function));
	}
	else
	{
		return Super::GetCompactNodeTitle();
	}
}

void UK2Node_CallFunction::GetRedirectPinNames(const UEdGraphPin& Pin, TArray<FString>& RedirectPinNames) const
{
	Super::GetRedirectPinNames(Pin, RedirectPinNames);

	if (RedirectPinNames.Num() > 0)
	{
		const FString OldPinName = RedirectPinNames[0];

		// first add functionname.param
		RedirectPinNames.Add(FString::Printf(TEXT("%s.%s"), *FunctionReference.GetMemberName().ToString(), *OldPinName));

		// if there is class, also add an option for class.functionname.param
		UClass* FunctionClass = FunctionReference.GetMemberParentClass(GetBlueprintClassFromNode());
		while (FunctionClass)
		{
			RedirectPinNames.Add(FString::Printf(TEXT("%s.%s.%s"), *FunctionClass->GetName(), *FunctionReference.GetMemberName().ToString(), *OldPinName));
			FunctionClass = FunctionClass->GetSuperClass();
		}
	}
}

void UK2Node_CallFunction::FixupSelfMemberContext()
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this);
	auto IsBlueprintOfType = [Blueprint](UClass* ClassType)->bool
	{
		bool bIsChildOf  = Blueprint && (Blueprint->GeneratedClass != nullptr) && Blueprint->GeneratedClass->IsChildOf(ClassType);
		if (!bIsChildOf && Blueprint && (Blueprint->SkeletonGeneratedClass))
		{
			bIsChildOf = Blueprint->SkeletonGeneratedClass->IsChildOf(ClassType);
		}
		return bIsChildOf;
	};

	UClass* MemberClass = FunctionReference.GetMemberParentClass();
	const bool bIsLocalMacro = Blueprint && Blueprint->MacroGraphs.Contains(GetGraph());
	ensureMsgf(FunctionReference.IsSelfContext() || (MemberClass != nullptr) || bIsLocalMacro, TEXT("Unknown member class in %s"), *GetPathName());
	if (FunctionReference.IsSelfContext())
	{
		if (MemberClass == nullptr)
		{
			// the self pin may have type information stored on it
			if (UEdGraphPin* SelfPin = GetDefault<UEdGraphSchema_K2>()->FindSelfPin(*this, EGPD_Input))
			{
				MemberClass = Cast<UClass>(SelfPin->PinType.PinSubCategoryObject.Get());
			}
		}
		// if we happened to retain the ParentClass for a self reference 
		// (unlikely), then we know where this node came from... let's keep it
		// referencing that function
		if (MemberClass != nullptr)
		{
			if (!IsBlueprintOfType(MemberClass))
			{
				FunctionReference.SetExternalMember(FunctionReference.GetMemberName(), MemberClass);
			}
		}
		// else, there is nothing we can do... if there is an function matching 
		// the member name in this Blueprint, then it will reference that 
		// function (even if it came from a different Blueprint, one with an 
		// identically named function)... if there is no function matching this 
		// reference, then the node will produce an error later during compilation
	}
	else if (MemberClass != nullptr)
	{
		if (IsBlueprintOfType(MemberClass))
		{
			FunctionReference.SetSelfMember(FunctionReference.GetMemberName());
		}
	}
}

void UK2Node_CallFunction::PostPasteNode()
{
	Super::PostPasteNode();
	FixupSelfMemberContext();

	UFunction* Function = GetTargetFunction();
	if(Function != NULL)
	{
		// After pasting we need to go through and ensure the hidden the self pins is correct in case the source blueprint had different metadata
		TSet<FString> PinsToHide;
		FBlueprintEditorUtils::GetHiddenPinsForFunction(GetGraph(), Function, PinsToHide);

		const bool bShowWorldContextPin = ((PinsToHide.Num() > 0) && GetBlueprint()->ParentClass->HasMetaData(FBlueprintMetadata::MD_ShowWorldContextPin));

		FString const DefaultToSelfMetaValue = Function->GetMetaData(FBlueprintMetadata::MD_DefaultToSelf);
		FString const WorldContextMetaValue  = Function->GetMetaData(FBlueprintMetadata::MD_WorldContext);

		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
		{
			UEdGraphPin* Pin = Pins[PinIndex];

			bool bIsSelfPin = ((Pin->PinName == DefaultToSelfMetaValue) || (Pin->PinName == WorldContextMetaValue));
			bool bPinShouldBeHidden = PinsToHide.Contains(Pin->PinName) && (!bShowWorldContextPin || !bIsSelfPin);

			if (bPinShouldBeHidden && !Pin->bHidden)
			{
				Pin->BreakAllPinLinks();
				K2Schema->SetPinDefaultValueBasedOnType(Pin);
			}
			Pin->bHidden = bPinShouldBeHidden;
		}
	}
}

void UK2Node_CallFunction::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	if (!bDuplicateForPIE && (!this->HasAnyFlags(RF_Transient)))
	{
		FixupSelfMemberContext();
	}
}

void UK2Node_CallFunction::ValidateNodeAfterPrune(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeAfterPrune(MessageLog);

	const UBlueprint* Blueprint = GetBlueprint();
	UFunction *Function = GetTargetFunction();
	if (Function == NULL)
	{
		FString OwnerName;

		if (Blueprint != nullptr)
		{
			OwnerName = Blueprint->GetName();
			if (UClass* FuncOwnerClass = FunctionReference.GetMemberParentClass(Blueprint->GeneratedClass))
			{
				OwnerName = FuncOwnerClass->GetName();
			}
		}
		FString const FunctName = FunctionReference.GetMemberName().ToString();

		FText const WarningFormat = LOCTEXT("FunctionNotFound", "Could not find a function named \"%s\" in '%s'.\nMake sure '%s' has been compiled for @@");
		MessageLog.Error(*FString::Printf(*WarningFormat.ToString(), *FunctName, *OwnerName, *OwnerName), this);
	}
	else if (Function->HasMetaData(FBlueprintMetadata::MD_ExpandEnumAsExecs) && bWantsEnumToExecExpansion == false)
	{
		const FString& EnumParamName = Function->GetMetaData(FBlueprintMetadata::MD_ExpandEnumAsExecs);
		MessageLog.Warning(*FString::Printf(*LOCTEXT("EnumToExecExpansionFailed", "Unable to find enum parameter with name '%s' to expand for @@").ToString(), *EnumParamName), this);
	}

	if (Function)
	{
		// enforce UnsafeDuringActorConstruction keyword
		if (Function->HasMetaData(FBlueprintMetadata::MD_UnsafeForConstructionScripts))
		{
			// emit warning if we are in a construction script
			UEdGraph const* const Graph = GetGraph();
			UEdGraphSchema_K2 const* const Schema = Cast<const UEdGraphSchema_K2>(GetSchema());
			bool bNodeIsInConstructionScript = Schema && Schema->IsConstructionScript(Graph);

			if (bNodeIsInConstructionScript == false)
			{
				// IsConstructionScript() can return false if graph was cloned from the construction script
				// in that case, check the function entry
				TArray<const UK2Node_FunctionEntry*> EntryPoints;
				Graph->GetNodesOfClass(EntryPoints);

				if (EntryPoints.Num() == 1)
				{
					UK2Node_FunctionEntry const* const Node = EntryPoints[0];
					if (Node)
					{
						UFunction* const SignatureFunction = FindField<UFunction>(Node->SignatureClass, Node->SignatureName);
						bNodeIsInConstructionScript = SignatureFunction && (SignatureFunction->GetFName() == Schema->FN_UserConstructionScript);
					}
				}
			}

			if ( bNodeIsInConstructionScript )
			{
				MessageLog.Warning(*LOCTEXT("FunctionUnsafeDuringConstruction", "Function '@@' is unsafe to call in a construction script.").ToString(), this);
			}
		}

		// enforce WorldContext restrictions
		const bool bInsideBpFuncLibrary = Blueprint && (BPTYPE_FunctionLibrary == Blueprint->BlueprintType);
		if (!bInsideBpFuncLibrary && 
			Function->HasMetaData(FBlueprintMetadata::MD_WorldContext) && 
			!Function->HasMetaData(FBlueprintMetadata::MD_CallableWithoutWorldContext))
		{
			check(Blueprint);
			UClass* ParentClass = Blueprint->ParentClass;
			check(ParentClass);
			if (ParentClass && !ParentClass->GetDefaultObject()->ImplementsGetWorld() && !ParentClass->HasMetaData(FBlueprintMetadata::MD_ShowWorldContextPin))
			{
				MessageLog.Warning(*LOCTEXT("FunctionUnsafeInContext", "Function '@@' is unsafe to call from blueprints of class '@@'.").ToString(), this, ParentClass);
			}
		}
	}

	FDynamicOutputHelper::VerifyNode(this, MessageLog);
}

void UK2Node_CallFunction::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		if (Ar.UE4Ver() < VER_UE4_SWITCH_CALL_NODE_TO_USE_MEMBER_REFERENCE)
		{
			UFunction* Function = FindField<UFunction>(CallFunctionClass_DEPRECATED, CallFunctionName_DEPRECATED);
			const bool bProbablySelfCall = (CallFunctionClass_DEPRECATED == NULL) || ((Function != NULL) && (Function->GetOuterUClass()->ClassGeneratedBy == GetBlueprint()));

			FunctionReference.SetDirect(CallFunctionName_DEPRECATED, FGuid(), CallFunctionClass_DEPRECATED, bProbablySelfCall);
		}

		if(Ar.UE4Ver() < VER_UE4_K2NODE_REFERENCEGUIDS)
		{
			FGuid FunctionGuid;

			if (UBlueprint::GetGuidFromClassByFieldName<UFunction>(GetBlueprint()->GeneratedClass, FunctionReference.GetMemberName(), FunctionGuid))
			{
				const bool bSelf = FunctionReference.IsSelfContext();
				FunctionReference.SetDirect(FunctionReference.GetMemberName(), FunctionGuid, (bSelf ? NULL : FunctionReference.GetMemberParentClass((UClass*)NULL)), bSelf);
			}
		}

		if (!Ar.IsObjectReferenceCollector())
		{
			// Don't validate the enabled state if the user has explicitly set it. Also skip validation if we're just duplicating this node.
			const bool bIsDuplicating = (Ar.GetPortFlags() & PPF_Duplicate) != 0;
			if (!bIsDuplicating && !bUserSetEnabledState)
			{
				UClass* SelfScope = GetBlueprintClassFromNode();
				if (!FunctionReference.IsSelfContext() || SelfScope != nullptr)
				{
					if (const UFunction* Function = FunctionReference.ResolveMember<UFunction>(SelfScope))
					{
						// Enable as development-only if specified in metadata. This way existing functions that have the metadata added to them will get their enabled state fixed up on load.
						if (EnabledState == ENodeEnabledState::Enabled && Function->HasMetaData(FBlueprintMetadata::MD_DevelopmentOnly))
						{
							EnabledState = ENodeEnabledState::DevelopmentOnly;
						}
						// Ensure that if the metadata is removed, we also fix up the enabled state to avoid leaving it set as development-only in that case.
						else if (EnabledState == ENodeEnabledState::DevelopmentOnly && !Function->HasMetaData(FBlueprintMetadata::MD_DevelopmentOnly))
						{
							EnabledState = ENodeEnabledState::Enabled;
						}
					}
				}
			}
		}
	}
}

void UK2Node_CallFunction::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();

	// Try re-setting the function given our new parent scope, in case it turns an external to an internal, or vis versa
	FunctionReference.RefreshGivenNewSelfScope<UFunction>(GetBlueprintClassFromNode());

	// Re-enable for development only if specified in metadata.
	if(EnabledState == ENodeEnabledState::Enabled && !bUserSetEnabledState)
	{
		const UFunction* Function = GetTargetFunction();
		if (Function && Function->HasMetaData(FBlueprintMetadata::MD_DevelopmentOnly))
		{
			EnabledState = ENodeEnabledState::DevelopmentOnly;
		}
	}
}

FNodeHandlingFunctor* UK2Node_CallFunction::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_CallFunction(CompilerContext);
}

void UK2Node_CallFunction::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	UFunction* Function = GetTargetFunction();

	// connect DefaultToSelf and WorldContext inside static functions to proper 'self'  
	if (SourceGraph && Schema->IsStaticFunctionGraph(SourceGraph) && Function)
	{
		TArray<UK2Node_FunctionEntry*> EntryPoints;
		SourceGraph->GetNodesOfClass(EntryPoints);
		if (1 != EntryPoints.Num())
		{
			CompilerContext.MessageLog.Warning(*FString::Printf(*LOCTEXT("WrongEntryPointsNum", "%i entry points found while expanding node @@").ToString(), EntryPoints.Num()), this);
		}
		else if (auto BetterSelfPin = EntryPoints[0]->GetAutoWorldContextPin())
		{
			FString const DefaultToSelfMetaValue = Function->GetMetaData(FBlueprintMetadata::MD_DefaultToSelf);
			FString const WorldContextMetaValue = Function->GetMetaData(FBlueprintMetadata::MD_WorldContext);

			struct FStructConnectHelper
			{
				static void Connect(const FString& PinName, UK2Node* Node, UEdGraphPin* BetterSelf, const UEdGraphSchema_K2* InSchema, FCompilerResultsLog& MessageLog)
				{
					auto Pin = Node->FindPin(PinName);
					if (!PinName.IsEmpty() && Pin && !Pin->LinkedTo.Num())
					{
						const bool bConnected = InSchema->TryCreateConnection(Pin, BetterSelf);
						if (!bConnected)
						{
							MessageLog.Warning(*LOCTEXT("DefaultToSelfNotConnected", "DefaultToSelf pin @@ from node @@ cannot be connected to @@").ToString(), Pin, Node, BetterSelf);
						}
					}
				}
			};
			FStructConnectHelper::Connect(DefaultToSelfMetaValue, this, BetterSelfPin, Schema, CompilerContext.MessageLog);
			if (!Function->HasMetaData(FBlueprintMetadata::MD_CallableWithoutWorldContext))
			{
				FStructConnectHelper::Connect(WorldContextMetaValue, this, BetterSelfPin, Schema, CompilerContext.MessageLog);
			}
		}
	}

	// If we have an enum param that is expanded, we handle that first
	if(bWantsEnumToExecExpansion)
	{
		if(Function)
		{
			// Get the metadata that identifies which param is the enum, and try and find it
			const FString& EnumParamName = Function->GetMetaData(FBlueprintMetadata::MD_ExpandEnumAsExecs);
			UByteProperty* EnumProp = FindField<UByteProperty>(Function, FName(*EnumParamName));
			UEdGraphPin* EnumParamPin = FindPinChecked(EnumParamName);
			if(EnumProp != NULL && EnumProp->Enum != NULL)
			{
				// Expanded as input execs pins
				if (EnumParamPin->Direction == EGPD_Input)
				{
					// Create normal exec input
					UEdGraphPin* ExecutePin = CreatePin(EGPD_Input, Schema->PC_Exec, TEXT(""), NULL, false, false, Schema->PN_Execute);

					// Create temp enum variable
					UK2Node_TemporaryVariable* TempEnumVarNode = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
					TempEnumVarNode->VariableType.PinCategory = Schema->PC_Byte;
					TempEnumVarNode->VariableType.PinSubCategoryObject = EnumProp->Enum;
					TempEnumVarNode->AllocateDefaultPins();
					// Get the output pin
					UEdGraphPin* TempEnumVarOutput = TempEnumVarNode->GetVariablePin();

					// Connect temp enum variable to (hidden) enum pin
					Schema->TryCreateConnection(TempEnumVarOutput, EnumParamPin);

					// Now we want to iterate over other exec inputs...
					for(int32 PinIdx=Pins.Num()-1; PinIdx>=0; PinIdx--)
					{
						UEdGraphPin* Pin = Pins[PinIdx];
						if( Pin != NULL && 
							Pin != ExecutePin &&
							Pin->Direction == EGPD_Input && 
							Pin->PinType.PinCategory == Schema->PC_Exec )
						{
							// Create node to set the temp enum var
							UK2Node_AssignmentStatement* AssignNode = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
							AssignNode->AllocateDefaultPins();

							// Move connections from fake 'enum exec' pint to this assignment node
							CompilerContext.MovePinLinksToIntermediate(*Pin, *AssignNode->GetExecPin());

							// Connect this to out temp enum var
							Schema->TryCreateConnection(AssignNode->GetVariablePin(), TempEnumVarOutput);

							// Connect exec output to 'real' exec pin
							Schema->TryCreateConnection(AssignNode->GetThenPin(), ExecutePin);

							// set the literal enum value to set to
							AssignNode->GetValuePin()->DefaultValue = Pin->PinName;

							// Finally remove this 'cosmetic' exec pin
							Pins[PinIdx]->MarkPendingKill();
							Pins.RemoveAt(PinIdx);
						}
					}
				}
				// Expanded as output execs pins
				else if (EnumParamPin->Direction == EGPD_Output)
				{
					// Create normal exec output
					UEdGraphPin* ExecutePin = CreatePin(EGPD_Output, Schema->PC_Exec, TEXT(""), NULL, false, false, Schema->PN_Execute);
						
					// Create a SwitchEnum node to switch on the output enum
					UK2Node_SwitchEnum* SwitchEnumNode = CompilerContext.SpawnIntermediateNode<UK2Node_SwitchEnum>(this, SourceGraph);
					UEnum* EnumObject = Cast<UEnum>(EnumParamPin->PinType.PinSubCategoryObject.Get());
					SwitchEnumNode->SetEnum(EnumObject);
					SwitchEnumNode->AllocateDefaultPins();
						
					// Hook up execution to the switch node
					Schema->TryCreateConnection(ExecutePin, SwitchEnumNode->GetExecPin());
					// Connect (hidden) enum pin to switch node's selection pin
					Schema->TryCreateConnection(EnumParamPin, SwitchEnumNode->GetSelectionPin());
						
					// Now we want to iterate over other exec outputs
					for(int32 PinIdx=Pins.Num()-1; PinIdx>=0; PinIdx--)
					{
						UEdGraphPin* Pin = Pins[PinIdx];
						if( Pin != NULL &&
							Pin != ExecutePin &&
							Pin->Direction == EGPD_Output &&
							Pin->PinType.PinCategory == Schema->PC_Exec )
						{
							// Move connections from fake 'enum exec' pin to this switch node
							CompilerContext.MovePinLinksToIntermediate(*Pin, *SwitchEnumNode->FindPinChecked(Pin->PinName));
								
							// Finally remove this 'cosmetic' exec pin
							Pins[PinIdx]->MarkPendingKill();
							Pins.RemoveAt(PinIdx);
						}
					}
				}
			}
		}
	}

	// AUTO CREATED REFS
	{
		if ( Function )
		{
			TArray<FString> AutoCreateRefTermPinNames;
			const bool bHasAutoCreateRefTerms = Function->HasMetaData(FBlueprintMetadata::MD_AutoCreateRefTerm);
			if ( bHasAutoCreateRefTerms )
			{
				CompilerContext.GetSchema()->GetAutoEmitTermParameters(Function, AutoCreateRefTermPinNames);
			}

			for ( auto Pin : Pins )
			{
				if ( Pin && bHasAutoCreateRefTerms && AutoCreateRefTermPinNames.Contains(Pin->PinName) )
				{
					const bool bValidAutoRefPin = Pin->PinType.bIsReference
						&& !CompilerContext.GetSchema()->IsMetaPin(*Pin)
						&& ( Pin->Direction == EGPD_Input )
						&& !Pin->LinkedTo.Num();
					if ( bValidAutoRefPin )
					{
						const bool bHasDefaultValue = !Pin->DefaultValue.IsEmpty() || Pin->DefaultObject || !Pin->DefaultTextValue.IsEmpty();

						//default values can be reset when the pin is connected
						const auto DefaultValue = Pin->DefaultValue;
						const auto DefaultObject = Pin->DefaultObject;
						const auto DefaultTextValue = Pin->DefaultTextValue;
						const auto AutogeneratedDefaultValue = Pin->AutogeneratedDefaultValue;

						auto ValuePin = InnerHandleAutoCreateRef(this, Pin, CompilerContext, SourceGraph, bHasDefaultValue);
						if ( ValuePin )
						{
							if (!DefaultObject && DefaultTextValue.IsEmpty() && DefaultValue.Equals(AutogeneratedDefaultValue, ESearchCase::CaseSensitive))
							{
								// Use the latest code to set default value
								Schema->SetPinDefaultValueBasedOnType(ValuePin);
							}
							else
							{
								ValuePin->DefaultValue = DefaultValue;
								ValuePin->DefaultObject = DefaultObject;
								ValuePin->DefaultTextValue = DefaultTextValue;
							}
						}
					}
				}
			}
		}
	}

	// Then we go through and expand out array iteration if necessary
	const bool bAllowMultipleSelfs = AllowMultipleSelfs(true);
	UEdGraphPin* MultiSelf = Schema->FindSelfPin(*this, EEdGraphPinDirection::EGPD_Input);
	if(bAllowMultipleSelfs && MultiSelf && !MultiSelf->PinType.bIsArray)
	{
		const bool bProperInputToExpandForEach = 
			(1 == MultiSelf->LinkedTo.Num()) && 
			(NULL != MultiSelf->LinkedTo[0]) && 
			(MultiSelf->LinkedTo[0]->PinType.bIsArray);
		if(bProperInputToExpandForEach)
		{
			CallForEachElementInArrayExpansion(this, MultiSelf, CompilerContext, SourceGraph);
		}
	}
}

UEdGraphPin* UK2Node_CallFunction::InnerHandleAutoCreateRef(UK2Node* Node, UEdGraphPin* Pin, FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, bool bForceAssignment)
{
	const bool bAddAssigment = !Pin->PinType.bIsArray && bForceAssignment;

	// ADD LOCAL VARIABLE
	UK2Node_TemporaryVariable* LocalVariable = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(Node, SourceGraph);
	LocalVariable->VariableType = Pin->PinType;
	LocalVariable->VariableType.bIsReference = false;
	LocalVariable->AllocateDefaultPins();
	if (!bAddAssigment)
	{
		if (!CompilerContext.GetSchema()->TryCreateConnection(LocalVariable->GetVariablePin(), Pin))
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("AutoCreateRefTermPin_NotConnected", "AutoCreateRefTerm Expansion: Pin @@ cannot be connected to @@").ToString(), LocalVariable->GetVariablePin(), Pin);
			return NULL;
		}
	}
	// ADD ASSIGMENT
	else
	{
		// TODO connect to dest..
		UK2Node_PureAssignmentStatement* AssignDefaultValue = CompilerContext.SpawnIntermediateNode<UK2Node_PureAssignmentStatement>(Node, SourceGraph);
		AssignDefaultValue->AllocateDefaultPins();
		const bool bVariableConnected = CompilerContext.GetSchema()->TryCreateConnection(AssignDefaultValue->GetVariablePin(), LocalVariable->GetVariablePin());
		auto AssignInputPit = AssignDefaultValue->GetValuePin();
		const bool bPreviousInputSaved = AssignInputPit && CompilerContext.MovePinLinksToIntermediate(*Pin, *AssignInputPit).CanSafeConnect();
		const bool bOutputConnected = CompilerContext.GetSchema()->TryCreateConnection(AssignDefaultValue->GetOutputPin(), Pin);
		if (!bVariableConnected || !bOutputConnected || !bPreviousInputSaved)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("AutoCreateRefTermPin_AssignmentError", "AutoCreateRefTerm Expansion: Assignment Error @@").ToString(), AssignDefaultValue);
			return NULL;
		}
		CompilerContext.GetSchema()->SetPinDefaultValueBasedOnType(AssignDefaultValue->GetValuePin());
		return AssignInputPit;
	}
	return NULL;
}

void UK2Node_CallFunction::CallForEachElementInArrayExpansion(UK2Node* Node, UEdGraphPin* MultiSelf, FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	check(Node && MultiSelf && SourceGraph && Schema);
	const bool bProperInputToExpandForEach = 
		(1 == MultiSelf->LinkedTo.Num()) && 
		(NULL != MultiSelf->LinkedTo[0]) && 
		(MultiSelf->LinkedTo[0]->PinType.bIsArray);
	ensure(bProperInputToExpandForEach);

	UEdGraphPin* ThenPin = Node->FindPinChecked(Schema->PN_Then);

	// Create int Iterator
	UK2Node_TemporaryVariable* IteratorVar = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(Node, SourceGraph);
	IteratorVar->VariableType.PinCategory = Schema->PC_Int;
	IteratorVar->AllocateDefaultPins();

	// Initialize iterator
	UK2Node_AssignmentStatement* InteratorInitialize = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(Node, SourceGraph);
	InteratorInitialize->AllocateDefaultPins();
	InteratorInitialize->GetValuePin()->DefaultValue = TEXT("0");
	Schema->TryCreateConnection(IteratorVar->GetVariablePin(), InteratorInitialize->GetVariablePin());
	CompilerContext.MovePinLinksToIntermediate(*Node->GetExecPin(), *InteratorInitialize->GetExecPin());

	// Do loop branch
	UK2Node_IfThenElse* Branch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(Node, SourceGraph);
	Branch->AllocateDefaultPins();
	Schema->TryCreateConnection(InteratorInitialize->GetThenPin(), Branch->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*ThenPin, *Branch->GetElsePin());

	// Do loop condition
	UK2Node_CallFunction* Condition = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(Node, SourceGraph); 
	Condition->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_IntInt)));
	Condition->AllocateDefaultPins();
	Schema->TryCreateConnection(Condition->GetReturnValuePin(), Branch->GetConditionPin());
	Schema->TryCreateConnection(Condition->FindPinChecked(TEXT("A")), IteratorVar->GetVariablePin());

	// Array size
	UK2Node_CallArrayFunction* ArrayLength = CompilerContext.SpawnIntermediateNode<UK2Node_CallArrayFunction>(Node, SourceGraph); 
	ArrayLength->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Length)));
	ArrayLength->AllocateDefaultPins();
	CompilerContext.CopyPinLinksToIntermediate(*MultiSelf, *ArrayLength->GetTargetArrayPin());
	ArrayLength->PinConnectionListChanged(ArrayLength->GetTargetArrayPin());
	Schema->TryCreateConnection(Condition->FindPinChecked(TEXT("B")), ArrayLength->GetReturnValuePin());

	// Get Element
	UK2Node_CallArrayFunction* GetElement = CompilerContext.SpawnIntermediateNode<UK2Node_CallArrayFunction>(Node, SourceGraph); 
	GetElement->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Get)));
	GetElement->AllocateDefaultPins();
	CompilerContext.CopyPinLinksToIntermediate(*MultiSelf, *GetElement->GetTargetArrayPin());
	GetElement->PinConnectionListChanged(GetElement->GetTargetArrayPin());
	Schema->TryCreateConnection(GetElement->FindPinChecked(TEXT("Index")), IteratorVar->GetVariablePin());

	// Iterator increment
	UK2Node_CallFunction* Increment = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(Node, SourceGraph); 
	Increment->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Add_IntInt)));
	Increment->AllocateDefaultPins();
	Schema->TryCreateConnection(Increment->FindPinChecked(TEXT("A")), IteratorVar->GetVariablePin());
	Increment->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("1");

	// Iterator assigned
	UK2Node_AssignmentStatement* IteratorAssign = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(Node, SourceGraph);
	IteratorAssign->AllocateDefaultPins();
	Schema->TryCreateConnection(IteratorAssign->GetVariablePin(), IteratorVar->GetVariablePin());
	Schema->TryCreateConnection(IteratorAssign->GetValuePin(), Increment->GetReturnValuePin());
	Schema->TryCreateConnection(IteratorAssign->GetThenPin(), Branch->GetExecPin());

	// Connect pins from intermediate nodes back in to the original node
	Schema->TryCreateConnection(Branch->GetThenPin(), Node->GetExecPin());
	Schema->TryCreateConnection(ThenPin, IteratorAssign->GetExecPin());
	Schema->TryCreateConnection(GetElement->FindPinChecked(TEXT("Item")), MultiSelf);
}

FName UK2Node_CallFunction::GetCornerIcon() const
{
	if (const UFunction* Function = GetTargetFunction())
	{
		if (Function->HasAllFunctionFlags(FUNC_BlueprintAuthorityOnly))
		{
			return TEXT("Graph.Replication.AuthorityOnly");		
		}
		else if (Function->HasAllFunctionFlags(FUNC_BlueprintCosmetic))
		{
			return TEXT("Graph.Replication.ClientEvent");
		}
		else if(Function->HasMetaData(FBlueprintMetadata::MD_Latent))
		{
			return TEXT("Graph.Latent.LatentIcon");
		}
	}
	return Super::GetCornerIcon();
}

FSlateIcon UK2Node_CallFunction::GetIconAndTint(FLinearColor& OutColor) const
{
	return GetPaletteIconForFunction(GetTargetFunction(), OutColor);
}

bool UK2Node_CallFunction::ReconnectPureExecPins(TArray<UEdGraphPin*>& OldPins)
{
	if (IsNodePure())
	{
		// look for an old exec pin
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		UEdGraphPin* PinExec = nullptr;
		for (int32 PinIdx = 0; PinIdx < OldPins.Num(); PinIdx++)
		{
			if (OldPins[PinIdx]->PinName == K2Schema->PN_Execute)
			{
				PinExec = OldPins[PinIdx];
				break;
			}
		}
		if (PinExec)
		{
			// look for old then pin
			UEdGraphPin* PinThen = nullptr;
			for (int32 PinIdx = 0; PinIdx < OldPins.Num(); PinIdx++)
			{
				if (OldPins[PinIdx]->PinName == K2Schema->PN_Then)
				{
					PinThen = OldPins[PinIdx];
					break;
				}
			}
			if (PinThen)
			{
				// reconnect all incoming links to old exec pin to the far end of the old then pin.
				if (PinThen->LinkedTo.Num() > 0)
				{
					UEdGraphPin* PinThenLinked = PinThen->LinkedTo[0];
					while (PinExec->LinkedTo.Num() > 0)
					{
						UEdGraphPin* PinExecLinked = PinExec->LinkedTo[0];
						PinExecLinked->BreakLinkTo(PinExec);
						PinExecLinked->MakeLinkTo(PinThenLinked);
					}
					return true;
				}
			}
		}
	}
	return false;
}

void UK2Node_CallFunction::InvalidatePinTooltips()
{
	bPinTooltipsValid = false;
}

FText UK2Node_CallFunction::GetToolTipHeading() const
{
	FText Heading = Super::GetToolTipHeading();

	struct FHeadingBuilder
	{
		FHeadingBuilder(FText InitialHeading) : ConstructedHeading(InitialHeading) {}

		void Append(FText HeadingAddOn)
		{
			if (ConstructedHeading.IsEmpty())
			{
				ConstructedHeading = HeadingAddOn;
			}
			else 
			{
				ConstructedHeading = FText::Format(FText::FromString("{0}\n{1}"), HeadingAddOn, ConstructedHeading);
			}
		}

		FText ConstructedHeading;
	};
	FHeadingBuilder HeadingBuilder(Super::GetToolTipHeading());

	if (const UFunction* Function = GetTargetFunction())
	{
		if (Function->HasAllFunctionFlags(FUNC_BlueprintAuthorityOnly))
		{
			HeadingBuilder.Append(LOCTEXT("ServerOnlyFunc", "Server Only"));	
		}
		if (Function->HasAllFunctionFlags(FUNC_BlueprintCosmetic))
		{
			HeadingBuilder.Append(LOCTEXT("ClientOnlyFunc", "Client Only"));
		}
		if(Function->HasMetaData(FBlueprintMetadata::MD_Latent))
		{
			HeadingBuilder.Append(LOCTEXT("LatentFunc", "Latent"));
		}
	}

	return HeadingBuilder.ConstructedHeading;
}

void UK2Node_CallFunction::GetNodeAttributes( TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes ) const
{
	UFunction* TargetFunction = GetTargetFunction();
	const FString TargetFunctionName = TargetFunction ? TargetFunction->GetName() : TEXT( "InvalidFunction" );
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Type" ), TEXT( "Function" ) ));
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Class" ), GetClass()->GetName() ));
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Name" ), TargetFunctionName ));
}

FText UK2Node_CallFunction::GetMenuCategory() const
{
	UFunction* TargetFunction = GetTargetFunction();
	if (TargetFunction != nullptr)
	{
		return GetDefaultCategoryForFunction(TargetFunction, FText::GetEmpty());
	}
	return FText::GetEmpty();
}

bool UK2Node_CallFunction::HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const
{
	UFunction* Function = GetTargetFunction();
	const UClass* SourceClass = Function ? Function->GetOwnerClass() : nullptr;
	const UBlueprint* SourceBlueprint = GetBlueprint();
	bool bResult = (SourceClass != nullptr) && (SourceClass->ClassGeneratedBy != SourceBlueprint);
	if (bResult && OptionalOutput)
	{
		OptionalOutput->AddUnique(Function);
	}

	// All structures, that are required for the BP compilation, should be gathered
	for(auto Pin : Pins)
	{
		UStruct* DepStruct = Pin ? Cast<UStruct>(Pin->PinType.PinSubCategoryObject.Get()) : nullptr;

		UClass* DepClass = Cast<UClass>(DepStruct);
		if (DepClass && (DepClass->ClassGeneratedBy == SourceBlueprint))
		{
			//Don't include self
			continue;
		}

		if (DepStruct && !DepStruct->IsNative())
		{
			if (OptionalOutput)
			{
				OptionalOutput->AddUnique(DepStruct);
			}
			bResult = true;
		}
	}

	const bool bSuperResult = Super::HasExternalDependencies(OptionalOutput);
	return bSuperResult || bResult;
}

UEdGraph* UK2Node_CallFunction::GetFunctionGraph(const UEdGraphNode*& OutGraphNode) const
{
	OutGraphNode = NULL;

	// Search for the Blueprint owner of the function graph, climbing up through the Blueprint hierarchy
	UClass* MemberParentClass = FunctionReference.GetMemberParentClass(GetBlueprintClassFromNode());
	if(MemberParentClass != NULL)
	{
		UBlueprintGeneratedClass* ParentClass = Cast<UBlueprintGeneratedClass>(MemberParentClass);
		if(ParentClass != NULL && ParentClass->ClassGeneratedBy != NULL)
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(ParentClass->ClassGeneratedBy);
			while(Blueprint != NULL)
			{
				UEdGraph* TargetGraph = NULL;
				FName FunctionName = FunctionReference.GetMemberName();
				for (UEdGraph* Graph : Blueprint->FunctionGraphs) 
				{
					if (Graph->GetFName() == FunctionName)
					{
						TargetGraph = Graph;
						break;
					}
				}

				if((TargetGraph != NULL) && !TargetGraph->HasAnyFlags(RF_Transient))
				{
					// Found the function graph in a Blueprint, return that graph
					return TargetGraph;
				}
				else
				{
					// Did not find the function call as a graph, it may be a custom event
					UK2Node_CustomEvent* CustomEventNode = NULL;

					TArray<UK2Node_CustomEvent*> CustomEventNodes;
					FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, CustomEventNodes);

					for (UK2Node_CustomEvent* CustomEvent : CustomEventNodes)
					{
						if(CustomEvent->CustomFunctionName == FunctionReference.GetMemberName())
						{
							OutGraphNode = CustomEvent;
							return CustomEvent->GetGraph();
						}
					}
				}

				ParentClass = Cast<UBlueprintGeneratedClass>(Blueprint->ParentClass);
				Blueprint = ParentClass != NULL ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy) : NULL;
			}
		}
	}
	return NULL;
}

bool UK2Node_CallFunction::IsStructureWildcardProperty(const UFunction* Function, const FString& PropertyName)
{
	if (Function && !PropertyName.IsEmpty())
	{
		TArray<FString> Names;
		FCustomStructureParamHelper::FillCustomStructureParameterNames(Function, Names);
		if (Names.Contains(PropertyName))
		{
			return true;
		}
	}
	return false;
}

void UK2Node_CallFunction::AddSearchMetaDataInfo(TArray<struct FSearchTagDataPair>& OutTaggedMetaData) const
{
	Super::AddSearchMetaDataInfo(OutTaggedMetaData);

	if (UFunction* TargetFunction = GetTargetFunction())
	{
		OutTaggedMetaData.Add(FSearchTagDataPair(FFindInBlueprintSearchTags::FiB_NativeName, FText::FromString(TargetFunction->GetName())));
	}
}

bool UK2Node_CallFunction::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	bool bIsDisallowed = Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
	if (!bIsDisallowed && MyPin != nullptr)
	{
		if (MyPin->bNotConnectable)
		{
			bIsDisallowed = true;
			OutReason = LOCTEXT("PinConnectionDisallowed", "This parameter is for internal use only.").ToString();
		}
	}

	return bIsDisallowed;
}

#undef LOCTEXT_NAMESPACE
