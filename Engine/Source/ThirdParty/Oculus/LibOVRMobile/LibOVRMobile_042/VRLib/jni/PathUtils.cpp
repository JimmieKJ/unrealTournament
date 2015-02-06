/************************************************************************************

Filename    :   PathUtils.cpp
Content     :
Created     :   November 26, 2014
Authors     :   Warsam Osman, Madhu Kalva

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "PathUtils.h"
#include "VrCommon.h"
#include "App.h"
#include "VrApi/JniUtils.h"
#include <stdio.h>
#include "unistd.h"

namespace OVR
{
	OvrStoragePaths::OvrStoragePaths(JNIEnv * jni, jobject activityObj)
	{
		jclass vrLibClass = ovr_GetGlobalClassReference( jni, "com/oculusvr/vrlib/VrLib" );

		// Internal memory
		jmethodID internalRootDirID = ovr_GetStaticMethodID( jni, vrLibClass, "getInternalStorageRootDir", "()Ljava/lang/String;" );
		if( internalRootDirID )
		{
			jobject returnString = jni->CallStaticObjectMethod( vrLibClass, internalRootDirID );
			StorageFolderPaths[EST_INTERNAL_STORAGE][EFT_ROOT] = jni->GetStringUTFChars( (jstring)returnString, JNI_FALSE );
			jni->DeleteLocalRef(returnString);
		}

		jmethodID internalFilesDirID = ovr_GetStaticMethodID( jni, vrLibClass, "getInternalStorageFilesDir", "(Landroid/app/Activity;)Ljava/lang/String;" );
		if( internalFilesDirID )
		{
			jobject returnString = jni->CallStaticObjectMethod( vrLibClass, internalFilesDirID, activityObj );
			StorageFolderPaths[EST_INTERNAL_STORAGE][EFT_FILES] = jni->GetStringUTFChars( (jstring)returnString, JNI_FALSE );
			jni->DeleteLocalRef(returnString);
		}

		jmethodID internalCacheDirID = ovr_GetStaticMethodID( jni, vrLibClass, "getInternalStorageCacheDir", "(Landroid/app/Activity;)Ljava/lang/String;" );
		if( internalCacheDirID )
		{
			jobject returnString = jni->CallStaticObjectMethod( vrLibClass, internalCacheDirID, activityObj );
			StorageFolderPaths[EST_INTERNAL_STORAGE][EFT_CACHE] = jni->GetStringUTFChars( (jstring)returnString, JNI_FALSE );
			jni->DeleteLocalRef(returnString);
		}

		// Primary external memory
		jmethodID primaryRootDirID = ovr_GetStaticMethodID( jni, vrLibClass, "getPrimaryExternalStorageRootDir", "(Landroid/app/Activity;)Ljava/lang/String;" );
		if( primaryRootDirID )
		{
			jobject returnString = jni->CallStaticObjectMethod( vrLibClass, primaryRootDirID, activityObj );
			StorageFolderPaths[EST_PRIMARY_EXTERNAL_STORAGE][EFT_ROOT] = jni->GetStringUTFChars( (jstring)returnString, JNI_FALSE );
			jni->DeleteLocalRef(returnString);
		}

		jmethodID primaryFilesDirID = ovr_GetStaticMethodID( jni, vrLibClass, "getPrimaryExternalStorageFilesDir", "(Landroid/app/Activity;)Ljava/lang/String;" );
		if( primaryFilesDirID )
		{
			jobject returnString = jni->CallStaticObjectMethod( vrLibClass, primaryFilesDirID, activityObj );
			StorageFolderPaths[EST_PRIMARY_EXTERNAL_STORAGE][EFT_FILES] = jni->GetStringUTFChars( (jstring)returnString, JNI_FALSE );
			jni->DeleteLocalRef(returnString);
		}

		jmethodID primaryCacheDirID = ovr_GetStaticMethodID( jni, vrLibClass, "getPrimaryExternalStorageCacheDir", "(Landroid/app/Activity;)Ljava/lang/String;" );
		if( primaryCacheDirID )
		{
			jobject returnString = jni->CallStaticObjectMethod( vrLibClass, primaryCacheDirID, activityObj );
			StorageFolderPaths[EST_PRIMARY_EXTERNAL_STORAGE][EFT_CACHE] = jni->GetStringUTFChars( (jstring)returnString, JNI_FALSE );
			jni->DeleteLocalRef(returnString);
		}

		// secondary external memory
		jmethodID secondaryRootDirID = ovr_GetStaticMethodID( jni, vrLibClass, "getSecondaryExternalStorageRootDir", "()Ljava/lang/String;" );
		if( secondaryRootDirID )
		{
			jobject returnString = jni->CallStaticObjectMethod( vrLibClass, secondaryRootDirID );
			StorageFolderPaths[EST_SECONDARY_EXTERNAL_STORAGE][EFT_ROOT] = jni->GetStringUTFChars( (jstring)returnString, JNI_FALSE );
			jni->DeleteLocalRef(returnString);
		}

		jmethodID secondaryFilesDirID = ovr_GetStaticMethodID( jni, vrLibClass, "getSecondaryExternalStorageFilesDir", "(Landroid/app/Activity;)Ljava/lang/String;" );
		if( secondaryFilesDirID )
		{
			jobject returnString = jni->CallStaticObjectMethod( vrLibClass, secondaryFilesDirID, activityObj );
			StorageFolderPaths[EST_SECONDARY_EXTERNAL_STORAGE][EFT_FILES] = jni->GetStringUTFChars( (jstring)returnString, JNI_FALSE );
			jni->DeleteLocalRef(returnString);
		}

		jmethodID secondaryCacheDirID = ovr_GetStaticMethodID( jni, vrLibClass, "getSecondaryExternalStorageCacheDir", "(Landroid/app/Activity;)Ljava/lang/String;" );
		if( secondaryCacheDirID )
		{
			jobject returnString = jni->CallStaticObjectMethod( vrLibClass, secondaryCacheDirID, activityObj );
			StorageFolderPaths[EST_SECONDARY_EXTERNAL_STORAGE][EFT_CACHE] = jni->GetStringUTFChars( (jstring)returnString, JNI_FALSE );
			jni->DeleteLocalRef(returnString);
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
		}
		return false;
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
