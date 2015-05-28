// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlowResult.h"
#include "TypeContainer.h"


/**
 * Structure for package identifiers.
 *
 * Package identifiers are used to uniquely identify a package.
 */
struct FPortalPackageId
{
	/** The application name. */
	FString AppName;

	/** The application build label. */
	FString BuildLabel;
};


/**
 * Interface for package installer services.
 */
class IPortalPackageInstaller
{
public:

	/**
	 * Install the specified package using the given request object.
	 *
	 * @param Path The path at which the package should be installed.
	 * @param Id The identifier of the package to install.
	 * @return The result of the task.
	 * @see UninstallPackage
	 */
	virtual TSlowResult<bool> InstallPackage(const FString& Path, const FPortalPackageId& Id) = 0;

	/**
	 * Attempts to uninstall the specified package using the given request object.
	 *
	 * @param Path The path at which the package is installed.
	 * @param Id The identifier of the package to uninstall.
	 * @param RemoveUserFiles Whether user created files should be removed as well.
	 * @return The result of the task.
	 * @see InstallPackage
	 */
	virtual TSlowResult<bool> UninstallPackage(const FString& Path, const FString& Id, bool RemoveUserFiles) = 0;
};


Expose_TNameOf(IPortalPackageInstaller)
