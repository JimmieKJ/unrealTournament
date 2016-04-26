// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

class IPortalServiceLocator;
class FTypeContainer;

class FPortalServiceLocatorFactory
{
public:
	static TSharedRef<IPortalServiceLocator> Create(const TSharedRef<FTypeContainer>& ServiceDependencies);
};
