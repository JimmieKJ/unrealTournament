// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintCompilerCppBackendModulePrivatePCH.h"
#include "BlueprintCompilerCppBackendBase.h"
#include "BlueprintCompilerCppBackendUtils.h"

void FBlueprintCompilerCppBackendBase::EmitStructProperties(FEmitterLocalContext& EmitterContext, UStruct* SourceClass)
{
	// Emit class variables
	for (TFieldIterator<UProperty> It(SourceClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		UProperty* Property = *It;
		check(Property);
		FString PropertyMacro(TEXT("UPROPERTY("));
		{
			TArray<FString> Tags = FEmitHelper::ProperyFlagsToTags(Property->PropertyFlags, nullptr != Cast<UClass>(SourceClass));
			Tags.Emplace(FEmitHelper::HandleRepNotifyFunc(Property));
			Tags.Emplace(FEmitHelper::HandleMetaData(Property, false));
			Tags.Remove(FString());

			FString AllTags;
			FEmitHelper::ArrayToString(Tags, AllTags, TEXT(", "));
			PropertyMacro += AllTags;
		}
		PropertyMacro += TEXT(")");
		EmitterContext.Header.AddLine(PropertyMacro);

		const FString CppDeclaration = EmitterContext.ExportCppDeclaration(Property, EExportedDeclaration::Member, EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend);
		EmitterContext.Header.AddLine(CppDeclaration + TEXT(";"));
	}
}

void FBlueprintCompilerCppBackendBase::DeclareDelegates(FEmitterLocalContext& EmitterContext, TIndirectArray<FKismetFunctionContext>& Functions)
{
	// MC DELEGATE DECLARATION
	{
		FEmitHelper::EmitMulticastDelegateDeclarations(EmitterContext);
	}

	// GATHER ALL SC DELEGATES
	{
		TArray<UDelegateProperty*> Delegates;
		for (TFieldIterator<UDelegateProperty> It(EmitterContext.GetCurrentlyGeneratedClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
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

		TArray<UFunction*> SCDelegateSignaturesWithoutType;
		// Don't declare signatures, that are already declared in a native class
		for (int32 I = 0; I < Delegates.Num();)
		{
			auto Delegate = Delegates[I];
			auto DelegateSignature = Delegate ? Delegate->SignatureFunction : nullptr;
			auto DelegateSignatureOwner = DelegateSignature ? DelegateSignature->GetOwnerStruct() : nullptr;
			if (DelegateSignatureOwner && DelegateSignatureOwner->HasAnyInternalFlags(EInternalObjectFlags::Native))
			{
				if (DelegateSignature->HasAllFunctionFlags(FUNC_MulticastDelegate))
				{
					SCDelegateSignaturesWithoutType.AddUnique(DelegateSignature);
				}
				Delegates.RemoveAtSwap(I);
			}
			else
			{
				I++;
			}
		}

		// remove duplicates, n^2, but n is small:
		for (int32 I = 0; I < Delegates.Num(); ++I)
		{
			UFunction* TargetFn = Delegates[I]->SignatureFunction;
			for (int32 J = I + 1; J < Delegates.Num(); ++J)
			{
				if (TargetFn == Delegates[J]->SignatureFunction)
				{
					// swap erase:
					Delegates[J] = Delegates[Delegates.Num() - 1];
					Delegates.RemoveAt(Delegates.Num() - 1);
				}
			}
		}

		for (UFunction* SCDelegateSignature : SCDelegateSignaturesWithoutType)
		{
			FString SCType = FString::Printf(TEXT("F__%s__SC"), *FEmitHelper::GetCppName(SCDelegateSignature));
			FEmitHelper::EmitSinglecastDelegateDeclarations_Inner(EmitterContext, SCDelegateSignature, SCType);
			EmitterContext.MCDelegateSignatureToSCDelegateType.Add(SCDelegateSignature, SCType);
		}

		FEmitHelper::EmitSinglecastDelegateDeclarations(EmitterContext, Delegates);
	}
}

FString FBlueprintCompilerCppBackendBase::GenerateCodeFromClass(UClass* SourceClass, TIndirectArray<FKismetFunctionContext>& Functions, bool bGenerateStubsOnly, FString& OutCppBody)
{
	CleanBackend();

	// use GetBaseFilename() so that we can coordinate #includes and filenames
	auto CleanCppClassName = FEmitHelper::GetBaseFilename(SourceClass);
	auto CppClassName = FEmitHelper::GetCppName(SourceClass);
	
	FGatherConvertedClassDependencies Dependencies(SourceClass);
	FEmitterLocalContext EmitterContext(Dependencies);
	EmitFileBeginning(CleanCppClassName, EmitterContext);

	UClass* OriginalSourceClass = Dependencies.FindOriginalClass(SourceClass);
	ensure(OriginalSourceClass != SourceClass);

	// Class declaration
	const bool bIsInterface = SourceClass->IsChildOf<UInterface>();
	if (bIsInterface)
	{
		EmitterContext.Header.AddLine(FString::Printf(TEXT("UINTERFACE(Blueprintable, %s)"), *FEmitHelper::ReplaceConvertedMetaData(OriginalSourceClass)));
		EmitterContext.Header.AddLine(FString::Printf(TEXT("class %s : public UInterface"), *FEmitHelper::GetCppName(SourceClass, true)));
		EmitterContext.Header.AddLine(TEXT("{"));
		EmitterContext.Header.IncreaseIndent();
		EmitterContext.Header.AddLine(TEXT("GENERATED_BODY()"));
		EmitterContext.Header.DecreaseIndent();
		EmitterContext.Header.AddLine(TEXT("};"));
		EmitterContext.Header.AddLine(FString::Printf(TEXT("class %s"), *CppClassName));
	}
	else
	{
		EmitterContext.Header.AddLine(FString::Printf(TEXT("UCLASS(%s%s)")
			, (!SourceClass->IsChildOf<UBlueprintFunctionLibrary>()) ? TEXT("Blueprintable, BlueprintType, ") : TEXT("")
			, *FEmitHelper::ReplaceConvertedMetaData(OriginalSourceClass)));

		UClass* SuperClass = SourceClass->GetSuperClass();
		FString ClassDefinition = FString::Printf(TEXT("class %s : public %s"), *CppClassName, *FEmitHelper::GetCppName(SuperClass));

		for (auto& ImplementedInterface : SourceClass->Interfaces)
		{
			if (ImplementedInterface.Class)
			{
				ClassDefinition += FString::Printf(TEXT(", public %s"), *FEmitHelper::GetCppName(ImplementedInterface.Class));
			}
		}
		EmitterContext.Header.AddLine(ClassDefinition);
	}
	
	// Begin scope
	EmitterContext.Header.AddLine(TEXT("{"));
	EmitterContext.Header.AddLine(TEXT("public:"));
	EmitterContext.Header.IncreaseIndent();
	EmitterContext.Header.AddLine(TEXT("GENERATED_BODY()"));

	DeclareDelegates(EmitterContext, Functions);

	EmitStructProperties(EmitterContext, SourceClass);

	// Emit function declarations and definitions (writes to header and body simultaneously)
	if (!bIsInterface)
	{
		EmitterContext.Header.AddLine(FString::Printf(TEXT("%s(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());"), *CppClassName));
		EmitterContext.Header.AddLine(TEXT("virtual void PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph) override;"));
		EmitterContext.Header.AddLine(TEXT("static void __StaticDependenciesAssets(TArray<FBlueprintDependencyData>& AssetsToLoad);"));

		FEmitDefaultValueHelper::GenerateConstructor(EmitterContext);
	}

	// Create the state map
	for (int32 i = 0; i < Functions.Num(); ++i)
	{
		StateMapPerFunction.Add(FFunctionLabelInfo());
		FunctionIndexMap.Add(&Functions[i], i);
	}

	for (int32 i = 0; i < Functions.Num(); ++i)
	{
		if (Functions[i].IsValid())
		{
			ConstructFunction(Functions[i], EmitterContext, bGenerateStubsOnly);
		}
	}

	EmitterContext.Header.DecreaseIndent();
	EmitterContext.Header.AddLine(TEXT("public:"));
	EmitterContext.Header.IncreaseIndent();

	FBackendHelperUMG::WidgetFunctionsInHeader(EmitterContext);

	EmitterContext.Header.DecreaseIndent();
	EmitterContext.Header.AddLine(TEXT("};"));

	FEmitHelper::EmitLifetimeReplicatedPropsImpl(EmitterContext);

	CleanBackend();

	OutCppBody = EmitterContext.Body.Result;
	return EmitterContext.Header.Result;
}

void FBlueprintCompilerCppBackendBase::DeclareLocalVariables(FEmitterLocalContext& EmitterContext, TArray<UProperty*>& LocalVariables)
{
	for (int32 i = 0; i < LocalVariables.Num(); ++i)
	{
		UProperty* LocalVariable = LocalVariables[i];

		const FString CppDeclaration = EmitterContext.ExportCppDeclaration(LocalVariable, EExportedDeclaration::Local, EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend);
		EmitterContext.AddLine(CppDeclaration + TEXT("{};"));
	}
}

void FBlueprintCompilerCppBackendBase::ConstructFunction(FKismetFunctionContext& FunctionContext, FEmitterLocalContext& EmitterContext, bool bGenerateStubOnly)
{
	if (FunctionContext.IsDelegateSignature())
	{
		return;
	}

	TArray<UProperty*> LocalVariables;
	TArray<UProperty*> ArgumentList;
	// Split the function property list into arguments, a return value (if any), and local variable declarations
	for (UProperty* Property : TFieldRange<UProperty>(FunctionContext.Function))
	{
		const bool bNeedLocalVariable = !Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_ReturnParm);
		TArray<UProperty*>& PropertyDest = bNeedLocalVariable ? LocalVariables : ArgumentList;
		PropertyDest.Add(Property);
	}

	if (FunctionContext.bIsUbergraph)
	{
		EmitterContext.AddLine(TEXT("PRAGMA_DISABLE_OPTIMIZATION"));
	}

	ConstructFunctionDeclaration(EmitterContext, FunctionContext, ArgumentList);

	// Start the body of the implementation
	EmitterContext.AddLine(TEXT("{"));
	EmitterContext.IncreaseIndent();
	if (!bGenerateStubOnly)
	{
		DeclareLocalVariables(EmitterContext, LocalVariables);
		ConstructFunctionBody(EmitterContext, FunctionContext);
	}

	UProperty* ReturnValue = FunctionContext.Function->GetReturnProperty();
	const FString ReturnValueString = ReturnValue ? (FString(TEXT(" ")) + FEmitHelper::GetCppName(ReturnValue)) : TEXT("");
	EmitterContext.AddLine(FString::Printf(TEXT("return%s;"), *ReturnValueString));
	EmitterContext.DecreaseIndent();
	EmitterContext.AddLine(TEXT("}"));

	if (FunctionContext.bIsUbergraph)
	{
		EmitterContext.AddLine(TEXT("PRAGMA_ENABLE_OPTIMIZATION"));
	}
}

void FBlueprintCompilerCppBackendBase::ConstructFunctionDeclaration(FEmitterLocalContext &EmitterContext, FKismetFunctionContext &FunctionContext, TArray<UProperty*> &ArgumentList)
{
	FString FunctionHeaderName = FEmitHelper::GetCppName(FunctionContext.Function);
	FString FunctionBodyName = FunctionHeaderName;
	const bool bStaticFunction = FunctionContext.Function->HasAllFunctionFlags(FUNC_Static);
	const bool bInInterface = FunctionContext.Function->GetOwnerClass()->IsChildOf<UInterface>();
	bool bAddConst = false;
	bool bIsOverride = false;
	bool bIsVirtual = !bInInterface && !bStaticFunction && !FunctionContext.IsEventGraph();

	{
		UFunction* const Function = FunctionContext.Function;
		const UFunction* const OriginalFunction = FEmitHelper::GetOriginalFunction(Function);
		TArray<FString> AdditionalMetaData;
		TArray<FString> AdditionalTags;
		bool bGenerateAsNativeEventImplementation = false;
		const bool bNetImplementation = !bInInterface && Function->HasAllFunctionFlags(FUNC_Net) && !Function->HasAnyFunctionFlags(FUNC_NetResponse);

		const UBlueprintGeneratedClass* const OriginalFuncOwnerAsBPGC = Cast<UBlueprintGeneratedClass>(OriginalFunction->GetOwnerClass());
		const bool bBPInterfaceImplementation = OriginalFuncOwnerAsBPGC && OriginalFuncOwnerAsBPGC->IsChildOf<UInterface>();

		if (bInInterface)
		{
			AdditionalTags.Emplace(TEXT("BlueprintImplementableEvent"));
		}
		else if (bNetImplementation)
		{
			FunctionBodyName = FunctionHeaderName + TEXT("_Implementation");
		}
		else if (FEmitHelper::ShouldHandleAsNativeEvent(Function))
		{
			bGenerateAsNativeEventImplementation = true;
			FunctionBodyName = FunctionHeaderName = FEmitHelper::GetCppName(OriginalFunction) + TEXT("_Implementation");
			bAddConst = OriginalFunction->HasAllFunctionFlags(FUNC_Const);
		}
		else if (FEmitHelper::ShouldHandleAsImplementableEvent(Function) || bBPInterfaceImplementation)
		{
			//The function "bpf__BIE__pf" should never be called directly. Only via function "BIE" with generated implementation.
			bIsVirtual = false;
			AdditionalMetaData.Emplace(TEXT("CppFromBpEvent"));
		}

		ensure(!bIsVirtual || Function->IsSignatureCompatibleWith(OriginalFunction));
		bIsOverride = bGenerateAsNativeEventImplementation || (bIsVirtual && (Function != OriginalFunction));

		if (bNetImplementation && bIsOverride)
		{
			FunctionHeaderName = FunctionBodyName;
		}
		else if (!bGenerateAsNativeEventImplementation)
		{
			EmitterContext.Header.AddLine(FEmitHelper::EmitUFuntion(Function, AdditionalTags, AdditionalMetaData));
		}
	}

	const FString ReturnType = GenerateReturnType(EmitterContext, FunctionContext.Function);
	const FString ArgList = GenerateArgList(EmitterContext, ArgumentList);

	EmitterContext.Body.AddLine(FString::Printf(TEXT("%s %s::%s%s%s")
		, *ReturnType
		, *FEmitHelper::GetCppName(EmitterContext.GetCurrentlyGeneratedClass())
		, *FunctionBodyName
		, *ArgList
		, bAddConst ? TEXT(" const") : TEXT("")));

	EmitterContext.Header.AddLine(FString::Printf(TEXT("%s%s%s %s%s%s%s;")
		, bStaticFunction ? TEXT("static ") : TEXT("")
		, bIsVirtual ? TEXT("virtual ") : TEXT("")
		, *ReturnType
		, *FunctionHeaderName
		, *ArgList
		, bAddConst ? TEXT(" const") : TEXT("")
		, bIsOverride ? TEXT(" override") : TEXT("")));
}

FString FBlueprintCompilerCppBackendBase::GenerateArgList(const FEmitterLocalContext &EmitterContext, const TArray<UProperty*> &ArgumentList)
{
	FString ArgListStr = TEXT("(");
	for (int32 i = 0; i < ArgumentList.Num(); ++i)
	{
		UProperty* ArgProperty = ArgumentList[i];

		if (i > 0)
		{
			ArgListStr += TEXT(", ");
		}

		if (ArgProperty->HasAnyPropertyFlags(CPF_OutParm))
		{
			ArgListStr += TEXT("/*out*/ ");
		}

		ArgListStr += EmitterContext.ExportCppDeclaration(ArgProperty, EExportedDeclaration::Parameter, EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend);
	}
	ArgListStr += TEXT(")");
	return ArgListStr;
}

FString FBlueprintCompilerCppBackendBase::GenerateReturnType(const FEmitterLocalContext &EmitterContext, const UFunction* Function)
{
	UProperty* ReturnValue = Function->GetReturnProperty();
	if (ReturnValue)
	{
		const uint32 LocalExportCPPFlags = EPropertyExportCPPFlags::CPPF_CustomTypeName
			| EPropertyExportCPPFlags::CPPF_NoConst
			| EPropertyExportCPPFlags::CPPF_NoRef
			| EPropertyExportCPPFlags::CPPF_NoStaticArray
			| EPropertyExportCPPFlags::CPPF_BlueprintCppBackend;
		return EmitterContext.ExportCppDeclaration(ReturnValue, EExportedDeclaration::Parameter, LocalExportCPPFlags, true);
	}
	return TEXT("void");
}

void FBlueprintCompilerCppBackendBase::ConstructFunctionBody(FEmitterLocalContext& EmitterContext, FKismetFunctionContext &FunctionContext)
{
	const bool bUseSwitchState = FunctionContext.MustUseSwitchState(nullptr);

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

	InnerFunctionImplementation(FunctionContext, EmitterContext, bUseSwitchState);
}

FString FBlueprintCompilerCppBackendBase::GenerateCodeFromEnum(UUserDefinedEnum* SourceEnum)
{
	check(SourceEnum);
	FCodeText Header;
	Header.AddLine(TEXT("#pragma once"));
	// use GetBaseFilename() so that we can coordinate #includes and filenames
	Header.AddLine(FString::Printf(TEXT("#include \"%s.generated.h\""), *FEmitHelper::GetBaseFilename(SourceEnum)));
	Header.AddLine(FString::Printf(TEXT("UENUM(BlueprintType, %s )"), *FEmitHelper::ReplaceConvertedMetaData(SourceEnum)));
	Header.AddLine(FString::Printf(TEXT("enum class %s  : uint8"), *FEmitHelper::GetCppName(SourceEnum)));
	Header.AddLine(TEXT("{"));
	Header.IncreaseIndent();
	for (int32 Index = 0; Index < SourceEnum->NumEnums(); ++Index)
	{
		const FString ElemName = SourceEnum->GetEnumName(Index);
		const int32 ElemValue = Index;

		const FString& DisplayNameMD = SourceEnum->GetMetaData(TEXT("DisplayName"), ElemValue);// TODO: value or index?
		const FString Meta = DisplayNameMD.IsEmpty() ? FString() : FString::Printf(TEXT("UMETA(DisplayName = \"%s\")"), *DisplayNameMD);
		Header.AddLine(FString::Printf(TEXT("%s = %d %s,"), *ElemName, ElemValue, *Meta));
	}
	Header.DecreaseIndent();
	Header.AddLine(TEXT("};"));
	return Header.Result;
}

FString FBlueprintCompilerCppBackendBase::GenerateCodeFromStruct(UUserDefinedStruct* SourceStruct)
{
	check(SourceStruct);
	FGatherConvertedClassDependencies Dependencies(SourceStruct);
	FEmitterLocalContext EmitterContext(Dependencies);
	// use GetBaseFilename() so that we can coordinate #includes and filenames
	EmitFileBeginning(FEmitHelper::GetBaseFilename(SourceStruct), EmitterContext);

	EmitterContext.Header.AddLine(FString::Printf(TEXT("USTRUCT(BlueprintType, %s)"), *FEmitHelper::ReplaceConvertedMetaData(SourceStruct)));
	EmitterContext.Header.AddLine(FString::Printf(TEXT("struct %s"), *FEmitHelper::GetCppName(SourceStruct)));
	EmitterContext.Header.AddLine(TEXT("{"));
	EmitterContext.Header.AddLine(TEXT("public:"));
	EmitterContext.Header.IncreaseIndent();
	EmitterContext.Header.AddLine(TEXT("GENERATED_BODY()"));
	EmitStructProperties(EmitterContext, SourceStruct);

	FEmitDefaultValueHelper::GenerateGetDefaultValue(SourceStruct, EmitterContext);
	EmitterContext.Header.DecreaseIndent();
	EmitterContext.Header.AddLine(TEXT("};"));

	return EmitterContext.Header.Result;
}

FString FBlueprintCompilerCppBackendBase::GenerateWrapperForClass(UClass* SourceClass)
{
	FGatherConvertedClassDependencies Dependencies(SourceClass);
	FEmitterLocalContext EmitterContext(Dependencies);

	TArray<UFunction*> FunctionsToGenerate;
	for (auto Func : TFieldRange<UFunction>(SourceClass, EFieldIteratorFlags::ExcludeSuper))
	{
		// Exclude native events.. Unexpected.
		// Exclude delegate signatures.
		static const FName __UCSName(TEXT("UserConstructionScript"));
		if (__UCSName == Func->GetFName())
		{
			continue;
		}

		FunctionsToGenerate.Add(Func);
	}

	TArray<UMulticastDelegateProperty*> MCDelegateProperties;
	for (auto MCDelegateProp : TFieldRange<UMulticastDelegateProperty>(SourceClass, EFieldIteratorFlags::ExcludeSuper))
	{
		MCDelegateProperties.Add(MCDelegateProp);
	}

	const bool bGenerateAnyFunction = (0 != FunctionsToGenerate.Num()) || (0 != MCDelegateProperties.Num());

	// Include standard stuff
	EmitFileBeginning(FEmitHelper::GetBaseFilename(SourceClass), EmitterContext, bGenerateAnyFunction, true);

	// DELEGATES
	const FString DelegatesClassName = FString::Printf(TEXT("U__Delegates__%s"), *FEmitHelper::GetCppName(SourceClass));
	auto GenerateDelegateTypeName = [](UFunction* Func) -> FString
	{
		return FString::Printf(TEXT("F__Delegate__%s"), *FEmitHelper::GetCppName(Func));
	};
	auto GenerateMulticastDelegateTypeName = [](UMulticastDelegateProperty* MCDelegateProp) -> FString
	{
		return FString::Printf(TEXT("F__MulticastDelegate__%s"), *FEmitHelper::GetCppName(MCDelegateProp));
	};
	if (bGenerateAnyFunction)
	{
		EmitterContext.Header.AddLine(TEXT("UCLASS()"));
		EmitterContext.Header.AddLine(FString::Printf(TEXT("class %s : public UObject"), *DelegatesClassName));
		EmitterContext.Header.AddLine(TEXT("{"));
		EmitterContext.Header.AddLine(TEXT("public:"));
		EmitterContext.Header.IncreaseIndent();
		EmitterContext.Header.AddLine(TEXT("GENERATED_BODY()"));
		for (auto Func : FunctionsToGenerate)
		{
			FString ParamNumberStr, Parameters;
			FEmitHelper::ParseDelegateDetails(EmitterContext, Func, Parameters, ParamNumberStr);
			EmitterContext.Header.AddLine(FString::Printf(TEXT("DECLARE_DYNAMIC_DELEGATE%s(%s%s);"), *ParamNumberStr, *GenerateDelegateTypeName(Func), *Parameters));
		}
		for (auto MCDelegateProp : MCDelegateProperties)
		{
			FString ParamNumberStr, Parameters;
			FEmitHelper::ParseDelegateDetails(EmitterContext, MCDelegateProp->SignatureFunction, Parameters, ParamNumberStr);
			EmitterContext.Header.AddLine(FString::Printf(TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE%s(%s%s);"), *ParamNumberStr, *GenerateMulticastDelegateTypeName(MCDelegateProp), *Parameters));
		}
		EmitterContext.Header.DecreaseIndent();
		EmitterContext.Header.AddLine(TEXT("};"));
	}

	// Begin the struct
	const FString WrapperName = FString::Printf(TEXT("FUnconvertedWrapper__%s"), *FEmitHelper::GetCppName(SourceClass));

	FString ParentStruct;
	auto SuperBPGC = Cast<UBlueprintGeneratedClass>(SourceClass->GetSuperClass());
	if (SuperBPGC && !Dependencies.WillClassBeConverted(SuperBPGC))
	{
		//TODO: include header for super-class wrapper
		ParentStruct = FString::Printf(TEXT("FUnconvertedWrapper__%s"), *FEmitHelper::GetCppName(SourceClass->GetSuperClass()));
	}
	else
	{
		ParentStruct = FString::Printf(TEXT("FUnconvertedWrapper<%s>"), *FEmitHelper::GetCppName(SourceClass->GetSuperClass()));
	}
	
	EmitterContext.Header.AddLine(FString::Printf(TEXT("struct %s : public %s"), *WrapperName, *ParentStruct));
	EmitterContext.Header.AddLine(TEXT("{"));
	EmitterContext.Header.IncreaseIndent();

	// Constructor
	EmitterContext.Header.AddLine(FString::Printf(TEXT("%s(const %s* __InObject) : %s(__InObject){}"), *WrapperName, *FEmitHelper::GetCppName(EmitterContext.GetFirstNativeOrConvertedClass(SourceClass)), *ParentStruct));

	// PROPERTIES:
	for (auto Property : TFieldRange<UProperty>(SourceClass, EFieldIteratorFlags::ExcludeSuper))
	{
		//TODO: check if the property is really used?
		const FString TypeDeclaration = Property->IsA<UMulticastDelegateProperty>()
			? FString::Printf(TEXT("%s::%s"), *DelegatesClassName, *GenerateMulticastDelegateTypeName(CastChecked<UMulticastDelegateProperty>(Property)))
			: EmitterContext.ExportCppDeclaration(Property, EExportedDeclaration::Parameter, EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend | EPropertyExportCPPFlags::CPPF_NoRef, true);
		EmitterContext.Header.AddLine(FString::Printf(TEXT("%s& GetRef__%s()"), *TypeDeclaration, *UnicodeToCPPIdentifier(Property->GetName(), false, nullptr)));
		EmitterContext.Header.AddLine(TEXT("{"));
		EmitterContext.Header.IncreaseIndent();

		EmitterContext.Header.AddLine(TEXT("static TWeakObjectPtr<UProperty> __PropertyPtr{};"));
		EmitterContext.Header.AddLine(TEXT("const UProperty* __Property = __PropertyPtr.Get();"));
		EmitterContext.Header.AddLine(TEXT("if (nullptr == __Property)"));
		EmitterContext.Header.AddLine(TEXT("{"));
		EmitterContext.Header.IncreaseIndent();
		EmitterContext.Header.AddLine(FString::Printf(TEXT("__Property = GetClass()->FindPropertyByName(FName(TEXT(\"%s\")));"), *Property->GetName()));
		EmitterContext.Header.AddLine(TEXT("check(__Property);"));
		EmitterContext.Header.AddLine(TEXT("__PropertyPtr = __Property;"));
		EmitterContext.Header.DecreaseIndent();
		EmitterContext.Header.AddLine(TEXT("}"));

		EmitterContext.Header.AddLine(FString::Printf(TEXT("return *(__Property->ContainerPtrToValuePtr<%s>(__Object));"), *TypeDeclaration));
		EmitterContext.Header.DecreaseIndent();
		EmitterContext.Header.AddLine(TEXT("}"));
	}

	// FUNCTIONS:
	for (auto Func : FunctionsToGenerate)
	{
		{
			FString DelcareFunction = FString::Printf(TEXT("void %s("), *FEmitHelper::GetCppName(Func));
			bool bFirst = true;
			for (TFieldIterator<UProperty> It(Func); It; ++It)
			{
				UProperty* Property = *It;
				if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm))
				{
					continue;
				}

				if (!bFirst)
				{
					DelcareFunction += TEXT(", ");
				}
				else
				{
					bFirst = false;
				}

				if (Property->HasAnyPropertyFlags(CPF_OutParm))
				{
					DelcareFunction += TEXT("/*out*/ ");
				}

				DelcareFunction += EmitterContext.ExportCppDeclaration(Property, EExportedDeclaration::Parameter, EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend);
			}
			DelcareFunction += TEXT(")");
			EmitterContext.Header.AddLine(DelcareFunction);
		}

		EmitterContext.Header.AddLine(TEXT("{"));
		EmitterContext.Header.IncreaseIndent();

		EmitterContext.Header.AddLine(FString::Printf(TEXT("%s::%s __Delegate;"), *DelegatesClassName, *GenerateDelegateTypeName(Func)));
		EmitterContext.Header.AddLine(FString::Printf(TEXT("static const FName __FunctionName(TEXT(\"%s\"));"), *Func->GetName()));
		EmitterContext.Header.AddLine(TEXT("__Delegate.BindUFunction(__Object, __FunctionName);"));
		EmitterContext.Header.AddLine(TEXT("check(__Delegate.IsBound());"));

		{
			FString CallFunction(TEXT("__Delegate.Execute("));
			bool bFirst = true;
			for (TFieldIterator<UProperty> It(Func); It; ++It)
			{
				UProperty* Property = *It;
				if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm))
				{
					continue;
				}

				if (!bFirst)
				{
					CallFunction += TEXT(", ");
				}
				else
				{
					bFirst = false;
				}

				CallFunction += UnicodeToCPPIdentifier(Property->GetName(), Property->HasAnyPropertyFlags(CPF_Deprecated), TEXT("bpp__"));
			}
			CallFunction += TEXT(");");
			EmitterContext.Header.AddLine(CallFunction);
		}

		EmitterContext.Header.DecreaseIndent();
		EmitterContext.Header.AddLine(TEXT("}"));
	}

	// close struct
	EmitterContext.Header.DecreaseIndent();
	EmitterContext.Header.AddLine(TEXT("};"));

	return EmitterContext.Header.Result;
}

