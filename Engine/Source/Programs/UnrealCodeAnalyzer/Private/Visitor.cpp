// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerPCH.h"
#include "Visitor.h"

namespace UnrealCodeAnalyzer
{
	FVisitor::FVisitor(clang::ASTContext* Context, clang::StringRef InFile) :
		SourceManager(Context->getSourceManager())
	{ }

	bool FVisitor::VisitDeclRefExpr(clang::DeclRefExpr* Expression)
	{
		// Any usage of a named declaration - basically everything that causes compile time dependency.
		auto Decl = Expression->getFoundDecl();
		auto UsageLocation = Expression->getLocation();
		ParseUsageOfDecl(Decl, UsageLocation);
		return true;
	}

	bool FVisitor::VisitCallExpr(clang::CallExpr* Expression)
	{
		auto Decl = Expression->getCalleeDecl();
		auto UsageLocation = Expression->getLocStart();
		ParseUsageOfDecl(Decl, UsageLocation);
		return true;
	}

	bool FVisitor::VisitCXXConstructExpr(clang::CXXConstructExpr* Expression)
	{
		auto Decl = Expression->getConstructor();
		auto UsageLocation = Expression->getLocStart();
		ParseUsageOfDecl(Decl, UsageLocation);
		return true;
	}

	bool FVisitor::VisitCXXDefaultInitExpr(clang::CXXDefaultInitExpr* Expression)
	{
		auto Decl = Expression->getField();
		auto UsageLocation = Expression->getLocStart();
		ParseUsageOfDecl(Decl, UsageLocation);
		return true;
	}

	bool FVisitor::VisitCXXDefaultArgExpr(clang::CXXDefaultArgExpr* Expression)
	{
		auto Decl = Expression->getParam();
		auto UsageLocation = Expression->getLocStart();
		ParseUsageOfDecl(Decl, UsageLocation);
		return true;
	}

	bool FVisitor::VisitCXXNewExpr(clang::CXXNewExpr* Expression)
	{
		auto Decl = Expression->getOperatorNew();
		auto UsageLocation = Expression->getLocStart();
		ParseUsageOfDecl(Decl, UsageLocation);
		Decl = Expression->getOperatorDelete();
		ParseUsageOfDecl(Decl, UsageLocation);
		return true;
	}

	bool FVisitor::VisitCXXDeleteExpr(clang::CXXDeleteExpr* Expression)
	{
		auto Decl = Expression->getOperatorDelete();
		auto UsageLocation = Expression->getLocStart();
		ParseUsageOfDecl(Decl, UsageLocation);
		return true;
	}

	bool FVisitor::VisitMemberExpr(clang::MemberExpr* Expression)
	{
		auto Decl = Expression->getMemberDecl();
		auto UsageLocation = Expression->getLocStart();
		ParseUsageOfDecl(Decl, UsageLocation);
		return true;
	}

	bool FVisitor::VisitSizeOfPackExpr(clang::SizeOfPackExpr* Expression)
	{
		auto Decl = Expression->getPack();
		auto UsageLocation = Expression->getLocStart();
		ParseUsageOfDecl(Decl, UsageLocation);
		return true;
	}

	bool FVisitor::VisitSubstNonTypeTemplateParmExpr(clang::SubstNonTypeTemplateParmExpr* Expression)
	{
		auto Decl = Expression->getParameter();
		auto UsageLocation = Expression->getLocStart();
		ParseUsageOfDecl(Decl, UsageLocation);
		return true;
	}

	bool FVisitor::VisitDecl(clang::Decl* Declaration)
	{
		// Any declaration with a name: label, namespace alias, namespace, template, type, various forms of "using", value.
		auto SourceRange = Declaration->getSourceRange();
		auto Begin = SourceRange.getBegin();
		auto DecomposedLoc = SourceManager.getDecomposedLoc(Begin);
		auto Line = SourceManager.getLineNumber(DecomposedLoc.first, DecomposedLoc.second);
		auto Col = SourceManager.getColumnNumber(DecomposedLoc.first, DecomposedLoc.second);
		auto Filename = SourceManager.getFilename(Begin);
		auto Data= Filename.data();
		DeclToFilename.Add(Declaration, Data);
		return true;
	}

