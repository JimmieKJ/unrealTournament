// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HlslUtils.cpp - Utils for HLSL.
=============================================================================*/

#include "ShaderCompilerCommon.h"
#include "HlslUtils.h"
#include "HlslAST.h"
#include "HlslParser.h"

namespace CrossCompiler
{
	namespace Memory
	{
		static struct FPagePoolInstance
		{
			~FPagePoolInstance()
			{
				check(UsedPages.Num() == 0);
				for (int32 Index = 0; Index < FreePages.Num(); ++Index)
				{
					delete FreePages[Index];
				}
			}

			FPage* AllocatePage()
			{
				FScopeLock ScopeLock(&CriticalSection);

				if (FreePages.Num() == 0)
				{
					FreePages.Add(new FPage(PageSize));
				}

				auto* Page = FreePages.Last();
				FreePages.RemoveAt(FreePages.Num() - 1, 1, false);
				UsedPages.Add(Page);
				return Page;
			}

			void FreePage(FPage* Page)
			{
				FScopeLock ScopeLock(&CriticalSection);

				int32 Index = UsedPages.Find(Page);
				check(Index >= 0);
				UsedPages.RemoveAt(Index, 1, false);
				FreePages.Add(Page);
			}

			TArray<FPage*, TInlineAllocator<8> > FreePages;
			TArray<FPage*, TInlineAllocator<8> > UsedPages;

			FCriticalSection CriticalSection;

		} GMemoryPagePool;

		FPage* FPage::AllocatePage()
		{
#if USE_PAGE_POOLING
			return GMemoryPagePool.AllocatePage();
#else
			return new FPage(PageSize);
#endif
		}

		void FPage::FreePage(FPage* Page)
		{
#if USE_PAGE_POOLING
			GMemoryPagePool.FreePage(Page);
#else
			delete Page;
#endif
		}
	}
}


struct FRemoveUnusedOutputs
{
	const TArray<FString>& SystemOutputs;
	const TArray<FString>& UsedOutputs;
	FString EntryPoint;
	bool bSuccess;
	FString GeneratedCode;
	TArray<FString> Errors;
	CrossCompiler::FLinearAllocator* Allocator;
	CrossCompiler::FSourceInfo SourceInfo;

	TArray<FString> RemovedSemantics;

	FRemoveUnusedOutputs(const TArray<FString>& InSystemOutputs, const TArray<FString>& InUsedOutputs, CrossCompiler::FLinearAllocator* InAllocator = nullptr) :
		SystemOutputs(InSystemOutputs),
		UsedOutputs(InUsedOutputs),
		bSuccess(false),
		Allocator(InAllocator)
	{
	}

	static CrossCompiler::AST::FUnaryExpression* MakeIdentifierExpression(CrossCompiler::FLinearAllocator* Allocator, const TCHAR* Name, const CrossCompiler::FSourceInfo& SourceInfo)
	{
		using namespace CrossCompiler::AST;
		auto* Expression = new(Allocator) FUnaryExpression(Allocator, EOperators::Identifier, nullptr, SourceInfo);
		Expression->Identifier = Allocator->Strdup(Name);
		return Expression;
	};

	CrossCompiler::AST::FFunctionDefinition* FindEntryPointAndPopulateSymbolTable(CrossCompiler::TLinearArray<CrossCompiler::AST::FNode*>& ASTNodes, TArray<CrossCompiler::AST::FStructSpecifier*>& OutMiniSymbolTable, FString* OutOptionalWriteNodes)
	{
		using namespace CrossCompiler::AST;
		FFunctionDefinition* EntryFunction = nullptr;
		for (int32 Index = 0; Index < ASTNodes.Num(); ++Index)
		{
			auto* Node = ASTNodes[Index];
			if (FDeclaratorList* DeclaratorList = Node->AsDeclaratorList())
			{
				// Skip unnamed structures
				if (DeclaratorList->Type->Specifier->Structure && DeclaratorList->Type->Specifier->Structure->Name)
				{
					OutMiniSymbolTable.Add(DeclaratorList->Type->Specifier->Structure);
				}
			}
			else if (FFunctionDefinition* FunctionDefinition = Node->AsFunctionDefinition())
			{
				if (FCString::Strcmp(*EntryPoint, FunctionDefinition->Prototype->Identifier) == 0)
				{
					EntryFunction = FunctionDefinition;
				}
			}

			if (OutOptionalWriteNodes)
			{
				FASTWriter Writer(*OutOptionalWriteNodes);
				Node->Write(Writer);
			}
		}

		return EntryFunction;
	}

