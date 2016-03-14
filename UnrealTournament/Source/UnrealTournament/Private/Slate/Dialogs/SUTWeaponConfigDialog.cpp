// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "SUTWeaponConfigDialog.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "../Widgets/SUTTabWidget.h"
#include "UTCrosshair.h"
#include "UTCanvasRenderTarget2D.h"
#include "EngineModule.h"
#include "SlateMaterialBrush.h"
#include "SNumericEntryBox.h"

#if !UE_SERVER
#include "Widgets/SUTColorPicker.h"

void SUTWeaponConfigDialog::Construct(const FArguments& InArgs)
{
	SUTDialogBase::Construct(SUTDialogBase::FArguments()
		.PlayerOwner(InArgs._PlayerOwner)
		.DialogTitle(NSLOCTEXT("SUTMenuBase", "WeaponSettings", "Weapon Settings"))
		.DialogSize(InArgs._DialogSize)
		.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
		.DialogPosition(InArgs._DialogPosition)
		.DialogAnchorPoint(InArgs._DialogAnchorPoint)
		.ContentPadding(InArgs._ContentPadding)
		.ButtonMask(UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_CANCEL)
		.OnDialogResult(InArgs._OnDialogResult)
		);


	UUTProfileSettings* ProfileSettings = GetPlayerOwner()->GetProfileSettings();
	AUTHUD* Hud = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>();
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
					if (!TestClass->GetDefaultObject<AUTWeapon>()->bHideInMenus && (TestClass->GetDefaultObject<AUTWeapon>()->DefaultGroup > 0))
					{
						if (WeaponClassList.Find(TestClass) == INDEX_NONE)
						{
							//Add weapons for the priority list
							int32 Group = (TestClass->GetDefaultObject<AUTWeapon>()->Group >= 0) ? TestClass->GetDefaultObject<AUTWeapon>()->Group : TestClass->GetDefaultObject<AUTWeapon>()->DefaultGroup;
							Group = ProfileSettings && ProfileSettings->WeaponGroupLookup.Contains(*ClassPath) ? ProfileSettings->WeaponGroupLookup[*ClassPath].Group : Group;
							Group = FMath::Clamp<int32>(Group, 1,10);
							WeaponClassList.Add(TestClass);
							WeakWeaponClassList.Add(TestClass);
							WeaponGroups.Add(TestClass, Group);
						}
					}
				}
			}
		}
	}

	if (Hud)
	{
		bool bFoundGlobal = false;
		for (int32 i = 0; i < Hud->CrosshairInfos.Num(); i++)
		{
			TSharedPtr<FCrosshairInfo> NewCrosshairInfo = MakeShareable(new FCrosshairInfo());
			*NewCrosshairInfo = Hud->CrosshairInfos[i];
			CrosshairInfos.Add(NewCrosshairInfo);
			if (Hud->CrosshairInfos[i].WeaponClassName == TEXT("Global"))
			{
				bFoundGlobal = true;
			}
		}
		if (!bFoundGlobal)
		{
			CrosshairInfos.Add(MakeShareable(new FCrosshairInfo()));
		}
	}

	bCustomWeaponCrosshairs = Hud->bCustomWeaponCrosshairs;
	
	{//Gather all the crosshair classes
		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(UUTCrosshair::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL && !ClassPath->Contains(TEXT("/EpicInternal/"))) // exclude debug/test crosshairs
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(UUTCrosshair::StaticClass()))
				{
					CrosshairClassList.Add(TestClass);
					WeakCrosshairList.Add(TestClass);

					//Add weapon to a map for easy weapon class lookup
					CrosshairMap.Add(TestClass->GetPathName(), TestClass);
				}
			}
		}
	}
	
	{//Gather all the weapon skin classes
		TArray<FAssetData> AssetList;
		GetAllAssetData(UUTWeaponSkin::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			UUTWeaponSkin* EligibleWeaponSkin = Cast<UUTWeaponSkin>(Asset.GetAsset());
			if (EligibleWeaponSkin)
			{
				TArray<UUTWeaponSkin*>* WeaponSkinArray = WeaponToSkinListMap.Find(EligibleWeaponSkin->WeaponType.ToString());
				if (WeaponSkinArray == nullptr)
				{
					WeaponSkinArray = &(WeaponToSkinListMap.Add(EligibleWeaponSkin->WeaponType.ToString()));
				}

				if (WeaponSkinArray)
				{
					(*WeaponSkinArray).Add(EligibleWeaponSkin);
				}
			}
		}
	}

	//Set up the Crosshair WYSIWYG view
	//Just set to some samll size to satisfy hooking everything up. Will resize the rendertarget once we know the size Slate gives us for the crosshair image
	FVector2D CrosshairSize(1.0f, 1.0f);
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(NULL, TEXT("/Game/RestrictedAssets/UI/PlayerPreviewProxy.PlayerPreviewProxy"));
	if (BaseMat != NULL)
	{
		CrosshairPreviewTexture = Cast<UUTCanvasRenderTarget2D>(UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetPlayerOwner().Get(), UUTCanvasRenderTarget2D::StaticClass(), CrosshairSize.X, CrosshairSize.Y));
		CrosshairPreviewTexture->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		CrosshairPreviewTexture->OnNonUObjectRenderTargetUpdate.BindSP(this, &SUTWeaponConfigDialog::UpdateCrosshairRender);
		CrosshairPreviewMID = UMaterialInstanceDynamic::Create(BaseMat, CrosshairPreviewTexture);
		CrosshairPreviewMID->SetTextureParameterValue(FName(TEXT("TheTexture")), CrosshairPreviewTexture);
		CrosshairPreviewBrush = new FSlateMaterialBrush(*CrosshairPreviewMID, CrosshairSize);
	}
	else
	{
		CrosshairPreviewTexture = NULL;
		CrosshairPreviewMID = NULL;
		CrosshairPreviewBrush = new FSlateMaterialBrush(*UMaterial::GetDefaultMaterial(MD_Surface), CrosshairSize);
	}
	CrosshairPreviewTexture->TargetGamma = GEngine->GetDisplayGamma();


	if (ProfileSettings)
	{
		for (int32 i = 0; i < WeakWeaponClassList.Num(); i++)
		{
			float Pri = ProfileSettings->GetWeaponPriority(GetNameSafe(WeakWeaponClassList[i].Get()), WeakWeaponClassList[i]->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority);
			WeakWeaponClassList[i]->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority = Pri;
		}
	}

	struct FWeaponListSort
	{
		bool operator()(TWeakObjectPtr<UClass> A, TWeakObjectPtr<UClass> B) const
		{
			return (A->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority > B->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority);
		}
	};

	WeakWeaponClassList.Sort(FWeaponListSort());

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
		if (TestItem.Get()->EqualTo(WeaponHandDesc[uint8(GetDefault<AUTPlayerController>()->GetWeaponHand())]))
		{
			InitiallySelectedHand = TestItem;
		}
	}

	if (DialogContent.IsValid())
	{
		const float MessageTextPaddingX = 10.0f;
		TSharedPtr<STextBlock> MessageTextBlock;
		DialogContent->AddSlot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SBox).HeightOverride(80)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot().FillWidth(0.33).Padding(FMargin(20.0,0.0,10.0,0.0))
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight()			
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot().HAlign(HAlign_Left).AutoWidth()
							[
								SNew(STextBlock)
								.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
								.Text(NSLOCTEXT("SUTWeaponConfigDialog", "AutoWeaponSwitch", "Weapon Switch on Pickup"))
							]
							+ SHorizontalBox::Slot().FillWidth(1.0).HAlign(HAlign_Right)
							[
								SAssignNew(AutoWeaponSwitch, SCheckBox)
								.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
								.ForegroundColor(FLinearColor::White)
								.IsChecked(GetDefault<AUTPlayerController>()->bAutoWeaponSwitch ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked)
							]
						]
						+SVerticalBox::Slot().AutoHeight()			
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot().HAlign(HAlign_Left).AutoWidth()
							[
								SNew(STextBlock)
								.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
								.Text(NSLOCTEXT("SUTWeaponConfigDialog", "CustomWeaponCrosshairs", "Use Custom Weapon Crosshairs"))
							]
							+ SHorizontalBox::Slot().FillWidth(1.0).HAlign(HAlign_Right)
							[
								SNew(SCheckBox)
								.IsChecked(this, &SUTWeaponConfigDialog::GetCustomWeaponCrosshairs)
								.OnCheckStateChanged(this, &SUTWeaponConfigDialog::SetCustomWeaponCrosshairs)
								.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
							]
						]

					]

					+SHorizontalBox::Slot().AutoWidth().Padding(20.0f, 0.0f, 20.0f, 0.0f).VAlign(VAlign_Fill)
					[
						SNew(SBox).WidthOverride(2)
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
						]
					]

					+SHorizontalBox::Slot().FillWidth(0.34).Padding(FMargin(10.0,0.0,10.0,0.0))
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot().HAlign(HAlign_Left).AutoWidth()
							[
								SNew(STextBlock)
								.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
								.Text(NSLOCTEXT("SUTWeaponConfigDialog", "WeaponHand", "Weapon Hand"))
							]
							+ SHorizontalBox::Slot().FillWidth(1.0).HAlign(HAlign_Right)
							[
								SAssignNew(WeaponHand, SComboBox< TSharedPtr<FText> >)
								.InitiallySelectedItem(InitiallySelectedHand)
								.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
								.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
								.OptionsSource(&WeaponHandList)
								.OnGenerateWidget(this, &SUTWeaponConfigDialog::GenerateHandListWidget)
								.OnSelectionChanged(this, &SUTWeaponConfigDialog::OnHandSelected)
								.ContentPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
								.Content()
								[
									SAssignNew(SelectedWeaponHand, STextBlock)
									.Text(*InitiallySelectedHand.Get())
									.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
								]
							]
						]
					]
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(FMargin(25.0,0.0,25.0,0.0))
			[
				SNew(SBox).HeightOverride(2)
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SBox).HeightOverride(750)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot().AutoWidth()
					[
						// This is the Priority list / Weapon list
						SNew(SBox).WidthOverride(450)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.Padding(10.0f, 10.0f, 0.0f, 5.0f)
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot().FillWidth(1.0).VAlign(VAlign_Center).HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("SUTWeaponConfigDialog", "WeaponTitle", "Available Weapons"))
									.TextStyle(SUWindowsStyle::Get(),"UT.Common.BoldText")
								]
							]
							+ SVerticalBox::Slot()
							.Padding(10.0f, 5.0f, 0.0f, 1.0f)
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0.0f, 0.0f, 10.0f, 0.0f)
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Left)
								[
									SNew(SBox)
									.WidthOverride(440)
									.HeightOverride(600)
									[
										SNew(SBorder)
										.Content()
										[
											SAssignNew(WeaponList, SListView<TWeakObjectPtr<UClass>>)
											.SelectionMode(ESelectionMode::Single)
											.ListItemsSource(&WeakWeaponClassList)
											.OnSelectionChanged(this, &SUTWeaponConfigDialog::OnWeaponChanged)
											.OnGenerateRow(this, &SUTWeaponConfigDialog::GenerateWeaponListRow)
										]
									]
								]
							]

							+SVerticalBox::Slot()
							.Padding(10.0f, 10.0f, 0.0f, 5.0f)
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot().AutoWidth().Padding(FMargin(5.0,5.0,5.0,5.0))
								[
									SNew(SBox)
									.HeightOverride(48)
									.WidthOverride(48)
									[
										SNew(SButton)
										.HAlign(HAlign_Center)
										.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
										.ContentPadding(FMargin(4.0f, 4.0f, 4.0f, 4.0f))
										.OnClicked(this, &SUTWeaponConfigDialog::WeaponPriorityUp)
										.IsEnabled(this, &SUTWeaponConfigDialog::CanMoveWeaponPriorityUp)
										[
											SNew(SImage)
											.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.UpArrow"))
										]
									]
								]
								+SHorizontalBox::Slot().FillWidth(1.0).VAlign(VAlign_Center).HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("SUTWeaponConfigDialog", "WeaponPriorities", "Change Priorities"))
									.TextStyle(SUWindowsStyle::Get(),"UT.Common.BoldText")
								]
								+SHorizontalBox::Slot().AutoWidth().Padding(FMargin(5.0,5.0,5.0,5.0))
								[
									SNew(SBox)
									.HeightOverride(48)
									.WidthOverride(48)
									[
										SNew(SButton)
										.HAlign(HAlign_Center)
										.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
										.ContentPadding(FMargin(4.0f, 4.0f, 4.0f, 4.0f))
										.OnClicked(this, &SUTWeaponConfigDialog::WeaponPriorityDown)
										.IsEnabled(this, &SUTWeaponConfigDialog::CanMoveWeaponPriorityDown)
										[
											SNew(SImage)
											.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.DownArrow"))
										]

									]
								]
							]
						]
					]
					+SHorizontalBox::Slot().FillWidth(1.0).Padding(FMargin(20.0,20.0,20.0,20.0))
					[
						SAssignNew(TabWidget, SUTTabWidget)
					]
				]
			]

		];

		TabWidget->AddTab(FText::FromString(TEXT("Crosshairs & Groups")),
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10.0f, 0.0f, 0.0f, 0.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(0.0f, 10.0f, 0.0f, 5.0f)
				.AutoHeight()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SBox)
						.WidthOverride(300.0f)
						.HeightOverride(300.0f)
						.MaxDesiredWidth(300.0f)
						.MaxDesiredHeight(300.0f)
						[
							SAssignNew(CrosshairImage, SImage)
							.Image(CrosshairPreviewBrush)
						]
					]
				]
				+ SVerticalBox::Slot()
				.Padding(0.0f, 10.0f, 0.0f, 5.0f)
				.AutoHeight()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				[
					SAssignNew(CrosshairComboBox, SComboBox<TWeakObjectPtr<UClass> >)
					.InitiallySelectedItem(nullptr)
					.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
					.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
					.OptionsSource(&WeakCrosshairList)
					.OnGenerateWidget(this, &SUTWeaponConfigDialog::GenerateCrosshairRow)
					.OnSelectionChanged(this, &SUTWeaponConfigDialog::OnCrosshairSelected)
					.Content()
					[
						SAssignNew(CrosshairText, STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
					]
				]
				+ SVerticalBox::Slot()
				.Padding(0.0f, 10.0f, 0.0f, 5.0f)
				.AutoHeight()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(10.0f, 0.0f, 0.0f, 0.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SUTWeaponConfigDialog", "Scale", "Scale"))
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
					]
					+ SHorizontalBox::Slot()
					.Padding(10.0f, 0.0f, 0.0f, 0.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)
					[
						SNew(SBox)
						.MaxDesiredWidth(200)
						.Content()
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
				+ SVerticalBox::Slot()
				.Padding(0.0f, 10.0f, 0.0f, 5.0f)
				.AutoHeight()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Center)
				[
					SAssignNew(ColorOverlay, SOverlay)
				]
			]	
			+SHorizontalBox::Slot().Padding(20.0,0.0,0.0,0.0).FillWidth(1.0)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(0.0f, 10.0f, 0.0f, 5.0f)
				.AutoHeight()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(10.0f, 0.0f, 0.0f, 0.0f)
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Left)
					[
						SNew(SBox).WidthOverride(200)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUTWeaponConfigDialog", "Group", "Weapon Group"))
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
						]
					]
					+ SHorizontalBox::Slot().AutoWidth()
					.Padding(10.0f, 0.0f, 0.0f, 0.0f)
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Left)
					[
						SNew(SBox).WidthOverride(100)
						[
							SAssignNew(GroupEdit, SNumericEntryBox<int32>)
							.EditableTextBoxStyle(SUWindowsStyle::Get(), "UT.Common.Editbox.White")
							.Value(this, &SUTWeaponConfigDialog::GetWeaponGroup)
							.OnValueChanged(this, &SUTWeaponConfigDialog::SetWeaponGroup)
							.LabelPadding(FMargin(50.0f, 50.0f))
							.AllowSpin(true)
							.MinSliderValue(1)
							.MinValue(1)
							.MaxSliderValue(10)
							.MaxValue(10)
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0)
					.Padding(FMargin(10.0f,0.0f,10.0f,0.0f))
					.VAlign(VAlign_Top)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
							.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f))
							.Text(NSLOCTEXT("SUTWeaponConfigDialog", "WepsInGroupText", "Weapons Also in this group..."))
						]
						+SVerticalBox::Slot().AutoHeight()
						[
							SAssignNew(WeaponsInGroupText, STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.SmallText")
							.Text(FText::GetEmpty())
							.AutoWrapText(true)
						]
					]
				]
				+ SVerticalBox::Slot()
				.Padding(0.0f, 10.0f, 0.0f, 5.0f)
				.AutoHeight()
				.HAlign(HAlign_Fill)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(200)
						[
							SNew(SButton)
							.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
							.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
							.Text(NSLOCTEXT("SUTWeaponConfigDialog", "DefaultGroups", "Reset Groups"))
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText.Black")
							.OnClicked(this, &SUTWeaponConfigDialog::DefaultGroupsClicked)
						]
					]
				]
			]
		]);
	
		WeaponList->SetSelection(WeakWeaponClassList[0]);
		
		WeaponSkinList.Add(MakeShareable(new FString(TEXT("No Skins Available"))));

		TabWidget->AddTab(FText::FromString(TEXT("Weapon Skins")),
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10.0f, 0.0f, 0.0f, 0.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(0.0f, 10.0f, 0.0f, 5.0f)
				.AutoHeight()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				[
					SAssignNew(WeaponSkinComboBox, SComboBox< TSharedPtr<FString> >)
					.InitiallySelectedItem(nullptr)
					.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
					.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
					.OptionsSource(&WeaponSkinList)
					.OnSelectionChanged(this, &SUTWeaponConfigDialog::OnWeaponSkinSelected)
					.OnGenerateWidget(this, &SUTWeaponConfigDialog::GenerateStringListWidget)
					.Content()
					[
						SAssignNew(WeaponSkinText, STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
					]
				]
			]
		]);

		WeaponSkinText->SetText(*WeaponSkinList[0].Get());

		SetCustomWeaponCrosshairs(bCustomWeaponCrosshairs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

		//Color picker needs to be created after the weapon has been selected due to a SColorPicker animation quirk
		ColorOverlay->AddSlot()
		[
			SAssignNew(ColorPicker, SUTColorPicker)
			.TargetColorAttribute(this, &SUTWeaponConfigDialog::GetCrosshairColor)
			.OnColorCommitted(this, &SUTWeaponConfigDialog::SetCrosshairColor)
			.PreColorCommitted(this, &SUTWeaponConfigDialog::SetCrosshairColor)
			.DisplayInlineVersion(true)
			.UseAlpha(true)
		];
	}
}

