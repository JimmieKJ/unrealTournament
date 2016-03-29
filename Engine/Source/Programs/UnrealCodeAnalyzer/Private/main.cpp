// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerPCH.h"
#include "Action.h"

#include "RequiredProgramMainCPPInclude.h"
#include "ThreadSafety/TSAction.h"

DEFINE_LOG_CATEGORY(LogUnrealCodeAnalyzer)

IMPLEMENT_APPLICATION(UnrealCodeAnalyzer, "UnrealCodeAnalyzer");

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

TMap<uint32, FString> GHashToFilename;
FString OutputFileName;
FStringOutputDevice OutputFileContents;

// Apply a custom category to all command-line options so that they are the only ones displayed.
static cl::OptionCategory MyToolCategory("UnrealCodeAnalyzer options");

static const float DefaultPCHIncludeThreshold = 0.3f;

const TCHAR* SkipToBeginningOfNextLine(const TCHAR* Buffer)
{
	// Find end of line marker
	auto Result = FCString::Strstr(Buffer, LINE_TERMINATOR);

	// If end of line not found, we reached end of file.
	if (!Result)
	{
		return TEXT("\0");
	}

	// Skip end of line marker
	for (int32 i = 0, e = FCString::Strlen(LINE_TERMINATOR); i < e; ++i)
	{
		++Result;
	}

	return Result;
}

bool StartsWith(const TCHAR* Buffer, const TCHAR* Prefix)
{
	return FCString::Strstr(Buffer, Prefix) == Buffer;
}

const TCHAR* SkipNonBreakingWhiteCharacters(const TCHAR* CurrentPosition)
{
	while (FChar::IsWhitespace(*CurrentPosition) && !FChar::IsLinebreak(*CurrentPosition))
	{
		++CurrentPosition;
	}

	return CurrentPosition;
}

FString GetAbsolutePath(const FString& RelativePath, const TArray<FString>& IncludeSearchPaths)
{
	FString FilenameAbsolutePath;

	for (const auto& IncludeSearchPath : IncludeSearchPaths)
	{
		auto PotentialFullPath = IncludeSearchPath + "/" + RelativePath;
		if (IPlatformFile::GetPlatformPhysical().FileExists(*PotentialFullPath))
		{
			FilenameAbsolutePath = PotentialFullPath;
			break;
		}
	}

	check(!FilenameAbsolutePath.IsEmpty());
	FilenameAbsolutePath.ReplaceInline(TEXT("\\"), TEXT("/"));

	return FilenameAbsolutePath;
}

FString CreateCommandLine(int32 argc, const char* argv[])
{
	FString CmdLine;

	for (int32 Arg = 0; Arg < argc; Arg++)
	{
		FString LocalArg = argv[Arg];
		if (LocalArg.Contains(TEXT(" ")))
		{
			CmdLine += TEXT("\"");
			CmdLine += LocalArg;
			CmdLine += TEXT("\"");
		}
		else
		{
			CmdLine += LocalArg;
		}

		if (Arg + 1 < argc)
		{
			CmdLine += TEXT(" ");
		}
	}

	return CmdLine;
}

float ParseUsageThreshold(const TCHAR* CmdLine)
{
	float UsageThreshold;

	if (!FParse::Value(CmdLine, TEXT("-UsageThreshold "), UsageThreshold))
	{
		UsageThreshold = DefaultPCHIncludeThreshold;
	}

	return UsageThreshold;
}

const TCHAR* GetPositionAfterSpace(const TCHAR* Buffer)
{
	const TCHAR* Result = FCString::Strstr(Buffer, TEXT(" "));
	return ++Result;
}

TArray<FString> ParseIncludeSearchPaths(const TCHAR* CmdLine)
{
	TArray<FString> IncludeSearchPaths;
	FString IncludeSearchPath;

	while (auto IncludeStart = FCString::Strstr(CmdLine, TEXT("/I")))
	{
		// /I <include path>
		// ^ IncludeStart is here
		FParse::Value(IncludeStart, TEXT("/I"), IncludeSearchPath);
		IncludeSearchPaths.Add(IncludeSearchPath);

		// Skip two TCHARs of /I.
		CmdLine = IncludeStart + 2;

		// Skip all possible spaces
		while (FChar::IsWhitespace(*CmdLine)) { ++CmdLine; }

		// Skip found path
		CmdLine += IncludeSearchPath.Len();

		// Skip all possible spaces
		while (FChar::IsWhitespace(*CmdLine)) { ++CmdLine; }
	}

	return IncludeSearchPaths;
}