	struct FOriginalReturn
	{
		TArray<CrossCompiler::AST::FStructSpecifier*> NewStructs;
		CrossCompiler::AST::FStructSpecifier* NewReturnStruct;

		const TCHAR* ReturnVariableName;
		const TCHAR* ReturnTypeName;

		// Instructions before calling the original function
		TArray<CrossCompiler::AST::FNode*> PreInstructions;

		// Call to the original function
		CrossCompiler::AST::FFunctionExpression* CallToOriginalFunction;

		// Expression (might be assignment) calling CallToOriginalFunction
		CrossCompiler::AST::FExpression* CallExpression;

		// Instructions after calling the original function
		TArray<CrossCompiler::AST::FNode*> PostInstructions;

		// Final return instruction
		CrossCompiler::AST::FNode* FinalReturn;

		// Parameter of the new entry point
		TArray<CrossCompiler::AST::FParameterDeclarator*> NewFunctionParameters;

		FOriginalReturn() :
			NewReturnStruct(nullptr),
			ReturnVariableName(TEXT("OptimizedReturn")),
			ReturnTypeName(TEXT("FOptimizedReturn")),
			CallToOriginalFunction(nullptr),
			CallExpression(nullptr),
			FinalReturn(nullptr)
		{
		}
	};

	bool SetupReturnType(CrossCompiler::AST::FFunctionDefinition* EntryFunction, TArray<CrossCompiler::AST::FStructSpecifier*>& MiniSymbolTable, FOriginalReturn& OutReturn)
	{
		using namespace CrossCompiler::AST;

		// Create the new return type, local variable and the final return statement
		{
			// New return type
			OutReturn.NewReturnStruct = CreateNewReturnStruct(OutReturn.ReturnTypeName, OutReturn.NewStructs);

			// Local Variable
			OutReturn.PreInstructions.Add(CreateLocalVariable(OutReturn.NewReturnStruct->Name, OutReturn.ReturnVariableName));

			// Return Statement
			auto* ReturnStatement = new(Allocator) FJumpStatement(Allocator, EJumpType::Return, SourceInfo);
			ReturnStatement->OptionalExpression = MakeIdentifierExpression(Allocator, OutReturn.ReturnVariableName, SourceInfo);
			OutReturn.FinalReturn = ReturnStatement;
		}

		auto* OriginalReturnType = EntryFunction->Prototype->ReturnType;
		if (OriginalReturnType && OriginalReturnType->Specifier && OriginalReturnType->Specifier->TypeName)
		{
			const TCHAR* ReturnTypeName = OriginalReturnType->Specifier->TypeName;
			if (!EntryFunction->Prototype->ReturnSemantic && !FCString::Strcmp(ReturnTypeName, TEXT("void")))
			{
				return true;
			}
			else
			{
				// Confirm this is a struct living in the symbol table
				FStructSpecifier* OriginalStructSpecifier = FindStructSpecifier(MiniSymbolTable, ReturnTypeName);
				if (OriginalStructSpecifier)
				{
					return ProcessStructReturnType(OriginalStructSpecifier, MiniSymbolTable, OutReturn);
				}
				else if (CheckSimpleVectorType(ReturnTypeName))
				{
					if (EntryFunction->Prototype->ReturnSemantic)
					{
						ProcessSimpleReturnType(ReturnTypeName, EntryFunction->Prototype->ReturnSemantic, OutReturn);
						return true;
					}
					else
					{
						Errors.Add(FString::Printf(TEXT("RemoveUnused: Function %s with return type %s doesn't have a return semantic"), *EntryPoint, ReturnTypeName));
					}
				}
				else
				{
					Errors.Add(FString::Printf(TEXT("RemoveUnused: Invalid return type %s for function %s"), ReturnTypeName, *EntryPoint));
				}
			}
		}
		else
		{
			Errors.Add(FString::Printf(TEXT("RemoveUnused: Internal error trying to determine return type")));
		}

		return false;
	};

