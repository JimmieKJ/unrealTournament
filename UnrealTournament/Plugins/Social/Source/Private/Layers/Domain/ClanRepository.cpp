// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "ClanRepository.h"
#include "ClanInfo.h"
#include "IFriendItem.h"

#define LOCTEXT_NAMESPACE ""

struct FClanMemberInfo
{
	FString MemberName;
	FText Location;
	TSharedPtr<const FUniqueNetId> MemberId;
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

	virtual const FString GetAppId() const override
	{
		return TEXT("Test ID");
	}

	virtual const FSlateBrush* GetPresenceBrush() const override
	{
		return nullptr;
	}

	FDateTime GetLastSeen() const
	{
		return FDateTime::UtcNow();
	}

	virtual const bool IsOnline() const override
	{
		return false;
	}

	virtual const EOnlinePresenceState::Type GetOnlineStatus() const override
	{
		return EOnlinePresenceState::Offline;
	}

	virtual const TSharedRef< const FUniqueNetId > GetUniqueID() const override
	{
		return MemberInfo.MemberId.ToSharedRef();
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

	virtual bool IsInParty() const override
	{
		return false;
	}

	virtual bool CanJoinParty() const override
	{
		return false;
	}

	virtual bool CanInvite() const override
	{
		return false;
	}

	virtual TSharedPtr<const FUniqueNetId> GetGameSessionId() const override
	{
		return nullptr;
	}

	virtual TSharedPtr<IOnlinePartyJoinInfo> GetPartyJoinInfo() const override
	{
		return nullptr;
	}

	virtual EInviteStatus::Type GetInviteStatus() override
	{
		return EInviteStatus::Accepted;
	}

	virtual void SetWebImageService(const TSharedRef<class FWebImageService>& WebImageService) override
	{}

	FClanMemberInfo MemberInfo;
};

class FClanInfo
	: public TSharedFromThis<FClanInfo>
	, public IClanInfo
{
public:
	FClanInfo()
	{
		IOnlineIdentityPtr OnlineIdentity = Online::GetIdentityInterface(TEXT("MCP"));
		if(OnlineIdentity.IsValid())
		{
			// Add some fake members
			FClanMemberInfo Member;
			Member.MemberName = "Nick";
			Member.Location = FText::FromString("Launcher");
			Member.MemberId = OnlineIdentity->CreateUniquePlayerId("Test1");
			MemberList.Add(MakeShareable(new FClanMember(Member)));
			Member.MemberName = "Tony";
			Member.MemberId = OnlineIdentity->CreateUniquePlayerId("Test2");
			MemberList.Add(MakeShareable(new FClanMember(Member)));
			Member.MemberName = "Bob";
			Member.MemberId = OnlineIdentity->CreateUniquePlayerId("Test3");
			MemberList.Add(MakeShareable(new FClanMember(Member)));
			Member.MemberName = "Terry";
			Member.MemberId = OnlineIdentity->CreateUniquePlayerId("Test4");
			MemberList.Add(MakeShareable(new FClanMember(Member)));
		}
		bPrimaryClan = true;
	}

	virtual FText GetTitle() const override
	{
		return FText::FromString("Test Title");
	}

	virtual const TSharedRef<const FUniqueNetId> GetUniqueID() const override
	{
		return UniqueID.ToSharedRef();
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

	virtual bool IsPrimaryClan() const override
	{
		return bPrimaryClan;
	}

	DECLARE_DERIVED_EVENT(FClanInfo, IClanInfo::FChangedEvent, FChangedEvent)
	virtual IClanInfo::FChangedEvent& OnChanged() override { return ChangedEvent; }

private:
	TArray<TSharedPtr<IFriendItem> > MemberList;
	TSharedPtr<FUniqueNetId> UniqueID;
	bool bPrimaryClan;

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
		// Add some dummy data
		ClanList.Add(MakeShareable(new FClanInfo()));
		ClanList.Add(MakeShareable(new FClanInfo()));
		ClanList.Add(MakeShareable(new FClanInfo()));
	}

private:

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
