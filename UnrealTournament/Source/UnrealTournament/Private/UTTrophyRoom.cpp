// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTTrophyRoom.h"
//#include "UTTrophyStart.h"
#include "Matinee/MatineeActor.h"
#include "UTWeaponAttachment.h"
#include "UTTrophyRoomRenderingComponent.h"

static const float PLAYER_SPACING = 120.0f;
static const float PLAYER_ALTOFFSET = 80.0f;
static const float MIN_TEAM_SPACING = 120.f;
static const float TEAM_CAMERA_OFFSET = 500.0f;
static const float TEAM_CAMERA_ZOFFSET = 0.0f;
static const float LARGETEAM_CAMERA_ZOFFSET = 100.0f;
static const float ALL_CAMERA_OFFSET = 400.0f;
static const float ALL_CAMERA_ANGLE = -5.0f;
static const float TEAMANGLE = 45.0f;

// Sets default values
AUTTrophyRoom::AUTTrophyRoom()
{
	SetReplicates(true);
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	RootComponent = SceneComponent;
	RootComponent->Mobility = EComponentMobility::Static;

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));

	if (!IsRunningCommandlet())
	{
		//TODOTIM: make trophy dev art
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> NoteTextureObject;
			FName ID_Notes;
			FText NAME_Notes;
			FConstructorStatics()
				: NoteTextureObject(TEXT("/Engine/EditorResources/S_Note"))
				, ID_Notes(TEXT("Notes"))
				, NAME_Notes(NSLOCTEXT("SpriteCategory", "Notes", "Notes"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (SpriteComponent)
		{
			SpriteComponent->Sprite = ConstructorStatics.NoteTextureObject.Get();
			SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
			SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Notes;
			SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Notes;
			SpriteComponent->AttachParent = RootComponent;
			SpriteComponent->Mobility = EComponentMobility::Static;
		}
	}


	TrophyRenderComp = CreateDefaultSubobject<UUTTrophyRoomRenderingComponent>(TEXT("TrophyRenderComp"));
	TrophyRenderComp->PostPhysicsComponentTick.bCanEverTick = false;
	TrophyRenderComp->AttachParent = RootComponent;

#endif // WITH_EDITORONLY_DATA

	// Setup camera defaults
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->FieldOfView = 90.0f;
	CameraComponent->bConstrainAspectRatio = true;
	CameraComponent->AspectRatio = 1.777778f;
	CameraComponent->PostProcessBlendWeight = 1.0f;
	CameraComponent->AttachParent = RootComponent;

	CameraTeamRed = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraTeamRed"));
	CameraTeamRed->FieldOfView = 90.0f;
	CameraTeamRed->bConstrainAspectRatio = true;
	CameraTeamRed->AspectRatio = 1.777778f;
	CameraTeamRed->PostProcessBlendWeight = 1.0f;
	CameraTeamRed->AttachParent = RootComponent;

	CameraTeamBlue = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraTeamBlue"));
	CameraTeamBlue->FieldOfView = 90.0f;
	CameraTeamBlue->bConstrainAspectRatio = true;
	CameraTeamBlue->AspectRatio = 1.777778f;
	CameraTeamBlue->PostProcessBlendWeight = 1.0f;
	CameraTeamBlue->AttachParent = RootComponent;

	bHidden = true;
	bCanBeDamaged = false;


	bSnapToFloor = true;
	bShowCharacters = false;

	TrophyType = ETrophyType::FFA_Intro;

	SetupGroups();

	static ConstructorHelpers::FObjectFinder<UTexture2D> Tex(TEXT("Texture2D'/Game/RestrictedAssets/UI/Textures/UTScoreboard01.UTScoreboard01'"));
	ScoreboardAtlas = Tex.Object;
}

void AUTTrophyRoom::SetupGroups()
{
	TeamGroups.Empty();

	if (GetNumTeamsFromType(TrophyType) == 2)
	{
		TeamGroups.AddDefaulted(2);
		TeamGroups[0].Team = 0;
		TeamGroups[1].Team = 1;
	}
	else
	{
		TeamGroups.AddDefaulted(1);
		TeamGroups[0].Team = 255;
	}

	for (int32 iTeam = 0; iTeam < TeamGroups.Num(); iTeam++)
	{
		FTrophyGroup& Group = TeamGroups[iTeam];

		int32 Row = 0;
		for (int32 iPlayer = 0; iPlayer < MAX_TROPHY_PLAYERS; iPlayer++)
		{
			FTransform& PlayerSpot = Group.Players[iPlayer];

			int32 Dir = (iPlayer % 2 == 0) ? 1 : -1;

			float RowOffset = (Row % 2 != 0) ? PLAYER_SPACING * 0.5f : 0.0f;
			float X = PLAYER_SPACING * Row + TEAM_CAMERA_OFFSET;
			float Y = (float)(((iPlayer % 5) + 1) / 2 * Dir) * PLAYER_SPACING + RowOffset;
			
			if ((iPlayer + 1) % 5 == 0)
			{
				Row++;
			}

			PlayerSpot.SetLocation(FVector(X, Y, 0.0f));
			PlayerSpot.SetRotation(FRotator(0.0f,180.0f,0.0f).Quaternion());
			if (TeamGroups.Num() == 2)
			{
				float Angle = -45.0f + (90.0f * Group.Team);
				PlayerSpot.SetLocation(FRotator(0.0f, Angle, 0.0f).RotateVector(PlayerSpot.GetLocation()));
				PlayerSpot.SetRotation(FRotator(0.0f, Angle + 180.0f, 0.0f).Quaternion());
			}
		}
	}

	CenterPivot();

	CameraComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -1.f * ALL_CAMERA_OFFSET * FMath::Sin(ALL_CAMERA_ANGLE * PI / 180.f)));
	CameraComponent->SetRelativeRotation(FRotator(ALL_CAMERA_ANGLE, 0.0f, 0.0f));

	if (bSnapToFloor)
	{
		SnapToFloor();
	}
}

