// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealSync.h"

#include "P4Env.h"
#include "ProcessHelper.h"
#include "Regex.h"
#include "Map.h"
#include "JsonSerializer.h"
#include "JsonObject.h"
#include "JsonReader.h"
#include "SEditableTextBox.h"
#include "SGridPanel.h"

DECLARE_LOG_CATEGORY_CLASS(LogP4Env, Log, All);

/**
 * Parses param from command line.
 *
 * @param Value Output value.
 * @param CommandLine Command line to parse.
 * @param ParamName Param name to parse.
 *
 * @returns True if found, false otherwise.
 */
bool GetCommandLineParam(FString& Value, const TCHAR* CommandLine, EP4ParamType Type)
{
	return FParse::Value(CommandLine, *(FP4Env::GetParamName(Type) + "="), Value);
}

/**
 * Cache class for file settings.
 */
class FSettingsCache
{
public:
	/**
	 * Instance getter.
	 */
	static FSettingsCache& Get()
	{
		static FSettingsCache Cache;
		return Cache;
	}

	/**
	 * Gets setting of given type.
	 *
	 * @param Value Output parameter. If succeeded this parameter will be overwritten with setting value.
	 * @param Type Type to look for.
	 *
	 * @returns True if setting was found. False otherwise.
	 */
	bool GetSetting(FString& Value, EP4ParamType Type)
	{
		auto* ParamPtr = Settings.Find(FP4Env::GetParamName(Type));
		if (ParamPtr == nullptr)
		{
			return false;
		}

		Value = *ParamPtr;
		return true;
	}

	/**
	 * Sets setting of given type to given value.
	 *
	 * @param Type Type of the setting.
	 * @param Value Value of the setting.
	 */
	void SetSetting(EP4ParamType Type, const FString& Value)
	{
		auto* ParamPtr = Settings.Find(FP4Env::GetParamName(Type));
		if (ParamPtr != nullptr)
		{
			*ParamPtr = Value;
		}

		Settings.Add(FP4Env::GetParamName(Type), Value);
	}

	/**
	 * Saves setting to file.
	 */
	void Save()
	{
		TSharedRef<FJsonObject> Object(new FJsonObject());

		for (const auto& Setting : Settings)
		{
			if (!Setting.Value.IsEmpty())
			{
				Object->SetStringField(Setting.Key, Setting.Value);
			}
		}

		FString Buffer;
		TSharedRef<TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&Buffer);
		if (!FJsonSerializer::Serialize(Object, Writer))
		{
			UE_LOG(LogP4Env, Error, TEXT("Failed serializing settings using JSON."));
			return;
		}

		auto bNeedsWrite = true;

		if (FPaths::FileExists(GetSettingsFileName()))
		{
			// Read the file to a string.
			FString FileContents;
			bNeedsWrite = !(FFileHelper::LoadFileToString(FileContents, *GetSettingsFileName()) && FileContents == Buffer);
		}

		if (bNeedsWrite && !FFileHelper::SaveStringToFile(Buffer, *GetSettingsFileName()))
		{
			UE_LOG(LogP4Env, Error, TEXT("Failed writing settings to file: \"%s\"."), *GetSettingsFileName());
			return;
		}
	}

private:
	/**
	 * Constructor.
	 *
	 * Creates the cache from value read from settings file if it exists.
	 */
	FSettingsCache()
	{
		// Read the file to a string.
		FString FileContents;
		if (!FFileHelper::LoadFileToString(FileContents, *GetSettingsFileName()))
		{
			UE_LOG(LogP4Env, Log, TEXT("Couldn't find settings file \"%s\"."), *GetSettingsFileName());
			return;
		}

		// Deserialize a JSON object from the string
		TSharedPtr<FJsonObject> Object;
		TSharedRef<TJsonReader<> > Reader = TJsonReaderFactory<>::Create(FileContents);
		if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
		{
			UE_LOG(LogP4Env, Error, TEXT("Settings file JSON parsing failed: \"%s\"."), *Reader->GetErrorMessage());
			return;
		}

		for (const auto& Value : Object->Values)
		{
			FString Text;
			if (!Value.Value->TryGetString(Text))
			{
				Settings.Empty();
				break;
			}

			Settings.Add(Value.Key, Text);
		}
	}

	/**
	 * Gets settings file name. For now it's hardcoded.
	 *
	 * @returns Settings file name.
	 */
	static const FString& GetSettingsFileName()
	{
		const static FString HardCodedSettingsFileName = "UnrealSync.settings";

		return HardCodedSettingsFileName;
	}

	/** Value map of settings. */
	TMap<FString, FString> Settings;
};

/**
 * Gets param from environment variables.
 *
 * @param Value Output value.
 * @param ParamName Param name to parse.
 *
 * @returns True if found, false otherwise.
 */