void SUTWeaponConfigDialog::OnWeaponChanged(TWeakObjectPtr<UClass> NewSelectedWeapon, ESelectInfo::Type SelectInfo)
{
	SelectedWeapon = NewSelectedWeapon;
	for (int32 i=0; i < CrosshairInfos.Num(); i++)
	{
		if (CrosshairInfos[i]->WeaponClassName == SelectedWeapon.Get()->GetPathName())
		{
			SelectedCrosshairInfo = CrosshairInfos[i];
			if (SelectedCrosshairInfo.IsValid() && CrosshairMap.Contains(SelectedCrosshairInfo->CrosshairClassName))
			{
				//Change the crosshair combobox text
				UClass* SelectedCrosshair = CrosshairMap[SelectedCrosshairInfo->CrosshairClassName].Get();
				CrosshairComboBox->SetSelectedItem(SelectedCrosshair);
		
				//Update the color
				if (ColorPicker.IsValid())
				{
					ColorPicker->UTSetNewTargetColorRGB(SelectedCrosshairInfo->Color);
				}
			}
		}
	}

	UpdateWeaponsInGroup();

	UpdateAvailableWeaponSkins();
}

void SUTWeaponConfigDialog::UpdateCrosshairRender(UCanvas* C, int32 Width, int32 Height)
{
	if (SelectedCrosshairInfo.IsValid() && CrosshairMap.Contains(SelectedCrosshairInfo->CrosshairClassName))
	{
		UClass* SelectedCrosshair = CrosshairMap[SelectedCrosshairInfo->CrosshairClassName].Get();
		if (SelectedCrosshair != nullptr)
		{
			SelectedCrosshair->GetDefaultObject<UUTCrosshair>()->DrawPreviewCrosshair(C, nullptr, 0.0f, SelectedCrosshairInfo->Scale, SelectedCrosshairInfo->Color);
		}
	}
}

