// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SUWPlayerSettingsDialog.h"
#include "SUWindowsStyle.h"
#include "SUWWeaponConfigDialog.h"
#include "SNumericEntryBox.h"
#include "../Public/UTCanvasRenderTarget2D.h"
#include "EngineModule.h"
#include "SlateMaterialBrush.h"
#include "../Public/UTPlayerCameraManager.h"
#include "UTCharacterContent.h"
#include "UTWeap_ShockRifle.h"
#include "UTWeaponAttachment.h"
#include "UTHUD.h"
#include "Engine/UserInterfaceSettings.h"

#if !UE_SERVER
#include "Runtime/AppFramework/Public/Widgets/Colors/SColorPicker.h"
#endif

#include "AssetData.h"

// scale factor for weapon/view bob sliders (i.e. configurable value between 0 and this)
static const float BOB_SCALING_FACTOR = 2.0f;

#if !UE_SERVER

#include "SScaleBox.h"
#include "Widgets/SDragImage.h"

#define LOCTEXT_NAMESPACE "SUWPlayerSettingsDialog"

void SUWPlayerSettingsDialog::Construct(const FArguments& InArgs)
{
	FVector2D ViewportSize;
	InArgs._PlayerOwner->ViewportClient->GetViewportSize(ViewportSize);
	float DPIScale = GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass())->GetDPIScaleBasedOnSize(FIntPoint(ViewportSize.X, ViewportSize.Y));

	FVector2D UnscaledDialogSize = ViewportSize / DPIScale;
	SUWDialog::Construct(SUWDialog::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(NSLOCTEXT("SUWindowsDesktop", "PlayerSettings", "Player Settings"))
							.DialogSize(UnscaledDialogSize)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.ContentPadding(FVector2D(0,0))
							.IsScrollable(false)
							.ButtonMask(UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_CANCEL)
							.OnDialogResult(InArgs._OnDialogResult)
						);

	PlayerPreviewMesh = nullptr;
	PreviewWeapon = nullptr;
	bSpinPlayer = true;
	ZoomOffset = 0;

	WeaponConfigDelayFrames = 0;


	AUTHUD* DefaultHud = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>();
	if (DefaultHud)
	{
		for (int32 i=0; i< DefaultHud->FlagList.Num(); i++)
		{
			CountyFlagNames.Add(MakeShareable(new FFlagInfo(DefaultHud->FlagList[i])));
		}
	
	}
	
	CountyFlagNames.Add(MakeShareable(new FFlagInfo(TEXT("Unreal"),0)));

	if (GetPlayerOwner()->CommunityRole != EUnrealRoles::Gamer)
	{
		CountyFlagNames.Add(MakeShareable(new FFlagInfo(TEXT("Red Team"),140)));
		CountyFlagNames.Add(MakeShareable(new FFlagInfo(TEXT("Blue Team"),141)));
		if (GetPlayerOwner()->CommunityRole == EUnrealRoles::Developer)
		{
			CountyFlagNames.Add(MakeShareable(new FFlagInfo(TEXT("Epic Logo"),142)));
		}
	}

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
	
	PlayerPreviewAnimBlueprint = LoadObject<UClass>(nullptr, TEXT("/Game/RestrictedAssets/UI/ABP_PlayerPreview.ABP_PlayerPreview_C"));

	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(NULL, TEXT("/Game/RestrictedAssets/UI/PlayerPreviewProxy.PlayerPreviewProxy"));
	if (BaseMat != NULL)
	{
		PlayerPreviewTexture = Cast<UUTCanvasRenderTarget2D>(UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetPlayerOwner().Get(), UUTCanvasRenderTarget2D::StaticClass(), ViewportSize.X, ViewportSize.Y));
		PlayerPreviewTexture->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		PlayerPreviewTexture->OnNonUObjectRenderTargetUpdate.BindSP(this, &SUWPlayerSettingsDialog::UpdatePlayerRender);
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

	UUTGameUserSettings* Settings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	
	{
		HatList.Add(MakeShareable(new FString(TEXT("No Hat"))));
		HatPathList.Add(TEXT(""));

		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(AUTHat::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL)
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTHat::StaticClass()) && !TestClass->IsChildOf(AUTHatLeader::StaticClass()))
				{
					if (!TestClass->GetDefaultObject<AUTHat>()->bOverrideOnly)
					{
						HatList.Add(MakeShareable(new FString(TestClass->GetDefaultObject<AUTHat>()->CosmeticName)));
						HatPathList.Add(Asset.ObjectPath.ToString() + TEXT("_C"));
					}
				}
			}
		}
	}

	HatVariantList.Add(MakeShareable(new FString(TEXT("Default"))));
	EyewearVariantList.Add(MakeShareable(new FString(TEXT("Default"))));

	{
		EyewearList.Add(MakeShareable(new FString(TEXT("No Glasses"))));
		EyewearPathList.Add(TEXT(""));

		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(AUTEyewear::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL)
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTEyewear::StaticClass()))
				{
					EyewearList.Add(MakeShareable(new FString(TestClass->GetDefaultObject<AUTEyewear>()->CosmeticName)));
					EyewearPathList.Add(Asset.ObjectPath.ToString() + TEXT("_C"));
				}
			}
		}
	}

	{
		CharacterList.Add(MakeShareable(new FString(TEXT("Malcolm"))));
		CharacterPathList.Add(TEXT(""));

		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(AUTCharacterContent::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL)
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				// TODO: long term should probably delayed load this... but would need some way to look up the loc'ed display name without loading the class (currently impossible...)
				if ( TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTCharacterContent::StaticClass()) &&
					 !TestClass->GetDefaultObject<AUTCharacterContent>()->bHideInUI )
				{
					CharacterList.Add(MakeShareable(new FString(TestClass->GetDefaultObject<AUTCharacterContent>()->DisplayName.ToString())));
					CharacterPathList.Add(Asset.ObjectPath.ToString() + TEXT("_C"));
				}
			}
		}
	}

	{
		TauntList.Add(MakeShareable(new FString(TEXT("No Taunt"))));
		TauntPathList.Add(TEXT(""));

		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(AUTTaunt::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL)
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTTaunt::StaticClass()))
				{
					TauntList.Add(MakeShareable(new FString(TestClass->GetDefaultObject<AUTTaunt>()->DisplayName)));
					TauntPathList.Add(Asset.ObjectPath.ToString() + TEXT("_C"));
				}
			}
		}
	}

	SelectedPlayerColor = GetDefault<AUTPlayerController>()->FFAPlayerColor;
	SelectedHatVariantIndex = GetPlayerOwner()->GetHatVariant();
	SelectedEyewearVariantIndex = GetPlayerOwner()->GetEyewearVariant();

	FMargin NameColumnPadding = FMargin(10, 4);
	FMargin ValueColumnPadding = FMargin(0, 4);
	
	float FOVSliderSetting = (GetDefault<AUTPlayerController>()->ConfigDefaultFOV - FOV_CONFIG_MIN) / (FOV_CONFIG_MAX - FOV_CONFIG_MIN);

	if (DialogContent.IsValid())
	{
		const float MessageTextPaddingX = 10.0f;
		TSharedPtr<STextBlock> MessageTextBlock;
		DialogContent->AddSlot()
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFill)
				[
					SNew(SDragImage)
					.Image(PlayerPreviewBrush)
					.OnDrag(this, &SUWPlayerSettingsDialog::DragPlayerPreview)
					.OnZoom(this, &SUWPlayerSettingsDialog::ZoomPlayerPreview)
				]
			]

			+ SOverlay::Slot()
			[
				SNew(SHorizontalBox)
			
				+ SHorizontalBox::Slot()
				.FillWidth(0.50f)
				[
					SNew(SSpacer)
					.Size(FVector2D(1,1))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.35f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
				
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("PlayerName", "Name"))
						]

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.Padding(ValueColumnPadding)
						[
							SAssignNew(PlayerName, SEditableTextBox)
							.OnTextChanged(this, &SUWPlayerSettingsDialog::OnNameTextChanged)
							.Text(FText::FromString(GetPlayerOwner()->GetNickname()))
							.Style(SUWindowsStyle::Get(), "UT.Common.Editbox.White")
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()

					[
						SNew(SGridPanel)

						// Country Flag
						// ---------------------------------------------------------------------------------
						+ SGridPanel::Slot(0, 0)
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("CountryFlag", "Country Flag"))
						]

						+ SGridPanel::Slot(1, 0)
						.Padding(ValueColumnPadding)
						[
							SNew( SBox )
							.WidthOverride(250.f)
							[
								SAssignNew(CountryFlagComboBox, SComboBox< TSharedPtr<FFlagInfo> >)
								.InitiallySelectedItem(0)
								.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
								.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
								.OptionsSource(&CountyFlagNames)
								.OnGenerateWidget(this, &SUWPlayerSettingsDialog::GenerateFlagListWidget)
								.OnSelectionChanged(this, &SUWPlayerSettingsDialog::OnFlagSelected)
								.Content()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.Padding(10.0f, 0.0f, 10.0f, 0.0f)
									[
										SAssignNew(SelectedFlag, STextBlock)
										.Text(FText::FromString(TEXT("Unreal")))
										.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
									]
								]
							]
						]

						// Hat
						// ---------------------------------------------------------------------------------
						+ SGridPanel::Slot(0, 1)
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("HatSelectionLabel", "Hat"))
						]

						+ SGridPanel::Slot(1, 1)
						.Padding(ValueColumnPadding)
						[
							SAssignNew(HatComboBox, SComboBox< TSharedPtr<FString> >)
							.InitiallySelectedItem(0)
							.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
							.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
							.OptionsSource(&HatList)
							.OnGenerateWidget(this, &SUWDialog::GenerateStringListWidget)
							.OnSelectionChanged(this, &SUWPlayerSettingsDialog::OnHatSelected)
							.ContentPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
							.Content()
							[
								SAssignNew(SelectedHat, STextBlock)
								.Text(FText::FromString(TEXT("No Hats Available")))
								.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
							]
						]
						
						+ SGridPanel::Slot(2, 1)
						.Padding(NameColumnPadding)
						[
							SAssignNew(HatVariantComboBox, SComboBox< TSharedPtr<FString> >)
							.InitiallySelectedItem(0)
							.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
							.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
							.OptionsSource(&HatVariantList)
							.OnGenerateWidget(this, &SUWDialog::GenerateStringListWidget)
							.OnSelectionChanged(this, &SUWPlayerSettingsDialog::OnHatVariantSelected)
							.ContentPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
							.Content()
							[
								SAssignNew(SelectedHatVariant, STextBlock)
								.Text(FText::FromString(TEXT("Default")))
								.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
							]
						]

						// Eyewear
						// ---------------------------------------------------------------------------------
						+ SGridPanel::Slot(0, 2)
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("EyewearSelectionLabel", "Eyewear"))
						]

						+ SGridPanel::Slot(1, 2)
						.Padding(ValueColumnPadding)
						[
							SAssignNew(EyewearComboBox, SComboBox< TSharedPtr<FString> >)
							.InitiallySelectedItem(0)
							.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
							.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
							.OptionsSource(&EyewearList)
							.OnGenerateWidget(this, &SUWDialog::GenerateStringListWidget)
							.OnSelectionChanged(this, &SUWPlayerSettingsDialog::OnEyewearSelected)
							.Content()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.Padding(10.0f, 0.0f, 10.0f, 0.0f)
								[
									SAssignNew(SelectedEyewear, STextBlock)
									.Text(FText::FromString(TEXT("No Glasses Available")))
									.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
								]
							]
						]
						
						+ SGridPanel::Slot(2, 2)
						.Padding(NameColumnPadding)
						[
							SAssignNew(EyewearVariantComboBox, SComboBox< TSharedPtr<FString> >)
							.InitiallySelectedItem(0)
							.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
							.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
							.OptionsSource(&EyewearVariantList)
							.OnGenerateWidget(this, &SUWDialog::GenerateStringListWidget)
							.OnSelectionChanged(this, &SUWPlayerSettingsDialog::OnEyewearVariantSelected)
							.ContentPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
							.Content()
							[
								SAssignNew(SelectedEyewearVariant, STextBlock)
								.Text(FText::FromString(TEXT("Default")))
								.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
							]
						]
						
						// Taunt
						// ---------------------------------------------------------------------------------
						+ SGridPanel::Slot(0, 3)
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("TauntSelectionLabel", "Taunt"))
						]

						+ SGridPanel::Slot(1, 3)
						.Padding(ValueColumnPadding)
						[
							SAssignNew(TauntComboBox, SComboBox< TSharedPtr<FString> >)
							.InitiallySelectedItem(0)
							.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
							.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
							.OptionsSource(&TauntList)
							.OnGenerateWidget(this, &SUWDialog::GenerateStringListWidget)
							.OnSelectionChanged(this, &SUWPlayerSettingsDialog::OnTauntSelected)
							.Content()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.Padding(10.0f, 0.0f, 10.0f, 0.0f)
								[
									SAssignNew(SelectedTaunt, STextBlock)
									.Text(FText::FromString(TEXT("No Taunts Available")))
									.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
								]
							]
						]
						
						// Taunt 2
						// ---------------------------------------------------------------------------------
						+ SGridPanel::Slot(0, 4)
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("Taunt2SelectionLabel", "Taunt 2"))
						]

						+ SGridPanel::Slot(1, 4)
						.Padding(ValueColumnPadding)
						[
							SAssignNew(Taunt2ComboBox, SComboBox< TSharedPtr<FString> >)
							.InitiallySelectedItem(0)
							.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
							.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
							.OptionsSource(&TauntList)
							.OnGenerateWidget(this, &SUWDialog::GenerateStringListWidget)
							.OnSelectionChanged(this, &SUWPlayerSettingsDialog::OnTaunt2Selected)
							.Content()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.Padding(10.0f, 0.0f, 10.0f, 0.0f)
								[
									SAssignNew(SelectedTaunt2, STextBlock)
									.Text(FText::FromString(TEXT("No Taunts Available")))
									.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
								]
							]
						]

						// Character
						// ---------------------------------------------------------------------------------
						+ SGridPanel::Slot(0, 5)
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("CharSelectionLabel", "Character"))
						]

						+ SGridPanel::Slot(1, 5)
						.Padding(ValueColumnPadding)
						[
							SAssignNew(CharacterComboBox, SComboBox< TSharedPtr<FString> >)
							.InitiallySelectedItem(0)
							.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
							.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
							.OptionsSource(&CharacterList)
							.OnGenerateWidget(this, &SUWDialog::GenerateStringListWidget)
							.OnSelectionChanged(this, &SUWPlayerSettingsDialog::OnCharacterSelected)
							.Content()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.Padding(10.0f, 0.0f, 10.0f, 0.0f)
								[
									SAssignNew(SelectedCharacter, STextBlock)
									.Text(FText::FromString(TEXT("No Characters Available")))
									.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
								]
							]
						]
					]

					/*
					TODO FIX ME
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 4)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("FFAPlayerColor", "Free for All Player Color"))
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(ValueColumnPadding)
						[
							SNew(SComboButton)
							.Method(EPopupMethod::UseCurrentWindow)
							.MenuPlacement(MenuPlacement_ComboBoxRight)
							.HasDownArrow(false)
							.ContentPadding(0)
							.VAlign(VAlign_Fill)
							.ButtonContent()
							[
								SNew(SColorBlock)
								.Color(this, &SUWPlayerSettingsDialog::GetSelectedPlayerColor)
								.IgnoreAlpha(true)
								.Size(FVector2D(32.0f * ResolutionScale.X, 16.0f * ResolutionScale.Y))
							]
							.MenuContent()
							[
								SNew(SBorder)
								.BorderImage(SUWindowsStyle::Get().GetBrush("UWindows.Standard.Dialog.Background"))
								[
									SNew(SColorPicker)
									.OnColorCommitted(this, &SUWPlayerSettingsDialog::PlayerColorChanged)
									.TargetColorAttribute(SelectedPlayerColor)
								]
							]
						]
					]*/

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SSpacer)
						.Size(FVector2D(30, 30))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 4)
					[
						SNew(SGridPanel)
						.FillColumn(1, 1)

						// Weapon Bob
						// ---------------------------------------------------------------------------------
						+ SGridPanel::Slot(0, 0)
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("WeaponBobScaling", "Weapon Bob"))
						]

						+ SGridPanel::Slot(1, 0)
						.Padding(ValueColumnPadding)
						[
							SAssignNew(WeaponBobScaling, SSlider)
							.Orientation(Orient_Horizontal)
							.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
							.Value(GetDefault<AUTPlayerController>()->WeaponBobGlobalScaling / BOB_SCALING_FACTOR)
						]

						// View Bob
						// ---------------------------------------------------------------------------------
						+ SGridPanel::Slot(0, 1)
						.Padding(NameColumnPadding)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(LOCTEXT("ViewBobScaling", "View Bob"))
						]

						+ SGridPanel::Slot(1, 1)
						.Padding(ValueColumnPadding)
						[
							SAssignNew(ViewBobScaling, SSlider)
							.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
							.Orientation(Orient_Horizontal)
							.Value(GetDefault<AUTPlayerController>()->EyeOffsetGlobalScaling / BOB_SCALING_FACTOR)
						]
						// FOV
						+ SGridPanel::Slot(0, 2)
						.Padding(NameColumnPadding)
						[
							SAssignNew(FOVLabel, STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.Text(FText::FromString(GetFOVLabelText(FOVSliderSetting)))
						]

						+ SGridPanel::Slot(1, 2)
						.Padding(ValueColumnPadding)
						[
							SAssignNew(FOV, SSlider)
							.Style(SUWindowsStyle::Get(), "UT.Common.Slider")
							.Orientation(Orient_Horizontal)
							.Value(FOVSliderSetting)
							.OnValueChanged(this, &SUWPlayerSettingsDialog::OnFOVChange)
						]
					]

					//+ SVerticalBox::Slot()
					//.AutoHeight()
					//[
					//	SNew(SButton)
					//	.HAlign(HAlign_Center)
					//	.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
					//	.ContentPadding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
					//	.Text(LOCTEXT("WeaponConfig", "Weapon Settings"))
					//	.OnClicked(this, &SUWPlayerSettingsDialog::WeaponConfigClick)
					//]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.15f)
				[
					SNew(SSpacer)
					.Size(FVector2D(1,1))
				]
			]
		];

		bool bFoundSelectedHat = false;
		for (int32 i = 0; i < HatPathList.Num(); i++)
		{
			if (HatPathList[i] == GetPlayerOwner()->GetHatPath())
			{
				HatComboBox->SetSelectedItem(HatList[i]);
				bFoundSelectedHat = true;
				break;
			}
		}
		if (!bFoundSelectedHat && HatPathList.Num() > 0)
		{
			HatComboBox->SetSelectedItem(HatList[0]);
		}
		PopulateHatVariants();
		if (SelectedHatVariantIndex > 0 && SelectedHatVariantIndex < HatVariantList.Num())
		{
			HatVariantComboBox->SetSelectedItem(HatVariantList[SelectedHatVariantIndex]);
		}

		bool bFoundSelectedEyewear = false;
		for (int32 i = 0; i < EyewearPathList.Num(); i++)
		{
			if (EyewearPathList[i] == GetPlayerOwner()->GetEyewearPath())
			{
				EyewearComboBox->SetSelectedItem(EyewearList[i]);
				bFoundSelectedEyewear = true;
				break;
			}
		}
		if (!bFoundSelectedEyewear && EyewearPathList.Num() > 0)
		{
			EyewearComboBox->SetSelectedItem(EyewearList[0]);
		}
		PopulateEyewearVariants();
		if (SelectedEyewearVariantIndex > 0 && SelectedEyewearVariantIndex < EyewearVariantList.Num())
		{
			EyewearVariantComboBox->SetSelectedItem(EyewearVariantList[SelectedEyewearVariantIndex]);
		}

		bool bFoundSelectedTaunt = false;
		for (int32 i = 0; i < TauntPathList.Num(); i++)
		{
			if (TauntPathList[i] == GetPlayerOwner()->GetTauntPath())
			{
				TauntComboBox->SetSelectedItem(TauntList[i]);
				bFoundSelectedTaunt = true;
				break;
			}
		}
		if (!bFoundSelectedTaunt && TauntPathList.Num() > 0)
		{
			TauntComboBox->SetSelectedItem(TauntList[0]);
		}

		bool bFoundSelectedTaunt2 = false;
		for (int32 i = 0; i < TauntPathList.Num(); i++)
		{
			if (TauntPathList[i] == GetPlayerOwner()->GetTaunt2Path())
			{
				Taunt2ComboBox->SetSelectedItem(TauntList[i]);
				bFoundSelectedTaunt2 = true;
				break;
			}
		}
		if (!bFoundSelectedTaunt2 && TauntPathList.Num() > 0)
		{
			Taunt2ComboBox->SetSelectedItem(TauntList[0]);
		}

		bool bFoundSelectedCharacter = false;
		for (int32 i = 0; i < CharacterPathList.Num(); i++)
		{
			if (CharacterPathList[i] == GetPlayerOwner()->GetCharacterPath())
			{
				CharacterComboBox->SetSelectedItem(CharacterList[i]);
				bFoundSelectedCharacter = true;
				break;
			}
		}
		if (!bFoundSelectedCharacter && CharacterPathList.Num() > 0)
		{
			CharacterComboBox->SetSelectedItem(CharacterList[0]);
		}
	}

	if (GetPlayerOwner().IsValid())
	{
		uint32 CountryFlag = GetPlayerOwner()->GetCountryFlag();

		for (int32 i=0; i < CountyFlagNames.Num(); i++)
		{
			if (CountyFlagNames[i]->Id == CountryFlag)
			{
				CountryFlagComboBox->SetSelectedItem(CountyFlagNames[i]);
				break;
			}
		}
	}

}