void AUTTrophyRoom::BecomeViewTarget(APlayerController* PC)
{
	Super::BecomeViewTarget(PC);
	ShowCharacters(true);

	for (TActorIterator<AUTPickupInventory> It(GetWorld()); It; ++It)
	{
		AUTPickupInventory* Inv = *It;
		Inv->SetActorHiddenInGame(true);
	}
}

void AUTTrophyRoom::EndViewTarget(APlayerController* PC)
{
	Super::EndViewTarget(PC);
	ShowCharacters(false);

	for (TActorIterator<AUTPickupInventory> It(GetWorld()); It; ++It)
	{
		AUTPickupInventory* Inv = *It;
		Inv->SetActorHiddenInGame(false);
	}
}

void AUTTrophyRoom::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);

	//CameraComponent->GetCameraView(DeltaTime, OutResult);
	OutResult.Location = CamTransform.GetLocation();
	OutResult.Rotation = CamTransform.GetRotation().Rotator();
	OutResult.FOV = 90.0f;
}

void AUTTrophyRoom::SetCameraTransform(const FTransform& NewTransform)
{
	CamTransform = NewTransform;
}

void AUTTrophyRoom::CollectPlayerStates()
{
	PlayerStates.Empty();

	//Add any Playerstates that were created before loading the trophy room
	for (TActorIterator<AUTPlayerState> It(GetWorld()); It; ++It)
	{
		AUTPlayerState* PS = *It;
		if (PS != nullptr)
		{
			PlayerStates.Add(PS);
		}
	}
}

void AUTTrophyRoom::AddPlayerState(AUTPlayerState* PlayerState)
{
	if (PlayerState != nullptr && FindCharacter(PlayerState) == nullptr)
	{
		PlayerStates.AddUnique(PlayerState);

		//Player states added before beginplay will be created in begin play
		if (HasActorBegunPlay())
		{
			RecreatePlayerPreview(PlayerState);
			PositionPlayers();
		}
	}
}

