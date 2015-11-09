// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "../Public/UnrealTournament.h"
#include "SUWWeaponConfigDialog.h"
#include "SUWindowsStyle.h"
#include "SUTStyle.h"
#include "Widgets/SUTTabWidget.h"
#include "UTCrosshair.h"
#include "../Public/UTCanvasRenderTarget2D.h"
#include "EngineModule.h"
#include "SlateMaterialBrush.h"
#include "SNumericEntryBox.h"

#if !UE_SERVER
#include "Widgets/SUTColorPicker.h"

void SUWWeaponConfigDialog::Construct(const FArguments& InArgs)
{
	SUWDialog::Construct(SUWDialog::FArguments()
		.PlayerOwner(InArgs._PlayerOwner)
		.DialogTitle(NSLOCTEXT("SUWindowsDesktop", "WeaponSettings", "Weapon Settings"))
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
					if (!TestClass->GetDefaultObject<AUTWeapon>()->bHideInMenus)
					{
						//Add weapons for the priority list

						int32 Group = ProfileSettings && ProfileSettings->WeaponGroupLookup.Contains(*ClassPath) ? ProfileSettings->WeaponGroupLookup[*ClassPath].Group : TestClass->GetDefaultObject<AUTWeapon>()->Group;
						Group = FMath::Clamp<int32>(Group, 1,10);
						WeaponClassList.Add(TestClass);
						WeakWeaponClassList.Add(TestClass);
						WeaponGroups.Add(TestClass, Group);
						UE_LOG(UT,Log,TEXT("###### %s"),*GetNameSafe(TestClass));
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

	//Set up the Crosshair WYSIWYG view
	//Just set to some samll size to satisfy hooking everything up. Will resize the rendertarget once we know the size Slate gives us for the crosshair image
	FVector2D CrosshairSize(1.0f, 1.0f);
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(NULL, TEXT("/Game/RestrictedAssets/UI/PlayerPreviewProxy.PlayerPreviewProxy"));
	if (BaseMat != NULL)
	{
		CrosshairPreviewTexture = Cast<UUTCanvasRenderTarget2D>(UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetPlayerOwner().Get(), UUTCanvasRenderTarget2D::StaticClass(), CrosshairSize.X, CrosshairSize.Y));
		CrosshairPreviewTexture->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		CrosshairPreviewTexture->OnNonUObjectRenderTargetUpdate.BindSP(this, &SUWWeaponConfigDialog::UpdateCrosshairRender);
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

	WeaponHandList.Add(MakeShareable(new FText(WeaponHandDesc[HAND_Right])));
	//WeaponHandList.Add(MakeShareable(new FText(WeaponHandDesc[HAND_Left])));
	//WeaponHandList.Add(MakeShareable(new FText(WeaponHandDesc[HAND_Center])));
	WeaponHandList.Add(MakeShareable(new FText(WeaponHandDesc[HAND_Hidden])));

	TSharedPtr<FText> InitiallySelectedHand = WeaponHandList[0];
	for (TSharedPtr<FText> TestItem : WeaponHandList)
	{
		if (TestItem.Get()->EqualTo(WeaponHandDesc[GetDefault<AUTPlayerController>()->GetWeaponHand()]))
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
								.Text(NSLOCTEXT("SUWWeaponConfigDialog", "AutoWeaponSwitch", "Weapon Switch on Pickup"))
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
								.Text(NSLOCTEXT("SUWWeaponConfigDialog", "CustomWeaponCrosshairs", "Use Custom Weapon Crosshairs"))
							]
							+ SHorizontalBox::Slot().FillWidth(1.0).HAlign(HAlign_Right)
							[
								SNew(SCheckBox)
								.IsChecked(this, &SUWWeaponConfigDialog::GetCustomWeaponCrosshairs)
								.OnCheckStateChanged(this, &SUWWeaponConfigDialog::SetCustomWeaponCrosshairs)
								.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
							]
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
								.Text(NSLOCTEXT("SUWWeaponConfigDialog", "WeaponHand", "Weapon Hand"))
							]
							+ SHorizontalBox::Slot().FillWidth(1.0).HAlign(HAlign_Right)
							[
								SAssignNew(WeaponHand, SComboBox< TSharedPtr<FText> >)
								.InitiallySelectedItem(InitiallySelectedHand)
								.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
								.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
								.OptionsSource(&WeaponHandList)
								.OnGenerateWidget(this, &SUWWeaponConfigDialog::GenerateHandListWidget)
								.OnSelectionChanged(this, &SUWWeaponConfigDialog::OnHandSelected)
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
					+SHorizontalBox::Slot().FillWidth(0.33).Padding(FMargin(10.0,0.0,20.0,0.0))
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight()			
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot().HAlign(HAlign_Left).AutoWidth()
							[
								SNew(STextBlock)
								.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
								.Text(NSLOCTEXT("SUWWeaponConfigDialog", "ClassicGroups", "Classic Weapon Groups"))
							]
							+ SHorizontalBox::Slot().FillWidth(1.0).HAlign(HAlign_Right)
							[
								SAssignNew(ClassicGroups, SCheckBox)
								.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
								.ForegroundColor(FLinearColor::White)
								.IsChecked(GetDefault<AUTPlayerController>()->bUseClassicGroups ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked)
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
									.Text(NSLOCTEXT("SUWWeaponConfigDialog", "WeaponTitle", "Available Weapons"))
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
											.OnSelectionChanged(this, &SUWWeaponConfigDialog::OnWeaponChanged)
											.OnGenerateRow(this, &SUWWeaponConfigDialog::GenerateWeaponListRow)
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
										.OnClicked(this, &SUWWeaponConfigDialog::WeaponPriorityUp)
										.IsEnabled(this, &SUWWeaponConfigDialog::CanMoveWeaponPriorityUp)
										[
											SNew(SImage)
											.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.UpArrow"))
										]
									]
								]
								+SHorizontalBox::Slot().FillWidth(1.0).VAlign(VAlign_Center).HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("SUWWeaponConfigDialog", "WeaponPriorities", "Change Priorities"))
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
										.OnClicked(this, &SUWWeaponConfigDialog::WeaponPriorityDown)
										.IsEnabled(this, &SUWWeaponConfigDialog::CanMoveWeaponPriorityDown)
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
					.OnGenerateWidget(this, &SUWWeaponConfigDialog::GenerateCrosshairRow)
					.OnSelectionChanged(this, &SUWWeaponConfigDialog::OnCrosshairSelected)
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
						.Text(NSLOCTEXT("SUWWeaponConfigDialog", "Scale", "Scale"))
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
							.Value(this, &SUWWeaponConfigDialog::GetCrosshairScale)
							.OnValueChanged(this, &SUWWeaponConfigDialog::SetCrosshairScale)
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
							.Text(NSLOCTEXT("SUWWeaponConfigDialog", "Group", "Weapon Group"))
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
							.Value(this, &SUWWeaponConfigDialog::GetWeaponGroup)
							.OnValueChanged(this, &SUWWeaponConfigDialog::SetWeaponGroup)
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
							.Text(NSLOCTEXT("SUWWeaponConfigDialog", "WepsInGroupText", "Weapons Also in this group..."))
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
							.Text(NSLOCTEXT("SUWWeaponConfigDialog", "DefaultGroups", "Reset Groups"))
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText.Black")
							.OnClicked(this, &SUWWeaponConfigDialog::DefaultGroupsClicked)
						]
					]
				]
			]
		]);
	
		WeaponList->SetSelection(WeakWeaponClassList[0]);

		TabWidget->AddTab(FText::FromString(TEXT("Weapon Skins")),
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SCanvas)
		]);

		SetCustomWeaponCrosshairs(bCustomWeaponCrosshairs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

		//Color picker needs to be created after the weapon has been selected due to a SColorPicker animation quirk
		ColorOverlay->AddSlot()
		[
			SAssignNew(ColorPicker, SUTColorPicker)
			.TargetColorAttribute(this, &SUWWeaponConfigDialog::GetCrosshairColor)
			.OnColorCommitted(this, &SUWWeaponConfigDialog::SetCrosshairColor)
			.PreColorCommitted(this, &SUWWeaponConfigDialog::SetCrosshairColor)
			.DisplayInlineVersion(true)
			.UseAlpha(true)
		];
	}
}

