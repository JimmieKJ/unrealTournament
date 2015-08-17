// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "TAttributeProperty.h"
#include "UTLobbyMatchInfo.h"
#include "../Widgets/SUTButton.h"
#include "../Widgets/SUTComboButton.h"
#include "UTGameEngine.h"

#define MAX_CHAT_LINES = 1000;

#if !UE_SERVER

DECLARE_DELEGATE_OneParam(FChatDestinationChangedDelegate, FName);

class FChatDestination : public TSharedFromThis<FChatDestination>
{
public:
	// The Caption of the button
	FText Caption;

	// The Destination for the Chat/etc
	FName ChatDestination;

	TSharedPtr<SUTButton> Button;
	TSharedPtr<SVerticalBox> ChatBox;

	TArray<TSharedPtr<FStoredChatMessage>> Chat;

	// The tab panel will be sorted by this weight
	float Weight;

	FChatDestination(const FText& inCaption, const FName& inChatDestination, float inWeight)
		: Caption(inCaption)
		, ChatDestination(inChatDestination)
		, Weight(inWeight)
	{}

	static TSharedRef<FChatDestination> Make(const FText& inCaption, const FName& inChatDestination, float inWeight)
	{
		return MakeShareable(new FChatDestination(inCaption, inChatDestination, inWeight));
	}

	FSlateColor GetButtonColorAndOpacity() const
	{
		return (Button.IsValid() && Button->IsPressed() ? FSlateColor(FLinearColor::White) : FSlateColor(FLinearColor::Gray));
	}	

	FText GetButtonCaption()
	{
		return Caption;
	}

	void HandleChat(TSharedPtr<FStoredChatMessage> ChatMessage)
	{
		TOptional<EFocusCause> FocusCause = ChatBox->HasAnyUserFocus();
		if (FocusCause.IsSet())
		{
			UE_LOG(UT,Log,TEXT("ChatBox Has Focus"));
		}

		if (ChatBox.IsValid())
		{
			// Look to see if the chatbox is too big.

			if (Chat.Num() >= 1000)
			{
				Chat.RemoveAt(0,1);
				TSharedRef<SWidget> Slot = ChatBox->GetChildren()->GetChildAt(0);
				ChatBox->RemoveSlot(Slot);
			}

			Chat.Add(ChatMessage);
		
			if (ChatMessage->Type == ChatDestinations::MOTD || ChatMessage->Type == ChatDestinations::System)
			{
				ChatBox->AddSlot()
				.AutoHeight()
				.Padding(0.0f,10.0f)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(1.0)
					.Padding(5.0f,0.0f,5.0f,0.0f)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBorder)
							.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Navy"))
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.Padding(5.0f,5.0f,5.0f,5.0f)
								[
									SNew(SRichTextBlock)
									.Text(FText::FromString(ChatMessage->Message))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
									.Justification(ETextJustify::Center)
									.DecoratorStyleSet(&SUTStyle::Get())
									.AutoWrapText(true)
								]
							]
						]
					]
				];
			}
			else
			{
				ChatBox->AddSlot()
				.AutoHeight()
				.Padding(0.0,0.0,0.0,0.0)
				.VAlign(VAlign_Top)
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.Padding(5.0,7.0,5.0,0.0)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(UUTGameEngine::ConvertTime(FText::GetEmpty(), FText::GetEmpty(), ChatMessage->Timestamp, false, true))
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny")
					]
					+SHorizontalBox::Slot()
					.Padding(5.0)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.Padding(0.0f,0.0f,5.0f,0.0f)
						[
							SNew(SBorder)
							.BorderImage(SUTStyle::Get().GetBrush( (ChatMessage->bMyChat ? "UT.HeaderBackground.Shaded" : "UT.NoStyle")))
							[
								SNew(SRichTextBlock)
								.Text(FText::Format(NSLOCTEXT("SUTextChatPanel","TextFormat","<UT.Font.ChatText.Name>{0}</>:   {1}"), FText::FromString(ChatMessage->Sender), FText::FromString(ChatMessage->Message)))
								.TextStyle(SUTStyle::Get(), "UT.Font.ChatText.Text")
								.DecoratorStyleSet(&SUTStyle::Get())
								.AutoWrapText(true)
							]
						]
					]
				];
			}

		}
	}

};

class SUPlayerListPanel;

class UNREALTOURNAMENT_API SUTextChatPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUTextChatPanel)
	{}
		SLATE_ARGUMENT( TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner )
		SLATE_EVENT(FChatDestinationChangedDelegate, OnChatDestinationChanged)
	SLATE_END_ARGS()


public:	
	/** needed for every widget */
	void Construct(const FArguments& InArgs);
	
	// Make sure to clean up the delegates in the destructor
	virtual ~SUTextChatPanel();
	
	void AddDestination(const FText& Caption, const FName Destionation, float Weigh, bool bSelect);
	void RemoveDestination(const FName Destination);

	// Determines how this player should be displayed in a given chat group.
	virtual int32 FilterPlayer(AUTPlayerState* PlayerState, FUniqueNetIdRepl PlayerIdToCheck, uint8 TeamNum);

	FName CurrentChatDestination;

	TSharedPtr<SUPlayerListPanel> ConnectedPlayerListPanel;

	void RouteBufferedChat();

protected:
	// The Player Owner that owns this panel
	TWeakObjectPtr<UUTLocalPlayer> PlayerOwner;

	TArray<TSharedPtr<FChatDestination>> ChatDestinationList;
	TSharedPtr<SHorizontalBox> ChatDestinationBar;
	TSharedPtr<SScrollBox> ChatScrollBox;
	TSharedPtr<SEditableTextBox> ChatEditBox;
	TSharedPtr<STextBlock> TypeMsg;

	FReply OnDestinationClick(TSharedPtr<FChatDestination> Destination);

	void DestinationChanged(const FName& NewChatDestionation);

	void BuildChatDestinationBar();

	FDelegateHandle RouteChatHande;

	// Routes chat to the proper destination
	void RouteChat(UUTLocalPlayer* LocalPlayer, TSharedPtr<FStoredChatMessage> ChatMessage);

	void ChatTextChanged(const FText& NewText);
	void ChatTextCommited(const FText& NewText, ETextCommit::Type CommitType);

	FChatDestinationChangedDelegate ChatDestinationChangedDelegate;

};

#endif