void AUTTrophyRoom::RemovePlayerState(AUTPlayerState* PlayerState)
{
	if (PlayerState != nullptr)
	{
		PlayerStates.Remove(PlayerState);

		AUTCharacter* Character = FindCharacter(PlayerState);
		if (Character != nullptr)
		{
			Characters.Remove(Character);
			Character->Destroy();
		}

		if (HasActorBegunPlay())
		{
			PositionPlayers();
		}
	}
}

AUTCharacter* AUTTrophyRoom::FindCharacter(AUTPlayerState* PS)
{
	AUTCharacter** CharacterPtr = Characters.FindByPredicate([PS](const AUTCharacter* Char){ return Char && Char->PlayerState == PS; });
	if (CharacterPtr != nullptr)
	{
		return *CharacterPtr;
	}
	return nullptr;
}

AUTPlayerState* AUTTrophyRoom::NextCharacter(AUTPlayerState* OldPS, bool bBackward) 
{
	//TODOTIM incomplete broke
	int32 Dir = bBackward ? -1 : 1;

	int32 StartIndex = PlayerStates.Find(OldPS);
	if (StartIndex == INDEX_NONE)
	{
		return PlayerStates.Num() > 0 ? PlayerStates[0] : nullptr;
	}

	int32 Index = StartIndex + Dir;
	while (Index != StartIndex)
	{
		if (Index < 0)	{ Index = PlayerStates.Num() - 1; }
		else if (Index >= PlayerStates.Num()) { Index = 0; }

		//TODO same team loop around
		return PlayerStates[Index];

		Index += Dir;
	}
	return nullptr;
}

bool AUTTrophyRoom::FindSpot(uint8 Team, int32 Index, FTransform& OutSpot)
{
	for (auto& Group : TeamGroups)
	{
		if (Group.Team == Team && Group.Players.IsValidIndex(Index))
		{
			OutSpot = Group.Players[Index];
			return true;
		}
	}
	return false;
}

void AUTTrophyRoom::BeginPlay()
{
	Super::BeginPlay();

	if (bShowCharacters)
	{
		ShowCharacters(bShowCharacters);
	}
}

void AUTTrophyRoom::Destroyed()
{
	DestroyCharacters();
	PlayerStates.Empty();

	if (bShowCharacters && GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
		if (PC != NULL && PC->MyHUD != NULL)
		{
			PC->MyHUD->RemovePostRenderedActor(this);
		}
	}

	Super::Destroyed();
}

void AUTTrophyRoom::DestroyCharacters()
{
	for (auto UTC : Characters)
	{
		if (UTC != nullptr)
		{
			UTC->Destroy();
		}
	}
	Characters.Empty();
	for (auto Weapon : PreviewWeapons)
	{
		if (Weapon != nullptr)
		{
			Weapon->Destroy();
		}
	}
	PreviewWeapons.Empty();
}

void AUTTrophyRoom::RecreateCharacters()
{
	DestroyCharacters();
	CollectPlayerStates();
	for (auto PS : PlayerStates)
	{
		RecreatePlayerPreview(PS);
	}
	PositionPlayers();
}
void AUTTrophyRoom::ShowCharacters(bool bShow)
{
	bShowCharacters = bShow;
	if (HasActorBegunPlay() && GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
		if (PC != NULL)
		{
			if (bShowCharacters)
			{
				RecreateCharacters();

				if (PC->MyHUD != NULL)
				{
					PC->MyHUD->AddPostRenderedActor(this);
				}
			}
			else
			{
				DestroyCharacters();

				if (PC->MyHUD != NULL)
				{
					PC->MyHUD->RemovePostRenderedActor(this);
				}
			}
		}
	}
}