SUWPlayerSettingsDialog::~SUWPlayerSettingsDialog()
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

void SUWPlayerSettingsDialog::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PlayerPreviewTexture);
	Collector.AddReferencedObject(PlayerPreviewMID);
	Collector.AddReferencedObject(PlayerPreviewWorld);
}

FString SUWPlayerSettingsDialog::GetFOVLabelText(float SliderValue)
{
	int32 FOVAngle = FMath::TruncToInt(SliderValue * (FOV_CONFIG_MAX - FOV_CONFIG_MIN) + FOV_CONFIG_MIN);
	return FText::Format(NSLOCTEXT("SUWPlayerSettingsDialog", "FOV", "Field of View ({Value})"), FText::FromString(FString::Printf(TEXT("%i"), FOVAngle))).ToString();
}

void SUWPlayerSettingsDialog::OnFOVChange(float NewValue)
{
	FOVLabel->SetText(GetFOVLabelText(NewValue));
}

void SUWPlayerSettingsDialog::OnNameTextChanged(const FText& NewText)
{
	FString AdjustedText = NewText.ToString();
	FString InvalidNameChars = FString(INVALID_NAME_CHARACTERS);
	for (int32 i = AdjustedText.Len() - 1; i >= 0; i--)
	{
		if (InvalidNameChars.GetCharArray().Contains(AdjustedText.GetCharArray()[i]))
		{
			AdjustedText.GetCharArray().RemoveAt(i);
		}
	}
	if (AdjustedText != NewText.ToString())
	{
		PlayerName->SetText(FText::FromString(AdjustedText));
	}
}

