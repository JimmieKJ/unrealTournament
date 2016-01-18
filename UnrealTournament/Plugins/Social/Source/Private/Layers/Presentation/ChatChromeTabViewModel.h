// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Class containing the friend information - used to build the list view.
 */
class FChatChromeTabViewModel
	: public TSharedFromThis<FChatChromeTabViewModel>
{
public:

	virtual bool IsTabVisible() const = 0;
	virtual const FText GetTabText() const = 0;
	virtual const FText GetOutgoingChannelText() const = 0;
	virtual bool HasPendingMessages() const = 0;
	virtual void SetPendingMessage() = 0;
	virtual void ClearPendingMessages() = 0;

	/**
	 * Provides an ID Used in Chat Window navigation.
	 *
	 * @returns an uppercase version of the tab ID.
	 */	
	virtual const EChatMessageType::Type GetTabID() const = 0;
	virtual const EChatMessageType::Type GetOutgoingChannel() const = 0;

	virtual const FSlateBrush* GetTabImage() const = 0;
	virtual const TSharedPtr<class FChatViewModel> GetChatViewModel() const = 0;
	virtual TSharedRef<FChatChromeTabViewModel> Clone(TSharedRef<class FChatDisplayService> ChatDisplayService) = 0;

	DECLARE_EVENT(FChatChromeTabViewModel, FChatTabVisibilityChangedEvent)
	virtual FChatTabVisibilityChangedEvent& OnTabVisibilityChanged() = 0;

	virtual ~FChatChromeTabViewModel() {}


};

/**
* Creates the implementation for an ChatChromeViewModel.
*
* @return the newly created ChatChromeViewModel implementation.
*/
FACTORY(TSharedRef< FChatChromeTabViewModel >, FChatChromeTabViewModel, const TSharedRef<class FChatViewModel>& ChatViewModel);