void SUTWeaponConfigDialog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	//Find the geometry of the Crosshair Image
	TSet< TSharedRef<SWidget> > WidgetsToFind;
	{
		WidgetsToFind.Add(CrosshairImage.ToSharedRef());
	}
	TMap<TSharedRef<SWidget>, FArrangedWidget> Result;
	FindChildGeometries(AllottedGeometry, WidgetsToFind, Result);

	//Check to see if the size of our render target matches what Slate has given us. Resize if needed
	if (Result.Contains(CrosshairImage.ToSharedRef()))
	{
		FGeometry CrosshairGeomety = Result[CrosshairImage.ToSharedRef()].Geometry;
		FVector2D WantedSize = CrosshairGeomety.Size * CrosshairGeomety.Scale;
		WantedSize.X = FMath::RoundToFloat(WantedSize.X);
		WantedSize.Y = FMath::RoundToFloat(WantedSize.Y);

		FVector2D CrosshairSize(CrosshairPreviewTexture->SizeX, CrosshairPreviewTexture->SizeY);
		
		if (WantedSize != CrosshairSize)
		{
			CrosshairPreviewTexture->InitCustomFormat(WantedSize.X, WantedSize.Y, PF_B8G8R8A8, false);
			CrosshairPreviewTexture->UpdateResourceImmediate();
		}
	}

	//Draw the crosshair
	CrosshairPreviewTexture->UpdateResource();

	return SUTDialogBase::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

