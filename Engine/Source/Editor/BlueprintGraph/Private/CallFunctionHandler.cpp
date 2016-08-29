// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"

#include "CallFunctionHandler.h"

#define LOCTEXT_NAMESPACE "CallFunctionHandler"

//////////////////////////////////////////////////////////////////////////
// FImportTextErrorContext

// Support class to pipe logs from UProperty->ImportText (for struct literals) to the message log as warnings
class FImportTextErrorContext : public FOutputDevice
{
protected:
	FCompilerResultsLog& MessageLog;
	UObject* TargetObject;
public:
	int32 NumErrors;

	FImportTextErrorContext(FCompilerResultsLog& InMessageLog, UObject* InTargetObject)
		: FOutputDevice()
		, MessageLog(InMessageLog)
		, TargetObject(InTargetObject)
		, NumErrors(0)
	{
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		if (TargetObject == NULL)
		{
			MessageLog.Error(V);
		}
		else
		{
			const FString ErrorString = FString::Printf(TEXT("Invalid default on node @@: %s"), V);
			MessageLog.Error(*ErrorString, TargetObject);		
		}
		NumErrors++;
	}
};

//////////////////////////////////////////////////////////////////////////
// FKCHandler_CallFunction

#if _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4750)
#endif

/**
 * Searches for the function referenced by a graph node in the CallingContext class's list of functions,
 * validates that the wiring matches up correctly, and creates an execution statement.
 */
