// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "SUTWeaponConfigDialog.h"
#include "SUTWeaponPriorityDialog.h"
#include "SUTWeaponWheelConfigDialog.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "../SUTUtils.h"
#include "../Widgets/SUTTabWidget.h"
#include "UTCrosshair.h"
#include "UTCanvasRenderTarget2D.h"
#include "EngineModule.h"
#include "SlateMaterialBrush.h"
#include "SNumericEntryBox.h"
#include "UTPlayerCameraManager.h"
#include "Engine/UserInterfaceSettings.h"
#include "../Widgets/SUTButton.h"
#include "AssetData.h"
#include "SScaleBox.h"
#include "Widgets/SDragImage.h"
#include "../Widgets/SUTBorder.h"
#include "UTCrosshair.h"
#include "UTWeap_Translocator.h"

#if !UE_SERVER
#include "Widgets/SUTColorPicker.h"

const float CAMERA_RETURN_WAIT_TIME=5.0f;
const float CAMERA_SPEED = 128.0f;
const float PREVIEW_Y_SPACING = 256.0f;
const float NEXT_PREV_ANIM_TIME = 2.75f;

void SUTWeaponConfigDialog::Construct(const FArguments& InArgs)
{
	DefaultPreviewCameraLocation = FVector(220.0f, 0.0f, -16.0f);

	SUTDialogBase::Construct(SUTDialogBase::FArguments()
		.PlayerOwner(InArgs._PlayerOwner)
		.DialogTitle(NSLOCTEXT("SUTMenuBase", "WeaponSettings", "Weapon Settings"))
		.DialogSize(InArgs._DialogSize)
		.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
		.DialogPosition(InArgs._DialogPosition)
		.DialogAnchorPoint(InArgs._DialogAnchorPoint)
		.ContentPadding(InArgs._ContentPadding)
		.ButtonMask(UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_CANCEL)
		.IsScrollable(false)
		.OnDialogResult(InArgs._OnDialogResult)
		);

	FVector2D ViewportSize;
	InArgs._PlayerOwner->ViewportClient->GetViewportSize(ViewportSize);
	ViewportSize = FVector2D(1160.0f, 902.0f);

	bShowingCrosshairs = false;

	PreviewActorRotation = FRotator(0.0f, 0.0f, 0.0f);

	UUTProfileSettings* ProfileSettings = GetPlayerOwner()->GetProfileSettings();
	ConstructPreviewWorld(ViewportSize);

	GatherCrosshairs(ProfileSettings);
	GatherWeaponData(ProfileSettings);
	GatherWeaponSkins(ProfileSettings);

	bRequiresSave = false;

	bCustomWeaponCrosshairs = ProfileSettings->bCustomWeaponCrosshairs;
	bSingleCustomWeaponCrosshair = ProfileSettings->bSingleCustomWeaponCrosshair;

	if (DialogContent.IsValid())
	{
		const float MessageTextPaddingX = 10.0f;
		TSharedPtr<STextBlock> MessageTextBlock;
		DialogContent->AddSlot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(10.0f, 10.0f, 10.0f, 10.0f)
				[
					SNew(SBox).WidthOverride(700).HeightOverride(902)
					[
						SNew(SUTBorder)
						.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SUTBorder)
								.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Medium"))
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f,5.0f,0.0f,5.0f).HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large")
										.Text(NSLOCTEXT("SUTWeaponConfigDialog", "Options", "Weapon Options"))
									]

									+ SVerticalBox::Slot().AutoHeight().Padding(5.0f, 5.0f, 5.0f, 5.0f).HAlign(HAlign_Fill)
									[
										GenerateCustomWeaponCrosshairs(ProfileSettings)
									]

									+SVerticalBox::Slot().AutoHeight().Padding(5.0f,5.0f,5.0f,5.0f).HAlign(HAlign_Fill)
									[
										GenerateAutoSwitch(ProfileSettings)		
									]

									+ SVerticalBox::Slot().AutoHeight().Padding(5.0f,5.0f,5.0f,5.0f).HAlign(HAlign_Fill)
									[
										GenerateWeaponHand(ProfileSettings)
									]
								]
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f,15.0f,0.0f,10.0f).HAlign(HAlign_Center)
							[
								SNew(STextBlock)
								.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large")
								.Text(NSLOCTEXT("SUTWeaponConfigDialog", "AvailableWeapons", "Unreal Weapon Guide"))
							]
							+ SVerticalBox::Slot().FillHeight(1.0f)
							[
								SAssignNew(WeaponScrollBox, SScrollBox).Orientation(Orient_Vertical)
							]
						]
					]
				]

				+SHorizontalBox::Slot().FillWidth(1.0f).Padding(10.0f,10.0f,10.0f,10.0f)
				[
					SNew(SBox).WidthOverride(1160).HeightOverride(902)
					[
						SNew(SOverlay)
						+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
						[
							SNew(SDragImage)
							.Image(PreviewBrush)
								.OnDrag(this, &SUTWeaponConfigDialog::DragPreview)
								.OnZoom(this, &SUTWeaponConfigDialog::ZoomPreview)
						]
						+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Bottom)
						[
							SAssignNew(Page,SVerticalBox)
						]
						+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Top).Padding(40.0f, 20.0f, 40.0f,0.0f)
						[
							SNew(SBox).HeightOverride(128)
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot().AutoWidth()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot().AutoHeight()
									[
										SNew(SBox).HeightOverride(24)
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot().FillHeight(1.0f)
											[
												SNew(SHorizontalBox)
												+SHorizontalBox::Slot().FillWidth(1.0)
												[
													SNew(SBorder)
													.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
													[
														SNew(SVerticalBox)
														+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
														[
															SNew(STextBlock)
															.Text(NSLOCTEXT("SUTWeaponConfigDialog", "View", " View "))
															.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny.Bold.Gold")
														]
													]
												]
											]
										]
									]
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SHorizontalBox)
										+SHorizontalBox::Slot().AutoWidth()
										[
											SNew(SUTBorder)
											.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
											[
												SAssignNew(VariantsButton, SUTButton)
												.OnClicked(this, &SUTWeaponConfigDialog::OnModeChanged, 0)
												.ButtonStyle(SUTStyle::Get(), "UT.WeaponConfig.Button")
												.ContentPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
												.ClickMethod(EButtonClickMethod::MouseDown)
												.IsToggleButton(true)
												[
													SNew(STextBlock)
													.Text(NSLOCTEXT("SUTWeaponConfigDialog","Variants","Skins"))
													.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large")
												]
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(60.0f, 0.0f, 0.0f, 0.0f)
										[
											SNew(SUTBorder)
											.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
											[
												SAssignNew(CrosshairButton, SUTButton)
												.ContentPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
												.OnClicked(this, &SUTWeaponConfigDialog::OnModeChanged, 1)
												.IsToggleButton(true)
												.ClickMethod(EButtonClickMethod::MouseDown)
												.ButtonStyle(SUTStyle::Get(), "UT.WeaponConfig.Button")
												[
													SNew(STextBlock)
													.Text(NSLOCTEXT("SUTWeaponConfigDialog", "Crosshairs", "Crosshair"))
													.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large")
												]
											]
										]
									]
								]
							]
						]
						+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Bottom)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot().AutoHeight().Padding(0.0f,0.0f,0.0f,72.0f)
							[
								SNew(SOverlay)
								+SOverlay::Slot().HAlign(HAlign_Fill)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().HAlign(HAlign_Left).AutoWidth()
									[
										SNew(SBox).WidthOverride(128.0f).HeightOverride(128.0f)
										[
											SNew(SUTButton)
											.OnClicked(this, &SUTWeaponConfigDialog::HandleNextPrevious, -1)
											.ButtonStyle(SUTStyle::Get(), "UT.ArrowButton.Left")
											.Visibility(this, &SUTWeaponConfigDialog::LeftArrowVis)
										]
									]
									+ SHorizontalBox::Slot().HAlign(HAlign_Center).FillWidth(1.0)
									[
										SNew(SCanvas)
									]
									+ SHorizontalBox::Slot().HAlign(HAlign_Left).AutoWidth()
									[
										SNew(SBox).WidthOverride(128.0f).HeightOverride(128.0f)
										[
											SNew(SUTButton)
											.OnClicked(this, &SUTWeaponConfigDialog::HandleNextPrevious, 1)
											.ButtonStyle(SUTStyle::Get(), "UT.ArrowButton.Right")
											.Visibility(this, &SUTWeaponConfigDialog::RightArrowVis)
										]
									]
								]
								+ SOverlay::Slot().HAlign(HAlign_Fill)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Text(this, &SUTWeaponConfigDialog::GetVarientText)
										.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Huge")
										.ColorAndOpacity(FLinearColor(0.2f, 0.75f, 1.0f, 1.0f))
									]
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Text(this, &SUTWeaponConfigDialog::GetVarientSetText)
										.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
										.ColorAndOpacity(FLinearColor::Yellow)
									]
								]

							]
						]
					]
				]
			];
	}

	VariantsButton->BePressed();

	GenerateWeaponList(nullptr);
	GeneratePage();

	if (AllWeapons.Num() > 0)
	{
		CurrentWeaponIndex = -1;
		
		int32 LowestGroup = -1;
		int32 SelectedIndex = -1;
		for (int32 i=0; i < AllWeapons.Num(); i++)
		{
			int32 Group = AllWeapons[i].WeaponCustomizationInfo->WeaponGroup;
			if (SelectedIndex < 0 || Group < LowestGroup)
			{
				LowestGroup = Group;
				SelectedIndex = i;
			}
		}

		WeaponClicked(SelectedIndex < 0 ? 0 : SelectedIndex);
	}

}

