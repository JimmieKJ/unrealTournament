// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HlslAST.h - Abstract Syntax Tree interfaces for HLSL.
=============================================================================*/

#pragma once

#include "HlslLexer.h"

#define USE_UNREAL_ALLOCATOR 0

namespace CrossCompiler
{
	struct FLinearAllocator
	{
		static const int PageSize = 1024* 1024;

		FLinearAllocator()
		{
			auto* Initial = new FPage(PageSize);
			Pages.Add(Initial);
		}

		~FLinearAllocator()
		{
			for (auto* Page : Pages)
			{
				delete Page;
			}
		}

		inline void* Alloc(SIZE_T NumBytes)
		{
			check(NumBytes <= PageSize);
			auto* Page = Pages.Last();
			if (Page->Current + NumBytes > Page->End)
			{
				Page = new FPage(PageSize);
				Pages.Add(Page);
			}

			void* Ptr = Page->Current;
			Page->Current += NumBytes;
			return Ptr;
		}

		inline void* Alloc(SIZE_T NumBytes, SIZE_T Align)
		{
			void* Data = Alloc(NumBytes + Align - 1);
			UPTRINT Address = (UPTRINT)Data;
			Address += (Align - (Address % (UPTRINT)Align)) % Align;
			return (void*)Address;
		}

		inline TCHAR* Strdup(const TCHAR* String)
		{
			if (!String)
			{
				return nullptr;
			}

			auto* Data = (TCHAR*)Alloc(FCString::Strlen(String) + 1);
			FCString::Strcpy(Data, FCString::Strlen(String) + 1, String);
			return Data;
		}

		inline TCHAR* Strdup(const FString& String)
		{
			auto* Data = (TCHAR*)Alloc(String.Len() + 1);
			FCString::Strcpy(Data, String.Len() + 1, *String);
			return Data;
		}

		struct FPage
		{
			char* Current;
			char* Begin;
			char* End;

			FPage(SIZE_T Size)
			{
				Begin = new char[Size];
				End = Begin + Size;
				Current = Begin;
			}

			~FPage()
			{
				delete [] Begin;
			}
		};
		TArray<FPage*> Pages;
	};

#if !USE_UNREAL_ALLOCATOR
	class FLinearAllocatorPolicy
	{
	public:
		// Unreal allocator magic
		enum { NeedsElementType = false };
		enum { RequireRangeCheck = true };

		template<typename ElementType>
		class ForElementType
		{
		public:

			/** Default constructor. */
			ForElementType() :
				LinearAllocator(nullptr),
				Data(nullptr)
			{}

			// FContainerAllocatorInterface
			/*FORCEINLINE*/ ElementType* GetAllocation() const
			{
				return Data;
			}
			void ResizeAllocation(int32 PreviousNumElements, int32 NumElements, int32 NumBytesPerElement)
			{
				void* OldData = Data;
				if (NumElements)
				{
					// Allocate memory from the stack.
					Data = (ElementType*)LinearAllocator->Alloc(NumElements * NumBytesPerElement,
						FMath::Max((uint32)sizeof(void*), (uint32)ALIGNOF(ElementType))
						);

					// If the container previously held elements, copy them into the new allocation.
					if (OldData && PreviousNumElements)
					{
						const int32 NumCopiedElements = FMath::Min(NumElements, PreviousNumElements);
						FMemory::Memcpy(Data, OldData, NumCopiedElements * NumBytesPerElement);
					}
				}
			}
			int32 CalculateSlack(int32 NumElements, int32 NumAllocatedElements, int32 NumBytesPerElement) const
			{
				return DefaultCalculateSlack(NumElements, NumAllocatedElements, NumBytesPerElement);
			}

			int32 GetAllocatedSize(int32 NumAllocatedElements, int32 NumBytesPerElement) const
			{
				return NumAllocatedElements * NumBytesPerElement;
			}

			FLinearAllocator* LinearAllocator;