void FKCHandler_CallFunction::CreateFunctionCallStatement(FKismetFunctionContext& Context, UEdGraphNode* Node, UEdGraphPin* SelfPin)
{
	int32 NumErrorsAtStart = CompilerContext.MessageLog.NumErrors;

	// Find the function, starting at the parent class
	UFunction* Function = FindFunction(Context, Node);
	
	if (Function != NULL)
	{
		CheckIfFunctionIsCallable(Function, Context, Node);
		// Make sure the pin mapping is sound (all pins wire up to a matching function parameter, and all function parameters match a pin)

		// Remaining unmatched pins
		TArray<UEdGraphPin*> RemainingPins;
		RemainingPins.Append(Node->Pins);

		// Remove expected exec and self pins
		//@TODO: Check to make sure there is exactly one exec in and one exec out, as well as one self pin
		for (int32 i = 0; i < RemainingPins.Num(); )
		{
			if (CompilerContext.GetSchema()->IsMetaPin(*RemainingPins[i]))
			{
				RemainingPins.RemoveAtSwap(i);
			}
			else
			{
				++i;
			}
		}

		// Check for magic pins
		const bool bIsLatent = Function->HasMetaData(FBlueprintMetadata::MD_Latent);
		if (bIsLatent && (CompilerContext.UbergraphContext != &Context))
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ContainsLatentCall_Error", "@@ contains a latent call, which cannot exist outside of the event graph").ToString(), Node);
		}

		// Check access specifier
		const uint32 AccessSpecifier = Function->FunctionFlags & FUNC_AccessSpecifiers;
		if(FUNC_Private == AccessSpecifier)
		{
			if(Function->GetOuter() != Context.NewClass)
			{
				CompilerContext.MessageLog.Warning(*LOCTEXT("PrivateFunctionCall_Error", "Function @@ is private and cannot be called outside its class").ToString(), Node);
			}
		}
		else if(FUNC_Protected == AccessSpecifier)
		{
			if( !Context.NewClass->IsChildOf( Cast<UStruct>( Function->GetOuter() ) ) )
			{
				CompilerContext.MessageLog.Warning(*LOCTEXT("ProtectedFunctionCall_Error", "Function @@ is protected and can be called only from its class or subclasses").ToString(), Node);
			}
		}

		UEdGraphPin* LatentInfoPin = NULL;

		TMap<FName, FString>* MetaData = UMetaData::GetMapForObject(Function);
		if (MetaData != NULL)
		{
			for (TMap<FName, FString>::TConstIterator It(*MetaData); It; ++It)
			{
				const FName& Key = It.Key();

				if (Key == TEXT("LatentInfo"))
				{
					UEdGraphPin* Pin = Node->FindPin(It.Value());
					if( (Pin != NULL) && (Pin->Direction == EGPD_Input) && (Pin->LinkedTo.Num() == 0))
					{
						LatentInfoPin = Pin;

						UEdGraphPin* PinToTry = FEdGraphUtilities::GetNetFromPin(Pin);
						FBPTerminal** Term = Context.NetMap.Find(PinToTry);
						if(Term != NULL)
						{
							check((*Term)->bIsLiteral);
						
							int32 LatentUUID = INDEX_NONE;
							UEdGraphNode* AssociatedNode = LatentInfoPin->GetOwningNode();
							if (AssociatedNode && AssociatedNode->NodeGuid.IsValid())
							{
								LatentUUID = GetTypeHash(AssociatedNode->NodeGuid);
							}
							else
							{
								static int32 FallbackUUID = 0;
								LatentUUID = FallbackUUID++;

								CompilerContext.MessageLog.Warning(*LOCTEXT("UUIDDeterministicCookWarn", "Failed to produce a deterministic UUID for a node's latent action: @@. Cooking this Blueprint (@@) asset will non-deterministic.").ToString(), LatentInfoPin, Context.Blueprint);
							}							

							const FString ExecutionFunctionName = CompilerContext.GetSchema()->FN_ExecuteUbergraphBase.ToString() + TEXT("_") + Context.Blueprint->GetName();
							(*Term)->Name = FString::Printf(TEXT("(Linkage=%s,UUID=%s,ExecutionFunction=%s,CallbackTarget=None)"), *FString::FromInt(INDEX_NONE), *FString::FromInt(LatentUUID), *ExecutionFunctionName);

							// Record the UUID in the debugging information
							UEdGraphNode* TrueSourceNode = Cast<UEdGraphNode>(Context.MessageLog.FindSourceObject(Node));
							Context.NewClass->GetDebugData().RegisterUUIDAssociation(TrueSourceNode, LatentUUID);
						}
					}
					else
					{
						CompilerContext.MessageLog.Error(*FString::Printf(*LOCTEXT("FindPinFromLinkage_Error", "Function %s (called from @@) was specified with LatentInfo metadata but does not have a pin named %s").ToString(),
							*(Function->GetName()), *(It.Value())), Node);
					}
				}
			}
		}

		// Parameter info to be stored, and assigned to all function call statements generated below
		FBPTerminal* LHSTerm = NULL;
		TArray<FBPTerminal*> RHSTerms;
		UEdGraphPin* ThenExecPin = NULL;
		UEdGraphNode* LatentTargetNode = NULL;
		int32 LatentTargetParamIndex = INDEX_NONE;

		// Grab the special case structs that use their own literal path
		UScriptStruct* VectorStruct = TBaseStructure<FVector>::Get();
		UScriptStruct* RotatorStruct = TBaseStructure<FRotator>::Get();
		UScriptStruct* TransformStruct = TBaseStructure<FTransform>::Get();

		// Check each property
		bool bMatchedAllParams = true;
		for (TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			UProperty* Property = *It;

			bool bFoundParam = false;
			for (int32 i = 0; !bFoundParam && (i < RemainingPins.Num()); ++i)
			{
				UEdGraphPin* PinMatch = RemainingPins[i];
				if (FCString::Stricmp(*Property->GetName(), *PinMatch->PinName) == 0)
				{
					// Found a corresponding pin, does it match in type and direction?
					if (FKismetCompilerUtilities::IsTypeCompatibleWithProperty(PinMatch, Property, CompilerContext.MessageLog, CompilerContext.GetSchema(), Context.NewClass))
					{
						UEdGraphPin* PinToTry = FEdGraphUtilities::GetNetFromPin(PinMatch);
						FBPTerminal** Term = Context.NetMap.Find(PinToTry);

						if (Term != NULL)
						{
							// For literal structs, we have to verify the default here to make sure that it has valid formatting
							if( (*Term)->bIsLiteral && (PinMatch != LatentInfoPin))
							{
								UStructProperty* StructProperty = Cast<UStructProperty>(Property);
								if( StructProperty )
								{
									UScriptStruct* Struct = StructProperty->Struct;
									if( Struct != VectorStruct
										&& Struct != RotatorStruct
										&& Struct != TransformStruct )
									{
										// Ensure all literal struct terms can be imported if its empty
										if ( (*Term)->Name.IsEmpty() )
										{
											(*Term)->Name = TEXT("()");
										}

										int32 StructSize = Struct->GetStructureSize();
										[this, StructSize, StructProperty, Node, Term, &bMatchedAllParams]()
										{
											uint8* StructData = (uint8*)FMemory_Alloca(StructSize);
											StructProperty->InitializeValue(StructData);

											// Import the literal text to a dummy struct to verify it's well-formed
											FImportTextErrorContext ErrorPipe(CompilerContext.MessageLog, Node);
											StructProperty->ImportText(*((*Term)->Name), StructData, 0, NULL, &ErrorPipe);
											if(ErrorPipe.NumErrors > 0)
											{
												bMatchedAllParams = false;
											}
										}();
									}
									
								}
							}

							if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
							{
								LHSTerm = *Term;
							}
							else
							{
								FBPTerminal* RHSTerm = *Term;

								// if this term is an object that needs to be cast to an interface
								if (FBPTerminal** InterfaceTerm = InterfaceTermMap.Find(PinMatch))
								{
									UClass* InterfaceClass = CastChecked<UClass>(PinMatch->PinType.PinSubCategoryObject.Get());

									FBPTerminal* ClassTerm = Context.CreateLocalTerminal(ETerminalSpecification::TS_Literal);
									ClassTerm->Name       = InterfaceClass->GetName();
									ClassTerm->bIsLiteral = true;
									ClassTerm->Source     = Node;
									ClassTerm->ObjectLiteral = InterfaceClass;
									ClassTerm->Type.PinCategory = CompilerContext.GetSchema()->PC_Class;

									// insert a cast op before a call to the function (and replace
									// the param with the result from the cast)
									FBlueprintCompiledStatement& CastStatement = Context.AppendStatementForNode(Node);
									CastStatement.Type = InterfaceClass->HasAnyClassFlags(CLASS_Interface) ? KCST_CastObjToInterface : KCST_CastInterfaceToObj;
									CastStatement.LHS = *InterfaceTerm;
									CastStatement.RHS.Add(ClassTerm);
									CastStatement.RHS.Add(*Term);

									RHSTerm = *InterfaceTerm;
								}

								int32 ParameterIndex = RHSTerms.Add(RHSTerm);

								if (PinMatch == LatentInfoPin)
								{
									// Record the (latent) output impulse from this node
									ThenExecPin = CompilerContext.GetSchema()->FindExecutionPin(*Node, EGPD_Output);

									if( (ThenExecPin != NULL) && (ThenExecPin->LinkedTo.Num() > 0) )
									{
										LatentTargetNode = ThenExecPin->LinkedTo[0]->GetOwningNode();
									}

									if( LatentTargetNode != NULL )
									{
										LatentTargetParamIndex = ParameterIndex;
									}
								}
							}

							// Make sure it isn't trying to modify a const term
							if (Property->HasAnyPropertyFlags(CPF_OutParm) && !((*Term)->IsTermWritable()))
							{
								if (Property->HasAnyPropertyFlags(CPF_ReferenceParm))
								{
									if (!Property->HasAnyPropertyFlags(CPF_ConstParm))
									{
										CompilerContext.MessageLog.Error(*LOCTEXT("PassReadOnlyReferenceParam_Error", "Cannot pass a read-only variable to a reference parameter @@").ToString(), PinMatch);
									}
								}
								else
								{
									CompilerContext.MessageLog.Error(*LOCTEXT("PassReadOnlyOutputParam_Error", "Cannot pass a read-only variable to a output parameter @@").ToString(), PinMatch);
								}
							}
						}
						else
						{
							CompilerContext.MessageLog.Error(*LOCTEXT("ResolveTermPassed_Error", "Failed to resolve term passed into @@").ToString(), PinMatch);
							bMatchedAllParams = false;
						}
					}
					else
					{
						bMatchedAllParams = false;
					}

					bFoundParam = true;
					RemainingPins.RemoveAtSwap(i);
				}
			}

			if (!bFoundParam)
			{
				CompilerContext.MessageLog.Error(*FString::Printf(*LOCTEXT("FindPinParameter_Error", "Could not find a pin for the parameter %s of %s on @@").ToString(), *Property->GetName(), *Function->GetName()), Node);
				bMatchedAllParams = false;
			}
		}

		// At this point, we should have consumed all pins.  If not, there are extras that need to be removed.
		for (int32 i = 0; i < RemainingPins.Num(); ++i)
		{
			CompilerContext.MessageLog.Error(*FString::Printf(*LOCTEXT("PinMismatchParameter_Error", "Pin @@ named %s doesn't match any parameters of function %s").ToString(), *RemainingPins[i]->PinName, *Function->GetName()), RemainingPins[i]);
		}

		if (NumErrorsAtStart == CompilerContext.MessageLog.NumErrors)
		{
			// Build up a list of contexts that this function will be called on
			TArray<FBPTerminal*> ContextTerms;
			if (SelfPin != NULL)
			{
				const bool bIsConstSelfContext = Context.IsConstFunction();
				const bool bIsNonConstFunction = !Function->HasAnyFunctionFlags(FUNC_Const|FUNC_Static);
				const bool bEnforceConstCorrectness = Context.EnforceConstCorrectness();
				auto CheckAndAddSelfTermLambda = [this, &Node, &ContextTerms, bIsConstSelfContext, bIsNonConstFunction, bEnforceConstCorrectness](FBPTerminal* Target)
				{
					bool bIsSelfTerm = true;
					if(Target != nullptr)
					{
						const UEdGraphPin* SourcePin = Target->SourcePin;
						bIsSelfTerm = (SourcePin == nullptr || CompilerContext.GetSchema()->IsSelfPin(*SourcePin));
					}

					// Ensure const correctness within the context of the function call:
					//	a) Attempting to call a non-const, non-static function within a const function graph (i.e. 'const self' as context)
					//	b) Attempting to call a non-const, non-static function with a 'const' term linked to the target pin as the function context
					if(bIsSelfTerm && bIsConstSelfContext && bIsNonConstFunction)
					{
						// If we're not enforcing const correctness in this context, emit a warning here rather than an error, and allow compilation of this statement to proceed
						if(Target != nullptr)
						{
							if(bEnforceConstCorrectness)
							{
								CompilerContext.MessageLog.Error(*LOCTEXT("NonConstFunctionCallOnReadOnlyTarget_Error", "Function @@ can modify state and cannot be called on @@ because it is a read-only Target in this context").ToString(), Node, Target->Source);
							}
							else
							{
								CompilerContext.MessageLog.Warning(*LOCTEXT("NonConstFunctionCallOnReadOnlyTarget_Warning", "Function @@ can modify state and should not be called on @@ because it is considered to be a read-only Target in this context").ToString(), Node, Target->Source);
							}
						}
						else
						{
							if(bEnforceConstCorrectness)
							{
								CompilerContext.MessageLog.Error(*LOCTEXT("NonConstFunctionCallOnReadOnlySelfScope_Error", "Function @@ can modify state and cannot be called on 'self' because it is a read-only Target in this context").ToString(), Node);
							}
							else
							{
								CompilerContext.MessageLog.Warning(*LOCTEXT("NonConstFunctionCallOnReadOnlySelfScope_Warning", "Function @@ can modify state and should not be called on 'self' because it is considered to be a read-only Target in this context").ToString(), Node);
							}
						}
					}

					ContextTerms.Add(Target);
				};

				if( SelfPin->LinkedTo.Num() > 0 )
				{
					for(int32 i = 0; i < SelfPin->LinkedTo.Num(); i++)
					{
						FBPTerminal** pContextTerm = Context.NetMap.Find(SelfPin->LinkedTo[i]);
						if(ensure(pContextTerm != nullptr))
						{
							CheckAndAddSelfTermLambda(*pContextTerm);
						}
					}
				}
				else
				{
					FBPTerminal** pContextTerm = Context.NetMap.Find(SelfPin);
					CheckAndAddSelfTermLambda((pContextTerm != nullptr) ? *pContextTerm : nullptr);
				}
			}

			// Check for a call into the ubergraph, which will require a patchup later on for the exact state entry point
			UEdGraphNode** pSrcEventNode = NULL;
			if (!bIsLatent)
			{
				pSrcEventNode = CompilerContext.CallsIntoUbergraph.Find(Node);
			}

			bool bInlineEventCall = false;
			bool bLocalCall = false;
			FName EventName = NAME_None;
			if (Context.IsInstrumentationRequired())
			{
				if (UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(Node))
				{
					if (CallFunctionNode->FunctionReference.IsSelfContext())
					{
						bLocalCall = true;
						const UEdGraphNode* EventNodeOut = nullptr;
						CallFunctionNode->GetFunctionGraph(EventNodeOut);
						if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(EventNodeOut))
						{
							bInlineEventCall = true;
							EventName = EventNode->GetFunctionName();
						}
					}
				}
			}
			const bool bInstrumentFunctionEntry = Context.IsInstrumentationRequired() && (bIsLatent || bInlineEventCall || bLocalCall);
			if (bInstrumentFunctionEntry)
			{
				if (bInlineEventCall)
				{
					FBlueprintCompiledStatement& EventStatement = Context.AppendStatementForNode(Node);
					EventStatement.Type = KCST_InstrumentedEvent;
					EventStatement.Comment = EventName.ToString();
				}
				else
				{
					FBlueprintCompiledStatement& PushState = Context.AppendStatementForNode(Node);
					PushState.Type = KCST_InstrumentedStatePush;
				}
			}

			// Iterate over all the contexts this functions needs to be called on, and emit a call function statement for each
			for (auto TargetListIt = ContextTerms.CreateIterator(); TargetListIt; ++TargetListIt)
			{
				FBPTerminal* Target = *TargetListIt;

				FBlueprintCompiledStatement& Statement = Context.AppendStatementForNode(Node);
				Statement.FunctionToCall = Function;
				Statement.FunctionContext = Target;
				Statement.Type = KCST_CallFunction;
				Statement.bIsInterfaceContext = IsCalledFunctionFromInterface(Node);
				Statement.bIsParentContext = IsCalledFunctionFinal(Node);

				Statement.LHS = LHSTerm;
				Statement.RHS = RHSTerms;

				if (!bIsLatent)
				{
					// Fixup ubergraph calls
					if (pSrcEventNode != NULL)
					{
						UEdGraphPin* ExecOut = CompilerContext.GetSchema()->FindExecutionPin(**pSrcEventNode, EGPD_Output);

						check(CompilerContext.UbergraphContext);
						CompilerContext.UbergraphContext->GotoFixupRequestMap.Add(&Statement, ExecOut);
						Statement.UbergraphCallIndex = 0;
					}
				}
				else
				{
					// Fixup latent functions
					if ((LatentTargetNode != NULL) && (Target == ContextTerms.Last()))
					{
						check(LatentTargetParamIndex != INDEX_NONE);
						Statement.UbergraphCallIndex = LatentTargetParamIndex;
						Context.GotoFixupRequestMap.Add(&Statement, ThenExecPin);
					}
				}

				AdditionalCompiledStatementHandling(Context, Node, Statement);
			}

			// Create the exit from this node if there is one
			if (bIsLatent)
			{
				if (bInstrumentFunctionEntry)
				{
					FBlueprintCompiledStatement& SuspendState = Context.AppendStatementForNode(Node);
					SuspendState.Type = KCST_InstrumentedStateSuspend;
				}
				// End this thread of execution; the latent function will resume it at some point in the future
				FBlueprintCompiledStatement& PopStatement = Context.AppendStatementForNode(Node);
				PopStatement.Type = KCST_EndOfThread;
			}
			else
			{
				// Generate the output impulse from this node
				if (!IsCalledFunctionPure(Node))
				{
					if (bInstrumentFunctionEntry)
					{
						if (bInlineEventCall)
						{
							FBlueprintCompiledStatement& EventStop = Context.AppendStatementForNode(Node);
							EventStop.Type = KCST_InstrumentedEventStop;
						}
						else
						{
							FBlueprintCompiledStatement& PopState = Context.AppendStatementForNode(Node);
							PopState.Type = KCST_InstrumentedStatePop;
						}
					}
					GenerateSimpleThenGoto(Context, *Node);
				}
			}
		}
	}
	else
	{
		FString WarningMessage = FString::Printf(*LOCTEXT("FindFunction_Error", "Could not find the function '%s' called from @@").ToString(), *GetFunctionNameFromNode(Node));
		CompilerContext.MessageLog.Warning(*WarningMessage, Node);
	}
}

