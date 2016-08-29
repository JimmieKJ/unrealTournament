// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "BlueprintGraphPrivatePCH.h"
#include "KismetCompiler.h"
#include "K2Node_CallFunction.h"
#include "BlueprintsObjectVersion.h"

#define LOCTEXT_NAMESPACE "K2Node_FunctionEntry"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_FunctionEntry

class FKCHandler_FunctionEntry : public FNodeHandlingFunctor
{
public:
	FKCHandler_FunctionEntry(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	void RegisterFunctionInput(FKismetFunctionContext& Context, UEdGraphPin* Net, UFunction* Function)
	{
		// This net is a parameter into the function
		FBPTerminal* Term = new (Context.Parameters) FBPTerminal();
		Term->CopyFromPin(Net, Net->PinName);

		// Flag pass by reference parameters specially
		//@TODO: Still doesn't handle/allow users to declare new pass by reference, this only helps inherited functions
		if( Function )
		{
			if (UProperty* ParentProperty = FindField<UProperty>(Function, FName(*(Net->PinName))))
			{
				if (ParentProperty->HasAnyPropertyFlags(CPF_ReferenceParm))
				{
					Term->bPassedByReference = true;
				}
			}
		}


		Context.NetMap.Add(Net, Term);
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_FunctionEntry* EntryNode = CastChecked<UK2Node_FunctionEntry>(Node);

		UFunction* Function = FindField<UFunction>(EntryNode->SignatureClass, EntryNode->SignatureName);
		// if this function has a predefined signature (like for inherited/overridden 
		// functions), then we want to make sure to account for the output 
		// parameters - this is normally handled by the FunctionResult node, but 
		// we're not guaranteed that one is connected to the entry node 
		if (Function && Function->HasAnyFunctionFlags(FUNC_HasOutParms))
		{
			const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

			for (TFieldIterator<UProperty> ParamIt(Function, EFieldIteratorFlags::ExcludeSuper); ParamIt; ++ParamIt)
			{
				UProperty* ParamProperty = *ParamIt;

				// mirrored from UK2Node_FunctionResult::CreatePinsForFunctionEntryExit()
				const bool bIsFunctionInput = !ParamProperty->HasAnyPropertyFlags(CPF_OutParm) || ParamProperty->HasAnyPropertyFlags(CPF_ReferenceParm);
				if (bIsFunctionInput)
				{
					// 
					continue;
				}

				FEdGraphPinType ParamType;
				if (K2Schema->ConvertPropertyToPinType(ParamProperty, ParamType))
				{
					FString ParamName = ParamProperty->GetName();

					bool bTermExists = false;
					// check to see if this terminal already exists (most 
					// likely added by a FunctionResult node) - if so, then 
					// we don't need to add it ourselves
					for (const FBPTerminal& ResultTerm : Context.Results)
					{
						if (ResultTerm.Name == ParamName && ResultTerm.Type == ParamType)
						{
							bTermExists = true;
							break;
						}
					}

					if (!bTermExists)
					{
						// create a terminal that represents a output param 
						// for this function; if there is a FunctionResult 
						// node wired into our function graph, know that it
						// will first check to see if this already exists 
						// for it to use (rather than creating one of its own)
						FBPTerminal* ResultTerm = new (Context.Results) FBPTerminal();
						ResultTerm->Name = ParamName;

						ResultTerm->Type = ParamType;
						ResultTerm->bPassedByReference = ParamType.bIsReference;
						ResultTerm->SetContextTypeStruct(ParamType.PinCategory == UEdGraphSchema_K2::PC_Struct && Cast<UScriptStruct>(ParamType.PinSubCategoryObject.Get()));
					}
				}
			}
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->ParentPin == nullptr && !CompilerContext.GetSchema()->IsMetaPin(*Pin))
			{
				UEdGraphPin* Net = FEdGraphUtilities::GetNetFromPin(Pin);

				if (Context.NetMap.Find(Net) == NULL)
				{
					// New net, resolve the term that will be used to construct it
					FBPTerminal* Term = NULL;

					check(Net->Direction == EGPD_Output);

					RegisterFunctionInput(Context, Pin, Function);
				}
			}
		}
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_FunctionEntry* EntryNode = CastChecked<UK2Node_FunctionEntry>(Node);
		//check(EntryNode->SignatureName != NAME_None);
		if (EntryNode->SignatureName == CompilerContext.GetSchema()->FN_ExecuteUbergraphBase)
		{
			UEdGraphPin* EntryPointPin = Node->FindPin(CompilerContext.GetSchema()->PN_EntryPoint);
			FBPTerminal** pTerm = Context.NetMap.Find(EntryPointPin);
			if ((EntryPointPin != NULL) && (pTerm != NULL))
			{
				FBlueprintCompiledStatement& ComputedGotoStatement = Context.AppendStatementForNode(Node);
				ComputedGotoStatement.Type = KCST_ComputedGoto;
				ComputedGotoStatement.LHS = *pTerm;
			}
			else
			{
				CompilerContext.MessageLog.Error(*LOCTEXT("NoEntryPointPin_Error", "Expected a pin named EntryPoint on @@").ToString(), Node);
			}
		}
		else
		{
			// Generate the output impulse from this node
			GenerateSimpleThenGoto(Context, *Node);
		}
	}
};

