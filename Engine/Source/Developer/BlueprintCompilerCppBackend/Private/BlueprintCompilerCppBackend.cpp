// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintCompilerCppBackendModulePrivatePCH.h"
#include "BlueprintCompilerCppBackend.h"
#include "BlueprintCompilerCppBackendUtils.h"

FString FBlueprintCompilerCppBackend::TermToText(const FBPTerminal* Term, const UProperty* CoerceProperty)
{
	const FString PSC_Self(TEXT("self"));
	if (Term->bIsLiteral)
	{
		//@TODO: Must have a coercion type if it's a literal, because the symbol table isn't plumbed in here and the literals don't carry type information either, yay!
		ensure(CoerceProperty);

		if (CoerceProperty->IsA(UStrProperty::StaticClass()))
		{
			return FString::Printf(TEXT("TEXT(\"%s\")"), *(Term->Name));
		}
		else if (CoerceProperty->IsA(UTextProperty::StaticClass()))
		{
			return FString::Printf(TEXT("FText::FromString(TEXT(\"%s\"))"), *(Term->Name));
		}
		else if (CoerceProperty->IsA(UFloatProperty::StaticClass()))
		{
			float Value = FCString::Atof(*(Term->Name));
			return FString::Printf(TEXT("%f"), Value);
		}
		else if (CoerceProperty->IsA(UIntProperty::StaticClass()))
		{
			int32 Value = FCString::Atoi(*(Term->Name));
			return FString::Printf(TEXT("%d"), Value);
		}
		else if (auto ByteProperty = Cast<const UByteProperty>(CoerceProperty))
		{
			// The PinSubCategoryObject check is to allow enum literals communicate with byte properties as literals
			if (ByteProperty->Enum != NULL ||
				(NULL != Cast<UEnum>(Term->Type.PinSubCategoryObject.Get())))
			{
				return Term->Name;
			}
			else
			{
				uint8 Value = FCString::Atoi(*(Term->Name));
				return FString::Printf(TEXT("%u"), Value);
			}
		}
		else if (auto BoolProperty = Cast<const UBoolProperty>(CoerceProperty))
		{
			bool bValue = Term->Name.ToBool();
			return bValue ? TEXT("true") : TEXT("false");
		}
		else if (auto NameProperty = Cast<const UNameProperty>(CoerceProperty))
		{
			FName LiteralName(*(Term->Name));
			return FString::Printf(TEXT("FName(TEXT(\"%s\"))"), *(LiteralName.ToString()));
		}
		else if (auto StructProperty = Cast<const UStructProperty>(CoerceProperty))
		{
			if (StructProperty->Struct == VectorStruct)
			{
				FVector Vect = FVector::ZeroVector;
				FDefaultValueHelper::ParseVector(Term->Name, /*out*/ Vect);

				return FString::Printf(TEXT("FVector(%f,%f,%f)"), Vect.X, Vect.Y, Vect.Z);
			}
			else if (StructProperty->Struct == RotatorStruct)
			{
				FRotator Rot = FRotator::ZeroRotator;
				FDefaultValueHelper::ParseRotator(Term->Name, /*out*/ Rot);

				return FString::Printf(TEXT("FRotator(%f,%f,%f)"), Rot.Pitch, Rot.Yaw, Rot.Roll);
			}
			else if (StructProperty->Struct == TransformStruct)
			{
				FTransform Trans = FTransform::Identity;
				Trans.InitFromString(Term->Name);

				const FQuat Rot = Trans.GetRotation();
				const FVector Translation = Trans.GetTranslation();
				const FVector Scale = Trans.GetScale3D();

				return FString::Printf(TEXT("FTransform( FQuat(%f,%f,%f,%f), FVector(%f,%f,%f), FVector(%f,%f,%f) )"),
					Rot.X, Rot.Y, Rot.Z, Rot.W, Translation.X, Translation.Y, Translation.Z, Scale.X, Scale.Y, Scale.Z);
			}
			else if (StructProperty->Struct == LinearColorStruct)
			{
				FLinearColor LinearColor;
				LinearColor.InitFromString(Term->Name);
				return FString::Printf(TEXT("FLinearColor(%f,%f,%f,%f)"), LinearColor.R, LinearColor.G, LinearColor.B, LinearColor.A);
			}
			else
			{
				//@todo:  This needs to be more robust, since import text isn't really proper for struct construction.
				return FString(TEXT("F")) + StructProperty->Struct->GetName() + Term->Name;
			}
		}
		else if (auto ClassProperty = Cast<const UClassProperty>(CoerceProperty))
		{
			if (auto FoundClass = Cast<const UClass>(Term->ObjectLiteral))
			{
				return FString::Printf(TEXT("%s%s::StaticClass()"), FoundClass->GetPrefixCPP(), *FoundClass->GetName());
			}
			return FString(TEXT("NULL"));
		}
		else if (CoerceProperty->IsA(UDelegateProperty::StaticClass()))
		{
			//@TODO: K2 Delegate Support: Won't compile, there isn't an operator= that does what we want here, it should be a proper call to BindDynamic instead!
			if (Term->Name == TEXT(""))
			{
				return TEXT("NULL");
			}
			else
			{
				return FString::Printf(TEXT("BindDynamic(this, &%s::%s)"), *CppClassName, *(Term->Name));
			}
		}
		else if (CoerceProperty->IsA(UObjectPropertyBase::StaticClass()))
		{
			if (Term->Type.PinSubCategory == PSC_Self)
			{
				return TEXT("this");
			}
			else if (Term->ObjectLiteral)
			{
				if (auto LiteralClass = Cast<const UClass>(Term->ObjectLiteral))
				{
					return FString::Printf(TEXT("%s%s::StaticClass()"), LiteralClass->GetPrefixCPP(), *LiteralClass->GetName());
				}
				else
				{
					auto ObjectCoerceProperty = CastChecked<const UObjectPropertyBase>(CoerceProperty);
					UClass* FoundClass = ObjectCoerceProperty ? ObjectCoerceProperty->PropertyClass : NULL;
					FString ClassString = FoundClass ? (FString(FoundClass->GetPrefixCPP()) + FoundClass->GetName()) : TEXT("UObject");
					return FString::Printf(TEXT("FindObject<%s>(ANY_PACKAGE, TEXT(\"%s\"))"), *ClassString, *(Term->ObjectLiteral->GetPathName()));
				}
			}
			else
			{
				return FString(TEXT("NULL"));
			}
		}
		else if (CoerceProperty->IsA(UInterfaceProperty::StaticClass()))
		{
			if (Term->Type.PinSubCategory == PSC_Self)
			{
				return TEXT("this");
			}
			else
			{
				ensureMsg(false, TEXT("It is not possible to express this interface property as a literal value!"));
				return Term->Name;
			}
		}
		else
			// else if (CoerceProperty->IsA(UMulticastDelegateProperty::StaticClass()))
			// Cannot assign a literal to a multicast delegate; it should be added instead of assigned
		{
			ensureMsg(false, TEXT("It is not possible to express this type as a literal value!"));
			return Term->Name;
		}
	}
	else
	{
		FString Prefix(TEXT(""));
		if ((Term->Context != NULL) && (Term->Context->Name != PSC_Self))
		{
			Prefix = TermToText(Term->Context);

			if (Term->Context->bIsStructContext)
			{
				Prefix += TEXT(".");
			}
			else
			{
				Prefix += TEXT("->");
			}
		}

		Prefix += Term->Name;
		return Prefix;
	}
}