bool GetEnvParam(FString& Value, const FString& ParamName)
{
	TCHAR Buf[512];

	FPlatformMisc::GetEnvironmentVariable(*ParamName, Buf, 512);

	if (FCString::Strlen(Buf) > 0)
	{
		Value = Buf;
		return true;
	}

	return false;
}

FP4Env::FP4Env()
{
}

const FString& FP4Env::GetClient() const
{
	return Client;
}

const FString& FP4Env::GetPort() const
{
	return Port;
}

const FString& FP4Env::GetUser() const
{
	return User;
}

/**
 * Gets known path in the depot (arbitrarily chosen).
 *
 * @returns Known path.
 */
FString GetKnownPath()
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::Combine(*FPaths::EngineConfigDir(), TEXT("BaseEngine.ini")));
}

/**
 * Gets computer host name.
 *
 * @returns Host name.
 */
FString GetHostName()
{
	return FPlatformProcess::ComputerName();
}

const FString& FP4Env::GetBranch() const
{
	return Branch;
}

FP4Env& FP4Env::Get()
{
	return *Env;
}

/**
 * Param detection iterator interface.
 */
class IP4EnvParamDetectionIterator
{
public:
	/**
	 * Try to auto-detect next param proposition.
	 *
	 * @returns True if found. False otherwise.
	 */
	virtual bool MoveNext() = 0;

	/**
	 * Gets currently found param proposition.
	 *
	 * @returns Found param.
	 */
	virtual const FString& GetCurrent() const = 0;

	/**
	 * Creates auto-detection iterator for given type.
	 *
	 * @param Type Type of iterator to create.
	 * @param CommandLine Command line to parse by iterator.
	 * @param Env Current P4 environment state.
	 *
	 * @returns Shared pointer to created iterator.
	 */
	static TSharedPtr<IP4EnvParamDetectionIterator> Create(EP4ParamType Type, const TCHAR *CommandLine, const FP4Env& Env);
};

/**
 * Base class for param detection iterators.
 */
class FP4EnvParamDetectionIteratorBase : public IP4EnvParamDetectionIterator
{
public:
	/**
	 * Constructor
	 *
	 * @param Type Type of the param to auto-detect.
	 * @param CommandLine Command line to check for param.
	 */
	FP4EnvParamDetectionIteratorBase(EP4ParamType Type, const TCHAR* CommandLine)
		: Type(Type)
	{
		if (GetCommandLineParam(HardParam, CommandLine, Type))
		{
			OverriddenSetting(Type, HardParam, true);
		}

		if (FSettingsCache::Get().GetSetting(HardParam, Type))
		{
			OverriddenSetting(Type, HardParam, false);
		}
	}

	/**
	 * Try to auto-detect next param proposition.
	 *
	 * @returns True if found. False otherwise.
	 */
	virtual bool MoveNext() override
	{
		if (bFinished)
		{
			if (!HardParam.IsEmpty())
			{
				Current = HardParam;
				HardParam.Empty();
				return true;
			}
			else
			{
				return false;
			}
		}

		if (!bEnvChecked)
		{
			bEnvChecked = true;

			if (GetEnvParam(Current, FP4Env::GetParamName(Type)))
			{
				return true;
			}
		}

		return FindNext(Current);
	}

	/**
	 * Gets currently found param proposition.
	 *
	 * @returns Found param.
	 */
	virtual const FString& GetCurrent() const override
	{
		return Current;
	}

protected:
	/**
	 * Find next param.
	 *
	 * @param Output Output parameter.
	 *
	 * @returns True if found. False otherwise.
	 */
	virtual bool FindNext(FString& Output) = 0;

	/**
	 * Mark this iterator as finished.
	 */
	void Finish()
	{
		bFinished = true;
	}

	/**
	 * Method reports in the log that given param is overridden (from settings or command line).
	 *
	 * @param Type Param type to report on.
	 * @param Value Value of this param.
	 * @param CommandLine Boolean value that tells if this override comes from command line. If false it comes from settings file.
	 */
	void OverriddenSetting(EP4ParamType Type, const FString& Value, bool CommandLine)
	{
		FString Source = CommandLine ? TEXT("command line") : TEXT("settings file");

		UE_LOG(LogP4Env, Log, TEXT("Setting %s overridden from %s and set to value \"%s\"."), *FP4Env::GetParamName(Type), *Source, *Value);
		Finish();
	}

private:
	/** Currently found param proposition. */
	FString Current;

	/** Found hard param i.e. it can't be replaced by any autodetected ones. */
	FString HardParam;

	/** Type of the param. */
	EP4ParamType Type;

	/** Is this iterator finished. */
	bool bFinished = false;

	/** Tells if environment variable have been checked already by this iterator. */
	bool bEnvChecked = false;
};

/**
 * P4 path param auto-detection iterator.
 */