void SUWPlayerSettingsDialog::PlayerColorChanged(FLinearColor NewValue)
{
	SelectedPlayerColor = NewValue;
	RecreatePlayerPreview();
}

FReply SUWPlayerSettingsDialog::OKClick()
{
	GetPlayerOwner()->SetNickname(PlayerName->GetText().ToString());

	uint32 NewFlag = 0;
	for (int32 i=0; i<CountyFlagNames.Num();i++)
	{
		if (CountyFlagNames[i]->Title == SelectedFlag->GetText().ToString())
		{
			NewFlag = CountyFlagNames[i]->Id;
			break;
		}
	}
	
	GetPlayerOwner()->SetCountryFlag(NewFlag,false);

	// FOV
	float NewFOV = FMath::TruncToFloat(FOV->GetValue() * (FOV_CONFIG_MAX - FOV_CONFIG_MIN) + FOV_CONFIG_MIN);

	// If we have a valid PC then tell the PC to set it's name
	AUTPlayerController* UTPlayerController = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	if (UTPlayerController != NULL)
	{
		UTPlayerController->ServerChangeName(PlayerName->GetText().ToString());
		UTPlayerController->WeaponBobGlobalScaling = WeaponBobScaling->GetValue() * BOB_SCALING_FACTOR;
		UTPlayerController->EyeOffsetGlobalScaling = ViewBobScaling->GetValue() * BOB_SCALING_FACTOR;
		UTPlayerController->FFAPlayerColor = SelectedPlayerColor;
		UTPlayerController->SaveConfig();

		if (UTPlayerController->GetUTCharacter())
		{
			UTPlayerController->GetUTCharacter()->NotifyTeamChanged();
		}

		UTPlayerController->FOV(NewFOV);
	}
	else
	{
		AUTPlayerController::StaticClass()->GetDefaultObject<AUTPlayerController>()->ConfigDefaultFOV = NewFOV;
		AUTPlayerController::StaticClass()->GetDefaultObject<AUTPlayerController>()->SaveConfig();
	}

	UUTProfileSettings* ProfileSettings = GetPlayerOwner()->GetProfileSettings();

	int32 Index = HatList.Find(HatComboBox->GetSelectedItem());
	GetPlayerOwner()->SetHatPath(HatPathList.IsValidIndex(Index) ? HatPathList[Index] : FString());
	Index = EyewearList.Find(EyewearComboBox->GetSelectedItem());
	GetPlayerOwner()->SetEyewearPath(EyewearPathList.IsValidIndex(Index) ? EyewearPathList[Index] : FString());
	Index = TauntList.Find(TauntComboBox->GetSelectedItem());
	GetPlayerOwner()->SetTauntPath(TauntPathList.IsValidIndex(Index) ? TauntPathList[Index] : FString());
	Index = TauntList.Find(Taunt2ComboBox->GetSelectedItem());
	GetPlayerOwner()->SetTaunt2Path(TauntPathList.IsValidIndex(Index) ? TauntPathList[Index] : FString());
	Index = CharacterList.Find(CharacterComboBox->GetSelectedItem());
	GetPlayerOwner()->SetCharacterPath(CharacterPathList.IsValidIndex(Index) ? CharacterPathList[Index] : FString());

	Index = HatVariantList.Find(HatVariantComboBox->GetSelectedItem());
	GetPlayerOwner()->SetHatVariant(Index);
	Index = EyewearVariantList.Find(EyewearVariantComboBox->GetSelectedItem());
	GetPlayerOwner()->SetEyewearVariant(Index);

	GetPlayerOwner()->SaveProfileSettings();
	GetPlayerOwner()->CloseDialog(SharedThis(this));

	return FReply::Handled();
}

