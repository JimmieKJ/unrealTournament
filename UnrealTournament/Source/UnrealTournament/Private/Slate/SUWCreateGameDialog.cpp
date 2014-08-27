// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWCreateGameDialog.h"
#include "SUWindowsStyle.h"
#include "UTDMGameMode.h"

void SUWCreateGameDialog::Construct(const FArguments& InArgs)
{
	SUWDialog::Construct(SUWDialog::FArguments().PlayerOwner(InArgs._PlayerOwner));

	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->IsChildOf(AUTGameMode::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
		{
			AllGametypes.Add(*It);
		}
	}
	// TODO: need to get blueprint classes somehow (asset registry?)

	TSubclassOf<AUTGameMode> InitialSelectedGameClass = AUTDMGameMode::StaticClass(); // FIXME remember prev selection

	TSharedPtr<SVerticalBox> MainBox;
	TSharedPtr<SUniformGridPanel> ButtonRow;

	TSharedPtr<TAttributePropertyString> ServerNameProp = MakeShareable(new TAttributePropertyString(AUTGameState::StaticClass()->GetDefaultObject(), &AUTGameState::StaticClass()->GetDefaultObject<AUTGameState>()->ServerName));
	PropertyLinks.Add(ServerNameProp);
	TSharedPtr<TAttributePropertyString> ServerMOTDProp = MakeShareable(new TAttributePropertyString(AUTGameState::StaticClass()->GetDefaultObject(), &AUTGameState::StaticClass()->GetDefaultObject<AUTGameState>()->ServerMOTD));
	PropertyLinks.Add(ServerMOTDProp);

	ChildSlot
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Center)
	[
		SNew(SBorder)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.BorderImage(SUWindowsStyle::Get().GetBrush("UWindows.Standard.MenuBar.Background"))
		[
			SAssignNew(MainBox, SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::Black)
				.Text(NSLOCTEXT("SUWCreateGameDialog", "Gametype", "Gametype:"))
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 0.0f)
			.AutoHeight()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			[
				SAssignNew(GameList, SComboBox<UClass*>)
				.Method(SMenuAnchor::UseCurrentWindow)
				.InitiallySelectedItem(InitialSelectedGameClass)
				.ComboBoxStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
				.OptionsSource(&AllGametypes)
				.OnGenerateWidget(this, &SUWCreateGameDialog::GenerateGameNameWidget)
				.OnSelectionChanged(this, &SUWCreateGameDialog::OnGameSelected)
				.Content()
				[
					SAssignNew(SelectedGameName, STextBlock)
					.Text(InitialSelectedGameClass.GetDefaultObject()->DisplayName.ToString())
				]
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::Black)
				.Text(NSLOCTEXT("SUWCreateGameDialog", "Map", "Map:"))
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 0.0f)
			.AutoHeight()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			[
				SAssignNew(MapList, SComboBox< TSharedPtr<FString> >)
				.Method(SMenuAnchor::UseCurrentWindow)
				.ComboBoxStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
				.OptionsSource(&AllMaps)
				.OnGenerateWidget(this, &SUWDialog::GenerateStringListWidget)
				.OnSelectionChanged(this, &SUWCreateGameDialog::OnMapSelected)
				.Content()
				[
					SAssignNew(SelectedMap, STextBlock)
					.Text(NSLOCTEXT("SUWCreateGameDialog", "NoMaps", "No Maps Available").ToString())
				]
			]
		]
	];

	if (InArgs._IsOnline)
	{
		MainBox->AddSlot()
		.Padding(0.0f, 10.0f, 0.0f, 5.0f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(5.0f, 0.0f, 5.0f, 0.0f)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::Black)
				.Text(NSLOCTEXT("SUWCreateGameDialog", "ServerName", "Server Name:"))
			]
			+ SHorizontalBox::Slot()
			.Padding(5.0f, 0.0f, 5.0f, 0.0f)
			.AutoWidth()
			[
				SNew(SEditableTextBox)
				.OnTextChanged(ServerNameProp.ToSharedRef(), &TAttributePropertyString::SetFromText)
				.MinDesiredWidth(300.0f)
				.Text(ServerNameProp.ToSharedRef(), &TAttributePropertyString::GetAsText)
			]
		];
		MainBox->AddSlot()
		.Padding(0.0f, 5.0f, 0.0f, 5.0f)
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::Black)
			.Text(NSLOCTEXT("SUWCreateGameDialog", "ServerMOTD", "Server MOTD:"))
		];
		MainBox->AddSlot()
		.Padding(0.0f, 0.0f, 0.0f, 0.0f)
		.AutoHeight()
		[
			SNew(SEditableTextBox)
			.OnTextChanged(ServerMOTDProp.ToSharedRef(), &TAttributePropertyString::SetFromText)
			.MinDesiredWidth(400.0f)
			.Text(ServerMOTDProp.ToSharedRef(), &TAttributePropertyString::GetAsText)
		];
	}

	MainBox->AddSlot()
	.Padding(0.0f, 10.0f, 0.0f, 5.0f)
	.HAlign(HAlign_Center)
	.AutoHeight()
	[
		SNew(STextBlock)
		.ColorAndOpacity(FLinearColor::Black)
		.Text(NSLOCTEXT("SUWCreateGameDialog", "GameSettings", "Game Settings:"))
	];
	MainBox->AddSlot()
	.Padding(0.0f, 5.0f, 0.0f, 5.0f)
	.AutoHeight()
	[
		SAssignNew(GameConfigPanel, SVerticalBox)
	];
	MainBox->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Bottom)
	.HAlign(HAlign_Right)
	.Padding(5.0f, 5.0f, 5.0f, 5.0f)
	[
		SAssignNew(ButtonRow, SUniformGridPanel)
		.SlotPadding(FMargin(10.0f, 10.0f, 10.0f, 10.0f))
	];

	OnGameSelected(InitialSelectedGameClass, ESelectInfo::Direct);

	int32 ButtonID = 0;
	if (InArgs._IsOnline)
	{
		ButtonRow->AddSlot(ButtonID++, 0)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
			.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
			.Text(NSLOCTEXT("SUWCreateGameDialog", "StartListenButton", "Start Listen").ToString())
			.OnClicked(this, &SUWCreateGameDialog::StartClick, SERVER_Listen)
		];
		if (!FPlatformProperties::IsGameOnly())
		{
			ButtonRow->AddSlot(ButtonID++, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
				.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
				.Text(NSLOCTEXT("SUWCreateGameDialog", "StartDedicatedButton", "Start Dedicated").ToString())
				.OnClicked(this, &SUWCreateGameDialog::StartClick, SERVER_Dedicated)
			];
		}
	}
	else
	{
		ButtonRow->AddSlot(ButtonID++, 0)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
			.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
			.Text(NSLOCTEXT("SUWCreateGameDialog", "StartButton", "Start").ToString())
			.OnClicked(this, &SUWCreateGameDialog::StartClick, SERVER_Standalone)
		];
	}
	ButtonRow->AddSlot(ButtonID++, 0)
	[
		SNew(SButton)
		.HAlign(HAlign_Center)
		.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
		.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
		.Text(NSLOCTEXT("SUWMessageBox", "CancelButton", "Cancel").ToString())
		.OnClicked(this, &SUWCreateGameDialog::CancelClick)
	];
}

