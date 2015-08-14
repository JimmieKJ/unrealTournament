// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "../Public/UnrealTournament.h"
#include "SUWWeaponConfigDialog.h"
#include "SUWindowsStyle.h"
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
			if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTWeapon::StaticClass()) && !TestClass->GetDefaultObject<AUTWeapon>()->bHideInMenus)
			{
				//Only add crosshairs for new weapons that might not be in the config already
				FCrosshairInfo* FoundPtr = Hud->CrosshairInfos.FindByPredicate([TestClass](const FCrosshairInfo& Info)
				{
					return Info.WeaponClassName == TestClass->GetPathName();
				});

				if (FoundPtr == nullptr)
				{
					TSharedPtr<FCrosshairInfo> NewCrosshairInfo = MakeShareable(new FCrosshairInfo());
					NewCrosshairInfo->WeaponClassName = TestClass->GetPathName();
					NewCrosshairInfo->Color = FLinearColor::White;
					NewCrosshairInfo->Scale = 1.0f;
					CrosshairInfos.Add(NewCrosshairInfo);
				}
				else
				{
					TSharedPtr<FCrosshairInfo> NewCrosshairInfo = MakeShareable(new FCrosshairInfo());
					*NewCrosshairInfo = *FoundPtr;
					CrosshairInfos.Add(NewCrosshairInfo);
				}
					
				//Add weapon to a map for easy weapon class lookup
				WeaponMap.Add(TestClass->GetPathName(), TestClass);
				WeaponList.Add(TestClass);
			}
		}
	}
}

	bCustomWeaponCrosshairs = Hud->bCustomWeaponCrosshairs;

	//Check to see if we have a global crosshair. Add one if it is somehow missing
	{
		bool bFoundGlobal = false;
		for (int32 i = 0; i < CrosshairInfos.Num(); i++)
		{
			if (CrosshairInfos[i]->WeaponClassName == TEXT("Global"))
			{
				bFoundGlobal = true;
			}
		}
		if (!bFoundGlobal)
		{
			CrosshairInfos.Add(MakeShareable(new FCrosshairInfo()));
		}
	}

	//Sort alphabetically but keep the global one on top
	CrosshairInfos.Sort([](const TSharedPtr<FCrosshairInfo>& A, const TSharedPtr<FCrosshairInfo>& B) -> bool
	{
		if (A->WeaponClassName == TEXT("Global"))
		{
			return true;
		}
		else if (B->WeaponClassName == TEXT("Global"))
		{
			return false;
		}
		return A->WeaponClassName < B->WeaponClassName;
	});
	

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
					CrosshairList.Add(TestClass);
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


	UUTProfileSettings* ProfileSettings = GetPlayerOwner()->GetProfileSettings();
	if (ProfileSettings)
	{
		for (int32 i = 0; i < WeaponList.Num(); i++)
		{
			float Pri = ProfileSettings->GetWeaponPriority(GetNameSafe(WeaponList[i]), WeaponList[i]->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority);
			WeaponList[i]->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority = Pri;
		}
	}

	struct FWeaponListSort
	{
		bool operator()(UClass& A, UClass& B) const
		{
			return (A.GetDefaultObject<AUTWeapon>()->AutoSwitchPriority > B.GetDefaultObject<AUTWeapon>()->AutoSwitchPriority);
		}
	};
	WeaponList.Sort(FWeaponListSort());

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
			SAssignNew(TabWidget, SUTTabWidget)
		];

		TabWidget->AddTab(FText::FromString(TEXT("Weapon")),
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0.0f, 10.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(750)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(NSLOCTEXT("SUWWeaponConfigDialog", "AutoWeaponSwitch", "Weapon Switch on Pickup"))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(AutoWeaponSwitch, SCheckBox)
					.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
					.ForegroundColor(FLinearColor::White)
					.IsChecked(GetDefault<AUTPlayerController>()->bAutoWeaponSwitch ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked)
				]
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 10.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(500)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(NSLOCTEXT("SUWWeaponConfigDialog", "WeaponHand", "Weapon Hand"))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
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
			+ SVerticalBox::Slot()
			.Padding(0.0f, 10.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUWWeaponConfigDialog", "WeaponPriorities", "Weapon Priorities"))
				.TextStyle(SUWindowsStyle::Get(),"UT.Common.BoldText")
			]

			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SBox)
					.WidthOverride(700)
					.HeightOverride(600)
					[
						SNew(SBorder)
						.Content()
						[
							SAssignNew(WeaponPriorities, SListView<UClass*>)
							.SelectionMode(ESelectionMode::Single)
							.ListItemsSource(&WeaponList)
							.OnGenerateRow(this, &SUWWeaponConfigDialog::GenerateWeaponListRow)
						]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(10.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.Padding(0.0f, 10.0f, 0.0f, 10.0f)
					.AutoHeight()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SBox)
						.HeightOverride(56)
						.WidthOverride(56)
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
					+ SVerticalBox::Slot()
					.Padding(0.0f, 10.0f, 0.0f, 10.0f)
					.AutoHeight()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SBox)
						.HeightOverride(56)
						.WidthOverride(56)
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
		]);
		TabWidget->SelectTab(0);

		WeaponPriorities->SetSelection(WeaponList[0]);

		//Crosshair Info
		if (CrosshairInfos.Num() > 0)
		{
			SelectedCrosshairInfo = CrosshairInfos[0];
		}

		TabWidget->AddTab(FText::FromString(TEXT("Crosshair")),
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
				+ SVerticalBox::Slot()
				.Padding(0.0f, 10.0f, 0.0f, 5.0f)
				.AutoHeight()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Center)
				[
					SAssignNew(ColorOverlay, SOverlay)
				]
			]	
			+ SHorizontalBox::Slot()
			.Padding(30.0f, 20.0f, 30.0f, 0.0f)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Fill)
			[
				SNew(SBorder)
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Fill)
				.Content()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.Padding(0.0f)
					.AutoHeight()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Fill)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SBox)
							.MaxDesiredHeight(30.0f)
							.Content()
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UT.TopMenu.MidFill"))
							]
						]
						+ SOverlay::Slot()
						.HAlign(HAlign_Center)
						[
							SNew(SCheckBox)
							.IsChecked(this, &SUWWeaponConfigDialog::GetCustomWeaponCrosshairs)
							.OnCheckStateChanged(this, &SUWWeaponConfigDialog::SetCustomWeaponCrosshairs)
							.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
							.Padding(FMargin(0.0f, 10.0f, 0.0f, 10.0f))
							.Content()
							[
								SNew(STextBlock)
								.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
								.Text(NSLOCTEXT("SUWWeaponConfigDialog", "CustomWeaponCrosshairs", "Use Custom Weapon Crosshairs"))
							]
						]
					]
					+ SVerticalBox::Slot()
					.Padding(0.0f, 0.0f, 0.0f, 5.0f)
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[

						SAssignNew(CrosshairInfosList, SListView<TSharedPtr<FCrosshairInfo> >)
						.SelectionMode(ESelectionMode::Single)
						.ListItemsSource(&CrosshairInfos)
						.OnGenerateRow(this, &SUWWeaponConfigDialog::GenerateCrosshairListRow)
						.OnSelectionChanged(this, &SUWWeaponConfigDialog::OnCrosshairInfoSelected)					
					]
				]
			]
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

