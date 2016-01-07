// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "FriendsChatMarkupService.h"
#include "IChatCommunicationService.h"
#include "FriendsNavigationService.h"
#include "GameAndPartyService.h"
#include "FriendViewModel.h"
#include "FriendListViewModel.h"
#include "IFriendItem.h"
#include "IFriendList.h"

#define LOCTEXT_NAMESPACE ""

class FFriendsChatMarkupServiceFactoryFactory;
class FFriendsChatMarkupServiceFactoryImpl;

struct FProcessedInput
{
	FProcessedInput()
		: ChatChannel(EChatMessageType::Invalid)
		, NeedsTip(false)
		, FoundMatch(false)
		, HasFriendToken(false)
		, FillFromSpace(false)
	{}

	EChatMessageType::Type ChatChannel;
	FString Message;
	bool NeedsTip; // The input will only be executed when we no longer require a tip
	bool FoundMatch;
	bool HasFriendToken; // If we have tried to narrow the search by entering a user name
	bool FillFromSpace; // If we are completed via space

	TArray<TSharedPtr<FFriendViewModel> > ValidFriends;
	TArray<TSharedPtr<FFriendViewModel> > MatchedFriends;
	TArray<TSharedPtr<IChatTip> > NavigationTips;
	TArray<TSharedPtr<IChatTip> > ActionTips;
	TArray<TSharedPtr<IChatTip> > ValidCustomTips;

	void Clear()
	{
		ValidFriends.Empty();
		MatchedFriends.Empty();
		ValidCustomTips.Empty();
		NavigationTips.Empty();
		ActionTips.Empty();
		NeedsTip = false;
		FoundMatch = false;
		ChatChannel = EChatMessageType::Invalid;
		HasFriendToken = false;
		FillFromSpace = false;
	}
};

class FChatActionTip : public IChatTip
{
public:

	virtual FText GetTipText() override
	{
		return CommandTextToken;
	}

	virtual FText GetTipTextParam() override
	{
		return EChatActionType::ShortcutParamString(ActionType);
	}

	virtual bool IsEnabled() override
	{
		return MarkupService->IsActionTipEnabled(ActionType);
	}

	virtual bool IsValidForType(EChatMessageType::Type ChatChannel) override
	{
		return false;
	}

	virtual bool IsValidForToken(FString& NavigationToken) override
	{
		return CommandToken.Contains(NavigationToken);
	}

	virtual FReply ExecuteTip() override
	{
		MarkupService->PerformTipAction(ActionType);
		return FReply::Handled();
	}

	virtual void ExecuteCommand() override
	{
		MarkupService->PerformTipAction(ActionType);
	}

	FChatActionTip(const TSharedRef<FFriendsChatMarkupService>& InMarkupService, EChatActionType::Type InActionType)
		: MarkupService(InMarkupService)
		, ActionType(InActionType)
		{
			CommandTextToken = EChatActionType::ToText(InActionType);
			CommandToken = EChatActionType::ShortcutString(InActionType);
		}

	virtual ~FChatActionTip(){}

	private:
	TSharedRef<FFriendsChatMarkupService> MarkupService;
	EChatActionType::Type ActionType;
	FString CommandToken;
	FText CommandTextToken;
};

class FChatTip : public IChatTip
{
public:
	virtual FText GetTipText() override
	{
		return FText::FromString(EChatMessageType::ShortcutString(ChatChannel));
	}

	virtual FText GetTipTextParam() override
	{
		return EChatMessageType::ShortcutParamString(ChatChannel);
	}

	virtual bool IsValidForType(EChatMessageType::Type InChatChannel) override
	{
		return ChatChannel == InChatChannel;
	}

	virtual bool IsValidForToken(FString& NavigationToken) override
	{
		return EChatMessageType::ShortcutString(ChatChannel).Contains(NavigationToken);
	}

	virtual bool IsEnabled() override
	{
		return true;
	}

	virtual FReply ExecuteTip() override
	{
		MarkupService->SetValidatedText(EChatMessageType::ShortcutString(ChatChannel) + TEXT(" "));
		return FReply::Handled();
	}

	virtual void ExecuteCommand() override
	{
		if(ChatChannel != EChatMessageType::Whisper)
		{
			MarkupService->NavigateToChannel(ChatChannel);
		}
		else
		{
			ExecuteTip();
		}
	}

	FChatTip(const TSharedRef<FFriendsChatMarkupService>& InMarkupService, EChatMessageType::Type InChatChannel)
		: MarkupService(InMarkupService)
		, ChatChannel(InChatChannel)
		{}

	virtual ~FChatTip(){}

private:
	TSharedRef<FFriendsChatMarkupService> MarkupService;
	EChatMessageType::Type ChatChannel;
};

// Tip for picking an outgoing friend
class FFriendChatTip : public IChatTip
{
public:
	virtual FText GetTipText() override
	{
		return FText::Format(LOCTEXT("MarkupFriendTip", "{0} {1}"), FText::FromString(EChatMessageType::ShortcutString(EChatMessageType::Whisper)), FText::FromString(FriendItem->GetName()));
	}

	virtual bool IsValidForType(EChatMessageType::Type ChatChannel) override
	{
		return true;
	}

	virtual bool IsEnabled() override
	{
		return true;
	}

