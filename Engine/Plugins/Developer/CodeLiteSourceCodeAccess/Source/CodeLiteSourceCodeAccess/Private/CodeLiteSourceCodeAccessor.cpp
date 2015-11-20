// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CodeLiteSourceCodeAccessPrivatePCH.h"
#include "CodeLiteSourceCodeAccessor.h"
#include "CodeLiteSourceCodeAccessModule.h"
#include "ISourceCodeAccessModule.h"
#include "ModuleManager.h"
#include "DesktopPlatformModule.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

#if WITH_EDITOR
#include "Developer/HotReload/Public/IHotReload.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogCodeLiteAccessor, Log, All);

#define LOCTEXT_NAMESPACE "CodeLiteSourceCodeAccessor"

void FCodeLiteSourceCodeAccessor::Startup()
{
	// Cache this so we don't have to do it on a background thread
	GetSolutionPath();
}

void FCodeLiteSourceCodeAccessor::Shutdown()
{

}

bool FCodeLiteSourceCodeAccessor::CanAccessSourceCode() const
{
	FString Path;
	return CanRunCodeLite(Path);
}

FName FCodeLiteSourceCodeAccessor::GetFName() const
{
	return FName("CodeLiteSourceCodeAccessor");
}

FText FCodeLiteSourceCodeAccessor::GetNameText() const
{
	return LOCTEXT("CodeLiteDisplayName", "CodeLite 7/8.x");
}

FText FCodeLiteSourceCodeAccessor::GetDescriptionText() const
{
	return LOCTEXT("CodeLiteDisplayDesc", "Open source code files in CodeLite");
}

bool FCodeLiteSourceCodeAccessor::OpenSolution()
{
	FString Filename = FPaths::GetBaseFilename(GetSolutionPath()) + ".workspace";
	FString Directory = FPaths::GetPath(GetSolutionPath());
	FString Solution = "\"" + Directory + "/" + Filename + "\"";
	FString CodeLitePath;
	if(!CanRunCodeLite(CodeLitePath))
	{
		UE_LOG(LogCodeLiteAccessor, Warning, TEXT("FCodeLiteSourceCodeAccessor::OpenSolution: Cannot find CodeLite binary"));
		return false;
	}

	UE_LOG(LogCodeLiteAccessor, Warning, TEXT("FCodeLiteSourceCodeAccessor::OpenSolution: %s %s"), *CodeLitePath, *Solution);
	
	FProcHandle Proc = FPlatformProcess::CreateProc(*CodeLitePath, *Solution, true, false, false, nullptr, 0, nullptr, nullptr);
	if(Proc.IsValid())
	{
		FPlatformProcess::CloseProc(Proc);
		return true;
	}
	return false;

}

bool FCodeLiteSourceCodeAccessor::OpenSourceFiles(const TArray<FString>& AbsoluteSourcePaths)
{
	FString CodeLitePath;
	if(!CanRunCodeLite(CodeLitePath))
	{
		UE_LOG(LogCodeLiteAccessor, Warning, TEXT("FCodeLiteSourceCodeAccessor::OpenSourceFiles: Cannot find CodeLite binary"));
		return false;
	}

	for(const auto& SourcePath : AbsoluteSourcePaths)
	{
		const FString Path = FString::Printf(TEXT("\"%s\""), *SourcePath);

		FProcHandle Proc = FPlatformProcess::CreateProc(*CodeLitePath, *Path, true, false, false, nullptr, 0, nullptr, nullptr);
		if(Proc.IsValid())
		{
			UE_LOG(LogCodeLiteAccessor, Warning, TEXT("CodeLiteSourceCodeAccessor::OpenSourceFiles: %s"), *Path);
			FPlatformProcess::CloseProc(Proc);
			return true;
		}
	}

	return false;
}

bool FCodeLiteSourceCodeAccessor::OpenFileAtLine(const FString& FullPath, int32 LineNumber, int32 ColumnNumber)
{
	FString CodeLitePath;
	if(!CanRunCodeLite(CodeLitePath))
	{
		UE_LOG(LogCodeLiteAccessor,Warning, TEXT("FCodeLiteSourceCodeAccessor::OpenFileAtLine: Cannot find CodeLite binary"));
		return false;
	}

	const FString Path = FString::Printf(TEXT("\"%s --line=%d\""), *FullPath, LineNumber);

	if(FPlatformProcess::CreateProc(*CodeLitePath, *Path, true, true, false, nullptr, 0, nullptr, nullptr).IsValid())
	{
		UE_LOG(LogCodeLiteAccessor, Warning, TEXT("FCodeLiteSourceCodeAccessor::OpenSolution: Cannot find CodeLite binary"));
	}

	UE_LOG(LogCodeLiteAccessor, Warning, TEXT("CodeLiteSourceCodeAccessor::OpenFileAtLine: %s %d"), *FullPath, LineNumber);

	return true;
}

bool FCodeLiteSourceCodeAccessor::AddSourceFiles(const TArray<FString>& AbsoluteSourcePaths, const TArray<FString>& AvailableModules)
{
	// TODO Is this possible without dbus here? Maybe we can add this functionality to CodeLite.
	return false;
}

bool FCodeLiteSourceCodeAccessor::SaveAllOpenDocuments() const
{
	// TODO Is this possible without dbus here? Maybe we can add this functionality to CodeLite.
	return false;
}

void FCodeLiteSourceCodeAccessor::Tick(const float DeltaTime)
{
	// TODO What can we do here?
}


bool FCodeLiteSourceCodeAccessor::CanRunCodeLite(FString& OutPath) const
{
	// TODO This might be not a good idea to find an executable.
	OutPath = TEXT("/usr/bin/codelite");
	return FPaths::FileExists(OutPath);
}


bool FCodeLiteSourceCodeAccessor::IsIDERunning()
{
#ifdef PLATFORM_LINUX
	pid_t pid = FindProcess("codelite");
	if(pid == -1)
	{
		return false;
	}

	return true;
#endif
	return false;

}

FString FCodeLiteSourceCodeAccessor::GetSolutionPath() const
{
	if(IsInGameThread())
	{
		FString SolutionPath;
		if(FDesktopPlatformModule::Get()->GetSolutionPath(SolutionPath))
		{
			CachedSolutionPath = FPaths::ConvertRelativePathToFull(SolutionPath);
		}
	}
	return CachedSolutionPath;
}

#ifdef PLATFORM_LINUX
// This needs to be changed to use platform abstraction layer (see FPlatformProcess::FProcEnumerator)
pid_t FCodeLiteSourceCodeAccessor::FindProcess(const char* name)
{
	// TODO This is only to test. Will be changed later.
	DIR* dir;
	struct dirent* ent;
	char* endptr;
	char buf[512];

	if(!(dir = opendir("/proc")))
	{
		perror("can't open /proc");
		return -1;
	}

	while((ent = readdir(dir)) != NULL)
	{
		long lpid = strtol(ent->d_name, &endptr, 10);
		if(*endptr != '\0')
		{
			continue;
		}

		// Try to open the cmdline file
		snprintf(buf, sizeof(buf), "/proc/%ld/cmdline", lpid);
		FILE* fp = fopen(buf, "r");
		if(fp)
		{
			if(fgets(buf, sizeof(buf), fp) != NULL)
			{
				// check the first token in the file, the program name.
				char* first = strtok(buf, " ");
				if(!strcmp(first, name))
				{
					fclose(fp);
					closedir(dir);
					return (pid_t)lpid;
				}
			}
			fclose(fp);
		}
	}

	closedir(dir);

	return -1;
}
#endif

#undef LOCTEXT_NAMESPACE
