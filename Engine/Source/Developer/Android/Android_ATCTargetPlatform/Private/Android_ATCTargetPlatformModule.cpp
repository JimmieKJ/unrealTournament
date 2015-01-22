// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidATC_TargetPlatformModule.cpp: Implements the FAndroidATC_TargetPlatformModule class.
=============================================================================*/

#include "Android_ATCTargetPlatformPrivatePCH.h"

#define LOCTEXT_NAMESPACE "FAndroid_ATCTargetPlatformModule" 


/**
 * Android cooking platform which cooks only ATC based textures.
 */
class FAndroid_ATCTargetPlatform
	: public FAndroidTargetPlatform<FAndroid_ATCPlatformProperties>
{
	virtual FString GetAndroidVariantName( ) override
	{
		return TEXT("ATC");
	}

	virtual FText DisplayName( ) const override
	{
		return LOCTEXT("Android_ATC", "Android (ATC)");
	}

	virtual FString PlatformName() const override
	{
		return FString(FAndroid_ATCPlatformProperties::PlatformName());
	}

	virtual bool SupportsTextureFormat( FName Format ) const override
	{
		if( Format == AndroidTexFormat::NameATC_RGB ||
			Format == AndroidTexFormat::NameATC_RGBA_I ||
			Format == AndroidTexFormat::NameAutoATC )
		{
			return true;
		}
		return false;
	}

	virtual bool SupportedByExtensionsString( const FString& ExtensionsString, const int GLESVersion ) const override
	{
		return (ExtensionsString.Contains(TEXT("GL_ATI_texture_compression_atitc")) || ExtensionsString.Contains(TEXT("GL_AMD_compressed_ATC_texture")));
	}

	virtual FText GetVariantDisplayName() const override
	{
		return LOCTEXT("Android_ATC_ShortName", "ATC");
	}

	virtual float GetVariantPriority() const override
	{
		return 0.5f;
	}
};


/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* AndroidTargetSingleton = NULL;


/**
 * Module for the Android target platform.
 */
class FAndroid_ATCTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FAndroid_ATCTargetPlatformModule( )
	{
		AndroidTargetSingleton = NULL;
	}


public:
	
	// Begin ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform() override
	{
		if (AndroidTargetSingleton == NULL)
		{
			AndroidTargetSingleton = new FAndroid_ATCTargetPlatform();
		}
		
		return AndroidTargetSingleton;
	}

	// End ITargetPlatformModule interface
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FAndroid_ATCTargetPlatformModule, Android_ATCTargetPlatform);
