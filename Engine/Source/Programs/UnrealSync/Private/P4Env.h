// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "CoreUObject.h"

/**
 * Enum describing P4 param type.
 */
enum class EP4ParamType
{
	Path = 0,
	Port = 1,
	User = 2,
	Client = 3,
	Branch = 4,
	OptionalDelimiter = 5,
	P4VPath = 6,
	EndParam = 7
};

/**
 * P4 settings tab.
 */
class SP4EnvTabWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SP4EnvTabWidget){}
	SLATE_END_ARGS()

	/**
	 * Slate constructor
	 *
	 * @param InArgs Slate args.
	 */
	void Construct(const FArguments& InArgs);

	/**
	 * Function that is called on when Close button is clicked.
	 * It tells the app to close.
	 *
	 * @returns Tells that this event was handled.
	 */
	FReply OnCloseButtonClick();

	/**
	 * Function that is called on when Close button is clicked.
	 * It tells the app to close.
	 *
	 * @returns Tells that this event was handled.
	 */
	FReply OnSaveAndRestartButtonClick();

private:
	/**
	 * P4 option container.
	 */
	class FP4Option
	{
	public:
		/**
		 * Constructor
		 *
		 * @param InType The type for this option.
		 */
		FP4Option(EP4ParamType InType);

		/**
		 * Gets the type of this option.
		 */
		EP4ParamType GetType() const { return Type; }

		/**
		 * Gets the text of this option.
		 */
		FText GetText() const { return Text; }

		/**
		 * Gets the reference to text of this option.
		 */
		FText& GetText() { return Text; }

	private:
		/** This option's type. */
		EP4ParamType Type;

		/** This option's text. */
		FText Text;
	};

	/** Collection of options maintained by this widget. */
	TArray<TSharedRef<FP4Option> > Options;
};

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
	 * Tells if P4 environment is valid.
	 *
	 * @returns True if P4 environment is valid. False otherwise.
	 */
	static bool IsValid();

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
	 * Gets P4V path.
	 *
	 * @returns P4V path.
	 */
	const FString& GetP4VPath() const;

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
	static FString GetParamName(EP4ParamType Type);

	/**
	 * Gets param value using type.
	 *
	 * @param Type Param type enum id.
	 *
	 * @returns Param value.
	 */
	const FString& GetParamByType(EP4ParamType Type) const;

	/**
	 * Checks if given file needs update, i.e. is not at head revision.
	 *
	 * @param FilePath
	 *
	 * @returns True if needs update. False otherwise.
	 */
	static bool CheckIfFileNeedsUpdate(const FString& FilePath);

	/**
	 * Gets setting of given type.
	 *
	 * @param Value Output parameter. If succeeded this parameter will be overwritten with setting value.
	 * @param Type Type to look for.
	 *
	 * @returns True if setting was found. False otherwise.
	 */
	static bool GetSetting(FString& Value, EP4ParamType Type);

private:
	/* Param serialization delegate. */
	DECLARE_DELEGATE_TwoParams(FSerializationTask, FString&, EP4ParamType);

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
	 * Auto-detect missing optional params.
	 *
	 * @param CommandLine Command line that the program was run with.
	 */
	void AutoDetectMissingOptionalParams(const TCHAR* CommandLine);

	/**
	 * Sets param value.
	 *
	 * @param Type Type of param to set.
	 * @param Value Value to set.
	 */
	void SetParam(EP4ParamType Type, const FString& Value);

	/* Singleton instance pointer. */
	static TSharedPtr<FP4Env> Env;

	/* Path to P4 executable. */
	FString Path;

	/* Path to P4V executable. */
	FString P4VPath;

	/* P4 port. */
	FString Port;

	/* P4 user. */
	FString User;

	/* P4 workspace name. */
	FString Client;

	/* P4 current branch. */
	FString Branch;

	/** Value map of P4 settings. */
	TMap<FString, FString> P4FileSettingsCache;
};
