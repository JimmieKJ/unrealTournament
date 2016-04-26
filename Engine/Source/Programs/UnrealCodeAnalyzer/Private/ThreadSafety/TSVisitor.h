// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "clang/AST/RecursiveASTVisitor.h"

namespace UnrealCodeAnalyzer
{
	class FTSVisitor : public clang::RecursiveASTVisitor<FTSVisitor>
	{
	public:
		explicit FTSVisitor(clang::ASTContext* InContext, clang::StringRef InFile);
		bool VisitCXXConstructorDecl(clang::CXXConstructorDecl* Decl);
		bool TraverseCXXMethodDecl(clang::CXXMethodDecl* Decl);
		bool VisitDeclRefExpr(clang::DeclRefExpr* Expr);
		bool VisitCallExpr(clang::CallExpr* Expr);

	private:
		bool InheritsFromUObject(clang::CXXConstructorDecl* Decl);
		bool IsGlobalVariable(clang::VarDecl* VarDecl);

		clang::SourceManager& SourceManager;
		TArray<clang::CXXMethodDecl*> FunctionDeclarationsStack;
		TSet<clang::Decl*> VisitedDecls;
	};
}