class FP4PathDetectionIterator : public FP4EnvParamDetectionIteratorBase
{
public:
	/**
	 * Constructor.
	 *
	 * @param CommandLine Command line to parse.
	 */
	FP4PathDetectionIterator(const TCHAR* CommandLine)
		: Step(0), FP4EnvParamDetectionIteratorBase(EP4ParamType::Path, CommandLine)
	{
		const TCHAR* LocationsToLook[] =
		{
			TEXT("C:\\Program Files\\Perforce"),
			TEXT("C:\\Program Files (x86)\\Perforce")
		};

		// Tries to detect in standard environment paths.
		static const FString WhereCommand = "where"; // TODO Mac: I think 'where' command equivalent on Mac is 'whereis'
		static const FString P4ExecutableName = "p4.exe";

		FString WhereOutput;
		if (RunProcessOutput(WhereCommand, P4ExecutableName, WhereOutput))
		{
			AddUniqueLocation(FPaths::ConvertRelativePathToFull(
				WhereOutput.Replace(TEXT("\n"), TEXT("")).Replace(TEXT("\r"), TEXT(""))));
		}

		for (const auto* LocationToLook : LocationsToLook)
		{
			FString LocationCandidate = FPaths::ConvertRelativePathToFull(FPaths::Combine(LocationToLook, *P4ExecutableName));
			if (FPaths::FileExists(LocationCandidate))
			{
				AddUniqueLocation(LocationCandidate);
			}
		}
	}

	/**
	 * Function that tries to detect P4 executable path.
	 *
	 * @param Output Output param. Found P4 executable path.
	 *
	 * @returns True if succeeded. False otherwise.
	 */
	virtual bool FindNext(FString& Output) override
	{
		if (Step >= PossibleLocations.Num())
		{
			Finish();
			return false;
		}

		Output = PossibleLocations[Step++];
		return true;
	}

private:
	/**
	 * Adds unique location of P4.
	 *
	 * @param Path Location to add.
	 */
	void AddUniqueLocation(const FString& Path)
	{
		auto NormalizedPath = PlatformNormalize(Path);
		if (!PossibleLocations.Contains(NormalizedPath))
		{
			PossibleLocations.Add(NormalizedPath);
		}
	}

	/**
	 * Normalizes path in platform-specific way, i.e. lowers case on Windowsish
	 * systems and leaves it intact on *nix.
	 */
	FString PlatformNormalize(const FString& Path)
	{
#if PLATFORM_WINDOWS || PLATFORM_WINRT || PLATFORM_XBOXONE
		return Path.ToLower();
#else
		return Path;
#endif
	}

	/** Possible P4 installation locations. */
	TArray<FString> PossibleLocations;

	/** Current step number. */
	int32 Step;
};

#include "XmlParser.h"

/**
 * Tries to get last connection string from P4V config file.
 *
 * @param Output Output param.
 * @param LastConnectionStringId Which string to output.
 *
 * @returns True if succeeded, false otherwise.
 */
bool GetP4VLastConnectionStringElement(FString& Output, int32 LastConnectionStringId)
{
	class FLastConnectionStringCache
	{
	public:
		FLastConnectionStringCache()
		{
			bFoundData = false;
			// TODO Mac: This path is going to be different on Mac.
			FString AppSettingsXmlPath = FPaths::ConvertRelativePathToFull(
					FPaths::Combine(FPlatformProcess::UserDir(),
						TEXT(".."), TEXT(".p4qt"),
						TEXT("ApplicationSettings.xml")
					)
				);

			if (!FPaths::FileExists(AppSettingsXmlPath))
			{
				return;
			}

			TSharedPtr<FXmlFile> Doc = MakeShareable(new FXmlFile(AppSettingsXmlPath, EConstructMethod::ConstructFromFile));

			for (FXmlNode* PropertyListNode : Doc->GetRootNode()->GetChildrenNodes())
			{
				if (PropertyListNode->GetTag() != "PropertyList" || PropertyListNode->GetAttribute("varName") != "Connection")
				{
					continue;
				}

				for (FXmlNode* VarNode : PropertyListNode->GetChildrenNodes())
				{
					if (VarNode->GetTag() != "String" || VarNode->GetAttribute("varName") != "LastConnection")
					{
						continue;
					}

					bFoundData = true;

					FString Content = VarNode->GetContent();
					FString Current;
					FString Rest;

					while (Content.Split(",", &Current, &Rest))
					{
						Current.Trim();
						Current.TrimTrailing();

						Data.Add(Current);

						Content = Rest;
					}

					Rest.Trim();
					Rest.TrimTrailing();

					Data.Add(Rest);
				}
			}
		}

		bool bFoundData;
		TArray<FString> Data;
	};

	static FLastConnectionStringCache Cache;

	if (!Cache.bFoundData || Cache.Data.Num() <= LastConnectionStringId)
	{
		return false;
	}

	Output = Cache.Data[LastConnectionStringId];
	return true;
}