void SUWWeaponConfigDialog::OnWeaponChanged(TWeakObjectPtr<UClass> NewSelectedWeapon, ESelectInfo::Type SelectInfo)
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

}

void SUWWeaponConfigDialog::UpdateCrosshairRender(UCanvas* C, int32 Width, int32 Height)
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

void SUWWeaponConfigDialog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
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

	return SUWDialog::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

void SUWWeaponConfigDialog::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(WeaponClassList);
	Collector.AddReferencedObjects(CrosshairClassList);
	Collector.AddReferencedObject(CrosshairPreviewTexture);
	Collector.AddReferencedObject(CrosshairPreviewMID);
}

void SUWWeaponConfigDialog::OnCrosshairSelected(TWeakObjectPtr<UClass> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectedCrosshairInfo.IsValid())
	{
		SelectedCrosshairInfo->CrosshairClassName = NewSelection->GetPathName();
		CrosshairPreviewTexture->UpdateResource();
		CrosshairText->SetText(NewSelection->GetDefaultObject<UUTCrosshair>()->CrosshairName);
	}
}

ECheckBoxState SUWWeaponConfigDialog::GetCustomWeaponCrosshairs() const
{
	return bCustomWeaponCrosshairs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SUWWeaponConfigDialog::SetCustomWeaponCrosshairs(ECheckBoxState NewState)
{
	bCustomWeaponCrosshairs = NewState == ECheckBoxState::Checked;
}

TSharedRef<SWidget> SUWWeaponConfigDialog::GenerateCrosshairRow(TWeakObjectPtr<UClass> CrosshairClass)
{	
	return 	SNew(STextBlock)
		.Text(CrosshairClass->GetDefaultObject<UUTCrosshair>()->CrosshairName)
		.TextStyle(SUTStyle::Get(), "UT.Font.ContextMenuItem");
}

TSharedRef<ITableRow> SUWWeaponConfigDialog::GenerateWeaponListRow(TWeakObjectPtr<UClass> WeaponType, const TSharedRef<STableViewBase>& OwningList)
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

TSharedRef<SWidget> SUWWeaponConfigDialog::GenerateHandListWidget(TSharedPtr<FText> InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.Text(*InItem.Get())
			.TextStyle(SUTStyle::Get(), "UT.Font.ContextMenuItem")
		];
}