TSharedRef<class SWidget> BuildCustomButtonBar();
TSharedRef<class SWidget> SUTWeaponConfigDialog::BuildCustomButtonBar()
{
	return SNew(SHorizontalBox)
		+SHorizontalBox::Slot().AutoWidth().Padding(0.0f,0.0f,20.0f,0.0f)
		[
			SAssignNew(WeaponPriorityConfigButton, SButton)
			.HAlign(HAlign_Center)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
			.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
			.Text(NSLOCTEXT("SUTWeaponConfigDialog", "PriorityButton", "SET AUTO-SWITCH PRIORITIES"))
			.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
			.OnClicked(this, &SUTWeaponConfigDialog::SetAutoSwitchPriorities)
		]
	+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
		[
			SAssignNew(WeaponPriorityConfigButton, SButton)
			.HAlign(HAlign_Center)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
			.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
			.Text(NSLOCTEXT("SUTWeaponConfigDialog", "WeaponWheel", "CONFIGURE WEAPON WHEEL"))
			.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
			.OnClicked(this, &SUTWeaponConfigDialog::OnConfigureWheelClick)
		]

	+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
	[
		SNew(SButton)
		.HAlign(HAlign_Center)
		.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
		.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
		.Text(NSLOCTEXT("SUTWeaponConfigDialog", "ResetToDefaults", "RESET TO DEFAULTS"))
		.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
		.OnClicked(this, &SUTWeaponConfigDialog::OnResetClick)
	];

}

void SUTWeaponConfigDialog::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PreviewTexture);
	Collector.AddReferencedObject(PreviewMID);
	Collector.AddReferencedObject(PreviewWorld);

	for (int32 i=0; i < AllCrosshairs.Num(); i++)
	{
		Collector.AddReferencedObject(AllCrosshairs[i]);
	}

	for (int32 i=0; i < AllWeapons.Num(); i++)
	{
		Collector.AddReferencedObject(AllWeapons[i].WeaponClass);
		AUTWeapon* D = AllWeapons[i].WeaponDefaultObject.Get();
		Collector.AddReferencedObject(D);
	}

	for (int32 i = 0 ; i < WeaponSkinGCList.Num(); i++) Collector.AddReferencedObject(WeaponSkinGCList[i]);
	for (int32 i = 0 ; i < PickupPreviewActors.Num(); i++) Collector.AddReferencedObject(PickupPreviewActors[i]);
	for (int32 i = 0; i < WeaponPreviewActors.Num(); i++) Collector.AddReferencedObject(WeaponPreviewActors[i]);


}

void SUTWeaponConfigDialog::GeneratePage()
{
	Page->ClearChildren();
	if (bShowingCrosshairs)
	{
		// If we can change the crosshairs, then 
		if (CustomWeaponCrosshairs->IsChecked())
		{
			Page->AddSlot().HAlign(HAlign_Fill).AutoHeight().Padding(50.0f, 0.0f, 50.0f, 300.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().FillWidth(0.3).VAlign(VAlign_Top)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot().AutoHeight()
					[
						SNew(SUTBorder)
						.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
						[
							SNew(STextBlock)
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
							.Text(NSLOCTEXT("SUTWeaponConfigDialog", "ColorPicker", " Crosshair Color "))
						]
					]
					+SVerticalBox::Slot().AutoHeight().Padding(0.0f,10.0f,0.0f,0.0f)
					[
						SAssignNew(ColorPicker, SUTColorPicker)
						.TargetColorAttribute(this, &SUTWeaponConfigDialog::GetCrosshairColor)
						.OnColorCommitted(this, &SUTWeaponConfigDialog::SetCrosshairColor)
						.PreColorCommitted(this, &SUTWeaponConfigDialog::SetCrosshairColor)
						.DisplayInlineVersion(true)
						.UseAlpha(true)
					]
				]
				+ SHorizontalBox::Slot().FillWidth(0.3).VAlign(VAlign_Top)
				[
					SNew(SCanvas)
				]
				+SHorizontalBox::Slot().FillWidth(0.3).VAlign(VAlign_Top)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
					[
						SNew(SUTBorder)
						.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
						[
							SNew(STextBlock)
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
							.Text(NSLOCTEXT("SUTWeaponConfigDialog", "CrosshairScale", " Crosshair Scale "))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
					[
						SNew(SBox).WidthOverride(300)
						[
							SNew(SNumericEntryBox<float>)
							.EditableTextBoxStyle(SUWindowsStyle::Get(), "UT.Common.Editbox.White")
							.LabelPadding(FMargin(50.0f, 50.0f))
							.Value(this, &SUTWeaponConfigDialog::GetCrosshairScale)
							.OnValueChanged(this, &SUTWeaponConfigDialog::SetCrosshairScale)
							.AllowSpin(true)
							.MinSliderValue(0.00001f)
							.MinValue(0.00001f)
							.MaxSliderValue(10.0f)
							.MaxValue(10.0f)
						]
					]
				]
			];
		}
	}
	else
	{
		Page->AddSlot().HAlign(HAlign_Fill).AutoHeight()
		[
			SNew(SBox).HeightOverride(880)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.0f,40.0f,40.0f,0.0f)
				[
					SNew(SHorizontalBox).Visibility(this, &SUTWeaponConfigDialog::VarientSelectVis)
					+SHorizontalBox::Slot()
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUTWeaponConfigDialog","ClickToUse","Click to Select"))
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
							.ColorAndOpacity(FLinearColor::Yellow)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SUTButton)
							.ContentPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
							.OnClicked(this, &SUTWeaponConfigDialog::SetWeaponSkin)
							.ButtonStyle(SUTStyle::Get(), "UT.WeaponConfig.Use.Button")
						]
					]
				]
			]
		];
	}
	Page->AddSlot().HAlign(HAlign_Fill).VAlign(VAlign_Bottom)
	[
		SNew(SBox).HeightOverride(40)
		[
			SNew(SUTBorder)
			.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(40.0f, 0.0f, 15.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(this, &SUTWeaponConfigDialog::GetVariantTitle)
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tween")
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(200)
					[
						SNew(STextBlock)
						.Text(this, &SUTWeaponConfigDialog::GetVariantCount)
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tween")
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0).Padding(0.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SCanvas)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 15.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUTWeaponConfigDialog", "Group", "Weapon Group:"))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tween")
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 40.0f, 0.0f)
				[
					SAssignNew(WeaponGroupBox, SHorizontalBox)
				]
			]
		]
	];

	GenerateWeaponGroups();
}


void SUTWeaponConfigDialog::ConstructPreviewWorld(FVector2D ViewportSize)
{	
	/********************* Create the preview world to render meshes */

	// allocate a preview scene for rendering
	PreviewWorld = UWorld::CreateWorld(EWorldType::GamePreview, true);
	PreviewWorld->bShouldSimulatePhysics = true;
	GEngine->CreateNewWorldContext(EWorldType::GamePreview).SetCurrentWorld(PreviewWorld);
	PreviewWorld->InitializeActorsForPlay(FURL(), true);
	PreviewViewState.Allocate();
	{
		UClass* PreviewEnvironmentClass = LoadObject<UClass>(nullptr, TEXT("/Game/RestrictedAssets/UI/PlayerPreviewEnvironment.PlayerPreviewEnvironment_C"));
		PreviewEnvironment = PreviewWorld->SpawnActor<AActor>(PreviewEnvironmentClass, FVector(500.f, 50.f, 0.f), FRotator(0, 0, 0));
	}

	UMaterialInterface* PreviewBaseMat = LoadObject<UMaterialInterface>(NULL, TEXT("/Game/RestrictedAssets/UI/PlayerPreviewProxy.PlayerPreviewProxy"));
	if (PreviewBaseMat != NULL)
	{
		PreviewTexture = Cast<UUTCanvasRenderTarget2D>(UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetPlayerOwner().Get(), UUTCanvasRenderTarget2D::StaticClass(), ViewportSize.X, ViewportSize.Y));
		PreviewTexture->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		PreviewTexture->OnNonUObjectRenderTargetUpdate.BindSP(this, &SUTWeaponConfigDialog::UpdatePreviewRender);
		PreviewMID = UMaterialInstanceDynamic::Create(PreviewBaseMat, PreviewWorld);
		PreviewMID->SetTextureParameterValue(FName(TEXT("TheTexture")), PreviewTexture);
		PreviewBrush = new FSlateMaterialBrush(*PreviewMID, ViewportSize);
	}
	else
	{
		PreviewTexture = NULL;
		PreviewMID = NULL;
		PreviewBrush = new FSlateMaterialBrush(*UMaterial::GetDefaultMaterial(MD_Surface), ViewportSize);
	}

	PreviewTexture->TargetGamma = GEngine->GetDisplayGamma();
	PreviewTexture->InitCustomFormat(ViewportSize.X, ViewportSize.Y, PF_B8G8R8A8, false);

	PreviewTexture->UpdateResourceImmediate();

	PreviewCameraLocation = DefaultPreviewCameraLocation;
	DesiredPreviewCameraLocation = DefaultPreviewCameraLocation;
}


