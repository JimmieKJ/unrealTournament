// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTWeaponWheelConfigDialog.h"
#include "../Widgets/SUTButton.h"
#include "../SUTStyle.h"
#include "UTPlayerInput.h"
#include "Scalability.h"
#include "UTWorldSettings.h"
#include "UTGameEngine.h"
#include "../SUTUtils.h"
#include "../Widgets/SUTBorder.h"

#if !UE_SERVER


void SUTWeaponWheelConfigDialog::Construct(const FArguments& InArgs)
{
	SUTDialogBase::Construct(SUTDialogBase::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(InArgs._DialogTitle)
							.DialogSize(InArgs._DialogSize)
							.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.ContentPadding(InArgs._ContentPadding)
							.ButtonMask(InArgs._ButtonMask)
							.bShadow(true)
							.IsScrollable(false)
							.OnDialogResult(InArgs._OnDialogResult)
						);

	if (DialogContent.IsValid())
	{
		DialogContent->AddSlot()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().Padding(10.0f, 10.0f, 5.0f, 10.0f).FillWidth(1.0)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUTWeaponWheelConfigDialog","Instructions","Select the slot on the weapon wheel then pick the weapon group to be activated when you choose that slot in game."))
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium.Bold")
					.AutoWrapText(true)
				]
				+SVerticalBox::Slot().AutoHeight().Padding(10.0f,10.0f,10.0f,10.0f)
				[
					SAssignNew(WeaponGroupPanel, SGridPanel)
				]
			]
			+SHorizontalBox::Slot().Padding(5.0f, 10.0f, 10.0f, 10.0f).AutoWidth().HAlign(HAlign_Center)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().FillHeight(1.0).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(700.0f).HeightOverride(700.0f)
					[
						SNew(SUTBorder)
						.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
						[
							SNew(SOverlay)
							+SOverlay::Slot()
							[
								SAssignNew(ButtonCanvas, SCanvas)
							]
						]
					]
				]
			
			]
		];


		float Angle = 0;
		for (int32 Slot = 0; Slot < 8; Slot++)
		{

			FVector2D Position = FVector2D(260.0f, 285.0f);

			float Sin = 0.f;
			float Cos = 0.f;

			FMath::SinCos(&Sin,&Cos, FMath::DegreesToRadians(Angle));
			Position.X += 250 * Sin;
			Position.Y += -1.0f * 250 * Cos;

			TSharedPtr<SUTButton> Button;
			TSharedPtr<STextBlock> Number;

			FSlateRenderTransform Transform = ::Concatenate(FQuat2D(FMath::DegreesToRadians(Angle)), FVector2D(0.0f, 0.0));
			ButtonCanvas->AddSlot().Position(Position).Size(FVector2D(189,113))
			[
				SNew(SBox).WidthOverride(189).HeightOverride(113).MinDesiredHeight(189).MaxDesiredHeight(113)
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SAssignNew(Button,SUTButton)
						.ButtonStyle(SUTStyle::Get(), "UT.WeaponWheelButton")
						.OnClicked(this, &SUTWeaponWheelConfigDialog::WheelClicked, Slot)
						.ContentPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
						.IsToggleButton(true)
						.RenderTransform(Transform)
						.RenderTransformPivot(FVector2D(0.5f,0.5f))
					]
					+SOverlay::Slot()
					[
						SNew(SImage)
						.Image(this, &SUTWeaponWheelConfigDialog::GetWeaponImage, Slot)
						.Visibility(EVisibility::HitTestInvisible)
					]
					+SOverlay::Slot()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot().FillWidth(1.0).HAlign(HAlign_Center)
						[
							SAssignNew(Number, STextBlock)
							.Text(this, &SUTWeaponWheelConfigDialog::GetGroupNumber, Slot)
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large.Bold")
							.Visibility(EVisibility::Collapsed)
							.AutoWrapText(true)	
						]
					]
				]
			];
			WheelNumbers.Add(Number);
			WheelButtons.Add(Button);
			Angle += 45;
		}

	}

	for (int Slot = 0; Slot < 8 ; Slot++) SlotIcons.Add(nullptr);

}

FText SUTWeaponWheelConfigDialog::GetGroupNumber(int32 Slot) const
{
	UUTProfileSettings* Profile = PlayerOwner->GetProfileSettings();
	if (Profile != nullptr)
	{
		if (LocalSlots.IsValidIndex(Slot) && LocalSlots[Slot] > 0)
		{
			return LocalSlots[Slot] < 10 ? FText::AsNumber(LocalSlots[Slot]) : FText::AsNumber(0);
		}
	}

	return FText::GetEmpty();
}

void SUTWeaponWheelConfigDialog::InitializeList(TArray<FWeaponInfo>* inAllWeapons)
{
	UUTProfileSettings* Profile = PlayerOwner->GetProfileSettings();
	if (Profile != nullptr)
	{
		LocalSlots = Profile->WeaponWheelMapping;

		AllWeapons = inAllWeapons;
		GenerateSlots();
		GenerateWeaponGroups();
		WheelClicked(0);
	}
}

