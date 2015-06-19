// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
				HMODULE APEX_Clothing_GPUHandle = 0;
		#endif  //WITH_APEX_CLOTHING
	#endif	//WITH_APEX
#endif

/**
 *	Load the required modules for PhysX
 */
void LoadPhysXModules()
{

// NOTE: You can change this, along with bShippingBuildsActuallyUseShippingPhysXLibraries in PhysX.Build.cs, to link with
// pure Shipping PhysX binaries if you have those files available.  The performance and memory difference is fairly negligible.
#define SHIPPING_BUILDS_ACTUALLY_USE_SHIPPING_PHYSX_LIBRARIES 0

#if PLATFORM_WINDOWS
	FString PhysXBinariesRoot = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/PhysX/PhysX-3.3/");
	FString APEXBinariesRoot = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/PhysX/APEX-1.3/");

	#if PLATFORM_64BITS

		FString RootPhysXPath(PhysXBinariesRoot + TEXT("Win64/VS2013/"));
		FString RootAPEXPath(APEXBinariesRoot + TEXT("Win64/VS2013/"));


		#if UE_BUILD_DEBUG && !defined(NDEBUG)	// Use !defined(NDEBUG) to check to see if we actually are linking with Debug third party libraries (bDebugBuildsActuallyUseDebugCRT)

			PhysX3CommonHandle = LoadLibraryW(*(RootPhysXPath + "PhysX3CommonDEBUG_x64.dll"));
			nvToolsExtHandle = LoadLibraryW(*(RootPhysXPath + "nvToolsExt64_1.dll"));
			PhysX3Handle = LoadLibraryW(*(RootPhysXPath + "PhysX3DEBUG_x64.dll"));
			#if WITH_PHYSICS_COOKING || WITH_RUNTIME_PHYSICS_COOKING
				PhysX3CookingHandle = LoadLibraryW(*(RootPhysXPath + "PhysX3CookingDEBUG_x64.dll"));
			#endif
			#if WITH_APEX
				APEXFrameworkHandle = LoadLibraryW(*(RootAPEXPath + "APEXFrameworkDEBUG_x64.dll"));
				APEX_DestructibleHandle = LoadLibraryW(*(RootAPEXPath + "APEX_DestructibleDEBUG_x64.dll"));
				APEX_LegacyHandle = LoadLibraryW(*(RootAPEXPath + "APEX_LegacyDEBUG_x64.dll"));
				#if WITH_APEX_CLOTHING
					APEX_ClothingHandle = LoadLibraryW(*(RootAPEXPath + "APEX_ClothingDEBUG_x64.dll"));
					APEX_Clothing_GPUHandle = LoadLibraryW(*(RootAPEXPath + "APEX_ClothingGPUDEBUG_x64.dll"));					
				#endif //WITH_APEX_CLOTHING

			#endif	//WITH_APEX

		#elif (UE_BUILD_SHIPPING || UE_BUILD_TEST) && SHIPPING_BUILDS_ACTUALLY_USE_SHIPPING_PHYSX_LIBRARIES

			PhysX3CommonHandle = LoadLibraryW(*(RootPhysXPath + "PhysX3Common_x64.dll"));
			nvToolsExtHandle = LoadLibraryW(*(RootPhysXPath + "nvToolsExt64_1.dll"));
			PhysX3Handle = LoadLibraryW(*(RootPhysXPath + "PhysX3_x64.dll"));
			#if WITH_PHYSICS_COOKING || WITH_RUNTIME_PHYSICS_COOKING
				PhysX3CookingHandle = LoadLibraryW(*(RootPhysXPath + "PhysX3Cooking_x64.dll"));
			#endif
			#if WITH_APEX
				APEXFrameworkHandle = LoadLibraryW(*(RootAPEXPath + "APEXFramework_x64.dll"));
				APEX_DestructibleHandle = LoadLibraryW(*(RootAPEXPath + "APEX_Destructible_x64.dll"));
				APEX_LegacyHandle = LoadLibraryW(*(RootAPEXPath + "APEX_Legacy_x64.dll"));
				#if WITH_APEX_CLOTHING
					APEX_ClothingHandle = LoadLibraryW(*(RootAPEXPath + "APEX_Clothing_x64.dll"));
					APEX_Clothing_GPUHandle = LoadLibraryW(*(RootAPEXPath + "APEX_ClothingGPU_x64.dll"));					
				#endif //WITH_APEX_CLOTHING
			#endif	//WITH_APEX

		#else	//UE_BUILD_DEBUG
		
			PhysX3CommonHandle = LoadLibraryW(*(RootPhysXPath + "PhysX3CommonPROFILE_x64.dll"));
			nvToolsExtHandle = LoadLibraryW(*(RootPhysXPath + "nvToolsExt64_1.dll"));
			PhysX3Handle = LoadLibraryW(*(RootPhysXPath + "PhysX3PROFILE_x64.dll"));
			#if WITH_PHYSICS_COOKING || WITH_RUNTIME_PHYSICS_COOKING
				PhysX3CookingHandle = LoadLibraryW(*(RootPhysXPath + "PhysX3CookingPROFILE_x64.dll"));
			#endif
			#if WITH_APEX
				APEXFrameworkHandle = LoadLibraryW(*(RootAPEXPath + "APEXFrameworkPROFILE_x64.dll"));
				APEX_DestructibleHandle = LoadLibraryW(*(RootAPEXPath + "APEX_DestructiblePROFILE_x64.dll"));
				APEX_LegacyHandle = LoadLibraryW(*(RootAPEXPath + "APEX_LegacyPROFILE_x64.dll"));
				#if WITH_APEX_CLOTHING
					APEX_ClothingHandle = LoadLibraryW(*(RootAPEXPath + "APEX_ClothingPROFILE_x64.dll"));
					APEX_Clothing_GPUHandle = LoadLibraryW(*(RootAPEXPath + "APEX_ClothingGPUPROFILE_x64.dll"));					
				#endif //WITH_APEX_CLOTHING
			#endif	//WITH_APEX

		#endif	//UE_BUILD_DEBUG
	#else	//PLATFORM_64BITS

		#if _MSC_VER >= 1800
			FString RootPhysXPath(PhysXBinariesRoot + TEXT("Win32/VS2013/"));
			FString RootAPEXPath(APEXBinariesRoot + TEXT("Win32/VS2013/"));
		#else
			FString RootPhysXPath(PhysXBinariesRoot + TEXT("Win32/VS2012/"));
			FString RootAPEXPath(APEXBinariesRoot + TEXT("Win32/VS2012/"));
		#endif

		#if UE_BUILD_DEBUG && !defined(NDEBUG)	// Use !defined(NDEBUG) to check to see if we actually are linking with Debug third party libraries (bDebugBuildsActuallyUseDebugCRT)

			PhysX3CommonHandle = LoadLibraryW(*(RootPhysXPath + "PhysX3CommonDEBUG_x86.dll"));
			nvToolsExtHandle = LoadLibraryW(*(RootPhysXPath + "nvToolsExt32_1.dll"));
			PhysX3Handle = LoadLibraryW(*(RootPhysXPath + "PhysX3DEBUG_x86.dll"));
			#if WITH_PHYSICS_COOKING || WITH_RUNTIME_PHYSICS_COOKING
				PhysX3CookingHandle = LoadLibraryW(*(RootPhysXPath + "PhysX3CookingDEBUG_x86.dll"));
			#endif
			#if WITH_APEX
				APEXFrameworkHandle = LoadLibraryW(*(RootAPEXPath + "APEXFrameworkDEBUG_x86.dll"));
				APEX_DestructibleHandle = LoadLibraryW(*(RootAPEXPath + "APEX_DestructibleDEBUG_x86.dll"));
				APEX_LegacyHandle = LoadLibraryW(*(RootAPEXPath + "APEX_LegacyDEBUG_x86.dll"));
				#if WITH_APEX_CLOTHING
					APEX_ClothingHandle = LoadLibraryW(*(RootAPEXPath + "APEX_ClothingDEBUG_x86.dll"));
					APEX_Clothing_GPUHandle = LoadLibraryW(*(RootAPEXPath + "APEX_ClothingGPUDEBUG_x86.dll"));
				#endif //WITH_APEX_CLOTHING
			#endif	//WITH_APEX

		#elif (UE_BUILD_SHIPPING || UE_BUILD_TEST) && SHIPPING_BUILDS_ACTUALLY_USE_SHIPPING_PHYSX_LIBRARIES

			PhysX3CommonHandle = LoadLibraryW(*(RootPhysXPath + "PhysX3Common_x86.dll"));
			nvToolsExtHandle = LoadLibraryW(*(RootPhysXPath + "nvToolsExt32_1.dll"));
			PhysX3Handle = LoadLibraryW(*(RootPhysXPath + "PhysX3_x86.dll"));
			#if WITH_PHYSICS_COOKING || WITH_RUNTIME_PHYSICS_COOKING
				PhysX3CookingHandle = LoadLibraryW(*(RootPhysXPath + "PhysX3Cooking_x86.dll"));
			#endif
			#if WITH_APEX
				APEXFrameworkHandle = LoadLibraryW(*(RootAPEXPath + "APEXFramework_x86.dll"));
				APEX_DestructibleHandle = LoadLibraryW(*(RootAPEXPath + "APEX_Destructible_x86.dll"));
				APEX_LegacyHandle = LoadLibraryW(*(RootAPEXPath + "APEX_Legacy_x86.dll"));
				#if WITH_APEX_CLOTHING
					APEX_ClothingHandle = LoadLibraryW(*(RootAPEXPath + "APEX_Clothing_x86.dll"));
					APEX_Clothing_GPUHandle = LoadLibraryW(*(RootAPEXPath + "APEX_ClothingGPU_x86.dll"));
				#endif //WITH_APEX_CLOTHING
			#endif	//WITH_APEX

		#else	//UE_BUILD_DEBUG

			PhysX3CommonHandle = LoadLibraryW(*(RootPhysXPath + "PhysX3CommonPROFILE_x86.dll"));
			nvToolsExtHandle = LoadLibraryW(*(RootPhysXPath + "nvToolsExt32_1.dll"));
			PhysX3Handle = LoadLibraryW(*(RootPhysXPath + "PhysX3PROFILE_x86.dll"));
			#if WITH_PHYSICS_COOKING || WITH_RUNTIME_PHYSICS_COOKING
				PhysX3CookingHandle = LoadLibraryW(*(RootPhysXPath + "PhysX3CookingPROFILE_x86.dll"));
			#endif
			#if WITH_APEX
					APEXFrameworkHandle = LoadLibraryW(*(RootAPEXPath + "APEXFrameworkPROFILE_x86.dll"));
					APEX_DestructibleHandle = LoadLibraryW(*(RootAPEXPath + "APEX_DestructiblePROFILE_x86.dll"));
					APEX_LegacyHandle = LoadLibraryW(*(RootAPEXPath + "APEX_LegacyPROFILE_x86.dll"));
					#if WITH_APEX_CLOTHING
						APEX_ClothingHandle = LoadLibraryW(*(RootAPEXPath + "APEX_ClothingPROFILE_x86.dll"));
						APEX_Clothing_GPUHandle = LoadLibraryW(*(RootAPEXPath + "APEX_ClothingGPUPROFILE_x86.dll"));
					#endif //WITH_APEX_CLOTHING
			#endif	//WITH_APEX

		#endif	//UE_BUILD_DEBUG
	#endif	//PLATFORM_64BITS
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
			FreeLibrary(APEX_Clothing_GPUHandle);
		#endif //WITH_APEX_CLOTHING
	#endif	//WITH_APEX
#endif
}

#endif // WITH_PHYSX