int32 CreateIncludeFiles(int32 ArgC, const char** ArgV)
{
	LLVMInitializeX86TargetInfo();
	LLVMInitializeX86TargetMC();
	LLVMInitializeX86AsmParser();

	// First argument decides which control flow path we follow, second is output file. They're not relevant from clang's point of view.
	ArgC -= 2;
	ArgV[3] = ArgV[2];
	ArgV[2] = ArgV[0];
	CommonOptionsParser OptionsParser(ArgC, &(ArgV[2]), MyToolCategory);

	ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
	Tool.clearArgumentsAdjusters();

	return Tool.run(newFrontendActionFactory<UnrealCodeAnalyzer::FAction>().get());
}

void GatherModuleHeaderData(const TCHAR* IncludesDirectory, int32& FileCount, TMap<FString, int32>& IncludeHeaderCount)
{
	class FIncludeFileVisitor : public IPlatformFile::FDirectoryVisitor
	{
	public:
		FIncludeFileVisitor(int32& InFileCount, TMap<FString, int32>& InIncludeHeaderCount)
			: FileCount(InFileCount)
			, IncludeHeaderCount(InIncludeHeaderCount)
		{ }

	private:
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			auto Filename = FString(FilenameOrDirectory);
			if (!Filename.EndsWith(".cpp.includes"))
			{
				return true;
			}

			++FileCount;
			TSet<FString> IncludePaths;
			FString FileContents;
			TArray<FString> OutStrings;
			FFileHelper::LoadANSITextFileToStrings(*Filename, &IFileManager::Get(), OutStrings);
			IncludePaths.Reserve(OutStrings.Num());
			for (auto& IncludePath : OutStrings)
			{
				IncludePaths.Add(MoveTemp(IncludePath));
			}

			for (const auto& IncludePath : IncludePaths)
			{
				if (!IncludeHeaderCount.Contains(IncludePath))
				{
					IncludeHeaderCount.Add(IncludePath, 1);
				}
				else
				{
					++IncludeHeaderCount[IncludePath];
				}
			}
			return true;
		}
		int32& FileCount;
		TMap<FString, int32>& IncludeHeaderCount;
	};

	auto IncludeFileVisitor = FIncludeFileVisitor(FileCount, IncludeHeaderCount);
	IPlatformFile::GetPlatformPhysical().IterateDirectory(IncludesDirectory, IncludeFileVisitor);
}

const TCHAR* GetPositionAfterOpenAngleBracketOrParenthesis(const TCHAR* Buffer)
{
	auto Result = Buffer;
	auto OpenAngleBracket = TEXT("<");
	auto Parenthesis = TEXT("\"");

	while (*Result != *OpenAngleBracket && *Result != *Parenthesis) { ++Result; }

	return ++Result;
}

const TCHAR* GetPositionOfClosingAngleBracketOrParenthesis(const TCHAR* Buffer)
{
	auto Result = Buffer;
	auto OpenAngleBracket = TEXT(">");
	auto Parenthesis = TEXT("\"");

	while (*Result != *OpenAngleBracket && *Result != *Parenthesis) { ++Result; }

	return Result;
}

/** Parses one line from file returning absolute path of included header or empty string. Advances pointer to beginning of next line*/
FString ParseLine(const TCHAR*& CurrentPosition, const TArray<FString>& IncludeSearchPaths)
{
	const TCHAR* Prefix = TEXT("#include");

	// Skip all spaces
	CurrentPosition = SkipNonBreakingWhiteCharacters(CurrentPosition);

	if (!StartsWith(CurrentPosition, Prefix))
	{
		// If line doesn't start with prefix, jump to beginning of next line
		CurrentPosition = SkipToBeginningOfNextLine(CurrentPosition);
		return FString();
	}

	CurrentPosition = GetPositionAfterOpenAngleBracketOrParenthesis(CurrentPosition);
	auto IncludeBegin = CurrentPosition;
	auto IncludeEnd = GetPositionOfClosingAngleBracketOrParenthesis(CurrentPosition);

	auto Filename = FString(IncludeEnd - IncludeBegin, CurrentPosition);
	CurrentPosition = SkipToBeginningOfNextLine(CurrentPosition);

	return GetAbsolutePath(Filename, IncludeSearchPaths);
}

int32 GetIncludeCount(const TMap<FString, int32>& IncludeHeaderCount, const FString& AbsoluteFilePath)
{
	auto IncludeCountPtr = IncludeHeaderCount.Find(AbsoluteFilePath);
	return IncludeCountPtr ? *IncludeCountPtr : 0;
}

TArray<FString> GetIncludesInPCH(const FString& PCHFilePath, const TArray<FString>& IncludeSearchPaths)
{
	TArray<FString> IncludesInPCH;
	FString FileContents;
	FFileHelper::LoadFileToString(FileContents, *PCHFilePath);

	if (FileContents.IsEmpty())
	{
		return IncludesInPCH;
	}

	auto CurrentPosition = *FileContents;

	while (*CurrentPosition)
	{
		FString FilenameAbsolutePath = ParseLine(CurrentPosition, IncludeSearchPaths);
		if (FilenameAbsolutePath.IsEmpty())
		{
			continue;
		}
		IncludesInPCH.Add(FilenameAbsolutePath);
	}

	return IncludesInPCH;
}