void SUTWeaponConfigDialog::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(WeaponClassList);
	Collector.AddReferencedObjects(CrosshairClassList);
	Collector.AddReferencedObject(CrosshairPreviewTexture);
	Collector.AddReferencedObject(CrosshairPreviewMID);

	for (auto WeaponToSkinListMapPair : WeaponToSkinListMap)
	{
		Collector.AddReferencedObjects(WeaponToSkinListMapPair.Value);
	}
}

void SUTWeaponConfigDialog::OnCrosshairSelected(TWeakObjectPtr<UClass> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectedCrosshairInfo.IsValid())
	{
		SelectedCrosshairInfo->CrosshairClassName = NewSelection->GetPathName();
		CrosshairPreviewTexture->UpdateResource();
		CrosshairText->SetText(NewSelection->GetDefaultObject<UUTCrosshair>()->CrosshairName);
	}
}

ECheckBoxState SUTWeaponConfigDialog::GetCustomWeaponCrosshairs() const
{
	return bCustomWeaponCrosshairs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SUTWeaponConfigDialog::SetCustomWeaponCrosshairs(ECheckBoxState NewState)
{
	bCustomWeaponCrosshairs = NewState == ECheckBoxState::Checked;
}

TSharedRef<SWidget> SUTWeaponConfigDialog::GenerateCrosshairRow(TWeakObjectPtr<UClass> CrosshairClass)
{	
	return 	SNew(STextBlock)
		.Text(CrosshairClass->GetDefaultObject<UUTCrosshair>()->CrosshairName)
		.TextStyle(SUTStyle::Get(), "UT.Font.ContextMenuItem");
}

TSharedRef<ITableRow> SUTWeaponConfigDialog::GenerateWeaponListRow(TWeakObjectPtr<UClass> WeaponType, const TSharedRef<STableViewBase>& OwningList)
{
	checkSlow(WeaponType->IsChildOf(AUTWeapon::StaticClass()));

	return SNew(STableRow<TWeakObjectPtr<UClass>>, OwningList)
		.Padding(5)
		[
			SNew(STextBlock)
			.Text(WeaponType->GetDefaultObject<AUTWeapon>()->DisplayName)
			.TextStyle(SUWindowsStyle::Get(),"UT.Common.NormalText")
		];
}

TSharedRef<SWidget> SUTWeaponConfigDialog::GenerateHandListWidget(TSharedPtr<FText> InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.Text(*InItem.Get())
			.TextStyle(SUTStyle::Get(), "UT.Font.ContextMenuItem")
		];
}