	void FVisitor::ParseUsageOfDecl(clang::Decl* Declaration, const clang::SourceLocation& UsageLocation)
	{
		if (!DeclToFilename.Contains(Declaration))
		{
			// There are various cases when NamedDecl isn't added to NamedDeclToFilename
			// but we can safely ignore all of them, since they don't cause compile time dependency.
			return;
		}

		auto ExpressionLocation = UsageLocation;
		auto ExpressionFileId = SourceManager.getFileID(ExpressionLocation);
		auto ExpressionFilename = FString(SourceManager.getFilename(SourceManager.getLocForStartOfFile(ExpressionFileId)).data());
		ExpressionFilename.ReplaceInline(TEXT("\\"), TEXT("/"));
		auto DeclarationLocation = Declaration->getLocation();
		auto DeclarationFileId = SourceManager.getFileID(DeclarationLocation);
		if (DeclarationFileId == ExpressionFileId)
		{
			// Don't add compiled file as a required include.
			return;
		}


		auto DeclarationFilename = FString(SourceManager.getFilename(DeclarationLocation).data());
		DeclarationFilename.ReplaceInline(TEXT("\\"), TEXT("/"));


		auto DecomposedExpressionLocation = SourceManager.getDecomposedLoc(ExpressionLocation);
		auto Line = SourceManager.getLineNumber(DecomposedExpressionLocation.first, DecomposedExpressionLocation.second);
		auto Col = SourceManager.getColumnNumber(DecomposedExpressionLocation.first, DecomposedExpressionLocation.second);

		auto DeclarationFilenameHash = FIncludeSetMapKeyFuncs::GetKeyHash(DeclarationFilename);
		auto ExpressionFilenameHash = FIncludeSetMapKeyFuncs::GetKeyHash(ExpressionFilename);

		GHashToFilename.Add(DeclarationFilenameHash, DeclarationFilename);
		GHashToFilename.Add(ExpressionFilenameHash, ExpressionFilename);

		if (!Includes.Contains(ExpressionFilenameHash))
		{
			Includes.Add(ExpressionFilenameHash, FIncludeSet());
		}
		if (!Includes.Contains(DeclarationFilenameHash))
		{
			Includes.Add(DeclarationFilenameHash, FIncludeSet());
		}

		Includes[ExpressionFilenameHash].Add(DeclarationFilenameHash);
	}

	bool FVisitor::VisitCXXRecordDecl(clang::CXXRecordDecl* Declaration)
	{
		if (!Declaration->getDefinition())
		{
			return true;
		}

		for (auto BaseDecl : Declaration->bases())
		{
			if (auto TypePtr = BaseDecl.getType().getTypePtr())
			{
				if (auto BaseClassDecl = TypePtr->getAsCXXRecordDecl())
				{
					ParseUsageOfDecl(BaseClassDecl, Declaration->getSourceRange().getBegin());
				}
			}
		}

		return true;
	}

	bool FVisitor::VisitClassTemplateSpecializationDecl(clang::ClassTemplateSpecializationDecl* Declaration)
	{
		for (int32 Index = 0, Size = Declaration->getTemplateInstantiationArgs().size(); Index < Size; ++Index)
		{
			auto Argument = Declaration->getTemplateInstantiationArgs().get(Index);
			if (Argument.getKind() == clang::TemplateArgument::ArgKind::Type)
			{
				if (auto TypePtr = Argument.getAsType().getTypePtr())
				{
					if (auto RecordDecl = TypePtr->getAsCXXRecordDecl())
					{
						ParseUsageOfDecl(RecordDecl, Declaration->getLocStart());
					}
				}
			}
		}
		return true;
	}

	bool FVisitor::VisitFunctionTemplateDecl(clang::FunctionTemplateDecl* Declaration)
	{
		for (auto Spec : Declaration->specializations())
		{
			auto TemplateArguments = Spec->getTemplateSpecializationInfo()->TemplateArguments;

			for (int32 Index = 0, Size = TemplateArguments->size(); Index < Size; ++Index)
			{
				auto Argument = TemplateArguments->get(Index);
				if (Argument.getKind() == clang::TemplateArgument::ArgKind::Type)
				{
					if (auto TypePtr = Argument.getAsType().getTypePtr())
					{
						if (auto RecordDecl = TypePtr->getAsCXXRecordDecl())
						{
							ParseUsageOfDecl(RecordDecl, Declaration->getLocStart());
						}
					}
				}
			}
		}

		return true;
	}

	bool FVisitor::VisitVarTemplateDecl(clang::VarTemplateDecl* Declaration)
	{
		for (auto Spec : Declaration->specializations())
		{
			const auto& TemplateArguments = Spec->getTemplateArgs();

			for (int32 Index = 0, Size = TemplateArguments.size(); Index < Size; ++Index)
			{
				auto Argument = TemplateArguments.get(Index);
				if (Argument.getKind() == clang::TemplateArgument::ArgKind::Type)
				{
					if (auto TypePtr = Argument.getAsType().getTypePtr())
					{
						if (auto RecordDecl = TypePtr->getAsCXXRecordDecl())
						{
							ParseUsageOfDecl(RecordDecl, Declaration->getLocStart());
						}
					}
				}
			}
		}

		return true;
	}

}
