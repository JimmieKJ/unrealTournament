// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "SUMatchPanel.h"
#include "SNumericEntryBox.h"
#include "UTLobbyMatchInfo.h"
#include "UTLocalPlayer.h"
#include "UTGameMode.h"
#include "SULobbyMatchSetupPanel.h"
#include "../Widgets/SUTComboButton.h"

#if !UE_SERVER

class AUTGameMode;


class FMatchPlayerData
{
public:
	TWeakObjectPtr<AUTLobbyPlayerState> PlayerState;
	TSharedPtr<SWidget> Button;
	TSharedPtr<SUTComboButton> ComboButton;
	TSharedPtr<SImage> ReadyImage;

	FMatchPlayerData(TWeakObjectPtr<AUTLobbyPlayerState> inPlayerState, TSharedPtr<SWidget> inButton, TSharedPtr<SUTComboButton> inComboButton, TSharedPtr<SImage> inReadyImage )
		: PlayerState(inPlayerState)
		, Button(inButton)
		, ComboButton(inComboButton)
		, ReadyImage(inReadyImage)
	{}

	static TSharedRef<FMatchPlayerData> Make(TWeakObjectPtr<AUTLobbyPlayerState> inPlayerState, TSharedPtr<SWidget> inButton, TSharedPtr<SUTComboButton> inComboButton, TSharedPtr<SImage> inReadyImage)
	{
		return MakeShareable( new FMatchPlayerData(inPlayerState, inButton, inComboButton, inReadyImage));
	}
};

template<typename T>
struct TGameOption
{
	T Data;
	FGameOptionChangedDelegate OnChangeDelegate;

	TGameOption(T InData)
		: Data(InData)
	{
	}
	TOptional<T> GetOptional() const
	{
		return Data;
	}
	T Get() const
	{
		return Data;
	}

	void Set(T NewValue)
	{
		Data = NewValue;
		OnChangeDelegate.ExecuteIfBound();
	}
};


struct FGameOption : public TSharedFromThis<FGameOption>
{
	
	FString Text;		// The Text of the option
	FString Value;		// The value of this option if it's a text option
	FString Min;		// The Min Value for this option.  Might not be set.
	FString Max;		// The Max Value for this option.  Might not be set.

	TSharedPtr<TGameOption<float>> fValue;
	TSharedPtr<TGameOption<int32>> iValue;
	
	TSharedPtr<SWidget> HostControl;
	TSharedPtr<STextBlock> ClientText;

	FGameOptionChangedDelegate OnChangeDelegate;

	FGameOption()
	{
	};
	
	void CheckBoxChanged(ECheckBoxState NewState)
	{
		Value = NewState == ECheckBoxState::Checked ? TEXT("1") : TEXT("0");
		OnChangeDelegate.ExecuteIfBound();
	}

	void OnOptionChanged()
	{
		OnChangeDelegate.ExecuteIfBound();
	}

	// Converts a GameOption to a sendable/usable string
	FString ToString()
	{
		FString Val = Value;
		if (fValue.IsValid()) Val = FString::Printf(TEXT("%f"), fValue->Get());
		else if (iValue.IsValid()) Val = FString::Printf(TEXT("%i"), iValue->Get());
		
		if (!Min.IsEmpty() && !Max.IsEmpty())
		{
			return FString::Printf(TEXT("%s=%s[%s|%s]"), *Text, *Val, *Min, *Max);
		}
		else
		{
			return FString::Printf(TEXT("%s=%s"), *Text, *Val);
		}
	}

	// Take a string in the format:   Parameter=Value[Min|Max] and parse it in to it's parts
	static bool ParseOption(FString OptionStr, FString& OutOption, FString& OutValue, FString& OutMin, FString& OutMax)
	{
		TArray<FString> Sections;
		OptionStr.ParseIntoArray(&Sections, TEXT("="),true);

		if (Sections.Num() == 2)
		{
			// Pull the name
			OutOption = Sections[0];

			// Break out the values
			TArray<FString> Values;
			Sections[1].ParseIntoArray(&Values, TEXT("["), true);
			if (Values.Num() >= 1)
			{
				OutValue = Values[0];
				if (Values.Num() == 2 && Values[1].Right(1) == TEXT("]"))
				{
					Values[1] = Values[1].Left(Values[1].Len()-1);	// Remove the trailing "]"

					TArray<FString> MinMax;
					Values[1].ParseIntoArray(&MinMax, TEXT("|"), true);
					if (MinMax.Num() == 2)
					{
						OutMin = MinMax[0];
						OutMax = MinMax[1];
					}
				}
			}
			return true;
		}
		return false;
	}

