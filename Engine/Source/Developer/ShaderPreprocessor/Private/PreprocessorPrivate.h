// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ShaderPreprocessor.h"
#include "mcpp.h"

/**
 * Filter MCPP errors.
 * @param ErrorMsg - The error message.
 * @returns true if the message is valid and has not been filtered out.
 */
inline bool FilterMcppError(const FString& ErrorMsg)
{
	const TCHAR* SubstringsToFilter[] =
	{
		TEXT("Unknown encoding:"),
		TEXT("with no newline, supplemented newline"),
		TEXT("Converted [CR+LF] to [LF]")
	};
	const int32 FilteredSubstringCount = ARRAY_COUNT(SubstringsToFilter);

	for (int32 SubstringIndex = 0; SubstringIndex < FilteredSubstringCount; ++SubstringIndex)
	{
		if (ErrorMsg.Contains(SubstringsToFilter[SubstringIndex]))
		{
			return false;
		}
	}
	return true;
}

/**
 * Parses MCPP error output.
 * @param ShaderOutput - Shader output to which to add errors.
 * @param McppErrors - MCPP error output.
 */
static bool ParseMcppErrors(TArray<FShaderCompilerError>& OutErrors, const FString& McppErrors, bool bConvertFilenameToRelative)
{
	bool bSuccess = true;
	if (McppErrors.Len() > 0)
	{
		TArray<FString> Lines;
		McppErrors.ParseIntoArray(Lines, TEXT("\n"), true);
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString& Line = Lines[LineIndex];
			int32 SepIndex1 = Line.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 2);
			int32 SepIndex2 = Line.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SepIndex1 + 1);
			if (SepIndex1 != INDEX_NONE && SepIndex2 != INDEX_NONE)
			{
				FString Filename = Line.Left(SepIndex1);
				FString LineNumStr = Line.Mid(SepIndex1 + 1, SepIndex2 - SepIndex1 - 1);
				FString Message = Line.Mid(SepIndex2 + 1, Line.Len() - SepIndex2 - 1);
				if (Filename.Len() && LineNumStr.Len() && LineNumStr.IsNumeric() && Message.Len())
				{
					while (++LineIndex < Lines.Num() && Lines[LineIndex].Len() && Lines[LineIndex].StartsWith(TEXT(" "),ESearchCase::CaseSensitive))
					{
						Message += FString(TEXT("\n")) + Lines[LineIndex];
					}
					--LineIndex;
					Message = Message.Trim().TrimTrailing();

					// Ignore the warning about files that don't end with a newline.
					if (FilterMcppError(Message))
					{
						FShaderCompilerError* CompilerError = new(OutErrors) FShaderCompilerError;
						CompilerError->ErrorFile = bConvertFilenameToRelative ? GetRelativeShaderFilename(Filename): Filename;
						CompilerError->ErrorLineString = LineNumStr;
						CompilerError->StrippedErrorMessage = Message;
						bSuccess = false;
					}
				}

			}
		}
	}
	return bSuccess;
}