SUTWeaponConfigDialog::~SUTWeaponConfigDialog()
{
	if (!GExitPurge)
	{
		for (int32 i=0; i < DeleteList.Num(); i++)
		{
			delete DeleteList[i];
		}

		if (PreviewTexture != NULL)
		{
			PreviewTexture->OnNonUObjectRenderTargetUpdate.Unbind();
			PreviewTexture = NULL;
		}
		FlushRenderingCommands();
		if (PreviewBrush != NULL)
		{
			// FIXME: Slate will corrupt memory if this is deleted. Must be referencing it somewhere that doesn't get cleaned up...
			//		for now, we'll take the minor memory leak (the texture still gets GC'ed so it's not too bad)
			//delete PlayerPreviewBrush;
			PreviewBrush->SetResourceObject(NULL);
			PreviewBrush = NULL;
		}
		if (PreviewWorld != NULL)
		{
			PreviewWorld->DestroyWorld(true);
			GEngine->DestroyWorldContext(PreviewWorld);
			PreviewWorld = NULL;
			GetPlayerOwner()->GetWorld()->ForceGarbageCollection(true);
		}
	}
	PreviewViewState.Destroy();
}

void SUTWeaponConfigDialog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SUTDialogBase::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bAnimating)
	{
		AnimTimer += InDeltaTime;
		if (AnimTimer < NEXT_PREV_ANIM_TIME)
		{
			PreviewCameraOffset.Y = FUTMath::LerpOvershoot(AnimStartY, AnimEndY, AnimTimer / NEXT_PREV_ANIM_TIME, 2.56f, 9.0f);
		}
		else
		{
			PreviewCameraOffset.Y = AnimEndY;
			bAnimating = false;
		}
	}

	TimeSinceLastCameraAdjustment += InDeltaTime;
	if (TimeSinceLastCameraAdjustment > CAMERA_RETURN_WAIT_TIME)
	{
		if (!PreviewCameraFreeLookOffset.IsZero())
		{
			FUTMath::ReturnVectorToZero(PreviewCameraFreeLookOffset, CAMERA_SPEED * InDeltaTime);		
		}
		

		if ( PickupPreviewActors.IsValidIndex(CurrentWeaponPreviewIndex) )
		{
			FRotator R = PickupPreviewActors[CurrentWeaponPreviewIndex]->GetActorRotation();
			if (R.Yaw != 0)
			{
				FUTMath::ReturnToZero(R.Yaw, CAMERA_SPEED * InDeltaTime);
				PickupPreviewActors[CurrentWeaponPreviewIndex]->SetActorRotation(R);
			}
		}
	}

	if (DesiredPreviewCameraLocation != PreviewCameraLocation)
	{
		float Dist = (PreviewCameraLocation - DesiredPreviewCameraLocation).Size();
		FVector Direction = (DesiredPreviewCameraLocation - PreviewCameraLocation).GetSafeNormal();
		PreviewCameraLocation += Direction * CAMERA_SPEED * InDeltaTime;

		if ( (PreviewCameraLocation - DesiredPreviewCameraLocation).Size() >= Dist)
		{
			PreviewCameraLocation = DesiredPreviewCameraLocation;
		}
	}

	if (PreviewWorld != nullptr)
	{
		PreviewWorld->Tick(LEVELTICK_All, InDeltaTime);
	}

	if (PreviewTexture != nullptr)
	{
		PreviewTexture->FastUpdateResource();
	}
}


void SUTWeaponConfigDialog::UpdatePreviewRender(UCanvas* C, int32 Width, int32 Height)
{
	FEngineShowFlags ShowFlags(ESFIM_Game);
	//ShowFlags.SetLighting(false); // FIXME: create some proxy light and use lit mode
	ShowFlags.SetMotionBlur(false);
	ShowFlags.SetGrain(false);
	
	//ShowFlags.SetPostProcessing(false);
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(PreviewTexture->GameThread_GetRenderTargetResource(), PreviewWorld->Scene, ShowFlags).SetRealtimeUpdate(true));

	const float PreviewFOV = 45;
	const float AspectRatio = Width / (float)Height;

	FSceneViewInitOptions PreviewInitOptions;
	PreviewInitOptions.SetViewRectangle(FIntRect(0, 0, C->SizeX, C->SizeY));
	PreviewInitOptions.ViewOrigin = -(PreviewCameraLocation - PreviewCameraOffset - PreviewCameraFreeLookOffset);
	PreviewInitOptions.ViewRotationMatrix = FMatrix(FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
	PreviewInitOptions.ProjectionMatrix =
		FReversedZPerspectiveMatrix(
			FMath::Max(0.001f, PreviewFOV) * (float)PI / 360.0f,
			AspectRatio,
			1.0f,
			GNearClippingPlane);
	PreviewInitOptions.ViewFamily = &ViewFamily;
	PreviewInitOptions.SceneViewStateInterface = PreviewViewState.GetReference();
	PreviewInitOptions.BackgroundColor = FLinearColor::Black;
	PreviewInitOptions.WorldToMetersScale = GetPlayerOwner()->GetWorld()->GetWorldSettings()->WorldToMeters;
	PreviewInitOptions.CursorPos = FIntPoint(-1, -1);

	ViewFamily.bUseSeparateRenderTarget = true;

	FSceneView* View = new FSceneView(PreviewInitOptions); // note: renderer gets ownership
	View->ViewLocation = FVector::ZeroVector;
	View->ViewRotation = FRotator::ZeroRotator;
	FPostProcessSettings PPSettings = GetDefault<AUTPlayerCameraManager>()->DefaultPPSettings;

	ViewFamily.Views.Add(View);
	View->StartFinalPostprocessSettings(PreviewCameraLocation);
	View->EndFinalPostprocessSettings(PreviewInitOptions);
	View->ViewRect = View->UnscaledViewRect;

	// workaround for hacky renderer code that uses GFrameNumber to decide whether to resize render targets
	--GFrameNumber;
	GetRendererModule().BeginRenderingViewFamily(C->Canvas, &ViewFamily);

	if (bShowingCrosshairs && AllCrosshairs.IsValidIndex(CurrentCrosshairIndex))
	{
		FWeaponCustomizationInfo Customization = FWeaponCustomizationInfo();
		Customization.CrosshairTag = AllWeapons[CurrentWeaponIndex].WeaponCustomizationInfo->DefaultCrosshairTag;
		if (bCustomWeaponCrosshairs)
		{
			Customization = bSingleCustomWeaponCrosshair ? SingleCustomWeaponCrosshair : *AllWeapons[CurrentWeaponIndex].WeaponCustomizationInfo;
		}
		AllCrosshairs[CurrentCrosshairIndex]->DrawCrosshair(nullptr, C, nullptr, 0.0f, Customization);
	}
}