FReply SUWPlayerSettingsDialog::CancelClick()
{
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

FReply SUWPlayerSettingsDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK) OKClick();
	else if (ButtonID == UTDIALOG_BUTTON_CANCEL) CancelClick();
	return FReply::Handled();
}

FReply SUWPlayerSettingsDialog::WeaponConfigClick()
{
	WeaponConfigDelayFrames = 3;
	GetPlayerOwner()->ShowContentLoadingMessage();
	return FReply::Handled();
}

void SUWPlayerSettingsDialog::OnEmote1Committed(int32 NewValue, ETextCommit::Type CommitInfo)
{
	Emote1Index = NewValue;
}

void SUWPlayerSettingsDialog::OnEmote2Committed(int32 NewValue, ETextCommit::Type CommitInfo)
{
	Emote2Index = NewValue;
}

void SUWPlayerSettingsDialog::OnEmote3Committed(int32 NewValue, ETextCommit::Type CommitInfo)
{
	Emote3Index = NewValue;
}

TOptional<int32> SUWPlayerSettingsDialog::GetEmote1Value() const
{
	return Emote1Index;
}

TOptional<int32> SUWPlayerSettingsDialog::GetEmote2Value() const
{
	return Emote2Index;
}

TOptional<int32> SUWPlayerSettingsDialog::GetEmote3Value() const
{
	return Emote3Index;
}

