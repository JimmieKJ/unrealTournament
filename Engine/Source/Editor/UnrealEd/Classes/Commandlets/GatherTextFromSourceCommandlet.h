// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once 
#include "Commandlets/Commandlet.h"
#include "GatherTextFromSourceCommandlet.generated.h"

/**
 *	UGatherTextFromSourceCommandlet: Localization commandlet that collects all text to be localized from the source code.
 */
UCLASS()
class UGatherTextFromSourceCommandlet : public UGatherTextCommandletBase
{
	GENERATED_UCLASS_BODY()

private:
#define LOC_DEFINE_REGION

	enum class EMacroBlockState : uint8
	{
		Normal,
		EditorOnly,
	};

	struct FSourceFileParseContext
	{
		bool AddManifestText( const FString& Token, const FString& Namespace, const FString& SourceText, const FManifestContext& Context );

		void PushMacroBlock( const FString& InBlockCtx );

		void PopMacroBlock();

		void FlushMacroStack();

		EMacroBlockState EvaluateMacroStack() const;

		void SetDefine( const FString& InDefineCtx );

		void RemoveDefine( const FString& InDefineCtx );

		//Working data
		FString Filename;
		int32 LineNumber;
		FString LineText;
		FString Namespace;
		bool ExcludedRegion;
		bool EndParsingCurrentLine;
		bool WithinBlockComment;
		bool WithinLineComment;
		bool WithinStringLiteral;
		bool WithinNamespaceDefine;
		FString WithinStartingLine;

		//Should editor-only data be included in this gather?
		bool ShouldGatherFromEditorOnlyData;

		//Destination location of the parsed FLocTextEntrys
		TSharedPtr< FLocTextHelper > GatherManifestHelper;

		FSourceFileParseContext()
			: Filename()
			, LineNumber(0)
			, LineText()
			, Namespace()
			, ExcludedRegion(false)
			, EndParsingCurrentLine(false)
			, WithinBlockComment(false)
			, WithinLineComment(false)
			, WithinStringLiteral(false)
			, WithinNamespaceDefine(false)
			, WithinStartingLine()
			, ShouldGatherFromEditorOnlyData(false)
			, GatherManifestHelper()
			, MacroBlockStack()
			, CachedMacroBlockState()
		{

		}

	private:
		//Working data
		TArray<FString> MacroBlockStack;
		mutable TOptional<EMacroBlockState> CachedMacroBlockState;
	};

	class FParsableDescriptor
	{
	public:
		FParsableDescriptor():bOverridesLongerTokens(false){}
		FParsableDescriptor(bool bOverride):bOverridesLongerTokens(bOverride){}
		virtual ~FParsableDescriptor(){}
		virtual const FString& GetToken() const = 0;
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const = 0;

		bool OverridesLongerTokens(){ return bOverridesLongerTokens; }
	protected:
		bool bOverridesLongerTokens;
	};

	class FPreProcessorDescriptor : public FParsableDescriptor
	{
	public:
		FPreProcessorDescriptor():FParsableDescriptor(true){}
	protected:
		static const FString DefineString;
		static const FString UndefString;
		static const FString IfString;
		static const FString IfDefString;
		static const FString ElIfString;
		static const FString ElseString;
		static const FString EndIfString;
		static const FString DefinedString;
		static const FString IniNamespaceString;
	};

	class FDefineDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const override { return FPreProcessorDescriptor::DefineString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FUndefDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const override { return FPreProcessorDescriptor::UndefString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FIfDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const override { return FPreProcessorDescriptor::IfString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FIfDefDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const override { return FPreProcessorDescriptor::IfDefString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FElIfDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const override { return FPreProcessorDescriptor::ElIfString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FElseDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const override { return FPreProcessorDescriptor::ElseString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FEndIfDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const override { return FPreProcessorDescriptor::EndIfString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FMacroDescriptor : public FParsableDescriptor
	{
	public:
		enum EMacroArgSemantic
		{
			MAS_Namespace,
			MAS_Identifier,
			MAS_SourceText,
		};

		struct FMacroArg
		{
			EMacroArgSemantic Semantic;
			bool IsAutoText;

			FMacroArg(EMacroArgSemantic InSema, bool InIsAutoText) : Semantic(InSema), IsAutoText(InIsAutoText) {}
		};

		static const FString TextMacroString;

		FMacroDescriptor(FString InName) : Name(InName) {}

		virtual const FString& GetToken() const override { return Name; }

	protected:
		bool ParseArgsFromMacro(const FString& Text, TArray<FString>& Args, FSourceFileParseContext& Context) const;

		static bool PrepareArgument(FString& Argument, bool IsAutoText, const FString& IdentForLogging, bool& OutHasQuotes);

	private:
		FString Name;
	};

	class FCommandMacroDescriptor : public FMacroDescriptor
	{
	public:
		FCommandMacroDescriptor() : FMacroDescriptor(TEXT("UI_COMMAND")) {}

		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	class FStringMacroDescriptor : public FMacroDescriptor
	{
	public:
		FStringMacroDescriptor(FString InName, FMacroArg Arg0, FMacroArg Arg1, FMacroArg Arg2) : FMacroDescriptor(InName)
		{
			Arguments.Add(Arg0);
			Arguments.Add(Arg1);
			Arguments.Add(Arg2);
		}

		FStringMacroDescriptor(FString InName, FMacroArg Arg0, FMacroArg Arg1) : FMacroDescriptor(InName)
		{
			Arguments.Add(Arg0);
			Arguments.Add(Arg1);
		}

		FStringMacroDescriptor(FString InName, FMacroArg Arg0) : FMacroDescriptor(InName)
		{
			Arguments.Add(Arg0);
		}

		virtual void TryParse(const FString& LineText, FSourceFileParseContext& Context) const override;

	private:
		TArray<FMacroArg> Arguments;
	};

	class FIniNamespaceDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const override { return FPreProcessorDescriptor::IniNamespaceString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const override;
	};

	static const FString ChangelistName;

	static FString UnescapeLiteralCharacterEscapeSequences(const FString& InString);
	static FString RemoveStringFromTextMacro(const FString& TextMacro, const FString& IdentForLogging, bool& Error);
	static FString StripCommentsFromToken(const FString& InToken, FSourceFileParseContext& Context);
	static bool ParseSourceText(const FString& Text, const TArray<FParsableDescriptor*>& Parsables, FSourceFileParseContext& ParseCtxt);

public:
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

#undef LOC_DEFINE_REGION
};