void AUTTrophyRoom::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{
	TArray<FVector2D> BadgeNumberUVs;
	BadgeNumberUVs.Add(FVector2D(248, 183));
	BadgeNumberUVs.Add(FVector2D(283, 183));
	BadgeNumberUVs.Add(FVector2D(318, 183));
	BadgeNumberUVs.Add(FVector2D(353, 183));
	BadgeNumberUVs.Add(FVector2D(388, 183));
	BadgeNumberUVs.Add(FVector2D(423, 183));
	BadgeNumberUVs.Add(FVector2D(458, 183));
	BadgeNumberUVs.Add(FVector2D(248, 219));
	BadgeNumberUVs.Add(FVector2D(283, 219));

	TArray<FVector2D> BadgeUVs;
	BadgeUVs.Add(FVector2D(388, 219));
	BadgeUVs.Add(FVector2D(353, 219));
	BadgeUVs.Add(FVector2D(318, 219));

	UTexture* TextureAtlas = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->HUDAtlas;

	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PC);
	AUTGameState* GS = Cast<AUTGameState>(GetWorld()->GameState);
	if (GS != nullptr && UTPC != nullptr && UTPC->MyUTHUD != nullptr && !UTPC->MyUTHUD->bShowScores)
	{
		for (auto UTC : Characters)
		{
			if (UTC != nullptr && !UTC->IsPendingKillPending() && FVector::DotProduct(CameraDir, (UTC->GetActorLocation() - CameraPosition)) > 0.0f)
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(UTC->PlayerState);
				if (!UTC->bHidden && PS != nullptr)
				{
					float Scale = Canvas->ClipX / 1920.f;
					FVector WorldPosition = UTC->GetMesh()->GetComponentLocation();
					FVector ScreenPosition = Canvas->Project(WorldPosition + FVector(0.f, 0.f, UTC->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * 2.50f));

					float TextXL, TextYL;
					UFont* TinyFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->SmallFont;
					Canvas->TextSize(TinyFont, PS->PlayerName, TextXL, TextYL, Scale, Scale);

					float Border = 1.0f;
					float BadgeSize = 20.0f;
					float BGXL = TextXL + BadgeSize + (Border * 4.0f);
					float BGYL = BadgeSize + (Border * 2.0f);
					float StartX = FMath::TruncToFloat(Canvas->OrgX + ScreenPosition.X - (BGXL * 0.5f));
					float StartY = FMath::TruncToFloat(Canvas->OrgX + ScreenPosition.Y - (BGYL * 0.5f));

					FLinearColor BarColor = FLinearColor::Black;
					BarColor.A = 0.7f;
					Canvas->SetLinearDrawColor(BarColor);
					Canvas->DrawTile(TextureAtlas, StartX, StartY, BGXL, BGYL, 185.f, 400.f, 4.f, 4.f);

					AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
					AUTGameMode* DefaultGame = UTGameState && UTGameState->GameModeClass ? UTGameState->GameModeClass->GetDefaultObject<AUTGameMode>() : NULL;
					if (DefaultGame)
					{
						//TODO: This should probably be based on the actual gamemode ELO if there is specifics, average if not
						/*int32 Badge, Level;
						bool bEloIsValid = false;
						int32 EloRating = DefaultGame->GetEloFor(PS, bEloIsValid);
						UUTLocalPlayer::GetBadgeFromELO(EloRating, bEloIsValid, Badge, Level);
						Badge = FMath::Clamp<int32>(Badge, 0, 3);
						Level = FMath::Clamp<int32>(Level, 0, 8);

						Canvas->SetLinearDrawColor(FLinearColor::White);
						Canvas->DrawTile(ScoreboardAtlas, StartX + Border, StartY + Border, BadgeSize, BadgeSize, BadgeUVs[Badge].X, BadgeUVs[Badge].Y, 32, 32);
						Canvas->DrawTile(ScoreboardAtlas, StartX + Border, StartY + Border, BadgeSize, BadgeSize, BadgeNumberUVs[Level].X, BadgeNumberUVs[Level].Y, 32, 32);
						*/
					}

					FUTCanvasTextItem TextItem(FVector2D(StartX + BadgeSize + (Border * 2.0f), StartY + Border + 7.0f - (TextYL * 0.5f)), FText::FromString(PS->PlayerName), TinyFont, FLinearColor::White, NULL);
					TextItem.Scale = FVector2D(Scale, Scale);
					TextItem.BlendMode = SE_BLEND_Translucent;
					TextItem.FontRenderInfo = Canvas->CreateFontRenderInfo(true, false);
					TextItem.ShadowOffset = FVector2D(2.0f, 2.0f);
					Canvas->DrawItem(TextItem);
				}
			}
		}
	}
}