void SUWPlayerSettingsDialog::OnHatSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (NewSelection.IsValid())
	{
		SelectedHat->SetText(*NewSelection.Get());
		PopulateHatVariants();
		RecreatePlayerPreview();
	}
}

void SUWPlayerSettingsDialog::OnHatVariantSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (NewSelection.IsValid())
	{
		SelectedHatVariant->SetText(*NewSelection.Get());
		RecreatePlayerPreview();
	}
}

void SUWPlayerSettingsDialog::OnEyewearSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (NewSelection.IsValid())
	{
		SelectedEyewear->SetText(*NewSelection.Get());
		PopulateEyewearVariants();
		RecreatePlayerPreview();
	}
}

void SUWPlayerSettingsDialog::OnEyewearVariantSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedEyewearVariant->SetText(*NewSelection.Get());
	RecreatePlayerPreview();
}

void SUWPlayerSettingsDialog::OnCharacterSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (NewSelection.IsValid())
	{
		SelectedCharacter->SetText(*NewSelection.Get());
		RecreatePlayerPreview();
	}
}

void SUWPlayerSettingsDialog::OnTauntSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (NewSelection.IsValid())
	{
		SelectedTaunt->SetText(*NewSelection.Get());

		if (PlayerPreviewMesh != nullptr)
		{
			int32 TauntIndex = TauntList.Find(NewSelection);
			UClass* TauntClass = LoadObject<UClass>(NULL, *TauntPathList[TauntIndex]);
			if (TauntClass)
			{
				PlayerPreviewMesh->PlayTauntByClass(TSubclassOf<AUTTaunt>(TauntClass));
				//PlayerPreviewMesh->GetMesh()->PlayAnimation(TauntClass->GetDefaultObject<AUTTaunt>()->TauntMontage, true);
			}
		}
	}
}