void SUWWeaponConfigDialog::OnHandSelected(TSharedPtr<FText> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedWeaponHand->SetText(*NewSelection.Get());
}

FReply SUWWeaponConfigDialog::WeaponPriorityUp()
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
FReply SUWWeaponConfigDialog::WeaponPriorityDown()
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

FReply SUWWeaponConfigDialog::OKClick()
{
	EWeaponHand NewHand = HAND_Right;
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
		UTPlayerController->bUseClassicGroups = ClassicGroups->GetCheckedState() == ECheckBoxState::Checked;
		UTPlayerController->SetWeaponHand(NewHand);
		UTPlayerController->SaveConfig();
	}
	else
	{
		AUTPlayerController* DefaultPC = AUTPlayerController::StaticClass()->GetDefaultObject<AUTPlayerController>();
		DefaultPC->bUseClassicGroups = ClassicGroups->GetCheckedState() == ECheckBoxState::Checked;
		DefaultPC->bAutoWeaponSwitch = AutoWeaponSwitch->IsChecked();
		DefaultPC->SetWeaponHand(NewHand);
		DefaultPC->SaveConfig();
	}

	UUTProfileSettings* ProfileSettings = GetPlayerOwner()->GetProfileSettings();

	// note that the array mirrors the list widget so we can use it directly
	for (int32 i = 0; i < WeakWeaponClassList.Num(); i++)
	{
		float NewPriority = float(WeakWeaponClassList.Num() - i); // top of list is highest priority
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

		// Clear out existing data
		ProfileSettings->WeaponGroups.Empty();
		ProfileSettings->WeaponGroupLookup.Empty();

		// Update it.
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

	//Copy crosshair settings to every hud class
	for (TObjectIterator<AUTHUD> It(EObjectFlags::RF_NoFlags, true); It; ++It)
	{
		(*It)->LoadedCrosshairs.Empty();
		(*It)->CrosshairInfos = DefaultHud->CrosshairInfos;
		(*It)->bCustomWeaponCrosshairs = DefaultHud->bCustomWeaponCrosshairs;
	}

	if (ProfileSettings != nullptr)
	{
		ProfileSettings->CrosshairInfos = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->CrosshairInfos;
		ProfileSettings->bCustomWeaponCrosshairs = bCustomWeaponCrosshairs;
	}

	GetPlayerOwner()->SaveProfileSettings();
	GetPlayerOwner()->CloseDialog(SharedThis(this));

	return FReply::Handled();
}

