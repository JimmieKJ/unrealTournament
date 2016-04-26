// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

class IPortalRpcLocator;

class FPortalRpcLocatorFactory
{
public:
	static TSharedRef<IPortalRpcLocator> Create();
};