UClass* FKCHandler_CallFunction::GetCallingContext(FKismetFunctionContext& Context, UEdGraphNode* Node)
{
	// Find the calling scope
	UClass* SearchScope = Context.NewClass;
	UK2Node_CallFunction* CallFuncNode = Cast<UK2Node_CallFunction>(Node);
	if ((CallFuncNode != NULL) && CallFuncNode->bIsFinalFunction)
	{
		if (UK2Node_CallParentFunction* ParentCall = Cast<UK2Node_CallParentFunction>(Node))
		{
			// Special Case:  super call functions should search up their class hierarchy, and find the first legitimate implementation of the function
			const FName FuncName = CallFuncNode->FunctionReference.GetMemberName();
			UClass* SearchContext = Context.NewClass->GetSuperClass();

			UFunction* ParentFunc = NULL;
			if (SearchContext != NULL)
			{
				ParentFunc = SearchContext->FindFunctionByName(FuncName);
			}

			return ParentFunc ? ParentFunc->GetOuterUClass() : NULL;
		}
		else
		{
			// Final functions need the call context to be the specified class, so don't bother checking for the self pin.   The schema should enforce this.
			return CallFuncNode->FunctionReference.GetMemberParentClass(CallFuncNode->GetBlueprintClassFromNode());
		}
	}
	else
	{
		UEdGraphPin* SelfPin = CompilerContext.GetSchema()->FindSelfPin(*Node, EGPD_Input);
		if (SelfPin != NULL)
		{
			SearchScope = Cast<UClass>(Context.GetScopeFromPinType(SelfPin->PinType, Context.NewClass));
		}
	}

	return SearchScope;
}