		private:

			/** A pointer to the container's elements. */
			ElementType* Data;
		};

		typedef ForElementType<FScriptContainerElement> ForAnyElementType;
	};

	class FLinearBitArrayAllocator
		: public TInlineAllocator<4, FLinearAllocatorPolicy>
	{
	};

	class FLinearSparseArrayAllocator
		: public TSparseArrayAllocator<FLinearAllocatorPolicy, FLinearBitArrayAllocator>
	{
	};

	class FLinearSetAllocator
		: public TSetAllocator<FLinearSparseArrayAllocator, TInlineAllocator<1, FLinearAllocatorPolicy> >
	{
	};

	template <typename TType>
	class TLinearArray : public TArray<TType, FLinearAllocatorPolicy>
	{
	public:
		TLinearArray(FLinearAllocator* Allocator)
		{
			TArray<TType, FLinearAllocatorPolicy>::AllocatorInstance.LinearAllocator = Allocator;
		}
	};

/*
	template <typename TType>
	struct TLinearSet : public TSet<typename TType, DefaultKeyFuncs<typename TType>, FLinearSetAllocator>
	{
		TLinearSet(FLinearAllocator* InAllocator)
		{
			Elements.AllocatorInstance.LinearAllocator = InAllocator;
		}
	};*/
#endif

	namespace AST
	{
		class FNode
		{
		public:
			FSourceInfo SourceInfo;

			TLinearArray<struct FAttribute*> Attributes;

			virtual void Dump(int32 Indent) const = 0;

			/* Callers of this ralloc-based new need not call delete. It's
			* easier to just ralloc_free 'ctx' (or any of its ancestors). */
			static void* operator new(SIZE_T Size, FLinearAllocator* Allocator)
			{
#if USE_UNREAL_ALLOCATOR
				return FMemory::Malloc(Size);
#else
				auto* Ptr = Allocator->Alloc(Size);
				return Ptr;
#endif
			}

			/* If the user *does* call delete, that's OK, we will just
			* ralloc_free in that case. */
			static void operator delete(void* Pointer)
			{
#if USE_UNREAL_ALLOCATOR
				FMemory::Free(Pointer);
#else
				// Do nothing...
#endif
			}

			virtual ~FNode() {}

		protected:
			//FNode();
			FNode(FLinearAllocator* Allocator, const FSourceInfo& InInfo);

			static void DumpIndent(int32 Indent);

			void DumpAttributes() const;
		};

		/**
		* Operators for AST expression nodes.
		*/
		enum class EOperators
		{
			Assign,
			Plus,        /**< Unary + operator. */
			Neg,
			Add,
			Sub,
			Mul,
			Div,
			Mod,
			LShift,
			RShift,
			Less,
			Greater,
			LEqual,
			GEqual,
			Equal,
			NEqual,
			BitAnd,
			BitXor,
			BitOr,
			BitNot,
			LogicAnd,
			LogicXor,
			LogicOr,
			LogicNot,

			MulAssign,
			DivAssign,
			ModAssign,
			AddAssign,
			SubAssign,
			LSAssign,
			RSAssign,
			AndAssign,
			XorAssign,
			OrAssign,

			Conditional,

			PreInc,
			PreDec,
			PostInc,
			PostDec,
			FieldSelection,
			ArrayIndex,

			FunctionCall,
			InitializerList,

			Identifier,
			//Int_constant,
			UintConstant,
			FloatConstant,
			BoolConstant,

			//Sequence,

			TypeCast,
		};