	CrossCompiler::AST::FStructSpecifier* FindStructSpecifier(TArray<CrossCompiler::AST::FStructSpecifier*>& MiniSymbolTable, const TCHAR* StructName)
	{
		for (auto* StructSpecifier : MiniSymbolTable)
		{
			if (!FCString::Strcmp(StructSpecifier->Name, StructName))
			{
				return StructSpecifier;
			}
		}

		return nullptr;
	}

	CrossCompiler::AST::FFunctionDefinition* CreateNewEntryFunction(CrossCompiler::AST::FCompoundStatement* Body, CrossCompiler::AST::FStructSpecifier* ReturnType, TArray<CrossCompiler::AST::FParameterDeclarator*>& Parameters)
	{
		using namespace CrossCompiler::AST;
		// New Entry definition/prototype
		FFunctionDefinition* NewEntryFunction = new(Allocator) FFunctionDefinition(Allocator, SourceInfo);
		NewEntryFunction->Prototype = new(Allocator) FFunction(Allocator, SourceInfo);
		NewEntryFunction->Prototype->Identifier = Allocator->Strdup(*(EntryPoint + TEXT("__OPTIMIZED")));
		NewEntryFunction->Prototype->ReturnType = new(Allocator) FFullySpecifiedType(Allocator, SourceInfo);
		NewEntryFunction->Prototype->ReturnType->Specifier = new(Allocator) FTypeSpecifier(Allocator, SourceInfo);
		NewEntryFunction->Prototype->ReturnType->Specifier->TypeName = ReturnType->Name;
		NewEntryFunction->Body = Body;
		for (auto* Parameter : Parameters)
		{
			NewEntryFunction->Prototype->Parameters.Add(Parameter);
		}

		return NewEntryFunction;
	}

	CrossCompiler::AST::FFullySpecifiedType* MakeSimpleType(const TCHAR* Name)
	{
		auto* ReturnType = new(Allocator) CrossCompiler::AST::FFullySpecifiedType(Allocator, SourceInfo);
		ReturnType->Specifier = new(Allocator) CrossCompiler::AST::FTypeSpecifier(Allocator, SourceInfo);
		ReturnType->Specifier->TypeName = Allocator->Strdup(Name);
		return ReturnType;
	};

	bool IsStringInArray(const TArray<FString>& Array, const TCHAR* Semantic)
	{
		for (const FString& String : Array)
		{
			if (FCString::Strcmp(*String, Semantic) == 0)
			{
				return true;
			}
		}

		return false;
	};

	void RemoveUnusedOutputs(CrossCompiler::TLinearArray<CrossCompiler::AST::FNode*>& ASTNodes)
	{
		using namespace CrossCompiler::AST;

		// Find Entry point from original AST nodes
		TArray<FStructSpecifier*> MiniSymbolTable;
		FString Test;
		FFunctionDefinition* EntryFunction = FindEntryPointAndPopulateSymbolTable(ASTNodes, MiniSymbolTable, &Test);
		if (!EntryFunction)
		{
			Errors.Add(FString::Printf(TEXT("RemoveUnused: Unable to find entry point %s"), *EntryPoint));
			bSuccess = false;
			return;
		}
		//FPlatformMisc::LowLevelOutputDebugString(*Test);

		SourceInfo = EntryFunction->SourceInfo;

		FOriginalReturn OriginalReturn;

		// Setup the call to the original entry point
		OriginalReturn.CallToOriginalFunction = new(Allocator) FFunctionExpression(Allocator, SourceInfo, MakeIdentifierExpression(Allocator, *EntryPoint, SourceInfo));

		if (!SetupReturnType(EntryFunction, MiniSymbolTable, OriginalReturn))
		{
			bSuccess = false;
			return;
		}

		if (!ProcessOriginalParameters(EntryFunction, MiniSymbolTable, OriginalReturn))
		{
			bSuccess = false;
			return;
		}

		// Real call statement
		if (OriginalReturn.CallToOriginalFunction && !OriginalReturn.CallExpression)
		{
			OriginalReturn.CallExpression = OriginalReturn.CallToOriginalFunction;
		}
		auto* CallInstruction = new(Allocator) CrossCompiler::AST::FExpressionStatement(Allocator, OriginalReturn.CallExpression, SourceInfo);

		FCompoundStatement* Body = AddStatementsToBody(OriginalReturn, CallInstruction);
		FFunctionDefinition* NewEntryFunction = CreateNewEntryFunction(Body, OriginalReturn.NewReturnStruct, OriginalReturn.NewFunctionParameters);

		WriteGeneratedCode(NewEntryFunction, OriginalReturn.NewStructs, GeneratedCode);
		bSuccess = true;
	}

