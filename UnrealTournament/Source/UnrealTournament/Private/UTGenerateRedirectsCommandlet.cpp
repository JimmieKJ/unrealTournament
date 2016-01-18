// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGenerateRedirectsCommandlet.h"

DEFINE_LOG_CATEGORY_STATIC(LogGenerateRedirectsCommandlet, Log, All);

UUTGenerateRedirectsCommandlet::UUTGenerateRedirectsCommandlet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	IsEditor = false;
	IsClient = false;
	IsServer = true;
}

FString UUTGenerateRedirectsCommandlet::MD5Sum(const TArray<uint8>& Data)
{
	uint8 Digest[16];

	FMD5 Md5Gen;

	Md5Gen.Update((uint8*)Data.GetData(), Data.Num());
	Md5Gen.Final(Digest);

	FString MD5;
	for (int32 i = 0; i < 16; i++)
	{
		MD5 += FString::Printf(TEXT("%02x"), Digest[i]);
	}

	return MD5;
}

int32 UUTGenerateRedirectsCommandlet::Main(const FString& Params)
{
	UE_LOG(LogGenerateRedirectsCommandlet, Warning, TEXT("Starting UTGenerateRedirectsCommandlet..."));
	
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> SwitchParams;
	ParseCommandLine(*Params, Tokens, Switches, SwitchParams);

	FString PackageName;
	FString WebAddress;
	for (auto It = SwitchParams.CreateConstIterator(); It; ++It)
	{
		if (It.Key() == TEXT("Package"))
		{
			PackageName = It.Value();
		}
		if (It.Key() == TEXT("WebAddress"))
		{
			WebAddress = It.Value();
		}
	}

	if (PackageName.IsEmpty())
	{
		UE_LOG(LogGenerateRedirectsCommandlet, Warning, TEXT("No package specified with -package"));
		return 0;
	}
	if (WebAddress.IsEmpty())
	{
		UE_LOG(LogGenerateRedirectsCommandlet, Warning, TEXT("No web address specified with -WebAddress"));
		return 0;
	}

	FString WebProtocol;
	int32 Pos = WebAddress.Find(TEXT("://"));
	if (Pos != INDEX_NONE)
	{
		WebProtocol = WebAddress.Left(Pos);
		WebAddress = WebAddress.RightChop(Pos + 3);
	}

	if (WebProtocol.IsEmpty())
	{
		UE_LOG(LogGenerateRedirectsCommandlet, Warning, TEXT("No web protocol specified with -WebAddress"));
		return 0;
	}


	// Helper class to find all pak files.
	class FPakFileSearchVisitor : public IPlatformFile::FDirectoryVisitor
	{
		TArray<FString>& FoundFiles;
	public:
		FPakFileSearchVisitor(TArray<FString>& InFoundFiles)
			: FoundFiles(InFoundFiles)
		{}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if (bIsDirectory == false)
			{
				FString Filename(FilenameOrDirectory);
				if (Filename.MatchesWildcard(TEXT("*.pak")))
				{
					FoundFiles.Add(Filename);
				}
			}
			return true;
		}
	};

	bool bFoundPackage = false;
	FString Checksum;

	// Search for pak files
	TArray<FString>	FoundPaks;
	FPakFileSearchVisitor PakVisitor(FoundPaks);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	FoundPaks.Empty();
	PlatformFile.IterateDirectoryRecursively(*FPaths::Combine(*FPaths::GameContentDir(), TEXT("Paks")), PakVisitor);
	for (const auto& PakPath : FoundPaks)
	{
		FString PakFilename = FPaths::GetBaseFilename(PakPath);
		if (!PakFilename.StartsWith(TEXT("UnrealTournament-"), ESearchCase::IgnoreCase))
		{
			if (PakFilename == PackageName)
			{
				bFoundPackage = true;
				TArray<uint8> Data;
				if (FFileHelper::LoadFileToArray(Data, *PakPath))
				{
					Checksum = MD5Sum(Data);
				}
				break;
			}
		}
	}

	if (!bFoundPackage)
	{
		FoundPaks.Empty();
		PlatformFile.IterateDirectoryRecursively(*FPaths::Combine(*FPaths::GameSavedDir(), TEXT("Paks"), TEXT("MyContent")), PakVisitor);
		for (const auto& PakPath : FoundPaks)
		{
			FString PakFilename = FPaths::GetBaseFilename(PakPath);
			if (!PakFilename.StartsWith(TEXT("UnrealTournament-"), ESearchCase::IgnoreCase))
			{
				if (PakFilename == PackageName)
				{
					bFoundPackage = true;
					TArray<uint8> Data;
					if (FFileHelper::LoadFileToArray(Data, *PakPath))
					{
						Checksum = MD5Sum(Data);
					}
				}
			}
		}
	}

	if (!bFoundPackage)
	{
		UE_LOG(LogGenerateRedirectsCommandlet, Warning, TEXT("Could not find %s.pak"), *PackageName);
		return 0;
	}

	AUTBaseGameMode* GM = AUTBaseGameMode::StaticClass()->GetDefaultObject<AUTBaseGameMode>();
	if (GM)
	{
		bool bAlreadyHadPackage = false;
		for (int i = 0; i < GM->RedirectReferences.Num(); i++)
		{
			if (GM->RedirectReferences[i].PackageName == PackageName)
			{
				bAlreadyHadPackage = true;
				GM->RedirectReferences[i].PackageURLProtocol = WebProtocol;
				GM->RedirectReferences[i].PackageURL = WebAddress;
				GM->RedirectReferences[i].PackageChecksum = Checksum;
			}
		}

		if (!bAlreadyHadPackage)
		{
			FPackageRedirectReference NewRedirectReference;

			NewRedirectReference.PackageName = PackageName;
			NewRedirectReference.PackageURLProtocol = WebProtocol;
			NewRedirectReference.PackageURL = WebAddress;
			NewRedirectReference.PackageChecksum = Checksum;
			GM->RedirectReferences.Add(NewRedirectReference);
		}

		GM->SaveConfig();
	}

	return 0;
}