		inline EOperators TokenToASTOperator(EHlslToken Token)
		{
			switch (Token)
			{
			case EHlslToken::Equal:
				return AST::EOperators::Assign;

			case EHlslToken::PlusEqual:
				return AST::EOperators::AddAssign;

			case EHlslToken::MinusEqual:
				return AST::EOperators::SubAssign;

			case EHlslToken::TimesEqual:
				return AST::EOperators::MulAssign;

			case EHlslToken::DivEqual:
				return AST::EOperators::DivAssign;

			case EHlslToken::ModEqual:
				return AST::EOperators::ModAssign;

			case EHlslToken::GreaterGreaterEqual:
				return AST::EOperators::RSAssign;

			case EHlslToken::LowerLowerEqual:
				return AST::EOperators::LSAssign;

			case EHlslToken::AndEqual:
				return AST::EOperators::AndAssign;

			case EHlslToken::OrEqual:
				return AST::EOperators::OrAssign;

			case EHlslToken::XorEqual:
				return AST::EOperators::XorAssign;

			case EHlslToken::Question:
				return AST::EOperators::Conditional;

			case EHlslToken::OrOr:
				return AST::EOperators::LogicOr;

			case EHlslToken::AndAnd:
				return AST::EOperators::LogicAnd;

			case EHlslToken::Or:
				return AST::EOperators::BitOr;

			case EHlslToken::Xor:
				return AST::EOperators::BitXor;

			case EHlslToken::And:
				return AST::EOperators::BitAnd;

			case EHlslToken::EqualEqual:
				return AST::EOperators::Equal;

			case EHlslToken::NotEqual:
				return AST::EOperators::NEqual;

			case EHlslToken::Lower:
				return AST::EOperators::Less;

			case EHlslToken::Greater:
				return AST::EOperators::Greater;

			case EHlslToken::LowerEqual:
				return AST::EOperators::LEqual;

			case EHlslToken::GreaterEqual:
				return AST::EOperators::GEqual;

			case EHlslToken::LowerLower:
				return AST::EOperators::LShift;

			case EHlslToken::GreaterGreater:
				return AST::EOperators::RShift;

			case EHlslToken::Plus:
				return AST::EOperators::Add;

			case EHlslToken::Minus:
				return AST::EOperators::Sub;

			case EHlslToken::Times:
				return AST::EOperators::Mul;

			case EHlslToken::Div:
				return AST::EOperators::Div;

			case EHlslToken::Mod:
				return AST::EOperators::Mod;

			default:
				check(0);
				break;
			}

			return AST::EOperators::Plus;
		}

		struct FExpression : public FNode
		{
			FExpression(FLinearAllocator* InAllocator, EOperators InOperator, FExpression* E0, FExpression* E1, FExpression* E2, const FSourceInfo& InInfo);
			~FExpression();

			EOperators Operator;
			FExpression* SubExpressions[3];

			union
			{
				uint32 UintConstant;
				float FloatConstant;
				bool BoolConstant;
				struct FTypeSpecifier* TypeSpecifier;
			};
			const TCHAR* Identifier;

			TLinearArray<FExpression*> Expressions;

			void DumpOperator() const;
			virtual void Dump(int32 Indent) const;
		};

		struct FUnaryExpression : public FExpression
		{
			FUnaryExpression(FLinearAllocator* InAllocator, EOperators InOperator, FExpression* Expr, const FSourceInfo& InInfo);

			virtual void Dump(int32 Indent) const;
		};

		struct FBinaryExpression : public FExpression
		{
			FBinaryExpression(FLinearAllocator* InAllocator, EOperators InOperator, FExpression* E0, FExpression* E1, const FSourceInfo& InInfo);

			virtual void Dump(int32 Indent) const;
		};

		struct FFunctionExpression : public FExpression
		{
			FFunctionExpression(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, FExpression* Callee);

			virtual void Dump(int32 Indent) const;
		};

		struct FInitializerListExpression : public FExpression
		{
			FInitializerListExpression(FLinearAllocator* InAllocator, const FSourceInfo& InInfo);

			virtual void Dump(int32 Indent) const;
		};

		struct FCompoundStatement : public FNode
		{
			FCompoundStatement(FLinearAllocator* InAllocator, const FSourceInfo& InInfo);
			~FCompoundStatement();

			TLinearArray<FNode*> Statements;