	void WriteGeneratedCode(CrossCompiler::AST::FFunctionDefinition* NewEntryFunction, TArray<CrossCompiler::AST::FStructSpecifier*>& NewStructs, FString& OutGeneratedCode)
	{
		CrossCompiler::AST::FASTWriter Writer(OutGeneratedCode);
		GeneratedCode = TEXT("#line 1 \"RemoveUnusedOutputs.usf\"\n// Generated Entry Point: \n");
		if (RemovedSemantics.Num() > 0)
		{
			GeneratedCode = TEXT("// Removed Outputs: ");
			for (int32 Index = 0; Index < RemovedSemantics.Num(); ++Index)
			{
				GeneratedCode += RemovedSemantics[Index];
				GeneratedCode += TEXT(" ");
			}
			GeneratedCode += TEXT("\n");
		}
		for (auto* Struct : NewStructs)
		{
			auto* Declarator = new(Allocator) CrossCompiler::AST::FDeclaratorList(Allocator, SourceInfo);
			Declarator->Declarations.Add(Struct);
			Declarator->Write(Writer);
		}
		NewEntryFunction->Write(Writer);
		//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("*********************************\n%s\n"), *GeneratedCode);
	}

	bool CheckSimpleVectorType(const TCHAR* SimpleType)
	{
		if (!FCString::Strncmp(SimpleType, TEXT("float"), 5))
		{
			SimpleType += 5;
		}
		else if (!FCString::Strncmp(SimpleType, TEXT("int"), 3))
		{
			SimpleType += 3;
		}
		else if (!FCString::Strncmp(SimpleType, TEXT("half"), 4))
		{
			SimpleType += 4;
		}
		else
		{
			return false;
		}

		return FChar::IsDigit(SimpleType[0]) && SimpleType[1] == 0;
	}

	CrossCompiler::AST::FDeclaratorList* CreateLocalVariable(const TCHAR* Type, const TCHAR* Name)
	{
		using namespace CrossCompiler::AST;
		auto* LocalVarDeclaratorList = new(Allocator) FDeclaratorList(Allocator, SourceInfo);
		LocalVarDeclaratorList->Type = MakeSimpleType(Type);
		auto* LocalVarDeclaration = new(Allocator) FDeclaration(Allocator, SourceInfo);
		LocalVarDeclaration->Identifier = Allocator->Strdup(Name);
		LocalVarDeclaratorList->Declarations.Add(LocalVarDeclaration);
		return LocalVarDeclaratorList;
	}

