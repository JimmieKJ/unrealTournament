// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PartyPrivatePCH.h"
#include "Chatroom.h"
#include "Engine/GameInstance.h"
#include "OnlineSubsystemUtils.h"

inline FString GetLocalUserNickName(UWorld* World, const FUniqueNetId& LocalUserId)
{
	check(World);
	UGameInstance* GameInstance = World->GetGameInstance();
	check(GameInstance);
	ULocalPlayer* LP = Cast<ULocalPlayer>(GameInstance->FindLocalPlayerFromUniqueNetId(LocalUserId));
	if (LP)
	{
		return LP->GetNickname();
	}

	return TEXT("INVALID");
}

UChatroom::UChatroom()
	: MaxChatRoomRetries(5)
	, NumChatRoomRetries(0)
{
}

void UChatroom::UnregisterDelegates()
{
	IOnlineChatPtr ChatInt = Online::GetChatInterface(GetWorld());
	if (ChatInt.IsValid())
	{
		if (ChatRoomCreatedDelegateHandle.IsValid())
		{
            // Failsafe if chat CreateRoom never completes (xmpp is misbehaving)
			ChatInt->ClearOnChatRoomCreatedDelegate_Handle(ChatRoomCreatedDelegateHandle);
		}
	}
}

void UChatroom::CreateOrJoinChatRoom(const FUniqueNetIdRepl& LocalUserId, const FChatRoomId& ChatRoomId, const FOnChatRoomCreatedOrJoined& CompletionDelegate, const FString& Password)
{
	bool bShouldFireDelegate = false;
	if (!ChatRoomId.IsEmpty())
	{
		if (LocalUserId.IsValid())
		{
			UWorld* World = GetWorld();
			IOnlineChatPtr ChatInt = Online::GetChatInterface(World);
			if (ensure(ChatInt.IsValid()))
			{
				if (!IsAlreadyInChatRoom(LocalUserId, ChatRoomId))
				{
					if (CurrentChatRoomId.IsEmpty())
					{
						UE_LOG(LogOnlineChat, Verbose, TEXT("[UChatroom::CreateOrJoinChatRoom] attempting to join %s"), *ChatRoomId);
						CurrentChatRoomId = ChatRoomId;
						GetTimerManager().ClearTimer(ChatRoomRetryTimerHandle);

						FOnChatRoomJoinPublicDelegate ChatRoomDelegate;
						ChatRoomDelegate.BindUObject(this, &ThisClass::OnChatRoomCreatedOrJoined, CompletionDelegate, Password);

						FChatRoomConfig ChatRoomConfig;
						ChatRoomConfig.bPasswordRequired = !Password.IsEmpty();
						ChatRoomConfig.Password = Password;

						// Try to create the room first (it will create if it doesn't exist, or just join if it does)
						ChatRoomCreatedDelegateHandle = ChatInt->AddOnChatRoomCreatedDelegate_Handle(ChatRoomDelegate);
						ChatInt->CreateRoom(*LocalUserId, ChatRoomId, GetLocalUserNickName(World, *LocalUserId), ChatRoomConfig);
					}
					else
					{
						if (ChatRoomId == CurrentChatRoomId)
						{
							UE_LOG(LogOnlineChat, Verbose, TEXT("[UChatroom::CreateOrJoinChatRoom] already joining %s"), *ChatRoomId);
						}
						else
						{
							UE_LOG(LogOnlineChat, Verbose, TEXT("[UChatroom::CreateOrJoinChatRoom] can't join %s already joining %s"), *ChatRoomId, *CurrentChatRoomId);
							bShouldFireDelegate = true;
						}
					}
				}
				else
				{
					UE_LOG(LogOnlineChat, Verbose, TEXT("[UChatroom::CreateOrJoinChatRoom] already joined %s"), *ChatRoomId);
				}
			}
		}
		else
		{
			UE_LOG(LogOnlineChat, Verbose, TEXT("[UChatroom::CreateOrJoinChatRoom] invalid user id"));
			bShouldFireDelegate = true;
		}
	}
	else
	{
		UE_LOG(LogOnlineChat, Verbose, TEXT("[UChatroom::CreateOrJoinChatRoom] invalid chat room id"));
		bShouldFireDelegate = true;
	}

	if (bShouldFireDelegate)
	{
		CompletionDelegate.ExecuteIfBound(ChatRoomId, false);
	}
}