UClass* FKCHandler_CallFunction::GetTrueCallingClass(FKismetFunctionContext& Context, UEdGraphPin* SelfPin)
{
	if (SelfPin != NULL)
	{
		UEdGraphSchema_K2 const* K2Schema = CompilerContext.GetSchema();

		// TODO: here FBlueprintCompiledStatement::GetScopeFromPinType should be called, but since FEdGraphPinType::PinSubCategory is not always initialized properly that function works wrong
		// return Cast<UClass>(Context.GetScopeFromPinType(SelfPin->PinType, Context.NewClass));
		FEdGraphPinType& Type = SelfPin->PinType;
		if ((Type.PinCategory == K2Schema->PC_Object) || (Type.PinCategory == K2Schema->PC_Class) || (Type.PinCategory == K2Schema->PC_Interface))
		{
			if (!Type.PinSubCategory.IsEmpty() && (Type.PinSubCategory != K2Schema->PSC_Self))
			{
				return Cast<UClass>(Type.PinSubCategoryObject.Get());
			}
		}
	}
	return Context.NewClass;
}

void FKCHandler_CallFunction::RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node)
{
	UEdGraphSchema_K2 const* K2Schema = CompilerContext.GetSchema();

	UFunction* Function = FindFunction(Context, Node);
	if (Function != NULL)
	{
		TArray<FString> DefaultToSelfParamNames;
		TArray<FString> RequiresSetValue;

		if (Function->HasMetaData(FBlueprintMetadata::MD_DefaultToSelf))
		{
			FString const DefaltToSelfPinName = Function->GetMetaData(FBlueprintMetadata::MD_DefaultToSelf);

			DefaultToSelfParamNames.Add(DefaltToSelfPinName);
		}
		if (Function->HasMetaData(FBlueprintMetadata::MD_WorldContext))
		{
			const bool bHasIntrinsicWorldContext = !K2Schema->IsStaticFunctionGraph(Context.SourceGraph) && Context.Blueprint->ParentClass->GetDefaultObject()->ImplementsGetWorld();

			FString const WorldContextPinName = Function->GetMetaData(FBlueprintMetadata::MD_WorldContext);

			if (bHasIntrinsicWorldContext)
			{
				DefaultToSelfParamNames.Add(WorldContextPinName);
			}
			else if (!Function->HasMetaData(FBlueprintMetadata::MD_CallableWithoutWorldContext))
			{
				RequiresSetValue.Add(WorldContextPinName);
			}
		}

		for (auto It = Node->Pins.CreateConstIterator(); It; ++It)
		{
			UEdGraphPin* Pin = (*It);
			const bool bIsConnected = (Pin->LinkedTo.Num() != 0);

			// if this pin could use a default (it doesn't have a connection or default of its own)
			if (!bIsConnected && (Pin->DefaultObject == NULL))
			{
				if (DefaultToSelfParamNames.Contains(Pin->PinName) && FKismetCompilerUtilities::ValidateSelfCompatibility(Pin, Context))
				{
					ensure(Pin->PinType.PinSubCategoryObject != NULL);
					ensure((Pin->PinType.PinCategory == K2Schema->PC_Object) || (Pin->PinType.PinCategory == K2Schema->PC_Interface));

					FBPTerminal* Term = Context.RegisterLiteral(Pin);
					Term->Type.PinSubCategory = CompilerContext.GetSchema()->PN_Self;
					Context.NetMap.Add(Pin, Term);
				}
				else if (RequiresSetValue.Contains(Pin->PinName))
				{
					CompilerContext.MessageLog.Error(*NSLOCTEXT("KismetCompiler", "PinMustHaveConnection_Error", "Pin @@ must have a connection").ToString(), Pin);
				}
			}
		}
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if ((Pin->Direction != EGPD_Input) || (Pin->LinkedTo.Num() == 0))
		{
			continue;
		}

		// if we have an object plugged into an interface pin, let's create a 
		// term that'll be used as an intermediate, holding the result of a cast 
		// from object to interface
		if (((Pin->PinType.PinCategory == K2Schema->PC_Interface) && (Pin->LinkedTo[0]->PinType.PinCategory == K2Schema->PC_Object)) ||
			((Pin->PinType.PinCategory == K2Schema->PC_Object) && (Pin->LinkedTo[0]->PinType.PinCategory == K2Schema->PC_Interface)))
		{
			FBPTerminal* InterfaceTerm = Context.CreateLocalTerminal();
			InterfaceTerm->CopyFromPin(Pin, Context.NetNameMap->MakeValidName(Pin) + TEXT("_CastInput"));
			InterfaceTerm->Source = Node;

			InterfaceTermMap.Add(Pin, InterfaceTerm);
		}
	}

	FNodeHandlingFunctor::RegisterNets(Context, Node);
}