/**
 * P4 port param auto-detection iterator.
 */
class FP4PortDetectionIterator : public FP4EnvParamDetectionIteratorBase
{
public:
	/**
	 * Constructor.
	 *
	 * @param CommandLine Command line to parse.
	 */
	FP4PortDetectionIterator(const TCHAR* CommandLine)
		: Step(0), FP4EnvParamDetectionIteratorBase(EP4ParamType::Port, CommandLine)
	{ }

	/**
	 * Function that tries to detect P4 port.
	 *
	 * @param Output Output param. Found P4 port.
	 *
	 * @returns True if succeeded. False otherwise.
	 */
	virtual bool FindNext(FString& Output) override
	{
		if (Step == 0)
		{
			++Step;
			if (GetP4VLastConnectionStringElement(Output, 0))
			{
				return true;
			}
		}

		if (Step == 1)
		{
			++Step;

			// Fallback to default port. If it's not valid auto-detection will fail later.
			Output = "perforce:1666";
			return true;
		}

		Finish();
		return false;
	}

private:
	/** Current step number. */
	int32 Step;
};

/**
 * P4 user param auto-detection iterator.
 */
class FP4UserDetectionIterator : public FP4EnvParamDetectionIteratorBase
{
public:
	/**
	 * Constructor.
	 *
	 * @param CommandLine Command line to parse.
	 * @param Env Current P4 environment state.
	 */
	FP4UserDetectionIterator(const TCHAR* CommandLine, const FP4Env& Env)
		: Step(0), FP4EnvParamDetectionIteratorBase(EP4ParamType::User, CommandLine), Env(Env)
	{
	}

	/**
	 * Function that tries to detect P4 user.
	 *
	 * @param Output Output param. Found P4 user.
	 *
	 * @returns True if succeeded. False otherwise.
	 */
	virtual bool FindNext(FString& Output) override
	{
		if (Step == 0)
		{
			++Step;
			if (GetP4VLastConnectionStringElement(Output, 1))
			{
				return true;
			}
		}

		if (Step == 1)
		{
			++Step;

			FString InfoOutput;
			RunProcessOutput(Env.GetPath(), FString::Printf(TEXT("-p%s info"), *Env.GetPort()), InfoOutput);

			const FRegexPattern UserNamePattern(TEXT("User name:\\s*([^ \\t\\n\\r]+)\\s*"));

			FRegexMatcher Matcher(UserNamePattern, InfoOutput);
			if (Matcher.FindNext())
			{
				Output = InfoOutput.Mid(Matcher.GetCaptureGroupBeginning(1), Matcher.GetCaptureGroupEnding(1) - Matcher.GetCaptureGroupBeginning(1));
				return true;
			}
		}

		Finish();
		return false;
	}

private:
	/** Current P4 environment state. */
	const FP4Env& Env;
	
	/** Current step number. */
	int32 Step;
};

#include "Internationalization/Regex.h"

/**
 * P4 client param auto-detection iterator.
 */
class FP4ClientDetectionIterator : public FP4EnvParamDetectionIteratorBase
{
public:
	/**
	 * Constructor.
	 *
	 * @param CommandLine Command line to parse.
	 * @param Env Current P4 environment state.
	 */
	FP4ClientDetectionIterator(const TCHAR* CommandLine, const FP4Env& Env)
		: FP4EnvParamDetectionIteratorBase(EP4ParamType::Client, CommandLine), Env(Env)
	{
		auto P4CommandLine = FString::Printf(TEXT("-p%s clients -u%s"), *Env.GetPort(), *Env.GetUser());
		if (!RunProcessOutput(Env.GetPath(), P4CommandLine, P4ClientsOutput))
		{
			UE_LOG(LogP4Env, Log, TEXT("Failed to get client list. Used settings:") LINE_TERMINATOR
				TEXT("\tPath: %s") LINE_TERMINATOR
				TEXT("\tPort: %s") LINE_TERMINATOR
				TEXT("\tUser: %s"), *Env.GetPath(), *Env.GetPort(), *Env.GetUser())

			Finish();
			return;
		}

		static const FRegexPattern ClientsPattern(TEXT("Client ([^ ]+) \\d{4}/\\d{2}/\\d{2} root (.+) '.*'"));
		Matcher = MakeShareable(new FRegexMatcher(ClientsPattern, P4ClientsOutput));
	}

