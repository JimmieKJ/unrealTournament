// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

/** 
 * Interface for DDC utility modules.
 **/
class IDDCUtilsModuleInterface : public IModuleInterface
{
public:
	/**
	 * Returns the path to the shared cache location given its name.
	 **/
	virtual FString GetSharedCachePath(const FString& CacheName) = 0;
};