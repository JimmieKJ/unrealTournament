// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HlslAST.cpp - Abstract Syntax Tree implementation for HLSL.
=============================================================================*/

#include "ShaderCompilerCommon.h"
#include "HlslLexer.h"
#include "HlslAST.h"

namespace CrossCompiler
{
	namespace AST
	{
		void DumpOptionalArraySize(bool bIsArray, const TLinearArray<FExpression*>& ArraySize)
		{
			if (bIsArray && ArraySize.Num() == 0)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("[]"));
			}
			else
			{
				for (const auto* Dimension : ArraySize)
				{
					FPlatformMisc::LowLevelOutputDebugString(TEXT("["));
					Dimension->Dump(0);
					FPlatformMisc::LowLevelOutputDebugString(TEXT("]"));
				}
			}
		}

#if 0
		FNode::FNode()/* :
			Prev(nullptr),
			Next(nullptr)*/
		{
		}
#endif // 0

		FNode::FNode(FLinearAllocator* Allocator, const FSourceInfo& InInfo) :
			SourceInfo(InInfo),
			Attributes(Allocator)/*,
			Prev(nullptr),
			Next(nullptr)*/
		{
		}

		void FNode::DumpIndent(int32 Indent)
		{
			while (--Indent >= 0)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("\t"));
			}
		}

		void FNode::DumpAttributes() const
		{
			if (Attributes.Num() > 0)
			{
				for (auto* Attr : Attributes)
				{
					Attr->Dump(0);
				}

				FPlatformMisc::LowLevelOutputDebugString(TEXT(" "));
			}
		}

		FExpression::FExpression(FLinearAllocator* InAllocator, EOperators InOperator, FExpression* E0, FExpression* E1, FExpression* E2, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Operator(InOperator),
			Identifier(nullptr),
			Expressions(InAllocator)
		{
			SubExpressions[0] = E0;
			SubExpressions[1] = E1;
			SubExpressions[2] = E2;
			TypeSpecifier = 0;
		}

		void FExpression::DumpOperator() const
		{
			switch (Operator)
			{
			case EOperators::Plus:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("+"));
				break;

			case EOperators::Neg:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("-"));
				break;

			case EOperators::Assign:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("="));
				break;

			case EOperators::AddAssign:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("+="));
				break;

			case EOperators::SubAssign:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("-="));
				break;

			case EOperators::MulAssign:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("*="));
				break;

			case EOperators::DivAssign:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("/="));
				break;

			case EOperators::ModAssign:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("%="));
				break;

			case EOperators::RSAssign:
				FPlatformMisc::LowLevelOutputDebugString(TEXT(">>="));
				break;

			case EOperators::LSAssign:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("<<="));
				break;

			case EOperators::AndAssign:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("&="));
				break;

			case EOperators::OrAssign:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("|="));
				break;

			case EOperators::XorAssign:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("^="));
				break;

			case EOperators::Conditional:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("?"));
				break;

			case EOperators::LogicOr:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("||"));
				break;

			case EOperators::LogicAnd:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("&&"));
				break;

			case EOperators::LogicNot:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("!"));
				break;

			case EOperators::BitOr:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("|"));
				break;

			case EOperators::BitXor:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("^"));
				break;

			case EOperators::BitAnd:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("&"));
				break;

			case EOperators::Equal:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("=="));
				break;

			case EOperators::NEqual:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("!="));
				break;

			case EOperators::Less:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("<"));
				break;

			case EOperators::Greater:
				FPlatformMisc::LowLevelOutputDebugString(TEXT(">"));
				break;

			case EOperators::LEqual:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("<="));
				break;

			case EOperators::GEqual:
				FPlatformMisc::LowLevelOutputDebugString(TEXT(">="));
				break;

			case EOperators::LShift:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("<<"));
				break;

			case EOperators::RShift:
				FPlatformMisc::LowLevelOutputDebugString(TEXT(">>"));
				break;

			case EOperators::Add:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("+"));
				break;

			case EOperators::Sub:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("-"));
				break;

			case EOperators::Mul:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("*"));
				break;

			case EOperators::Div:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("/"));
				break;

			case EOperators::Mod:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("%"));
				break;

			case EOperators::PreInc:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("++"));
				break;

			case EOperators::PreDec:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("--"));
				break;

			case EOperators::Identifier:
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s"), Identifier);
				break;

			case EOperators::UintConstant:
			case EOperators::BoolConstant:
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%d"), UintConstant);
				break;

			case EOperators::FloatConstant:
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%f"), FloatConstant);
				break;

			case EOperators::InitializerList:
				// Nothing...
				break;

			case EOperators::PostInc:
			case EOperators::PostDec:
			case EOperators::FieldSelection:
			case EOperators::ArrayIndex:
				break;

			case EOperators::TypeCast:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("("));
				TypeSpecifier->Dump(0);
				FPlatformMisc::LowLevelOutputDebugString(TEXT(")"));
				break;

			default:
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("*MISSING_%d*"), Operator);
				check(0);
				break;
			}
		}

		void FExpression::Dump(int32 Indent) const
		{
			switch (Operator)
			{
			case EOperators::Conditional:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("("));
				SubExpressions[0]->Dump(0);
				FPlatformMisc::LowLevelOutputDebugString(TEXT(" ? "));
				SubExpressions[1]->Dump(0);
				FPlatformMisc::LowLevelOutputDebugString(TEXT(" : "));
				SubExpressions[2]->Dump(0);
				FPlatformMisc::LowLevelOutputDebugString(TEXT(")"));
				break;

			default:
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("*MISSING_%d*"), Operator);
				check(0);
				break;
			}
		}

		FExpression::~FExpression()
		{
			for (auto* Expr : Expressions)
			{
				if (Expr)
				{
					delete Expr;
				}
			}

			for (int32 Index = 0; Index < 3; ++Index)
			{
				if (SubExpressions[Index])
				{
					delete SubExpressions[Index];
				}
				else
				{
					break;
				}
			}
		}

		FUnaryExpression::FUnaryExpression(FLinearAllocator* InAllocator, EOperators InOperator, FExpression* Expr, const FSourceInfo& InInfo) :
			FExpression(InAllocator, InOperator, Expr, nullptr, nullptr, InInfo)
		{
		}

		void FUnaryExpression::Dump(int32 Indent) const
		{
			DumpOperator();
			if (SubExpressions[0])
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("("));
				SubExpressions[0]->Dump(Indent);
				FPlatformMisc::LowLevelOutputDebugString(TEXT(")"));
			}

			// Suffix
			switch (Operator)
			{
			case EOperators::PostInc:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("++"));
				break;

			case EOperators::PostDec:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("--"));
				break;

			case EOperators::FieldSelection:
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT(".%s"), Identifier);
				break;

			default:
				break;
			}
		}

		FBinaryExpression::FBinaryExpression(FLinearAllocator* InAllocator, EOperators InOperator, FExpression* E0, FExpression* E1, const FSourceInfo& InInfo) :
			FExpression(InAllocator, InOperator, E0, E1, nullptr, InInfo)
		{
		}

		void FBinaryExpression::Dump(int32 Indent) const
		{
			switch (Operator)
			{
			case EOperators::ArrayIndex:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("("));
				SubExpressions[0]->Dump(Indent);
				FPlatformMisc::LowLevelOutputDebugString(TEXT(")"));
				FPlatformMisc::LowLevelOutputDebugString(TEXT("["));
				SubExpressions[1]->Dump(Indent);
				FPlatformMisc::LowLevelOutputDebugString(TEXT("]"));
				break;

			default:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("("));
				SubExpressions[0]->Dump(Indent);
				DumpOperator();
				SubExpressions[1]->Dump(Indent);
				FPlatformMisc::LowLevelOutputDebugString(TEXT(")"));
				break;
			}
		}

		FExpressionStatement::FExpressionStatement(FLinearAllocator* InAllocator, FExpression* InExpr, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Expression(InExpr)
		{
		}

		void FExpressionStatement::Dump(int32 Indent) const
		{
			DumpIndent(Indent);
			Expression->Dump(Indent);
			FPlatformMisc::LowLevelOutputDebugString(TEXT(";\n"));
		}

		FExpressionStatement::~FExpressionStatement()
		{
			if (Expression)
			{
				delete Expression;
			}
		}

		FCompoundStatement::FCompoundStatement(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Statements(InAllocator)
		{
		}

		void FCompoundStatement::Dump(int32 Indent) const
		{
			DumpIndent(Indent);
			FPlatformMisc::LowLevelOutputDebugString(TEXT("{\n"));
			for (auto* Statement : Statements)
			{
				Statement->Dump(Indent + 1);
			}
			DumpIndent(Indent);
			FPlatformMisc::LowLevelOutputDebugString(TEXT("}\n"));
		}

		FCompoundStatement::~FCompoundStatement()
		{
			for (auto* Statement : Statements)
			{
				if (Statement)
				{
					delete Statement;
				}
			}
		}

		FFunctionDefinition::FFunctionDefinition(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Prototype(nullptr),
			Body(nullptr)
		{
		}

		void FFunctionDefinition::Dump(int32 Indent) const
		{
			DumpAttributes();
			Prototype->Dump(0);
			if (Body)
			{
				Body->Dump(Indent);
			}
		}

		FFunctionDefinition::~FFunctionDefinition()
		{
			delete Prototype;
			delete Body;
		}

		FFunction::FFunction(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			ReturnType(nullptr),
			Identifier(nullptr),
			ReturnSemantic(nullptr),
			Parameters(InAllocator)
		{
		}

		void FFunction::Dump(int32 Indent) const
		{
			DumpAttributes();
			FPlatformMisc::LowLevelOutputDebugString(TEXT("\n"));
			ReturnType->Dump(0);
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT(" %s("), Identifier);
			bool bFirst = true;
			for (auto* Param : Parameters)
			{
				if (bFirst)
				{
					bFirst = false;
				}
				else
				{
					FPlatformMisc::LowLevelOutputDebugString(TEXT(", "));
				}
				Param->Dump(0);
			}

			if (ReturnSemantic && *ReturnSemantic)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT(": %s"), ReturnSemantic);
			}

			FPlatformMisc::LowLevelOutputDebugString(TEXT(")\n"));
		}

		FFunction::~FFunction()
		{
			for (auto* Param : Parameters)
			{
				delete Param;
			}
		}

		FJumpStatement::FJumpStatement(FLinearAllocator* InAllocator, EJumpType InType, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Type(InType),
			OptionalExpression(nullptr)
		{
		}

		void FJumpStatement::Dump(int32 Indent) const
		{
			DumpIndent(Indent);

			switch (Type)
			{
			case EJumpType::Return:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("return"));
				break;

			case EJumpType::Break:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("break"));
				break;

			case EJumpType::Continue:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("continue"));
				break;

			default:
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("*MISSING_%d*"), Type);
				check(0);
				break;
			}

			if (OptionalExpression)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT(" "));
				OptionalExpression->Dump(Indent);
			}
			FPlatformMisc::LowLevelOutputDebugString(TEXT(";\n"));
		}

		FJumpStatement::~FJumpStatement()
		{
			if (OptionalExpression)
			{
				delete OptionalExpression;
			}
		}

		FSelectionStatement::FSelectionStatement(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Condition(nullptr),
			ThenStatement(nullptr),
			ElseStatement(nullptr)
		{
		}

		void FSelectionStatement::Dump(int32 Indent) const
		{
			DumpIndent(Indent);
			DumpAttributes();
			FPlatformMisc::LowLevelOutputDebugString(TEXT("if ("));
			Condition->Dump(Indent);
			FPlatformMisc::LowLevelOutputDebugString(TEXT(")\n"));
			ThenStatement->Dump(Indent);
			if (ElseStatement)
			{
				DumpIndent(Indent);
				FPlatformMisc::LowLevelOutputDebugString(TEXT("else\n"));
				ElseStatement->Dump(Indent);
			}
		}

		FSelectionStatement::~FSelectionStatement()
		{
			delete Condition;
			delete ThenStatement;
			if (ElseStatement)
			{
				delete ElseStatement;
			}
		}

		FTypeSpecifier::FTypeSpecifier(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			TypeName(nullptr),
			InnerType(nullptr),
			Structure(nullptr),
			TextureMSNumSamples(1),
			PatchSize(0),
			bIsArray(false),
			//bIsUnsizedArray(false),
			ArraySize(nullptr)
		{
		}

		void FTypeSpecifier::Dump(int32 Indent) const
		{
			if (Structure)
			{
				Structure->Dump(Indent);
			}
			else
			{
				FPlatformMisc::LowLevelOutputDebugString(TypeName);
				if (TextureMSNumSamples > 1)
				{
					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("<%s, %d>"), InnerType, TextureMSNumSamples);
				}
				else if (InnerType && *InnerType)
				{
					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("<%s>"), InnerType);
				}
			}

			if (bIsArray)
			{
				printf("[ ");

				if (ArraySize)
				{
					ArraySize->Dump(Indent);
				}

				printf("]");
			}
		}

		FTypeSpecifier::~FTypeSpecifier()
		{
			if (Structure)
			{
				delete Structure;
			}

			if (ArraySize)
			{
				delete ArraySize;
			}
		}

		FCBufferDeclaration::FCBufferDeclaration(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Name(nullptr),
			Declarations(InAllocator)
		{
		}

		void FCBufferDeclaration::Dump(int32 Indent) const
		{
			DumpIndent(Indent);
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("cbuffer %s\n"), Name);

			DumpIndent(Indent);
			FPlatformMisc::LowLevelOutputDebugString(TEXT("{\n"));

			for (auto* Declaration : Declarations)
			{
				Declaration->Dump(Indent + 1);
			}

			DumpIndent(Indent);
			FPlatformMisc::LowLevelOutputDebugString(TEXT("}\n\n"));
		}

		FCBufferDeclaration::~FCBufferDeclaration()
		{
			for (auto* Decl : Declarations)
			{
				delete Decl;
			}
		}

		FTypeQualifier::FTypeQualifier()
		{
			Raw = 0;
		}

		void FTypeQualifier::Dump() const
		{
			if (bConstant)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("const "));
			}

			if (bIsStatic)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("static "));
			}

			if (bShared)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("groupshared "));
			}
			else if (bIn && bOut)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("inout "));
			}
			else if (bIn)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("in "));
			}
			else if (bOut)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("out "));
			}

			if (bLinear)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("linear "));
			}
			if (bCentroid)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("centroid "));
			}
			if (bNoInterpolation)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("nointerpolation "));
			}
			if (bNoPerspective)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("noperspective "));
			}
			if (bSample)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("sample "));
			}

			if (bRowMajor)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("row_major "));
			}
		}

		FFullySpecifiedType::FFullySpecifiedType(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Specifier(nullptr)
		{
		}

		void FFullySpecifiedType::Dump(int32 Indent) const
		{
			Qualifier.Dump();
			Specifier->Dump(Indent);
		}

		FFullySpecifiedType::~FFullySpecifiedType()
		{
			delete Specifier;
		}

		FDeclaration::FDeclaration(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Identifier(nullptr),
			Semantic(nullptr),
			bIsArray(false),
			ArraySize(InAllocator),
			Initializer(nullptr)
		{
		}

		void FDeclaration::Dump(int32 Indent) const
		{
			DumpAttributes();
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s"), Identifier);

			DumpOptionalArraySize(bIsArray, ArraySize);

			if (Initializer)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT(" = "));
				Initializer->Dump(Indent);
			}

			if (Semantic && *Semantic)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT(" : %s"), Semantic);
			}
		}

		FDeclaration::~FDeclaration()
		{
			for (auto* Expr : ArraySize)
			{
				delete Expr;
			}

			if (Initializer)
			{
				delete Initializer;
			}
		}

		FDeclaratorList::FDeclaratorList(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Type(nullptr),
			Declarations(InAllocator)
		{
		}

		void FDeclaratorList::Dump(int32 Indent) const
		{
			DumpIndent(Indent);
			DumpAttributes();
			if (Type)
			{
				Type->Dump(Indent);
			}

			bool bFirst = true;
			for (auto* Decl : Declarations)
			{
				if (bFirst)
				{
					FPlatformMisc::LowLevelOutputDebugString(TEXT(" "));
					bFirst = false;
				}
				else
				{
					FPlatformMisc::LowLevelOutputDebugString(TEXT(", "));
				}

				Decl->Dump(0);
			}

			FPlatformMisc::LowLevelOutputDebugString(TEXT(";\n"));
		}

		FDeclaratorList::~FDeclaratorList()
		{
			delete Type;

			for (auto* Decl : Declarations)
			{
				delete Decl;
			}
		}

		FInitializerListExpression::FInitializerListExpression(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FExpression(InAllocator, EOperators::InitializerList, nullptr, nullptr, nullptr, InInfo)
		{
		}

		void FInitializerListExpression::Dump(int32 Indent) const
		{
			FPlatformMisc::LowLevelOutputDebugString(TEXT("{"));
			bool bFirst = true;
			for (auto* Expr : Expressions)
			{
				if (bFirst)
				{
					bFirst = false;
				}
				else
				{
					FPlatformMisc::LowLevelOutputDebugString(TEXT(", "));
				}

				Expr->Dump(0);
			}
			FPlatformMisc::LowLevelOutputDebugString(TEXT("}"));
		}

		FParameterDeclarator::FParameterDeclarator(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Type(nullptr),
			bIsArray(false),
			ArraySize(InAllocator),
			DefaultValue(nullptr)
		{
		}

		void FParameterDeclarator::Dump(int32 Indent) const
		{
			DumpAttributes();
			Type->Dump(Indent);
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT(" %s"), Identifier);

			DumpOptionalArraySize(bIsArray, ArraySize);

			if (Semantic && *Semantic)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT(" : %s"), Semantic);
			}

			if (DefaultValue)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT(" = "));
				DefaultValue->Dump(Indent);
			}
		}

		FParameterDeclarator* FParameterDeclarator::CreateFromDeclaratorList(FDeclaratorList* List, FLinearAllocator* Allocator)
		{
			check(List);
			check(List->Declarations.Num() == 1);

			auto* Source = (FDeclaration*)List->Declarations[0];
			auto* New = new(Allocator) FParameterDeclarator(Allocator, Source->SourceInfo);
			New->Type = List->Type;
			New->Identifier = Source->Identifier;
			New->Semantic = Source->Semantic;
			New->bIsArray = Source->bIsArray;
			New->ArraySize = Source->ArraySize;
			New->DefaultValue = Source->Initializer;
			return New;
		}

		FParameterDeclarator::~FParameterDeclarator()
		{
			delete Type;

			for (auto* Expr : ArraySize)
			{
				delete Expr;
			}

			if (DefaultValue)
			{
				delete DefaultValue;
			}
		}

		FIterationStatement::FIterationStatement(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, EIterationType InType) :
			FNode(InAllocator, InInfo),
			Type(InType),
			InitStatement(nullptr),
			Condition(nullptr),
			RestExpression(nullptr),
			Body(nullptr)
		{
		}

		void FIterationStatement::Dump(int32 Indent) const
		{
			DumpIndent(Indent);
			DumpAttributes();
			switch (Type)
			{
			case EIterationType::For:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("for ("));
				if (InitStatement)
				{
					InitStatement->Dump(0);
					DumpIndent(Indent + 1);
				}
				else
				{
					FPlatformMisc::LowLevelOutputDebugString(TEXT("; "));
				}
				if (Condition)
				{
					Condition->Dump(0);
				}
				FPlatformMisc::LowLevelOutputDebugString(TEXT(";"));
				if (RestExpression)
				{
					RestExpression->Dump(0);
				}
				FPlatformMisc::LowLevelOutputDebugString(TEXT(")\n"));
				if (Body)
				{
					Body->Dump(Indent + 1);
				}
				else
				{
					DumpIndent(Indent);
					FPlatformMisc::LowLevelOutputDebugString(TEXT("{\n"));
					DumpIndent(Indent);
					FPlatformMisc::LowLevelOutputDebugString(TEXT("}\n"));
				}
				break;

			case EIterationType::While:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("while ("));
				Condition->Dump(0);
				FPlatformMisc::LowLevelOutputDebugString(TEXT(")\n"));
				DumpIndent(Indent);
				FPlatformMisc::LowLevelOutputDebugString(TEXT("{\n"));
				if (Body)
				{
					Body->Dump(Indent + 1);
				}
				DumpIndent(Indent);
				FPlatformMisc::LowLevelOutputDebugString(TEXT("}\n"));
				break;

			case EIterationType::DoWhile:
				FPlatformMisc::LowLevelOutputDebugString(TEXT("do\n"));
				DumpIndent(Indent);
				FPlatformMisc::LowLevelOutputDebugString(TEXT("{\n"));
				if (Body)
				{
					Body->Dump(Indent + 1);
				}
				DumpIndent(Indent);
				FPlatformMisc::LowLevelOutputDebugString(TEXT("}\n"));
				DumpIndent(Indent);
				FPlatformMisc::LowLevelOutputDebugString(TEXT("while ("));
				Condition->Dump(0);
				FPlatformMisc::LowLevelOutputDebugString(TEXT(");\n"));
				break;

			default:
				check(0);
				break;
			}
		}

		FIterationStatement::~FIterationStatement()
		{
			if (InitStatement)
			{
				delete InitStatement;
			}
			if (Condition)
			{
				delete Condition;
			}

			if (RestExpression)
			{
				delete RestExpression;
			}

			if (Body)
			{
				delete Body;
			}
		}

		FFunctionExpression::FFunctionExpression(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, FExpression* Callee) :
			FExpression(InAllocator, EOperators::FunctionCall, Callee, nullptr, nullptr, InInfo)
		{
		}

		void FFunctionExpression::Dump(int32 Indent) const
		{
			SubExpressions[0]->Dump(0);
			FPlatformMisc::LowLevelOutputDebugString(TEXT("("));
			bool bFirst = true;
			for (auto* Expr : Expressions)
			{
				if (bFirst)
				{
					bFirst = false;
				}
				else
				{
					FPlatformMisc::LowLevelOutputDebugString(TEXT(", "));
				}
				Expr->Dump(0);
			}
			FPlatformMisc::LowLevelOutputDebugString(TEXT(")"));
		}

		FSwitchStatement::FSwitchStatement(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, FExpression* InCondition, FSwitchBody* InBody) :
			FNode(InAllocator, InInfo),
			Condition(InCondition),
			Body(InBody)
		{
		}

		void FSwitchStatement::Dump(int32 Indent) const
		{
			DumpIndent(Indent);
			FPlatformMisc::LowLevelOutputDebugString(TEXT("switch ("));
			Condition->Dump(0);
			FPlatformMisc::LowLevelOutputDebugString(TEXT(")\n"));
			Body->Dump(Indent);
		}

		FSwitchStatement::~FSwitchStatement()
		{
			delete Condition;
			delete Body;
		}

		FSwitchBody::FSwitchBody(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			CaseList(nullptr)
		{
		}

		void FSwitchBody::Dump(int32 Indent) const
		{
			DumpIndent(Indent);
			FPlatformMisc::LowLevelOutputDebugString(TEXT("{\n"));
			CaseList->Dump(Indent + 1);
			DumpIndent(Indent);
			FPlatformMisc::LowLevelOutputDebugString(TEXT("}\n"));
		}

		FSwitchBody::~FSwitchBody()
		{
			delete CaseList;
		}

		FCaseLabel::FCaseLabel(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, AST::FExpression* InExpression) :
			FNode(InAllocator, InInfo),
			TestExpression(InExpression)
		{
		}

		void FCaseLabel::Dump(int32 Indent) const
		{
			DumpIndent(Indent);
			if (TestExpression)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("case "));
				TestExpression->Dump(0);
			}
			else
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("default"));
			}

			FPlatformMisc::LowLevelOutputDebugString(TEXT(":\n"));
		}

		FCaseLabel::~FCaseLabel()
		{
			if (TestExpression)
			{
				delete TestExpression;
			}
		}


		FCaseStatement::FCaseStatement(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, FCaseLabelList* InLabels) :
			FNode(InAllocator, InInfo),
			Labels(InLabels),
			Statements(InAllocator)
		{
		}

		void FCaseStatement::Dump(int32 Indent) const
		{
			Labels->Dump(Indent);

			if (Statements.Num() > 1)
			{
				DumpIndent(Indent);
				FPlatformMisc::LowLevelOutputDebugString(TEXT("{\n"));
				for (auto* Statement : Statements)
				{
					Statement->Dump(Indent + 1);
				}
				DumpIndent(Indent);
				FPlatformMisc::LowLevelOutputDebugString(TEXT("}\n"));
			}
			else if (Statements.Num() > 0)
			{
				Statements[0]->Dump(Indent + 1);
			}
		}

		FCaseStatement::~FCaseStatement()
		{
			delete Labels;
			for (auto* Statement : Statements)
			{
				if (Statement)
				{
					delete Statement;
				}
			}
		}

		FCaseLabelList::FCaseLabelList(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Labels(InAllocator)
		{
		}

		void FCaseLabelList::Dump(int32 Indent) const
		{
			for (auto* Label : Labels)
			{
				Label->Dump(Indent);
			}
		}

		FCaseLabelList::~FCaseLabelList()
		{
			for (auto* Label : Labels)
			{
				delete Label;
			}
		}

		FCaseStatementList::FCaseStatementList(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Cases(InAllocator)
		{
		}

		void FCaseStatementList::Dump(int32 Indent) const
		{
			for (auto* Case : Cases)
			{
				Case->Dump(Indent);
			}
		}

		FCaseStatementList::~FCaseStatementList()
		{
			for (auto* Case : Cases)
			{
				delete Case;
			}
		}

		FStructSpecifier::FStructSpecifier(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Name(nullptr),
			ParentName(nullptr),
			Declarations(InAllocator)
		{
		}

		void FStructSpecifier::Dump(int32 Indent) const
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("struct %s"), Name ? Name : TEXT(""));
			if (ParentName && *ParentName)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT(" : %s"), ParentName);
			}
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\n"));

			DumpIndent(Indent);
			FPlatformMisc::LowLevelOutputDebugString(TEXT("{\n"));

			for (auto* Decl : Declarations)
			{
				Decl->Dump(Indent + 1);
			}

			DumpIndent(Indent);
			FPlatformMisc::LowLevelOutputDebugString(TEXT("}"));
		}

		FStructSpecifier::~FStructSpecifier()
		{
			for (auto* Decl : Declarations)
			{
				delete Decl;
			}
		}

		FAttribute::FAttribute(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, const TCHAR* InName) :
			FNode(InAllocator, InInfo),
			Name(InName),
			Arguments(InAllocator)
		{
		}

		void FAttribute::Dump(int32 Indent) const
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[%s"), Name);

			bool bFirst = true;
			for (auto* Arg : Arguments)
			{
				if (bFirst)
				{
					FPlatformMisc::LowLevelOutputDebugString(TEXT("("));
					bFirst = false;
				}
				else
				{
					FPlatformMisc::LowLevelOutputDebugString(TEXT(", "));
				}

				Arg->Dump(0);
			}

			if (!bFirst)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT(")"));
			}

			FPlatformMisc::LowLevelOutputDebugString(TEXT("]"));
		}

		FAttribute::~FAttribute()
		{
			for (auto* Arg : Arguments)
			{
				delete Arg;
			}
		}

		FAttributeArgument::FAttributeArgument(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			StringArgument(nullptr),
			ExpressionArgument(nullptr)
		{
		}

		void FAttributeArgument::Dump(int32 Indent) const
		{
			if (ExpressionArgument)
			{
				ExpressionArgument->Dump(0);
			}
			else
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\"%s\""), StringArgument);
			}
		}

		FAttributeArgument::~FAttributeArgument()
		{
			if (ExpressionArgument)
			{
				delete ExpressionArgument;
			}
		}
	}
}
