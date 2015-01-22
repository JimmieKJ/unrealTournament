// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "CoreUObject.h"

/**
 * Enum describing P4 param type.
 */
namespace EP4ParamType
{
	enum Type
	{
		Path	= 0,
		Port	= 1,
		User	= 2,
		Client	= 3,
		Branch	= 4
	};
}

/**
 * P4 environment class to connect with P4 executable.
 */
class FP4Env
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnP4MadeProgress, const FString&)

	/**
	 * Initializes P4 environment from command line.
	 *
	 * @param CommandLine Command line to initialize from.
	 *
	 * @returns True if succeeded. False otherwise.
	 */
	static bool Init(const TCHAR* CommandLine);

	/**
	 * Initializes P4 environment.
	 *
	 * @param Port P4 port to connect to.
	 * @param User P4 user to connect.
	 * @param Client P4 workspace name.
	 *
	 * @returns True if succeeded. False otherwise.
	 */
	static bool Init(const FString& Port, const FString& User, const FString& Client);



	/**
	 * Gets instance of the P4 environment.
	 *
	 * @returns P4 environment object.
	 */
	static FP4Env& Get();

	/**
	 * Runs P4 process and allows communication through progress delegate.
	 *
	 * @param CommandLine Command line to run P4 process with.
	 * @param OnP4MadeProgress Progress delegate function.
	 *
	 * @returns True if succeeded. False otherwise.
	 */
	static bool RunP4Progress(const FString& CommandLine, const FOnP4MadeProgress& OnP4MadeProgress);

	/**
	 * Runs P4 process and catches output made by this process.
	 *
	 * @param CommandLine Command line to run P4 process with.
	 * @param Output Catched output.
	 *
	 * @returns True if succeeded. False otherwise.
	 */
	static bool RunP4Output(const FString& CommandLine, FString& Output);

	/**
	 * Runs P4 process.
	 *
	 * @param CommandLine Command line to run P4 process with.
	 *
	 * @returns True if succeeded. False otherwise.
	 */
	static bool RunP4(const FString& CommandLine);



	/**
	 * Gets P4 path.
	 *
	 * @returns P4 path.
	 */
	const FString& GetPath() const;

	/**
	 * Gets P4 user.
	 *
	 * @returns P4 user.
	 */
	const FString& GetUser() const;

	/**
	 * Gets P4 port.
	 *
	 * @returns P4 port.
	 */
	const FString& GetPort() const;

	/**
	 * Gets P4 workspace name.
	 *
	 * @returns P4 workspace name.
	 */
	const FString& GetClient() const;

	/**
	 * Gets current P4 branch.
	 *
	 * @returns Current P4 branch.
	 */
	const FString& GetBranch() const;

	/**
	 * Gets command line string to pass to new UnrealSync process to copy current environment.
	 *
	 * @returns Command line string.
	 */
	FString GetCommandLine();

	/**
	 * Gets param type name.
	 *
	 * @param Type Param type enum id.
	 *
	 * @returns Param type name.
	 */
	static FString GetParamName(EP4ParamType::Type Type);

private:
	/* Param serialization delegate. */
	DECLARE_DELEGATE_TwoParams(FSerializationTask, FString&, EP4ParamType::Type);

	/**
	 * Constructor
	 */
	FP4Env();

	/**
	 * Serializes all P4Env params.
	 *
	 * @param SerializationTask Task to perform serialization.
	 */
	void SerializeParams(const FSerializationTask& SerializationTask);

	/**
	 * Auto-detect missing params.
	 *
	 * @param CommandLine Command line that the program was run with.
	 *
	 * @returns True if succeeded. False otherwise.
	 */
	bool AutoDetectMissingParams(const TCHAR* CommandLine);

	/**
	 * Sets param value.
	 *
	 * @param Type Type of param to set.
	 * @param Value Value to set.
	 */
	void SetParam(EP4ParamType::Type Type, const FString& Value);

	/* Singleton instance pointer. */
	static TSharedPtr<FP4Env> Env;

	/* Path to P4 executable. */
	FString Path;

	/* P4 port. */
	FString Port;

	/* P4 user. */
	FString User;

	/* P4 workspace name. */
	FString Client;

	/* P4 current branch. */
	FString Branch;
};
