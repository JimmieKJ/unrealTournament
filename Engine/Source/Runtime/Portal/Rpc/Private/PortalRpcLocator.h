// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class IPortalRpcLocator;

class FPortalRpcLocatorFactory
{
public:
	static TSharedRef<IPortalRpcLocator> Create();
};
