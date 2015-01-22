// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KDevelopSourceCodeAccessor.h"

class FKDevelopSourceCodeAccessModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FKDevelopSourceCodeAccessor& GetAccessor();

private:
	FKDevelopSourceCodeAccessor KDevelopSourceCodeAccessor;
};