struct FFunctionEntryHelper
{
	static const FString& GetWorldContextPinName()
	{
		static const FString WorldContextPinName(TEXT("__WorldContext"));
		return WorldContextPinName;
	}

	static bool RequireWorldContextParameter(const UK2Node_FunctionEntry* Node)
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		return K2Schema->IsStaticFunctionGraph(Node->GetGraph());
	}
};

UK2Node_FunctionEntry::UK2Node_FunctionEntry(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Enforce const-correctness by default
	bEnforceConstCorrectness = true;
}

void UK2Node_FunctionEntry::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FBlueprintsObjectVersion::GUID);

	if (Ar.IsLoading())
	{
		if (Ar.UE4Ver() < VER_UE4_BLUEPRINT_ENFORCE_CONST_IN_FUNCTION_OVERRIDES)
		{
			// Allow legacy implementations to violate const-correctness
			bEnforceConstCorrectness = false;
		}

		if (Ar.CustomVer(FBlueprintsObjectVersion::GUID) < FBlueprintsObjectVersion::CleanBlueprintFunctionFlags)
		{
			// Flags we explicitly use ExtraFlags for (at the time this fix was made):
			//     FUNC_Public, FUNC_Protected, FUNC_Private, 
			//     FUNC_Static, FUNC_Const,
			//     FUNC_BlueprintPure, FUNC_BlueprintCallable, FUNC_BlueprintEvent, FUNC_BlueprintAuthorityOnly,
			//     FUNC_Net, FUNC_NetMulticast, FUNC_NetServer, FUNC_NetClient, FUNC_NetReliable
			// 
			// FUNC_Exec, FUNC_Event, & FUNC_BlueprintCosmetic are all inherited 
			// in FKismetCompilerContext::PrecompileFunction()
			static const uint32 InvalidExtraFlagsMask = FUNC_Final | FUNC_RequiredAPI | FUNC_BlueprintCosmetic |
				FUNC_NetRequest | FUNC_Exec | FUNC_Native | FUNC_Event | FUNC_NetResponse | FUNC_MulticastDelegate |
				FUNC_Delegate | FUNC_HasOutParms | FUNC_HasDefaults | FUNC_DLLImport | FUNC_NetValidate;
			ExtraFlags &= ~InvalidExtraFlagsMask;
		}
	}
}