	void ProcessSimpleOutParameter(CrossCompiler::AST::FParameterDeclarator* ParameterDeclarator, FOriginalReturn& OriginalReturn)
	{
		using namespace CrossCompiler::AST;

		// Only add the parameter if it needs to also be returned
		bool bRequiresToBeInReturnStruct = IsSemanticUsed(ParameterDeclarator->Semantic);

		if (bRequiresToBeInReturnStruct)
		{
			// Add the member to the return struct
			auto* MemberDeclaratorList = new(Allocator) FDeclaratorList(Allocator, SourceInfo);
			MemberDeclaratorList->Type = CloneType(ParameterDeclarator->Type);
			auto* MemberDeclaration = new(Allocator) FDeclaration(Allocator, SourceInfo);
			MemberDeclaration->Identifier = ParameterDeclarator->Identifier;
			MemberDeclaration->Semantic = ParameterDeclarator->Semantic;
			MemberDeclaratorList->Declarations.Add(MemberDeclaration);

			// Add it to the return struct type
			check(OriginalReturn.NewReturnStruct);
			OriginalReturn.NewReturnStruct->Declarations.Add(MemberDeclaratorList);

			// Add the parameter to the actual function call
			FString ParameterName = OriginalReturn.ReturnVariableName;
			ParameterName += TEXT(".");
			ParameterName += ParameterDeclarator->Identifier;
			auto* Parameter = MakeIdentifierExpression(Allocator, *ParameterName, SourceInfo);
			OriginalReturn.CallToOriginalFunction->Expressions.Add(Parameter);
		}
		else
		{
			// Make a local to receive the out parameter
			auto* LocalVar = CreateLocalVariable(ParameterDeclarator->Type->Specifier->TypeName, ParameterDeclarator->Identifier);
			OriginalReturn.PreInstructions.Add(LocalVar);

			// Add the parameter to the actual function call
			auto* Parameter = MakeIdentifierExpression(Allocator, ParameterDeclarator->Identifier, SourceInfo);
			OriginalReturn.CallToOriginalFunction->Expressions.Add(Parameter);
		}
	}

	void ProcessSimpleReturnType(const TCHAR* TypeName, const TCHAR* Semantic, FOriginalReturn& Return)
	{
		using namespace CrossCompiler::AST;

		// Create a member to return this simple type out
		auto* MemberDeclaratorList = new(Allocator) FDeclaratorList(Allocator, SourceInfo);
		MemberDeclaratorList->Type = MakeSimpleType(TypeName);
		auto* MemberDeclaration = new(Allocator) FDeclaration(Allocator, SourceInfo);
		MemberDeclaration->Identifier = TEXT("SimpleReturn");
		MemberDeclaration->Semantic = Semantic;
		MemberDeclaratorList->Declarations.Add(MemberDeclaration);

		// Add it to the return struct type
		check(Return.NewReturnStruct);
		Return.NewReturnStruct->Declarations.Add(MemberDeclaratorList);

		// Create the LHS of the member assignment
		FString MemberName = Return.ReturnVariableName;
		MemberName += TEXT(".");
		MemberName += MemberDeclaration->Identifier;
		auto* SimpleTypeMember = MakeIdentifierExpression(Allocator, *MemberName, SourceInfo);

		// Create an assignment from the call the original function
		check(Return.CallToOriginalFunction);
		Return.CallExpression = new(Allocator) FBinaryExpression(Allocator, EOperators::Assign, SimpleTypeMember, Return.CallToOriginalFunction, SourceInfo);
	}

	bool ProcessStructReturnType(CrossCompiler::AST::FStructSpecifier* StructSpecifier, TArray<CrossCompiler::AST::FStructSpecifier*>& MiniSymbolTable, FOriginalReturn& Return)
	{
		using namespace CrossCompiler::AST;

		// Add a local variable to receive the output from the function
		FString LocalStructVarName = TEXT("Local_");
		LocalStructVarName += StructSpecifier->Name;
		auto* LocalStructVariable = CreateLocalVariable(StructSpecifier->Name, *LocalStructVarName);
		Return.PreInstructions.Add(LocalStructVariable);

		// Create the LHS of the member assignment
		auto* SimpleTypeMember = MakeIdentifierExpression(Allocator, *LocalStructVarName, SourceInfo);

		// Create an assignment from the call the original function
		check(Return.CallToOriginalFunction);
		Return.CallExpression = new(Allocator) FBinaryExpression(Allocator, EOperators::Assign, SimpleTypeMember, Return.CallToOriginalFunction, SourceInfo);

		// Add all the members and the copies to the return struct
		return AddUsedMembers(Return.NewReturnStruct, Return.ReturnVariableName, StructSpecifier, *LocalStructVarName, MiniSymbolTable, Return);
	}