	virtual FReply ExecuteTip() override
	{
		MarkupService->SetSelectedFriend(FriendItem);
		return FReply::Handled();
	}

	virtual void ExecuteCommand() override
	{
		MarkupService->SetSelectedFriend(FriendItem);
	}

	FFriendChatTip(const TSharedRef<FFriendsChatMarkupService>& InMarkupService, TSharedRef<FFriendViewModel> InFriendItem)
		: MarkupService(InMarkupService)
		, FriendItem(InFriendItem)
		{}

	virtual ~FFriendChatTip(){}

private:
	TSharedRef<FFriendsChatMarkupService> MarkupService;
	TSharedRef<FFriendViewModel> FriendItem;
};

// Custom actions
class FCustomActionChatTip : public IChatTip
{
public:
	virtual FText GetTipText() override
	{
		return FText::FromString(CustomSlashCommand->GetCommandToken());
	}

	virtual bool IsValidForType(EChatMessageType::Type ChatChannel) override
	{
		return true;
	}

	virtual bool IsEnabled() override
	{
		return CustomSlashCommand->IsEnabled();
	}

	virtual FReply ExecuteTip() override
	{
		CustomSlashCommand->ExecuteCommand();
		MarkupService->NotifyCommandExecuted();
		return FReply::Handled();
	}

	virtual void ExecuteCommand() override
	{
		CustomSlashCommand->ExecuteCommand();
		MarkupService->NotifyCommandExecuted();
	}

	bool IsEnabled(const FString& NavigationToken)
	{
		if(CustomSlashCommand->GetCommandToken().ToUpper().Contains(NavigationToken.ToUpper()))
		{
			return CustomSlashCommand->IsEnabled();
		}
		return false;
	}

	bool CanExecute(const FString& NavigationToken)
	{
		return NavigationToken.ToUpper() == CustomSlashCommand->GetCommandToken().ToUpper();
	}

	FCustomActionChatTip(const TSharedRef<FFriendsChatMarkupService>& InMarkupService, TSharedRef<ICustomSlashCommand> InCustomSlashCommand)
		: MarkupService(InMarkupService)
		, CustomSlashCommand(InCustomSlashCommand)
		{}

	virtual ~FCustomActionChatTip(){}

private:
	TSharedRef<FFriendsChatMarkupService> MarkupService;
	TSharedRef<ICustomSlashCommand> CustomSlashCommand;
};

class FInvalidChatTip : public IChatTip
{
public:
	virtual FText GetTipText() override
	{
		if(InvalidReason.IsEmpty())
		{
			return LOCTEXT("InvalidChatTip", "Invalid markup");
		}
		return InvalidReason;
	}

	virtual bool IsValidForType(EChatMessageType::Type ChatChannel) override
	{
		return true;
	}

	virtual bool IsEnabled() override
	{
		return true;
	}

	virtual FReply ExecuteTip() override
	{
		return FReply::Handled();
	}

	virtual void ExecuteCommand() override
	{
		// Do Nothing
	}

	FInvalidChatTip(FText InInvalidReason)
	 : InvalidReason(InInvalidReason)
	{}

	FInvalidChatTip()
	 : InvalidReason(FText::GetEmpty())
	{}

	virtual ~FInvalidChatTip(){}

private:
	FText InvalidReason;
};