FText UK2Node_FunctionEntry::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	UEdGraph* Graph = GetGraph();
	FGraphDisplayInfo DisplayInfo;
	Graph->GetSchema()->GetGraphDisplayInformation(*Graph, DisplayInfo);

	return DisplayInfo.DisplayName;
}

void UK2Node_FunctionEntry::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, K2Schema->PN_Then);

	UFunction* Function = FindField<UFunction>(SignatureClass, SignatureName);

	if (Function == nullptr)
	{
		Function = FindDelegateSignature(SignatureName);
	}

	if (Function != NULL)
	{
		CreatePinsForFunctionEntryExit(Function, /*bIsFunctionEntry=*/ true);
	}

	Super::AllocateDefaultPins();

	if (FFunctionEntryHelper::RequireWorldContextParameter(this) 
		&& ensure(!FindPin(FFunctionEntryHelper::GetWorldContextPinName())))
	{
		UEdGraphPin* WorldContextPin = CreatePin(
			EGPD_Output,
			K2Schema->PC_Object,
			FString(),
			UObject::StaticClass(),
			false,
			false,
			FFunctionEntryHelper::GetWorldContextPinName());
		WorldContextPin->bHidden = true;
	}
}

UEdGraphPin* UK2Node_FunctionEntry::GetAutoWorldContextPin() const
{
	return FindPin(FFunctionEntryHelper::GetWorldContextPinName());
}

void UK2Node_FunctionEntry::RemoveOutputPin(UEdGraphPin* PinToRemove)
{
	UK2Node_FunctionEntry* OwningSeq = Cast<UK2Node_FunctionEntry>( PinToRemove->GetOwningNode() );
	if (OwningSeq)
	{
		PinToRemove->MarkPendingKill();
		OwningSeq->Pins.Remove(PinToRemove);
	}
}

bool UK2Node_FunctionEntry::CanCreateUserDefinedPin(const FEdGraphPinType& InPinType, EEdGraphPinDirection InDesiredDirection, FText& OutErrorMessage)
{
	bool bResult = Super::CanCreateUserDefinedPin(InPinType, InDesiredDirection, OutErrorMessage);
	if (bResult)
	{
		if(InDesiredDirection == EGPD_Input)
		{
			OutErrorMessage = LOCTEXT("AddInputPinError", "Cannot add input pins to function entry node!");
			bResult = false;
		}
	}
	return bResult;
}

UEdGraphPin* UK2Node_FunctionEntry::CreatePinFromUserDefinition(const TSharedPtr<FUserPinInfo> NewPinInfo)
{
	// Make sure that if this is an exec node we are allowed one.
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (NewPinInfo->PinType.PinCategory == Schema->PC_Exec && !CanModifyExecutionWires())
	{
		return nullptr;
	}

	UEdGraphPin* NewPin = CreatePin(EGPD_Output, NewPinInfo->PinType, NewPinInfo->PinName);
	NewPin->DefaultValue = NewPin->AutogeneratedDefaultValue = NewPinInfo->PinDefaultValue;
	return NewPin;
}

FNodeHandlingFunctor* UK2Node_FunctionEntry::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_FunctionEntry(CompilerContext);
}

void UK2Node_FunctionEntry::GetRedirectPinNames(const UEdGraphPin& Pin, TArray<FString>& RedirectPinNames) const
{
	Super::GetRedirectPinNames(Pin, RedirectPinNames);

	if(RedirectPinNames.Num() > 0)
	{
		const FString& OldPinName = RedirectPinNames[0];

		
		// first add functionname.param
		RedirectPinNames.Add(FString::Printf(TEXT("%s.%s"), *SignatureName.ToString(), *OldPinName));
		// if there is class, also add an option for class.functionname.param
		if(SignatureClass!=NULL)
		{
			RedirectPinNames.Add(FString::Printf(TEXT("%s.%s.%s"), *SignatureClass->GetName(), *SignatureName.ToString(), *OldPinName));
		}
	}
}