	bool ProcessStructOutParameter(CrossCompiler::AST::FParameterDeclarator* ParameterDeclarator, CrossCompiler::AST::FStructSpecifier* OriginalStructSpecifier, TArray<CrossCompiler::AST::FStructSpecifier*>& MiniSymbolTable, FOriginalReturn& OriginalReturn)
	{
		// Add a local variable to receive the output from the function
		FString LocalStructVarName = TEXT("Local_");
		LocalStructVarName += OriginalStructSpecifier->Name;
		LocalStructVarName += TEXT("_OUT");
		auto* LocalStructVariable = CreateLocalVariable(OriginalStructSpecifier->Name, *LocalStructVarName);
		OriginalReturn.PreInstructions.Add(LocalStructVariable);

		// Add the parameter to the actual function call
		auto* Parameter = MakeIdentifierExpression(Allocator, *LocalStructVarName, SourceInfo);
		OriginalReturn.CallToOriginalFunction->Expressions.Add(Parameter);

		return AddUsedMembers(OriginalReturn.NewReturnStruct, OriginalReturn.ReturnVariableName, OriginalStructSpecifier, *LocalStructVarName, MiniSymbolTable, OriginalReturn);
	}

	bool ProcessOriginalParameters(CrossCompiler::AST::FFunctionDefinition* EntryFunction, TArray<CrossCompiler::AST::FStructSpecifier*>& MiniSymbolTable, FOriginalReturn& OriginalReturn)
	{
		using namespace CrossCompiler::AST;
		for (FNode* ParamNode : EntryFunction->Prototype->Parameters)
		{
			FParameterDeclarator* ParameterDeclarator = ParamNode->AsParameterDeclarator();
			check(ParameterDeclarator);

			if (ParameterDeclarator->Type->Qualifier.bOut)
			{
				if (ParameterDeclarator->Semantic)
				{
					ProcessSimpleOutParameter(ParameterDeclarator, OriginalReturn);
				}
				else
				{
					// Confirm this is a struct living in the symbol table
					FStructSpecifier* OriginalStructSpecifier = FindStructSpecifier(MiniSymbolTable, ParameterDeclarator->Type->Specifier->TypeName);
					if (OriginalStructSpecifier)
					{
						if (!ProcessStructOutParameter(ParameterDeclarator, OriginalStructSpecifier, MiniSymbolTable, OriginalReturn))
						{
							return false;
						}
					}
					else if (CheckSimpleVectorType(ParameterDeclarator->Type->Specifier->TypeName))
					{
						Errors.Add(FString::Printf(TEXT("RemoveUnused: Function %s with out parameter %s doesn't have a return semantic"), *EntryPoint, ParameterDeclarator->Identifier));
						return false;
					}
					else
					{
						Errors.Add(FString::Printf(TEXT("RemoveUnused: Invalid return type %s for out parameter %s for function %s"), ParameterDeclarator->Type->Specifier->TypeName, ParameterDeclarator->Identifier, *EntryPoint));
						return false;
					}
				}
			}
			else
			{
				// Add this parameter as an input to the new function
				OriginalReturn.NewFunctionParameters.Add(ParameterDeclarator);

				// Add the parameter to the actual function call
				auto* Parameter = MakeIdentifierExpression(Allocator, ParameterDeclarator->Identifier, SourceInfo);
				OriginalReturn.CallToOriginalFunction->Expressions.Add(Parameter);
			}
		}

		return true;
	}

	CrossCompiler::AST::FStructSpecifier* CreateNewReturnStruct(const TCHAR* TypeName, TArray<CrossCompiler::AST::FStructSpecifier*>& NewStructs)
	{
		auto* NewReturnType = new(Allocator) CrossCompiler::AST::FStructSpecifier(Allocator, SourceInfo);
		NewReturnType->Name = Allocator->Strdup(TypeName);
		NewStructs.Add(NewReturnType);
		return NewReturnType;
	}

	CrossCompiler::AST::FCompoundStatement* AddStatementsToBody(FOriginalReturn& Return, CrossCompiler::AST::FNode* CallInstruction)
	{
		CrossCompiler::AST::FCompoundStatement* Body = new(Allocator) CrossCompiler::AST::FCompoundStatement(Allocator, SourceInfo);
		for (auto* Instruction : Return.PreInstructions)
		{
			Body->Statements.Add(Instruction);
		}

		check(CallInstruction);
		Body->Statements.Add(CallInstruction);

		for (auto* Instruction : Return.PostInstructions)
		{
			Body->Statements.Add(Instruction);
		}

		Body->Statements.Add(Return.FinalReturn);

		return Body;
	}