			virtual void Dump(int32 Indent) const;
		};

		struct FDeclaration : public FNode
		{
			FDeclaration(FLinearAllocator* InAllocator, const FSourceInfo& InInfo);
			~FDeclaration();

			virtual void Dump(int32 Indent) const;

			const TCHAR* Identifier;

			const TCHAR* Semantic;

			bool bIsArray;
			//bool bIsUnsizedArray;

			TLinearArray<FExpression*> ArraySize;

			FExpression* Initializer;
		};

		struct FTypeQualifier
		{
			union
			{
				struct
				{
					uint32 bIsStatic : 1;
					uint32 bConstant : 1;
					uint32 bIn : 1;
					uint32 bOut : 1;
					uint32 bRowMajor : 1;
					uint32 bShared : 1;
				};
				uint32 Raw;
			};

			FTypeQualifier();

			void Dump() const;
		};

		struct FStructSpecifier : public FNode
		{
			FStructSpecifier(FLinearAllocator* InAllocator, const FSourceInfo& InInfo);
			~FStructSpecifier();

			virtual void Dump(int32 Indent) const;

			TCHAR* Name;
			TCHAR* ParentName;
			TLinearArray<FNode*> Declarations;
		};

		struct FCBufferDeclaration : public FNode
		{
			FCBufferDeclaration(FLinearAllocator* InAllocator, const FSourceInfo& InInfo);
			~FCBufferDeclaration();

			virtual void Dump(int32 Indent) const;

			const TCHAR* Name;
			TLinearArray<FNode*> Declarations;
		};

		struct FTypeSpecifier : public FNode
		{
			FTypeSpecifier(FLinearAllocator* InAllocator, const FSourceInfo& InInfo);
			~FTypeSpecifier();

			virtual void Dump(int32 Indent) const;

			const TCHAR* TypeName;
			const TCHAR* InnerType;

			FStructSpecifier* Structure;

			int32 TextureMSNumSamples;

			int32 PatchSize;

			bool bIsArray;
			//bool bIsUnsizedArray;
			FExpression* ArraySize;
		};

		struct FFullySpecifiedType : public FNode
		{
			FFullySpecifiedType(FLinearAllocator* InAllocator, const FSourceInfo& InInfo);
			~FFullySpecifiedType();

			virtual void Dump(int32 Indent) const;

			FTypeQualifier Qualifier;
			FTypeSpecifier* Specifier;
		};

		struct FDeclaratorList : public FNode
		{
			FDeclaratorList(FLinearAllocator* InAllocator, const FSourceInfo& InInfo);
			~FDeclaratorList();

			virtual void Dump(int32 Indent) const;

			FFullySpecifiedType* Type;
			TLinearArray<FNode*> Declarations;
		};

		struct FParameterDeclarator : public FNode
		{
			FParameterDeclarator(FLinearAllocator* InAllocator, const FSourceInfo& InInfo);
			~FParameterDeclarator();

			static FParameterDeclarator* CreateFromDeclaratorList(FDeclaratorList* List, FLinearAllocator* Allocator);

			virtual void Dump(int32 Indent) const;

			FFullySpecifiedType* Type;
			const TCHAR* Identifier;
			const TCHAR* Semantic;
			bool bIsArray;

			TLinearArray<FExpression*> ArraySize;
			FExpression* DefaultValue;
		};

		struct FFunction : public FNode
		{
			FFunction(FLinearAllocator* InAllocator, const FSourceInfo& InInfo);
			~FFunction();

			virtual void Dump(int32 Indent) const;

			FFullySpecifiedType* ReturnType;
			const TCHAR* Identifier;
			const TCHAR* ReturnSemantic;

			TLinearArray<FNode*> Parameters;

			bool bIsDefinition;

			//Signature
		};

		struct FExpressionStatement : public FNode
		{
			FExpressionStatement(FLinearAllocator* InAllocator, FExpression* InExpr, const FSourceInfo& InInfo);
			~FExpressionStatement();