void LogIncludeCount(const TArray<FString>& IncludesInPCH, const TMap<FString, int32>& IncludeHeaderCount, int32 FileCount, float IncludePercentageThreshold)
{
	for (const auto& FilenameAbsolutePath : IncludesInPCH)
	{
		auto IncludeCount = GetIncludeCount(IncludeHeaderCount, FilenameAbsolutePath);
		auto IncludePercentage = static_cast<float>(IncludeCount) / static_cast<float>(FileCount);
		if (IncludePercentage < IncludePercentageThreshold)
		{
			OutputFileContents.Logf(TEXT("%s %i(%f%%) with threshold %f%%"), *FilenameAbsolutePath, IncludeCount, IncludePercentage, IncludePercentageThreshold);
		}
	}
}

int AnalyzePCHFile(const FString& CmdLine)
{
	auto IncludePercentageThreshold = ParseUsageThreshold(*CmdLine);
	auto IncludeSearchPaths = ParseIncludeSearchPaths(*CmdLine);

	FString IncludesDirectory;
	FParse::Value(*CmdLine, TEXT("-HeaderDataPath="), IncludesDirectory);

	int32 FileCount = 0;
	TMap<FString, int32> IncludeHeaderCount;
	GatherModuleHeaderData(*IncludesDirectory, FileCount, IncludeHeaderCount);

	// Open PCH file
	FString PCHFilePath;
	FParse::Value(*CmdLine, TEXT("-PCHFile="), PCHFilePath);

	TArray<FString> IncludesInPCH = GetIncludesInPCH(PCHFilePath, IncludeSearchPaths);

	OutputFileContents.Logf(TEXT("Remove the following #includes from %s"), *PCHFilePath);
	LogIncludeCount(IncludesInPCH, IncludeHeaderCount, FileCount, IncludePercentageThreshold);


	OutputFileContents.Logf(TEXT("Add the following #includes to %s"), *PCHFilePath);
	for (auto KVP : IncludeHeaderCount)
	{
		const auto& Filename = KVP.Key;
		const auto& HeaderIncludeCount = KVP.Value;
		auto IncludePercentage = static_cast<float>(HeaderIncludeCount) / static_cast<float>(FileCount);
		if (IncludePercentage > IncludePercentageThreshold)
		{
			OutputFileContents.Logf(TEXT("%s (%i/%i)%f%%"), *Filename, HeaderIncludeCount, FileCount, IncludePercentage);
		}
	}

	return 0;
}

int32 CheckThreadSafety(int32 ArgC, const char** ArgV)
{
	LLVMInitializeX86TargetInfo();
	LLVMInitializeX86TargetMC();
	LLVMInitializeX86AsmParser();

	// First argument decides which control flow path we follow, second is output file. They're not relevant from clang's point of view.
	ArgC -= 2;
	ArgV[3] = ArgV[2];
	ArgV[2] = ArgV[0];
	CommonOptionsParser OptionsParser(ArgC, &(ArgV[2]), MyToolCategory);

	ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
	Tool.clearArgumentsAdjusters();

	return Tool.run(newFrontendActionFactory<UnrealCodeAnalyzer::FTSAction>().get());
}

//
// If analysis/clang compilation gives an error, we still can provide some meaningful data to user,
// which is why we return 0 in such cases. We return error only when -OutputFile is not provided and
// when saving output file fails.
//
int main(int32 ArgC, const char* ArgV[])
{
	FString CmdLine = CreateCommandLine(ArgC, ArgV);
	FString ShortCmdLine = FCommandLine::RemoveExeName(*CmdLine);
	ShortCmdLine = ShortCmdLine.Trim();

	GEngineLoop.PreInit(*ShortCmdLine);

	if (!FParse::Value(*CmdLine, TEXT("-OutputFile="), OutputFileName))
	{
		return 1;
	}

	OutputFileContents.SetAutoEmitLineTerminator(true);

	if (FParse::Param(*CmdLine, TEXT("AnalyzePCHFile")))
	{
		AnalyzePCHFile(CmdLine);
	}

	if (FParse::Param(*CmdLine, TEXT("CreateIncludeFiles")))
	{
		CreateIncludeFiles(ArgC, ArgV);
	}

	if (FParse::Param(*CmdLine, TEXT("CheckThreadSafety")))
	{
		CheckThreadSafety(ArgC, ArgV);
	}

	if (!FFileHelper::SaveStringToFile(OutputFileContents, *OutputFileName))
	{
		return 2;
	}

	return 0;
}
