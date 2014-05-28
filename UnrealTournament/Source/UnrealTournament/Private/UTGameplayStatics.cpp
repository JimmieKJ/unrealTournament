// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGameplayStatics.h"

void UUTGameplayStatics::UTPlaySound(UWorld* TheWorld, USoundBase* TheSound, AActor* SourceActor, ESoundReplicationType RepType, bool bStopWhenOwnerDestroyed, const FVector& SoundLoc)
{
	if (TheSound != NULL)
	{
		if (SourceActor == NULL && SoundLoc.IsZero())
		{
			UE_LOG(UT, Warning, TEXT("UTPlaySound(): No source (SourceActor == None and SoundLoc not specified)"));
		}
		else
		{
			if (RepType >= SRT_MAX)
			{
				UE_LOG(UT, Warning, TEXT("UTPlaySound(): Unexpected RepType"));
				RepType = SRT_All;
			}

			const FVector& SourceLoc = !SoundLoc.IsZero() ? SoundLoc : SourceActor->GetActorLocation();

			if (TheWorld->GetNetMode() != NM_Standalone && TheWorld->GetNetDriver() != NULL)
			{
				APlayerController* TopOwner = NULL;
				for (AActor* TestActor = SourceActor; TestActor != NULL && TopOwner != NULL; TestActor = TestActor->GetOwner())
				{
					TopOwner = Cast<APlayerController>(TopOwner);
				}

				for (int32 i = 0; i < TheWorld->GetNetDriver()->ClientConnections.Num(); i++)
				{
					AUTPlayerController* PC = Cast<AUTPlayerController>(TheWorld->GetNetDriver()->ClientConnections[i]->OwningActor);
					if (PC != NULL)
					{
						bool bShouldReplicate;
						switch (RepType)
						{
						case SRT_All:
							bShouldReplicate = true;
							break;
						case SRT_AllButOwner:
							bShouldReplicate = PC != TopOwner;
							break;
						case SRT_IfSourceNotReplicated:
							bShouldReplicate = TheWorld->GetNetDriver()->ClientConnections[i]->ActorChannels.Find(SourceActor) == NULL;
							break;
						default:
							// should be impossible
							bShouldReplicate = true;
							break;
						}

						if (bShouldReplicate)
						{
							PC->HearSound(TheSound, SourceActor, SourceLoc, bStopWhenOwnerDestroyed);
						}
					}
				}
			}

			for (FLocalPlayerIterator It(GEngine, TheWorld); It; ++It)
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(It->PlayerController);
				if (PC != NULL && PC->IsLocalPlayerController())
				{
					PC->HearSound(TheSound, SourceActor, SourceLoc, bStopWhenOwnerDestroyed);
				}
			}
		}
	}
}