void SUTWeaponConfigDialog::GatherWeaponData(UUTProfileSettings* ProfileSettings)
{
	if (ProfileSettings != nullptr)
	{
		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(AUTWeapon::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL && !ClassPath->Contains(TEXT("/EpicInternal/"))) // exclude debug/test weapons
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTWeapon::StaticClass()))
				{
					AUTWeapon* WeaponDefaultObject = TestClass->GetDefaultObject<AUTWeapon>();
					if (WeaponDefaultObject != nullptr && !WeaponDefaultObject->bHideInMenus)
					{
						bool bFound = false;
						for (int32 i=0; i < AllWeapons.Num(); i++)
						{
							if (AllWeapons[i].WeaponClass == TestClass)
							{
								bFound = true;
								break;
							}
						}

						if (!bFound)
						{
							int32 Index = AllWeapons.Add(FWeaponInfo(TestClass, WeaponDefaultObject));
							if (Index != INDEX_NONE)
							{
								bool bUnique = true;
								// Look for duplicates
								for (int32 i = 0; i < Index; i++)
								{
									if (AllWeapons[i].WeaponCustomizationInfo->WeaponCustomizationTag == WeaponDefaultObject->WeaponCustomizationTag)
									{
										AllWeapons[Index].WeaponCustomizationInfo = AllWeapons[i].WeaponCustomizationInfo;
										bUnique = false;
										break;
									}
								}

								if (bUnique)
								{
									// Look to see if there is a customization record for this tag in the profile. If not, create  one
									if (ProfileSettings->WeaponCustomizations.Contains(WeaponDefaultObject->WeaponCustomizationTag))
									{
										AllWeapons[Index].WeaponCustomizationInfo = new FWeaponCustomizationInfo(ProfileSettings->WeaponCustomizations[WeaponDefaultObject->WeaponCustomizationTag]);
									}
									else
									{
										AllWeapons[Index].WeaponCustomizationInfo = new FWeaponCustomizationInfo(WeaponDefaultObject->WeaponCustomizationTag, WeaponDefaultObject->Group, WeaponDefaultObject->AutoSwitchPriority, DefaultWeaponCrosshairs::Dot, FLinearColor::White, 1.0f);
										DeleteList.Add(AllWeapons[Index].WeaponCustomizationInfo);
									}
								}

								AllWeapons[Index].WeaponSkinClassname = ProfileSettings->GetWeaponSkinClassname(WeaponDefaultObject);
							}
						}
					}
				}
			}
		}
	}
}

void SUTWeaponConfigDialog::GenerateWeaponList(UClass* DesiredSelectedWeaponClass)
{
	if (!WeaponScrollBox.IsValid()) return;
	WeaponScrollBox->ClearChildren();

	TSharedPtr<SUTButton> SelectedButton;
	SelectedButton.Reset();

	struct FGroupInfo
	{
		TSharedPtr<SHorizontalBox> GroupBox;
		TSharedPtr<SGridPanel> ButtonBox;
		int32 NoButtons;

		FGroupInfo()
		{
			GroupBox.Reset();
			ButtonBox.Reset();
			NoButtons = 0;
		}

		FGroupInfo(TSharedPtr<SHorizontalBox> inGroupBox, TSharedPtr<SGridPanel> inButtonBox)
			: GroupBox(inGroupBox)
			, ButtonBox(inButtonBox)
		{
			NoButtons = 0;
		}
	};

	TMap<int32, FGroupInfo> GroupBoxes;
	int32 MaxGroupIndex = -1;

	// Build all of the groups.
	for (int32 i = 0; i < AllWeapons.Num(); i++)
	{
		TSharedPtr<SHorizontalBox> GroupBox;
		TSharedPtr<SGridPanel> ButtonBox;
		int32 Group = AllWeapons[i].WeaponCustomizationInfo->WeaponGroup;
		if (Group > MaxGroupIndex) MaxGroupIndex = Group;

		bool bAlt = Group % 2 > 0;

		if ( !GroupBoxes.Contains(Group) )
		{
			FText GroupText = FText::GetEmpty();
			if (Group > 0)
			{
				GroupText = FText::AsNumber(Group < 10 ? Group : 0);
			}

			// Add an slot for it.
			SAssignNew(GroupBox, SHorizontalBox)
			+SHorizontalBox::Slot().Padding(0.0f, 0.0f, 0.0f, 0.0f).AutoWidth()
			[
				SNew(SBox).WidthOverride(32)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot().FillHeight(0.45f).VAlign(VAlign_Fill).HAlign(HAlign_Center)
					[
						SNew(SBox).WidthOverride(6)
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f,5.0f,0.0f,5.0f).HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(GroupText)
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
						.ColorAndOpacity(FLinearColor::Yellow)
					]
					+ SVerticalBox::Slot().FillHeight(0.45f).VAlign(VAlign_Fill).HAlign(HAlign_Center)
					[
						SNew(SBox).WidthOverride(6)
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
						]
					]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0)
			[
				SNew(SUTBorder)
				.BorderImage(bAlt ? SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark.ListB") : SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark.ListA"))
				[
					SAssignNew(ButtonBox, SGridPanel)
				]

			];

			// Save them.
			GroupBoxes.Add(Group, FGroupInfo(GroupBox, ButtonBox));	
		}
		else
		{
			GroupBox = GroupBoxes[Group].GroupBox;
			ButtonBox = GroupBoxes[Group].ButtonBox;
		}
		// Add the button....

		int32 Row = GroupBoxes[Group].NoButtons / 3;
		int32 Col = GroupBoxes[Group].NoButtons % 3;
		GroupBoxes[Group].NoButtons++;
		TSharedPtr<SUTButton> Button;

		if (AllWeapons[i].WeaponDefaultObject.IsValid())
		{
			ButtonBox->AddSlot(Col, Row).Padding(5.0f, 5.0f, 5.0f, 5.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).WidthOverride(192).HeightOverride(96)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().HAlign(HAlign_Center)
						[
							SAssignNew(Button, SUTButton)
							.OnClicked(this, &SUTWeaponConfigDialog::WeaponClicked, i)
							.IsToggleButton(true)
							.ButtonStyle(SUTStyle::Get(), "UT.WeaponConfig.Button")
							.ClickMethod(EButtonClickMethod::MouseDown)
							.ToolTip(SUTUtils::CreateTooltip(AllWeapons[i].WeaponDefaultObject->DisplayName))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().HAlign(HAlign_Center).AutoHeight()
								[	
									SNew(STextBlock)
									.Text(AllWeapons[i].WeaponDefaultObject->DisplayName)
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny.Bold")
								]
								+ SVerticalBox::Slot().HAlign(HAlign_Center).AutoHeight()
								[
									SNew(SBox).WidthOverride(144).HeightOverride(72).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
									[
										SNew(SImage)
										.Image(AllWeapons[i].WeaponIconBrush)
									]
								]
							]
						]
					]
				]
			];
		}

		AllWeapons[i].WeaponButton = Button;
		if (DesiredSelectedWeaponClass != nullptr && AllWeapons[i].WeaponClass == DesiredSelectedWeaponClass)
		{
			SelectedButton = Button;
		}
	}

	TSharedPtr<SVerticalBox> Box;
	WeaponScrollBox->AddSlot()
	[
		SAssignNew(Box, SVerticalBox)
	];
	
	if (Box.IsValid())
	{
		for (int32 i=0; i <= MaxGroupIndex; i++)
		{
			if (GroupBoxes.Contains(i))
			{
				Box->AddSlot().Padding(5.0f, 0.0f, 5.0f, 15.0f).AutoHeight()
				[
					GroupBoxes[i].GroupBox.ToSharedRef()
				];
			}
		}
	}

	if (SelectedButton.IsValid())
	{
		SelectedButton->IsPressed();
	}
}

