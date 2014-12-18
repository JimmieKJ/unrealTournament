// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWCreateGameDialog.h"
#include "SUWindowsStyle.h"
#include "UTDMGameMode.h"
#include "AssetData.h"
#include "UTLevelSummary.h"
#include "UTMutator.h"

#if !UE_SERVER

#include "UserWidget.h"

void SUWCreateGameDialog::Construct(const FArguments& InArgs)
{
	SUWDialog::Construct(SUWDialog::FArguments().PlayerOwner(InArgs._PlayerOwner));

	for (TObjectIterator<UClass> It; It; ++It)
	{
		// non-native classes are detected by asset search even if they're loaded for consistency
		if (!It->HasAnyClassFlags(CLASS_Abstract | CLASS_HideDropDown) && It->HasAnyClassFlags(CLASS_Native))
		{
			if (It->IsChildOf(AUTGameMode::StaticClass()))
			{
				AllGametypes.Add(*It);
			}
			else if (It->IsChildOf(AUTMutator::StaticClass()) && !It->GetDefaultObject<AUTMutator>()->DisplayName.IsEmpty())
			{
				MutatorListAvailable.Add(*It);
			}
		}
	}
	{
		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(AUTGameMode::StaticClass(), AssetList);
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
	{
		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(AUTMutator::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL)
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTMutator::StaticClass()) && !TestClass->GetDefaultObject<AUTMutator>()->DisplayName.IsEmpty())
				{
					MutatorListAvailable.AddUnique(TestClass);
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
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
			[
				SAssignNew(MainBox, SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(0.0f, 5.0f, 0.0f, 5.0f)
				.AutoHeight()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FLinearColor::White)
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
					.ColorAndOpacity(FLinearColor::White)
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
				+ SVerticalBox::Slot()
				.Padding(0.0f, 5.0f, 0.0f, 5.0f)
				.AutoHeight()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Center)
				[
					SAssignNew(MapAuthor, STextBlock)
					.ColorAndOpacity(FLinearColor::White)
					.Text(FText::Format(NSLOCTEXT("SUWCreateGameDialog", "Author", "Author: {0}"), FText::FromString(TEXT("-"))))
				]
				+ SVerticalBox::Slot()
				.Padding(0.0f, 0.0f, 0.0f, 5.0f)
				.AutoHeight()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Center)
				[
					SAssignNew(MapRecommendedPlayers, STextBlock)
					.ColorAndOpacity(FLinearColor::White)
					.Text(FText::Format(NSLOCTEXT("SUWCreateGameDialog", "OptimalPlayers", "Recommended Players: {0} - {1}"), FText::AsNumber(8), FText::AsNumber(12)))
				]
				+ SVerticalBox::Slot()
				.Padding(0.0f, 0.0f, 0.0f, 5.0f)
				.AutoHeight()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Center)
				[
					SAssignNew(MapDesc, STextBlock)
					.ColorAndOpacity(FLinearColor::White)
					.Text(FText())
					.WrapTextAt(500.0f)
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
				.ColorAndOpacity(FLinearColor::White)
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
			.ColorAndOpacity(FLinearColor::White)
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
		.ColorAndOpacity(FLinearColor::White)
		.Text(NSLOCTEXT("SUWCreateGameDialog", "GameSettings", "Game Settings:"))
	];
	MainBox->AddSlot()
	.Padding(0.0f, 5.0f, 0.0f, 5.0f)
	.AutoHeight()
	[
		SAssignNew(GameConfigPanel, SVerticalBox)
	];
	MainBox->AddSlot()
	.Padding(0.0f, 10.0f, 0.0f, 5.0f)
	.HAlign(HAlign_Center)
	.AutoHeight()
	[
		SNew(STextBlock)
		.ColorAndOpacity(FLinearColor::White)
		.Text(NSLOCTEXT("SUWCreateGameDialog", "Mutators", "Mutators:"))
	];
	MainBox->AddSlot()
	.Padding(0.0f, 10.0f, 0.0f, 5.0f)
	.HAlign(HAlign_Center)
	.AutoHeight()
	[
		SNew(SGridPanel)
		+ SGridPanel::Slot(0, 0)
		.Padding(5.0f, 5.0f, 5.0f, 5.0f)
		[
			SNew(SBox)
			.WidthOverride(200.0f)
			.HeightOverride(200.0f)
			[
				SNew(SBorder)
				.Content()
				[
					SAssignNew(AvailableMutators, SListView<UClass*>)
					.SelectionMode(ESelectionMode::Single)
					.ListItemsSource(&MutatorListAvailable)
					.OnGenerateRow(this, &SUWCreateGameDialog::GenerateMutatorListRow)
				]
			]
		]
		+ SGridPanel::Slot(1, 0)
		.Padding(5.0f, 5.0f, 5.0f, 5.0f)
		[
			SNew(SBox)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(0.0f, 5.0f, 0.0f, 5.0f)
				.HAlign(HAlign_Center)
				.AutoHeight()
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
					.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
					.Text(NSLOCTEXT("SUWCreateGameDialog", "MutatorAdd", "-->").ToString())
					.OnClicked(this, &SUWCreateGameDialog::AddMutator)
				]
				+ SVerticalBox::Slot()
				.Padding(0.0f, 5.0f, 0.0f, 5.0f)
				.HAlign(HAlign_Center)
				.AutoHeight()
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
					.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
					.Text(NSLOCTEXT("SUWCreateGameDialog", "MutatorRemove", "<--").ToString())
					.OnClicked(this, &SUWCreateGameDialog::RemoveMutator)
				]
			]
		]
		+ SGridPanel::Slot(2, 0)
		.Padding(5.0f, 5.0f, 5.0f, 5.0f)
		[
			SNew(SBox)
			.WidthOverride(200.0f)
			.HeightOverride(200.0f)
			[
				SNew(SBorder)
				.Content()
				[
					SAssignNew(EnabledMutators, SListView<UClass*>)
					.SelectionMode(ESelectionMode::Single)
					.ListItemsSource(&MutatorListEnabled)
					.OnGenerateRow(this, &SUWCreateGameDialog::GenerateMutatorListRow)
				]
			]
		]
		+ SGridPanel::Slot(2, 1)
		.Padding(5.0f, 5.0f, 5.0f, 5.0f)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
			.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
			.Text(NSLOCTEXT("SUWCreateGameDialog", "ConfigureMutator", "Configure Mutator").ToString())
			.OnClicked(this, &SUWCreateGameDialog::ConfigureMutator)
		]
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

	UUTLevelSummary* Summary = NULL;
	FString MapFullName;
	if (FPackageName::SearchForPackageOnDisk(*NewSelection + FPackageName::GetMapPackageExtension(), &MapFullName))
	{
		static FName NAME_LevelSummary(TEXT("LevelSummary"));

		UPackage* Pkg = CreatePackage(NULL, *MapFullName);
		Summary = FindObject<UUTLevelSummary>(Pkg, *NAME_LevelSummary.ToString());
		if (Summary == NULL)
		{
			// LoadObject() actually forces the whole package to be loaded for some reason so we need to take the long way around
			BeginLoad();
			ULinkerLoad* Linker = GetPackageLinker(Pkg, NULL, LOAD_NoWarn | LOAD_Quiet, NULL, NULL);
			if (Linker != NULL)
			{
				//UUTLevelSummary* Summary = Cast<UUTLevelSummary>(Linker->Create(UUTLevelSummary::StaticClass(), FName(TEXT("LevelSummary")), Pkg, LOAD_NoWarn | LOAD_Quiet, false));
				// ULinkerLoad::Create() not exported... even more hard way :(
				FPackageIndex SummaryClassIndex, SummaryPackageIndex;
				if (Linker->FindImportClassAndPackage(FName(TEXT("UTLevelSummary")), SummaryClassIndex, SummaryPackageIndex) && SummaryPackageIndex.IsImport() && Linker->ImportMap[SummaryPackageIndex.ToImport()].ObjectName == AUTGameMode::StaticClass()->GetOutermost()->GetFName())
				{
					for (int32 Index = 0; Index < Linker->ExportMap.Num(); Index++)
					{
						FObjectExport& Export = Linker->ExportMap[Index];
						if (Export.ObjectName == NAME_LevelSummary && Export.ClassIndex == SummaryClassIndex)
						{
							Export.Object = StaticConstructObject(UUTLevelSummary::StaticClass(), Linker->LinkerRoot, Export.ObjectName, EObjectFlags(Export.ObjectFlags | RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WasLoaded));
							Export.Object->SetLinker(Linker, Index);
							//GObjLoaded.Add(Export.Object);
							Linker->Preload(Export.Object);
							Export.Object->ConditionalPostLoad();
							break;
						}
					}
				}
			}
			EndLoad();
		}
	}
	if (Summary != NULL)
	{
		MapAuthor->SetText(FText::Format(NSLOCTEXT("SUWCreateGameDialog", "Author", "Author: {0}"), FText::FromString(Summary->Author)));
		MapRecommendedPlayers->SetText(FText::Format(NSLOCTEXT("SUWCreateGameDialog", "OptimalPlayers", "Recommended Players: {0} - {1}"), FText::AsNumber(Summary->OptimalPlayerCount.X), FText::AsNumber(Summary->OptimalPlayerCount.Y)));
		MapDesc->SetText(Summary->Description);
	}
	else
	{
		MapAuthor->SetText(FText::Format(NSLOCTEXT("SUWCreateGameDialog", "Author", "Author: {0}"), FText::FromString(TEXT("Unknown"))));
		MapRecommendedPlayers->SetText(FText::Format(NSLOCTEXT("SUWCreateGameDialog", "OptimalPlayers", "Recommended Players: {0} - {1}"), FText::AsNumber(8), FText::AsNumber(12)));
		MapDesc->SetText(FText());
	}
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
		OnMapSelected(MapList->GetSelectedItem(), ESelectInfo::Direct);
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
	if (MutatorListEnabled.Num() > 0)
	{
		NewURL += FString::Printf(TEXT("?mutator=%s"), *MutatorListEnabled[0]->GetPathName());
		for (int32 i = 1; i < MutatorListEnabled.Num(); i++)
		{
			NewURL += TEXT(",");
			NewURL += MutatorListEnabled[i]->GetPathName();
		}
	}

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

