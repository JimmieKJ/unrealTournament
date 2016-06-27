// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournamentEditor.h"

#include "GenerateLoadOrderFileCommandlet.h"
#include "CoreMisc.h"

UGenerateLoadOrderFileCommandlet::UGenerateLoadOrderFileCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

int32 UGenerateLoadOrderFileCommandlet::Main(const FString& FullCommandLine)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Params;
	ParseCommandLine(*FullCommandLine, Tokens, Switches, Params);

	FString LogFileDirectory;
	FString LogFilePath;
	FString NewLogFilePath;

	LogFileDirectory = FPaths::Combine(FPlatformMisc::GameDir(), TEXT("Build"), TEXT("WindowsNoEditor"), TEXT("FileOpenOrder"));
	LogFilePath = FPaths::Combine(*LogFileDirectory, TEXT("GameOpenOrder.log"));
	
	TArray<FString> OrderEntries;
	FFileHelper::LoadANSITextFileToStrings(*LogFilePath, &IFileManager::Get(), OrderEntries);

	int32 SpaceIndex = 0;
	for (int i = 0; i < OrderEntries.Num(); i++)
	{
		if (OrderEntries[i].FindLastChar(TEXT('\"'), SpaceIndex))
		{
			OrderEntries[i] = OrderEntries[i].Left(SpaceIndex + 1);
		}
	}

	// Sanitize entries
	for (int i = OrderEntries.Num() - 1; i >= 0; i--)
	{
		if (OrderEntries[i].IsEmpty())
		{
			OrderEntries.RemoveAt(i);
		}
		else if ((*OrderEntries[i])[1] != TEXT('.'))
		{
			OrderEntries.RemoveAt(i);
		}
	}

	TMap<FString, int32> PreviousFileOrderEntries;
	for (int i = 0; i < OrderEntries.Num(); i++)
	{
		PreviousFileOrderEntries.Emplace(OrderEntries[i], i);
	}

	// Helper class to find all uassets (and umaps) files.
	class FAssetkFileSearchVisitor : public IPlatformFile::FDirectoryVisitor
	{
		TArray<FString>& FoundFiles;
	public:
		FAssetkFileSearchVisitor(TArray<FString>& InFoundFiles)
			: FoundFiles(InFoundFiles)
		{}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if (bIsDirectory == false)
			{
				FString Filename(FilenameOrDirectory);
				if (Filename.MatchesWildcard(TEXT("*.uasset")) || Filename.MatchesWildcard(TEXT("*.umap")))
				{
					FoundFiles.Add(Filename);
				}
			}
			return true;
		}
	};

	// Search for asset files that were downloaded through redirects
	TArray<FString>	FoundAssets;
	FAssetkFileSearchVisitor AssetVisitor(FoundAssets);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	FoundAssets.Empty();
	PlatformFile.IterateDirectoryRecursively(*FPaths::Combine(*FPaths::GameContentDir(), TEXT("RestrictedAssets")), AssetVisitor);
	for (const FString& AssetPath : FoundAssets)
	{
		FString Text = FString::Printf(TEXT("\"%s\""), *AssetPath);
		if (!PreviousFileOrderEntries.Contains(Text))
		{
			OrderEntries.Add(Text);
		}
	}

	FoundAssets.Empty();
	PlatformFile.IterateDirectoryRecursively(*FPaths::Combine(*FPaths::GameContentDir(), TEXT("EpicInternal")), AssetVisitor);
	for (const FString& AssetPath : FoundAssets)
	{
		FString Text = FString::Printf(TEXT("\"%s\""), *AssetPath);
		if (!PreviousFileOrderEntries.Contains(Text))
		{
			OrderEntries.Add(Text);
		}
	}

	NewLogFilePath = FPaths::Combine(*LogFileDirectory, TEXT("NewGameOpenOrder.log"));
	IFileHandle* NewOpenOrder = PlatformFile.OpenWrite(*NewLogFilePath);

	for (int i = 0; i < OrderEntries.Num(); i++)
	{
		FString Text = FString::Printf(TEXT("%s %llu\n"), *OrderEntries[i], i + 1);
		NewOpenOrder->Write((uint8*)StringCast<ANSICHAR>(*Text).Get(), Text.Len());
	}

	delete NewOpenOrder;

	return 0;
}