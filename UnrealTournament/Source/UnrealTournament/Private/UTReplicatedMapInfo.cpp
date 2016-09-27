	// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTReplicatedMapInfo.h"
#include "Net/UnrealNetwork.h"
#include "UTBaseGameMode.h"
#include "SUTStyle.h"

AUTReplicatedMapInfo::AUTReplicatedMapInfo(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bAlwaysRelevant = true;
	bReplicateMovement = false;
	bNetLoadOnClient = false;

	bHasRights = false;

#if !UE_SERVER
	MapBrush = nullptr;
#endif
}

void AUTReplicatedMapInfo::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, MapPackageName, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, MapAssetName, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, Title, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, Author, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, Description, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, OptimalPlayerCount, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, OptimalTeamPlayerCount, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, Redirect, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, MapScreenshotReference, COND_InitialOnly);

	DOREPLIFETIME(AUTReplicatedMapInfo, VoteCount);
}

void AUTReplicatedMapInfo::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		PreLoadScreenshot();
	}
}

bool AUTReplicatedMapInfo::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	// prevent replication to clients that are not entitled to use this map, unless some other user selects it
	// that prevents unentitled clients from starting the map themselves
	if (VoteCount > 0)
	{
		return true;
	}
	else
	{
		const APlayerController* PC = Cast<APlayerController>(RealViewer);
		if (PC == NULL || PC->PlayerState == NULL || !PC->PlayerState->UniqueId.IsValid())
		{
			return true;
		}
		else
		{
			IOnlineEntitlementsPtr EntitlementInterface = IOnlineSubsystem::Get()->GetEntitlementsInterface();
			FUniqueEntitlementId Entitlement = GetRequiredEntitlementFromPackageName(FName(*MapPackageName));
			return (!EntitlementInterface.IsValid() || Entitlement.IsEmpty() || EntitlementInterface->GetItemEntitlement(*PC->PlayerState->UniqueId.GetUniqueNetId().Get(), Entitlement).IsValid());
		}
	}
}

void AUTReplicatedMapInfo::RegisterVoter(AUTPlayerState* Voter)
{
	int32 Index = VoterRegistry.Find(Voter);
	if (Index == INDEX_NONE)
	{
		VoterRegistry.Add(Voter);
		VoteCount++;
	}
}
void AUTReplicatedMapInfo::UnregisterVoter(AUTPlayerState* Voter)
{
	int32 Index = VoterRegistry.Find(Voter);
	if (Index != INDEX_NONE)
	{
		VoterRegistry.Remove(Voter);
		VoteCount--;
	}
}

void AUTReplicatedMapInfo::OnRep_VoteCount()
{
	UE_LOG(UT,Log,TEXT("Updated"));
	bNeedsUpdate = true;
}

void AUTReplicatedMapInfo::OnRep_MapScreenshotReference()
{
	PreLoadScreenshot();
}

void AUTReplicatedMapInfo::PreLoadScreenshot()
{
	if (!MapScreenshotReference.IsEmpty())
	{
		FString Package = MapScreenshotReference;
		const int32 Pos = Package.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
		if ( Pos != INDEX_NONE )
		{
			Package = Package.Left(Pos);
		}

		LoadPackageAsync(Package, FLoadPackageAsyncDelegate::CreateUObject(this, &AUTReplicatedMapInfo::MapTextureLoadComplete),0);
	}
}

void AUTReplicatedMapInfo::MapTextureLoadComplete(const FName& InPackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result)
{
	if (Result == EAsyncLoadingResult::Succeeded)	
	{
		MapScreenshot = FindObject<UTexture2D>(nullptr, *MapScreenshotReference);
		if (MapScreenshot)
		{
#if !UE_SERVER
			MapBrush = new FSlateDynamicImageBrush(MapScreenshot, FVector2D(256.0, 128.0), NAME_None);
#endif
		}
	}
}

void AUTReplicatedMapInfo::OnRep_MapPackageName()
{
	AUTBaseGameMode* BaseGameModeDO = AUTBaseGameMode::StaticClass()->GetDefaultObject<AUTBaseGameMode>();
	if (BaseGameModeDO)
	{
		BaseGameModeDO->CheckMapStatus(MapPackageName, bIsEpicMap, bIsMeshedMap, bHasRights);
	}
}

#if !UE_SERVER
TSharedRef<SWidget> AUTReplicatedMapInfo::BuildMapOverlay(FVector2D Size, bool bIgnoreLock)
{
	TSharedPtr<SOverlay> Overlay;
	SAssignNew(Overlay,SOverlay);

	float Scale = Size.Y / 512.0f;
	FVector2D IconTransform(512.0f - 128.0f, 256.0f - 128.0f);
	IconTransform *= Scale;

	if (bIsEpicMap)
	{
		if (!bIsMeshedMap)
		{
			Overlay->AddSlot()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(Size.X).HeightOverride(Size.Y)
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.MapOverlay.Epic.WIP"))
						]
					]	
				]
			];
		}
	}
	else
	{
		Overlay->AddSlot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(Size.X).HeightOverride(Size.Y)
					[
						SNew(SImage)
						.Image(SUTStyle::Get().GetBrush(bIsMeshedMap ? "UT.MapOverlay.Community" : "UT.MapOverlay.Community.WIP"))
					]
				]	
			]
		];
	
	}

	if (!bIgnoreLock && !bHasRights)
	{
		Overlay->AddSlot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(256.0f * Scale).HeightOverride(256.0f * Scale).RenderTransform(IconTransform)
					[
						SNew(SImage)
						.Image(SUTStyle::Get().GetBrush("UT.Icon.LockedContent"))
					]
				]	
			]
		];
	}

	return Overlay.ToSharedRef();
}

#endif