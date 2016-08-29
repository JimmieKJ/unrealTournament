// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysXLibs.cpp: PhysX library imports
=============================================================================*/

#include "EnginePrivate.h"
#include "PhysicsPublic.h"

#if WITH_PHYSX

// PhysX library imports
#include "PhysXSupport.h"

#if PLATFORM_WINDOWS
	HMODULE PhysX3CommonHandle = 0;
	HMODULE	PhysX3Handle = 0;
	#if WITH_PHYSICS_COOKING || WITH_RUNTIME_PHYSICS_COOKING
		HMODULE	PhysX3CookingHandle = 0;
	#endif
	HMODULE	nvToolsExtHandle = 0;
	#if WITH_APEX
			HMODULE	APEXFrameworkHandle = 0;
			HMODULE	APEX_DestructibleHandle = 0;
			HMODULE	APEX_LegacyHandle = 0;
		#if WITH_APEX_CLOTHING
				HMODULE	APEX_ClothingHandle = 0;
		#endif  //WITH_APEX_CLOTHING
	#endif	//WITH_APEX
#endif

/**
 *	Load the required modules for PhysX
 */
void LoadPhysXModules()
{


#if PLATFORM_WINDOWS
	FString PhysXBinariesRoot = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/PhysX/PhysX-3.3/");
	FString APEXBinariesRoot = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/PhysX/APEX-1.3/");

	#if _MSC_VER >= 1900
		FString VSDirectory(TEXT("VS2015/"));
	#elif _MSC_VER >= 1800
		FString VSDirectory(TEXT("VS2013/"));
	#else
		#error "Unrecognized Visual Studio version."
	#endif

	#if PLATFORM_64BITS
		FString RootPhysXPath(PhysXBinariesRoot + TEXT("Win64/") + VSDirectory);
		FString RootAPEXPath(APEXBinariesRoot + TEXT("Win64/") + VSDirectory);
		FString ArchName(TEXT("_x64"));
		FString ArchBits(TEXT("64"));
	#else
		FString RootPhysXPath(PhysXBinariesRoot + TEXT("Win32/") + VSDirectory);
		FString RootAPEXPath(APEXBinariesRoot + TEXT("Win32/") + VSDirectory);
		FString ArchName(TEXT("_x86"));
		FString ArchBits(TEXT("32"));
	#endif

#ifdef UE_PHYSX_SUFFIX
	FString PhysXSuffix(TEXT(PREPROCESSOR_TO_STRING(UE_PHYSX_SUFFIX)) + ArchName + TEXT(".dll"));
#else
	FString PhysXSuffix(ArchName + TEXT(".dll"));
#endif

#ifdef UE_APEX_SUFFIX
	FString APEXSuffix(TEXT(PREPROCESSOR_TO_STRING(UE_APEX_SUFFIX)) + ArchName + TEXT(".dll"));
#else
	FString APEXSuffix(ArchName + TEXT(".dll"));
#endif

	auto LoadPhysicsLibrary([](const FString& Path) -> HMODULE
	{
		HMODULE Handle = LoadLibraryW(*Path);
		if (Handle == nullptr)
		{
			UE_LOG(LogPhysics, Fatal, TEXT("Failed to load module '%s'."), *Path);
		}
		return Handle;
	});

	PhysX3CommonHandle = LoadPhysicsLibrary(RootPhysXPath + "PhysX3Common" + PhysXSuffix);
	nvToolsExtHandle = LoadPhysicsLibrary(RootPhysXPath + "nvToolsExt" + ArchBits + "_1.dll");
	PhysX3Handle = LoadPhysicsLibrary(RootPhysXPath + "PhysX3" + PhysXSuffix);
	#if WITH_PHYSICS_COOKING || WITH_RUNTIME_PHYSICS_COOKING
		PhysX3CookingHandle = LoadPhysicsLibrary(RootPhysXPath + "PhysX3Cooking" + PhysXSuffix);
	#endif
	#if WITH_APEX
		APEXFrameworkHandle = LoadPhysicsLibrary(RootAPEXPath + "APEXFramework" + APEXSuffix);
		APEX_DestructibleHandle = LoadPhysicsLibrary(RootAPEXPath + "APEX_Destructible" + APEXSuffix);
		#if WITH_APEX_LEGACY
			APEX_LegacyHandle = LoadPhysicsLibrary(RootAPEXPath + "APEX_Legacy" + APEXSuffix);
		#endif //WITH_APEX_LEGACY
		#if WITH_APEX_CLOTHING
			APEX_ClothingHandle = LoadPhysicsLibrary(RootAPEXPath + "APEX_Clothing" + APEXSuffix);
		#endif //WITH_APEX_CLOTHING
	#endif	//WITH_APEX
#endif	//PLATFORM_WINDOWS
}

/** 
 *	Unload the required modules for PhysX
 */
void UnloadPhysXModules()
{
#if PLATFORM_WINDOWS
	FreeLibrary(PhysX3Handle);
	#if WITH_PHYSICS_COOKING || WITH_RUNTIME_PHYSICS_COOKING
		FreeLibrary(PhysX3CookingHandle);
	#endif
	FreeLibrary(PhysX3CommonHandle);
	#if WITH_APEX
		FreeLibrary(APEXFrameworkHandle);
		FreeLibrary(APEX_DestructibleHandle);
		FreeLibrary(APEX_LegacyHandle);
		#if WITH_APEX_CLOTHING
			FreeLibrary(APEX_ClothingHandle);
		#endif //WITH_APEX_CLOTHING
	#endif	//WITH_APEX
#endif
}

#endif // WITH_PHYSX

