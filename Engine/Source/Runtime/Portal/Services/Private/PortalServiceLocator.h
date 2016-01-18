// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class IPortalServiceLocator;
class FTypeContainer;

class FPortalServiceLocatorFactory
{
public:
	static TSharedRef<IPortalServiceLocator> Create(const TSharedRef<FTypeContainer>& ServiceDependencies);
};
