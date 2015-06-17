// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "TAttributeProperty.h"
#include "SlateBasics.h"
#include "SWidgetSwitcher.h"
#include "../SUWPanel.h"
#include "../Widgets/SUTTabButton.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUWCreateGamePanel : public SUWPanel, public FGCObject
{
public:
	virtual void ConstructPanel(FVector2D ViewportSize);	
	virtual TSharedRef<SWidget> BuildGamePanel(TSubclassOf<AUTGameMode> InitialSelectedGameClass);

	virtual ~SUWCreateGamePanel();

	void GetCustomGameSettings(FString& GameMode, FString& StartingMap, FString& Description, TArray<FString>&GameOptions, int32& DesiredPlayerCount, int32 BotSkillLevel);

protected:
	enum EServerStartMode
	{
		SERVER_Standalone,
		SERVER_Listen,
		SERVER_Dedicated,
	};
	/** records start mode when content warning is being displayed before starting */
	EServerStartMode PendingStartMode;

	TSharedPtr<SVerticalBox> GameConfigPanel;

	TArray< TSharedPtr<FMapListItem> > AllMaps;
	TSharedPtr< SComboBox< TSharedPtr<FMapListItem> > > MapList;
	TSharedPtr<STextBlock> SelectedMap;
	TArray<UClass*> AllGametypes;
	TSharedPtr< SComboBox<UClass*> > GameList;
	TSharedPtr<STextBlock> SelectedGameName;
	TSubclassOf<AUTGameMode> SelectedGameClass;
	TSharedPtr<STextBlock> MapAuthor;
	TSharedPtr<STextBlock> MapRecommendedPlayers;
	TSharedPtr<STextBlock> MapDesc;
	TArray<UClass*> MutatorListAvailable, MutatorListEnabled;
	TSharedPtr< SListView<UClass*> > AvailableMutators;
	TSharedPtr< SListView<UClass*> > EnabledMutators;
	FSlateDynamicImageBrush* LevelScreenshot;

	/** currently opened mutator config menu - if valid, can't open another
	 * workaround for Slate not supporting modal dialogs...
	 */
	TWeakObjectPtr<class UUserWidget> MutatorConfigMenu;

	TSharedPtr<SVerticalBox> GamePanel;

	TSharedPtr<SUTTabButton> GameSettingsTabButton;
	TSharedPtr<STextBlock> GameSettingsLabel;
	TSharedPtr<SUTTabButton> ServerSettingsTabButton;
	TSharedPtr<STextBlock> ServerSettingsLabel;

	// container for pointers to TAttributeProperty objects linked directly to setting properties
	TArray< TSharedPtr<TAttributePropertyBase> > PropertyLinks;

	// holders for pointers to game config properties so the objects don't die and invalidate the delegates
	TArray< TSharedPtr<TAttributePropertyBase> > GameConfigProps;

	void OnMapSelected(TSharedPtr<FMapListItem> NewSelection, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateGameNameWidget(UClass* InItem);
	TSharedRef<SWidget> GenerateMapNameWidget(TSharedPtr<FMapListItem> InItem);
	void OnGameSelected(UClass* NewSelection, ESelectInfo::Type SelectInfo);

	void Cancel();

	TSharedRef<ITableRow> GenerateMutatorListRow(UClass* MutatorType, const TSharedRef<STableViewBase>& OwningList);
	FReply AddMutator();
	FReply RemoveMutator();
	FReply ConfigureMutator();
	FReply ConfigureBots();

	TSharedPtr<SGridPanel> MutatorGrid;


	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObjects(AllGametypes);
	}

	void OnTextChanged(const FText& NewText);

	TSharedRef<SWidget> AddMutatorMenu();

	void GetCustomMutatorOptions(UClass* MutatorClass, FString& Description, TArray<FString>&GameOptions);
	// Returns true if this custom screen has everything needed to play

public:

	bool IsReadyToPlay();

};

#endif