	bool GetArrayLength(CrossCompiler::AST::FDeclaration* A, uint32& OutLength)
	{
		using namespace CrossCompiler::AST;
		if (!A->bIsArray)
		{
			Errors.Add(FString::Printf(TEXT("%s is expected to be an array!"), A->Identifier));
			return false;
		}
		else
		{
			if (A->ArraySize.Num() > 1)
			{
				Errors.Add(FString::Printf(TEXT("No support for multidimensional arrays on %s!"), A->Identifier));
				return false;
			}

			for (int32 Index = 0; Index < A->ArraySize.Num(); ++Index)
			{
				int32 DimA = 0;
				if (!A->ArraySize[Index]->GetConstantIntValue(DimA))
				{
					Errors.Add(FString::Printf(TEXT("Array %s is not a compile-time constant expression!"), A->Identifier));
					return false;
				}

				OutLength = DimA;
			}
		}

		return true;
	};

	bool CopyMember(CrossCompiler::AST::FDeclaration* Declaration, const TCHAR* DestPrefix, const TCHAR* SourcePrefix, FOriginalReturn& Return)
	{
		using namespace CrossCompiler::AST;

		// Add copy statement(s)
		FString LHSName = DestPrefix;
		LHSName += '.';
		LHSName += Declaration->Identifier;
		FString RHSName = SourcePrefix;
		RHSName += '.';
		RHSName += Declaration->Identifier;

		if (Declaration->bIsArray)
		{
			uint32 ArrayLength = 0;
			if (!GetArrayLength(Declaration, ArrayLength))
			{
				return false;
			}

			for (uint32 Index = 0; Index < ArrayLength; ++Index)
			{
				FString LHSElement = FString::Printf(TEXT("%s[%d]"), *LHSName, Index);
				FString RHSElement = FString::Printf(TEXT("%s[%d]"), *RHSName, Index);
				auto* LHS = MakeIdentifierExpression(Allocator, *LHSElement, SourceInfo);
				auto* RHS = MakeIdentifierExpression(Allocator, *RHSElement, SourceInfo);
				auto* Assignment = new(Allocator) FBinaryExpression(Allocator, EOperators::Assign, LHS, RHS, SourceInfo);
				Return.PostInstructions.Add(new(Allocator) FExpressionStatement(Allocator, Assignment, SourceInfo));
			}
		}
		else
		{
			auto* LHS = MakeIdentifierExpression(Allocator, *LHSName, SourceInfo);
			auto* RHS = MakeIdentifierExpression(Allocator, *RHSName, SourceInfo);
			auto* Assignment = new(Allocator) FBinaryExpression(Allocator, EOperators::Assign, LHS, RHS, SourceInfo);
			Return.PostInstructions.Add(new(Allocator) FExpressionStatement(Allocator, Assignment, SourceInfo));
		}

		return true;
	}

	bool IsSemanticUsed(const TCHAR* Semantic)
	{
		//#todo-rco: For debugging...
		static bool bLeaveAllUsed = false;
		return bLeaveAllUsed || IsStringInArray(UsedOutputs, Semantic) || IsStringInArray(SystemOutputs, Semantic);
	};

