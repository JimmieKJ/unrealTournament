#include "UnrealTournament.h"
#include "UTDemoNetDriver.h"
#include "UTGameInstance.h"
#include "UTGameEngine.h"

void UUTDemoNetDriver::WriteGameSpecificDemoHeader(TArray<FString>& GameSpecificData)
{
	AUTBaseGameMode* Game = GetWorld()->GetAuthGameMode<AUTBaseGameMode>();
	if (Game != NULL)
	{
		TArray<FPackageRedirectReference> AllRedirects;
		Game->GatherRequiredRedirects(AllRedirects);
		for (const FPackageRedirectReference& Redirect : AllRedirects)
		{
			GameSpecificData.Add(FString::Printf(TEXT("%s\n%s\n%s"), *Redirect.PackageName, *Redirect.ToString(), *Redirect.PackageChecksum));
		}
	}
}

bool UUTDemoNetDriver::ProcessGameSpecificDemoHeader(const TArray<FString>& GameSpecificData, FString& Error)
{
	UUTGameInstance* GI = Cast<UUTGameInstance>(GetWorld()->GetGameInstance());
	if (GI == NULL || GameSpecificData.Num() == 0 || GI->GetLastTriedDemo() == DemoFilename)
	{ 
		return true;
	}
	else
	{
		bool bDownloading = false;
		UUTGameEngine* Engine = Cast<UUTGameEngine>(GEngine);
		for (const FString& Item : GameSpecificData)
		{
			TArray<FString> Pieces;
			Item.ParseIntoArray(Pieces, TEXT("\n"), false);
			if (Pieces.Num() == 3)
			{
				// 0: pak name, 1: URL, 2: checksum
				bool bNeedsDownload = true;
				if (Engine != NULL)
				{
					FString* Checksum = Engine->MountedDownloadedContentChecksums.Find(Pieces[0]);
					if (Checksum != NULL && *Checksum == Pieces[2])
					{
						bNeedsDownload = false;
					}
				}
				if (bNeedsDownload)
				{
					GI->StartRedirectDownload(Pieces[1]);
					bDownloading = true;
				}
			}
		}
		if (bDownloading)
		{
			GI->SetLastTriedDemo(DemoFilename);
			Error = TEXT("Waiting for redirects to download files");
			return false;
		}
		else
		{
			return true;
		}
	}
}