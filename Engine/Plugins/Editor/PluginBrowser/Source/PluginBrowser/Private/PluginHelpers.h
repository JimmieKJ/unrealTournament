// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FPluginHelpers
{
public:
	/**
	 * Copy the contents of a template folder to create a new plugin, replacing instances of PLUGIN_NAME with the new plugin name
	 * @param DestinationDirectory Base Directory that the template directory should be copied into
	 * @param Source Source directory that the template contents should be copied from
	 * @param PluginName Name of the new plugin that will replace instances of PLUGIN_NAME
	 * @return Whether the copy completed successfully
	 */
	static bool CopyPluginTemplateFolder(const TCHAR* DestinationDirectory, const TCHAR* Source, const FString& PluginName);
};