void SUWCreateGameDialog::OnMapSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedMap->SetText(NewSelection.IsValid() ? *NewSelection.Get() : NSLOCTEXT("SUWCreateGameDialog", "NoMaps", "No Maps Available").ToString());
}

TSharedRef<SWidget> SUWCreateGameDialog::GenerateGameNameWidget(UClass* InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::Black)
			.Text(InItem->GetDefaultObject<AUTGameMode>()->DisplayName.ToString())
		];
}
void SUWCreateGameDialog::OnGameSelected(UClass* NewSelection, ESelectInfo::Type SelectInfo)
{
	if (NewSelection != SelectedGameClass)
	{
		// clear existing game type info and cancel any changes that were made
		if (SelectedGameClass != NULL)
		{
			SelectedGameClass.GetDefaultObject()->ReloadConfig();
			GameConfigPanel->ClearChildren();
			GameConfigProps.Empty();
		}

		SelectedGameClass = NewSelection;
		SelectedGameClass.GetDefaultObject()->CreateConfigWidgets(GameConfigPanel, GameConfigProps);

		SelectedGameName->SetText(SelectedGameClass.GetDefaultObject()->DisplayName.ToString());

		// generate map list
		const AUTGameMode* GameDefaults = SelectedGameClass.GetDefaultObject();
		AllMaps.Empty();
		TArray<FString> Paths;
		FPackageName::QueryRootContentPaths(Paths);
		int32 MapExtLen = FPackageName::GetMapPackageExtension().Len();
		for (int32 i = 0; i < Paths.Num(); i++)
		{
			// ignore /Engine/ as those aren't real gameplay maps
			if (!Paths[i].StartsWith(TEXT("/Engine/")))
			{
				TArray<FString> List;
				FPackageName::FindPackagesInDirectory(List, *FPackageName::LongPackageNameToFilename(Paths[i]));
				for (int32 j = 0; j < List.Num(); j++)
				{
					if (List[j].Right(MapExtLen) == FPackageName::GetMapPackageExtension())
					{
						TSharedPtr<FString> BaseName = MakeShareable(new FString(FPaths::GetBaseFilename(List[j])));
						if (GameDefaults->SupportsMap(*BaseName.Get()))
						{
							AllMaps.Add(BaseName);
						}
					}
				}
			}
		}

		MapList->RefreshOptions();
		if (AllMaps.Num() > 0)
		{
			// TODO: remember last selection
			MapList->SetSelectedItem(AllMaps[0]);
		}
		else
		{
			MapList->SetSelectedItem(NULL);
		}
	}
}

