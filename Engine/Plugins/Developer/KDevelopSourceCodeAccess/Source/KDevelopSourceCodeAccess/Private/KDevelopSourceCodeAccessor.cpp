// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "KDevelopSourceCodeAccessPrivatePCH.h"
#include "KDevelopSourceCodeAccessor.h"
#include "KDevelopSourceCodeAccessModule.h"
#include "ISourceCodeAccessModule.h"
#include "ModuleManager.h"
#include "DesktopPlatformModule.h"

#if WITH_EDITOR
#include "Developer/HotReload/Public/IHotReload.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogKDevelopAccessor, Log, All);

#define LOCTEXT_NAMESPACE "KDevelopSourceCodeAccessor"

void FKDevelopSourceCodeAccessor::Startup()
{
	// Cache this so we don't have to do it on a background thread
	GetSolutionPath();
	
	// FIXME: look for kdevelop and cache the path
	
}

void FKDevelopSourceCodeAccessor::Shutdown()
{
}

bool FKDevelopSourceCodeAccessor::OpenSolution()
{
	if (IsIDERunning())
	{
		// use qdbus to open the project within session?
		STUBBED("OpenSolution: KDevelop is running, use qdbus to open the project within session?");
		return false;
	}

	FString Solution = GetSolutionPath();
	FString IDEPath;
	if (!CanRunKDevelop(IDEPath))
	{
		UE_LOG(LogKDevelopAccessor, Warning, TEXT("FKDevelopSourceCodeAccessor::OpenSourceFiles: cannot find kdevelop binary"));
		return false;
	}
	
	FProcHandle Proc = FPlatformProcess::CreateProc(*IDEPath, *Solution, true, false, false, nullptr, 0, nullptr, nullptr);
	if (Proc.IsValid())
	{
		FPlatformProcess::CloseProc(Proc);
		return true;
	}
	return false;
}

bool FKDevelopSourceCodeAccessor::CanRunKDevelop(FString& OutPath) const
{
	// FIXME: search properly
	OutPath = TEXT("/usr/bin/kdevelop");	
	return FPaths::FileExists(OutPath);
}

bool FKDevelopSourceCodeAccessor::OpenSourceFiles(const TArray<FString>& AbsoluteSourcePaths)
{
	if (IsIDERunning())
	{
		// use qdbus
		STUBBED("OpenSourceFiles: KDevelop is running, use qdbus");
		return false;
	}
	
	STUBBED("FKDevelopSourceCodeAccessor::OpenSourceFiles");
	return false;
}

bool FKDevelopSourceCodeAccessor::AddSourceFiles(const TArray<FString>& AbsoluteSourcePaths, const TArray<FString>& AvailableModules)
{
	return false;
}

bool FKDevelopSourceCodeAccessor::OpenFileAtLine(const FString& FullPath, int32 LineNumber, int32 ColumnNumber)
{
	// column & line numbers are 1-based, so dont allow zero
	if(LineNumber == 0)
	{
		LineNumber++;
	}
	if(ColumnNumber == 0)
{
		ColumnNumber++;
	}

	// Automatically fail if there's already an attempt in progress
	if (IsIDERunning())
	{
		// use qdbus
		STUBBED("OpenFileAtLine: KDevelop is running, use qdbus");
		return false;
	}

	STUBBED("FKDevelopSourceCodeAccessor::OpenFileAtLine");
	return false;
}

bool FKDevelopSourceCodeAccessor::SaveAllOpenDocuments() const
{
	STUBBED("FKDevelopSourceCodeAccessor::SaveAllOpenDocuments");
	return false;
}

void FKDevelopSourceCodeAccessor::Tick(const float DeltaTime)
{
}

bool FKDevelopSourceCodeAccessor::CanAccessSourceCode() const
{
	FString Path;
	return CanRunKDevelop(Path);
}

FName FKDevelopSourceCodeAccessor::GetFName() const
{
	return FName("KDevelopSourceCodeAccessor");
}

FText FKDevelopSourceCodeAccessor::GetNameText() const
{
	return LOCTEXT("KDevelopDisplayName", "KDevelop 4.x");
}

FText FKDevelopSourceCodeAccessor::GetDescriptionText() const
{
	return LOCTEXT("KDevelopDisplayDesc", "Open source code files in KDevelop 4.x");
}

bool FKDevelopSourceCodeAccessor::IsIDERunning()
{
	// FIXME: implement
	STUBBED("FKDevelopSourceCodeAccessor::IsIDERunning");
	return false;
}

FString FKDevelopSourceCodeAccessor::GetSolutionPath() const
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

#undef LOCTEXT_NAMESPACE