void SUWWeaponConfigDialog::UpdateCrosshairRender(UCanvas* C, int32 Width, int32 Height)
{
	if (SelectedCrosshairInfo.IsValid() && CrosshairMap.Contains(SelectedCrosshairInfo->CrosshairClassName))
	{
		UClass* SelectedCrosshair = CrosshairMap[SelectedCrosshairInfo->CrosshairClassName];
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
	Collector.AddReferencedObjects(WeaponList);
	Collector.AddReferencedObjects(CrosshairList);
	Collector.AddReferencedObject(CrosshairPreviewTexture);
	Collector.AddReferencedObject(CrosshairPreviewMID);
}

void SUWWeaponConfigDialog::OnCrosshairInfoSelected(TSharedPtr<FCrosshairInfo> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedCrosshairInfo = NewSelection;

	//If nothing selected, make sure we fake select the Global crosshair
	if (!SelectedCrosshairInfo.IsValid())
	{
		for (int32 i = 0; i < CrosshairInfos.Num(); i++)
		{
			if (!bCustomWeaponCrosshairs && CrosshairInfos[i]->WeaponClassName == TEXT("Global"))
			{
				SelectedCrosshairInfo = CrosshairInfos[i];
				break;
			}
		}
	}

	
	if (SelectedCrosshairInfo.IsValid() && CrosshairMap.Contains(SelectedCrosshairInfo->CrosshairClassName))
	{
		//Change the crosshair combobox text
		UClass* SelectedCrosshair = CrosshairMap[SelectedCrosshairInfo->CrosshairClassName];
		CrosshairComboBox->SetSelectedItem(SelectedCrosshair);
		
		//Update the color
		if (ColorPicker.IsValid())
		{
			ColorPicker->UTSetNewTargetColorRGB(SelectedCrosshairInfo->Color);
		}
	}
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
	CrosshairInfosList->SetEnabled(bCustomWeaponCrosshairs);

	if (bCustomWeaponCrosshairs)
	{
		for (int32 i = 0; i < CrosshairInfos.Num(); i++)
		{
			//Select the first weapon crosshair we find
			if (CrosshairInfos[i]->WeaponClassName != TEXT("Global"))
			{
				CrosshairInfosList->SetSelection(CrosshairInfos[i], ESelectInfo::Direct);
				break;
			}
		}
	}
	else
	{
		CrosshairInfosList->ClearSelection();

		//Make sure we select the global since ClearSelection() with no selection wont call OnCrosshairInfoSelected()
		for (int32 i = 0; i < CrosshairInfos.Num(); i++)
		{
			if (CrosshairInfos[i]->WeaponClassName == TEXT("Global"))
			{
				SelectedCrosshairInfo = CrosshairInfos[i];
			}
		}
	}
}

TSharedRef<SWidget> SUWWeaponConfigDialog::GenerateCrosshairRow(TWeakObjectPtr<UClass> CrosshairClass)
{	
	return 	SNew(STextBlock)
		.Text(CrosshairClass->GetDefaultObject<UUTCrosshair>()->CrosshairName)
		.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle");
}

TSharedRef<ITableRow> SUWWeaponConfigDialog::GenerateWeaponListRow(UClass* WeaponType, const TSharedRef<STableViewBase>& OwningList)
{
	checkSlow(WeaponType->IsChildOf(AUTWeapon::StaticClass()));

	return SNew(STableRow<UClass*>, OwningList)
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
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
		];
}

