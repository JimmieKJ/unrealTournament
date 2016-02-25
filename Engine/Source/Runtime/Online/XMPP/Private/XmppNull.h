// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Mock for when Xmpp implementation is not available on a platform */
class FXmppNull
{
public:

	// FXmppNull

	static TSharedRef<class IXmppConnection> CreateConnection();
};

