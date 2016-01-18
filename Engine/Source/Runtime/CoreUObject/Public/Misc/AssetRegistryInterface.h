// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "CoreUObject.h"

namespace EAssetRegistryDependencyType
{
	enum Type
	{
		// Dependencies which don't need to be loaded for the object to be used (i.e. string asset references)
		Soft = 1,

		// Dependencies which are required for correct usage of the source asset, and must be loaded at the same time
		Hard = 2
	};

	static const Type All = (Type)(Soft | Hard);
};

/**
* HotReload module interface
*/
class IAssetRegistryInterface : public IModuleInterface
{
public:
	/**
	 * Tries to gets a pointer to the active AssetRegistryInterface implementation. 
	 */
	static inline IAssetRegistryInterface* GetPtr()
	{
		static FName AssetRegistry("AssetRegistry");
		auto Module = FModuleManager::GetModulePtr<IAssetRegistryInterface>(AssetRegistry);
		if (Module == nullptr)
		{
			Module = &FModuleManager::LoadModuleChecked<IAssetRegistryInterface>(AssetRegistry);
		}
		return reinterpret_cast<IAssetRegistryInterface*>(Module);
	}

	/**
	* Lookup dependencies for the given package name and fill OutDependencies with direct dependencies
	*/
	virtual void GetDependencies(FName InPackageName, TArray<FName>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType = EAssetRegistryDependencyType::All) = 0;
};

