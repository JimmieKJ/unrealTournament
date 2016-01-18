// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "McpProfileSysPCH.h"

DEFINE_LOG_CATEGORY(LogProfileSys);

/**
 * Mcp Profile System Module
 */
class FMcpProfileSysModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule( ) override { }
	virtual void ShutdownModule( ) override { }
};

IMPLEMENT_MODULE(FMcpProfileSysModule, McpProfileSys);

