// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	GenericPlatformFile.h: Generic platform file interfaces
==============================================================================================*/

#pragma once
#include "HAL/Platform.h"

class FArchive;
class FString;
struct FDateTime;

/** 
 * File handle interface. 
**/
class CORE_API IFileHandle
{
public:
	/** Destructor, also the only way to close the file handle **/
	virtual ~IFileHandle()
	{
	}

	/** Return the current write or read position. **/
	virtual int64		Tell() = 0;
	/** 
	 * Change the current write or read position. 
	 * @param NewPosition	new write or read position
	 * @return				true if the operation completed successfully.
	**/
	virtual bool		Seek(int64 NewPosition) = 0;

	/** 
	 * Change the current write or read position, relative to the end of the file.
	 * @param NewPositionRelativeToEnd	new write or read position, relative to the end of the file should be <=0!
	 * @return							true if the operation completed successfully.
	**/
	virtual bool		SeekFromEnd(int64 NewPositionRelativeToEnd = 0) = 0;

	/** 
	 * Read bytes from the file.
	 * @param Destination	Buffer to holds the results, should be at least BytesToRead in size.
	 * @param BytesToRead	Number of bytes to read into the destination.
	 * @return				true if the operation completed successfully.
	**/
	virtual bool		Read(uint8* Destination, int64 BytesToRead) = 0;

	/** 
	 * Write bytes to the file.
	 * @param Source		Buffer to write, should be at least BytesToWrite in size.
	 * @param BytesToWrite	Number of bytes to write.
	 * @return				true if the operation completed successfully.
	**/
	virtual bool		Write(const uint8* Source, int64 BytesToWrite) = 0;

public:
	/////////// Utility Functions. These have a default implementation that uses the pure virtual operations.

	/** Return the total size of the file **/
	virtual int64		Size();
};


/**
* File I/O Interface
**/
class CORE_API IPlatformFile
{
public:
	/** Physical file system of the _platform_, never wrapped. **/
	static IPlatformFile& GetPlatformPhysical();
	/** Returns the name of the physical platform file type. */
	static const TCHAR* GetPhysicalTypeName();
	/** Destructor. */
	virtual ~IPlatformFile() {}

	/**
	 *	Set whether the sandbox is enabled or not
	 *
	 *	@param	bInEnabled		true to enable the sandbox, false to disable it
	 */
	virtual void SetSandboxEnabled(bool bInEnabled)
	{
	}

	/**
	 *	Returns whether the sandbox is enabled or not
	 *
	 *	@return	bool			true if enabled, false if not
	 */
	virtual bool IsSandboxEnabled() const
	{
		return false;
	}

	/**
	 * Checks if this platform file should be used even though it was not asked to be.
	 * i.e. pak files exist on disk so we should use a pak file
	 */
	virtual bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const
	{
		return false;
	}