	/**
	 * Function that tries to detect P4 client.
	 *
	 * @param Output Output param. Found P4 client.
	 *
	 * @returns True if succeeded. False otherwise.
	 */
	virtual bool FindNext(FString& Output) override
	{
		static const FString KnownPath = GetKnownPath();
		static const FString HostName = GetHostName();

		while (Matcher->FindNext())
		{
			auto ClientName = P4ClientsOutput.Mid(Matcher->GetCaptureGroupBeginning(1), Matcher->GetCaptureGroupEnding(1) - Matcher->GetCaptureGroupBeginning(1));
			auto Root = P4ClientsOutput.Mid(Matcher->GetCaptureGroupBeginning(2), Matcher->GetCaptureGroupEnding(2) - Matcher->GetCaptureGroupBeginning(2));

			if (KnownPath.StartsWith(FPaths::ConvertRelativePathToFull(Root)))
			{
				FString InfoOutput;
				if (!RunProcessOutput(Env.GetPath(), FString::Printf(TEXT("-p%s -u%s client -o %s"), *Env.GetPort(), *Env.GetUser(), *ClientName), InfoOutput))
				{
					continue;
				}

				const FRegexPattern InfoPattern(TEXT("Host:\\s*([^\\r\\n\\t ]+)\\s*"));
				FRegexMatcher InfoMatcher(InfoPattern, InfoOutput);

				while (InfoMatcher.FindNext())
				{
					if (InfoOutput.Mid(
						InfoMatcher.GetCaptureGroupBeginning(1),
						InfoMatcher.GetCaptureGroupEnding(1) - InfoMatcher.GetCaptureGroupBeginning(1)
						).Equals(HostName, ESearchCase::IgnoreCase))
					{
						Output = ClientName;
						return true;
					}
				}
			}
		}

		Finish();
		return false;
	}

private:
	/** Current regex matcher that parses next client. */
	TSharedPtr<FRegexMatcher> Matcher;

	/** Current P4 environment state. */
	const FP4Env& Env;

	/** Output of the p4 clients command. */
	FString P4ClientsOutput;
};

/**
 * Gets branch detected for this app from current P4 environment state.
 *
 * @param Output Found branch prefix.
 * @param Env Current P4 environment state.
 *
 * @returns True if found, false otherwise.
 */
bool GetCurrentBranch(FString& Output, const FP4Env& Env)
{
	FString FilesOutput;
	if (!RunProcessOutput(Env.GetPath(), FString::Printf(TEXT("-p%s -u%s -c%s files %s"),
		*Env.GetPort(), *Env.GetUser(), *Env.GetClient(), *GetKnownPath()), FilesOutput))
	{
		UE_LOG(LogP4Env, Log,
			TEXT("Failed to verify branch. Details below:") LINE_TERMINATOR
			TEXT("Note that you can't use this tool for cross syncing, i.e. you need to use") LINE_TERMINATOR
			TEXT("UnrealSync from the same path as the branch being synced. In other word, if P4") LINE_TERMINATOR
			TEXT("with given settings can't recognize currently running executable the") LINE_TERMINATOR
			TEXT("verification will fail. Used settings:") LINE_TERMINATOR
			TEXT("\tPath: %s") LINE_TERMINATOR
			TEXT("\tPort: %s") LINE_TERMINATOR
			TEXT("\tUser: %s") LINE_TERMINATOR
			TEXT("\tClient: %s"), *Env.GetPath(),*Env.GetPort(), *Env.GetUser(), *Env.GetClient())
		return false;
	}

	FString Rest;

	if (!FilesOutput.Split("/Engine/", &Output, &Rest))
	{
		return false;
	}

	return true;
}

/**
 * P4 client param auto-detection iterator.
 */
class FP4BranchDetectionIterator : public FP4EnvParamDetectionIteratorBase
{
public:
	/**
	 * Constructor.
	 *
	 * @param CommandLine Command line to parse.
	 * @param Env Current P4 environment state.
	 */
	FP4BranchDetectionIterator(const TCHAR* CommandLine, const FP4Env& Env)
		: FP4EnvParamDetectionIteratorBase(EP4ParamType::Branch, CommandLine), Env(Env)
	{
	}

	/**
	 * Function that tries to detect P4 branch.
	 *
	 * @param Output Output param. Found P4 branch.
	 *
	 * @returns True if succeeded. False otherwise.
	 */
	virtual bool FindNext(FString& Output) override
	{
		Finish();

		return GetCurrentBranch(Output, Env);
	}

private:
	/** Current P4 environment state. */
	const FP4Env& Env;
};

bool FP4Env::Init(const TCHAR* CommandLine)
{
	TSharedPtr<FP4Env> Env = MakeShareable(new FP4Env());

	if (!Env->AutoDetectMissingParams(CommandLine))
	{
		return false;
	}

	FString CurrentBranch;
	if (!GetCurrentBranch(CurrentBranch, *Env) || CurrentBranch != Env->GetBranch())
	{
		return false;
	}

	FP4Env::Env = Env;

	return true;
}

bool FP4Env::RunP4Progress(const FString& CommandLine, const FOnP4MadeProgress& OnP4MadeProgress)
{
	return RunProcessProgress(Get().GetPath(), FString::Printf(TEXT("-p%s -c%s -u%s %s"), *Get().GetPort(), *Get().GetClient(), *Get().GetUser(), *CommandLine), OnP4MadeProgress);
}

