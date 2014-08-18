// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGameSession.h"

AUTGameSession::AUTGameSession(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
}

FString AUTGameSession::ApproveLogin(const FString& Options)
{
	AUTGameMode* GameMode = Cast<AUTGameMode>(GetWorld()->GetAuthGameMode());
	UE_LOG(UT,Log,TEXT("ApproveLogin: %s"),*Options);

	if (!GameMode->HasOption(Options, TEXT("VersionCheck")))
	{
		UE_LOG(UT, Warning, TEXT("********************************YOU MUST UPDATE TO A NEW VERSION %s"), *Options);
		return TEXT("You must update to a the latest version.  For more information, go to forums.unrealtournament.com");
	}
	if (GameMode->bRequirePassword && !GameMode->ServerPassword.IsEmpty())
	{
		FString Password = GameMode->ParseOption(Options, TEXT("Password"));
		if (Password.IsEmpty() || Password != GameMode->ServerPassword)
		{
			return TEXT("NEEDPASS");
		}
	}

	return Super::ApproveLogin(Options);
}