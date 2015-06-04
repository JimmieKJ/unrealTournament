// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SUWPlayerInfoDialog.h"
#include "SUWindowsStyle.h"
#include "../Public/UTCanvasRenderTarget2D.h"
#include "EngineModule.h"
#include "SlateMaterialBrush.h"
#include "../Public/UTPlayerCameraManager.h"
#include "UTCharacterContent.h"
#include "UTWeap_ShockRifle.h"
#include "UTWeaponAttachment.h"
#include "Engine/UserInterfaceSettings.h"

#if !UE_SERVER

#include "SScaleBox.h"
#include "Widgets/SDragImage.h"

void SUWPlayerInfoDialog::Construct(const FArguments& InArgs)
{
	FVector2D ViewportSize;
	InArgs._PlayerOwner->ViewportClient->GetViewportSize(ViewportSize);
	FText DialogTitle = FText::Format(NSLOCTEXT("SUWindowsDesktop", "PlayerInfoTitleFormat", "Player Info - {0}"), FText::FromString(InArgs._TargetPlayerState->PlayerName));
	SUWDialog::Construct(SUWDialog::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(DialogTitle)
							.DialogSize(InArgs._DialogSize)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.ContentPadding(FVector2D(0,0))
							.IsScrollable(false)
							.ButtonMask(UTDIALOG_BUTTON_OK)
							.OnDialogResult(InArgs._OnDialogResult)
						);

	TargetPlayerState = InArgs._TargetPlayerState;
	if (TargetPlayerState.IsValid()) 
	{
		TargetUniqueId = TargetPlayerState->UniqueId.GetUniqueNetId();
	}

	PlayerPreviewMesh = nullptr;
	PreviewWeapon = nullptr;
	bSpinPlayer = true;
	ZoomOffset = -50;


	PoseAnimation = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/Pose_E.Pose_E"));

	// allocate a preview scene for rendering
	PlayerPreviewWorld = UWorld::CreateWorld(EWorldType::Preview, true);
	PlayerPreviewWorld->bHack_Force_UsesGameHiddenFlags_True = true;
	GEngine->CreateNewWorldContext(EWorldType::Preview).SetCurrentWorld(PlayerPreviewWorld);
	PlayerPreviewWorld->InitializeActorsForPlay(FURL(), true);
	ViewState.Allocate();
	{
		UClass* EnvironmentClass = LoadObject<UClass>(nullptr, TEXT("/Game/RestrictedAssets/UI/PlayerPreviewEnvironment.PlayerPreviewEnvironment_C"));
		PreviewEnvironment = PlayerPreviewWorld->SpawnActor<AActor>(EnvironmentClass, FVector(500.f, 50.f, 0.f), FRotator(0, 0, 0));
	}
	
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(NULL, TEXT("/Game/RestrictedAssets/UI/PlayerPreviewProxy.PlayerPreviewProxy"));
	if (BaseMat != NULL)
	{
		PlayerPreviewTexture = Cast<UUTCanvasRenderTarget2D>(UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetPlayerOwner().Get(), UUTCanvasRenderTarget2D::StaticClass(), ViewportSize.X, ViewportSize.Y));
		PlayerPreviewTexture->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		PlayerPreviewTexture->OnNonUObjectRenderTargetUpdate.BindSP(this, &SUWPlayerInfoDialog::UpdatePlayerRender);
		PlayerPreviewMID = UMaterialInstanceDynamic::Create(BaseMat, PlayerPreviewWorld);
		PlayerPreviewMID->SetTextureParameterValue(FName(TEXT("TheTexture")), PlayerPreviewTexture);
		PlayerPreviewBrush = new FSlateMaterialBrush(*PlayerPreviewMID, ViewportSize);
	}
	else
	{
		PlayerPreviewTexture = NULL;
		PlayerPreviewMID = NULL;
		PlayerPreviewBrush = new FSlateMaterialBrush(*UMaterial::GetDefaultMaterial(MD_Surface), ViewportSize);
	}

	PlayerPreviewTexture->TargetGamma = GEngine->GetDisplayGamma();
	PlayerPreviewTexture->InitCustomFormat(ViewportSize.X, ViewportSize.Y, PF_B8G8R8A8, false);
	PlayerPreviewTexture->UpdateResourceImmediate();

	FVector2D ResolutionScale(ViewportSize.X / 1280.0f, ViewportSize.Y / 720.0f);

	if (DialogContent.IsValid())
	{
		const float MessageTextPaddingX = 10.0f;
		TSharedPtr<STextBlock> MessageTextBlock;
		DialogContent->AddSlot()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(350)
				[
					SNew(SScaleBox)
					.Stretch(EStretch::ScaleToFill)
					[
						SNew(SDragImage)
						.Image(PlayerPreviewBrush)
						.OnDrag(this, &SUWPlayerInfoDialog::DragPlayerPreview)
						.OnZoom(this, &SUWPlayerInfoDialog::ZoomPlayerPreview)
					]
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.FillHeight(1.0)
				[
					SNew(SScrollBox)
					+SScrollBox::Slot()
					[
						SAssignNew(InfoPanel, SVerticalBox)
					]
				]
			]

		];
	}

	TargetPlayerState->BuildPlayerInfo(InfoPanel);

	InfoPanel->AddSlot()
	.HAlign(HAlign_Center)
	.Padding(0.0f,20.0f,0.0f,20.0f)
	[
		SNew(STextBlock)
		.Text(NSLOCTEXT("Generic", "CurrentStats", "- Current Stats -"))
		.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
		.ColorAndOpacity(FLinearColor::Gray)
	];
		
	AUTGameMode* DefaultGameMode = GetPlayerOwner()->GetWorld()->GetGameState()->GameModeClass->GetDefaultObject<AUTGameMode>();
	if (DefaultGameMode)
	{
		DefaultGameMode->BuildPlayerInfo(InfoPanel, TargetPlayerState.Get());
	}

	FriendStatus = NAME_None;
	RecreatePlayerPreview();
}