void SUTWeaponConfigDialog::OnHandSelected(TSharedPtr<FText> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedWeaponHand->SetText(*NewSelection.Get());
}

FReply SUTWeaponConfigDialog::WeaponPriorityUp()
{
	TArray<TWeakObjectPtr<UClass>> SelectedItems = WeaponList->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		int32 Index = WeakWeaponClassList.Find(SelectedItems[0]);
		if (Index > 0)
		{
			Exchange(WeakWeaponClassList[Index], WeakWeaponClassList[Index - 1]);
			WeaponList->RequestListRefresh();
		}
	}
	return FReply::Handled();
}
FReply SUTWeaponConfigDialog::WeaponPriorityDown()
{
	TArray<TWeakObjectPtr<UClass>> SelectedItems = WeaponList->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		int32 Index = WeakWeaponClassList.Find(SelectedItems[0]);
		if (Index != INDEX_NONE && Index < WeakWeaponClassList.Num() - 1)
		{
			Exchange(WeakWeaponClassList[Index], WeakWeaponClassList[Index + 1]);
			WeaponList->RequestListRefresh();
		}
	}
	return FReply::Handled();
}

FReply SUTWeaponConfigDialog::OKClick()
{
	EWeaponHand NewHand = EWeaponHand::HAND_Right;
	for (int32 i = 0; i < WeaponHandDesc.Num(); i++)
	{
		if (WeaponHandDesc[i].EqualTo(*WeaponHand->GetSelectedItem().Get()))
		{
			NewHand = EWeaponHand(i);
			break;
		}
	}

	AUTPlayerController* UTPlayerController = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	if (UTPlayerController != NULL)
	{
		UTPlayerController->bAutoWeaponSwitch = AutoWeaponSwitch->IsChecked();
		UTPlayerController->SetWeaponHand(NewHand);
		UTPlayerController->SaveConfig();
	}
	else
	{
		AUTPlayerController* DefaultPC = AUTPlayerController::StaticClass()->GetDefaultObject<AUTPlayerController>();
		DefaultPC->bAutoWeaponSwitch = AutoWeaponSwitch->IsChecked();
		DefaultPC->SetWeaponHand(NewHand);
		DefaultPC->SaveConfig();
	}

	UUTProfileSettings* ProfileSettings = GetPlayerOwner()->GetProfileSettings();
	if (ProfileSettings)
	{
		// Clear out existing data
		ProfileSettings->WeaponGroups.Empty();
		ProfileSettings->WeaponGroupLookup.Empty();
	}
	// note that the array mirrors the list widget so we can use it directly
	for (int32 i = 0; i < WeakWeaponClassList.Num(); i++)
	{
		float NewPriority = float(WeakWeaponClassList.Num() - i); // top of list is highest priority
		int32 NewGroup = FMath::Clamp<int32>(WeaponGroups[WeakWeaponClassList[i].Get()], 1, 10);
		WeakWeaponClassList[i]->GetDefaultObject<AUTWeapon>()->Group = NewGroup;
		WeakWeaponClassList[i]->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority = NewPriority;
		WeakWeaponClassList[i]->GetDefaultObject<AUTWeapon>()->SaveConfig();
		// update instances so changes take effect immediately
		if (UTPlayerController != NULL)
		{
			for (TActorIterator<AUTWeapon> It(UTPlayerController->GetWorld()); It; ++It)
			{
				if (It->GetClass() == WeakWeaponClassList[i])
				{
					It->AutoSwitchPriority = NewPriority;
				}
			}
		}
		if (ProfileSettings != NULL)
		{
			ProfileSettings->SetWeaponPriority(GetNameSafe(WeakWeaponClassList[i].Get()), WeakWeaponClassList[i]->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority);
		}
	}

	if (ProfileSettings)
	{
		for (int32 i = 0; i < WeaponClassList.Num(); i++)
		{
			int32 NewGroup = FMath::Clamp<int32>(WeaponGroups[WeaponClassList[i]],1,10);
			FString ClassPath = GetNameSafe(WeaponClassList[i]);

			FStoredWeaponGroupInfo WeaponInfo(ClassPath, NewGroup);
			ProfileSettings->WeaponGroups.Add(WeaponInfo);
			ProfileSettings->WeaponGroupLookup.Add(ClassPath, WeaponInfo);
			WeaponClassList[i]->GetDefaultObject<AUTWeapon>()->Group = NewGroup;
			WeaponClassList[i]->GetDefaultObject<AUTWeapon>()->SaveConfig();
		}
	}

	AUTHUD* DefaultHud = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>();
	for (auto CrosshairInfo : CrosshairInfos)
	{
		//Update or add a new one if not found
		int32 Index = DefaultHud->CrosshairInfos.Find(*CrosshairInfo.Get());
		if (Index != INDEX_NONE)
		{
			DefaultHud->CrosshairInfos[Index] = *CrosshairInfo.Get();
		}
		else
		{
			DefaultHud->CrosshairInfos.Add(*CrosshairInfo.Get());
		}
	}
	DefaultHud->LoadedCrosshairs.Empty();
	DefaultHud->bCustomWeaponCrosshairs = bCustomWeaponCrosshairs;
	DefaultHud->SaveConfig();

	// Copy settings to existing actors
	if (UTPlayerController)
	{
		for (FActorIterator It(UTPlayerController->GetWorld()); It; ++It)
		{
			AUTWeapon* Weapon = Cast<AUTWeapon>(*It);
			if (Weapon)
			{
				Weapon->Group = Weapon->GetClass()->GetDefaultObject<AUTWeapon>()->Group;
				Weapon->AutoSwitchPriority = Weapon->GetClass()->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority;
			}
			AUTHUD* HUD = Cast<AUTHUD>(*It);
			if (HUD)
			{
				HUD->LoadedCrosshairs.Empty();
				HUD->CrosshairInfos = DefaultHud->CrosshairInfos;
				HUD->bCustomWeaponCrosshairs = DefaultHud->bCustomWeaponCrosshairs;
			}
		}
	}

	if (ProfileSettings != nullptr)
	{
		ProfileSettings->CrosshairInfos = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->CrosshairInfos;
		ProfileSettings->bCustomWeaponCrosshairs = bCustomWeaponCrosshairs;

		for (auto& Selection : WeaponSkinSelection)
		{
			// Find the weapon skin pointer for this selection
			TArray<UUTWeaponSkin*>* SkinArrayPtr = WeaponToSkinListMap.Find(Selection.Key);
			if (SkinArrayPtr)
			{
				TArray<UUTWeaponSkin*>& SkinArray = *SkinArrayPtr;

				UUTWeaponSkin* Skin = nullptr;
				for (int32 SkinIter = 0; SkinIter < SkinArray.Num(); SkinIter++)
				{
					if (SkinArray[SkinIter]->DisplayName.ToString() == Selection.Value)
					{
						Skin = SkinArray[SkinIter];
						break;
					}
				}

				if (Skin)
				{
					// First check for an existing entry in profile
					bool bFoundInProfile = false;
					for (int i = 0; i < ProfileSettings->WeaponSkins.Num(); i++)
					{
						if (ProfileSettings->WeaponSkins[i] && ProfileSettings->WeaponSkins[i]->WeaponType == Selection.Key)
						{
							ProfileSettings->WeaponSkins[i] = Skin;
							bFoundInProfile = true;
							break;
						}
					}
					if (!bFoundInProfile)
					{
						ProfileSettings->WeaponSkins.Add(Skin);
					}
				}
				else
				{
					// User selected "No Skin"
					for (int i = 0; i < ProfileSettings->WeaponSkins.Num(); i++)
					{
						if (ProfileSettings->WeaponSkins[i] && ProfileSettings->WeaponSkins[i]->WeaponType == Selection.Key)
						{
							ProfileSettings->WeaponSkins.RemoveAt(i);
							break;
						}
					}
				}
			}
		}
	}

	GetPlayerOwner()->SaveProfileSettings();
	GetPlayerOwner()->CloseDialog(SharedThis(this));

	return FReply::Handled();
}

