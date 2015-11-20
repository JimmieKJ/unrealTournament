// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class IPortalRpcResponder;

class FPortalRpcResponderFactory
{
public:
	static TSharedRef<IPortalRpcResponder> Create();
};