	/**
	 * Initializes platform file.
	 *
	 * @param Inner Platform file to wrap by this file.
	 * @param CmdLine Command line to parse.
	 * @return true if the initialization was successful, false otherise. */
	virtual bool		Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) = 0;

	/**
	 * Performs initialization of the platform file after it has become the active (FPlatformFileManager.GetPlatformFile() will return this
	 */
	virtual void		InitializeAfterSetActive() { }

	/**
	 * Identifies any platform specific paths that are guaranteed to be local (i.e. cache, scratch space)
	 */
	virtual void		AddLocalDirectories(TArray<FString> &LocalDirectories) { }

	/** Gets the platform file wrapped by this file. */
	virtual IPlatformFile* GetLowerLevel() = 0;
		/** Gets this platform file type name. */
	virtual const TCHAR* GetName() const = 0;
	/** Return true if the file exists. **/
	virtual bool		FileExists(const TCHAR* Filename) = 0;
	/** Return the size of the file, or -1 if it doesn't exist. **/
	virtual int64		FileSize(const TCHAR* Filename) = 0;
	/** Delete a file and return true if the file exists. Will not delete read only files. **/
	virtual bool		DeleteFile(const TCHAR* Filename) = 0;
	/** Return true if the file is read only. **/
	virtual bool		IsReadOnly(const TCHAR* Filename) = 0;
	/** Attempt to move a file. Return true if successful. Will not overwrite existing files. **/
	virtual bool		MoveFile(const TCHAR* To, const TCHAR* From) = 0;
	/** Attempt to change the read only status of a file. Return true if successful. **/
	virtual bool		SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) = 0;
	/** Return the modification time of a file. Returns FDateTime::MinValue() on failure **/
	virtual FDateTime	GetTimeStamp(const TCHAR* Filename) = 0;
	/** Sets the modification time of a file **/
	virtual void		SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) = 0;
	/** Return the last access time of a file. Returns FDateTime::MinValue() on failure **/
	virtual FDateTime	GetAccessTimeStamp(const TCHAR* Filename) = 0;
	/** For case insensitive filesystems, returns the full path of the file with the same case as in the filesystem */
	virtual FString GetFilenameOnDisk(const TCHAR* Filename) = 0;

	/** Attempt to open a file for reading.
	 *
	 * @param Filename file to be opened
	 * @param bAllowWrite (applies to certain platforms only) whether this file is allowed to be written to by other processes. This flag is needed to open files that are currently being written to as well.
	 *
	 * @return If successful will return a non-nullptr pointer. Close the file by delete'ing the handle.
	 */
	virtual IFileHandle*	OpenRead(const TCHAR* Filename, bool bAllowWrite = false) = 0;
	/** Attempt to open a file for writing. If successful will return a non-nullptr pointer. Close the file by delete'ing the handle. **/
	virtual IFileHandle*	OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) = 0;

	/** Return true if the directory exists. **/
	virtual bool		DirectoryExists(const TCHAR* Directory) = 0;
	/** Create a directory and return true if the directory was created or already existed. **/
	virtual bool		CreateDirectory(const TCHAR* Directory) = 0;
	/** Delete a directory and return true if the directory was deleted or otherwise does not exist. **/
	virtual bool		DeleteDirectory(const TCHAR* Directory) = 0;

	/** Base class for file and directory visitors. **/
	class FDirectoryVisitor
	{
	public:
		/** 
		 * Callback for a single file or a directory in a directory iteration.
		 * @param FilenameOrDirectory		If bIsDirectory is true, this is a directory (with no trailing path delimiter), otherwise it is a file name.
		 * @param bIsDirectory				true if FilenameOrDirectory is a directory.
		 * @return							true if the iteration should continue.
		**/
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) = 0;
	};
	/** 
	 * Call the Visit function of the visitor once for each file or directory in a single directory. This function does not explore subdirectories.
	 * @param Directory		The directory to iterate the contents of.
	 * @param Visitor		Visitor to call for each element of the directory
	 * @return				false if the directory did not exist or if the visitor returned false.
	**/
	virtual bool		IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) = 0;


	/////////////////////////////////////////////////////////////////////////////////////////////////////////
	/////////// Utility Functions. These have a default implementation that uses the pure virtual operations.
	/////////// Generally, these do not need to be implemented per platform.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////

	/** 
	 * Call the Visit function of the visitor once for each file or directory in a directory tree. This function explores subdirectories.
	 * @param Directory		The directory to iterate the contents of, recursively.
	 * @param Visitor		Visitor to call for each element of the directory and each element of all subdirectories.
	 * @return				false if the directory did not exist or if the visitor returned false.
	**/
	virtual bool IterateDirectoryRecursively(const TCHAR* Directory, FDirectoryVisitor& Visitor);

	/** 
	 * Delete all files and subdirectories in a directory, then delete the directory itself
	 * @param Directory		The directory to delete.
	 * @return				true if the directory was deleted or did not exist.
	**/
	virtual bool DeleteDirectoryRecursively(const TCHAR* Directory);

	/** Create a directory, including any parent directories and return true if the directory was created or already existed. **/
	virtual bool CreateDirectoryTree(const TCHAR* Directory);

	/** 
	 * Copy a file. This will fail if the destination file already exists.
	 * @param To		File to copy to.
	 * @param From		File to copy from.
	 * @return			true if the file was copied sucessfully.
	**/
	virtual bool CopyFile(const TCHAR* To, const TCHAR* From);

	/** 
	 * Copy a file or a hierarchy of files (directory).
	 * @param DestinationDirectory			Target path (either absolute or relative) to copy to - always a directory! (e.g. "/home/dest/").
	 * @param Source						Source file (or directory) to copy (e.g. "/home/source/stuff").
	 * @param bOverwriteAllExisting			Whether to overwrite everything that exists at target
	 * @return								true if operation completed successfully.
	 */
	virtual bool CopyDirectoryTree(const TCHAR* DestinationDirectory, const TCHAR* Source, bool bOverwriteAllExisting);

	/**
	 * Converts passed in filename to use an absolute path (for reading).
	 *
	 * @param	Filename	filename to convert to use an absolute path, safe to pass in already using absolute path
	 * 
	 * @return	filename using absolute path
	 */
	virtual FString ConvertToAbsolutePathForExternalAppForRead( const TCHAR* Filename );

	/**
	 * Converts passed in filename to use an absolute path (for writing)
	 *
	 * @param	Filename	filename to convert to use an absolute path, safe to pass in already using absolute path
	 * 
	 * @return	filename using absolute path
	 */
	virtual FString ConvertToAbsolutePathForExternalAppForWrite( const TCHAR* Filename );

	/**
	 * Helper class to send/receive data to the file server function
	 */
	class IFileServerMessageHandler
	{
	public:
		/** Subclass fills out an archive to send to the server */
		virtual void FillPayload(FArchive& Payload) = 0;

		/** Subclass pulls data response from the server */
		virtual void ProcessResponse(FArchive& Response) = 0;
	};

	/**
	 * Sends a message to the file server, and will block until it's complete. Will return 
	 * immediately if the file manager doesn't support talking to a server.
	 *
	 * @param Message	The string message to send to the server
	 *
	 * @return			true if the message was sent to server and it returned success, or false if there is no server, or the command failed
	 */
	virtual bool SendMessageToServer(const TCHAR* Message, IFileServerMessageHandler* Handler)
	{
		// by default, IPlatformFile's can't talk to a server
		return false;
	}
};

/**
* Common base for physical platform File I/O Interface
**/
class CORE_API IPhysicalPlatformFile : public IPlatformFile
{
public:
	// BEGIN IPlatformFile Interface
	virtual bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const override
	{
		return true;
	}
	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) override;
	virtual IPlatformFile* GetLowerLevel() override
	{
		return nullptr;
	}
	virtual const TCHAR* GetName() const override
	{
		return IPlatformFile::GetPhysicalTypeName();
	}
	// END IPlatformFile Interface
};
