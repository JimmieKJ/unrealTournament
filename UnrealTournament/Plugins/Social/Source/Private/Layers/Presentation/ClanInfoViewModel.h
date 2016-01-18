// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE ""

enum class EClanActionLevel
{
	Action,			// A normal action
	Emphasis,		// An important action to be shown with emphasis
	Critical,		// A critical action
};


// Enum holding friend action types
namespace EClanActionType
{
	enum Type : uint8
	{
		AcceptClanRequest,		// Accept an incoming clan request
		IgnoreClanRequest,		// Ignore an incoming clan request
		SendClanRequest,		// Send an outgoing clan request
		CancelClanRequest,		// Cancel an outgoing clan request
		LeaveClan,				// Leave Clan
		SetPrimaryClan,			// Set as Primary Clan
		MAX_None,
	};

	inline FText ToText(EClanActionType::Type State)
	{
		switch (State)
		{
		case AcceptClanRequest: return LOCTEXT("AcceptClanRequest", "Accept");
		case IgnoreClanRequest: return LOCTEXT("IgnoreClanRequest", "Ignore");
		case CancelClanRequest: return LOCTEXT("RejectClanRequest", "Reject");
		case LeaveClan: return LOCTEXT("CancelClanRequest", "Cancel");
		case SetPrimaryClan: return LOCTEXT("SetPrimary", "Set Primary");

		default: return FText::GetEmpty();
		}
	}

	inline EClanActionLevel ToActionLevel(EClanActionType::Type State)
	{
		switch (State)
		{
		case AcceptClanRequest:
		case SendClanRequest:
			return EClanActionLevel::Emphasis;
		case LeaveClan:
			return EClanActionLevel::Critical;
		default:
			return EClanActionLevel::Action;
		}
	}
};

class FClanInfoViewModel
	: public TSharedFromThis<FClanInfoViewModel>
{
public:
	virtual ~FClanInfoViewModel() {}

	virtual FText GetClanTitle() const = 0;
	virtual int32 GetMemberCount() const = 0;
	virtual TSharedRef<class FFriendListViewModel > GetFriendListViewModel() = 0;
	virtual FText GetListCountText() const = 0;
	virtual FText GetClanBrushName() const = 0;
	virtual void EnumerateActions(TArray<EClanActionType::Type>& OUTActions) = 0;
	virtual void PerformAction(EClanActionType::Type ClanAction) = 0;
	virtual bool IsPrimaryClan() const = 0;

	DECLARE_EVENT_OneParam(FClanInfoViewModel, FOpenClanDetails, const TSharedRef<class FClanInfoViewModel>&)
	virtual FOpenClanDetails& OpenClanDetails() = 0;
};

/**
 * Creates the implementation for an ClanInfoViewModel.
 *
 * @return the newly created ClanInfoViewModel implementation.
 */
FACTORY(TSharedRef< FClanInfoViewModel >, FClanInfoViewModel,
	const TSharedRef<class IClanInfo>& ClanInfo,
	const TSharedRef<class IFriendListFactory>& FriendsListFactory,
	const TSharedRef<class FChatSettingsService>& ChatSettingService);

#undef LOCTEXT_NAMESPACE