SUWPlayerInfoDialog::~SUWPlayerInfoDialog()
{
	if (PlayerPreviewTexture != NULL)
	{
		PlayerPreviewTexture->OnNonUObjectRenderTargetUpdate.Unbind();
		PlayerPreviewTexture = NULL;
	}
	FlushRenderingCommands();
	if (PlayerPreviewBrush != NULL)
	{
		// FIXME: Slate will corrupt memory if this is deleted. Must be referencing it somewhere that doesn't get cleaned up...
		//		for now, we'll take the minor memory leak (the texture still gets GC'ed so it's not too bad)
		//delete PlayerPreviewBrush;
		PlayerPreviewBrush->SetResourceObject(NULL);
		PlayerPreviewBrush = NULL;
	}
	if (PlayerPreviewWorld != NULL)
	{
		PlayerPreviewWorld->DestroyWorld(true);
		GEngine->DestroyWorldContext(PlayerPreviewWorld);
		PlayerPreviewWorld = NULL;
	}
	ViewState.Destroy();
}

void SUWPlayerInfoDialog::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PlayerPreviewTexture);
	Collector.AddReferencedObject(PlayerPreviewMID);
	Collector.AddReferencedObject(PlayerPreviewWorld);
	Collector.AddReferencedObject(PoseAnimation);
}

FReply SUWPlayerInfoDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK) 
	{
		GetPlayerOwner()->CloseDialog(SharedThis(this));	
	}
	return FReply::Handled();
}

void SUWPlayerInfoDialog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SUWDialog::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (PlayerPreviewWorld != nullptr)
	{
		PlayerPreviewWorld->Tick(LEVELTICK_All, InDeltaTime);
	}

	if ( PlayerPreviewTexture != nullptr )
	{
		PlayerPreviewTexture->UpdateResource();
	}

	BuildFriendPanel();

}

void SUWPlayerInfoDialog::RecreatePlayerPreview()
{
	FRotator ActorRotation = FRotator(0.0f, 180.0f, 0.0f);

	if (!TargetPlayerState.IsValid()) return;

	if (PlayerPreviewMesh != nullptr)
	{
		ActorRotation = PlayerPreviewMesh->GetActorRotation();
		PlayerPreviewMesh->Destroy();
	}

	if ( PreviewWeapon != nullptr )
	{
		PreviewWeapon->Destroy();
	}

	AUTGameMode* DefaultGameMode = GetPlayerOwner()->GetWorld()->GetGameState()->GameModeClass->GetDefaultObject<AUTGameMode>();
	if (DefaultGameMode)
	{
		PlayerPreviewMesh = PlayerPreviewWorld->SpawnActor<AUTCharacter>(DefaultGameMode->DefaultPawnClass, FVector(300.0f, 0.f, 4.f), ActorRotation);
		if (PlayerPreviewMesh)
		{
			PlayerPreviewMesh->ApplyCharacterData(TargetPlayerState->GetSelectedCharacter());
			PlayerPreviewMesh->SetHatClass(TargetPlayerState->HatClass);
			PlayerPreviewMesh->SetHatVariant(TargetPlayerState->HatVariant);
			PlayerPreviewMesh->SetEyewearClass(TargetPlayerState->EyewearClass);
			PlayerPreviewMesh->SetEyewearVariant(TargetPlayerState->EyewearVariant);

			if ( PoseAnimation )
			{
				PlayerPreviewMesh->GetMesh()->PlayAnimation(PoseAnimation, true);
				PlayerPreviewMesh->GetMesh()->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
			}

			UClass* PreviewAttachmentType = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/ShockRifle/ShockAttachment.ShockAttachment_C"), NULL, LOAD_None, NULL);
			if (PreviewAttachmentType != NULL)
			{
				PreviewWeapon = PlayerPreviewWorld->SpawnActor<AUTWeaponAttachment>(PreviewAttachmentType, FVector(0, 0, 0), FRotator(0, 0, 0));
				PreviewWeapon->Instigator = PlayerPreviewMesh;
			}

			// Tick the world to make sure the animation is up to date.
			if ( PlayerPreviewWorld != nullptr )
			{
				PlayerPreviewWorld->Tick(LEVELTICK_All, 0.0);
			}

			if ( PreviewWeapon )
			{
				PreviewWeapon->BeginPlay();
				PreviewWeapon->AttachToOwner();
			}
		}
		else
		{
			UE_LOG(UT,Log,TEXT("Could not spawn the player's mesh (DefaultPawnClass = %s"), *DefaultGameMode->DefaultPawnClass->GetFullName());
		}
	}
}