void SUTWeaponConfigDialog::RecreateWeaponPreview()
{
	PreviewWeaponSkins.Empty();

	int32 DesiredIndex = 0;
	if ( AllWeapons.IsValidIndex(CurrentWeaponIndex) )
	{
		// Kill all existing actors
		for (int32 i = 0 ; i < PickupPreviewActors.Num(); i++)
		{
			if (PickupPreviewActors[i] != nullptr)
			{
				PickupPreviewActors[i]->Destroy();
			}
		}

		for (int32 i = 0; i < WeaponPreviewActors.Num(); i++)
		{
			if (WeaponPreviewActors[i] != nullptr)
			{
				WeaponPreviewActors[i]->Destroy();
			}
		}

		// Reset everything
		PreviewCameraOffset = FVector(0.0f, 0.0f, 0.0f);
		PreviewCameraFreeLookOffset = FVector(0.0f, 0.0f, 0.0f);
		PickupPreviewActors.Empty();
		WeaponPreviewActors.Empty();
		CurrentWeaponPreviewIndex = 0;

		// Add the Epic Default Weapon
		FVector SpawnLocation = FVector(0.0f, 0.0f, 0.0f);

		AUTWeapon* BaseWeapon = PreviewWorld->SpawnActor<AUTWeapon>(AllWeapons[CurrentWeaponIndex].WeaponClass, SpawnLocation, PreviewActorRotation);
		if (BaseWeapon != nullptr)
		{
			BaseWeapon->SetActorTickEnabled(false);
			AUTDroppedPickup* BasePickup = PreviewWorld->SpawnActor<AUTDroppedPickup>(AUTDroppedPickup::StaticClass(), SpawnLocation, PreviewActorRotation);
			if (BasePickup != nullptr)
			{
				BasePickup->SetInventory(BaseWeapon);
				BasePickup->SetActorTickEnabled(false);
				BasePickup->Movement->StopMovementImmediately();
				BasePickup->Movement->ProjectileGravityScale = 0.0f;
				BasePickup->SetActorHiddenInGame(bShowingCrosshairs);
				BasePickup->PrestreamTextures(0, true);
				PickupPreviewActors.Add(BasePickup);
				
				WeaponPreviewActors.Add(BaseWeapon);
				PreviewWeaponSkins.Add(nullptr);
				SpawnLocation.Y += PREVIEW_Y_SPACING;
			}
		}

		// Build each of the skinned versions

		FName WeaponSkinCustomizationTag = AllWeapons[CurrentWeaponIndex].WeaponDefaultObject->WeaponSkinCustomizationTag;
		CurrentWeaponSkinArray = AllWeaponSkins.Find(WeaponSkinCustomizationTag);
		if (CurrentWeaponSkinArray != nullptr)
		{
			for (int32 i = 0; i < CurrentWeaponSkinArray->Num(); i++)
			{
				UUTWeaponSkin* WeaponSkin = (*CurrentWeaponSkinArray)[i];
				AUTWeapon* SkinWeapon = PreviewWorld->SpawnActor<AUTWeapon>(AllWeapons[CurrentWeaponIndex].WeaponClass, SpawnLocation, PreviewActorRotation);
				if (SkinWeapon != nullptr)
				{
					SkinWeapon->SetActorTickEnabled(false);
					AUTDroppedPickup* SkinPickup = PreviewWorld->SpawnActor<AUTDroppedPickup>(AUTDroppedPickup::StaticClass(), SpawnLocation, PreviewActorRotation);
					if (SkinPickup != nullptr)
					{
						SkinPickup->SetActorTickEnabled(false);
						SkinPickup->SetInventory(SkinWeapon);
						SkinPickup->Movement->StopMovementImmediately();
						SkinPickup->Movement->ProjectileGravityScale = 0.0f;
						SkinPickup->SetActorHiddenInGame(bShowingCrosshairs);
						SkinPickup->SetWeaponSkin(WeaponSkin);
						SkinPickup->PrestreamTextures(0,true);
						PreviewWeaponSkins.Add(WeaponSkin);

						PickupPreviewActors.Add(SkinPickup);
						WeaponPreviewActors.Add(SkinWeapon);

						if (WeaponSkin->GetPathName() == AllWeapons[CurrentWeaponIndex].WeaponSkinClassname)
						{
							DesiredIndex = WeaponPreviewActors.Num()-1;
						}

						SpawnLocation.Y += PREVIEW_Y_SPACING;
					}
				}
			}
		}
	}

	CurrentWeaponPreviewIndex = DesiredIndex;
	PreviewCameraOffset.Y = CurrentWeaponPreviewIndex * PREVIEW_Y_SPACING;;
}

FReply SUTWeaponConfigDialog::WeaponClicked(int32 NewWeaponIndex)
{
	if (AllWeapons.IsValidIndex(NewWeaponIndex))
	{
		if (AllWeapons.IsValidIndex(CurrentWeaponIndex) && AllWeapons[CurrentWeaponIndex].WeaponButton.IsValid() )
		{
			AllWeapons[CurrentWeaponIndex].WeaponButton->UnPressed();
		}

		CurrentWeaponIndex = NewWeaponIndex;
		if (AllWeapons[CurrentWeaponIndex].WeaponButton.IsValid())
		{
			AllWeapons[CurrentWeaponIndex].WeaponButton->BePressed();
			UpdateWeaponGroups(AllWeapons[CurrentWeaponIndex].WeaponCustomizationInfo->WeaponGroup);
		}
		
		CalcCrosshairIndex();
		RecreateWeaponPreview();
	}

	return FReply::Handled();
}

void SUTWeaponConfigDialog::CalcCrosshairIndex()
{
	FName CrosshairTag = AllWeapons[CurrentWeaponIndex].WeaponCustomizationInfo->DefaultCrosshairTag;
	if (bCustomWeaponCrosshairs)
	{
		if (bSingleCustomWeaponCrosshair)
		{
			CrosshairTag = SingleCustomWeaponCrosshair.CrosshairTag;
			if (ColorPicker.IsValid())
			{
				ColorPicker->UTSetNewTargetColorRGB(SingleCustomWeaponCrosshair.CrosshairColorOverride);
			}
		}
		else
		{
			CrosshairTag = AllWeapons[CurrentWeaponIndex].WeaponCustomizationInfo->CrosshairTag;
			if (ColorPicker.IsValid() && AllWeapons.IsValidIndex(CurrentWeaponIndex) && AllWeapons[CurrentWeaponIndex].WeaponCustomizationInfo)
			{
				ColorPicker->UTSetNewTargetColorRGB(AllWeapons[CurrentWeaponIndex].WeaponCustomizationInfo->CrosshairColorOverride);
			}
		}
	}

	CurrentCrosshairIndex = 0;
	for (int32 i = 0; i < AllCrosshairs.Num(); i++)
	{
		if (AllCrosshairs[i]->CrosshairTag == CrosshairTag)
		{
			CurrentCrosshairIndex = i;
			break;
		}
	}
}



void SUTWeaponConfigDialog::DragPreview(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TimeSinceLastCameraAdjustment = 0.0f;

	if ( MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton) )
	{
		PreviewCameraFreeLookOffset.X += MouseEvent.GetCursorDelta().Y;
		PreviewCameraFreeLookOffset.X = FMath::Clamp<float>(PreviewCameraFreeLookOffset.X, -96.0f, 96.0f);
	}
	else if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		PreviewCameraFreeLookOffset.Y -= MouseEvent.GetCursorDelta().X * 0.25f;
		PreviewCameraFreeLookOffset.Z += MouseEvent.GetCursorDelta().Y * 0.25f;

		PreviewCameraFreeLookOffset.Y = FMath::Clamp<float>(PreviewCameraFreeLookOffset.Y,-32.0f, 32.0f);
		PreviewCameraFreeLookOffset.Z = FMath::Clamp<float>(PreviewCameraFreeLookOffset.Z, -32.0f, 16.0f);

		// Update the local camera offset
	}
	else if (MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
	{
		if (PickupPreviewActors.IsValidIndex(CurrentWeaponPreviewIndex))
		{
			if (PickupPreviewActors[CurrentWeaponPreviewIndex]!= nullptr)
			{
				FRotator CurrentActorRotation = PickupPreviewActors[CurrentWeaponPreviewIndex]->GetActorRotation();
				PickupPreviewActors[CurrentWeaponPreviewIndex]->SetActorRotation(CurrentActorRotation + FRotator(0, 0.2f * -MouseEvent.GetCursorDelta().X, 0.0f));
			}
		}
	}
}

void SUTWeaponConfigDialog::ZoomPreview(float WheelDelta)
{
	PreviewCameraFreeLookOffset.X += (WheelDelta > 0.0f) ? 10.0f : -10.0f;
	PreviewCameraFreeLookOffset.X = FMath::Clamp<float>(PreviewCameraFreeLookOffset.X, -96.0f, 96.0f);

	TimeSinceLastCameraAdjustment = 0;
}


TSharedRef<SWidget> SUTWeaponConfigDialog::GenerateWeaponHand(UUTProfileSettings* Profile)
{
	WeaponHandDesc.Add(NSLOCTEXT("UT", "Right", "Right"));
	WeaponHandDesc.Add(NSLOCTEXT("UT", "Left", "Left"));
	WeaponHandDesc.Add(NSLOCTEXT("UT", "Center", "Center"));
	WeaponHandDesc.Add(NSLOCTEXT("UT", "Hidden", "Hidden"));

	WeaponHandList.Add(MakeShareable(new FText(WeaponHandDesc[uint8(EWeaponHand::HAND_Right)])));
	//WeaponHandList.Add(MakeShareable(new FText(WeaponHandDesc[EWeaponHand::HAND_Left])));
	//WeaponHandList.Add(MakeShareable(new FText(WeaponHandDesc[EWeaponHand::HAND_Center])));
	WeaponHandList.Add(MakeShareable(new FText(WeaponHandDesc[uint8(EWeaponHand::HAND_Hidden)])));

	TSharedPtr<FText> InitiallySelectedHand = WeaponHandList[0];
	for (TSharedPtr<FText> TestItem : WeaponHandList)
	{
		if (TestItem.Get()->EqualTo(WeaponHandDesc[uint8(Profile->WeaponHand)]))
		{
			InitiallySelectedHand = TestItem;
		}
	}

	TSharedPtr<SHorizontalBox> Box;
	SAssignNew(Box,SHorizontalBox)
	+ SHorizontalBox::Slot().HAlign(HAlign_Left).AutoWidth().Padding(5.0f,0.0f,0.0f,0.0f)
	[
		SNew(STextBlock)
		.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tween")
		.Text(NSLOCTEXT("SUTWeaponConfigDialog", "WeaponHand", "Weapon Hand"))
	]
	+ SHorizontalBox::Slot().FillWidth(1.0).HAlign(HAlign_Right).Padding(0.0f,0.0f,5.0f,0.0f)
	[
		SAssignNew(WeaponHand, SComboBox< TSharedPtr<FText> >)
		.InitiallySelectedItem(InitiallySelectedHand)
		.ComboBoxStyle(SUTStyle::Get(), "UT.ComboBox")
		.ButtonStyle(SUTStyle::Get(), "UT.SimpleButton.Bright")
		.OptionsSource(&WeaponHandList)
		.OnGenerateWidget(this, &SUTWeaponConfigDialog::GenerateHandListWidget)
		.OnSelectionChanged(this, &SUTWeaponConfigDialog::OnHandSelected)
		.ContentPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
		.Content()
		[
			SAssignNew(SelectedWeaponHand, STextBlock)
			.Text(*InitiallySelectedHand.Get())
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tween.Bold")
			.ColorAndOpacity(FLinearColor::Black)
		]
	];

	return Box.ToSharedRef();
}

