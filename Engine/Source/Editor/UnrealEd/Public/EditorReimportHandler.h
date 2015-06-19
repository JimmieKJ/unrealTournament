// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

class FReimportHandler;

/** Reimport manager for package resources with associated source files on disk. */
class FReimportManager
{
public:
	/**
	 * Singleton function, provides access to the only instance of the class
	 *
	 * @return	Singleton instance of the manager
	 */
	UNREALED_API static FReimportManager* Instance();

	/**
	 * Register a reimport handler with the manager
	 *
	 * @param	InHandler	Handler to register with the manager
	 */
	UNREALED_API void RegisterHandler( FReimportHandler& InHandler );

	/**
	 * Unregister a reimport handler from the manager
	 *
	 * @param	InHandler	Handler to unregister from the manager
	 */
	UNREALED_API void UnregisterHandler( FReimportHandler& InHandler );

	/**
	 * Check to see if we have a handler to manage the reimporting of the object
	 *
	 * @param	Obj	Object we want to reimport
	 *
	 * @return	true if the object is capable of reimportatio
	 */
	UNREALED_API virtual bool CanReimport( UObject* Obj ) const;

	/**
	 * Attempt to reimport the specified object from its source by giving registered reimport
	 * handlers a chance to try to reimport the object
	 *
	 * @param	Obj	Object to try reimporting
	 * @param	bAskForNewFileIfMissing If the file is missing, open a dialog to ask for a new one
	 * @param	bShowNotification True to show a notification when complete, false otherwise
	 *
	 * @return	true if the object was handled by one of the reimport handlers; false otherwise
	 */
	UNREALED_API virtual bool Reimport( UObject* Obj, bool bAskForNewFileIfMissing = false, bool bShowNotification = true );

	/**
	 * Update the reimport paths for the specified object
	 *
	 * @param	Obj	Object to update
	 * @param	InFilenames The files we want to set to its import paths
	 */
	UNREALED_API virtual void UpdateReimportPaths( UObject* Obj, const TArray<FString>& InFilenames );

	/**
	 * Convert a file path to be relative to the specified object, if it resides in the same package folder
	 * 
	 * @param	InPath Absolute (or relative by cwd) path to the source file
	 * @param	Obj Object file to make InPath relative to
	 *
	 * @return	A path relative to the specified object, BaseDir() or failing that, an absolute path
	 */
	UNREALED_API static FString SanitizeImportFilename(const FString& InPath, const UObject* Obj);

	/**
	 * Take an import filename and resolve it to an absolute filename on disk
	 * 
	 * @param	InRelativePath Path relative to the specified file or BaseDir(), or an absolute path
	 * @param	Obj Object file to make InPath relative to
	 *
	 * @return	An absolute path to the import file
	 */
	UNREALED_API static FString ResolveImportFilename(const FString& InRelativePath, const UObject* Obj);

	/**
	 * Gets the delegate that's fired prior to reimporting an asset
	 *
	 * @return The event delegate.
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FPreReimportNotification, UObject*);
	FPreReimportNotification& OnPreReimport(){ return PreReimport; }

	/**
	 * Gets the delegate that's fired after to reimporting an asset
	 *
	 * @param Whether the reimport was a success
	 * @return The event delegate.
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FPostReimportNotification, UObject*, bool);
	FPostReimportNotification& OnPostReimport(){ return PostReimport; }

private:
	/** Opens a file dialog to request a new reimport path */
	void GetNewReimportPath(UObject* Obj, TArray<FString>& InOutFilenames);

private:
	/** Reimport handlers registered with this manager */
	TArray<FReimportHandler*> Handlers;

	/** True when the Handlers array has been modified such that it needs sorting */
	bool bHandlersNeedSorting;

	/** Delegate to call before the asset is reimported */
	FPreReimportNotification PreReimport;

	/** Delegate to call after the asset is reimported */
	FPostReimportNotification PostReimport;

	/** Constructor */
	FReimportManager();

	/** Destructor */
	~FReimportManager();

	/** Copy constructor; intentionally left unimplemented */
	FReimportManager( const FReimportManager& );

	/** Assignment operator; intentionally left unimplemented */
	FReimportManager& operator=( const FReimportManager& );
};

/** 
* The various results we can receive from an object re-import
*/
namespace EReimportResult
{
	enum Type
	{
		Failed,
		Succeeded,
		Cancelled
	};
}

/** 
* Reimport handler for package resources with associated source files on disk.
*/
class FReimportHandler
{
public:
	/** Constructor. Add self to manager */
	FReimportHandler(){ FReimportManager::Instance()->RegisterHandler( *this ); }
	/** Destructor. Remove self from manager */
	virtual ~FReimportHandler(){ FReimportManager::Instance()->UnregisterHandler( *this ); }
	
	/**
	 * Check to see if the handler is capable of reimporting the object
	 *
	 * @param	Obj	Object to attempt to reimport
	 * @param	OutFilenames	The filename(s) of the source art for the specified object
	 *
	 * @return	true if this handler is capable of reimporting the provided object
	 */
	virtual bool CanReimport( UObject* Obj, TArray<FString>& OutFilenames ) = 0;

	/**
	 * Sets the reimport path(s) for the specified object
	 *
	 * @param	Obj	Object for which to change the reimport path(s)
	 * @param	NewReimportPaths	The new path(s) to set on the object
	 */
	virtual void SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths ) = 0;

	/**
	 * Attempt to reimport the specified object from its source
	 *
	 * @param	Obj	Object to attempt to reimport
	 *
	 * @return	EReimportResult::Succeeded if this handler was able to handle reimporting the provided object,
	 *			EReimportResult::Failed if this handler was unable to handle reimporting the provided object or
	 *			EReimportResult::Cancelled if the handler was cancelled part-way through re-importing the provided object.
	 */
	virtual EReimportResult::Type Reimport( UObject* Obj ) = 0;

	/**
	 * Get the import priority for this handler.
	 * Import handlers with higher priority values will take precedent over lower priorities.
	 */
	UNREALED_API virtual int32 GetPriority() const;
};
