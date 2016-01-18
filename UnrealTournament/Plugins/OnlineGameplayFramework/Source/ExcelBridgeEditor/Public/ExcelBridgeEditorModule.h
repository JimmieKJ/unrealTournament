// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

class FExcelBridge;

/**
 * Module for Voice capture/compression/decompression implementations
 */
class FExcelBridgeEditorModule : 
	public IModuleInterface, public FSelfRegisteringExec
{
public:
	FExcelBridgeEditorModule();
	~FExcelBridgeEditorModule();

	// FSelfRegisteringExec
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FExcelBridgeEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FExcelBridgeEditorModule>("ExcelBridgeEditor");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("ExcelBridgeEditor");
	}

	/**
	 * Allocate a new Excel bridge instance (loads the helper DLL if necessary). 
	 * Destroy this value using DeleteExcelBridge.
	 *
	 * @return A new ExcelBridge instance or nullptr in case of errors.
	 */
	FExcelBridge* NewExcelBridge();

	/**
	 * Delete counterpart to NewExcelBridge
	 * 
	 * @param Bridge The ExcelBridge to delete (will be null'd).
	 */
	void DeleteExcelBridge(FExcelBridge*& Bridge);

private:

	// IModuleInterface

	/**
	 * Called when module is loaded
	 * Initialize platform specific parts of template handling
	 */
	virtual void StartupModule() override;
	
	/**
	 * Called when module is unloaded
	 * Shutdown platform specific parts of template handling
	 */
	virtual void ShutdownModule() override;

private:
	void* DllHandle;

	typedef FExcelBridge* (*NewExcelBridgeSignature)();
	NewExcelBridgeSignature NewExcelBridgeDllFunc;

	typedef void (*DeleteExcelBridgeSignature)(FExcelBridge* Bridge);
	DeleteExcelBridgeSignature DeleteExcelBridgeDllFunc;
};