// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VisualStudioSourceCodeAccessor.h"

class FVisualStudioSourceCodeAccessModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FVisualStudioSourceCodeAccessor& GetAccessor();

private:
	FVisualStudioSourceCodeAccessor VisualStudioSourceCodeAccessor;
};