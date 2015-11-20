// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class IPortalRpcServer;

class FPortalRpcServerFactory
{
public:
	static TSharedRef<IPortalRpcServer> Create();
};