TSharedRef<SWidget> SUTWeaponConfigDialog::GenerateHandListWidget(TSharedPtr<FText> InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.Text(*InItem.Get())
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tween.Bold")
		];
}

void SUTWeaponConfigDialog::OnHandSelected(TSharedPtr<FText> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedWeaponHand->SetText(*NewSelection.Get());
}


TSharedRef<SWidget> SUTWeaponConfigDialog::GenerateAutoSwitch(UUTProfileSettings* Profile)
{
	TSharedPtr<SHorizontalBox> Box;
	SAssignNew(Box,SHorizontalBox)
	+ SHorizontalBox::Slot().HAlign(HAlign_Left).AutoWidth().Padding(5.0f,0.0f,0.0f,0.0f)
	[
		SNew(STextBlock)
		.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tween")
		.Text(NSLOCTEXT("SUTWeaponConfigDialog", "AutoWeaponSwitch", "Weapon Switch on Pickup"))
	]
	+ SHorizontalBox::Slot().FillWidth(1.0).HAlign(HAlign_Right).Padding(0.0f,0.0f,5.0f,0.0f)

	[
		SAssignNew(AutoWeaponSwitch, SCheckBox)
		.Style(SUTStyle::Get(), "UT.CheckBox")
		.ForegroundColor(FLinearColor::White)
		.IsChecked(Profile->bAutoWeaponSwitch ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked)
	];

	return Box.ToSharedRef();
}

TSharedRef<SWidget> SUTWeaponConfigDialog::GenerateCustomWeaponCrosshairs(UUTProfileSettings* Profile)
{
	TSharedPtr<SVerticalBox> Box;

	SAssignNew(Box,SVerticalBox)
	+SVerticalBox::Slot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().HAlign(HAlign_Left).AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tween")
			.Text(NSLOCTEXT("SUTWeaponConfigDialog", "CustomCrosshair", "Use Custom Weapon Crosshairs"))
		]
		+ SHorizontalBox::Slot().FillWidth(1.0).HAlign(HAlign_Right).Padding(0.0f, 0.0f, 5.0f, 0.0f)
		[
			SAssignNew(CustomWeaponCrosshairs, SCheckBox)
			.Style(SUTStyle::Get(), "UT.CheckBox")
			.ForegroundColor(FLinearColor::White)
			.IsChecked(this, &SUTWeaponConfigDialog::GetCustomWeaponCrosshairs)
			.OnCheckStateChanged(this, &SUTWeaponConfigDialog::SetCustomWeaponCrosshairs)
		]
	]
	+SVerticalBox::Slot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().HAlign(HAlign_Left).AutoWidth().Padding(15.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tween")
			.Text(NSLOCTEXT("SUTWeaponConfigDialog", "SingleCrosshair", "All Weapons use the same Crosshair"))
			.Visibility(this, &SUTWeaponConfigDialog::GetSingleCustomWeaponCrosshairVis)
		]
		+ SHorizontalBox::Slot().FillWidth(1.0).HAlign(HAlign_Right).Padding(0.0f, 0.0f, 5.0f, 0.0f)
		[
			SAssignNew(SingleCustomWeaponCrosshairCheckbox, SCheckBox)
			.Style(SUTStyle::Get(), "UT.CheckBox")
			.ForegroundColor(FLinearColor::White)
			.IsChecked(this, &SUTWeaponConfigDialog::GetSingleCustomWeaponCrosshair)
			.OnCheckStateChanged(this, &SUTWeaponConfigDialog::SetSingleCustomWeaponCrosshair)
			.Visibility(this, &SUTWeaponConfigDialog::GetSingleCustomWeaponCrosshairVis)
		]
	];

	return Box.ToSharedRef();
}

