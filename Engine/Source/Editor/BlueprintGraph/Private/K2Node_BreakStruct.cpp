// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "KismetCompiler.h"
#include "BlueprintFieldNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintActionFilter.h"	// for FBlueprintActionContext

#define LOCTEXT_NAMESPACE "K2Node_BreakStruct"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_BreakStruct

class FKCHandler_BreakStruct : public FNodeHandlingFunctor
{
public:
	FKCHandler_BreakStruct(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	FBPTerminal* RegisterInputTerm(FKismetFunctionContext& Context, UK2Node_BreakStruct* Node)
	{
		check(NULL != Node);

		if(NULL == Node->StructType)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("BreakStruct_UnknownStructure_Error", "Unknown structure to break for @@").ToString(), Node);
			return NULL;
		}

		//Find input pin
		UEdGraphPin* InputPin = NULL;
		for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
		{
			UEdGraphPin* Pin = Node->Pins[PinIndex];
			if(Pin && (EGPD_Input == Pin->Direction))
			{
				InputPin = Pin;
				break;
			}
		}
		check(NULL != InputPin);

		//Find structure source net
		UEdGraphPin* Net = FEdGraphUtilities::GetNetFromPin(InputPin);
		check(NULL != Net);

		FBPTerminal** FoundTerm = Context.NetMap.Find(Net);
		FBPTerminal* Term = FoundTerm ? *FoundTerm : NULL;
		if(NULL == Term)
		{
			// Dont allow literal
			if ((Net->Direction == EGPD_Input) && (Net->LinkedTo.Num() == 0))
			{
				CompilerContext.MessageLog.Error(*LOCTEXT("InvalidNoInputStructure_Error", "No input structure to break for @@").ToString(), Net);
				return NULL;
			}
			// standard register net
			else
			{
				Term = Context.CreateLocalTerminalFromPinAutoChooseScope(Net, Context.NetNameMap->MakeValidName(Net));
			}
			Context.NetMap.Add(Net, Term);
		}
		UStruct* StructInTerm = Cast<UStruct>(Term->Type.PinSubCategoryObject.Get());
		if(NULL == StructInTerm || !StructInTerm->IsChildOf(Node->StructType))
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("BreakStruct_NoMatch_Error", "Structures don't match for @@").ToString(), Node);
		}

		return Term;
	}

	void RegisterOutputTerm(FKismetFunctionContext& Context, UScriptStruct* StructType, UEdGraphPin* Net, FBPTerminal* ContextTerm)
	{
		UProperty* BoundProperty = FindField<UProperty>(StructType, *(Net->PinName));

		if (BoundProperty != NULL)
		{
			FBPTerminal* Term = Context.CreateLocalTerminalFromPinAutoChooseScope(Net, Net->PinName);
			Term->AssociatedVarProperty = BoundProperty;
			Context.NetMap.Add(Net, Term);
			Term->Context = ContextTerm;

			if (BoundProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
			{
				Term->bIsConst = true;
			}
		}
		else
		{
			CompilerContext.MessageLog.Error(TEXT("Failed to find a struct member for @@"), Net);
		}
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* InNode) override
	{
		UK2Node_BreakStruct* Node = Cast<UK2Node_BreakStruct>(InNode);
		check(NULL != Node);

		if(!UK2Node_BreakStruct::CanBeBroken(Node->StructType))
		{
			CompilerContext.MessageLog.Warning(*LOCTEXT("BreakStruct_NoBreak_Error", "The structure cannot be broken using generic 'break' node @@. Try use specialized 'break' function if available.").ToString(), Node);
		}

		if(FBPTerminal* StructContextTerm = RegisterInputTerm(Context, Node))
		{
			for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
			{
				UEdGraphPin* Pin = Node->Pins[PinIndex];
				if(NULL != Pin && EGPD_Output == Pin->Direction)
				{
					RegisterOutputTerm(Context, Node->StructType, Pin, StructContextTerm);
				}
			}
		}
	}
};


UK2Node_BreakStruct::UK2Node_BreakStruct(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

static bool CanCreatePinForProperty(const UProperty* Property, bool bIncludeEditAnywhere = true)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	FEdGraphPinType DumbGraphPinType;
	const bool bConvertable = Schema->ConvertPropertyToPinType(Property, /*out*/ DumbGraphPinType);

	const bool bVisible = Property ? (bIncludeEditAnywhere ? Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible) : Property->HasAnyPropertyFlags(CPF_BlueprintVisible) ): false;
	return bVisible && bConvertable;
}