FReply SUTWeaponConfigDialog::CancelClick()
{
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

FReply SUTWeaponConfigDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK)
	{
		OKClick();
	}
	else if (ButtonID == UTDIALOG_BUTTON_CANCEL)
	{
		CancelClick();
	}
	return FReply::Handled();
}

bool SUTWeaponConfigDialog::CanMoveWeaponPriorityUp() const
{
	auto SelectedItems = WeaponList->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		int32 Index = WeakWeaponClassList.Find(SelectedItems[0]);
		if (Index != INDEX_NONE && Index != 0)
		{
			return true;
		}
	}
	return false;
}

bool SUTWeaponConfigDialog::CanMoveWeaponPriorityDown() const
{
	auto SelectedItems = WeaponList->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		int32 Index = WeakWeaponClassList.Find(SelectedItems[0]);
		if (Index != INDEX_NONE && Index < WeakWeaponClassList.Num() - 1)
		{
			return true;
		}
	}
	return false;
}

TOptional<int32> SUTWeaponConfigDialog::GetWeaponGroup() const
{
	return WeaponGroups.Contains(SelectedWeapon.Get()) ? TOptional<int32>(WeaponGroups[SelectedWeapon.Get()]) : TOptional<int32>(0);
}


void SUTWeaponConfigDialog::SetWeaponGroup(int32 NewGroup)
{
	if (WeaponGroups.Contains(SelectedWeapon.Get()))
	{
		WeaponGroups[SelectedWeapon.Get()] = NewGroup;
		UpdateWeaponsInGroup();
	}
}