void SUWWeaponConfigDialog::OnHandSelected(TSharedPtr<FText> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedWeaponHand->SetText(*NewSelection.Get());
}

FReply SUWWeaponConfigDialog::WeaponPriorityUp()
{
	TArray<UClass*> SelectedItems = WeaponPriorities->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		int32 Index = WeaponList.Find(SelectedItems[0]);
		if (Index > 0)
		{
			Exchange(WeaponList[Index], WeaponList[Index - 1]);
			WeaponPriorities->RequestListRefresh();
		}
	}
	return FReply::Handled();
}
FReply SUWWeaponConfigDialog::WeaponPriorityDown()
{
	TArray<UClass*> SelectedItems = WeaponPriorities->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		int32 Index = WeaponList.Find(SelectedItems[0]);
		if (Index != INDEX_NONE && Index < WeaponList.Num() - 1)
		{
			Exchange(WeaponList[Index], WeaponList[Index + 1]);
			WeaponPriorities->RequestListRefresh();
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

	// note that the array mirrors the list widget so we can use it directly
	for (int32 i = 0; i < WeaponList.Num(); i++)
	{
		float NewPriority = float(WeaponList.Num() - i); // top of list is highest priority
		WeaponList[i]->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority = NewPriority;
		WeaponList[i]->GetDefaultObject<AUTWeapon>()->SaveConfig();
		// update instances so changes take effect immediately
		if (UTPlayerController != NULL)
		{
			for (TActorIterator<AUTWeapon> It(UTPlayerController->GetWorld()); It; ++It)
			{
				if (It->GetClass() == WeaponList[i])
				{
					It->AutoSwitchPriority = NewPriority;
				}
			}
		}
		if (ProfileSettings != NULL)
		{
			ProfileSettings->SetWeaponPriority(GetNameSafe(WeaponList[i]), WeaponList[i]->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority);
		}
	}

	//Save the crosshair info to the config and update the current hud if there is one
	TArray<AUTHUD*> Huds;
	if (UTPlayerController != nullptr && UTPlayerController->MyUTHUD != nullptr)
	{
		Huds.Add(UTPlayerController->MyUTHUD);
	}
	Huds.Add(AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>());

	for (AUTHUD* Hud : Huds)
	{
		for (auto CrosshairInfo : CrosshairInfos)
		{
			//Update or add a new one if not found
			int32 Index = Hud->CrosshairInfos.Find(*CrosshairInfo.Get());
			if (Index != INDEX_NONE)
			{
				Hud->CrosshairInfos[Index] = *CrosshairInfo.Get();
			}
			else
			{
				Hud->CrosshairInfos.Add(*CrosshairInfo.Get());
			}
		}
		Hud->LoadedCrosshairs.Empty();
		Hud->bCustomWeaponCrosshairs = bCustomWeaponCrosshairs;
		Hud->SaveConfig();
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
	auto SelectedItems = WeaponPriorities->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		int32 Index = WeaponList.Find(SelectedItems[0]);
		if (Index != INDEX_NONE && Index != 0)
		{
			return true;
		}
	}
	return false;
}

bool SUWWeaponConfigDialog::CanMoveWeaponPriorityDown() const
{
	auto SelectedItems = WeaponPriorities->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		int32 Index = WeaponList.Find(SelectedItems[0]);
		if (Index != INDEX_NONE && Index < WeaponList.Num() - 1)
		{
			return true;
		}
	}
	return false;
}

TSharedRef<ITableRow> SUWWeaponConfigDialog::GenerateCrosshairListRow(TSharedPtr<FCrosshairInfo> CrosshairInfo, const TSharedRef<STableViewBase>& OwningList)
{
	if (CrosshairInfo->WeaponClassName != TEXT("Global"))
	{
		if (WeaponMap.Contains(CrosshairInfo->WeaponClassName))
		{
			UClass* WeaponType = WeaponMap[CrosshairInfo->WeaponClassName];
			checkSlow(WeaponType->IsChildOf(AUTWeapon::StaticClass()));

			return SNew(STableRow<TSharedPtr<FCrosshairInfo>>, OwningList)
				.Padding(5)
				[
					SNew(STextBlock)
					.Text(WeaponType->GetDefaultObject<AUTWeapon>()->DisplayName)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				];
		}
	}
	return SNew(STableRow<TSharedPtr<FCrosshairInfo>>, OwningList).Padding(0).Visibility(EVisibility::Hidden);
}

#endif