void FKCHandler_CallFunction::RegisterNet(FKismetFunctionContext& Context, UEdGraphPin* Net)
{
	// This net is an output from a function call
	FBPTerminal* Term = Context.CreateLocalTerminalFromPinAutoChooseScope(Net, Context.NetNameMap->MakeValidName(Net));
	Context.NetMap.Add(Net, Term);
}

UFunction* FKCHandler_CallFunction::FindFunction(FKismetFunctionContext& Context, UEdGraphNode* Node)
{
	UClass* CallingContext = GetCallingContext(Context, Node);

	if (CallingContext)
	{
		FString FunctionName = GetFunctionNameFromNode(Node);

		return CallingContext->FindFunctionByName(*FunctionName);
	}

	return nullptr;
}

void FKCHandler_CallFunction::Transform(FKismetFunctionContext& Context, UEdGraphNode* Node)
{
	// Add an object reference pin for this call
	//UEdGraphPin* OperatingOn = Node->CreatePin(EGPD_Input, Schema->PC_Object, TEXT(""), TEXT("OperatingContext"));

	if (IsCalledFunctionPure(Node))
	{
		// Flag for removal if pure and there are no consumers of the outputs
		//@TODO: This isn't recursive (and shouldn't be here), it'll just catch the last node in a line of pure junk
		bool bAnyOutputsUsed = false;
		for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
		{
			UEdGraphPin* Pin = Node->Pins[PinIndex];
			if ((Pin->Direction == EGPD_Output) && (Pin->LinkedTo.Num() > 0))
			{
				bAnyOutputsUsed = true;
				break;
			}
		}

		if (!bAnyOutputsUsed)
		{
			//@TODO: Remove this node, not just warn about it

		}
	}

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Find the function, starting at the parent class
	UFunction* Function = FindFunction(Context, Node);
	if ((Function != NULL) && (Function->HasMetaData(FBlueprintMetadata::MD_Latent)))
	{
		UK2Node_CallFunction* CallFuncNode = CastChecked<UK2Node_CallFunction>(Node);
		UEdGraphPin* OldOutPin = K2Schema->FindExecutionPin(*CallFuncNode, EGPD_Output);

		if ((OldOutPin != NULL) && (OldOutPin->LinkedTo.Num() > 0))
		{
			// Create a dummy execution sequence that will be the target of the return call from the latent action
			UK2Node_ExecutionSequence* DummyNode = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(CallFuncNode);
			DummyNode->AllocateDefaultPins();

			// Wire in the dummy node
			UEdGraphPin* NewInPin = K2Schema->FindExecutionPin(*DummyNode, EGPD_Input);
			UEdGraphPin* NewOutPin = K2Schema->FindExecutionPin(*DummyNode, EGPD_Output);

			if ((NewInPin != NULL) && (NewOutPin != NULL))
			{
				CompilerContext.MessageLog.NotifyIntermediatePinCreation(NewOutPin, OldOutPin);

				while (OldOutPin->LinkedTo.Num() > 0)
				{
					UEdGraphPin* LinkedPin = OldOutPin->LinkedTo[0];

					LinkedPin->BreakLinkTo(OldOutPin);
					LinkedPin->MakeLinkTo(NewOutPin);
				}

				OldOutPin->MakeLinkTo(NewInPin);
			}
		}
	}
}