void FBlueprintCompilerCppBackendBase::EmitFileBeginning(const FString& CleanName, FEmitterLocalContext& EmitterContext, bool bIncludeGeneratedH, bool bIncludeCodeHelpersInHeader)
{
	EmitterContext.Header.AddLine(TEXT("#pragma once"));

	auto EmitIncludeHeader = [&](FCodeText& Dst, const TCHAR* Message, bool bAddDotH)
	{
		Dst.AddLine(FString::Printf(TEXT("#include \"%s%s\""), Message, bAddDotH ? TEXT(".h") : TEXT("")));
	};
	const FString PCHFilename = FEmitHelper::GetPCHFilename();
	if (!PCHFilename.IsEmpty())
	{
		EmitIncludeHeader(EmitterContext.Body, *PCHFilename, false);
	}
	/*
	const FString MainHeaderFilename = FEmitHelper::GetGameMainHeaderFilename();
	if (!MainHeaderFilename.IsEmpty() && MainHeaderFilename != PCHFilename)
	{
		EmitIncludeHeader(EmitterContext.Body, *MainHeaderFilename, false);
	}
	*/
	EmitIncludeHeader(EmitterContext.Body, *CleanName, true);
	EmitIncludeHeader(bIncludeCodeHelpersInHeader ? EmitterContext.Header : EmitterContext.Body, TEXT("GeneratedCodeHelpers"), true);
	EmitIncludeHeader(EmitterContext.Header, TEXT("Blueprint/BlueprintSupport"), true);

	FBackendHelperUMG::AdditionalHeaderIncludeForWidget(EmitterContext);
	EmitterContext.Header.AddLine(FBackendHelperAnim::AddHeaders(EmitterContext.GetCurrentlyGeneratedClass()));

	TSet<FString> AlreadyIncluded;
	AlreadyIncluded.Add(CleanName);
	auto EmitInner = [&](FCodeText& Dst, const TSet<UField*>& Src, const TSet<UField*>& Declarations)
	{
		auto EngineSourceDir = FPaths::EngineSourceDir();
		auto GameSourceDir = FPaths::GameSourceDir();

		for (UField* Field : Src)
		{
			check(Field);
			const bool bWantedType = Field->IsA<UBlueprintGeneratedClass>() || Field->IsA<UUserDefinedEnum>() || Field->IsA<UUserDefinedStruct>();

			// Wanted no-native type, that will be converted
			if (bWantedType)
			{
				// @TODO: Need to query if this asset will actually be converted

				const FString Name = Field->GetName();
				bool bAlreadyIncluded = false;
				AlreadyIncluded.Add(Name, &bAlreadyIncluded);
				if (!bAlreadyIncluded)
				{
					const FString GeneratedFilename = FEmitHelper::GetBaseFilename(Field);
					EmitIncludeHeader(Dst, *GeneratedFilename, true);
				}
			}
			// headers for native items
			else
			{
				FString PackPath;
				if (FSourceCodeNavigation::FindClassHeaderPath(Field, PackPath))
				{
					if (!PackPath.RemoveFromStart(EngineSourceDir))
					{
						if (!PackPath.RemoveFromStart(GameSourceDir))
						{
							PackPath = FPaths::GetCleanFilename(PackPath);
						}
					}
					bool bAlreadyIncluded = false;
					AlreadyIncluded.Add(PackPath, &bAlreadyIncluded);
					if (!bAlreadyIncluded)
					{
						EmitIncludeHeader(Dst, *PackPath, false);
					}
				}
			}
		}

		for (auto Type : Declarations)
		{
			if (auto ForwardDeclaredType = Cast<UClass>(Type))
			{
				Dst.AddLine(FString::Printf(TEXT("class %s;"), *FEmitHelper::GetCppName(ForwardDeclaredType)));
			}
		}
	};

	EmitInner(EmitterContext.Header, EmitterContext.Dependencies.IncludeInHeader, EmitterContext.Dependencies.DeclareInHeader);
	EmitInner(EmitterContext.Body, EmitterContext.Dependencies.IncludeInBody, TSet<UField*>());

	if (bIncludeGeneratedH)
	{
		EmitterContext.Header.AddLine(FString::Printf(TEXT("#include \"%s.generated.h\""), *CleanName));
	}
}

void FBlueprintCompilerCppBackendBase::CleanBackend()
{
	StateMapPerFunction.Empty();
	FunctionIndexMap.Empty();
}