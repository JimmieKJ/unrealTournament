// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ShaderPreprocessorPrivatePCH.h"
#include "ShaderPreprocessor.h"
#include "ModuleManager.h"
#include "PreprocessorPrivate.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, ShaderPreprocessor);

/**
 * Append defines to an MCPP command line.
 * @param OutOptions - Upon return contains MCPP command line parameters as a string appended to the current string.
 * @param Definitions - Definitions to add.
 */
static void AddMcppDefines(FString& OutOptions, const TMap<FString,FString>& Definitions)
{
	for (TMap<FString,FString>::TConstIterator It(Definitions); It; ++It)
	{
		OutOptions += FString::Printf(TEXT(" -D%s=%s"), *(It.Key()), *(It.Value()));
	}
}

/**
 * Helper class used to load shader source files for MCPP.
 */
class FMcppFileLoader
{
public:
	/** Initialization constructor. */
	explicit FMcppFileLoader(const FShaderCompilerInput& InShaderInput)
		: ShaderInput(InShaderInput)
	{
		FString ShaderDir = FPlatformProcess::ShaderDir();

		InputShaderFile = ShaderDir / FPaths::GetCleanFilename(ShaderInput.SourceFilename);
		if (FPaths::GetExtension(InputShaderFile) != TEXT("usf"))
		{
			InputShaderFile += TEXT(".usf");
		}

		FString InputShaderSource;
		if (LoadShaderSourceFile(*ShaderInput.SourceFilename,InputShaderSource))
		{
			InputShaderSource = FString::Printf(TEXT("%s\n#line 1\n%s"), *ShaderInput.SourceFilePrefix, *InputShaderSource);
			CachedFileContents.Add(GetRelativeShaderFilename(InputShaderFile),StringToArray<ANSICHAR>(*InputShaderSource, InputShaderSource.Len()));
		}
	}

	/** Returns the input shader filename to pass to MCPP. */
	const FString& GetInputShaderFilename() const
	{
		return InputShaderFile;
	}

	/** Retrieves the MCPP file loader interface. */
	file_loader GetMcppInterface()
	{
		file_loader Loader;
		Loader.get_file_contents = GetFileContents;
		Loader.user_data = (void*)this;
		return Loader;
	}

private:
	/** Holder for shader contents (string + size). */
	typedef TArray<ANSICHAR> FShaderContents;

	/** MCPP callback for retrieving file contents. */
	static int GetFileContents(void* InUserData, const ANSICHAR* InFilename, const ANSICHAR** OutContents, size_t* OutContentSize)
	{
		FMcppFileLoader* This = (FMcppFileLoader*)InUserData;
		FString Filename = GetRelativeShaderFilename(ANSI_TO_TCHAR(InFilename));

		FShaderContents* CachedContents = This->CachedFileContents.Find(Filename);
		if (!CachedContents)
		{
			FString FileContents;
			if (This->ShaderInput.Environment.IncludeFileNameToContentsMap.Contains(Filename))
			{
				FileContents = FString(UTF8_TO_TCHAR(This->ShaderInput.Environment.IncludeFileNameToContentsMap.FindRef(Filename).GetData()));
			}
			else
			{
				LoadShaderSourceFile(*Filename,FileContents);
			}

			if (FileContents.Len() > 0)
			{
				CachedContents = &This->CachedFileContents.Add(Filename,StringToArray<ANSICHAR>(*FileContents, FileContents.Len()));
			}
		}

		if (OutContents)
		{
			*OutContents = CachedContents ? CachedContents->GetData() : NULL;
		}
		if (OutContentSize)
		{
			*OutContentSize = CachedContents ? CachedContents->Num() : 0;
		}

		return !!CachedContents;
	}

	/** Shader input data. */
	const FShaderCompilerInput& ShaderInput;
	/** File contents are cached as needed. */
	TMap<FString,FShaderContents> CachedFileContents;
	/** The input shader filename. */
	FString InputShaderFile;
};

/**
 * Preprocess a shader.
 * @param OutPreprocessedShader - Upon return contains the preprocessed source code.
 * @param ShaderOutput - ShaderOutput to which errors can be added.
 * @param ShaderInput - The shader compiler input.
 * @param AdditionalDefines - Additional defines with which to preprocess the shader.
 * @returns true if the shader is preprocessed without error.
 */
bool PreprocessShader(
	FString& OutPreprocessedShader,
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	const FShaderCompilerDefinitions& AdditionalDefines
	)
{
	// Skip the cache system and directly load the file path (used for debugging)
	if (ShaderInput.bSkipPreprocessedCache)
	{
		return FFileHelper::LoadFileToString(OutPreprocessedShader, *ShaderInput.SourceFilename);
	}

	FString McppOptions;
	FString McppOutput, McppErrors;
	ANSICHAR* McppOutAnsi = NULL;
	ANSICHAR* McppErrAnsi = NULL;
	bool bSuccess = false;

	// MCPP is not threadsafe.
	static FCriticalSection McppCriticalSection;
	FScopeLock McppLock(&McppCriticalSection);

	FMcppFileLoader FileLoader(ShaderInput);

	AddMcppDefines(McppOptions, ShaderInput.Environment.GetDefinitions());
	AddMcppDefines(McppOptions, AdditionalDefines.GetDefinitionMap());

	int32 Result = mcpp_run(
		TCHAR_TO_ANSI(*McppOptions),
		TCHAR_TO_ANSI(*FileLoader.GetInputShaderFilename()),
		&McppOutAnsi,
		&McppErrAnsi,
		FileLoader.GetMcppInterface()
		);

	McppOutput = McppOutAnsi;
	McppErrors = McppErrAnsi;

	if (ParseMcppErrors(ShaderOutput.Errors, McppErrors, true))
	{
		// exchange strings
		FMemory::Memswap( &OutPreprocessedShader, &McppOutput, sizeof(FString) );
		bSuccess = true;
	}

	return bSuccess;
}