void AUTTrophyRoom::PositionPlayers()
{
	//Sort the players states
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL)
	{
		for (int32 i = 0; i < PlayerStates.Num() - 1; i++)
		{
			AUTPlayerState* P1 = Cast<AUTPlayerState>(PlayerStates[i]);
			for (int32 j = i + 1; j < PlayerStates.Num(); j++)
			{
				AUTPlayerState* P2 = Cast<AUTPlayerState>(PlayerStates[j]);
				if (!GS->InOrder(P1, P2))
				{
					PlayerStates[i] = P2;
					PlayerStates[j] = P1;
					P1 = P2;
				}
			}
		}
	}
	//Counters for iterating each team
	TArray<int32> TeamCount;
	TeamCount.AddZeroed(TeamGroups.Num());

	//Place the characters on the desired trophy starts
	for (AUTPlayerState* PS : PlayerStates)
	{
		AUTCharacter* Character = FindCharacter(PS);
		if (Character != nullptr)
		{
			//Find the TeamGroups for this characters team.
			int32 GroupIndex = INDEX_NONE;
			for (int32 iTeam = 0; iTeam < TeamGroups.Num(); iTeam++)
			{
				if (TeamGroups[iTeam].Team == PS->GetTeamNum())
				{
					TeamCount[iTeam]++;
					GroupIndex = iTeam;
					break;
				}
			}

			if (GroupIndex != INDEX_NONE)
			{
				FTransform Spot;
				if (FindSpot(Character->GetTeamNum(), TeamCount[GroupIndex] - 1, Spot))
				{
					Character->HideCharacter(false);
					Character->SetActorTransform(Spot * ActorToWorld());
					continue;
				}
			}
			Character->HideCharacter(true);
		}
	}
}

static int32 WeaponIndex = 0;