void UChatroom::OnChatRoomCreatedOrJoined(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error, FOnChatRoomCreatedOrJoined CompletionDelegate, FString Password)
{
	if (CurrentChatRoomId == RoomId)
	{
		UE_LOG(LogOnlineChat, Verbose, TEXT("[UChatroom::OnChatRoomCreatedOrJoined] %s joined room %s Success: %d%s"), *LocalUserId.ToString(), *RoomId, bWasSuccessful, !Error.IsEmpty() ? *FString::Printf(TEXT(" %s"), *Error) : TEXT(""));

		// Don't clear if this delegate failed because of existing pending join
		const bool bAlreadyJoining = !Error.IsEmpty() && Error.Contains(TEXT("operation pending"));
		if (!bAlreadyJoining)
		{
			IOnlineChatPtr ChatInt = Online::GetChatInterface(GetWorld());
			if (ChatInt.IsValid())
			{
				ChatInt->ClearOnChatRoomCreatedDelegate_Handle(ChatRoomCreatedDelegateHandle);
			}

			if (bWasSuccessful)
			{
				FTimerDelegate Delegate = FTimerDelegate::CreateLambda([RoomId, CompletionDelegate, bWasSuccessful]()
				{
					// Announce that chat is available
					CompletionDelegate.ExecuteIfBound(RoomId, bWasSuccessful);
				});

				GetTimerManager().SetTimerForNextTick(Delegate);
			}
			else
			{
				if (NumChatRoomRetries < MaxChatRoomRetries)
				{
					NumChatRoomRetries++;
					UE_LOG(LogOnlineChat, Verbose, TEXT("[UChatroom::OnChatRoomCreatedOrJoined] retry %d/%d"), NumChatRoomRetries, MaxChatRoomRetries);

					FUniqueNetIdRepl StrongUserId(LocalUserId.AsShared());

					auto RetryDelegate = FTimerDelegate::CreateLambda([this, StrongUserId, RoomId, CompletionDelegate, Password]()
					{
						// Attempt to rejoin room shortly
						CreateOrJoinChatRoom(StrongUserId, RoomId, CompletionDelegate, Password);
					});

					GetTimerManager().SetTimer(ChatRoomRetryTimerHandle, RetryDelegate, .3f, false);
				}
				else
				{
					UE_LOG(LogOnlineChat, Verbose, TEXT("[UChatroom::OnChatRoomCreatedOrJoined] exceeded %d retries"), MaxChatRoomRetries);
				}

				// Clear out the chat id since we failed to join
				CurrentChatRoomId.Empty();
			}
		}
		else
		{
			UE_LOG(LogOnlineChat, Verbose, TEXT("[UChatroom::OnChatRoomCreatedForRejoin] already attempting to join %s"), *CurrentChatRoomId);
		}
	}
	else
	{
		UE_LOG(LogOnlineChat, Verbose, TEXT("[UChatroom::OnChatRoomCreatedForRejoin] other chat room detected %s, waiting for %s"), *RoomId, *CurrentChatRoomId);
	}
}

