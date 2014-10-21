// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWCreateGameDialog.h"
#include "SUWindowsStyle.h"
#include "UTDMGameMode.h"
#include "AssetData.h"

#if !UE_SERVER

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
	GametypeLibrary = UObjectLibrary::CreateLibrary(AUTGameMode::StaticClass(), true, false);
	GametypeLibrary->LoadBlueprintAssetDataFromPath(TEXT("/Game/")); // TODO: do we need to iterate through all the root paths?
	GametypeLibrary->LoadBlueprintAssetDataFromPaths(TArray<FString>()); // HACK: library code doesn't properly handle giving it a root path
	{
		TArray<FAssetData> AssetList;
		GametypeLibrary->GetAssetDataList(AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL)
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTGameMode::StaticClass()))
				{
					AllGametypes.AddUnique(TestClass);
				}
			}
		}
	}

	TSubclassOf<AUTGameMode> InitialSelectedGameClass = AUTDMGameMode::StaticClass();
	// restore previous selection if possible
	FString LastGametypePath;
	if (GConfig->GetString(TEXT("CreateGameDialog"), TEXT("LastGametypePath"), LastGametypePath, GGameIni) && LastGametypePath.Len() > 0)
	{
		for (UClass* GameType : AllGametypes)
		{
			if (GameType->GetPathName() == LastGametypePath)
			{
				InitialSelectedGameClass = GameType;
				break;
			}
		}
	}

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
			.Text(NSLOCTEXT("SUWCreateGameDialog", "StartDedicatedButton", "Start Dedicated").ToString())
			.OnClicked(this, &SUWCreateGameDialog::StartClick, SERVER_Dedicated)
		];
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
			MapList->SetSelectedItem(AllMaps[0]);
			// remember last selection
			for (TSharedPtr<FString> TestMap : AllMaps)
			{
				if (*TestMap.Get() == SelectedGameClass.GetDefaultObject()->UILastStartingMap)
				{
					MapList->SetSelectedItem(TestMap);
					break;
				}
			}
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
	GConfig->SetString(TEXT("CreateGameDialog"), TEXT("LastGametypePath"), *SelectedGameClass->GetPathName(), GGameIni);
	SelectedGameClass.GetDefaultObject()->UILastStartingMap = SelectedMap->GetText().ToString();
	SelectedGameClass.GetDefaultObject()->SaveConfig();
	AUTGameState::StaticClass()->GetDefaultObject()->SaveConfig();

	FString NewURL = FString::Printf(TEXT("%s?game=%s"), *SelectedMap->GetText().ToString(), *SelectedGameClass->GetPathName());

	if (Mode == SERVER_Dedicated)
	{
		if (GetPlayerOwner()->DedicatedServerProcessHandle.IsValid())
		{
			if (FPlatformProcess::IsProcRunning(GetPlayerOwner()->DedicatedServerProcessHandle))
			{
				FPlatformProcess::TerminateProc(GetPlayerOwner()->DedicatedServerProcessHandle);
			}
			GetPlayerOwner()->DedicatedServerProcessHandle.Reset();
		}

		GConfig->Flush(false);
		FString ExecPath = FPlatformProcess::GenerateApplicationPath(FApp::GetName(), FApp::GetBuildConfiguration());
		FString Options = FString::Printf(TEXT("unrealtournament %s -log -server"), *NewURL);
		UE_LOG(UT, Log, TEXT("Run dedicated server with command line: %s %s"), *ExecPath, *Options);
		GetPlayerOwner()->DedicatedServerProcessHandle = FPlatformProcess::CreateProc(*ExecPath, *Options, true, false, false, NULL, 0, NULL, NULL);

		if (GetPlayerOwner()->DedicatedServerProcessHandle.IsValid())
		{
			GEngine->SetClientTravel(GetPlayerOwner()->PlayerController->GetWorld(), TEXT("127.0.0.1"), TRAVEL_Absolute);

			GetPlayerOwner()->CloseDialog(SharedThis(this));
			GetPlayerOwner()->HideMenu();
		}
	}
	else if (GetPlayerOwner()->PlayerController != NULL)
	{
		GEngine->SetClientTravel(GetPlayerOwner()->PlayerController->GetWorld(), *NewURL, TRAVEL_Absolute);
		GetPlayerOwner()->CloseDialog(SharedThis(this));
		GetPlayerOwner()->HideMenu();
	}

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

#endif