ECheckBoxState SUTWeaponConfigDialog::GetCustomWeaponCrosshairs() const
{
	return bCustomWeaponCrosshairs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SUTWeaponConfigDialog::SetCustomWeaponCrosshairs(ECheckBoxState NewState)
{
	bCustomWeaponCrosshairs = NewState == ECheckBoxState::Checked;
	CalcCrosshairIndex();
	GeneratePage();
}

ECheckBoxState SUTWeaponConfigDialog::GetSingleCustomWeaponCrosshair() const
{
	return bSingleCustomWeaponCrosshair ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SUTWeaponConfigDialog::SetSingleCustomWeaponCrosshair(ECheckBoxState NewState)
{
	bSingleCustomWeaponCrosshair = NewState == ECheckBoxState::Checked;

	CalcCrosshairIndex();
	GeneratePage();
}

EVisibility SUTWeaponConfigDialog::GetSingleCustomWeaponCrosshairVis() const
{
	return bCustomWeaponCrosshairs ? EVisibility::Visible : EVisibility::Collapsed;
}


void SUTWeaponConfigDialog::GenerateWeaponGroups()
{
	if (WeaponGroupBox.IsValid())
	{
		WeaponGroupBox->ClearChildren();
		WeaponGroupButtons.Empty();
		WeaponGroupButtons.AddZeroed(11);

		for (int32 i=1; i <=10; i++)
		{
			TSharedPtr<SUTButton> Button;

			WeaponGroupBox->AddSlot()
			.Padding(5.0f,0.0f,0.0f,0.0f)
			.AutoWidth()
			.HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(32)
				[
					SAssignNew(Button, SUTButton)
					.OnClicked(this, &SUTWeaponConfigDialog::WeaponGroupClicked, i)
					.ButtonStyle(SUTStyle::Get(), "UT.SimpleButton.Dark")
					.IsToggleButton(true)
					.ClickMethod(EButtonClickMethod::MouseDown)
					.CaptionHAlign(HAlign_Center)
					.Text(i < 10 ? FText::AsNumber(i) : FText::AsNumber(0))
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
				]
			];
			WeaponGroupButtons[i] = Button;
		}
	}
}

void SUTWeaponConfigDialog::UpdateWeaponGroups(int32 NewWeaponGroup)
{
	if (!AllWeapons[CurrentWeaponIndex].WeaponClass->IsChildOf(AUTWeap_Translocator::StaticClass()))
	{
		for (int i=0; i < WeaponGroupButtons.Num(); i++)
		{
			if (WeaponGroupButtons[i].IsValid())
			{
				WeaponGroupButtons[i]->SetVisibility(EVisibility::Visible);
				if (i == NewWeaponGroup)
				{
					WeaponGroupButtons[i]->BePressed();
				}
				else
				{
					WeaponGroupButtons[i]->UnPressed();
				}
			}
		}
	}
	else
	{
		for (int i = 0; i < WeaponGroupButtons.Num(); i++)
		{
			if (WeaponGroupButtons[i].IsValid())
			{
				WeaponGroupButtons[i]->SetVisibility(EVisibility::Collapsed);
			}
		}
	}
}

FReply SUTWeaponConfigDialog::WeaponGroupClicked(int32 NewWeaponGroup)
{
	if (AllWeapons.IsValidIndex(CurrentWeaponIndex))
	{

		AUTWeapon* DefaultWeapon = AllWeapons[CurrentWeaponIndex].WeaponDefaultObject.Get();

		FName WeaponCustomizationTag = AllWeapons[CurrentWeaponIndex].WeaponCustomizationInfo->WeaponCustomizationTag;

		for (int32 i=0; i < AllWeapons.Num(); i++)
		{
			if (AllWeapons[i].WeaponCustomizationInfo->WeaponCustomizationTag == WeaponCustomizationTag)
			{
				AllWeapons[i].WeaponCustomizationInfo->WeaponGroup = NewWeaponGroup;
			}
		}

		UpdateWeaponGroups(NewWeaponGroup);
		GenerateWeaponList(AllWeapons[CurrentWeaponIndex].WeaponClass);

		for (int32 i=0; i < AllWeapons.Num(); i++)
		{
			if (AllWeapons[i].WeaponDefaultObject.Get() == DefaultWeapon)
			{
				CurrentWeaponIndex = i;
				AllWeapons[i].WeaponButton->BePressed();
				break;
			}
		}

	}
	return FReply::Handled();
}


void SUTWeaponConfigDialog::GatherWeaponSkins(UUTProfileSettings* ProfileSettings)
{
	// We are not supporting weapon skins in editor builds right now due to performance.
	// So disable the gathering .  Also note that they are forced off in the profile.

/*
	TArray<FAssetData> AssetList;
	GetAllAssetData(UUTWeaponSkin::StaticClass(), AssetList);
	for (const FAssetData& Asset : AssetList)
	{
		UUTWeaponSkin* NewWeaponSkin = Cast<UUTWeaponSkin>(Asset.GetAsset());
		if (NewWeaponSkin)
		{
			TArray<UUTWeaponSkin*>* WeaponSkinArray = AllWeaponSkins.Find(NewWeaponSkin->WeaponSkinCustomizationTag);
			if (WeaponSkinArray == nullptr)
			{
				WeaponSkinArray = &(AllWeaponSkins.Add(NewWeaponSkin->WeaponSkinCustomizationTag));
			}

			if (WeaponSkinArray)
			{
				WeaponSkinGCList.Add(NewWeaponSkin);
				(*WeaponSkinArray).Add(NewWeaponSkin);
			}
		}
	}
*/
}

FText SUTWeaponConfigDialog::GetVariantTitle() const
{
	if (bShowingCrosshairs)
	{
		return NSLOCTEXT("SUTWeaponConfigDialog", "CrosshairVariant", "Crosshair:");
	}

	return NSLOCTEXT("SUTWeaponConfigDialog", "SkinVariant", "Weapon Skin:");
}

FText SUTWeaponConfigDialog::GetVariantCount() const
{
	return FText::Format(NSLOCTEXT("SUTWeaponConfigDialog", "VariantCountFormat", "{0} of {1}"), FText::AsNumber(bShowingCrosshairs ? CurrentCrosshairIndex + 1 : CurrentWeaponPreviewIndex + 1), FText::AsNumber(bShowingCrosshairs ? AllCrosshairs.Num() : WeaponPreviewActors.Num()));
}

FReply SUTWeaponConfigDialog::HandleNextPrevious(int32 Step)
{
	if (bShowingCrosshairs)
	{
		if (CustomWeaponCrosshairs->IsChecked())
		{
			CurrentCrosshairIndex = FMath::Clamp<int32>(CurrentCrosshairIndex + Step, 0, AllCrosshairs.Num()-1);
			if (!bSingleCustomWeaponCrosshair)
			{
				AllWeapons[CurrentWeaponIndex].WeaponCustomizationInfo->CrosshairTag = AllCrosshairs[CurrentCrosshairIndex]->CrosshairTag;
			}
			else
			{
				SingleCustomWeaponCrosshair.CrosshairTag = AllCrosshairs[CurrentCrosshairIndex]->CrosshairTag;
			}
		}
	}
	else
	{
		int32 LastWeaponPreviewIndex = CurrentWeaponPreviewIndex;

		CurrentWeaponPreviewIndex += Step;
		CurrentWeaponPreviewIndex = FMath::Clamp<int32>(CurrentWeaponPreviewIndex, 0, WeaponPreviewActors.Num()-1);

		if (LastWeaponPreviewIndex != CurrentWeaponPreviewIndex)
		{
			AnimStartY = PreviewCameraOffset.Y;
			AnimEndY = CurrentWeaponPreviewIndex * PREVIEW_Y_SPACING;
			AnimTimer = 0;
			bAnimating = true;
		}
	}
	return FReply::Handled();
}

FText SUTWeaponConfigDialog::GetVarientText() const
{
	if (bShowingCrosshairs)
	{
		if (AllCrosshairs.IsValidIndex(CurrentCrosshairIndex))
		{
			return AllCrosshairs[CurrentCrosshairIndex]->CrosshairName;
		}
		return NSLOCTEXT("SUTWeaponConfigDialog","BadCrosshair","Bad Crosshair");
	}
	else
	{
		// NOTE: we have to account for the default weapon not having a weapon skin
		if (CurrentWeaponPreviewIndex > 0 && CurrentWeaponSkinArray != nullptr && CurrentWeaponSkinArray->IsValidIndex(CurrentWeaponPreviewIndex-1))
		{
			UUTWeaponSkin* WeaponSkin = (*CurrentWeaponSkinArray)[CurrentWeaponPreviewIndex-1];
			if (WeaponSkin != nullptr)
			{
				return WeaponSkin->DisplayName;
			}
		}
	}
	return NSLOCTEXT("SUTWeaponConfigDialog","DefaultWeaponSkinDesc","Epic Special");
}

FText SUTWeaponConfigDialog::GetVarientSetText() const
{
	if (bShowingCrosshairs)
	{
		return (!bCustomWeaponCrosshairs ? NSLOCTEXT("SUTWeaponConfigDialog","TurnOnCustomWeaponCrosshairs","You are using default crosshairs -- Check \"Use Custom Weapon Crosshairs\" to change.") : FText::GetEmpty());
	}
	else
	{
		return (IsCurrentSkin() ? NSLOCTEXT("SUTWeaponConfigDialog","VarientSetLabel","( currently selected )") : FText::GetEmpty());
	}
}


EVisibility SUTWeaponConfigDialog::LeftArrowVis() const
{
	if (bShowingCrosshairs)
	{
		return (CustomWeaponCrosshairs->IsChecked() && AllCrosshairs.Num() > 1 && CurrentCrosshairIndex > 0) ? EVisibility::Visible : EVisibility::Collapsed;
	}
	else
	{
		return (PickupPreviewActors.Num() > 1 && CurrentWeaponPreviewIndex > 0) ? EVisibility::Visible : EVisibility::Collapsed;
	}
}
EVisibility SUTWeaponConfigDialog::RightArrowVis() const
{
	if (bShowingCrosshairs)
	{
		return (CustomWeaponCrosshairs->IsChecked() && AllCrosshairs.Num() > 1 && CurrentCrosshairIndex < AllCrosshairs.Num() - 1) ? EVisibility::Visible : EVisibility::Collapsed;
	}
	else
	{
		return (PickupPreviewActors.Num() > 1 && CurrentWeaponPreviewIndex < PickupPreviewActors.Num() - 1) ? EVisibility::Visible : EVisibility::Collapsed;
	}
}

FReply SUTWeaponConfigDialog::OnModeChanged(int32 NewMode)
{
	bShowingCrosshairs = NewMode == 1;
	
	for (int32 i=0; i < PickupPreviewActors.Num();i++)
	{
		PickupPreviewActors[i]->SetActorHiddenInGame(bShowingCrosshairs);
	}

	if (bShowingCrosshairs)
	{
		CalcCrosshairIndex();
		VariantsButton->UnPressed();
		CrosshairButton->BePressed();
	}
	else
	{
		VariantsButton->BePressed();
		CrosshairButton->UnPressed();
	}

	GeneratePage();
	UpdateWeaponGroups(AllWeapons[CurrentWeaponIndex].WeaponCustomizationInfo->WeaponGroup);

	return FReply::Handled();
}

void SUTWeaponConfigDialog::GatherCrosshairs(UUTProfileSettings* ProfileSettings)
{
	CurrentCrosshairIndex = 0;
	AllCrosshairs.Empty();
	TArray<FAssetData> AssetList;
	GetAllBlueprintAssetData(UUTCrosshair::StaticClass(), AssetList);
	for (const FAssetData& Asset : AssetList)
	{
		static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
		const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
		UClass* CrosshairClass = LoadObject<UClass>(NULL, **ClassPath);
		if (CrosshairClass != nullptr)
		{
			UUTCrosshair* Crosshair = NewObject<UUTCrosshair>(GetPlayerOwner().Get(), CrosshairClass, NAME_None, RF_NoFlags);
			if (Crosshair && Crosshair->CrosshairTag != NAME_None)
			{
				AllCrosshairs.Add(Crosshair);
			}
		}
	}
	
	SingleCustomWeaponCrosshair.CrosshairTag = ProfileSettings->SingleCustomWeaponCrosshair.CrosshairTag;
	SingleCustomWeaponCrosshair.CrosshairColorOverride = ProfileSettings->SingleCustomWeaponCrosshair.CrosshairColorOverride;
	SingleCustomWeaponCrosshair.CrosshairScaleOverride = ProfileSettings->SingleCustomWeaponCrosshair.CrosshairScaleOverride;
}

TOptional<float> SUTWeaponConfigDialog::GetCrosshairScale() const
{
	
	return !bSingleCustomWeaponCrosshair 
		? AllWeapons[CurrentWeaponIndex].WeaponCustomizationInfo->CrosshairScaleOverride 
		: SingleCustomWeaponCrosshair.CrosshairScaleOverride;
}

void SUTWeaponConfigDialog::SetCrosshairScale(float InScale)
{
	if (bCustomWeaponCrosshairs)
	{
		if (bSingleCustomWeaponCrosshair)
		{
			SingleCustomWeaponCrosshair.CrosshairScaleOverride = InScale;
		}
		else
		{
			AllWeapons[CurrentWeaponIndex].WeaponCustomizationInfo->CrosshairScaleOverride = InScale;
		}
	}
}

FLinearColor SUTWeaponConfigDialog::GetCrosshairColor() const
{
	return !bSingleCustomWeaponCrosshair 
		? AllWeapons[CurrentWeaponIndex].WeaponCustomizationInfo->CrosshairColorOverride 
		: SingleCustomWeaponCrosshair.CrosshairColorOverride;
}

void SUTWeaponConfigDialog::SetCrosshairColor(FLinearColor NewColor)
{
	if (!bSingleCustomWeaponCrosshair)
	{
		AllWeapons[CurrentWeaponIndex].WeaponCustomizationInfo->CrosshairColorOverride = NewColor;
	}
	else
	{
		SingleCustomWeaponCrosshair.CrosshairColorOverride = NewColor;
	}
}


bool SUTWeaponConfigDialog::IsCurrentSkin() const
{
	// If we are on the first variant and the customization is blank, this is the skin
	if (CurrentWeaponPreviewIndex == 0 && AllWeapons[CurrentWeaponIndex].WeaponSkinClassname.IsEmpty()) return true;
	return (PreviewWeaponSkins.IsValidIndex(CurrentWeaponPreviewIndex) && PreviewWeaponSkins[CurrentWeaponPreviewIndex] != nullptr
		&& PreviewWeaponSkins[CurrentWeaponPreviewIndex]->GetPathName() == AllWeapons[CurrentWeaponIndex].WeaponSkinClassname);
}

EVisibility SUTWeaponConfigDialog::VarientSelectVis() const
{
	if (!bShowingCrosshairs)
	{
		return IsCurrentSkin() ? EVisibility::Collapsed	: EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

FReply SUTWeaponConfigDialog::SetWeaponSkin()
{
	// If we are on the first variant and the customization is blank, this is the skin
	if (CurrentWeaponPreviewIndex == 0)
	{
		AllWeapons[CurrentWeaponIndex].WeaponSkinClassname = TEXT("");
	}
	else if (PreviewWeaponSkins.IsValidIndex(CurrentWeaponPreviewIndex) && PreviewWeaponSkins[CurrentWeaponPreviewIndex] != nullptr)
	{
		AllWeapons[CurrentWeaponIndex].WeaponSkinClassname = PreviewWeaponSkins[CurrentWeaponPreviewIndex]->GetPathName();
	}

	return FReply::Handled();
}

FReply SUTWeaponConfigDialog::SetAutoSwitchPriorities()
{
	SAssignNew(AutoSwitchDialog, SUTWeaponPriorityDialog)
		.PlayerOwner(PlayerOwner)
		.OnDialogResult(this, &SUTWeaponConfigDialog::AutoSwitchDialogClosed);

	AutoSwitchDialog->InitializeList(AllWeapons);

	PlayerOwner->OpenDialog(AutoSwitchDialog.ToSharedRef(), 512);
	return FReply::Handled();
}

void SUTWeaponConfigDialog::AutoSwitchDialogClosed(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	TMap<FName, float> Tags;
	if (ButtonID == UTDIALOG_BUTTON_OK)
	{
		for (int32 i = 0; i < AllWeapons.Num(); i++)
		{
			float Priority = AutoSwitchDialog->GetPriorityFor(AllWeapons[i].WeaponDefaultObject);
			if (Priority >= 0.0f)
			{
				// Look to see if any weapon that came before it used the same weapon settings tag.  If it does, then
				// look to see if we should override it.  This is to deal with 2 weapons in the same settings group
				// but still visible in the UI (which should be avoided).

				if (Tags.Contains(AllWeapons[i].WeaponCustomizationInfo->WeaponCustomizationTag))
				{
					float OtherPriority = Tags[AllWeapons[i].WeaponCustomizationInfo->WeaponCustomizationTag];
					if (Priority < OtherPriority)
					{
						continue;
					}
				}
				else
				{
					Tags.Add(AllWeapons[i].WeaponCustomizationInfo->WeaponCustomizationTag, Priority);
				}

				AllWeapons[i].WeaponCustomizationInfo->WeaponAutoSwitchPriority = Priority;
			}
		}
	}	
}

FReply SUTWeaponConfigDialog::OnButtonClick(uint16 ButtonID)
{
	UUTProfileSettings* ProfileSettings = GetPlayerOwner()->GetProfileSettings();
	if (ProfileSettings != nullptr && ButtonID == UTDIALOG_BUTTON_OK)
	{
		// Copy everything back in to the profile..	
	
		EWeaponHand NewHand = EWeaponHand::HAND_Right;
		for (int32 i = 0; i < WeaponHandDesc.Num(); i++)
		{
			if (WeaponHandDesc[i].EqualTo(*WeaponHand->GetSelectedItem().Get()))
			{
				NewHand = EWeaponHand(i);
				break;
			}
		}

		ProfileSettings->bCustomWeaponCrosshairs = bCustomWeaponCrosshairs;
		ProfileSettings->bAutoWeaponSwitch = AutoWeaponSwitch->IsChecked();
		ProfileSettings->WeaponHand = NewHand;
		ProfileSettings->WeaponSkins.Empty();

		AUTPlayerController* UTPlayerController = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
		if (UTPlayerController != nullptr)
		{
			UTPlayerController->SetWeaponHand(ProfileSettings->WeaponHand);
		}

		for(int32 i = 0; i < AllWeapons.Num(); i++)
		{
			FName WeaponCustomizationTag = AllWeapons[i].WeaponCustomizationInfo->WeaponCustomizationTag;
			if (ProfileSettings->WeaponCustomizations.Contains(WeaponCustomizationTag))
			{
				// We already have a customization tag for this weapon.. so, just update it.
				ProfileSettings->WeaponCustomizations[WeaponCustomizationTag].Copy(*AllWeapons[i].WeaponCustomizationInfo);
			}
			else
			{
				// This is a new customization so we add it.
				ProfileSettings->WeaponCustomizations.Add(WeaponCustomizationTag, FWeaponCustomizationInfo(*AllWeapons[i].WeaponCustomizationInfo));
			}

			if (!AllWeapons[i].WeaponSkinClassname.IsEmpty() && AllWeapons[i].WeaponDefaultObject.IsValid())
			{
				ProfileSettings->WeaponSkins.Add(AllWeapons[i].WeaponDefaultObject->WeaponSkinCustomizationTag,AllWeapons[i].WeaponSkinClassname);
			}

		}

		ProfileSettings->bSingleCustomWeaponCrosshair = bSingleCustomWeaponCrosshair;
		ProfileSettings->SingleCustomWeaponCrosshair.CrosshairTag = SingleCustomWeaponCrosshair.CrosshairTag;
		ProfileSettings->SingleCustomWeaponCrosshair.CrosshairColorOverride = SingleCustomWeaponCrosshair.CrosshairColorOverride;
		ProfileSettings->SingleCustomWeaponCrosshair.CrosshairScaleOverride = SingleCustomWeaponCrosshair.CrosshairScaleOverride;

		bRequiresSave = true;
	}

	if (bRequiresSave)
	{
		PlayerOwner->SaveProfileSettings();
	}

	return SUTDialogBase::OnButtonClick(ButtonID);
}

FReply SUTWeaponConfigDialog::OnConfigureWheelClick()
{
	SAssignNew(WheelConfigDialog, SUTWeaponWheelConfigDialog).PlayerOwner(PlayerOwner).OnDialogResult(this, &SUTWeaponConfigDialog::WheelConfigDialogClosed);
	WheelConfigDialog->InitializeList(&AllWeapons);
	PlayerOwner->OpenDialog(WheelConfigDialog.ToSharedRef());
	return FReply::Handled();

}

void SUTWeaponConfigDialog::WheelConfigDialogClosed(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK) bRequiresSave = true;
}


FReply SUTWeaponConfigDialog::OnResetClick()
{
	UUTProfileSettings* ProfileSettings = PlayerOwner->GetProfileSettings();
	ProfileSettings->ResetProfile(EProfileResetType::Weapons);
	PlayerOwner->SaveProfileSettings();

	GetPlayerOwner()->CloseDialog(SharedThis(this));
	PlayerOwner->OpenDialog(SNew(SUTWeaponConfigDialog).PlayerOwner(PlayerOwner));
	return FReply::Handled();
}

#endif