bool UK2Node_FunctionEntry::IsDeprecated() const
{
	if (UFunction* const Function = FindField<UFunction>(SignatureClass, SignatureName))
	{
		return Function->HasMetaData(FBlueprintMetadata::MD_DeprecatedFunction);
	}

	return false;
}

FString UK2Node_FunctionEntry::GetDeprecationMessage() const
{
	if (UFunction* const Function = FindField<UFunction>(SignatureClass, SignatureName))
	{
		if (Function->HasMetaData(FBlueprintMetadata::MD_DeprecationMessage))
		{
			return FString::Printf(TEXT("%s %s"), *LOCTEXT("FunctionDeprecated_Warning", "@@ is deprecated;").ToString(), *Function->GetMetaData(FBlueprintMetadata::MD_DeprecationMessage));
		}
	}

	return Super::GetDeprecationMessage();
}

FText UK2Node_FunctionEntry::GetTooltipText() const
{
	UFunction* Function = FindField<UFunction>(SignatureClass, SignatureName);
	if (Function)
	{
		return FText::FromString(UK2Node_CallFunction::GetDefaultTooltipForFunction(Function));
	}
	return Super::GetTooltipText();
}

int32 UK2Node_FunctionEntry::GetFunctionFlags() const
{
	int32 ReturnFlags = 0;

	UClass* ClassToLookup = SignatureClass;

	if (SignatureClass && SignatureClass->ClassGeneratedBy)
	{
		UBlueprint* GeneratingBP = CastChecked<UBlueprint>(SignatureClass->ClassGeneratedBy);
		ClassToLookup = GeneratingBP->SkeletonGeneratedClass;
	}

	UFunction* Function = FindField<UFunction>(ClassToLookup, SignatureName);
	if (Function)
	{
		ReturnFlags = Function->FunctionFlags;
	}
	return ReturnFlags | ExtraFlags;
}