void SUWPlayerSettingsDialog::OnTaunt2Selected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (NewSelection.IsValid())
	{
		SelectedTaunt2->SetText(*NewSelection.Get());
		if (PlayerPreviewMesh != nullptr)
		{
			int32 TauntIndex = TauntList.Find(NewSelection);
			UClass* TauntClass = LoadObject<UClass>(NULL, *TauntPathList[TauntIndex]);
			if (TauntClass)
			{
				PlayerPreviewMesh->PlayTauntByClass(TSubclassOf<AUTTaunt>(TauntClass));
				//PlayerPreviewMesh->GetMesh()->PlayAnimation(TauntClass->GetDefaultObject<AUTTaunt>()->TauntMontage, true);
			}
		}
	}
}

void SUWPlayerSettingsDialog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
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

	// Force the preview mesh to put the highest mips into memory
	if (PlayerPreviewMesh != nullptr)
	{
		PlayerPreviewMesh->PrestreamTextures(1, true);
	}

	if (WeaponConfigDelayFrames > 0)
	{
		WeaponConfigDelayFrames--;
		if (WeaponConfigDelayFrames == 0)
		{
			GetPlayerOwner()->HideContentLoadingMessage();
			GetPlayerOwner()->OpenDialog(SNew(SUWWeaponConfigDialog).PlayerOwner(GetPlayerOwner()));
		}
	}

	if ( bSpinPlayer )
	{
		if ( PlayerPreviewWorld != nullptr )
		{
			const float SpinRate = 15.0f * InDeltaTime;
			PlayerPreviewMesh->AddActorWorldRotation(FRotator(0, SpinRate, 0.0f));
		}
	}
}