void SUWPlayerInfoDialog::UpdatePlayerRender(UCanvas* C, int32 Width, int32 Height)
{
	FEngineShowFlags ShowFlags(ESFIM_Game);
	ShowFlags.SetMotionBlur(false);
	ShowFlags.SetGrain(false);
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(PlayerPreviewTexture->GameThread_GetRenderTargetResource(), PlayerPreviewWorld->Scene, ShowFlags).SetRealtimeUpdate(true));

	FVector CameraPosition(ZoomOffset, 0, -60);

	const float FOV = 45;
	const float AspectRatio = Width / (float)Height;

	FSceneViewInitOptions PlayerPreviewInitOptions;
	PlayerPreviewInitOptions.SetViewRectangle(FIntRect(0, 0, C->SizeX, C->SizeY));
	PlayerPreviewInitOptions.ViewOrigin = -CameraPosition;
	PlayerPreviewInitOptions.ViewRotationMatrix = FMatrix(FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
	PlayerPreviewInitOptions.ProjectionMatrix = 
		FReversedZPerspectiveMatrix(
			FMath::Max(0.001f, FOV) * (float)PI / 360.0f,
			AspectRatio,
			1.0f,
			GNearClippingPlane );
	PlayerPreviewInitOptions.ViewFamily = &ViewFamily;
	PlayerPreviewInitOptions.SceneViewStateInterface = ViewState.GetReference();
	PlayerPreviewInitOptions.BackgroundColor = FLinearColor::Black;
	PlayerPreviewInitOptions.WorldToMetersScale = GetPlayerOwner()->GetWorld()->GetWorldSettings()->WorldToMeters;
	PlayerPreviewInitOptions.CursorPos = FIntPoint(-1, -1);
	
	ViewFamily.bUseSeparateRenderTarget = true;

	FSceneView* View = new FSceneView(PlayerPreviewInitOptions); // note: renderer gets ownership
	View->ViewLocation = FVector::ZeroVector;
	View->ViewRotation = FRotator::ZeroRotator;
	FPostProcessSettings PPSettings = GetDefault<AUTPlayerCameraManager>()->DefaultPPSettings;

	ViewFamily.Views.Add(View);

	View->StartFinalPostprocessSettings(CameraPosition);
	View->EndFinalPostprocessSettings(PlayerPreviewInitOptions);

	// workaround for hacky renderer code that uses GFrameNumber to decide whether to resize render targets
	--GFrameNumber;
	GetRendererModule().BeginRenderingViewFamily(C->Canvas, &ViewFamily);
}

void SUWPlayerInfoDialog::DragPlayerPreview(const FVector2D MouseDelta)
{
	if (PlayerPreviewMesh != nullptr)
	{
		bSpinPlayer = false;
		PlayerPreviewMesh->SetActorRotation(PlayerPreviewMesh->GetActorRotation() + FRotator(0, 0.2f * -MouseDelta.X, 0.0f));
	}
}

void SUWPlayerInfoDialog::ZoomPlayerPreview(float WheelDelta)
{
	ZoomOffset = FMath::Clamp(ZoomOffset + (-WheelDelta * 10.0f), -100.0f, 400.0f);
}