void SUTWeaponConfigDialog::UpdateWeaponsInGroup()
{
	FString Text = TEXT("");
	if (SelectedWeapon.IsValid())
	{
		int32 CurrentGroup = WeaponGroups.Contains(SelectedWeapon.Get()) ? WeaponGroups[SelectedWeapon.Get()] : -1;
		for (int32 i=0; i < WeaponClassList.Num(); i++)
		{
			int32 Grp = WeaponGroups.Contains(WeaponClassList[i]) ? WeaponGroups[WeaponClassList[i]] : -1;
			if (Grp == CurrentGroup)
			{
				if (!Text.IsEmpty()) 
				{
					Text = Text + TEXT(", ");
				}
				Text = Text + WeaponClassList[i]->GetDefaultObject<AUTWeapon>()->DisplayName.ToString();
			}
		}
	}

	WeaponsInGroupText->SetText(FText::FromString(Text));
}

FReply SUTWeaponConfigDialog::DefaultGroupsClicked()
{
	for (int i = 0; i < WeaponClassList.Num(); i++)
	{
		WeaponGroups[WeaponClassList[i]] = WeaponClassList[i]->GetDefaultObject<AUTWeapon>()->DefaultGroup;
	}

	return FReply::Handled();
}

void SUTWeaponConfigDialog::OnWeaponSkinSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (NewSelection.IsValid())
	{
		WeaponSkinText->SetText(*NewSelection.Get());
		if (SelectedWeapon.IsValid())
		{
			WeaponSkinSelection.Add(SelectedWeapon->GetPathName(), *NewSelection.Get());
		}
	}
}