bool FP4Env::RunP4Output(const FString& CommandLine, FString& Output)
{
	class FOutputCollector
	{
	public:
		bool Progress(const FString& Chunk)
		{
			Output += Chunk;

			return true;
		}

		FString Output;
	};

	FOutputCollector OC;

	if (!RunP4Progress(CommandLine, FOnP4MadeProgress::CreateRaw(&OC, &FOutputCollector::Progress)))
	{
		return false;
	}

	Output = OC.Output;
	return true;
}

bool FP4Env::RunP4(const FString& CommandLine)
{
	return RunP4Progress(CommandLine, nullptr);
}

void FP4Env::SerializeParams(const FSerializationTask& SerializationTask)
{
	SerializationTask.ExecuteIfBound(Path, EP4ParamType::Path);
	SerializationTask.ExecuteIfBound(Port, EP4ParamType::Port);
	SerializationTask.ExecuteIfBound(User, EP4ParamType::User);
	SerializationTask.ExecuteIfBound(Client, EP4ParamType::Client);
	SerializationTask.ExecuteIfBound(Branch, EP4ParamType::Branch);
}

FString FP4Env::GetParamName(EP4ParamType Type)
{
	switch (Type)
	{
	case EP4ParamType::Path:
		return "P4PATH";
	case EP4ParamType::Port:
		return "P4PORT";
	case EP4ParamType::User:
		return "P4USER";
	case EP4ParamType::Client:
		return "P4CLIENT";
	case EP4ParamType::Branch:
		return "P4BRANCH";
	}

	checkNoEntry();
	return "";
}

bool FP4Env::AutoDetectMissingParams(const TCHAR* CommandLine)
{
	TArray<TSharedPtr<IP4EnvParamDetectionIterator> > ParamDetectionIteratorsStack;

	auto Type = EP4ParamType::Path;
	ParamDetectionIteratorsStack.Add(IP4EnvParamDetectionIterator::Create(Type, CommandLine, *this));

	int32 IterationCountdown = 20;

	while (ParamDetectionIteratorsStack.Num() > 0)
	{
		if (ParamDetectionIteratorsStack.Last()->MoveNext())
		{
			SetParam(Type, ParamDetectionIteratorsStack.Last()->GetCurrent());

			if (Type != EP4ParamType::Branch)
			{
				Type = (EP4ParamType) ((int)Type + 1);
				ParamDetectionIteratorsStack.Add(IP4EnvParamDetectionIterator::Create(Type, CommandLine, *this));
				continue;
			}
			else
			{
				return true;
			}
		}
		else
		{
			Type = (EP4ParamType) ((int)Type - 1);
			ParamDetectionIteratorsStack.RemoveAt(ParamDetectionIteratorsStack.Num() - 1);
			--IterationCountdown;
			if (!IterationCountdown)
			{
				break;
			}
		}
	}

	return false;
}

FString FP4Env::GetCommandLine()
{
	class CommandLineOutput
	{
	public:
		void AppendParam(FString& FieldReference, EP4ParamType Type)
		{
			Output += " -" + GetParamName(Type) + "=\"" + FieldReference + "\"";
		}

		FString Output;
	};

	CommandLineOutput Output;

	SerializeParams(FSerializationTask::CreateRaw(&Output, &CommandLineOutput::AppendParam));

	return Output.Output;
}

const FString& FP4Env::GetPath() const
{
	return Path;
}

void FP4Env::SetParam(EP4ParamType Type, const FString& Value)
{
	switch (Type)
	{
	case EP4ParamType::Path:
		Path = Value;
		break;
	case EP4ParamType::Port:
		Port = Value;
		break;
	case EP4ParamType::User:
		User = Value;
		break;
	case EP4ParamType::Client:
		Client = Value;
		break;
	case EP4ParamType::Branch:
		Branch = Value;
		break;
	default:
		// Unimplemented param type?
		checkNoEntry();
	}
}

