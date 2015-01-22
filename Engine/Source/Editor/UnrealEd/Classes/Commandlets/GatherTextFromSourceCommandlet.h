// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

	struct FSourceFileParseContext
	{
		bool AddManifestText( const FString& Token, const FString& Namespace, const FString& SourceText, const FContext& Context );

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

		//Destination location of the parsed FLocTextEntrys
		TSharedPtr< FManifestInfo > ManifestInfo;

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
		{

		}
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
		static const FString LocNamespaceString;
		static const FString LocDefRegionString;
		static const FString IniNamespaceString;
	};

	class FDefineDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const { return FPreProcessorDescriptor::DefineString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const;
	};

	class FUndefDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const { return FPreProcessorDescriptor::UndefString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const;
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

		virtual const FString& GetToken() const { return Name; }

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

		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const;
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

		virtual void TryParse(const FString& LineText, FSourceFileParseContext& Context) const;

	private:
		TArray<FMacroArg> Arguments;
	};

	class FIniNamespaceDescriptor : public FPreProcessorDescriptor
	{
	public:
		virtual const FString& GetToken() const { return FPreProcessorDescriptor::IniNamespaceString; }
		virtual void TryParse(const FString& Text, FSourceFileParseContext& Context) const;
	};

	static const FString ChangelistName;

	static FString RemoveStringFromTextMacro(const FString& TextMacro, const FString& IdentForLogging, bool& Error);
	static bool ParseSourceText(const FString& Text, const TArray<FParsableDescriptor*>& Parsables, FSourceFileParseContext& ParseCtxt);

public:
	// Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	// End UCommandlet Interface

#undef LOC_DEFINE_REGION
};
