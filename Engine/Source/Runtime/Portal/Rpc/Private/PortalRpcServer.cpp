// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PortalRpcPrivatePCH.h"
#include "PortalRpcServer.h"
#include "IPortalRpcServer.h"
#include "PortalRpcMessages.h"
#include "MessageRpcServer.h"
#include "IMessageContext.h"

class FPortalRpcServerImpl
	: public FMessageRpcServer
	, public IPortalRpcServer
{
public:

	virtual void ConnectTo(const FMessageAddress& Address) const
	{
		MessageEndpoint->Send(new FPortalRpcServer(GetAddress().ToString()), Address);
	}

	virtual IMessageRpcServer* GetMessageServer()
	{
		return static_cast<IMessageRpcServer*>(this);
	}

public:

	virtual ~FPortalRpcServerImpl() { }

private:

	FPortalRpcServerImpl()
		: FMessageRpcServer()
	{}

private:

	friend FPortalRpcServerFactory;
};

TSharedRef<IPortalRpcServer> FPortalRpcServerFactory::Create()
{
	return MakeShareable(new FPortalRpcServerImpl());
}