void FKCHandler_CallFunction::Compile(FKismetFunctionContext& Context, UEdGraphNode* Node)
{
	check(NULL != Node);

	//@TODO: Can probably move this earlier during graph verification instead of compilation, but after island pruning
	if (!IsCalledFunctionPure(Node))
	{
		// For imperative nodes, make sure the exec function was actually triggered and not just included due to an output data dependency
		UEdGraphPin* ExecTriggeringPin = CompilerContext.GetSchema()->FindExecutionPin(*Node, EGPD_Input);
		if (ExecTriggeringPin == NULL)
		{
			CompilerContext.MessageLog.Error(*FString::Printf(*NSLOCTEXT("KismetCompiler", "NoValidExecutionPinForCallFunc_Error", "@@ must have a valid execution pin").ToString()), Node);
			return;
		}
		else if (ExecTriggeringPin->LinkedTo.Num() == 0)
		{
			CompilerContext.MessageLog.Warning(*FString::Printf(*NSLOCTEXT("KismetCompiler", "NodeNeverExecuted_Warning", "@@ will never be executed").ToString()), Node);
			return;
		}
	}

	// Validate the self pin again if it is disconnected, because pruning isolated nodes could have caused an invalid target
	UEdGraphPin* SelfPin = CompilerContext.GetSchema()->FindSelfPin(*Node, EGPD_Input);
	if (SelfPin && (SelfPin->LinkedTo.Num() == 0))
	{
		FEdGraphPinType SelfType;
		SelfType.PinCategory = CompilerContext.GetSchema()->PC_Object;
		SelfType.PinSubCategory = CompilerContext.GetSchema()->PSC_Self;

		if (!CompilerContext.GetSchema()->ArePinTypesCompatible(SelfType, SelfPin->PinType, Context.NewClass) && (SelfPin->DefaultObject == NULL))
		{
			CompilerContext.MessageLog.Error(*NSLOCTEXT("KismetCompiler", "PinMustHaveConnectionPruned_Error", "Pin @@ must have a connection.  Self pins cannot be connected to nodes that are culled.").ToString(), SelfPin);
		}
	}

	// Make sure the function node is valid to call
	CreateFunctionCallStatement(Context, Node, SelfPin);
}