TSharedRef<SWidget> SUTWeaponConfigDialog::GenerateStringListWidget(TSharedPtr<FString> InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::White)
			.Text(FText::FromString(*InItem.Get()))
		];
}

void SUTWeaponConfigDialog::UpdateAvailableWeaponSkins()
{
	// We have not been initialized yet
	if (!WeaponSkinComboBox.IsValid())
	{
		return;
	}

	WeaponSkinList.Empty();
	WeaponSkinList.Add(MakeShareable(new FString(TEXT("No Skin"))));

	if (SelectedWeapon.IsValid())
	{
		FString SelectedWeaponPath = SelectedWeapon->GetPathName();
		TArray<UUTWeaponSkin*>* AvailableSkins = WeaponToSkinListMap.Find(SelectedWeaponPath);
		if (AvailableSkins)
		{
			for (auto Skin : *AvailableSkins)
			{
				WeaponSkinList.Add(MakeShareable(new FString(Skin->DisplayName.ToString())));
			}
		}

		// Check if we have an assigned skin in the pre-confirmation cache
		FString* SelectedSkin = WeaponSkinSelection.Find(SelectedWeaponPath);
		if (SelectedSkin)
		{
			for (int i = 0; i < WeaponSkinList.Num(); i++)
			{
				if (*WeaponSkinList[i].Get() == *SelectedSkin)
				{
					WeaponSkinComboBox->SetSelectedItem(WeaponSkinList[i]);
					return;
				}
			}
		}
		else
		{
			// If haven't picked one this session, highlight the one that was in profile settings
			UUTProfileSettings* ProfileSettings = GetPlayerOwner()->GetProfileSettings();
			if (ProfileSettings)
			{
				for (int32 j = 0; j < ProfileSettings->WeaponSkins.Num(); j++)
				{
					if (ProfileSettings->WeaponSkins[j] && ProfileSettings->WeaponSkins[j]->WeaponType.ToString() == SelectedWeaponPath)
					{
						for (int i = 0; i < WeaponSkinList.Num(); i++)
						{
							if (*WeaponSkinList[i].Get() == ProfileSettings->WeaponSkins[j]->DisplayName.ToString())
							{
								WeaponSkinComboBox->SetSelectedItem(WeaponSkinList[i]);
								return;
							}
						}
					}
				}
			}
		}
	}

	WeaponSkinComboBox->SetSelectedItem(WeaponSkinList[0]);
}

#endif