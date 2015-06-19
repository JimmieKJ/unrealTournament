/************************************************************************************

Filename    :   PathUtils.cpp
Content     :
Created     :   November 26, 2014
Authors     :   Warsam Osman, Madhu Kalva

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "PathUtils.h"

#include <stdio.h>
#include "unistd.h"
#include "Android/JniUtils.h"
#include "VrCommon.h"
#include "App.h"

namespace OVR
{
	const char* StorageName[EST_COUNT] =
	{
		"Phone Internal", 	// "/data/data/"
		"Phone External",  	// "/storage/emulated/0" or "/sdcard"
		"SD Card External" 	// "/storage/extSdCard"
	};

	const char* FolderName[EFT_COUNT] =
	{
		"Root",
		"Files",
		"Cache"
	};

	OvrStoragePaths::OvrStoragePaths( JNIEnv * jni, jobject activityObj )
	{
		VrLibClass = ovr_GetGlobalClassReference( jni, "com/oculusvr/vrlib/VrLib" );

		// Internal memory
		jmethodID internalRootDirID = ovr_GetStaticMethodID( jni, VrLibClass, "getInternalStorageRootDir", "()Ljava/lang/String;" );
		if( internalRootDirID )
		{
			JavaUTFChars returnString( jni, (jstring)jni->CallStaticObjectMethod( VrLibClass, internalRootDirID ) );
			StorageFolderPaths[EST_INTERNAL_STORAGE][EFT_ROOT] = returnString.ToStr();
		}

		jmethodID internalFilesDirID = ovr_GetStaticMethodID( jni, VrLibClass, "getInternalStorageFilesDir", "(Landroid/app/Activity;)Ljava/lang/String;" );
		if( internalFilesDirID )
		{
			JavaUTFChars returnString( jni, (jstring)jni->CallStaticObjectMethod( VrLibClass, internalFilesDirID, activityObj ) );
			StorageFolderPaths[EST_INTERNAL_STORAGE][EFT_FILES] = returnString.ToStr();
		}

		jmethodID internalCacheDirID = ovr_GetStaticMethodID( jni, VrLibClass, "getInternalStorageCacheDir", "(Landroid/app/Activity;)Ljava/lang/String;" );
		if( internalCacheDirID )
		{
			JavaUTFChars returnString( jni, (jstring)jni->CallStaticObjectMethod( VrLibClass, internalCacheDirID, activityObj ) );
			StorageFolderPaths[EST_INTERNAL_STORAGE][EFT_CACHE] = returnString.ToStr();
		}

		InternalCacheMemoryID = ovr_GetStaticMethodID( jni, VrLibClass, "getInternalCacheMemoryInBytes", "(Landroid/app/Activity;)J" );

		// Primary external memory
		jmethodID primaryRootDirID = ovr_GetStaticMethodID( jni, VrLibClass, "getPrimaryExternalStorageRootDir", "(Landroid/app/Activity;)Ljava/lang/String;" );
		if( primaryRootDirID )
		{
			JavaUTFChars returnString( jni, (jstring)jni->CallStaticObjectMethod( VrLibClass, primaryRootDirID, activityObj ) );
			StorageFolderPaths[EST_PRIMARY_EXTERNAL_STORAGE][EFT_ROOT] = returnString.ToStr();
		}

		jmethodID primaryFilesDirID = ovr_GetStaticMethodID( jni, VrLibClass, "getPrimaryExternalStorageFilesDir", "(Landroid/app/Activity;)Ljava/lang/String;" );
		if( primaryFilesDirID )
		{
			JavaUTFChars returnString( jni, (jstring)jni->CallStaticObjectMethod( VrLibClass, primaryFilesDirID, activityObj ) );
			StorageFolderPaths[EST_PRIMARY_EXTERNAL_STORAGE][EFT_FILES] = returnString.ToStr();
		}

		jmethodID primaryCacheDirID = ovr_GetStaticMethodID( jni, VrLibClass, "getPrimaryExternalStorageCacheDir", "(Landroid/app/Activity;)Ljava/lang/String;" );
		if( primaryCacheDirID )
		{
			JavaUTFChars returnString( jni, (jstring)jni->CallStaticObjectMethod( VrLibClass, primaryCacheDirID, activityObj ) );
			StorageFolderPaths[EST_PRIMARY_EXTERNAL_STORAGE][EFT_CACHE] = returnString.ToStr();
		}

		// secondary external memory
		jmethodID secondaryRootDirID = ovr_GetStaticMethodID( jni, VrLibClass, "getSecondaryExternalStorageRootDir", "()Ljava/lang/String;" );
		if( secondaryRootDirID )
		{
			JavaUTFChars returnString( jni, (jstring)jni->CallStaticObjectMethod( VrLibClass, secondaryRootDirID ) );
			StorageFolderPaths[EST_SECONDARY_EXTERNAL_STORAGE][EFT_ROOT] = returnString.ToStr();
		}

		jmethodID secondaryFilesDirID = ovr_GetStaticMethodID( jni, VrLibClass, "getSecondaryExternalStorageFilesDir", "(Landroid/app/Activity;)Ljava/lang/String;" );
		if( secondaryFilesDirID )
		{
			JavaUTFChars returnString( jni, (jstring)jni->CallStaticObjectMethod( VrLibClass, secondaryFilesDirID, activityObj ) );
			StorageFolderPaths[EST_SECONDARY_EXTERNAL_STORAGE][EFT_FILES] = returnString.ToStr();
		}

		jmethodID secondaryCacheDirID = ovr_GetStaticMethodID( jni, VrLibClass, "getSecondaryExternalStorageCacheDir", "(Landroid/app/Activity;)Ljava/lang/String;" );
		if( secondaryCacheDirID )
		{
			JavaUTFChars returnString( jni, (jstring)jni->CallStaticObjectMethod( VrLibClass, secondaryCacheDirID, activityObj ) );
			StorageFolderPaths[EST_SECONDARY_EXTERNAL_STORAGE][EFT_CACHE] = returnString.ToStr();
		}
	}

	void OvrStoragePaths::PushBackSearchPathIfValid( EStorageType toStorage, EFolderType toFolder, const char * subfolder, Array<String> & searchPaths ) const
	{
		PushBackSearchPathIfValidPermission( toStorage, toFolder, subfolder, R_OK, searchPaths );
	}

	void OvrStoragePaths::PushBackSearchPathIfValidPermission( EStorageType toStorage, EFolderType toFolder, const char * subfolder, mode_t permission, Array<String> & searchPaths ) const
	{
		String checkPath;
		if ( GetPathIfValidPermission( toStorage, toFolder, subfolder, permission, checkPath ) )
		{
			searchPaths.PushBack( checkPath );
		}
	}

	bool OvrStoragePaths::GetPathIfValidPermission( EStorageType toStorage, EFolderType toFolder, const char * subfolder, mode_t permission, String & outPath ) const
	{
		if ( StorageFolderPaths[ toStorage ][ toFolder ].GetSize() > 0 )
		{
			String checkPath = StorageFolderPaths[ toStorage ][ toFolder ] + subfolder;
			if ( HasPermission( checkPath, permission ) )
			{
				outPath = checkPath;
				return true;
			}
			else
			{
				WARN( "Failed to get permission for %s storage in %s folder ", StorageName[toStorage], FolderName[toFolder] );
			}
		}
		else
		{
			WARN( "Path not found for %s storage in %s folder ", StorageName[toStorage], FolderName[toFolder] );
		}
		return false;
	}

	bool OvrStoragePaths::HasStoragePath( const EStorageType toStorage, const EFolderType toFolder ) const
	{
		return ( StorageFolderPaths[ toStorage ][ toFolder ].GetSize() > 0 );
	}

	long long OvrStoragePaths::GetAvailableInternalMemoryInBytes( JNIEnv * jni, jobject activityObj ) const
	{
		return (long long )( jni->CallStaticLongMethod( VrLibClass, InternalCacheMemoryID, activityObj ) );
	}

	String GetFullPath( const Array<String>& searchPaths, const String & relativePath )
	{
		if ( FileExists( relativePath ) )
		{
			return relativePath;
		}

		const int numSearchPaths = searchPaths.GetSizeI();
		for ( int index = 0; index < numSearchPaths; ++index )
		{
			const String fullPath = searchPaths.At( index ) + String( relativePath );
			if ( FileExists( fullPath ) )
			{
				return fullPath;
			}
		}

		return String();
	}

	bool GetFullPath( const Array<String>& searchPaths, char const * relativePath, char * outPath, const int outMaxLen )
	{
		OVR_ASSERT( outPath != NULL && outMaxLen >= 1 );

		if ( FileExists( relativePath ) )
		{
			OVR_sprintf( outPath, OVR_strlen( relativePath ) + 1, "%s", relativePath );
			return true;
		}

		for ( int i = 0; i < searchPaths.GetSizeI(); ++i )
		{
			OVR_sprintf( outPath, outMaxLen, "%s%s", searchPaths[i].ToCStr(), relativePath );
			if ( FileExists( outPath ) )
			{
				return true;	// outpath is now set to the full path
			}
		}
		// just return the relative path if we never found the file
		OVR_sprintf( outPath, outMaxLen, "%s", relativePath );
		return false;
	}

	bool GetFullPath( const Array<String>& searchPaths, char const * relativePath, String & outPath )
	{
		char largePath[1024];
		bool result = GetFullPath( searchPaths, relativePath, largePath, sizeof( largePath ) );
		if( result )
		{
			outPath = largePath;
		}
		return result;
	}

	bool ToRelativePath( const Array<String>& searchPaths, char const * fullPath, char * outPath, const int outMaxLen )
	{
		// check if the path starts with any of the search paths
		const int n = searchPaths.GetSizeI();
		for ( int i = 0; i < n; ++i )
		{
			char const * path = searchPaths[i].ToCStr();
			if ( strstr( fullPath, path ) == fullPath )
			{
				size_t len = OVR_strlen( path );
				OVR_sprintf( outPath, outMaxLen, "%s", fullPath + len );
				return true;
			}
		}
		OVR_sprintf( outPath, outMaxLen, "%s", fullPath );
		return false;
	}

	bool ToRelativePath( const Array<String>& searchPaths, char const * fullPath, String & outPath )
	{
		char largePath[1024];
		bool result = ToRelativePath( searchPaths, fullPath, largePath, sizeof( largePath ) );
		outPath = largePath;
		return result;
	}

} // namespace OVR
