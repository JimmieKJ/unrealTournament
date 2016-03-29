// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

class IPortalService;
class IMessageRpcClient;

class FPortalUserProxyFactory
{
public:
	static TSharedRef<IPortalService> Create(const TSharedRef<IMessageRpcClient>& RpcClient);
}; 