void SUTWeaponWheelConfigDialog::GenerateSlots()
{
	UUTProfileSettings* Profile = PlayerOwner->GetProfileSettings();
	if (Profile != nullptr)
	{
		for (int32 Slot = 0 ; Slot < 8; Slot++)	
		{
			int32 BestWeaponIndex = -1;
			if (LocalSlots.IsValidIndex(Slot) && LocalSlots[Slot] > 0)
			{
				// Find the best weapon to display in the list.
				for (int32 i = 0; i < AllWeapons->Num(); i++)
				{
					if (LocalSlots.IsValidIndex(Slot) && (*AllWeapons)[i].WeaponCustomizationInfo->WeaponGroup == LocalSlots[Slot])			
					{
						if (BestWeaponIndex == -1 || (*AllWeapons)[i].WeaponCustomizationInfo->WeaponAutoSwitchPriority > (*AllWeapons)[BestWeaponIndex].WeaponCustomizationInfo->WeaponAutoSwitchPriority)
						{
							BestWeaponIndex = i;
						}
					}
				}

				if (BestWeaponIndex >= 0)
				{
					SlotIcons[Slot] = (*AllWeapons)[BestWeaponIndex].WeaponIconBrush;
					WheelNumbers[Slot]->SetVisibility(EVisibility::Collapsed);
				}
				else
				{
					WheelNumbers[Slot]->SetVisibility(EVisibility::Visible);
					SlotIcons[Slot] = nullptr;
				}
			}
			else
			{
				WheelNumbers[Slot]->SetVisibility(EVisibility::Collapsed);
				SlotIcons[Slot] = nullptr;
			}
		}
	}
}

const FSlateBrush* SUTWeaponWheelConfigDialog::GetWeaponImage(int32 Slot) const
{
	if (SlotIcons.IsValidIndex(Slot) && SlotIcons[Slot] != nullptr)
	{
		return SlotIcons[Slot];
	}

	return SUTStyle::Get().GetBrush("UT.NoStyle");
}

void SUTWeaponWheelConfigDialog::GenerateWeaponGroups()
{
	if (WeaponGroupPanel.IsValid())
	{
		WeaponGroupPanel->ClearChildren();
		WeaponGroupButtons.Empty();
		WeaponGroupButtons.AddZeroed(11);

		for (int32 i=0; i <=10; i++)
		{
			TSharedPtr<SUTButton> Button;

			int32 Row = i / 3;
			int32 Col = i % 3;

			FText GroupTitle = NSLOCTEXT("SUTWeaponWheelConfigDialog","Unused","Unused");
			if (i > 0 && i < 10) GroupTitle = FText::AsNumber(i);
			if (i == 10) GroupTitle = FText::AsNumber(0);

			WeaponGroupPanel->AddSlot(Col,Row)
			.Padding(5.0f,0.0f,0.0f,0.0f)
			.HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(160).HeightOverride(128)
				[
					SAssignNew(Button, SUTButton)
					.OnClicked(this, &SUTWeaponWheelConfigDialog::WeaponGroupSelected, i)
					.ButtonStyle(SUTStyle::Get(), "UT.SimpleButton.Dark")
					.IsToggleButton(true)
					.CaptionHAlign(HAlign_Center)
					.Text(GroupTitle)
					.TextStyle(SUTStyle::Get(), (i == 0 ? "UT.Font.NormalText.Medium.Bold" : "UT.Font.NormalText.Huge.Bold"))
				]
			];
			WeaponGroupButtons[i] = Button;
		}
	}
}

FReply SUTWeaponWheelConfigDialog::WeaponGroupSelected(int32 GroupIndex)
{
	for (int32 i=0; i < 11; i++)
	{
		if (i == GroupIndex)
		{
			WeaponGroupButtons[i]->BePressed();
		}
		else
		{
			WeaponGroupButtons[i]->UnPressed();
		}
	}

	LocalSlots[SelectedSlot] = GroupIndex;
	GenerateSlots();

	return FReply::Handled();
}

FReply SUTWeaponWheelConfigDialog::WheelClicked(int32 SlotIndex)
{
	for (int32 i=0; i < 8; i++)
	{
		if (i == SlotIndex)
		{
			WheelButtons[i]->BePressed();
		}
		else
		{
			WheelButtons[i]->UnPressed();
		}
	}

	if (LocalSlots.IsValidIndex(SlotIndex))
	{
		int32 GroupIndex = LocalSlots[SlotIndex];
		for (int32 i=0; i < 11; i++)
		{
			if (i == GroupIndex)
			{
				WeaponGroupButtons[i]->BePressed();
			}
			else
			{
				WeaponGroupButtons[i]->UnPressed();
			}
		}
	}

	SelectedSlot = SlotIndex;
	return FReply::Handled();
}

FReply SUTWeaponWheelConfigDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK)
	{
		UUTProfileSettings* Profile = PlayerOwner->GetProfileSettings();
		if (Profile)
		{
			Profile->WeaponWheelMapping = LocalSlots;
		}
	}

	return SUTDialogBase::OnButtonClick(ButtonID);

}


#endif