AUTCharacter* AUTTrophyRoom::RecreatePlayerPreview(AUTPlayerState* NewPS/*, FVector Location, FRotator Rotation*/)
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TSubclassOf<class APawn> DefaultPawnClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *GetDefault<AUTGameMode>()->PlayerPawnObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));

	AUTCharacter* PlayerPreviewMesh = GetWorld()->SpawnActor<AUTCharacter>(DefaultPawnClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (PlayerPreviewMesh)
	{
		// We need to get our tick functions registered, this seemed like best way to do it
		PlayerPreviewMesh->RegisterAllActorTickFunctions(true, true);

		PlayerPreviewMesh->PlayerState = NewPS; //PS needed for team colors
		PlayerPreviewMesh->Health = 100; //Set to 100 so the TacCom Overlay doesn't show damage
		PlayerPreviewMesh->DeactivateSpawnProtection();

		PlayerPreviewMesh->ApplyCharacterData(NewPS->GetSelectedCharacter());
		PlayerPreviewMesh->NotifyTeamChanged();

		PlayerPreviewMesh->SetHatClass(NewPS->HatClass);
		PlayerPreviewMesh->SetHatVariant(NewPS->HatVariant);
		PlayerPreviewMesh->SetEyewearClass(NewPS->EyewearClass);
		PlayerPreviewMesh->SetEyewearVariant(NewPS->EyewearVariant);

		int32 WeaponIndexToSpawn = 0;
		UClass* PreviewAttachmentType = NewPS->FavoriteWeapon ? NewPS->FavoriteWeapon->GetDefaultObject<AUTWeapon>()->AttachmentType : NULL;
		if (!PreviewAttachmentType)
		{
			UClass* PreviewAttachments[6];
			PreviewAttachments[0] = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/LinkGun/BP_LinkGun_Attach.BP_LinkGun_Attach_C"), NULL, LOAD_None, NULL);
			PreviewAttachments[1] = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/Sniper/BP_Sniper_Attach.BP_Sniper_Attach_C"), NULL, LOAD_None, NULL);
			PreviewAttachments[2] = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/RocketLauncher/BP_Rocket_Attachment.BP_Rocket_Attachment_C"), NULL, LOAD_None, NULL);
			PreviewAttachments[3] = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/ShockRifle/ShockAttachment.ShockAttachment_C"), NULL, LOAD_None, NULL);
			PreviewAttachments[4] = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/Flak/BP_Flak_Attach.BP_Flak_Attach_C"), NULL, LOAD_None, NULL);
			PreviewAttachments[5] = PreviewAttachments[3];
			WeaponIndexToSpawn = WeaponIndex % 6;
			PreviewAttachmentType = PreviewAttachments[WeaponIndexToSpawn];
			WeaponIndex++;
		}
		if (PreviewAttachmentType != NULL)
		{
			FActorSpawnParameters WeapSpawnParams;
			WeapSpawnParams.Instigator = PlayerPreviewMesh;
			AUTWeaponAttachment* PreviewWeapon = GetWorld()->SpawnActor<AUTWeaponAttachment>(PreviewAttachmentType, FVector(0, 0, 0), FRotator(0, 0, 0), WeapSpawnParams);
			if (PreviewWeapon)
			{
				PreviewWeapon->AttachToOwner();
				PreviewWeapons.Add(PreviewWeapon);
			}
		}

		UAnimationAsset* PlayerPreviewAnim;
		if (NewPS->IsFemale())
		{
			switch (WeaponIndexToSpawn)
			{
			case 1:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPoseFemale_Sniper.MatchPoseFemale_Sniper"));
				break;
			case 2:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPoseFemale_Flak_B.MatchPoseFemale_Flak_B"));
				break;
			case 4:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPoseFemale_Flak.MatchPoseFemale_Flak"));
				break;
			case 0:
			case 3:
			case 5:
			default:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPoseFemale_ShockRifle.MatchPoseFemale_ShockRifle"));
			}
		}
		else
		{
			switch (WeaponIndexToSpawn)
			{
			case 1:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPose_Sniper.MatchPose_Sniper"));
				break;
			case 2:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPose_Flak_B.MatchPose_Flak_B"));
				break;
			case 4:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPose_Flak.MatchPose_Flak"));
				break;
			case 0:
			case 3:
			case 5:
			default:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPose_ShockRifle.MatchPose_ShockRifle"));
			}
		}

		PreviewAnimations.AddUnique(PlayerPreviewAnim);

		PlayerPreviewMesh->GetMesh()->PlayAnimation(PlayerPreviewAnim, true);
		PlayerPreviewMesh->GetMesh()->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;

		Characters.Add(PlayerPreviewMesh);
		return PlayerPreviewMesh;
	}
	return nullptr;
}


void AUTTrophyRoom::GetTeamCameraTransform(uint8 Team, FTransform& Transform)
{
	int32 NumTeams = AUTTrophyRoom::GetNumTeamsFromType(TrophyType);

	if (NumTeams == 2)
	{
		if (Team == 0)
		{
			Transform = CameraTeamRed->GetComponentTransform();
		}
		else if(Team == 1)
		{
			Transform = CameraTeamBlue->GetComponentTransform();
		}
		else
		{
			GetAllCameraTransform(Transform);
		}
	}
	else
	{
		GetAllCameraTransform(Transform);
	}
}

void AUTTrophyRoom::GetAllCameraTransform(FTransform& Transform)
{
	Transform = CameraComponent->GetComponentTransform();
}

#if WITH_EDITOR
void AUTTrophyRoom::PreEditChange(UProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);
}
void AUTTrophyRoom::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AUTTrophyRoom, bSnapToFloor) && bSnapToFloor)
	{
		SnapToFloor();
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AUTTrophyRoom, TrophyType))
	{
		//Hide the team cameras to make things easier for map authors
		int32 TeamNum = AUTTrophyRoom::GetNumTeamsFromType(TrophyType);
		if (GetNumTeamsFromType(TrophyType) == 2)
		{
			CameraTeamRed->SetWorldScale3D(FVector(1.0f));
			CameraTeamBlue->SetWorldScale3D(FVector(1.0f));
		}
		else
		{
			CameraTeamRed->SetWorldScale3D(FVector(0.0f));
			CameraTeamBlue->SetWorldScale3D(FVector(0.0f));
		}

		SetupGroups();
	}