	void ToOption(FString OptionStr, bool bIsHost)
	{
		if ( FGameOption::ParseOption(OptionStr, Text, Value, Min, Max) )
		{
			if (bIsHost)
			{
				// Look to see if this option has been created.
				if (!HostControl.IsValid())
				{
					// NOPE.. build it...
				
					// String Edit
					if (Min == TEXT("") || Max == TEXT(""))
					{
						// No Min/Max provided.  So it's a text edit box.

						SAssignNew(HostControl, SHorizontalBox)

						// Add the Label
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(FText::FromString(Text))
							.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
						]

						+SHorizontalBox::Slot()
						.FillWidth(1.0)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.HAlign(HAlign_Right)
							[
								SNew(SBox)
								.WidthOverride(100)
								[
									SAssignNew(HostControl, SEditableTextBox)
									.Text(FText::FromString(Value))
									//.OnTextCommitted(this, &SUWLoginDialog::OnTextCommited)
									.MinDesiredWidth(70.0f)
								]
							]
						];
						
					}

					// CheckBox
					else if (Min == TEXT("0") && Max == TEXT("1"))		
					{
						// No Min/Max provided.  So it's a text edit box.

						SAssignNew(HostControl, SHorizontalBox)
						// Add the Label
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(Text)
							.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
						]
						+SHorizontalBox::Slot()
						.FillWidth(1.0)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.HAlign(HAlign_Right)
							[
								SNew(SBox)
								.WidthOverride(100)
								[
									SAssignNew(HostControl, SCheckBox)
									.IsChecked( (Value == TEXT("1") ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked))
									.OnCheckStateChanged(this, &FGameOption::CheckBoxChanged)
								]
							]
						];
						
					}
					
					// Numeric Edit
					else
					{
						// Float Edit
						if ( Min.Contains(TEXT(".")) )
						{
							float fMin = FCString::Atoi(*Min);
							float fMax = FCString::Atoi(*Max);
							float V = FCString::Atoi(*Value);
							fValue = MakeShareable(new TGameOption<float>(V));

							SAssignNew(HostControl, SHorizontalBox)

							// Add the Label
							+SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(STextBlock)
								.Text(FText::FromString(Text))
								.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
							]

							+SHorizontalBox::Slot()
							.FillWidth(1.0)
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.HAlign(HAlign_Right)
								[
									SNew(SBox)
									.WidthOverride(100)
									[
										SAssignNew(HostControl, SNumericEntryBox<int32>)
										.Value(iValue.ToSharedRef(), &TGameOption<int32>::GetOptional)
										.OnValueChanged(iValue.ToSharedRef(), &TGameOption<int32>::Set)
										.AllowSpin(true)
										.Delta(1)
										.MinValue(fMin)
										.MaxValue(fMax)
										.MinSliderValue(fMin)
										.MaxSliderValue(fMax)
									]
								]
							];
						}
						// INT Edit
						else
						{
							int32 iMin = FCString::Atoi(*Min);
							int32 iMax = FCString::Atoi(*Max);
							int32 V = FCString::Atoi(*Value);
							iValue = MakeShareable(new TGameOption<int32>(V));
							iValue->OnChangeDelegate.BindRaw(this, &FGameOption::OnOptionChanged);

							SAssignNew(HostControl, SHorizontalBox)

							// Add the Label
							+SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(STextBlock)
								.Text(FText::FromString(Text))
								.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
							]

							+SHorizontalBox::Slot()
							.FillWidth(1.0)
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.HAlign(HAlign_Right)
								[
									SNew(SBox)
									.WidthOverride(100)
									[
										SAssignNew(HostControl, SNumericEntryBox<int32>)
										.Value(iValue.ToSharedRef(), &TGameOption<int32>::GetOptional)
										.OnValueChanged(iValue.ToSharedRef(), &TGameOption<int32>::Set)
										.AllowSpin(true)
										.Delta(1)
										.MinValue(iMin)
										.MaxValue(iMax)
										.MinSliderValue(iMin)
										.MaxSliderValue(iMax)
									]
								]
							];
						}
					}
				}
			}
			else
			{
				// Convert bools...
				if (Min == TEXT("0") && Max == TEXT("1"))
				{
					Value = (Value == TEXT("1") ? NSLOCTEXT("Generic","Yes","Yes").ToString() : NSLOCTEXT("Generic","No","No").ToString());
				}

				// Look to see if this option has been created.
				if (!ClientText.IsValid())
				{
						SAssignNew(HostControl, SHorizontalBox)

						// Add the Label
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(FText::FromString(Text))
							.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
						]

						+SHorizontalBox::Slot()
						.FillWidth(1.0)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.HAlign(HAlign_Right)
							[
								SNew(SBox)
								.WidthOverride(100)
								[
									SAssignNew(ClientText, STextBlock)
									.Text(FText::FromString(Value))
									.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
								]
							]
						];
				}
				else
				{
					ClientText->SetText(FText::FromString(Value));
				}
			}
		}
		else
		{
			UE_LOG(UT,Log, TEXT("Attempted to parse option [%s] that failed"), (!OptionStr.IsEmpty() ? *OptionStr : TEXT("NONE")));
		}
	}
};

class SULobbyGameSettingsPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SULobbyGameSettingsPanel)
		: _bIsHost(false)
	{}
		SLATE_ARGUMENT( TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner )
		SLATE_ARGUMENT( TWeakObjectPtr<AUTLobbyMatchInfo>, MatchInfo )
		SLATE_ARGUMENT( TWeakObjectPtr<AUTGameMode>, DefaultGameMode )
		SLATE_ARGUMENT( bool, bIsHost )

	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	/** We need to build the player list each frame.  */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	/**
	 *	MapChanged is called on non-match host clients when their match map changees
	 **/
	virtual void MapChanged();

	/**
	 *	OptionsChanged is called on non-match host clients when their match options change
	 **/
	virtual void OptionsChanged();

	/**
	 *	Called to commit the local set of options.  This will replicate from the host to the server then out to the clients
	 **/
	virtual void CommitOptions();


protected:

	TArray<TSharedPtr<FGameOption>> GameOptionData;

	// Takes the MatchInfo->MatchOptions string and builds the needed widgets from it.
	virtual void BuildOptionsFromData();

	// Holds a cached list of player data and the slot they belong too.  
	TArray<TSharedPtr<FMatchPlayerData>> PlayerData;

	// Will be true if this is the host's setup window
	bool bIsHost;

	UPROPERTY()
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;

	UPROPERTY()
	TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo;

	TSharedPtr<SULobbyMatchSetupPanel> ParentPanel;
	TSharedPtr<SVerticalBox> OptionsPanel;
	TSharedPtr<SVerticalBox> MapPanel;

	TSharedPtr<SOverlay> PanelContents;
	TSharedPtr<SHorizontalBox> PlayerListBox;

	TSharedRef<SWidget> BuildMapsPanel();
	TSharedRef<SWidget> BuildMapsList();

	TSharedPtr<SListView< TSharedPtr<FAllowedMapData> >> MapList;

	// Can be override in children to construct their contents.  PlayerOwner, MatchInfo and DefaultGameMode will all be filled out by now.
	virtual TSharedRef<SWidget> ConstructContents();

	// Builds the game panel (Game Options and Maps)
	TSharedRef<SWidget> BuildGamePanel();

	// Build the Play List
	virtual void BuildPlayerList(float DeltaTime);

	FOnMatchInfoMapChanged OnMatchMapChangedDelegate;
	FOnMatchInfoOptionsChanged OnMatchOptionsChangedDelegate;

	FText GetReadyButtonText() const;
	void OnSubMenuSelect(int32 MenuCmdId, TSharedPtr<SUTComboButton> Sender);
	FReply Ready();

	FText GetMatchMessage() const;

	float BlinkyTimer;
	int Dots;

	TSharedPtr<STextBlock> StatusText;
	FString GetMapText() const;

	virtual void OnGameOptionChanged();
	TSharedRef<ITableRow> GenerateMapListWidget(TSharedPtr<FAllowedMapData> InItem, const TSharedRef<STableViewBase>& OwningList);
	void OnMapListChanged(TSharedPtr<FAllowedMapData> SelectedItem, ESelectInfo::Type SelectInfo);

};

#endif