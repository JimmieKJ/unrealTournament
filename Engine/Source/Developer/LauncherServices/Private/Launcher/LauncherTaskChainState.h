// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Structure for passing launcher task chain state data.
 */
struct FLauncherTaskChainState
{
	/** The path to the binaries directory (for deployment). */
	FString BinariesDirectory;

	/** The path to the content directory (for deployment). */
	FString ContentDirectory;

	/** The identifier of the deployed application. */
	FString DeployedAppId;

	/** The list of IP addresses and port numbers at which the file server is reachable. */
	FString FileServerAddressListString;

	/** A pointer to the launcher profile. */
	ILauncherProfilePtr Profile;

	/** The session identifier. */
	FGuid SessionId;
};