bool UK2Node_BreakStruct::CanBeBroken(const UScriptStruct* Struct, bool bIncludeEditAnywhere )
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if(Struct && Schema && !Struct->HasMetaData(TEXT("HasNativeBreak")))
	{
		if (UEdGraphSchema_K2::IsAllowableBlueprintVariableType(Struct))
		{
			return true;
		}

		for (TFieldIterator<UProperty> It(Struct); It; ++It)
		{
			if (CanCreatePinForProperty(*It, bIncludeEditAnywhere))
			{
				return true;
			}
		}
	}
	return false;
}

void UK2Node_BreakStruct::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if(Schema && StructType)
	{
		CreatePin(EGPD_Input, Schema->PC_Struct, TEXT(""), StructType, false, true, StructType->GetName(), true);
		
		struct FBreakStructPinManager : public FStructOperationOptionalPinManager
		{
			virtual bool CanTreatPropertyAsOptional(UProperty* TestProperty) const override
			{
				return CanCreatePinForProperty(TestProperty);
			}
		};

		{
			FBreakStructPinManager OptionalPinManager;
			OptionalPinManager.RebuildPropertyList(ShowPinForProperties, StructType);
			OptionalPinManager.CreateVisiblePins(ShowPinForProperties, StructType, EGPD_Output, this);
		}

		// When struct has a lot of fields, mark their pins as advanced
		if(Pins.Num() > 5)
		{
			if(ENodeAdvancedPins::NoPins == AdvancedPinDisplay)
			{
				AdvancedPinDisplay = ENodeAdvancedPins::Hidden;
			}

			for(int32 PinIndex = 3; PinIndex < Pins.Num(); ++PinIndex)
			{
				if(UEdGraphPin * EdGraphPin = Pins[PinIndex])
				{
					EdGraphPin->bAdvancedView = true;
				}
			}
		}
	}
}

FText UK2Node_BreakStruct::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (StructType == nullptr)
	{
		return LOCTEXT("BreakNullStruct_Title", "Break <unknown struct>");
	}
	else if (CachedNodeTitle.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("StructName"), FText::FromString(StructType->GetName()));

		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("BreakNodeTitle", "Break {StructName}"), Args), this);
	}
	return CachedNodeTitle;
}

FText UK2Node_BreakStruct::GetTooltipText() const
{
	if (StructType == nullptr)
	{
		return LOCTEXT("BreakNullStruct_Tooltip", "Adds a node that breaks an '<unknown struct>' into its member fields");
	}
	else if (CachedTooltip.IsOutOfDate(this))
	{
		// FText::Format() is slow, so we cache this to save on performance
		CachedTooltip.SetCachedText(FText::Format(
			LOCTEXT("BreakStruct_Tooltip", "Adds a node that breaks a '{0}' into its member fields"),
			FText::FromName(StructType->GetFName())
		), this);
	}
	return CachedTooltip;
}

void UK2Node_BreakStruct::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	if(!StructType)
	{
		MessageLog.Error(*LOCTEXT("NoStruct_Error", "No Struct in @@").ToString(), this);
	}
	else
	{
		bool bHasAnyBlueprintVisibleProperty = false;
		for (TFieldIterator<UProperty> It(StructType); It; ++It)
		{
			const UProperty* Property = *It;
			if (CanCreatePinForProperty(Property))
			{
				const bool bIsBlueprintVisible = Property->HasAnyPropertyFlags(CPF_BlueprintVisible) || (Property->GetOwnerStruct() && Property->GetOwnerStruct()->IsA<UUserDefinedStruct>());
				bHasAnyBlueprintVisibleProperty |= bIsBlueprintVisible;

				const UEdGraphPin* Pin = FindPin(Property->GetName());
				const bool bIsLinked = Pin && Pin->LinkedTo.Num();

				if (!bIsBlueprintVisible && bIsLinked)
				{
					MessageLog.Warning(*LOCTEXT("PropertyIsNotBPVisible_Warning", "@@ - the native property is not tagged as BlueprintReadWrite or BlueprintReadOnly, the pin will be removed in a future release.").ToString(), Pin);
				}

				if ((Property->ArrayDim > 1) && bIsLinked)
				{
					MessageLog.Warning(*LOCTEXT("StaticArray_Warning", "@@ - the native property is a static array, which is not supported by blueprints").ToString(), Pin);
				}
			}
		}

		if (!bHasAnyBlueprintVisibleProperty)
		{
			MessageLog.Warning(*LOCTEXT("StructHasNoBPVisibleProperties_Warning", "@@ has no property tagged as BlueprintReadWrite or BlueprintReadOnly. The node will be removed in a future release.").ToString(), this);
		}
	}
}

