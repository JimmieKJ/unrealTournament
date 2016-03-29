// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerPCH.h"
#include "Visitor.h"

namespace UnrealCodeAnalyzer
{
	FTSVisitor::FTSVisitor(clang::ASTContext* InContext, clang::StringRef InFile)
		: SourceManager(InContext->getSourceManager())
	{ }

	bool FTSVisitor::VisitCXXConstructorDecl(clang::CXXConstructorDecl* Decl)
	{
		if (!Decl->isThisDeclarationADefinition())
		{
			return true;
		}

		if (!InheritsFromUObject(Decl))
		{
			return true;
		}

		bool bVisited;
		VisitedDecls.Add(Decl, &bVisited);

		if (!bVisited)
		{
			FunctionDeclarationsStack.Add(Decl);
			RecursiveASTVisitor<FTSVisitor>::TraverseCXXConstructorDecl(Decl);
			FunctionDeclarationsStack.Pop(false);
		}

		return true;
	}

	bool FTSVisitor::VisitDeclRefExpr(clang::DeclRefExpr* Expr)
	{
		// Get declaration for the expression we found.
		clang::NamedDecl* NamedDecl = Expr->getFoundDecl();

		// If the expression is not a variable, we have nothing to do here.
		clang::VarDecl* VarDecl = llvm::dyn_cast<clang::VarDecl>(NamedDecl);

		if (VarDecl == nullptr)
		{
			return true;
		}

		if (IsGlobalVariable(VarDecl) && FunctionDeclarationsStack.Num() > 0)
		{
			clang::SourceLocation Begin = Expr->getExprLoc();
			std::pair<clang::FileID, unsigned> DecomposedLoc = SourceManager.getDecomposedLoc(Begin);
			int32 Line = SourceManager.getLineNumber(DecomposedLoc.first, DecomposedLoc.second);
			int32 Column = SourceManager.getColumnNumber(DecomposedLoc.first, DecomposedLoc.second);
			clang::StringRef Filename = SourceManager.getFilename(Begin);
			const char* Data = Filename.data();
			if (Line == 1 && Column == 1 && Data == '\0')
			{
				clang::SourceLocation Begin = FunctionDeclarationsStack.Last()->getLocation();
				std::pair<clang::FileID, unsigned> DecomposedLoc = SourceManager.getDecomposedLoc(Begin);
				int32 Line = SourceManager.getLineNumber(DecomposedLoc.first, DecomposedLoc.second);
				int32 Column = SourceManager.getColumnNumber(DecomposedLoc.first, DecomposedLoc.second);
				clang::StringRef Filename = SourceManager.getFilename(Begin);
				const char* Data = Filename.data();
				OutputFileContents.Logf(TEXT("[FuncLoc]%s:%d:%d Found usage of %s in function %s::%s"), *FString(Data), Line, Column, *FString(VarDecl->getName().data()), ANSI_TO_TCHAR(FunctionDeclarationsStack.Last()->getParent()->getNameAsString().c_str()), *FString(FunctionDeclarationsStack.Last()->getNameAsString().c_str()));
			}
			else
			{
				OutputFileContents.Logf(TEXT("[ExprLoc]%s:%d:%d Found usage of %s in function %s::%s"), *FString(Data), Line, Column, *FString(VarDecl->getName().data()), ANSI_TO_TCHAR(FunctionDeclarationsStack.Last()->getParent()->getNameAsString().c_str()), *FString(FunctionDeclarationsStack.Last()->getNameAsString().c_str()));
			}
		}
		return true;
	}

	bool FTSVisitor::TraverseCXXMethodDecl(clang::CXXMethodDecl* Decl)
	{
		if (!Decl->isThisDeclarationADefinition())
		{
			return true;
		}
		if ((strcmp(Decl->getName().data(), "PostInitProperties") == 0)
			|| (strcmp(Decl->getName().data(), "Serialize") == 0))
		{
			FunctionDeclarationsStack.Add(Decl);
			RecursiveASTVisitor<FTSVisitor>::TraverseFunctionDecl(Decl);
			FunctionDeclarationsStack.Pop(false);
		}

		return true;
	}