TSharedRef<ITableRow> SUWCreateGameDialog::GenerateMutatorListRow(UClass* MutatorType, const TSharedRef<STableViewBase>& OwningList)
{
	checkSlow(MutatorType->IsChildOf(AUTMutator::StaticClass()));

	FString MutatorName = MutatorType->GetDefaultObject<AUTMutator>()->DisplayName.IsEmpty() ? MutatorType->GetName() : MutatorType->GetDefaultObject<AUTMutator>()->DisplayName.ToString();
	return SNew(STableRow<UClass*>, OwningList)
		.Padding(5)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::White)
			.Text(MutatorName)
		]; 
}

FReply SUWCreateGameDialog::AddMutator()
{
	TArray<UClass*> Selection = AvailableMutators->GetSelectedItems();
	if (Selection.Num() > 0 && Selection[0] != NULL)
	{
		MutatorListEnabled.Add(Selection[0]);
		MutatorListAvailable.Remove(Selection[0]);
		AvailableMutators->RequestListRefresh();
		EnabledMutators->RequestListRefresh();
	}
	return FReply::Handled();
}
FReply SUWCreateGameDialog::RemoveMutator()
{
	TArray<UClass*> Selection = EnabledMutators->GetSelectedItems();
	if (Selection.Num() > 0 && Selection[0] != NULL)
	{
		MutatorListAvailable.Add(Selection[0]);
		MutatorListEnabled.Remove(Selection[0]);
		AvailableMutators->RequestListRefresh();
		EnabledMutators->RequestListRefresh();
	}
	return FReply::Handled();
}
FReply SUWCreateGameDialog::ConfigureMutator()
{
	TArray<UClass*> Selection = EnabledMutators->GetSelectedItems();
	if (Selection.Num() > 0 && Selection[0] != NULL)
	{
		checkSlow(Selection[0]->IsChildOf(AUTMutator::StaticClass()));
		AUTMutator* Mut = Selection[0]->GetDefaultObject<AUTMutator>();
		if (Mut->ConfigMenu != NULL)
		{
			UUserWidget* Widget = CreateWidget<UUserWidget>(GetPlayerOwner()->GetWorld(), Mut->ConfigMenu);
			if (Widget != NULL)
			{
				Widget->AddToViewport();
			}
		}
	}
	return FReply::Handled();
}

#endif