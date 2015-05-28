// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "ClanRepository.h"
#include "ClanInfo.h"
#include "IFriendItem.h"

#define LOCTEXT_NAMESPACE ""

struct FClanMemberInfo
{
	FString MemberName;
	FText Location;
};

/**
 * Class containing the friend information - used to build the list view.
 */
class FClanMember : public IFriendItem
{
public:

	/**
	 * Constructor takes the required details.
	 */
	FClanMember(FClanMemberInfo InMemberInfo)
		:MemberInfo(InMemberInfo)
	{ }

public:

	virtual const TSharedPtr< FOnlineUser > GetOnlineUser() const override
	{
		return nullptr;
	}

	virtual const TSharedPtr< FOnlineFriend > GetOnlineFriend() const override
	{
		return nullptr;
	}

	virtual const FString GetName() const override
	{
		return MemberInfo.MemberName;
	}

	virtual const FText GetFriendLocation() const override
	{
		return MemberInfo.Location;
	}

	virtual const FString GetClientId() const override
	{
		return TEXT("Test ID");
	}

	virtual const FString GetClientName() const override
	{
		return TEXT("Test Client");
	}

	virtual const TSharedPtr<FUniqueNetId> GetSessionId() const override
	{
		return nullptr;
	}

	virtual const bool IsOnline() const override
	{
		return false;
	}

	virtual const EOnlinePresenceState::Type GetOnlineStatus() const override
	{
		return EOnlinePresenceState::Offline;
	}

	virtual const TSharedRef< FUniqueNetId > GetUniqueID() const override
	{
		TSharedPtr<FUniqueNetId> TestID = MakeShareable(new FUniqueNetIdString(TEXT("Test")));
		return TestID.ToSharedRef();
	}

	virtual const EFriendsDisplayLists::Type GetListType() const override
	{
		return EFriendsDisplayLists::ClanMemberDisplay;
	}

	virtual void SetOnlineFriend( TSharedPtr< FOnlineFriend > InOnlineFriend ) override
	{}

	virtual void SetOnlineUser( TSharedPtr< FOnlineUser > InOnlineFriend ) override
	{}

	virtual void ClearUpdated() override
	{}

	virtual bool IsUpdated() override
	{
		return false;
	}

	virtual void SetPendingInvite() override
	{}

	virtual void SetPendingAccept() override
	{}

	virtual void SetPendingDelete() override
	{}

	virtual bool IsPendingDelete() const override
	{
		return false;
	}

	virtual bool IsPendingAccepted() const override
	{
		return false;
	}

	virtual bool IsGameRequest() const override
	{
		return false;
	}

	virtual bool IsGameJoinable() const override
	{
		return false;
	}

	virtual bool CanInvite() const override
	{
		return false;
	}

	virtual TSharedPtr<FUniqueNetId> GetGameSessionId() const override
	{
		return nullptr;
	}

	virtual EInviteStatus::Type GetInviteStatus() override
	{
		return EInviteStatus::Accepted;
	}

	FClanMemberInfo MemberInfo;
};

class FClanInfo
	: public TSharedFromThis<FClanInfo>
	, public IClanInfo
{
public:
	FClanInfo()
	{
		// Add some fake members
		FClanMemberInfo Member;
		Member.MemberName = "Nick";
		Member.Location = FText::FromString("Launcher");
		MemberList.Add(MakeShareable(new FClanMember(Member)));
		Member.MemberName = "Tony";
		MemberList.Add(MakeShareable(new FClanMember(Member)));
		Member.MemberName = "Bob";
		MemberList.Add(MakeShareable(new FClanMember(Member)));
		Member.MemberName = "Terry";
		MemberList.Add(MakeShareable(new FClanMember(Member)));
	}

	virtual FText GetTitle() const override
	{
		return FText::FromString("Test Title");
	}

	virtual FText GetDescription() const override
	{
		return FText::FromString("Test Description");
	}

	virtual FText GetLongDescription() const override
	{
		return FText::FromString("Test Long Description");
	}

	virtual TArray<TSharedPtr<IFriendItem> > GetMemberList() const override
	{
		return MemberList;
	}

	virtual FText GetClanBrushName() const override
	{
		return FText::FromString("TileMapEditor.LayerEyeOpened");
	}

	virtual int32 GetClanMembersCount() const override
	{
		return MemberList.Num();
	}

	DECLARE_DERIVED_EVENT(FClanInfo, IClanInfo::FChangedEvent, FChangedEvent)
	virtual IClanInfo::FChangedEvent& OnChanged() override { return ChangedEvent; }

private:
	TArray<TSharedPtr<IFriendItem> > MemberList;

	FChangedEvent ChangedEvent;
};

class FClanRepository : 
	public IClanRepository,
	public TSharedFromThis<FClanRepository>
{
public:

	virtual ~FClanRepository()
	{
	}

	virtual void EnumerateClanList(TArray<TSharedRef<IClanInfo>>& OUTClanList) override
	{
		// Do some filtering here
		for(const auto& ClanItem : ClanList)
		{
			OUTClanList.Add(ClanItem);
		}
	}

	DECLARE_DERIVED_EVENT(FClanRepository, IClanRepository::FOnClanListUpdated, FOnClanListUpdated)
	virtual FOnClanListUpdated& OnClanListUpdated() override
	{
		return OnClanListUpdatedEvent;
	}

	DECLARE_DERIVED_EVENT(FClanRepository, IClanRepository::FOnClanJoined, FOnClanJoined)
	virtual FOnClanJoined& OnClanJoined() override
	{
		return OnClanJoinedEvent;
	}

	DECLARE_DERIVED_EVENT(FClanRepository, IClanRepository::FOnClanLeft, FOnClanLeft)
	virtual FOnClanLeft& OnClanLeft() override
	{
		return OnClanLeftEvent;
	}

private:
	void Initialize()
	{
		OnlineSub = IOnlineSubsystem::Get( TEXT( "MCP" ) );

		// Add some dummy data
		ClanList.Add(MakeShareable(new FClanInfo()));
		ClanList.Add(MakeShareable(new FClanInfo()));
		ClanList.Add(MakeShareable(new FClanInfo()));
	}

private:
	IOnlineSubsystem* OnlineSub;

	FOnClanListUpdated OnClanListUpdatedEvent;
	FOnClanJoined OnClanJoinedEvent;
	FOnClanLeft OnClanLeftEvent;

	TArray<TSharedRef<FClanInfo>> ClanList;

	friend FClanRepositoryFactory;
};

TSharedRef<IClanRepository> FClanRepositoryFactory::Create()
{
	TSharedRef<FClanRepository> Repository = MakeShareable(new FClanRepository());
	Repository->Initialize();
	return Repository;
}

#undef LOCTEXT_NAMESPACE