	bool FTSVisitor::InheritsFromUObject(clang::CXXConstructorDecl* Decl)
	{
		const clang::CXXRecordDecl* ParentClass = Decl->getParent();
		return ParentClass->getNameAsString()[0] == 'U';
	}

	bool FTSVisitor::IsGlobalVariable(clang::VarDecl* VarDecl)
	{
		// Get context of declaration.
		clang::DeclContext* DeclContext = VarDecl->getDeclContext();

		// Globals are declared in the scope of translation unit
		if (DeclContext->isTranslationUnit()
			// Or are static data members
			|| VarDecl->isStaticDataMember())
		{
			return true;
		}

		if (DeclContext->isNamespace())
		{
			// Go to outer contexts until there is none or outer is translation unit.
			clang::DeclContext* OuterContext = DeclContext->getLookupParent();

			while (OuterContext != nullptr)
			{
				if (OuterContext->isTranslationUnit())
				{
					return true;
				}

				OuterContext = OuterContext->getLookupParent();
			}
		}

		return false;
	}

	bool FTSVisitor::VisitCallExpr(clang::CallExpr* Expr)
	{
		bool bIsGlobal = false;
		clang::FunctionDecl* Decl = dyn_cast_or_null<clang::FunctionDecl>(Expr->getCalleeDecl());
		if (Decl)
		{
			if (Decl->isGlobal())
			{
				bIsGlobal = true;
			}
			else if (clang::CXXMethodDecl* MethodDecl = dyn_cast_or_null<clang::CXXMethodDecl>(Decl))
			{
				if (MethodDecl->isStatic())
				{
					bIsGlobal = true;
				}
			}
		}

		if (!bIsGlobal || FunctionDeclarationsStack.Num() == 0)
		{
			return true;
		}

		FString FunctionName;
		if (clang::CXXMethodDecl* MethodDecl = dyn_cast_or_null<clang::CXXMethodDecl>(Decl))
		{
			FunctionName = FString::Printf(TEXT("%s::%s"), ANSI_TO_TCHAR(MethodDecl->getParent()->getNameAsString().c_str()), ANSI_TO_TCHAR(Decl->getNameAsString().c_str()));
		}
		else
		{
			FunctionName = FString(ANSI_TO_TCHAR(Decl->getNameAsString().c_str()));
		}

		clang::SourceLocation Begin = Expr->getExprLoc();
		std::pair<clang::FileID, unsigned> DecomposedLoc = SourceManager.getDecomposedLoc(Begin);
		int32 Line = SourceManager.getLineNumber(DecomposedLoc.first, DecomposedLoc.second);
		int32 Column = SourceManager.getColumnNumber(DecomposedLoc.first, DecomposedLoc.second);
		clang::StringRef Filename = SourceManager.getFilename(Begin);
		const char* Data = Filename.data();
		if (Line == 1 && Column == 1 && Data == '\0')
		{
			clang::SourceLocation Begin = FunctionDeclarationsStack.Last()->getLocation();
			std::pair<clang::FileID, unsigned> DecomposedLoc = SourceManager.getDecomposedLoc(Begin);
			int32 Line = SourceManager.getLineNumber(DecomposedLoc.first, DecomposedLoc.second);
			int32 Column = SourceManager.getColumnNumber(DecomposedLoc.first, DecomposedLoc.second);
			clang::StringRef Filename = SourceManager.getFilename(Begin);
			const char* Data = Filename.data();
			OutputFileContents.Logf(TEXT("[FuncLoc]%s:%d:%d Found usage of %s in function %s::%s"), *FString(Data), Line, Column, *FunctionName, ANSI_TO_TCHAR(FunctionDeclarationsStack.Last()->getParent()->getNameAsString().c_str()), *FString(FunctionDeclarationsStack.Last()->getNameAsString().c_str()));
		}
		else
		{
			OutputFileContents.Logf(TEXT("[ExprLoc]%s:%d:%d Found usage of %s in function %s::%s"), *FString(Data), Line, Column, *FunctionName, ANSI_TO_TCHAR(FunctionDeclarationsStack.Last()->getParent()->getNameAsString().c_str()), *FString(FunctionDeclarationsStack.Last()->getNameAsString().c_str()));
		}

		return true;
	}

}
