// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealSync.h"

#include "P4Env.h"
#include "ProcessHelper.h"
#include "Regex.h"

/**
 * Parses param from command line.
 *
 * @param Value Output value.
 * @param CommandLine Command line to parse.
 * @param ParamName Param name to parse.
 *
 * @returns True if found, false otherwise.
 */
bool GetParam(FString& Value, const TCHAR* CommandLine, const FString& ParamName)
{
	return FParse::Value(CommandLine, *(ParamName + "="), Value);
}

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
	static TSharedPtr<IP4EnvParamDetectionIterator> Create(EP4ParamType::Type Type, const TCHAR *CommandLine, const FP4Env& Env);
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
	FP4EnvParamDetectionIteratorBase(EP4ParamType::Type Type, const TCHAR* CommandLine)
		: Type(Type)
	{
		if (GetParam(CommandLineParam, CommandLine, FP4Env::GetParamName(Type)))
		{
			Finish();
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
			if (!CommandLineParam.IsEmpty())
			{
				Current = CommandLineParam;
				CommandLineParam.Empty();
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

private:
	/** Currently found param proposition. */
	FString Current;

	/** Found command line param. */
	FString CommandLineParam;

	/** Type of the param. */
	EP4ParamType::Type Type;

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
		PossibleLocations.Add("C:\\Program Files\\Perforce");
		PossibleLocations.Add("C:\\Program Files (x86)\\Perforce");
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
		// Tries to detect in standard environment paths.
		static const FString WhereCommand = "where"; // TODO Mac: I think 'where' command equivalent on Mac is 'whereis'
		static const FString P4ExecutableName = "p4.exe";

		if (Step == 0)
		{
			++Step;

			FString WhereOutput;
			if (RunProcessOutput(WhereCommand, P4ExecutableName, WhereOutput))
			{
				// P4 found, quick exit.
				Output = FPaths::ConvertRelativePathToFull(
					WhereOutput.Replace(TEXT("\n"), TEXT("")).Replace(TEXT("\r"), TEXT("")));
				return true;
			}

			return false;
		}

		if (Step > PossibleLocations.Num())
		{
			Finish();
			return false;
		}

		for (; Step <= PossibleLocations.Num(); ++Step)
		{
			FString LocationCandidate = FPaths::ConvertRelativePathToFull(FPaths::Combine(*PossibleLocations[Step - 1], *P4ExecutableName));
			if (FPaths::FileExists(LocationCandidate))
			{
				Output = LocationCandidate;
				return true;
			}
		}

		Finish();
		return false;
	}

private:
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
		if (!RunProcessOutput(Env.GetPath(), FString::Printf(TEXT("-p%s clients -u%s"), *Env.GetPort(), *Env.GetUser()), P4ClientsOutput))
		{
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
 * @parma Env Current P4 environment state.
 *
 * @returns True if found, false otherwise.
 */
bool GetCurrentBranch(FString& Output, const FP4Env& Env)
{
	FString FilesOutput;
	if (!RunProcessOutput(Env.GetPath(), FString::Printf(TEXT("-p%s -u%s -c%s files %s"),
		*Env.GetPort(), *Env.GetUser(), *Env.GetClient(), *GetKnownPath()), FilesOutput))
	{
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

FString FP4Env::GetParamName(EP4ParamType::Type Type)
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

	EP4ParamType::Type Type = EP4ParamType::Path;
	ParamDetectionIteratorsStack.Add(IP4EnvParamDetectionIterator::Create(Type, CommandLine, *this));

	int32 IterationCountdown = 20;

	while (ParamDetectionIteratorsStack.Num() > 0)
	{
		if (ParamDetectionIteratorsStack.Last()->MoveNext())
		{
			SetParam(Type, ParamDetectionIteratorsStack.Last()->GetCurrent());

			if (Type != EP4ParamType::Branch)
			{
				Type = (EP4ParamType::Type) ((int)Type + 1);
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
			Type = (EP4ParamType::Type) ((int)Type - 1);
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
		void AppendParam(FString& FieldReference, EP4ParamType::Type Type)
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

void FP4Env::SetParam(EP4ParamType::Type Type, const FString& Value)
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

TSharedPtr<IP4EnvParamDetectionIterator> IP4EnvParamDetectionIterator::Create(EP4ParamType::Type Type, const TCHAR* CommandLine, const FP4Env& Env)
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