FReply SUWWeaponConfigDialog::CancelClick()
{
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

FReply SUWWeaponConfigDialog::OnButtonClick(uint16 ButtonID)
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

bool SUWWeaponConfigDialog::CanMoveWeaponPriorityUp() const
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

bool SUWWeaponConfigDialog::CanMoveWeaponPriorityDown() const
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

TOptional<int32> SUWWeaponConfigDialog::GetWeaponGroup() const
{
	return WeaponGroups.Contains(SelectedWeapon.Get()) ? TOptional<int32>(WeaponGroups[SelectedWeapon.Get()]) : TOptional<int32>(0);
}


void SUWWeaponConfigDialog::SetWeaponGroup(int32 NewGroup)
{
	if (WeaponGroups.Contains(SelectedWeapon.Get()))
	{
		WeaponGroups[SelectedWeapon.Get()] = NewGroup;
		UpdateWeaponsInGroup();
	}
}

void SUWWeaponConfigDialog::UpdateWeaponsInGroup()
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

FReply SUWWeaponConfigDialog::DefaultGroupsClicked()
{
	for (int i = 0; i < WeaponClassList.Num(); i++)
	{
		if (GetNameSafe(WeaponClassList[i]).Equals(TEXT("Enforcer_C"),ESearchCase::IgnoreCase))				WeaponGroups[WeaponClassList[i]] = 4;
		if (GetNameSafe(WeaponClassList[i]).Equals(TEXT("BP_LinkGun_C"), ESearchCase::IgnoreCase))			WeaponGroups[WeaponClassList[i]] = 1;
		if (GetNameSafe(WeaponClassList[i]).Equals(TEXT("BP_ImpactHammer_C"), ESearchCase::IgnoreCase))		WeaponGroups[WeaponClassList[i]] = 5;
		if (GetNameSafe(WeaponClassList[i]).Equals(TEXT("BP_FlakCannon_C"), ESearchCase::IgnoreCase))		WeaponGroups[WeaponClassList[i]] = 2;
		if (GetNameSafe(WeaponClassList[i]).Equals(TEXT("BP_Minigun_C"), ESearchCase::IgnoreCase))			WeaponGroups[WeaponClassList[i]] = 4;
		if (GetNameSafe(WeaponClassList[i]).Equals(TEXT("BP_Redeemer_C"), ESearchCase::IgnoreCase))			WeaponGroups[WeaponClassList[i]] = 5;
		if (GetNameSafe(WeaponClassList[i]).Equals(TEXT("BP_RocketLauncher_C"), ESearchCase::IgnoreCase))	WeaponGroups[WeaponClassList[i]] = 2;
		if (GetNameSafe(WeaponClassList[i]).Equals(TEXT("ShockRifle_C"), ESearchCase::IgnoreCase))			WeaponGroups[WeaponClassList[i]] = 3;
		if (GetNameSafe(WeaponClassList[i]).Equals(TEXT("BP_Sniper_C"), ESearchCase::IgnoreCase))			WeaponGroups[WeaponClassList[i]] = 3;
		if (GetNameSafe(WeaponClassList[i]).Equals(TEXT("BP_Translocator_C"), ESearchCase::IgnoreCase))		WeaponGroups[WeaponClassList[i]] = 4;
		if (GetNameSafe(WeaponClassList[i]).Equals(TEXT("BP_BioRifle_C"), ESearchCase::IgnoreCase))			WeaponGroups[WeaponClassList[i]] = 1;
	}

	return FReply::Handled();
}
#endif