#if WITH_EDITORONLY_DATA
	if (TrophyRenderComp != nullptr)
	{
		TrophyRenderComp->MarkRenderStateDirty();
	}
#endif
}
void AUTTrophyRoom::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (bSnapToFloor)
	{
		SnapToFloor();
	}
}
#endif

void AUTTrophyRoom::SnapToFloor()
{
	static const FName NAME_FreeCam = FName(TEXT("FreeCam"));
	FCollisionQueryParams BoxParams(NAME_FreeCam, false, this);

	for (int32 iTeam = 0; iTeam < TeamGroups.Num(); iTeam++)
	{
		FTrophyGroup& Group = TeamGroups[iTeam];

		for (int32 iPlayer = 0; iPlayer < MAX_TROPHY_PLAYERS; iPlayer++)
		{
			FVector PlrLocation = (Group.Players[iPlayer] * ActorToWorld()).GetLocation();

			FVector Start(PlrLocation.X, PlrLocation.Y , GetActorLocation().Z + 500.0f);
			FVector End(PlrLocation.X, PlrLocation.Y , GetActorLocation().Z - 100000.0f);

			FHitResult Hit;
			GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, COLLISION_TRACE_WEAPON, FCollisionShape::MakeBox(FVector(12.f)), BoxParams);

			if (Hit.bBlockingHit)
			{
				FVector NewLocation = Group.Players[iPlayer].GetLocation();
				NewLocation.Z = Hit.Location.Z - GetActorLocation().Z + 80.0f;
				Group.Players[iPlayer].SetLocation(NewLocation);
			}
		}
	}

	CenterPivot();
}

void AUTTrophyRoom::CenterPivot()
{
	int32 PivotCount = 0;
	FVector PivotSum(0.0f);

	for (FTrophyGroup& Group : TeamGroups)
	{
		for (FTransform& Player : Group.Players)
		{
			PivotSum += Player.GetLocation();
			PivotCount++;
		}
	}
	CameraPivot = PivotSum / PivotCount;
}

