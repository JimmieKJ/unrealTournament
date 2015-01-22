// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Helper struct defining parameters we need to establish a connection */
struct FPerforceConnectionInfo
{
	/** The host and port for your Perforce server. Usage ServerName:1234 */
	FString Port;

	/** Perforce username */
	FString UserName;

	/** Perforce workspace */
	FString Workspace;

	/** Perforce host override - not usually set */
	FString HostOverride;

	/** Ticket if in use */
	FString Ticket;

	/** Password if in use */
	FString Password;
};