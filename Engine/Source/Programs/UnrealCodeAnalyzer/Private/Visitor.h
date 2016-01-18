// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "clang/AST/RecursiveASTVisitor.h"

namespace UnrealCodeAnalyzer
{
	class FVisitor : public clang::RecursiveASTVisitor<FVisitor>
	{
	public:
		explicit FVisitor(clang::ASTContext* Context, clang::StringRef InFile);

		bool VisitCXXBaseSpecifier(clang::CXXBaseSpecifier* Specifier);
		bool VisitDeclRefExpr(clang::DeclRefExpr* Expression);
		bool VisitCallExpr(clang::CallExpr* Expression);
		bool VisitCXXConstructExpr(clang::CXXConstructExpr* Expression);
		bool VisitCXXDefaultInitExpr(clang::CXXDefaultInitExpr* Expression);
		bool VisitCXXDefaultArgExpr(clang::CXXDefaultArgExpr* Expression);
		bool VisitCXXNewExpr(clang::CXXNewExpr* Expression);
		bool VisitCXXDeleteExpr(clang::CXXDeleteExpr* Expression);
		bool VisitMemberExpr(clang::MemberExpr* Expression);
		bool VisitSizeOfPackExpr(clang::SizeOfPackExpr* Expression);
		bool VisitSubstNonTypeTemplateParmExpr(clang::SubstNonTypeTemplateParmExpr* Expression);

		bool VisitDecl(clang::Decl* Declaration);
		bool VisitCXXRecordDecl(clang::CXXRecordDecl* Declaration);
		bool shouldVisitImplicitCode() const { return true; }
		bool shouldVisitTemplateInstantiations() const { return true; }
		bool VisitClassTemplateSpecializationDecl(clang::ClassTemplateSpecializationDecl* Declaration);
		bool VisitFunctionTemplateDecl(clang::FunctionTemplateDecl* Declaration);
		bool VisitVarTemplateDecl(clang::VarTemplateDecl* Declaration);
		FPreHashedIncludeSetMap& GetIncludes() { return Includes; }

	private:
		void ParseUsageOfDecl(clang::Decl* Declaration, const clang::SourceLocation& UsageLocation);

		TMap<clang::Decl*, FString> DeclToFilename;
		FPreHashedIncludeSetMap Includes;
		clang::SourceManager& SourceManager;
	};
}