bool FP4Env::CheckIfFileNeedsUpdate(const FString& FilePath)
{
	FString Output;
	if (!RunP4Output(FString::Printf(TEXT("fstat %s"), *FilePath), Output))
	{
		UE_LOG(LogP4Env, Error, TEXT("Checking if file \"%s\" needs update, failed."), *FilePath);
		return false;
	}

	static const FRegexPattern HeadRevPattern(TEXT("\\.\\.\\. headRev (\\d+)"));
	static const FRegexPattern HaveRevPattern(TEXT("\\.\\.\\. haveRev (\\d+)"));
	static const FRegexPattern ChangePattern(TEXT("\\.\\.\\. change ([^\\n]+)"));

	FRegexMatcher ChangeMatcher(ChangePattern, Output);

	if (ChangeMatcher.FindNext())
	{
		UE_LOG(LogP4Env, Error, TEXT("File \"%s\" is checked out, can't update."), *FilePath);
		return false;
	}

	FRegexMatcher HeadRevMatcher(HeadRevPattern, Output);
	FRegexMatcher HaveRevMatcher(HaveRevPattern, Output);

	if (!HeadRevMatcher.FindNext() || !HaveRevMatcher.FindNext())
	{
		UE_LOG(LogP4Env, Error, TEXT("Can't determine head or have revision for file \"%s\"."), *FilePath);
		return false;
	}

	int32 HeadRev = FPlatformString::Atoi(*Output.Mid(
		HeadRevMatcher.GetCaptureGroupBeginning(1),
		HeadRevMatcher.GetCaptureGroupEnding(1) - HeadRevMatcher.GetCaptureGroupBeginning(1)));

	int32 HaveRev = FPlatformString::Atoi(*Output.Mid(
		HaveRevMatcher.GetCaptureGroupBeginning(1),
		HaveRevMatcher.GetCaptureGroupEnding(1) - HaveRevMatcher.GetCaptureGroupBeginning(1)));

	return HaveRev < HeadRev;
}

bool FP4Env::IsValid()
{
	return Env.IsValid();
}

const FString& FP4Env::GetParamByType(EP4ParamType Type) const
{
	switch (Type)
	{
	case EP4ParamType::Path:
		return GetPath();
	case EP4ParamType::Port:
		return GetPort();
	case EP4ParamType::User:
		return GetUser();
	case EP4ParamType::Client:
		return GetClient();
	case EP4ParamType::Branch:
		return GetBranch();
	default:
		// Unimplemented param type?
		checkNoEntry();
		throw 0; // Won't get here, used to suppress "not all control paths return a value"
	}
}

TSharedPtr<IP4EnvParamDetectionIterator> IP4EnvParamDetectionIterator::Create(EP4ParamType Type, const TCHAR* CommandLine, const FP4Env& Env)
{
	switch (Type)
	{
	case EP4ParamType::Path:
		return MakeShareable(new FP4PathDetectionIterator(CommandLine));
	case EP4ParamType::Port:
		return MakeShareable(new FP4PortDetectionIterator(CommandLine));
	case EP4ParamType::User:
		return MakeShareable(new FP4UserDetectionIterator(CommandLine, Env));
	case EP4ParamType::Client:
		return MakeShareable(new FP4ClientDetectionIterator(CommandLine, Env));
	case EP4ParamType::Branch:
		return MakeShareable(new FP4BranchDetectionIterator(CommandLine, Env));
	default:
		// Unimplemented param type?
		checkNoEntry();
		return nullptr;
	}
}

/* Static variable initialization. */
TSharedPtr<FP4Env> FP4Env::Env;

/**
 * Grid panel widget with labelled options.
 */
class SOptionsPanel : public SGridPanel
{
public:
	SLATE_BEGIN_ARGS(SOptionsPanel) { }
	SLATE_END_ARGS()

	/**
	 * Method that constructs this widget given Slate construction arguments.
	 *
	 * @param InArgs Slate construction arguments.
	 */
	void Construct(const FArguments& InArgs)
	{
		SetColumnFill(1, 1.0f);
	}

	/**
	 * Inner class forward declaration.
	 */
	class SOption;

	/**
	 * Adds option widget to the grid.
	 *
	 * @param Option Option to add.
	 */
	void Add(TSharedPtr<SOption> Option);

private:
	/** Collection of all options in this grid. */
	TArray<TSharedPtr<SOption> > Options;
};

/**
 * Widget and label provider for any option added.
 */
class SOptionsPanel::SOption : public TSharedFromThis<SOption>
{
	friend void SOptionsPanel::Add(TSharedPtr<SOption> Option);

public:
	/**
	 * Constructor
	 */
	SOption() { }

protected:
	/**
	 * Must be overridden. Provides a label for this option.
	 *
	 * @returns Label for this option.
	 */
	virtual FText GetLabel() const = 0;

	/**
	 * Creates and returns widget for this option. If not overridden it creates editable text box.
	 *
	 * @returns Widget for this option.
	 */
	virtual TSharedRef<SWidget> CreateWidget() { return SNew(SEditableTextBox); }
};