			FExpression* Expression;

			virtual void Dump(int32 Indent) const;
		};

		struct FCaseLabel : public FNode
		{
			FCaseLabel(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, AST::FExpression* InExpression);
			~FCaseLabel();

			virtual void Dump(int32 Indent) const;

			FExpression* TestExpression;
		};

		struct FCaseLabelList : public FNode
		{
			FCaseLabelList(FLinearAllocator* InAllocator, const FSourceInfo& InInfo);
			~FCaseLabelList();

			virtual void Dump(int32 Indent) const;

			TLinearArray<FCaseLabel*> Labels;
		};

		struct FCaseStatement : public FNode
		{
			FCaseStatement(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, FCaseLabelList* InLabels);
			~FCaseStatement();

			virtual void Dump(int32 Indent) const;

			FCaseLabelList* Labels;
			TLinearArray<FNode*> Statements;
		};

		struct FCaseStatementList : public FNode
		{
			FCaseStatementList(FLinearAllocator* InAllocator, const FSourceInfo& InInfo);
			~FCaseStatementList();

			virtual void Dump(int32 Indent) const;

			TLinearArray<FCaseStatement*> Cases;
		};

		struct FSwitchBody : public FNode
		{
			FSwitchBody(FLinearAllocator* InAllocator, const FSourceInfo& InInfo);
			~FSwitchBody();

			virtual void Dump(int32 Indent) const;

			FCaseStatementList* CaseList;
		};

		struct FSelectionStatement : public FNode
		{
			FSelectionStatement(FLinearAllocator* InAllocator, const FSourceInfo& InInfo);
			~FSelectionStatement();

			virtual void Dump(int32 Indent) const;

			FExpression* Condition;

			FNode* ThenStatement;
			FNode* ElseStatement;
		};

		struct FSwitchStatement : public FNode
		{
			FSwitchStatement(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, FExpression* InCondition, FSwitchBody* InBody);
			~FSwitchStatement();

			virtual void Dump(int32 Indent) const;

			FExpression* Condition;
			FSwitchBody* Body;
		};

		enum class EIterationType
		{
			For,
			While,
			DoWhile,
		};

		struct FIterationStatement : public FNode
		{
			FIterationStatement(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, EIterationType InType);
			~FIterationStatement();

			virtual void Dump(int32 Indent) const;

			EIterationType Type;

			AST::FNode* InitStatement;
			AST::FNode* Condition;
			FExpression* RestExpression;

			AST::FNode* Body;
		};

		enum class EJumpType
		{
			Continue,
			Break,
			Return,
			//Discard,
		};

		struct FJumpStatement : public FNode
		{
			FJumpStatement(FLinearAllocator* InAllocator, EJumpType InType, const FSourceInfo& InInfo);
			~FJumpStatement();

			EJumpType Type;
			FExpression* OptionalExpression;

			virtual void Dump(int32 Indent) const;
		};

		struct FFunctionDefinition : public FNode
		{
			FFunctionDefinition(FLinearAllocator* InAllocator, const FSourceInfo& InInfo);
			~FFunctionDefinition();

			FFunction* Prototype;
			FCompoundStatement* Body;

			virtual void Dump(int32 Indent) const;
		};

		struct FAttributeArgument : public FNode
		{
			FAttributeArgument(FLinearAllocator* InAllocator, const FSourceInfo& InInfo);
			~FAttributeArgument();

			virtual void Dump(int32 Indent) const;

			const TCHAR* StringArgument;
			FExpression* ExpressionArgument;
		};

		struct FAttribute : public FNode
		{
			FAttribute(FLinearAllocator* Allocator, const FSourceInfo& InInfo, const TCHAR* InName);
			~FAttribute();

			virtual void Dump(int32 Indent) const;

			const TCHAR* Name;
			TLinearArray<FAttributeArgument*> Arguments;
		};
	}
}