TSharedRef<class SWidget> SUWPlayerInfoDialog::BuildCustomButtonBar()
{
	TSharedPtr<SHorizontalBox> CustomBox;
	SAssignNew(CustomBox, SHorizontalBox);
	CustomBox->AddSlot()
	.AutoWidth()
	[
		SAssignNew(FriendPanel, SHorizontalBox)
	];

	if (GetPlayerOwner()->GetWorld()->GetNetMode() == NM_Client)
	{
		CustomBox->AddSlot()
		.Padding(10.0f,0.0f,10.0f,0.0f)
		[
			SAssignNew(KickButton, SButton)
			.HAlign(HAlign_Center)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
			.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
			.Text(NSLOCTEXT("SUWPlayerInfoDialog","KickVote","Vote to Kick"))
			.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
			.OnClicked(this, &SUWPlayerInfoDialog::KickVote)
		];
	}


	return CustomBox.ToSharedRef();
}

FReply SUWPlayerInfoDialog::OnSendFriendRequest()
{
	if (FriendStatus != FFriendsStatus::FriendRequestPending && FriendStatus != FFriendsStatus::IsBot)
	{
		GetPlayerOwner()->RequestFriendship(TargetUniqueId);

		FriendPanel->ClearChildren();
		FriendPanel->AddSlot()
			.Padding(10.0, 0.0, 0.0, 0.0)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUWPlayerInfoDialog", "FriendRequestPending", "You have sent a friend request..."))
				.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
			];

		FriendStatus = FFriendsStatus::FriendRequestPending;
	}
	return FReply::Handled();
}

FText SUWPlayerInfoDialog::GetFunnyText()
{
	float Rnd = FMath::FRand();
	if (Rnd < 0.2) return NSLOCTEXT("SUWPlayerInfoDialog", "FunnyOne", "Narcissistic Much?");
	if (Rnd < 0.4) return NSLOCTEXT("SUWPlayerInfoDialog", "Funnytwo", "Mirror Mirror...");

	return NSLOCTEXT("SUWPlayerInfoDialog", "FunnyDefault", "Viewing Yourself!");
}

void SUWPlayerInfoDialog::BuildFriendPanel()
{
	if (TargetPlayerState == NULL) return;

	FName NewFriendStatus;
	if (GetPlayerOwner()->PlayerController->PlayerState == TargetPlayerState)
	{
		NewFriendStatus = FFriendsStatus::IsYou;
	}
	else if (TargetPlayerState->bIsABot)
	{
		NewFriendStatus = FFriendsStatus::IsBot;
	}
	else
	{
		NewFriendStatus = GetPlayerOwner()->IsAFriend(TargetPlayerState->UniqueId) ? FFriendsStatus::Friend : FFriendsStatus::NotAFriend;
	}

	bool bRequiresRefresh = false;
	if (FriendStatus == FFriendsStatus::FriendRequestPending)
	{
		if (NewFriendStatus == FFriendsStatus::Friend)
		{
			FriendStatus = NewFriendStatus;
			bRequiresRefresh = true;
		}
	}
	else
	{
		bRequiresRefresh = FriendStatus != NewFriendStatus;
		FriendStatus = NewFriendStatus;
	}

	if (bRequiresRefresh)
	{
		FriendPanel->ClearChildren();
		if (FriendStatus == FFriendsStatus::IsYou)
		{
			FText FunnyText = GetFunnyText();
			FriendPanel->AddSlot()
			.Padding(10.0, 0.0, 0.0, 0.0)
				[
					SNew(STextBlock)
					.Text(FunnyText)
					.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
				];
		}
		else if (FriendStatus == FFriendsStatus::IsBot)
		{
			FriendPanel->AddSlot()
				.Padding(10.0, 0.0, 0.0, 0.0)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUWPlayerInfoDialog", "IsABot", "AI (C) Liandri Corp."))
					.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
				];
		}
		else if (FriendStatus == FFriendsStatus::Friend)
		{
			FriendPanel->AddSlot()
				.Padding(10.0, 0.0, 0.0, 0.0)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUWPlayerInfoDialog", "IsAFriend", "Is your friend"))
					.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
				];
		}
		else if (FriendStatus == FFriendsStatus::FriendRequestPending)
		{
		}
		else
		{
			FriendPanel->AddSlot()
				.Padding(10.0, 0.0, 0.0, 0.0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
					.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
					.Text(NSLOCTEXT("SUWPlayerInfoDialog", "SendFriendRequest", "Send Friend Request"))
					.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
					.OnClicked(this, &SUWPlayerInfoDialog::OnSendFriendRequest)
				];
		}
	}

}

FReply SUWPlayerInfoDialog::KickVote()
{
	if (TargetPlayerState.IsValid()) 
	{
		AUTPlayerState* MyPlayerState =  Cast<AUTPlayerState>(GetPlayerOwner()->PlayerController->PlayerState);
		if (TargetPlayerState != MyPlayerState)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
			if (PC)
			{
				PC->ServerRegisterBanVote(TargetPlayerState.Get());
				KickButton->SetEnabled(false);
			}
		}
	}

	return FReply::Handled();
}

#endif