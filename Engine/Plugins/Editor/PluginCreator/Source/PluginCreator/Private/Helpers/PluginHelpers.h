// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PluginDescriptor.h"

class FPluginHelpers
{
public:
	/** Returns the contents of the specified template file.
	*	@param TemplateFileName - name of the file with fulll path
	*	@param OutFileContents - contents of given file
	*	@param OutFailReason - fail reason text if something goes wrong
	*	@return true if the file was loaded, false otherwise*/
	static bool ReadTemplateFile(const FString& TemplateFileName, FString& OutFileContents, FText& OutFailReason);

	/** Create a .build.cs file for a plugin.
	*	@param NewBuildFileName - filename with full path
	*	@param PluginName - name of the plugin this file belongs to
	*	@param OutFailReason - fail reason text if something goes wrong
	*	@param TemplateType - empty by default; can be Blank, Basic or Advanced
	*	@return true if file was created, false otherwise*/
	static bool CreatePluginBuildFile(const FString& NewBuildFileName, const FString& PluginName, FText& OutFailReason, FString TemplateType = FString(""));

	/** Create plugin header file.
	*	@param FolderPath - filename with full path
	*	@param PluginName - name of the plugin this file belongs to
	*	@param OutFailReason - fail reason text if something goes wrong
	*	@param TemplateType - empty by default; can be Blank, Basic or Advanced
	*	@return true if file was created, false otherwise*/
	static bool CreatePluginHeaderFile(const FString& FolderPath, const FString& PluginName, FText& OutFailReason, FString TemplateType = FString(""));

	/** Create plugin style files (source and header).
	*	@param PrivateFolderPath - Private folder path for source file
	*	@param PublicFolderPath - Public folder path for header file
	*	@param PluginName - name of the plugin this file belongs to
	*	@param OutFailReason - fail reason text if something goes wrong
	*	@param TemplateType - empty by default; can be Blank, Basic or Advanced
	*	@return true if file was created, false otherwise*/
	static bool CreatePluginStyleFiles(const FString& PrivateFolderPath, const FString& PublicFolderPath, const FString& PluginName, FText& OutFailReason, FString TemplateType = FString(""));

	/** Create plugin source file.
	*	@param FolderPath - filename with full path
	*	@param PluginName - name of the plugin this file belongs to
	*	@param OutFailReason - fail reason text if something goes wrong
	*	@param TemplateType - empty by default; can be Blank, Basic or Advanced
	*	@return true if file was created, false otherwise*/
	static bool CreatePluginCPPFile(const FString& FolderPath, const FString& PluginName, FText& OutFailReason, FString TemplateType = FString(""));

	/** Create plugin UI commands file.
	*	@param FolderPath - filename with full path
	*	@param PluginName - name of the plugin this file belongs to
	*	@param OutFailReason - fail reason text if something goes wrong
	*	@param TemplateType - empty by default; can be Blank, Basic or Advanced
	*	@return true if file was created, false otherwise*/
	static bool CreateCommandsFiles(const FString& FolderPath, const FString& PluginName, FText& OutFailReason, FString TemplateType = FString(""));

	/** Create plugin private precompiled header file.
	*	@param FolderPath - filename with full path
	*	@param PluginName - name of the plugin this file belongs to
	*	@param OutFailReason - fail reason text if something goes wrong
	*	@param TemplateType - empty by default; can be Blank, Basic or Advanced
	*	@return true if file was created, false otherwise*/
	static bool CreatePrivatePCHFile(const FString& FolderPath, const FString& PluginName, FText& OutFailReason, FString TemplateType = FString(""));

	/** Save plugin descriptor under given path as .uplugin file using JSon
	*	@param NewProjectFilename - filename with full path
	*	@param PluginDescriptor - descriptor to save
	*	@return true if file was created, false otherwise*/
	static bool SavePluginDescriptor(const FString& NewProjectFilename, const FPluginDescriptor& PluginDescriptor);

};