void UK2Node_FunctionEntry::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	
	UEdGraphPin* OldStartExecPin = nullptr;

	if(Pins[0]->LinkedTo.Num())
	{
		OldStartExecPin = Pins[0]->LinkedTo[0];
	}
	
	UEdGraphPin* LastActiveOutputPin = Pins[0];

	// Only look for FunctionEntry nodes who were duplicated and have a source object
	if ( UK2Node_FunctionEntry* OriginalNode = Cast<UK2Node_FunctionEntry>(CompilerContext.MessageLog.FindSourceObject(this)) )
	{
		check(OriginalNode->GetOuter());

		// Find the associated UFunction
		UFunction* Function = FindField<UFunction>(CompilerContext.Blueprint->SkeletonGeneratedClass, *OriginalNode->GetOuter()->GetName());

		// When regenerating on load, we may need to import text on certain properties to force load the assets
		TSharedPtr<FStructOnScope> LocalVarData;
		if (Function && CompilerContext.Blueprint->bIsRegeneratingOnLoad)
		{
			if (Function->GetStructureSize() > 0 || !ensure(Function->PropertyLink == nullptr))
			{
				LocalVarData = MakeShareable(new FStructOnScope(Function));
			}
		}

		for (TFieldIterator<UProperty> It(Function); It; ++It)
		{
			if (const UProperty* Property = *It)
			{
				for (auto& LocalVar : LocalVariables)
				{
					if (LocalVar.VarName == Property->GetFName() && !LocalVar.DefaultValue.IsEmpty())
					{
						// Add a variable set node for the local variable and hook it up immediately following the entry node or the last added local variable
						UK2Node_VariableSet* VariableSetNode = CompilerContext.SpawnIntermediateNode<UK2Node_VariableSet>(this, SourceGraph);
						VariableSetNode->SetFromProperty(Property, false);
						Schema->ConfigureVarNode(VariableSetNode, LocalVar.VarName, Function, CompilerContext.Blueprint);
						VariableSetNode->AllocateDefaultPins();
						CompilerContext.MessageLog.NotifyIntermediateObjectCreation(VariableSetNode, this);

						if(UEdGraphPin* SetPin = VariableSetNode->FindPin(Property->GetName()))
						{
							if(LocalVar.VarType.bIsArray)
							{
								TSharedPtr<FStructOnScope> StructData = MakeShareable(new FStructOnScope(Function));
								FBlueprintEditorUtils::PropertyValueFromString(Property, LocalVar.DefaultValue, StructData->GetStructMemory());

								// Create a Make Array node to setup the array's defaults
								UK2Node_MakeArray* MakeArray = CompilerContext.SpawnIntermediateNode<UK2Node_MakeArray>(this, SourceGraph);
								MakeArray->AllocateDefaultPins();
								MakeArray->GetOutputPin()->MakeLinkTo(SetPin);
								MakeArray->PostReconstructNode();

								const UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property);
								check(ArrayProperty);

								FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, StructData->GetStructMemory());
								FScriptArrayHelper_InContainer DefaultArrayHelper(ArrayProperty, StructData->GetStructMemory());

								uint8* StructDefaults = NULL;
								UStructProperty* StructProperty = dynamic_cast<UStructProperty*>(ArrayProperty->Inner);
								if ( StructProperty != NULL )
								{
									checkSlow(StructProperty->Struct);
									StructDefaults = (uint8*)FMemory::Malloc(StructProperty->Struct->GetStructureSize());
									StructProperty->InitializeValue(StructDefaults);
								}

								// Go through each element in the array to set the default value
								for( int32 ArrayIndex = 0 ; ArrayIndex < ArrayHelper.Num() ; ArrayIndex++ )
								{
									uint8* PropData = ArrayHelper.GetRawPtr(ArrayIndex);

									// Always use struct defaults if the inner is a struct, for symmetry with the import of array inner struct defaults
									uint8* PropDefault = ( StructProperty != NULL ) ? StructDefaults :
										( ( StructData->GetStructMemory() && DefaultArrayHelper.Num() > ArrayIndex ) ? DefaultArrayHelper.GetRawPtr(ArrayIndex) : NULL );

									// Retrieve the element's default value
									FString DefaultValue;
									FBlueprintEditorUtils::PropertyValueToString(ArrayProperty->Inner, PropData, DefaultValue);

									if(ArrayIndex > 0)
									{
										MakeArray->AddInputPin();
									}

									// Add one to the index for the pin to set the default on to skip the output pin
									Schema->TrySetDefaultValue(*MakeArray->Pins[ArrayIndex + 1], DefaultValue);
								}
							}
							else
							{
								if (CompilerContext.Blueprint->bIsRegeneratingOnLoad)
								{
									// When regenerating on load, we want to force load assets referenced by local variables.
									// This functionality is already handled when generating Terms in the Kismet Compiler for Arrays and Structs, so we do not have to worry about them.
									if (LocalVar.VarType.PinCategory == Schema->PC_Object || LocalVar.VarType.PinCategory == Schema->PC_Class || LocalVar.VarType.PinCategory == Schema->PC_Interface)
									{
										FBlueprintEditorUtils::PropertyValueFromString(Property, LocalVar.DefaultValue, LocalVarData->GetStructMemory());
									}
								}

								// Set the default value
								Schema->TrySetDefaultValue(*SetPin, LocalVar.DefaultValue);
							}
						}

						LastActiveOutputPin->BreakAllPinLinks();
						LastActiveOutputPin->MakeLinkTo(VariableSetNode->Pins[0]);
						LastActiveOutputPin = VariableSetNode->Pins[1];
					}
				}
			}
		}

		// Finally, hook up the last node to the old node the function entry node was connected to
		if(OldStartExecPin)
		{
			LastActiveOutputPin->MakeLinkTo(OldStartExecPin);
		}
	}
}

#undef LOCTEXT_NAMESPACE