	bool AddUsedMembers(CrossCompiler::AST::FStructSpecifier* DestStruct, const TCHAR* DestPrefix, CrossCompiler::AST::FStructSpecifier* SourceStruct, const TCHAR* SourcePrefix, TArray<CrossCompiler::AST::FStructSpecifier*>& MiniSymbolTable, FOriginalReturn& Return)
	{
		using namespace CrossCompiler::AST;

		for (auto* MemberNodes : SourceStruct->Declarations)
		{
			FDeclaratorList* MemberDeclarator = MemberNodes->AsDeclaratorList();
			check(MemberDeclarator);
			for (auto* DeclarationNode : MemberDeclarator->Declarations)
			{
				FDeclaration* MemberDeclaration = DeclarationNode->AsDeclaration();
				check(MemberDeclaration);
				if (MemberDeclaration->Semantic)
				{
					if (IsSemanticUsed(MemberDeclaration->Semantic))
					{
						// Add member to struct
						auto* NewDeclaratorList = new(Allocator) FDeclaratorList(Allocator, MemberDeclarator->SourceInfo);
						NewDeclaratorList->Type = CloneType(MemberDeclarator->Type);
						NewDeclaratorList->Declarations.Add(MemberDeclaration);
						DestStruct->Declarations.Add(NewDeclaratorList);

						CopyMember(MemberDeclaration, DestPrefix, SourcePrefix, Return);
					}
					else
					{
						RemovedSemantics.Add(MemberDeclaration->Semantic);
					}
				}
				else
				{
					if (!MemberDeclarator->Type || !MemberDeclarator->Type->Specifier || !MemberDeclarator->Type->Specifier->TypeName)
					{
						Errors.Add(FString::Printf(TEXT("RemoveUnused: Internal error tracking down nested type %s"), MemberDeclaration->Identifier));
						return false;
					}

					// No semantic, so make sure this is a nested struct, or error that it's missing a semantic
					FStructSpecifier* NestedStructSpecifier = FindStructSpecifier(MiniSymbolTable, MemberDeclarator->Type->Specifier->TypeName);
					if (!NestedStructSpecifier)
					{
						Errors.Add(FString::Printf(TEXT("RemoveUnused: Member (%s) %s is expected to have a semantic!"), MemberDeclarator->Type->Specifier->TypeName, MemberDeclaration->Identifier));
						return false;
					}

					// Add all the elements of this new struct into the return type
					FString NewSourcePrefix = SourcePrefix;
					NewSourcePrefix += TEXT(".");
					NewSourcePrefix += MemberDeclaration->Identifier;
					AddUsedMembers(DestStruct, DestPrefix, NestedStructSpecifier, *NewSourcePrefix, MiniSymbolTable, Return);
				}
			}
		}

		return true;
	}

	CrossCompiler::AST::FFullySpecifiedType* CloneType(CrossCompiler::AST::FFullySpecifiedType* InType, bool bStripInOut = true)
	{
		auto* New = new(Allocator) CrossCompiler::AST::FFullySpecifiedType(Allocator, SourceInfo);
		New->Qualifier = InType->Qualifier;
		if (bStripInOut)
		{
			New->Qualifier.bIn = false;
			New->Qualifier.bOut = false;
		}
		New->Specifier = InType->Specifier;
		return New;
	}
};

static void HlslParserCallbackWrapper(void* CallbackData, CrossCompiler::FLinearAllocator* Allocator, CrossCompiler::TLinearArray<CrossCompiler::AST::FNode*>& ASTNodes)
{
	auto* RemoveUnusedOutputsData = (FRemoveUnusedOutputs*)CallbackData;
	RemoveUnusedOutputsData->Allocator = Allocator;
	RemoveUnusedOutputsData->RemoveUnusedOutputs(ASTNodes);
	if (!RemoveUnusedOutputsData->bSuccess)
	{
		int i = 0;
		++i;
	}
}


bool RemoveUnusedOutputs(FString& InOutSourceCode, const TArray<FString>& SystemOutputs, const TArray<FString>& UsedOutputs, const FString& EntryPoint, TArray<FString>& OutErrors)
{
	FString DummyFilename(TEXT("RemoveUnusedOutputs.usf"));
	FRemoveUnusedOutputs Data(SystemOutputs, UsedOutputs);
	Data.EntryPoint = EntryPoint;
	CrossCompiler::FCompilerMessages Messages;
	if (!CrossCompiler::Parser::Parse(InOutSourceCode, DummyFilename, Messages, HlslParserCallbackWrapper, &Data))
	{
		Data.Errors.Add(FString(TEXT("HlslParser: Failed to compile!")));
		OutErrors = Data.Errors;
		for (auto& Message : Messages.MessageList)
		{
			OutErrors.Add(Message.Message);
		}
		return false;
	}

	for (auto& Message : Messages.MessageList)
	{
		OutErrors.Add(Message.Message);
	}

	if (Data.bSuccess)
	{
		InOutSourceCode += (TCHAR)'\n';
		InOutSourceCode += Data.GeneratedCode;

		return true;
	}

	OutErrors = Data.Errors;
	return false;
}