AUTTrophyRoom* AUTTrophyRoom::GetTrophyRoom(UObject* WorldContextObject, ETrophyType::Type WantedType)
{
	if (WantedType == ETrophyType::None)
	{
		return nullptr;
	}

	//Try to find the matching trophy room
	TArray<AUTTrophyRoom*> TrophyRooms;
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	for (TActorIterator<AUTTrophyRoom> It(World); It; ++It)
	{
		AUTTrophyRoom* TrophyRoom = *It;
		if (WantedType == TrophyRoom->TrophyType)
		{
			return TrophyRoom;
		}
		TrophyRooms.Add(TrophyRoom);
	}

	//try to find the next best Type if there are other trophyrooms
	if (TrophyRooms.Num() > 0)
	{
		int32 WantedTeamNum = AUTTrophyRoom::GetNumTeamsFromType(WantedType);

		TrophyRooms.Sort([WantedTeamNum](const AUTTrophyRoom& A, const AUTTrophyRoom& B)
		{
			int32 RatingA = WantedTeamNum == A.TeamGroups.Num() ? 1 : 0;
			int32 RatingB = WantedTeamNum == B.TeamGroups.Num() ? 1 : 0;
			return RatingA > RatingB;
		});

		return TrophyRooms[0];
	}
	
	//try to find a good location from powerups
	TArray<AActor*> PossibleActors;
	for (TActorIterator<AUTGameObjective> It(World); It; ++It)
	{
		PossibleActors.Add(*It);
	}
	for (TActorIterator<AUTPickupInventory> It(World); It; ++It)
	{
		PossibleActors.Add(*It);
	}

	FVector SpawnLocation;
	FRotator SpawnRotation;

	//TODOTIM: Make this better
	if (PossibleActors.Num() > 0)
	{
		float BestRating = 0.0f;
		AActor* BestActor = nullptr;
		const FName NAME_Trophy = FName(TEXT("Trophy"));
		FCollisionQueryParams BoxParams(NAME_Trophy, false, nullptr);

		for (AActor* Actor : PossibleActors)
		{
			float MaxTrace = 30000.0f;
			FVector Start = Actor->GetActorLocation();
			Start.Z += 50.0f;
			FVector Dir(MaxTrace, 0.0f, 0.0f);

			//Trace in 8 directions around the possible actor
			float Dists[8];
			for (int i = 0; i < 8; i++)
			{
				float Angle = i * 45.0f * PI / 180.f;
				FVector End(Start.X + (Dir.X * FMath::Cos(Angle) + Dir.X * -FMath::Sin(Angle)),
					Start.Y + (Dir.X * FMath::Sin(Angle) + Dir.X * FMath::Cos(Angle)),
					Start.Z);

				FHitResult Hit;
				if (World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, COLLISION_TRACE_WEAPON, FCollisionShape::MakeBox(FVector(12.f)), BoxParams))
				{
					Dists[i] = FVector::Dist(Hit.ImpactPoint, Start);
					DrawDebugLine(World, Start, Hit.ImpactPoint, FColor(0, 255, 0), true);
				}
				else
				{
					DrawDebugLine(World, Start, End, FColor(0, 255, 0), true);
					Dists[i] = MaxTrace;
				}
			}

			//figure out what has the best distances. Farthest back distance should provide a better background to look at
			for (int i = 0; i < 8; i++)
			{
				float FrontDist = Dists[i];
				float LeftDist = Dists[(i + 2) % 8];
				float BackDist = Dists[(i + 4) % 8];
				float RightDist = Dists[(i + 6) % 8];
				if (LeftDist > 1000.0f && RightDist > 1000.0f)
				{
					float Rating = BackDist * 5.0f + FrontDist * 2.0f + LeftDist + RightDist;
					if (Rating > BestRating)
					{
						BestActor = Actor;
						SpawnRotation = FRotator(0.0f, i * 45.0f + 225.0f, 0.0f);
						SpawnLocation = Actor->GetActorLocation();
						BestRating = Rating;
						DrawDebugLine(World, SpawnLocation, SpawnRotation.Vector() * 5000.0f + SpawnLocation, FColor(255, 0, 255), true);
					}
				}
			}
		}

		//Couldn't find a good pickup so just pick a random one for now
		if (BestActor == nullptr)
		{
			SpawnLocation = PossibleActors[0]->GetActorLocation();
			SpawnRotation = PossibleActors[0]->GetActorRotation();
		}
		else
		{
			DrawDebugBox(World, BestActor->GetActorLocation(), FVector(50.0f), FColor(255, 0, 0), true);
			DrawDebugLine(World, SpawnLocation, SpawnRotation.Vector() * 5000.0f + SpawnLocation, FColor(255, 0, 0), true);
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AUTTrophyRoom* TrophyRoom = World->SpawnActor<AUTTrophyRoom>(AUTTrophyRoom::StaticClass(), SpawnLocation, SpawnRotation, Params);

	AUTGameState* GS = Cast<AUTGameState>(World->GameState);
	if (TrophyRoom != nullptr && GS != nullptr)
	{
		TrophyRoom->TrophyType = WantedType;
		//TrophyRoom->SnapToFloor();
	}
	return TrophyRoom;
}

int32 AUTTrophyRoom::GetNumTeamsFromType(ETrophyType::Type WantedType)
{
	switch (WantedType)
	{
	case ETrophyType::Team_Intro:
	case ETrophyType::Team_Intermission:
	case ETrophyType::Team_PostMatch:
	case ETrophyType::Team_PostMatch_RedWin:
	case ETrophyType::Team_PostMatch_BlueWin:
		return 2;
	case ETrophyType::FFA_Intro:
	case ETrophyType::FFA_PostMatch:
	case ETrophyType::FFA_Intermission:
	default:
		return 1;
	}
}