void UChatroom::LeaveChatRoom(const FUniqueNetIdRepl& LocalUserId, const FOnChatRoomLeft& CompletionDelegate)
{
	if (!CurrentChatRoomId.IsEmpty())
	{
		UE_LOG(LogOnlineChat, Verbose, TEXT("[UChatroom::LeaveChatRoom] %s leaving chat room %s"),
			*LocalUserId.ToString(),
			*CurrentChatRoomId);

		// Always reset these regardless of chat room
		GetTimerManager().ClearTimer(ChatRoomRetryTimerHandle);
		ChatRoomRetryTimerHandle.Invalidate();
		NumChatRoomRetries = 0;

		// ExitRoom callback could fire inline if it fails, make a copy so the ensure in OnChatRoomLeft does not hit
		FChatRoomId ChatRoomIdCpy = CurrentChatRoomId;
		CurrentChatRoomId.Empty();

		if (IsOnline())
		{
			// Leave chat
			IOnlineChatPtr ChatInt = Online::GetChatInterface(GetWorld());
			if (ensure(ChatInt.IsValid()))
			{
				FOnChatRoomCreatedDelegate ChatRoomDelegate;
				ChatRoomDelegate.BindUObject(this, &ThisClass::OnChatRoomLeft, ChatRoomIdCpy, CompletionDelegate);

				ChatRoomLeftDelegateHandle = ChatInt->AddOnChatRoomExitDelegate_Handle(ChatRoomDelegate);
				ChatInt->ExitRoom(*LocalUserId, ChatRoomIdCpy);
			}
		}
		else
		{
			UE_LOG(LogOnlineChat, Display, TEXT("[UChatroom::LeaveChatRoom] Left chat while not logged in"));
			// Clear out the chat room to avoid ensures (logged out, not in this room anymore)
			OnChatRoomLeft(*LocalUserId, ChatRoomIdCpy, true, FString(), ChatRoomIdCpy, CompletionDelegate);
		}
	}
	else
	{
		UE_LOG(LogOnlineChat, Verbose, TEXT("[UChatroom::LeaveChatRoom] %s no chat room to leave."), *LocalUserId.ToString());
	}
}

void UChatroom::OnChatRoomLeft(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error, FChatRoomId ChatRoomIdCopy, FOnChatRoomLeft CompletionDelegate)
{
	if (ChatRoomIdCopy == RoomId)
	{
		// Not much to do failure or otherwise, just print logs here
		UE_LOG(LogOnlineChat, Verbose, TEXT("[UChatroom::OnChatRoomLeft] %s left chat room %s Success: %d%s"), *LocalUserId.ToString(), *RoomId, bWasSuccessful, !Error.IsEmpty() ? *FString::Printf(TEXT(" %s"), *Error) : TEXT(""));

		IOnlineChatPtr ChatInt = Online::GetChatInterface(GetWorld());
		if (ensure(ChatInt.IsValid()))
		{
			ChatInt->ClearOnChatRoomExitDelegate_Handle(ChatRoomLeftDelegateHandle);
		}

		ensure(CurrentChatRoomId.IsEmpty());

		GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([CompletionDelegate, RoomId]()
		{
			// Announce that chat is available
			CompletionDelegate.ExecuteIfBound(RoomId);
		}));
	}
	else
	{
		UE_LOG(LogOnlineChat, Verbose, TEXT("[UChatroom::OnChatRoomLeft] other chat room detected %s, waiting for %s"), *RoomId, *ChatRoomIdCopy);
	}
}

bool UChatroom::IsOnline() const
{
	return true;
}

bool UChatroom::IsAlreadyInChatRoom(const FUniqueNetIdRepl& LocalUserId, const FChatRoomId& ChatRoomId) const
{
	if (ensure(LocalUserId.IsValid()))
	{
		UWorld* World = GetWorld();
		IOnlineChatPtr ChatInt = Online::GetChatInterface(World);
		if (ensure(ChatInt.IsValid()))
		{
			TArray<FChatRoomId> JoinedRooms;
			ChatInt->GetJoinedRooms(*LocalUserId, JoinedRooms);

			auto Pred = [&ChatRoomId](const FChatRoomId& OtherChatRoomId)
			{
				return OtherChatRoomId == ChatRoomId;
			};

			return JoinedRooms.ContainsByPredicate(Pred);
		}
	}

	return false;
}

UWorld* UChatroom::GetWorld() const
{
	UWorld* World = GetOuter() ? GetOuter()->GetWorld() : nullptr;
	checkf(World, TEXT("[UChatroom::GetWorld] Should have an outer that can access a world"));
	return World;
}

FTimerManager& UChatroom::GetTimerManager() const
{
	UWorld* World = GetWorld();
	check(World);
	return World->GetTimerManager();
}

