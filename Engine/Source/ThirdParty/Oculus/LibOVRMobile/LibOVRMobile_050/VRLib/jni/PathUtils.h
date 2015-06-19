/************************************************************************************

Filename    :   PathUtils.h
Content     :
Created     :   November 26, 2014
Authors     :   Warsam Osman, Madhu Kalva

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/
#ifndef OVR_PathUtils_h
#define OVR_PathUtils_h

#include <jni.h>
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_Array.h"

namespace OVR
{
	class App;

	enum EStorageType
	{
		// By default data here is private and other apps shouldn't be able to access data from here
		// Path => "/data/data/", in Note 4 this is 24.67GB
		EST_INTERNAL_STORAGE = 0,

		// Also known as emulated internal storage, as this is part of phone memory( that can't be removed ) which is emulated as external storage
		// in Note 4 this is = 24.64GB, with WRITE_EXTERNAL_STORAGE permission can write anywhere in this storage
		// Path => "/storage/emulated/0" or "/sdcard",
		EST_PRIMARY_EXTERNAL_STORAGE,

		// Path => "/storage/extSdCard"
		// Can only write to app specific folder - /storage/extSdCard/Android/obb/<app>
		EST_SECONDARY_EXTERNAL_STORAGE,

		EST_COUNT
	};

	enum EFolderType
	{
		// Root folder, for example:
		//		internal 			=> "/data"
		//		primary external 	=> "/storage/emulated/0"
		//		secondary external 	=> "/storage/extSdCard"
		EFT_ROOT = 0,

		// Files folder
		EFT_FILES,

		// Cache folder, data in this folder can be flushed by OS when it needs more memory.
		EFT_CACHE,

		EFT_COUNT
	};

	class OvrStoragePaths
	{
	public:
					OvrStoragePaths( JNIEnv * jni, jobject activityObj );
		void		PushBackSearchPathIfValid( EStorageType toStorage, EFolderType toFolder, const char * subfolder, Array<String> & searchPaths ) const;
		void		PushBackSearchPathIfValidPermission( EStorageType toStorage, EFolderType toFolder, const char * subfolder, mode_t permission, Array<String> & searchPaths ) const;
		bool		GetPathIfValidPermission( EStorageType toStorage, EFolderType toFolder, const char * subfolder, mode_t permission, String & outPath ) const;
		bool		HasStoragePath( const EStorageType toStorage, const EFolderType toFolder ) const;
		long long 	GetAvailableInternalMemoryInBytes( JNIEnv * jni, jobject activityObj ) const;

	private:
		// contains all the folder paths for future reference
		String 			StorageFolderPaths[EST_COUNT][EFT_COUNT];
		jclass			VrLibClass;
		jmethodID		InternalCacheMemoryID;
	};

	String	GetFullPath		( const Array< String > & searchPaths, const String & relativePath );

	// Return false if it fails to find the relativePath in any of the search locations
	bool	GetFullPath		( const Array< String > & searchPaths, char const * relativePath, 	char * outPath, 	const int outMaxLen );
	bool	GetFullPath		( const Array< String > & searchPaths, char const * relativePath, 	String & outPath 						);

	bool	ToRelativePath	( const Array< String > & searchPaths, char const * fullPath, 		char * outPath, 	const int outMaxLen );
	bool	ToRelativePath	( const Array< String > & searchPaths, char const * fullPath, 		String & outPath 						);

} // namespace OVR

#endif // OVR_PathUtils_h