void SOptionsPanel::Add(TSharedPtr<SOption> Option)
{
	auto Id = Options.Add(Option);

	AddSlot(0, Id).Padding(1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Option.Get(), &SOption::GetLabel)
			]
		];

	AddSlot(1, Id).Padding(1.0f)
		[
			Option->CreateWidget()
		];
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SP4EnvTabWidget::Construct(const FArguments& InArgs)
{
	/**
	 * P4 option widget.
	 */
	class SP4Option : public SOptionsPanel::SOption
	{
	public:
		/**
		 * Constructor
		 *
		 * @param Reference to FP4Option object this object relates to.
		 */
		SP4Option(TSharedRef<FP4Option> Option)
			: Option(Option)
		{

		}

	protected:
		/**
		 * Creates and returns widget for this option.
		 *
		 * @returns Widget for this option.
		 */
		virtual TSharedRef<SWidget> CreateWidget() override
		{
			return SNew(SEditableTextBox)
				.OnTextCommitted(this, &SP4Option::OnTextCommitted)
				.Text(this, &SP4Option::GetText)
				.ToolTipText(FText::Format(LOCTEXT("P4OptionToolTip", "This is a setting for {0}. If you see greyed text, then it means that this setting was autodetected."), FText::FromString(FP4Env::GetParamName(Option->GetType()))))
				.HintText(this, &SP4Option::GetHintText);
		}

		/**
		 * On text committed event handler.
		 *
		 * @param CommittedText Committed text.
		 * @param Type Commit type.
		 */
		void OnTextCommitted(const FText& CommittedText, ETextCommit::Type Type)
		{
			Option->GetText() = CommittedText;
		}

		/**
		 * SEditableTextBox text provider.
		 *
		 * @returns Text that should be displayed in SEditableTextBox for this option.
		 */
		FText GetText() const
		{
			return Option->GetText();
		}

		/**
		 * SEditableTextBox hint text provider (greyed text).
		 *
		 * @returns Hint text that should be displayed in SEditableTextBox for this option.
		 */
		FText GetHintText() const
		{
			if (FP4Env::IsValid())
			{
				auto Env = FP4Env::Get();

				return FText::FromString(Env.GetParamByType(Option->GetType()));
			}

			return FText();
		}

		/**
		 * Provides a label for this option.
		 *
		 * @returns Label for this option.
		 */
		virtual FText GetLabel() const override
		{
			switch (Option->GetType())
			{
			case EP4ParamType::Branch:
				return LOCTEXT("P4Option_Branch", "Branch name");
			case EP4ParamType::Client:
				return LOCTEXT("P4Option_Client", "Workspace name");
			case EP4ParamType::Path:
				return LOCTEXT("P4Option_Path", "Path to P4 executable");
			case EP4ParamType::Port:
				return LOCTEXT("P4Option_Port", "P4 server address");
			case EP4ParamType::User:
				return LOCTEXT("P4Option_User", "P4 user name");
			default:
				checkNoEntry();
				throw 0; // Won't get here, used to suppress "not all control paths return a value"
			}
		}

	private:
		/** Option reference. */
		TSharedRef<FP4Option> Option;
	};

	Options.Reserve(4);
	Options.Add(MakeShareable(new FP4Option(EP4ParamType::Path)));
	Options.Add(MakeShareable(new FP4Option(EP4ParamType::Port)));
	Options.Add(MakeShareable(new FP4Option(EP4ParamType::User)));
	Options.Add(MakeShareable(new FP4Option(EP4ParamType::Client)));

	auto OptionsPanel = SNew(SOptionsPanel);

	for (auto Option : Options)
	{
		OptionsPanel->Add(MakeShareable(new SP4Option(Option)));
	}

	this->ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().HAlign(HAlign_Center).AutoHeight().Padding(10.0f)
			[
				SNew(STextBlock)
				.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 14))
				.Text(LOCTEXT("P4Settings", "Perforce settings"))
			]
			+ SVerticalBox::Slot()
			[
				OptionsPanel
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(
						!FP4Env::IsValid()
						? LOCTEXT("P4AutodetectFailed", "P4 environment detection failed. See the log for more details.")
						: LOCTEXT("P4AutodetectSucceeded", "P4 environment detection succeeded.")
					)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.OnClicked(this, &SP4EnvTabWidget::OnSaveAndRestartButtonClick)
					[
						SNew(STextBlock).Text(LOCTEXT("SaveAndRestart", "Save and restart"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.OnClicked(this, &SP4EnvTabWidget::OnCloseButtonClick)
					[
						SNew(STextBlock).Text(LOCTEXT("Close", "Close"))
					]
				]
			]
		];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FReply SP4EnvTabWidget::OnSaveAndRestartButtonClick()
{
	auto& Settings = FSettingsCache::Get();

	for (auto Option : Options)
	{
		Settings.SetSetting(Option->GetType(), Option->GetText().ToString());
	}

	Settings.Save();

	FUnrealSync::RunDetachedUS(FPlatformProcess::ExecutableName(false), true, true, false);
	FPlatformMisc::RequestExit(false);

	return FReply::Handled();
}

FReply SP4EnvTabWidget::OnCloseButtonClick()
{
	FPlatformMisc::RequestExit(false);

	return FReply::Handled();
}

SP4EnvTabWidget::FP4Option::FP4Option(EP4ParamType Type)
	: Type(Type)
{
	FString Param;
	if (FSettingsCache::Get().GetSetting(Param, Type))
	{
		Text = FText::FromString(Param);
	}
}