class FFriendsChatMarkupServiceImpl
	: public FFriendsChatMarkupService
{
public:

	virtual void AddCustomSlashMarkupCommand(TArray<TSharedRef<ICustomSlashCommand> >& InCustomSlashCommands) override
	{
		CustomSlashCommands = InCustomSlashCommands;
		CustomChatTips.Empty();
		for(const auto& ChatCommand : CustomSlashCommands)
		{
			CustomChatTips.Add(MakeShareable(new FCustomActionChatTip(SharedThis(this), ChatCommand)));
		}
	}

	virtual void CloseChatTips() override
	{
		SelectedChatTip.Reset();
		OnChatTipAvailable().Broadcast(false);
	}

	bool ValidateSlashMarkup(const FString NewMessage, const FString PlainText)
	{
		TSharedPtr<FProcessedInput> ValidatedInput = ProcessInputText(NewMessage, PlainText);

		if (ValidatedInput.IsValid())
		{
			if (ValidatedInput->ChatChannel == EChatMessageType::Global)
			{
				if(ValidatedInput->Message.IsEmpty())
				{
					NavigationService->ChangeViewChannel(EChatMessageType::Global);
				}
				else
				{
					CommunicationService->SendRoomMessage(FString(), ProcessedInput->Message);
					OnMessageCommitted().Broadcast();
				}
			}
			else if (ValidatedInput->ChatChannel == EChatMessageType::Party)
			{
				if(ValidatedInput->Message.IsEmpty())
				{
					NavigationService->ChangeViewChannel(EChatMessageType::Party);
				}
				else
				{
					if(GamePartyService->ShouldUseGameChatForPartyOutput())
					{
						CommunicationService->SendGameMessage(ProcessedInput->Message);
						OnMessageCommitted().Broadcast();
					}
					else
					{
						FChatRoomId PartyChatRoomId = GamePartyService->GetPartyChatRoomId();
						CommunicationService->SendRoomMessage(PartyChatRoomId, ProcessedInput->Message);
						OnMessageCommitted().Broadcast();
					}
				}
			}
			else if (ValidatedInput->ChatChannel == EChatMessageType::Team)
			{
				if(ValidatedInput->Message.IsEmpty())
				{
					NavigationService->ChangeViewChannel(EChatMessageType::Team);
				}
				else
				{
					FChatRoomId TeamChatRoomId = GamePartyService->GetTeamChatRoomId();
					CommunicationService->SendRoomMessage(TeamChatRoomId, ProcessedInput->Message);
					OnMessageCommitted().Broadcast();
				}
			}
			else if (ValidatedInput->ChatChannel == EChatMessageType::Game)
			{
				if(ValidatedInput->Message.IsEmpty())
				{
					NavigationService->ChangeViewChannel(EChatMessageType::Game);
				}
				else
				{
					CommunicationService->SendGameMessage(ProcessedInput->Message);
					OnMessageCommitted().Broadcast();
				}
			}
			else if (ValidatedInput->ChatChannel == EChatMessageType::Whisper)
			{
				if(ValidatedInput->ValidFriends.Num())
				{
					if(ProcessedInput->Message.IsEmpty())
					{
						// Sending the selected outgoing friend
						NavigationService->SetOutgoingChatFriend(ProcessedInput->ValidFriends[0]->GetFriendItem());
					}
					else
					{
						NavigationService->SetOutgoingChatFriend(ProcessedInput->ValidFriends[0]->GetFriendItem(), false);
						CommunicationService->SendPrivateMessage(ProcessedInput->ValidFriends[0]->GetFriendItem(), FText::FromString(ProcessedInput->Message));
						OnMessageCommitted().Broadcast();
					}
				}
				else
				{
					// Change outgoing chat channel to last chat friend
					NavigationService->ChangeViewChannel(EChatMessageType::Whisper);
				}
			}
			else if(ValidatedInput->ChatChannel == EChatMessageType::Custom)
			{
				if(ValidatedInput->ValidCustomTips.Num() && ValidatedInput->ValidCustomTips[0].IsValid())
				{
					ValidatedInput->ValidCustomTips[0]->ExecuteCommand();
				}
			}
			else if(ValidatedInput->ChatChannel == EChatMessageType::LiveStreaming)
			{
				if(ValidatedInput->Message.IsEmpty())
				{
					NavigationService->ChangeViewChannel(EChatMessageType::LiveStreaming);
				}
				else
				{
					// Send Live Stream message
					OnSendLiveStreamMessageEvent().Broadcast(ProcessedInput->Message);
					OnMessageCommitted().Broadcast();
				}
			}
			return true;
		}
		return false;
	}

	virtual void SetInputText(FString InInputText, FString InPlainText, bool InForceDisplayTip) override
	{
		this->InputText = InInputText;
		TSharedPtr<FProcessedInput> ValidatedInput = ProcessInputText(InputText, InPlainText);
		if(ValidatedInput.IsValid() && ValidatedInput->MatchedFriends.Num() > 0)
		{
			SetSelectedFriend(ValidatedInput->MatchedFriends[0]);
			OnChatTipAvailable().Broadcast(false);
		}

		if(InputText.StartsWith("/") && (!ValidatedInput.IsValid() || ValidatedInput->NeedsTip))
		{
			OnChatTipAvailable().Broadcast(true);
		}
		else if(InputText.IsEmpty() || (ValidatedInput.IsValid() && !ValidatedInput->NeedsTip))
		{
			OnChatTipAvailable().Broadcast(false);
		}
		GenerateTips();
	}

	virtual void SetValidatedText(FString InInputText) override
	{
		if(InInputText.StartsWith("/") && ProcessedInput->NeedsTip)
		{
			if(ProcessedInput->FillFromSpace)
			{
				InInputText = InInputText.TrimTrailing();
				ProcessedInput->FillFromSpace = false;
			}
			OnChatTipAvailable().Broadcast(true);
		}
		InputText = InInputText;
		OnValidateInputReady().Broadcast();
	}

	virtual FText GetValidatedInput() override
	{
		return FText::FromString(InputText);
	}

	virtual void SetSelectedFriend(TSharedPtr<FFriendViewModel> FriendViewModel) override
	{
		if(FriendViewModel.IsValid())
		{
			FString HyperlinkStyle = TEXT("UserNameTextStyle.DefaultHyperlink");
			FFormatNamedArguments Args;
			Args.Add(TEXT("SenderName"), FText::FromString(FriendViewModel->GetName()));
			Args.Add(TEXT("UniqueID"), FText::FromString(FriendViewModel->GetFriendItem()->GetUniqueID()->ToString()));

			Args.Add(TEXT("NameStyle"), FText::FromString(HyperlinkStyle));

			FText ValidatedText = FText::Format(NSLOCTEXT("FFriendsChatMarkupService", "DisplayNameAndMessage", "/whisper <a id=\"UserName\" uid=\"{UniqueID}\" Username=\"{SenderName}\" style=\"{NameStyle}\">{SenderName}</> "), Args);
			ValidatedInputString = ValidatedText.ToString();
			if(ProcessedInput->FillFromSpace)
			{
				ValidatedInputString = ValidatedInputString.TrimTrailing();
				ProcessedInput->FillFromSpace = false;
			}

			SelectedFriendViewModel = FriendViewModel;
			ProcessedInput->ValidFriends.Empty();
			ProcessedInput->ValidFriends.Add(FriendViewModel);
			ProcessedInput->NeedsTip = false;
			ProcessedInput->ChatChannel = EChatMessageType::Whisper;
			SetValidatedText(ValidatedInputString);
			
			CloseChatTips();
		}
		else
		{
			ValidatedInputString.Empty();
			SelectedFriendViewModel = nullptr;
		}
		ProcessedInput->MatchedFriends.Empty();
	}

	virtual void SetNextValidFriend() override
	{
		GetFriendViewModels();
		if(FriendViewModels.Num() > 0)
		{
			if(MarkupFriendViewModel.IsValid())
			{
				int32 FriendIndex = FriendViewModels.Find(MarkupFriendViewModel);
				FriendIndex++;
				if(FriendIndex > FriendViewModels.Num() - 1)
				{
					MarkupFriendViewModel = FriendViewModels[0];
					NavigationService->SetOutgoingChatFriend(FriendViewModels[0]->GetFriendItem());
				}
				else
				{
					MarkupFriendViewModel = FriendViewModels[FriendIndex];
					NavigationService->SetOutgoingChatFriend(FriendViewModels[FriendIndex]->GetFriendItem());
				}
			}
			else
			{
				MarkupFriendViewModel = FriendViewModels[0];
			}

			if(MarkupFriendViewModel.IsValid())
			{
				NavigationService->SetOutgoingChatFriend(MarkupFriendViewModel->GetFriendItem());
			}
		}
	}

	virtual bool IsAtEndOfFriendsList() override
	{
		GetFriendViewModels();
		return FriendViewModels.Num() && MarkupFriendViewModel == FriendViewModels.Last();
	}

	virtual void SetLastMessageFriend(TSharedPtr<const FUniqueNetId> SenderId) override
	{
		if(!RecentFriendViewModel.IsValid() || RecentFriendViewModel->GetFriendItem()->GetUniqueID() != SenderId)
		{
			for(const auto& Friend : GetFriendViewModels())
			{
				if(Friend->GetFriendItem()->GetUniqueID() == SenderId)
				{
					RecentFriendViewModel = Friend;
					break;
				}
			}
		}
	}

	virtual void NavigateToChannel(EChatMessageType::Type ChatChannel) override
	{
		NavigationService->ChangeViewChannel(ChatChannel);
		SetValidatedText(TEXT(""));
		ProcessedInput->Clear();
		CloseChatTips();
	}

	virtual void NotifyCommandExecuted() override
	{
		SetValidatedText(TEXT(""));
		ProcessedInput->Clear();
		CloseChatTips();
	}

	virtual void PerformTipAction(EChatActionType::Type Action) override
	{
		switch (Action)
		{
		case EChatActionType::Reply:
			if(IsReplyTargetValid())
			{
				SetSelectedFriend(RecentFriendViewModel);
			}
			NavigateToChannel(EChatMessageType::Whisper);
			break;
		case EChatActionType::NaviateToWhisper:
			NavigateToChannel(EChatMessageType::Whisper);
			break;
		case EChatActionType::Invalid:
			break;
		default:
			break;
		}
	}

	virtual bool IsActionTipEnabled(EChatActionType::Type Action) override
	{
		switch (Action)
		{
		case EChatActionType::Reply:
			return IsReplyTargetValid();
			break;
		case EChatActionType::Invalid:
			return false;
			break;
		default:
			return true;
		}
	}

	virtual FReply HandleChatKeyEntry(const FKeyEvent& KeyEvent) override
	{
		FReply Reply = FReply::Unhandled();
		if(KeyEvent.GetKey() == EKeys::Tab || KeyEvent.GetKey() == EKeys::Enter)
		{
			if(SelectedChatTip.IsValid() && ProcessedInput.IsValid() && ProcessedInput->NeedsTip == true )
			{
				if(KeyEvent.GetKey() == EKeys::Enter)
				{
					SelectedChatTip->ExecuteCommand();
				}
				else
				{
					SelectedChatTip->ExecuteTip();
				}
				Reply = FReply::Handled();
			}
			else if(KeyEvent.GetKey() == EKeys::Tab)
			{
				NavigationService->CycleChatChannel();
				Reply = FReply::Handled();
			}
		}
		else if (KeyEvent.GetKey() == EKeys::SpaceBar && SelectedChatTip.IsValid())
		{
			if(ProcessedInput.IsValid() && ProcessedInput->NeedsTip == true)
			{
				if(ProcessedInput->ChatChannel != EChatMessageType::Whisper || ProcessedInput->HasFriendToken)
				{
					ProcessedInput->FillFromSpace = true;
					SelectedChatTip->ExecuteTip();
					Reply = FReply::Handled();
				}
			}
		}
		else if (ProcessedInput.IsValid() && ProcessedInput->NeedsTip)
		{
			if(KeyEvent.GetKey() == EKeys::Up)
			{
				SetPreviousCommand();
				Reply = FReply::Handled();
			}
			else if( KeyEvent.GetKey() == EKeys::Down)
			{
				SetNextCommand();
				Reply = FReply::Handled();
			}
			else if(KeyEvent.GetKey() == EKeys::Escape)
			{
				OnChatTipAvailable().Broadcast(false);
				Reply = FReply::Handled();
			}
		}
		else if(KeyEvent.GetKey() == EKeys::Escape)
		{
			Reply = FReply::Handled();
		}

		return Reply;
	}

	virtual TArray<TSharedRef<IChatTip> >& GetChatTips() override
	{
		return ChatTipArray;
	}

	virtual TSharedPtr<IChatTip> GetActiveTip() override
	{
		return SelectedChatTip;
	}

	virtual EChatMessageType::Type GetMarkupChannel() const override
	{
		if(ProcessedInput.IsValid())
		{
			return ProcessedInput->ChatChannel;
		}
		return EChatMessageType::Invalid;
	}

	virtual bool IsReplyTargetValid() const override
	{
		return RecentFriendViewModel.IsValid() && RecentFriendViewModel->IsOnline();
	}

	DECLARE_DERIVED_EVENT(FFriendsChatMarkupServiceImpl, FFriendsChatMarkupService::FChatInputUpdated, FChatInputUpdated);
	virtual FChatInputUpdated& OnInputUpdated() override
	{
		return ChatInputUpdatedEvent;
	}

	DECLARE_DERIVED_EVENT(FFriendsChatMarkupServiceImpl, FFriendsChatMarkupService::FChatTipSelected, FChatTipSelected);
	virtual FChatTipSelected& OnChatTipSelected() override
	{
		return ChatTipSelectedEvent;
	}

	DECLARE_DERIVED_EVENT(FFriendsChatMarkupServiceImpl, FFriendsChatMarkupService::FChatTipAvailable, FChatTipAvailable);
	virtual FChatTipAvailable& OnChatTipAvailable() override
	{
		return ChatTipAvailableEvent;
	}

	DECLARE_DERIVED_EVENT(FFriendsChatMarkupServiceImpl, FFriendsChatMarkupService::FValidatedChatReadyEvent, FValidatedChatReadyEvent);
	virtual FValidatedChatReadyEvent& OnValidateInputReady() override
	{
		return ValidatedChatReadyEvent;
	}

	DECLARE_DERIVED_EVENT(FFriendsChatMarkupServiceImpl, FFriendsChatMarkupService::FChatMessageCommitted, FChatMessageCommitted)
	virtual FChatMessageCommitted& OnMessageCommitted() override
	{
		return ChatMessageCommittedEvent;
	}

	DECLARE_DERIVED_EVENT(FFriendsChatMarkupServiceImpl, FFriendsChatMarkupService::FSendLiveStreamMessageEvent, FSendLiveStreamMessageEvent)
	virtual FSendLiveStreamMessageEvent& OnSendLiveStreamMessageEvent() override
	{
		return SendLiveStreamMessageEvent;
	}

private:

	void GenerateTips()
	{
		SelectedChatTip.Reset();
		ChatTipArray.Empty();
		if(ProcessedInput->ChatChannel == EChatMessageType::Whisper)
		{
			if(!ProcessedInput->HasFriendToken)
			{
				ChatTipArray.Add(NavigateToWhisperTip.ToSharedRef());
			}

			if(ProcessedInput->ValidFriends.Num())
			{
				for(const auto& Friend : ProcessedInput->ValidFriends)
				{
					ChatTipArray.Add(MakeShareable(new FFriendChatTip(SharedThis(this), Friend.ToSharedRef())));
				}
			}
			else
			{
				ChatTipArray.Add(MakeShareable(new FInvalidChatTip(LOCTEXT("InvalidChatTip_NoFriends", "No matching friends currently online"))));
			}
		}
		else if(ProcessedInput->ChatChannel == EChatMessageType::Navigation)
		{
			for(const auto& SlashCommand : ProcessedInput->NavigationTips)
			{
				if(SlashCommand->IsEnabled())
				{
					ChatTipArray.Add(SlashCommand.ToSharedRef());
				}
			}

			for(const auto& SlashCommand : ProcessedInput->ValidCustomTips)
			{
				if(SlashCommand->IsEnabled())
				{
					ChatTipArray.Add(SlashCommand.ToSharedRef());
				}
			}
		}
		else if(ProcessedInput->ChatChannel != EChatMessageType::Invalid)
		{
			bool bAddedPartyChat = false;
			for(const auto& ChatTip : CommmonChatTips)
			{
				if(ProcessedInput->ChatChannel == EChatMessageType::Empty || ChatTip->IsValidForType(ProcessedInput->ChatChannel))
				{
					if(ChatTip->IsValidForType(EChatMessageType::Party))
					{
						if(GamePartyService->IsInPartyChat())
						{
							ChatTipArray.Add(ChatTip);
							bAddedPartyChat = true;
						}
						else if(GamePartyService->ShouldCombineGameChatIntoPartyTab() && !bAddedPartyChat && GamePartyService->IsInGameChat())
						{
							ChatTipArray.Add(ChatTip);
							bAddedPartyChat = true;
						}
					}
					else if(ChatTip->IsValidForType(EChatMessageType::Team))
					{
						if(GamePartyService->IsInTeamChat())
						{
							ChatTipArray.Add(ChatTip);
						}
					}
					else if(ChatTip->IsValidForType(EChatMessageType::Game))
					{
						if(GamePartyService->IsInGameChat())
						{
							ChatTipArray.Add(ChatTip);
						}
					}
					else if(ChatTip->IsEnabled())
					{
						ChatTipArray.Add(ChatTip); 
					}
				}
			}

			if(ProcessedInput->ChatChannel == EChatMessageType::Empty)
			{
				for(const auto& ChatTip : CustomChatTips)
				{
					if(ChatTip->IsEnabled())
					{
						ChatTipArray.Add(ChatTip); 
					}
				}
			}
		}

		if(ChatTipArray.Num() > 0)
		{
			SelectedChatTip = ChatTipArray[0];
			SelectedChatIndex = 0;
		}
		else
		{
			ChatTipArray.Add(MakeShareable(new FInvalidChatTip()));
		}

		OnInputUpdated().Broadcast();
	}

	void SetNextCommand()
	{
		if(SelectedChatTip.IsValid() && ChatTipArray.Num() > 0)
		{
			SelectedChatIndex++;
			if(SelectedChatIndex > ChatTipArray.Num() - 1)
			{
				SelectedChatIndex = 0;
			}
			SelectedChatTip = ChatTipArray[SelectedChatIndex];
			OnChatTipSelected().Broadcast(SelectedChatTip.ToSharedRef());
		}
	}

	void SetPreviousCommand()
	{
		if(SelectedChatTip.IsValid() && ChatTipArray.Num() > 0)
		{
			SelectedChatIndex--;
			if(SelectedChatIndex < 0)
			{
				SelectedChatIndex = ChatTipArray.Num() -1;
			}
			SelectedChatTip = ChatTipArray[SelectedChatIndex];
			OnChatTipSelected().Broadcast(SelectedChatTip.ToSharedRef());
		}
	}

	TSharedPtr<FProcessedInput> ProcessInputText(FString InInputText, FString InPlainText)
	{
		bool NeedsValidation = true;
		if(SelectedFriendViewModel.IsValid() && ProcessedInput.IsValid())
		{
			NeedsValidation = false;
			if(!InInputText.StartsWith(ValidatedInputString))
			{
				SetSelectedFriend(nullptr);
				SetValidatedText(InPlainText);
				NeedsValidation = true;
			}
			else
			{
				// Double check no friends names match
				ProcessedInput->Message = InInputText.RightChop(ValidatedInputString.Len());
				return ProcessedInput;
			}
		}

		if (bEnableSlahCommands && NeedsValidation && InPlainText.StartsWith(TEXT("/")))
		{
			ProcessedInput->ChatChannel = EChatMessageType::Invalid;
			ProcessedInput->NeedsTip = true;
			ProcessedInput->ValidFriends.Empty();
			ProcessedInput->MatchedFriends.Empty();
			ProcessedInput->Message.Empty();

			FString NavigationToken, Remainder;
			if(InPlainText.Contains(TEXT(" ")))
			{
				InPlainText.Split(TEXT(" "), &NavigationToken, &Remainder);
			}
			else
			{
				NavigationToken = InPlainText;
			}

			EChatMessageType::Type ChatType = EChatMessageType::EnumFromString(NavigationToken);
			if(ChatType != EChatMessageType::Invalid)
			{
				ProcessFriendList(ChatType, Remainder,InInputText, NavigationToken);
			}
			else
			{
				// Process tips
				GetValidMarkupTips(NavigationToken);

				ProcessedInput->ValidCustomTips.Empty();

				// Get potential tips
				// Validate each tip

				for (const auto& CustomChatTip : CustomChatTips)
				{
					if(CustomChatTip->IsEnabled(NavigationToken))
					{
						ProcessedInput->ChatChannel = EChatMessageType::Navigation;
						ProcessedInput->ValidCustomTips.Add(CustomChatTip);
					}
					if(CustomChatTip->CanExecute(NavigationToken))
					{
						ProcessedInput->ChatChannel = EChatMessageType::Custom;
						ProcessedInput->NeedsTip = false;
					}
				}
			}
			return ProcessedInput;
		}

		// Text is no longer markup - clear any existing tips
		ProcessedInput->Clear();
		return nullptr;
	}

	void ProcessFriendList(EChatMessageType::Type ChatType, FString& Remainder, FString& InInputText, FString& NavigationToken)
	{
		ProcessedInput->ChatChannel = ChatType;
		if(ChatType == EChatMessageType::Whisper)
		{
			if(!Remainder.IsEmpty())
			{
				// Mark that we have a friend token so we don't display the navigation tip
				ProcessedInput->HasFriendToken = true;
				FString PotentialName = Remainder.Left(25).Replace(TEXT(" "), TEXT(""));

				for(const auto& Friend : GetFriendViewModels())
				{
					if(Remainder.EndsWith(TEXT(" ")) && Friend->GetNameNoSpaces() == PotentialName)
					{
						// Found match
						ProcessedInput->MatchedFriends.Add(Friend);
					}
					else if(Friend->GetNameNoSpaces().Contains(PotentialName))
					{
						ProcessedInput->ValidFriends.Add(Friend);
					}
				}
			}
			else
			{
				ProcessedInput->HasFriendToken = false;
				for(const auto& Friend : GetFriendViewModels())
				{
					ProcessedInput->ValidFriends.Add(Friend);
				}
			}
		}
		else if (ChatType == EChatMessageType::Party)
		{
			if(GamePartyService->IsInPartyChat() || GamePartyService->ShouldUseGameChatForPartyOutput())
			{
				ProcessedInput->NeedsTip = false;
				ProcessedInput->Message = InInputText.RightChop(NavigationToken.Len() + 1);
			}
			else
			{
				ProcessedInput->ChatChannel = EChatMessageType::Invalid;
			}
		}
		else if (ChatType == EChatMessageType::Game)
		{
			if(!GamePartyService->ShouldCombineGameChatIntoPartyTab() && GamePartyService->IsInGameChat())
			{
				ProcessedInput->NeedsTip = false;
				ProcessedInput->Message = InInputText.RightChop(NavigationToken.Len() + 1);
			}
			else
			{
				ProcessedInput->ChatChannel = EChatMessageType::Invalid;
			}
		}
		else if (ChatType == EChatMessageType::Team)
		{
			if(GamePartyService->IsInTeamChat())
			{
				ProcessedInput->NeedsTip = false;
				ProcessedInput->Message = InInputText.RightChop(NavigationToken.Len() + 1);
			}
			else
			{
				ProcessedInput->ChatChannel = EChatMessageType::Invalid;
			}
		}
		else if(ChatType != EChatMessageType::Empty)
		{
			ProcessedInput->NeedsTip = false;
			ProcessedInput->Message = InInputText.RightChop(NavigationToken.Len() + 1);
		}
	}

	void GetValidMarkupTips(FString& NavigationToken)
	{
		ProcessedInput->NavigationTips.Empty();
		for (const auto& CommonChatTip : CommmonChatTips)
		{
			if(CommonChatTip->IsValidForToken(NavigationToken))
			{
				if (CommonChatTip->IsValidForType(EChatMessageType::Party))
				{
					if(GamePartyService->IsInPartyChat() || GamePartyService->ShouldUseGameChatForPartyOutput())
					{
						ProcessedInput->NavigationTips.Add(CommonChatTip);
					}
				}
				else if (CommonChatTip->IsValidForType(EChatMessageType::Game))
				{
					if(!GamePartyService->ShouldCombineGameChatIntoPartyTab() && GamePartyService->IsInGameChat())
					{
						ProcessedInput->NavigationTips.Add(CommonChatTip);
					}
				}
				else if (CommonChatTip->IsValidForType(EChatMessageType::Team))
				{
					if(GamePartyService->IsInTeamChat())
					{
						ProcessedInput->NavigationTips.Add(CommonChatTip);
					}
				}
				else if (CommonChatTip->IsValidForType(EChatMessageType::LiveStreaming))
				{
					if(GamePartyService->IsLiveStreaming())
					{
						ProcessedInput->NavigationTips.Add(CommonChatTip);
					}
				}
				else
				{
					ProcessedInput->NavigationTips.Add(CommonChatTip);
				}
			}
		}

		if(ProcessedInput->NavigationTips.Num())
		{
			ProcessedInput->ChatChannel = EChatMessageType::Navigation;
		}
	}

	TArray< TSharedPtr<FFriendViewModel > > GetFriendViewModels()
	{
		// Can't create friends list on init yet as still depending on Friends singleton
		CreateFriendsList();
		return FriendViewModels;
	}

	TSharedPtr<FFriendViewModel> FindFriendViewModel(FString UserName)
	{
		for(const auto& Friend : FriendViewModels)
		{
			if(Friend->GetName() == UserName)
			{
				return Friend;
			}
		}
		return nullptr;
	}

	void CreateFriendsList()
	{
		if(!FriendsList.IsValid())
		{
			FriendsList = FriendsListFactory->Create(EFriendsDisplayLists::DefaultDisplay);
			FriendsList->OnFriendsListUpdated().AddSP(this, &FFriendsChatMarkupServiceImpl::HandleFriendListUpdated);
			HandleFriendListUpdated();
		}
	}

	void HandleFriendListUpdated()
	{
		FriendViewModels.Empty();
		TArray< TSharedPtr<FFriendViewModel > > AllFriendViewModels;
		FriendsList->GetFriendList(AllFriendViewModels);
		for(const auto& Friend : AllFriendViewModels)
		{
			const EOnlinePresenceState::Type PresenceState = Friend->GetOnlineStatus();
			if(PresenceState == EOnlinePresenceState::Online || PresenceState == EOnlinePresenceState::Away || PresenceState == EOnlinePresenceState::ExtendedAway)
			{
				FriendViewModels.Add(Friend);
			}
		}
	}

	void Initialize(bool bInEnableSlahCommands, bool bEnableGlobalChat, bool bAllowTeamChat)
	{
		bEnableSlahCommands = bInEnableSlahCommands;
		if(bEnableSlahCommands)
		{
			if(bEnableGlobalChat)
			{
				CommmonChatTips.Add(MakeShareable(new FChatTip(SharedThis(this), EChatMessageType::Global)));
			}
			CommmonChatTips.Add(MakeShareable(new FChatTip(SharedThis(this), EChatMessageType::Party)));

			if(bAllowTeamChat)
			{
				CommmonChatTips.Add(MakeShareable(new FChatTip(SharedThis(this), EChatMessageType::Team)));
			}

			CommmonChatTips.Add(MakeShareable(new FChatTip(SharedThis(this), EChatMessageType::Whisper)));
			CommmonChatTips.Add(MakeShareable(new FChatActionTip(SharedThis(this), EChatActionType::Reply)));

			if(!GamePartyService->ShouldCombineGameChatIntoPartyTab())
			{
				CommmonChatTips.Add(MakeShareable(new FChatTip(SharedThis(this), EChatMessageType::Game)));
			}

			if(GamePartyService->IsLiveStreaming())
			{
				CommmonChatTips.Add(MakeShareable(new FChatTip(SharedThis(this), EChatMessageType::LiveStreaming)));
			}

			NavigateToWhisperTip = MakeShareable(new FChatActionTip(SharedThis(this), EChatActionType::NaviateToWhisper));
		}

		ProcessedInput = MakeShareable(new FProcessedInput());
	}

	FFriendsChatMarkupServiceImpl(
			const TSharedRef<IChatCommunicationService >& InCommunicationService,
			const TSharedRef<FFriendsNavigationService>& InNavigationService,
			const TSharedRef<IFriendListFactory>& InFriendsListFactory,
			const TSharedRef<FGameAndPartyService>& InGamePartyService
			)
		: CommunicationService(InCommunicationService)
		, NavigationService(InNavigationService)
		, FriendsListFactory(InFriendsListFactory)
		, GamePartyService(InGamePartyService)
		, InputText()
		, bEnableSlahCommands(true)
		{}

private:
	TSharedRef<IChatCommunicationService > CommunicationService;
	TSharedRef<FFriendsNavigationService > NavigationService;
	TSharedRef<IFriendListFactory> FriendsListFactory;
	TSharedRef<FGameAndPartyService> GamePartyService;
	TArray<TSharedRef<ICustomSlashCommand> > CustomSlashCommands;
	TSharedPtr<IFriendList> FriendsList;
	TArray< TSharedPtr<FFriendViewModel > > FriendViewModels;
	TArray<TSharedRef<IChatTip> > CommmonChatTips;
	TSharedPtr<IChatTip> NavigateToWhisperTip;
	TArray<TSharedRef<FCustomActionChatTip> > CustomChatTips;
	FChatInputUpdated ChatInputUpdatedEvent;
	FChatTipSelected ChatTipSelectedEvent;
	FChatTipAvailable ChatTipAvailableEvent;
	FValidatedChatReadyEvent ValidatedChatReadyEvent;
	FChatMessageCommitted ChatMessageCommittedEvent;
	FSendLiveStreamMessageEvent SendLiveStreamMessageEvent;
	FString InputText;
	TSharedPtr<FFriendViewModel> SelectedFriendViewModel;
	TSharedPtr<FFriendViewModel> RecentFriendViewModel;
	TSharedPtr<FFriendViewModel> MarkupFriendViewModel;
	FString ValidatedInputString;
	TSharedPtr<FProcessedInput> ProcessedInput;
	TArray<TSharedRef<IChatTip> > ChatTipArray;
	TSharedPtr<IChatTip> SelectedChatTip;
	int32 SelectedChatIndex;
	bool bEnableSlahCommands;

	friend FFriendsChatMarkupServiceFactoryImpl;
};