FLinearColor UK2Node_BreakStruct::GetNodeTitleColor() const 
{
	if(const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>())
	{
		FEdGraphPinType PinType;
		PinType.PinCategory = K2Schema->PC_Struct;
		PinType.PinSubCategoryObject = StructType;
		return K2Schema->GetPinTypeColor(PinType);
	}
	return UK2Node::GetNodeTitleColor();
}

UK2Node::ERedirectType UK2Node_BreakStruct::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex)  const
{
	ERedirectType Result = UK2Node::DoPinsMatchForReconstruction(NewPin, NewPinIndex, OldPin, OldPinIndex);
	if ((ERedirectType_None == Result) && DoRenamedPinsMatch(NewPin, OldPin, true))
	{
		Result = ERedirectType_Custom;
	}
	else if ((ERedirectType_None == Result) && NewPin && OldPin)
	{
		if ((EGPD_Input == NewPin->Direction) && (EGPD_Input == OldPin->Direction))
		{
			const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
			if (K2Schema->ArePinTypesCompatible( NewPin->PinType, OldPin->PinType))
			{
				Result = ERedirectType_Custom;
			}
		}
		else if ((EGPD_Output == NewPin->Direction) && (EGPD_Output == OldPin->Direction))
		{
			TMap<FName, FName>* StructRedirects = UStruct::TaggedPropertyRedirects.Find(StructType->GetFName());
			if (StructRedirects)
			{
				FName* PropertyRedirect = StructRedirects->Find(FName(*OldPin->PinName));
				if (PropertyRedirect)
				{
					Result = ((FCString::Stricmp(*PropertyRedirect->ToString(), *NewPin->PinName) != 0) ? ERedirectType_None : ERedirectType_Name);
				}
			}
		}
	}
	return Result;
}

FNodeHandlingFunctor* UK2Node_BreakStruct::CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_BreakStruct(CompilerContext);
}

void UK2Node_BreakStruct::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	auto SetNodeStructLambda = [](UEdGraphNode* NewNode, UField const* /*StructField*/, TWeakObjectPtr<UScriptStruct> NonConstStructPtr)
	{
		UK2Node_BreakStruct* BreakNode = CastChecked<UK2Node_BreakStruct>(NewNode);
		BreakNode->StructType = NonConstStructPtr.Get();
	};

	auto CategoryOverrideLambda = [](FBlueprintActionContext const& Context, IBlueprintNodeBinder::FBindingSet const& /*Bindings*/, FBlueprintActionUiSpec* UiSpecOut, TWeakObjectPtr<UScriptStruct> StructPtr)
	{
		for (UEdGraphPin* Pin : Context.Pins)
		{
			UScriptStruct* PinStruct = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get());
			if ((PinStruct != nullptr) && (StructPtr.Get() == PinStruct) && (Pin->Direction == EGPD_Output))
			{
				UiSpecOut->Category = LOCTEXT("EmptyCategory", "|"); 
				break;
			}
		}
	};

	for (TObjectIterator<UScriptStruct> StructIt; StructIt; ++StructIt)
	{
		UScriptStruct const* Struct = (*StructIt);
		if (!CanBeBroken(Struct, false))
		{
			continue;
		}

		// to keep from needlessly instantiating a UBlueprintNodeSpawners, first   
		// check to make sure that the registrar is looking for actions of this type
		// (could be regenerating actions for a specific asset, and therefore the 
		// registrar would only accept actions corresponding to that asset)
		if (!ActionRegistrar.IsOpenForRegistration(Struct))
		{
			continue;
		}

		UBlueprintFieldNodeSpawner* NodeSpawner = UBlueprintFieldNodeSpawner::Create(GetClass(), Struct);
		check(NodeSpawner != nullptr);
		TWeakObjectPtr<UScriptStruct> NonConstStructPtr = Struct;
		NodeSpawner->SetNodeFieldDelegate = UBlueprintFieldNodeSpawner::FSetNodeFieldDelegate::CreateStatic(SetNodeStructLambda, NonConstStructPtr);
		NodeSpawner->DynamicUiSignatureGetter = UBlueprintFieldNodeSpawner::FUiSpecOverrideDelegate::CreateStatic(CategoryOverrideLambda, NonConstStructPtr);

		// this struct could belong to a class, or is a user defined struct 
		// (asset), that's why we want to make sure to register it along with 
		// the action (so the action knows to refresh when the class/asset is).
		ActionRegistrar.AddBlueprintAction(Struct, NodeSpawner);
	}	
}

FText UK2Node_BreakStruct::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Struct);
}

#undef LOCTEXT_NAMESPACE