void FKCHandler_CallFunction::CheckIfFunctionIsCallable(UFunction* Function, FKismetFunctionContext& Context, UEdGraphNode* Node)
{
	// Verify that the function is a Blueprint callable function (in case a BlueprintCallable specifier got removed)
	if (!Function->HasAnyFunctionFlags(FUNC_BlueprintCallable) && (Function->GetOuter() != Context.NewClass))
	{
		if (!IsCalledFunctionFinal(Node) && Function->GetName().Find(CompilerContext.GetSchema()->FN_ExecuteUbergraphBase.ToString()))
		{
			CompilerContext.MessageLog.Error(*FString::Printf(*NSLOCTEXT("KismetCompiler", "ShouldNotCallFromBlueprint_Error", "Function '%s' called from @@ should not be called from a Blueprint").ToString(), *Function->GetName()), Node);
		}
	}
}

// Get the name of the function to call from the node
FString FKCHandler_CallFunction::GetFunctionNameFromNode(UEdGraphNode* Node) const
{
	UK2Node_CallFunction* CallFuncNode = Cast<UK2Node_CallFunction>(Node);
	if (CallFuncNode)
	{
		return CallFuncNode->FunctionReference.GetMemberName().ToString();
	}
	else
	{
		CompilerContext.MessageLog.Error(*NSLOCTEXT("KismetCompiler", "UnableResolveFunctionName_Error", "Unable to resolve function name for @@").ToString(), Node);
		return TEXT("");
	}
}

#if _MSC_VER
#pragma warning(pop)
#endif

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