FString FBlueprintCompilerCppBackend::LatentFunctionInfoTermToText(FBPTerminal* Term, FBlueprintCompiledStatement* TargetLabel)
{
	check(LatentInfoStruct);

	// Find the term name we need to fixup
	FString FixupTermName;
	for (UProperty* Prop = LatentInfoStruct->PropertyLink; Prop; Prop = Prop->PropertyLinkNext)
	{
		static const FName NeedsLatentFixup(TEXT("NeedsLatentFixup"));
		if (Prop->GetBoolMetaData(NeedsLatentFixup))
		{
			FixupTermName = Prop->GetName();
			break;
		}
	}

	check(!FixupTermName.IsEmpty());

	FString StructValues = Term->Name;

	// Index 0 is always the ubergraph
	const int32 TargetStateIndex = StateMapPerFunction[0].StatementToStateIndex(TargetLabel);
	const int32 LinkageTermStartIdx = StructValues.Find(FixupTermName);
	check(LinkageTermStartIdx != INDEX_NONE);
	StructValues = StructValues.Replace(TEXT("-1"), *FString::FromInt(TargetStateIndex));

	return FString(TEXT("F")) + LatentInfoStruct->GetName() + StructValues;
}

void FBlueprintCompilerCppBackend::EmitClassProperties(FStringOutputDevice& Target, UClass* SourceClass)
{
	// Emit class variables
	for (TFieldIterator<UProperty> It(SourceClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		UProperty* Property = *It;
		check(Property);

		Emit(Header, TEXT("\n\tUPROPERTY("));
		{
			TArray<FString> Tags = FEmitHelper::ProperyFlagsToTags(Property->PropertyFlags);
			Tags.Emplace(FEmitHelper::HandleRepNotifyFunc(Property));
			Tags.Emplace(FEmitHelper::HandleMetaData(Property, false));
			Tags.Remove(FString());

			FString AllTags;
			FEmitHelper::ArrayToString(Tags, AllTags, TEXT(", "));
			Emit(Header, *AllTags);
		}
		Emit(Header, TEXT(")\n"));
		Emit(Header, TEXT("\t"));
		Property->ExportCppDeclaration(Target, EExportedDeclaration::Member, NULL, EPropertyExportCPPFlags::CPPF_CustomTypeName);
		Emit(Header, TEXT(";\n"));
	}
}

void FBlueprintCompilerCppBackend::GenerateCodeFromClass(UClass* SourceClass, FString NewClassName, TIndirectArray<FKismetFunctionContext>& Functions, bool bGenerateStubsOnly)
{
	auto CleanCppClassName = NewClassName.IsEmpty() ? SourceClass->GetName() : NewClassName;
	CppClassName = FString(SourceClass->GetPrefixCPP()) + CleanCppClassName;

	UClass* SuperClass = SourceClass->GetSuperClass();

	Emit(Header, TEXT("#pragma once\n\n"));
	Emit(Header, *FString::Printf(TEXT("#include \"%s.generated.h\"\n\n"), *CleanCppClassName));

	// MC DELEGATE DECLARATION
	{
		auto DelegateDeclarations = FEmitHelper::EmitMulticastDelegateDeclarations(SourceClass);
		FString AllDeclarations;
		FEmitHelper::ArrayToString(DelegateDeclarations, AllDeclarations, TEXT(";\n"));
		if (DelegateDeclarations.Num())
		{
			Emit(Header, *AllDeclarations);
			Emit(Header, TEXT(";\n"));
		}
	}

	// GATHER ALL SC DELEGATES
	{
		TArray<UDelegateProperty*> Delegates;
		for (TFieldIterator<UDelegateProperty> It(SourceClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			Delegates.Add(*It);
		}

		for (auto& FuncContext : Functions)
		{
			for (TFieldIterator<UDelegateProperty> It(FuncContext.Function, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				Delegates.Add(*It);
			}
		}

		auto DelegateDeclarations = FEmitHelper::EmitSinglecastDelegateDeclarations(Delegates);
		FString AllDeclarations;
		FEmitHelper::ArrayToString(DelegateDeclarations, AllDeclarations, TEXT(";\n"));
		if (DelegateDeclarations.Num())
		{
			Emit(Header, *AllDeclarations);
			Emit(Header, TEXT(";\n"));
		}
	}

	Emit(Header, TEXT("UCLASS(Blueprintable)\n"));

	Emit(Header,
		*FString::Printf(TEXT("class %s : public %s%s\n"), *CppClassName, SuperClass->GetPrefixCPP(), *SuperClass->GetName()));
	Emit(Header,
		TEXT("{\n")
		TEXT("public:\n"));
	Emit(Header, *FString::Printf(TEXT("\tGENERATED_UCLASS_BODY()\n")));

	EmitClassProperties(Header, SourceClass);

	// Create the state map
	for (int32 i = 0; i < Functions.Num(); ++i)
	{
		StateMapPerFunction.Add(FFunctionLabelInfo());
		FunctionIndexMap.Add(&Functions[i], i);
	}

	// Emit function declarations and definitions (writes to header and body simultaneously)
	if (Functions.Num() > 0)
	{
		Emit(Header, TEXT("\n"));
	}

	TArray<FString> PersistentHeaders;
	PersistentHeaders.Add(FString(FApp::GetGameName()) + TEXT(".h"));
	PersistentHeaders.Add(CleanCppClassName + TEXT(".h"));
	PersistentHeaders.Add(TEXT("GeneratedCodeHelpers.h"));
	Emit(Body, *FEmitHelper::GatherHeadersToInclude(SourceClass, PersistentHeaders));

	//constructor
	Emit(Body, *FString::Printf(
		TEXT("%s::%s(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}\n\n"),
		*CppClassName, *CppClassName));

	for (int32 i = 0; i < Functions.Num(); ++i)
	{
		if (Functions[i].IsValid())
		{
			ConstructFunction(Functions[i], bGenerateStubsOnly);
		}
	}

	Emit(Header, TEXT("};\n\n"));

	Emit(Body, *FEmitHelper::EmitLifetimeReplicatedPropsImpl(SourceClass, CppClassName, TEXT("")));
}

void FBlueprintCompilerCppBackend::EmitCallDelegateStatment(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	check(Statement.FunctionContext && Statement.FunctionContext->AssociatedVarProperty);
	FSafeContextScopedEmmitter SafeContextScope(Body, Statement.FunctionContext->Context, *this, TEXT("\t\t\t"));

	Emit(Body, TEXT("\t\t\t"));
	Emit(Body, *SafeContextScope.GetAdditionalIndent());
	Emit(Body, *FString::Printf(TEXT("%s.Broadcast("), *TermToText(Statement.FunctionContext)));
	int32 NumParams = 0;
	for (TFieldIterator<UProperty> PropIt(Statement.FunctionToCall); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		UProperty* FuncParamProperty = *PropIt;

		if (!FuncParamProperty->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			if (NumParams > 0)
			{
				Emit(Body, TEXT(", "));
			}

			FString VarName;

			FBPTerminal* Term = Statement.RHS[NumParams];
			ensure(Term != NULL);

			// See if this is a hidden array param term, which needs to be fixed up with the final generated UArrayProperty
			if (FBPTerminal** ArrayParmTerm = Statement.ArrayCoersionTermMap.Find(Term))
			{
				Term->ObjectLiteral = (*ArrayParmTerm)->AssociatedVarProperty;
			}

			if ((Statement.TargetLabel != NULL) && (Statement.UbergraphCallIndex == NumParams))
			{
				// The target label will only ever be set on a call function when calling into the Ubergraph or
				// on a latent function that will later call into the ubergraph, either of which requires a patchup
				UStructProperty* StructProp = Cast<UStructProperty>(FuncParamProperty);
				if (StructProp && StructProp->Struct == LatentInfoStruct)
				{
					// Latent function info case
					VarName = LatentFunctionInfoTermToText(Term, Statement.TargetLabel);
				}
				else
				{
					// Ubergraph entry point case
					VarName = FString::FromInt(StateMapPerFunction[0].StatementToStateIndex(Statement.TargetLabel));
				}
			}
			else
			{
				// Emit a normal parameter term
				VarName = TermToText(Term, FuncParamProperty);
			}

			if (FuncParamProperty->HasAnyPropertyFlags(CPF_OutParm))
			{
				Emit(Body, TEXT("/*out*/ "));
			}
			Emit(Body, *VarName);

			NumParams++;
		}
	}
	Emit(Body, TEXT(");\n"));
}

void FBlueprintCompilerCppBackend::EmitCallStatment(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	const bool bCallOnDifferentObject = (Statement.FunctionContext != NULL) && (Statement.FunctionContext->Name != TEXT("self"));
	const bool bStaticCall = Statement.FunctionToCall->HasAnyFunctionFlags(FUNC_Static);

	const bool bUseSafeContext = bCallOnDifferentObject && !bStaticCall;
	FSafeContextScopedEmmitter SafeContextScope(Body, bUseSafeContext ? Statement.FunctionContext : NULL, *this, TEXT("\t\t\t"));

	Emit(Body, TEXT("\t\t\t"));
	Emit(Body, *SafeContextScope.GetAdditionalIndent());

	// Handle the return value of the function being called
	UProperty* FuncToCallReturnProperty = Statement.FunctionToCall->GetReturnProperty();
	if (FuncToCallReturnProperty != NULL)
	{
		FBPTerminal* Term = Statement.LHS;
		ensure(Term != NULL);

		FString ReturnVarName = TermToText(Term);
		Emit(Body, *FString::Printf(TEXT("%s = "), *ReturnVarName));
	}

	// Emit object to call the method on
	if (bCallOnDifferentObject) //@TODO: Badness, could be a self reference wired to another instance!
	{
		if (bStaticCall)
		{
			const bool bIsCustomThunk = Statement.FunctionToCall->HasMetaData(TEXT("CustomStructureParam")) || Statement.FunctionToCall->HasMetaData(TEXT("ArrayParm"));
			FString FullFunctionNamePrefix = bIsCustomThunk
				? TEXT("FCustomThunkTemplates::")
				: FString::Printf(TEXT("U%s::"), *Statement.FunctionToCall->GetOuter()->GetName());
			Emit(Body, *FullFunctionNamePrefix); //@TODO: Assuming U prefix but could be A
		}
		else
		{
			Emit(Body, *FString::Printf(TEXT("%s->"), *TermToText(Statement.FunctionContext, (UProperty*)(GetDefault<UObjectProperty>()))));
		}
	}

	// Emit method name
	FString FunctionNameToCall;
	Statement.FunctionToCall->GetName(FunctionNameToCall);
	if (Statement.bIsParentContext)
	{
		FunctionNameToCall = TEXT("Super::") + FunctionNameToCall;
	}
	Emit(Body, *FString::Printf(TEXT("%s"), *FunctionNameToCall));

	// Emit method parameter list
	Emit(Body, TEXT("("));
	{
		int32 NumParams = 0;

		for (TFieldIterator<UProperty> PropIt(Statement.FunctionToCall); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			UProperty* FuncParamProperty = *PropIt;

			if (!FuncParamProperty->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				if (NumParams > 0)
				{
					Emit(Body, TEXT(", "));
				}

				FString VarName;

				FBPTerminal* Term = Statement.RHS[NumParams];
				ensure(Term != NULL);

				// See if this is a hidden array param term, which needs to be fixed up with the final generated UArrayProperty
				if (FBPTerminal** ArrayParmTerm = Statement.ArrayCoersionTermMap.Find(Term))
				{
					Term->ObjectLiteral = (*ArrayParmTerm)->AssociatedVarProperty;
				}

				if ((Statement.TargetLabel != NULL) && (Statement.UbergraphCallIndex == NumParams))
				{
					// The target label will only ever be set on a call function when calling into the Ubergraph or
					// on a latent function that will later call into the ubergraph, either of which requires a patchup
					UStructProperty* StructProp = Cast<UStructProperty>(FuncParamProperty);
					if (StructProp && StructProp->Struct == LatentInfoStruct)
					{
						// Latent function info case
						VarName = LatentFunctionInfoTermToText(Term, Statement.TargetLabel);
					}
					else
					{
						// Ubergraph entry point case
						VarName = FString::FromInt(StateMapPerFunction[0].StatementToStateIndex(Statement.TargetLabel));
					}
				}
				else
				{
					// Emit a normal parameter term
					VarName = TermToText(Term, FuncParamProperty);
				}

				if (FuncParamProperty->HasAnyPropertyFlags(CPF_OutParm))
				{
					Emit(Body, TEXT("/*out*/ "));
				}
				Emit(Body, *VarName);

				NumParams++;
			}
		}
	}
	Emit(Body, TEXT(");\n"));
}

void FBlueprintCompilerCppBackend::EmitAssignmentStatment(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	FString DestinationExpression = TermToText(Statement.LHS);
	FString SourceExpression = TermToText(Statement.RHS[0], Statement.LHS->AssociatedVarProperty);

	FSafeContextScopedEmmitter SafeContextScope(Body, Statement.LHS->Context, *this, TEXT("\t\t\t"));
	// Emit the assignment statement
	Emit(Body, TEXT("\t\t\t"));
	Emit(Body, *SafeContextScope.GetAdditionalIndent());
	Emit(Body, *FString::Printf(TEXT("%s = %s;\n"), *DestinationExpression, *SourceExpression));
}

void FBlueprintCompilerCppBackend::EmitCastObjToInterfaceStatement(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	FString InterfaceClass = TermToText(Statement.RHS[0], (UProperty*)(GetDefault<UClassProperty>()));
	FString ObjectValue = TermToText(Statement.RHS[1], (UProperty*)(GetDefault<UObjectProperty>()));
	FString InterfaceValue = TermToText(Statement.LHS, (UProperty*)(GetDefault<UObjectProperty>()));

	Emit(Body, *FString::Printf(TEXT("\t\t\tif ( IsValid(%s) && %s->GetClass()->ImplementsInterface(%s) )\n"), *ObjectValue, *ObjectValue, *InterfaceClass));
	Emit(Body, *FString::Printf(TEXT("\t\t\t{\n")));
	Emit(Body, *FString::Printf(TEXT("\t\t\t\t%s.SetObject(%s);\n"), *InterfaceValue, *ObjectValue));
	Emit(Body, *FString::Printf(TEXT("\t\t\t\tvoid* IAddress = %s->GetInterfaceAddress(%s);\n"), *ObjectValue, *InterfaceClass));
	Emit(Body, *FString::Printf(TEXT("\t\t\t\t%s.SetInterface(IAddress);\n"), *InterfaceValue));
	Emit(Body, *FString::Printf(TEXT("\t\t\t}\n")));
	Emit(Body, *FString::Printf(TEXT("\t\t\telse\n")));
	Emit(Body, *FString::Printf(TEXT("\t\t\t{\n")));
	Emit(Body, *FString::Printf(TEXT("\t\t\t\t%s.SetObject(NULL);\n"), *InterfaceValue));
	Emit(Body, *FString::Printf(TEXT("\t\t\t}\n")));
}

void FBlueprintCompilerCppBackend::EmitCastBetweenInterfacesStatement(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	FString ClassToCastTo = TermToText(Statement.RHS[0], (UProperty*)(GetDefault<UClassProperty>()));
	FString InputInterface = TermToText(Statement.RHS[1], (UProperty*)(GetDefault<UInterfaceProperty>()));
	FString ResultInterface = TermToText(Statement.LHS, (UProperty*)(GetDefault<UInterfaceProperty>()));

	FString InputObject = FString::Printf(TEXT("%s.GetObjectRef()"), *InputInterface);

	Emit(Body, *FString::Printf(TEXT("\t\t\tif ( %s && %s->GetClass()->IsChildOf(%s) )\n"), *InputObject, *InputObject, *ClassToCastTo));
	Emit(Body, *FString::Printf(TEXT("\t\t\t{\n")));
	Emit(Body, *FString::Printf(TEXT("\t\t\t\t%s.SetObject(%s);\n"), *ResultInterface, *InputObject));
	Emit(Body, *FString::Printf(TEXT("\t\t\t\tvoid* IAddress = %s->GetInterfaceAddress(%s);\n"), *InputObject, *ClassToCastTo));
	Emit(Body, *FString::Printf(TEXT("\t\t\t\t%s.SetInterface(IAddress);\n"), *ResultInterface));
	Emit(Body, *FString::Printf(TEXT("\t\t\t}\n")));
	Emit(Body, *FString::Printf(TEXT("\t\t\telse\n")));
	Emit(Body, *FString::Printf(TEXT("\t\t\t{\n")));
	Emit(Body, *FString::Printf(TEXT("\t\t\t\t%s.SetObject(NULL);\n"), *ResultInterface));
	Emit(Body, *FString::Printf(TEXT("\t\t\t}\n")));
}

void FBlueprintCompilerCppBackend::EmitCastInterfaceToObjStatement(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	FString ClassToCastTo = TermToText(Statement.RHS[0], (UProperty*)(GetDefault<UClassProperty>()));
	FString InputInterface = TermToText(Statement.RHS[1], (UProperty*)(GetDefault<UInterfaceProperty>()));
	FString ResultObject = TermToText(Statement.LHS, (UProperty*)(GetDefault<UInterfaceProperty>()));

	FString InputObject = FString::Printf(TEXT("%s.GetObjectRef()"), *InputInterface);

	Emit(Body, *FString::Printf(TEXT("\t\t\t%s = Cast<%s>(%s);\n"),
		*ResultObject, *ClassToCastTo, *InputObject));
}

void FBlueprintCompilerCppBackend::EmitDynamicCastStatement(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	FString TargetClass = TermToText(Statement.RHS[0], (UProperty*)(GetDefault<UClassProperty>()));
	FString ObjectValue = TermToText(Statement.RHS[1], (UProperty*)(GetDefault<UObjectProperty>()));
	FString CastedValue = TermToText(Statement.LHS, (UProperty*)(GetDefault<UObjectProperty>()));

	Emit(Body, *FString::Printf(TEXT("\t\t\t%s = Cast<%s>(%s);\n"),
		*CastedValue, *TargetClass, *ObjectValue));
}

void FBlueprintCompilerCppBackend::EmitMetaCastStatement(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	FString DesiredClass = TermToText(Statement.RHS[0], (UProperty*)(GetDefault<UClassProperty>()));
	FString SourceClass = TermToText(Statement.RHS[1], (UProperty*)(GetDefault<UClassProperty>()));
	FString Destination = TermToText(Statement.LHS, (UProperty*)(GetDefault<UClassProperty>()));

	Emit(Body, *FString::Printf(TEXT("\t\t\t%s = DynamicMetaCast(%s, %s);\n"),
		*Destination, *DesiredClass, *SourceClass));
}

void FBlueprintCompilerCppBackend::EmitObjectToBoolStatement(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	FString ObjectTarget = TermToText(Statement.RHS[0]);
	FString DestinationExpression = TermToText(Statement.LHS);

	Emit(Body, *FString::Printf(TEXT("\t\t\t%s = (%s != NULL);\n"), *DestinationExpression, *ObjectTarget));
}

void FBlueprintCompilerCppBackend::EmitAddMulticastDelegateStatement(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	check(Statement.LHS && Statement.LHS->AssociatedVarProperty);
	FSafeContextScopedEmmitter SafeContextScope(Body, Statement.LHS->Context, *this, TEXT("\t\t\t"));

	Emit(Body, TEXT("\t\t\t"));
	Emit(Body, *SafeContextScope.GetAdditionalIndent());

	const FString Delegate = TermToText(Statement.LHS);
	const FString DelegateToAdd = TermToText(Statement.RHS[0]);

	Emit(Body, *FString::Printf(TEXT("%s.Add(%s);\n"), *Delegate, *DelegateToAdd));
}

void FBlueprintCompilerCppBackend::EmitRemoveMulticastDelegateStatement(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	check(Statement.LHS && Statement.LHS->AssociatedVarProperty);
	FSafeContextScopedEmmitter SafeContextScope(Body, Statement.LHS->Context, *this, TEXT("\t\t\t"));

	Emit(Body, TEXT("\t\t\t"));
	Emit(Body, *SafeContextScope.GetAdditionalIndent());

	const FString Delegate = TermToText(Statement.LHS);
	const FString DelegateToAdd = TermToText(Statement.RHS[0]);

	Emit(Body, *FString::Printf(TEXT("%s.Remove(%s);\n"), *Delegate, *DelegateToAdd));
}

void FBlueprintCompilerCppBackend::EmitBindDelegateStatement(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	check(2 == Statement.RHS.Num());
	check(Statement.LHS);
	FSafeContextScopedEmmitter SafeContextScope(Body, Statement.LHS->Context, *this, TEXT("\t\t\t"));

	Emit(Body, TEXT("\t\t\t"));
	Emit(Body, *SafeContextScope.GetAdditionalIndent());


	const FString Delegate = TermToText(Statement.LHS);
	const FString NameTerm = TermToText(Statement.RHS[0], GetDefault<UNameProperty>());
	const FString ObjectTerm = TermToText(Statement.RHS[1], GetDefault<UObjectProperty>());

	Emit(Body, *FString::Printf(TEXT("%s.BindUFunction(%s,%s);\n"), *Delegate, *ObjectTerm, *NameTerm));
}

void FBlueprintCompilerCppBackend::EmitClearMulticastDelegateStatement(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	check(Statement.LHS);
	FSafeContextScopedEmmitter SafeContextScope(Body, Statement.LHS->Context, *this, TEXT("\t\t\t"));

	Emit(Body, TEXT("\t\t\t"));
	Emit(Body, *SafeContextScope.GetAdditionalIndent());

	const FString Delegate = TermToText(Statement.LHS);

	Emit(Body, *FString::Printf(TEXT("%s.Clear();\n"), *Delegate));
}

void FBlueprintCompilerCppBackend::EmitCreateArrayStatement(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	FBPTerminal* ArrayTerm = Statement.LHS;
	const FString Array = TermToText(ArrayTerm);

	UArrayProperty* ArrayProperty = CastChecked<UArrayProperty>(ArrayTerm->AssociatedVarProperty);
	UProperty* InnerProperty = ArrayProperty->Inner;

	for (int32 i = 0; i < Statement.RHS.Num(); ++i)
	{
		FBPTerminal* CurrentTerminal = Statement.RHS[i];
		Emit(Body,
			*FString::Printf(
			TEXT("\t\t\t%s[%d] = %s;"),
			*Array,
			i,
			*TermToText(CurrentTerminal, (CurrentTerminal->bIsLiteral ? InnerProperty : NULL))));
	}
}

void FBlueprintCompilerCppBackend::EmitGotoStatement(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	if (Statement.Type == KCST_ComputedGoto)
	{
		FString NextStateExpression;
		NextStateExpression = TermToText(Statement.LHS, (UProperty*)(GetDefault<UIntProperty>()));

		Emit(Body, *FString::Printf(TEXT("\t\t\tCurrentState = %s;\n"), *NextStateExpression));
		Emit(Body, *FString::Printf(TEXT("\t\t\tbreak;\n")));
	}
	else if ((Statement.Type == KCST_GotoIfNot) || (Statement.Type == KCST_EndOfThreadIfNot) || (Statement.Type == KCST_GotoReturnIfNot))
	{
		FString ConditionExpression;
		ConditionExpression = TermToText(Statement.LHS, (UProperty*)(GetDefault<UBoolProperty>()));

		Emit(Body, *FString::Printf(TEXT("\t\t\tif (!%s)\n"), *ConditionExpression));
		Emit(Body, *FString::Printf(TEXT("\t\t\t{\n")));

		if (Statement.Type == KCST_EndOfThreadIfNot)
		{
			ensure(FunctionContext.bUseFlowStack);
			Emit(Body, TEXT("\t\t\t\tCurrentState = (StateStack.Num() > 0) ? StateStack.Pop(/*bAllowShrinking=*/ false) : -1;\n"));
		}
		else if (Statement.Type == KCST_GotoReturnIfNot)
		{
			Emit(Body, TEXT("\t\t\t\tCurrentState = -1;\n"));
		}
		else
		{
			Emit(Body, *FString::Printf(TEXT("\t\t\t\tCurrentState = %d;\n"), StatementToStateIndex(FunctionContext, Statement.TargetLabel)));
		}

		Emit(Body, *FString::Printf(TEXT("\t\t\t\tbreak;\n")));
		Emit(Body, *FString::Printf(TEXT("\t\t\t}\n")));
	}
	else if (Statement.Type == KCST_GotoReturn)
	{
		Emit(Body, TEXT("\t\t\tCurrentState = -1;\n"));
	}
	else
	{
		Emit(Body, *FString::Printf(TEXT("\t\t\tCurrentState = %d;\n"), StatementToStateIndex(FunctionContext, Statement.TargetLabel)));
		Emit(Body, *FString::Printf(TEXT("\t\t\tbreak;\n")));
	}
}

void FBlueprintCompilerCppBackend::EmitPushStateStatement(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	ensure(FunctionContext.bUseFlowStack);
	Emit(Body, *FString::Printf(TEXT("\t\t\tStateStack.Push(%d);\n"), StatementToStateIndex(FunctionContext, Statement.TargetLabel)));
}

void FBlueprintCompilerCppBackend::EmitEndOfThreadStatement(FKismetFunctionContext& FunctionContext, const FString& ReturnValueString)
{
	ensure(FunctionContext.bUseFlowStack);
	Emit(Body, TEXT("\t\t\tCurrentState = (StateStack.Num() > 0) ? StateStack.Pop(/*bAllowShrinking=*/ false) : -1;\n"));
	Emit(Body, TEXT("\t\t\tbreak;\n"));
}

void FBlueprintCompilerCppBackend::EmitReturnStatement(FKismetFunctionContext& FunctionContext, const FString& ReturnValueString)
{
	Emit(Body, *FString::Printf(TEXT("\treturn%s;\n"), *ReturnValueString));
}

void FBlueprintCompilerCppBackend::DeclareLocalVariables(FKismetFunctionContext& FunctionContext, TArray<UProperty*>& LocalVariables)
{
	for (int32 i = 0; i < LocalVariables.Num(); ++i)
	{
		UProperty* LocalVariable = LocalVariables[i];

		Emit(Body, TEXT("\t"));
		LocalVariable->ExportCppDeclaration(Body, EExportedDeclaration::Local, NULL, EPropertyExportCPPFlags::CPPF_CustomTypeName);
		Emit(Body, TEXT("{};\n"));
	}

	if (LocalVariables.Num() > 0)
	{
		Emit(Body, TEXT("\n"));
	}
}

void FBlueprintCompilerCppBackend::DeclareStateSwitch(FKismetFunctionContext& FunctionContext)
{
	if (FunctionContext.bUseFlowStack)
	{
		Emit(Body, TEXT("\tTArray< int32, TInlineAllocator<8> > StateStack;\n"));
	}
	Emit(Body, TEXT("\tint32 CurrentState = 0;\n"));
	Emit(Body, TEXT("\tdo\n"));
	Emit(Body, TEXT("\t{\n"));
	Emit(Body, TEXT("\t\tswitch( CurrentState )\n"));
	Emit(Body, TEXT("\t\t{\n"));
	Emit(Body, TEXT("\t\tcase 0:\n"));
}

void FBlueprintCompilerCppBackend::CloseStateSwitch(FKismetFunctionContext& FunctionContext)
{
	// Default error-catching case 
	Emit(Body, TEXT("\t\tdefault:\n"));
	if (FunctionContext.bUseFlowStack)
	{
		Emit(Body, TEXT("\t\t\tcheck(false); // Invalid state\n"));
	}
	Emit(Body, TEXT("\t\t\tbreak;\n"));

	// Close the switch block and do-while loop
	Emit(Body, TEXT("\t\t}\n"));
	Emit(Body, TEXT("\t} while( CurrentState != -1 );\n"));
}

void FBlueprintCompilerCppBackend::ConstructFunction(FKismetFunctionContext& FunctionContext, bool bGenerateStubOnly)
{
	if (FunctionContext.IsDelegateSignature())
	{
		return;
	}

	UFunction* Function = FunctionContext.Function;

	UProperty* ReturnValue = NULL;
	TArray<UProperty*> LocalVariables;

	{
		FString FunctionName;
		Function->GetName(FunctionName);

		TArray<UProperty*> ArgumentList;

		// Split the function property list into arguments, a return value (if any), and local variable declarations
		for (TFieldIterator<UProperty> It(Function); It; ++It)
		{
			UProperty* Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_Parm))
			{
				if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					if (ReturnValue == NULL)
					{
						ReturnValue = Property;
						LocalVariables.Add(Property);
					}
					else
					{
						MessageLog.Error(*FString::Printf(TEXT("Function %s from graph @@ has more than one return value (named %s and %s)"),
							*FunctionName, *ReturnValue->GetName(), *Property->GetName()), FunctionContext.SourceGraph);
					}
				}
				else
				{
					ArgumentList.Add(Property);
				}
			}
			else
			{
				LocalVariables.Add(Property);
			}
		}

		// Emit the declaration
		const FString ReturnType = ReturnValue ? ReturnValue->GetCPPType(NULL, EPropertyExportCPPFlags::CPPF_CustomTypeName) : TEXT("void");

		//@TODO: Make the header+body export more uniform
		{
			const FString Start = FString::Printf(TEXT("%s %s%s%s("), *ReturnType, TEXT("%s"), TEXT("%s"), *FunctionName);

			Emit(Header, *FString::Printf(TEXT("\t%s\n"), *FEmitHelper::EmitUFuntion(Function)));
			Emit(Header, TEXT("\t"));
			if (Function->HasAllFunctionFlags(FUNC_Static))
			{
				Emit(Header, TEXT("static "));
			}
			Emit(Header, *FString::Printf(*Start, TEXT(""), TEXT("")));
			Emit(Body, *FString::Printf(*Start, *CppClassName, TEXT("::")));

			for (int32 i = 0; i < ArgumentList.Num(); ++i)
			{
				UProperty* ArgProperty = ArgumentList[i];

				if (i > 0)
				{
					Emit(Header, TEXT(", "));
					Emit(Body, TEXT(", "));
				}

				if (ArgProperty->HasAnyPropertyFlags(CPF_OutParm))
				{
					Emit(Header, TEXT("/*out*/ "));
					Emit(Body, TEXT("/*out*/ "));
				}

				ArgProperty->ExportCppDeclaration(Header, EExportedDeclaration::Parameter, NULL, EPropertyExportCPPFlags::CPPF_CustomTypeName);
				ArgProperty->ExportCppDeclaration(Body, EExportedDeclaration::Parameter, NULL, EPropertyExportCPPFlags::CPPF_CustomTypeName);
			}

			Emit(Header, TEXT(")"));
			Emit(Header, TEXT(";\n"));
			Emit(Body, TEXT(")\n"));
		}

		// Start the body of the implementation
		Emit(Body, TEXT("{\n"));
	}

	const FString ReturnValueString = ReturnValue ? (FString(TEXT(" ")) + ReturnValue->GetName()) : TEXT("");

	if (!bGenerateStubOnly)
	{
		// Emit local variables
		DeclareLocalVariables(FunctionContext, LocalVariables);

		bool bUseSwitchState = false;
		for (auto Node : FunctionContext.LinearExecutionList)
		{
			TArray<FBlueprintCompiledStatement*>* StatementList = FunctionContext.StatementsPerNode.Find(Node);
			if (StatementList)
			{
				for (auto Statement : (*StatementList))
				{
					if (Statement && (
						Statement->Type == KCST_UnconditionalGoto ||
						Statement->Type == KCST_PushState ||
						Statement->Type == KCST_GotoIfNot ||
						Statement->Type == KCST_ComputedGoto ||
						Statement->Type == KCST_EndOfThread ||
						Statement->Type == KCST_EndOfThreadIfNot ||
						Statement->Type == KCST_GotoReturn ||
						Statement->Type == KCST_GotoReturnIfNot))
					{
						bUseSwitchState = true;
						break;
					}
				}
			}
			if (bUseSwitchState)
			{
				break;
			}
		}

		if (bUseSwitchState)
		{
			DeclareStateSwitch(FunctionContext);
		}


		// Run thru code looking only at things marked as jump targets, to make sure the jump targets are ordered in order of appearance in the linear execution list
		// Emit code in the order specified by the linear execution list (the first node is always the entry point for the function)
		for (int32 NodeIndex = 0; NodeIndex < FunctionContext.LinearExecutionList.Num(); ++NodeIndex)
		{
			UEdGraphNode* StatementNode = FunctionContext.LinearExecutionList[NodeIndex];
			TArray<FBlueprintCompiledStatement*>* StatementList = FunctionContext.StatementsPerNode.Find(StatementNode);

			if (StatementList != NULL)
			{
				for (int32 StatementIndex = 0; StatementIndex < StatementList->Num(); ++StatementIndex)
				{
					FBlueprintCompiledStatement& Statement = *((*StatementList)[StatementIndex]);

					if (Statement.bIsJumpTarget)
					{
						// Just making sure we number them in order of appearance, so jump statements don't influence the order
						const int32 StateNum = StatementToStateIndex(FunctionContext, &Statement);
					}
				}
			}
		}

		// Emit code in the order specified by the linear execution list (the first node is always the entry point for the function)
		for (int32 NodeIndex = 0; NodeIndex < FunctionContext.LinearExecutionList.Num(); ++NodeIndex)
		{
			UEdGraphNode* StatementNode = FunctionContext.LinearExecutionList[NodeIndex];
			TArray<FBlueprintCompiledStatement*>* StatementList = FunctionContext.StatementsPerNode.Find(StatementNode);

			if (StatementList != NULL)
			{
				for (int32 StatementIndex = 0; StatementIndex < StatementList->Num(); ++StatementIndex)
				{
					FBlueprintCompiledStatement& Statement = *((*StatementList)[StatementIndex]);

					if (Statement.bIsJumpTarget && bUseSwitchState)
					{
						const int32 StateNum = StatementToStateIndex(FunctionContext, &Statement);
						Emit(Body, *FString::Printf(TEXT("\n\t\tcase %d:\n"), StateNum));
					}

					switch (Statement.Type)
					{
					case KCST_Nop:
						Emit(Body, TEXT("\t\t\t//No operation.\n"));
						break;
					case KCST_WireTraceSite:
						Emit(Body, TEXT("\t\t\t// Wire debug site.\n"));
						break;
					case KCST_DebugSite:
						Emit(Body, TEXT("\t\t\t// Debug site.\n"));
						break;
					case KCST_CallFunction:
						EmitCallStatment(FunctionContext, Statement);
						break;
					case KCST_CallDelegate:
						EmitCallDelegateStatment(FunctionContext, Statement);
						break;
					case KCST_Assignment:
						EmitAssignmentStatment(FunctionContext, Statement);
						break;
					case KCST_CastObjToInterface:
						EmitCastObjToInterfaceStatement(FunctionContext, Statement);
						break;
					case KCST_CrossInterfaceCast:
						EmitCastBetweenInterfacesStatement(FunctionContext, Statement);
						break;
					case KCST_CastInterfaceToObj:
						EmitCastInterfaceToObjStatement(FunctionContext, Statement);
						break;
					case KCST_DynamicCast:
						EmitDynamicCastStatement(FunctionContext, Statement);
						break;
					case KCST_ObjectToBool:
						EmitObjectToBoolStatement(FunctionContext, Statement);
						break;
					case KCST_AddMulticastDelegate:
						EmitAddMulticastDelegateStatement(FunctionContext, Statement);
						break;
					case KCST_RemoveMulticastDelegate:
						EmitRemoveMulticastDelegateStatement(FunctionContext, Statement);
						break;
					case KCST_BindDelegate:
						EmitBindDelegateStatement(FunctionContext, Statement);
						break;
					case KCST_ClearMulticastDelegate:
						EmitClearMulticastDelegateStatement(FunctionContext, Statement);
						break;
					case KCST_CreateArray:
						EmitCreateArrayStatement(FunctionContext, Statement);
						break;
					case KCST_ComputedGoto:
					case KCST_UnconditionalGoto:
					case KCST_GotoIfNot:
					case KCST_EndOfThreadIfNot:
					case KCST_GotoReturn:
					case KCST_GotoReturnIfNot:
						EmitGotoStatement(FunctionContext, Statement);
						break;
					case KCST_PushState:
						EmitPushStateStatement(FunctionContext, Statement);
						break;
					case KCST_EndOfThread:
						EmitEndOfThreadStatement(FunctionContext, ReturnValueString);
						break;
					case KCST_Comment:
						Emit(Body, *FString::Printf(TEXT("\t\t\t// %s\n"), *Statement.Comment));
						break;
					case KCST_MetaCast:
						EmitMetaCastStatement(FunctionContext, Statement);
						break;
					case KCST_Return:
						Emit(Body, TEXT("\t\t\t// Return statement.\n"));
						break;
					default:
						Emit(Body, TEXT("\t// Warning: Ignoring unsupported statement\n"));
						UE_LOG(LogK2Compiler, Warning, TEXT("C++ backend encountered unsupported statement type %d"), (int32)Statement.Type);
						break;
					};
				}
			}
		}

		if (bUseSwitchState)
		{
			CloseStateSwitch(FunctionContext);
		}
	}

	EmitReturnStatement(FunctionContext, ReturnValueString);

	Emit(Body, TEXT("}\n\n"));
}