void SUWPlayerSettingsDialog::RecreatePlayerPreview()
{
	FRotator ActorRotation = FRotator(0.0f, 180.0f, 0.0f);

	if (PlayerPreviewMesh != nullptr)
	{
		ActorRotation = PlayerPreviewMesh->GetActorRotation();
		PlayerPreviewMesh->Destroy();
	}

	if ( PreviewWeapon != nullptr )
	{
		PreviewWeapon->Destroy();
	}

	PlayerPreviewMesh = PlayerPreviewWorld->SpawnActor<AUTCharacter>(GetDefault<AUTGameMode>()->DefaultPawnClass, FVector(300.0f, 0.f, 4.f), ActorRotation);
	PlayerPreviewMesh->GetMesh()->SetAnimInstanceClass(PlayerPreviewAnimBlueprint);
	
	// set character mesh
	// NOTE: important this is first since it may affect the following items (socket locations, etc)
	int32 Index = CharacterList.Find(CharacterComboBox->GetSelectedItem());
	FString NewCharPath = CharacterPathList.IsValidIndex(Index) ? CharacterPathList[Index] : FString();
	if (NewCharPath.Len() > 0)
	{
		TSubclassOf<AUTCharacterContent> CharacterClass = LoadClass<AUTCharacterContent>(NULL, *NewCharPath, NULL, LOAD_None, NULL);
		if (CharacterClass != NULL)
		{
			PlayerPreviewMesh->ApplyCharacterData(CharacterClass);
		}
	}

	// set FFA color
	/*TODO FIX ME
	const TArray<UMaterialInstanceDynamic*>& BodyMIs = PlayerPreviewMesh->GetBodyMIs();
	for (UMaterialInstanceDynamic* MI : BodyMIs)
	{
		static FName NAME_TeamColor(TEXT("TeamColor"));
		if (MI != NULL)
		{
			MI->SetVectorParameterValue(NAME_TeamColor, SelectedPlayerColor);
		}
	}*/

	// set hat
	Index = HatList.Find(HatComboBox->GetSelectedItem());
	FString NewHatPath = HatPathList.IsValidIndex(Index) ? HatPathList[Index] : FString();
	if (NewHatPath.Len() > 0)
	{
		TSubclassOf<AUTHat> HatClass = LoadClass<AUTHat>(NULL, *NewHatPath, NULL, LOAD_None, NULL);
		if (HatClass != NULL)
		{
			PlayerPreviewMesh->HatVariant = HatVariantList.Find(HatVariantComboBox->GetSelectedItem());
			PlayerPreviewMesh->SetHatClass(HatClass);
		}
	}

	// set eyewear
	Index = EyewearList.Find(EyewearComboBox->GetSelectedItem());
	FString NewEyewearPath = EyewearPathList.IsValidIndex(Index) ? EyewearPathList[Index] : FString();
	if (NewEyewearPath.Len() > 0)
	{
		TSubclassOf<AUTEyewear> EyewearClass = LoadClass<AUTEyewear>(NULL, *NewEyewearPath, NULL, LOAD_None, NULL);
		if (EyewearClass != NULL)
		{
			PlayerPreviewMesh->EyewearVariant = EyewearVariantList.Find(EyewearVariantComboBox->GetSelectedItem());
			PlayerPreviewMesh->SetEyewearClass(EyewearClass);
		}
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

void SUWPlayerSettingsDialog::PopulateHatVariants()
{
	HatVariantList.Empty();
	TSharedPtr<FString> DefaultVariant = MakeShareable(new FString(TEXT("Default")));
	HatVariantList.Add(DefaultVariant);
	HatVariantComboBox->SetSelectedItem(DefaultVariant);

	int32 Index = HatList.Find(HatComboBox->GetSelectedItem());
	FString NewHatPath = HatPathList.IsValidIndex(Index) ? HatPathList[Index] : FString();
	if (NewHatPath.Len() > 0)
	{
		TSubclassOf<AUTCosmetic> HatClass = LoadClass<AUTCosmetic>(NULL, *NewHatPath, NULL, LOAD_None, NULL);
		if (HatClass != NULL)
		{
			for (int32 i = 0; i < HatClass->GetDefaultObject<AUTCosmetic>()->VariantNames.Num(); i++)
			{
				HatVariantList.Add(MakeShareable(new FString(HatClass->GetDefaultObject<AUTCosmetic>()->VariantNames[i].ToString())));
			}
		}
	}

	if (HatVariantList.Num() == 1)
	{
		HatVariantComboBox->SetVisibility(EVisibility::Hidden);
	}
	else
	{
		HatVariantComboBox->SetVisibility(EVisibility::Visible);
	}

	HatVariantComboBox->RefreshOptions();
}

void SUWPlayerSettingsDialog::PopulateEyewearVariants()
{
	EyewearVariantList.Empty();
	TSharedPtr<FString> DefaultVariant = MakeShareable(new FString(TEXT("Default")));
	EyewearVariantList.Add(DefaultVariant);
	EyewearVariantComboBox->SetSelectedItem(DefaultVariant);

	int32 Index = EyewearList.Find(EyewearComboBox->GetSelectedItem());
	FString NewEyewearPath = EyewearPathList.IsValidIndex(Index) ? EyewearPathList[Index] : FString();
	if (NewEyewearPath.Len() > 0)
	{
		TSubclassOf<AUTCosmetic> EyewearClass = LoadClass<AUTCosmetic>(NULL, *NewEyewearPath, NULL, LOAD_None, NULL);
		if (EyewearClass != NULL)
		{
			for (int32 i = 0; i < EyewearClass->GetDefaultObject<AUTCosmetic>()->VariantNames.Num(); i++)
			{
				EyewearVariantList.Add(MakeShareable(new FString(EyewearClass->GetDefaultObject<AUTCosmetic>()->VariantNames[i].ToString())));
			}
		}
	}

	if (EyewearVariantList.Num() == 1)
	{
		EyewearVariantComboBox->SetVisibility(EVisibility::Hidden);
	}
	else
	{
		EyewearVariantComboBox->SetVisibility(EVisibility::Visible);
	}
}

void SUWPlayerSettingsDialog::UpdatePlayerRender(UCanvas* C, int32 Width, int32 Height)
{
	FEngineShowFlags ShowFlags(ESFIM_Game);
	//ShowFlags.SetLighting(false); // FIXME: create some proxy light and use lit mode
	ShowFlags.SetMotionBlur(false);
	ShowFlags.SetGrain(false);
	//ShowFlags.SetPostProcessing(false);
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(PlayerPreviewTexture->GameThread_GetRenderTargetResource(), PlayerPreviewWorld->Scene, ShowFlags).SetRealtimeUpdate(true));

//	EngineShowFlagOverride(ESFIM_Game, VMI_Lit, ViewFamily.EngineShowFlags, NAME_None, false);

	FVector CameraPosition(ZoomOffset, -60, -50);

	const float PreviewFOV = 45;
	const float AspectRatio = Width / (float)Height;

	FSceneViewInitOptions PlayerPreviewInitOptions;
	PlayerPreviewInitOptions.SetViewRectangle(FIntRect(0, 0, C->SizeX, C->SizeY));
	PlayerPreviewInitOptions.ViewOrigin = CameraPosition;
	PlayerPreviewInitOptions.ViewRotationMatrix = FMatrix(FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
	PlayerPreviewInitOptions.ProjectionMatrix = 
		FReversedZPerspectiveMatrix(
			FMath::Max(0.001f, PreviewFOV) * (float)PI / 360.0f,
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

	//View->OverridePostProcessSettings(PPSettings, 1.0f);

	View->EndFinalPostprocessSettings(PlayerPreviewInitOptions);

	// workaround for hacky renderer code that uses GFrameNumber to decide whether to resize render targets
	--GFrameNumber;
	GetRendererModule().BeginRenderingViewFamily(C->Canvas, &ViewFamily);
}

void SUWPlayerSettingsDialog::DragPlayerPreview(const FVector2D MouseDelta)
{
	if (PlayerPreviewMesh != nullptr)
	{
		bSpinPlayer = false;
		PlayerPreviewMesh->SetActorRotation(PlayerPreviewMesh->GetActorRotation() + FRotator(0, 0.2f * -MouseDelta.X, 0.0f));
	}
}

void SUWPlayerSettingsDialog::ZoomPlayerPreview(float WheelDelta)
{
	ZoomOffset = FMath::Clamp(ZoomOffset + (-WheelDelta * 10.0f), -100.0f, 400.0f);
}

void SUWPlayerSettingsDialog::OnFlagSelected(TSharedPtr<FFlagInfo> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedFlag->SetText(NewSelection->Title);
}


TSharedRef<SWidget> SUWPlayerSettingsDialog::GenerateFlagListWidget(TSharedPtr<FFlagInfo> InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.Text(FText::FromString(InItem->Title))
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
		];
}


#undef LOCTEXT_NAMESPACE

#endif