FReply SUWCreateGameDialog::StartClick(EServerStartMode Mode)
{
	// save changes
	SelectedGameClass.GetDefaultObject()->SaveConfig();
	AUTGameState::StaticClass()->GetDefaultObject()->SaveConfig();

	FString NewURL = FString::Printf(TEXT("%s?game=%s"), *SelectedMap->GetText(), *SelectedGameClass->GetPathName());

	if (Mode == SERVER_Dedicated)
	{
		GConfig->Flush(false);
		FString ExecPath = FPlatformProcess::GenerateApplicationPath(FPlatformProcess::ExecutableName(true), FApp::GetBuildConfiguration());
		FString Options = FString::Printf(TEXT("unrealtournament %s -log -game -server"), *NewURL);
		UE_LOG(UT, Log, TEXT("Run dedicated server with command line: %s %s"), *ExecPath, *Options);
		if (FPlatformProcess::CreateProc(*ExecPath, *Options, true, false, false, NULL, 0, NULL, NULL).IsValid())
		{
			FPlatformMisc::RequestExit(false);
		}
	}
	else if (GetPlayerOwner()->PlayerController != NULL)
	{
		if (Mode == SERVER_Listen)
		{
			NewURL += TEXT("?listen");
		}
		GEngine->SetClientTravel(GetPlayerOwner()->PlayerController->GetWorld(), *NewURL, TRAVEL_Absolute);
	}
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	GetPlayerOwner()->HideMenu();
	return FReply::Handled();
}
FReply SUWCreateGameDialog::CancelClick()
{
	// revert config changes
	SelectedGameClass.GetDefaultObject()->ReloadConfig();
	AUTGameState::StaticClass()->GetDefaultObject()->ReloadConfig();

	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}