class FFriendsChatMarkupServiceFactoryImpl
	: public IFriendsChatMarkupServiceFactory
{
public:
	virtual TSharedRef<FFriendsChatMarkupService> Create(bool bEnableSlashCommands, bool bEnableGloablChat, bool bAllowTeamChat) override
	{
		TSharedRef<FFriendsChatMarkupServiceImpl> Service = MakeShareable(
			new FFriendsChatMarkupServiceImpl(CommunicationService, NavigationService,FriendsListFactory,GamePartyService));
		Service->Initialize(bEnableSlashCommands, bEnableGloablChat, bAllowTeamChat);
		return Service;
	}

	virtual ~FFriendsChatMarkupServiceFactoryImpl(){}

private:

	FFriendsChatMarkupServiceFactoryImpl(
		const TSharedRef<IChatCommunicationService >& InCommunicationService,
		const TSharedRef<FFriendsNavigationService>& InNavigationService,
		const TSharedRef<IFriendListFactory>& InFriendsListFactory,
		const TSharedRef<FGameAndPartyService>& InGamePartyService
		)
		: CommunicationService(InCommunicationService)
		, NavigationService(InNavigationService)
		, FriendsListFactory(InFriendsListFactory)
		, GamePartyService(InGamePartyService)
	{ }

private:

	TSharedRef<IChatCommunicationService > CommunicationService;
	TSharedRef<FFriendsNavigationService> NavigationService;
	TSharedRef<IFriendListFactory> FriendsListFactory;
	TSharedRef<FGameAndPartyService> GamePartyService;

	friend FFriendsChatMarkupServiceFactoryFactory;
};

TSharedRef< IFriendsChatMarkupServiceFactory > FFriendsChatMarkupServiceFactoryFactory::Create(
	const TSharedRef<IChatCommunicationService >& CommunicationService,
	const TSharedRef<FFriendsNavigationService>& NavigationService,
	const TSharedRef<IFriendListFactory>& FriendsListFactory,
	const TSharedRef<FGameAndPartyService>& GamePartyService)
{
	TSharedRef< FFriendsChatMarkupServiceFactoryImpl > Service = MakeShareable(new FFriendsChatMarkupServiceFactoryImpl(CommunicationService, NavigationService, FriendsListFactory, GamePartyService));
	return Service;
}
#undef LOCTEXT_NAMESPACE