// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnOutputDevices.cpp: Collection of FOutputDevice subclasses
=============================================================================*/

#include "CorePrivatePCH.h"
#include "Misc/App.h"
#include "Templates/UniquePtr.h"
#include <stdio.h>
#include "OutputDeviceHelper.h"

/** Used by tools which include only core to disable log file creation. */
#ifndef ALLOW_LOG_FILE
	#define ALLOW_LOG_FILE 1
#endif

/*-----------------------------------------------------------------------------
	Name Suppression
-----------------------------------------------------------------------------*/

namespace
{
	struct FLogCategoryPtrs
	{
		explicit FLogCategoryPtrs(const FString& InName, ELogVerbosity::Type InVerbosity, bool InPostfix)
		: Name     (InName)
		, Verbosity(InVerbosity)
		, Postfix  (InPostfix)
		{
		}

		FString             Name;
		ELogVerbosity::Type Verbosity;
		bool                Postfix;

		friend bool operator<(const FLogCategoryPtrs& Lhs, const FLogCategoryPtrs& Rhs)
		{
			return Lhs.Name < Rhs.Name;
		}
	};
}

/** A "fake" logging category that is used as a proxy for changing all categories **/
static FLogCategoryBase GlobalVerbosity(TEXT("Global"), ELogVerbosity::All, ELogVerbosity::All);
/** A "fake" logging category that is used as a proxy for changing the default of all categories at boot time. **/
static FLogCategoryBase BootGlobalVerbosity(TEXT("BootGlobal"), ELogVerbosity::All, ELogVerbosity::All);

typedef uint8 UTF8BOMType[3];
static UTF8BOMType UTF8BOM = { 0xEF, 0xBB, 0xBF };

/** Log suppression system implementation **/
class FLogSuppressionImplementation: public FLogSuppressionInterface, private FSelfRegisteringExec
{
	/** Associates a category pointer with the name of the category **/
	TMap<FLogCategoryBase*, FName> Associations;
	/** Associates a category name with a set of category pointers; the inverse of the above.  **/
	TMultiMap<FName, FLogCategoryBase*> ReverseAssociations;
	/** Set of verbosity and break values that were set at boot time. **/
	TMap<FName, uint8> BootAssociations;
	/** For a given category stores the last non-zero verbosity...to support toggling without losing the specific verbosity level **/
	TMap<FName, uint8> ToggleAssociations;

	/**
	 * Process a "[cat] only" string command to the logging suppression system
	 * @param CmdString, string to process
	 * @return true if CmdString was a "[cat] only" string command, false otherwise
	 */
	bool ProcessLogOnly(const FString& CmdString, FOutputDevice& Ar)
	{
		TArray<FString> CommandParts;
		CmdString.ParseIntoArrayWS(CommandParts);
		if (CommandParts.Num() <= 1)
		{
			return false;
		}
		
		static FName NAME_Only(TEXT("only"));
		if (NAME_Only != FName(*CommandParts[1]))
		{
			return false;
		}
		
		FName LogCategory = FName(*CommandParts[0]);
		static const FString OffString = FString(" off");
		static const FString OnString = FString(" Verbose");
		for (auto It : Associations)
		{
			FName Name = It.Value;
			if (Name == LogCategory)
			{
				ProcessCmdString(Name.ToString() + OnString);
				FLogCategoryBase* Verb = It.Key;
				Ar.Logf(TEXT("%s is now %s"), *CommandParts[0], FOutputDeviceHelper::VerbosityToString(Verb ? ELogVerbosity::Type(Verb->Verbosity) : ELogVerbosity::Verbose));
			}
			else
			{
				ProcessCmdString(Name.ToString() + OffString);
			}
		}

		Ar.Logf(TEXT("Disabling other logs finished"));
		return true;
	}

	/** 
	 * Process a string command to the logging suppression system 
	 * @param CmdString, string to process
	 * @param FromBoot, if true, this is a boot time command, and is handled differently
	 */
	void ProcessCmdString(const FString& CmdString, bool FromBoot = false)
	{
		// How to use the log command : `log <category> <verbosity>
		// e.g., Turn off all logging : `log global none
		// e.g., Set specific filter  : `log logshaders verbose
		// e.g., Combo command        : `log global none, log logshaders verbose

		static FName NAME_BootGlobal(TEXT("BootGlobal"));
		static FName NAME_Reset(TEXT("Reset"));
		FString Cmds = CmdString;
		Cmds = Cmds.Trim().TrimQuotes();
		Cmds.Trim();
		TArray<FString> SubCmds;
		Cmds.ParseIntoArray(SubCmds, TEXT(","), true);
		for (int32 Index = 0; Index < SubCmds.Num(); Index++)
		{
			static FString LogString(TEXT("Log "));
			FString Command = SubCmds[Index].Trim();
			if (Command.StartsWith(*LogString))
			{
				Command = Command.Right(Command.Len() - LogString.Len());
			}
			TArray<FString> CommandParts;
			Command.ParseIntoArrayWS(CommandParts);
			if (CommandParts.Num() < 1)
			{
				continue;
			}
			FName Category(*CommandParts[0]);
			if (Category == NAME_Global && FromBoot)
			{
				Category = NAME_BootGlobal; // the boot time global is a special one, since we want things like "log global none, log logshaders verbose"
			}
			TArray<FLogCategoryBase*> CategoryVerbosities;
			uint8 Value = 0;
			if (FromBoot)
			{
				// now maybe this was already set at boot, in which case we override what it had
				uint8* Boot = BootAssociations.Find(Category);
				if (Boot)
				{
					Value = *Boot;
				}
				else
				{
					// see if we had a boot global override
					Boot = BootAssociations.Find(NAME_BootGlobal);
					if (Boot)
					{
						Value = *Boot;
					}
				}
			}
			else
			{
				for (TMultiMap<FName, FLogCategoryBase*>::TKeyIterator It(ReverseAssociations, Category); It; ++It)
				{
					checkSlow(!(It.Value()->Verbosity & ELogVerbosity::BreakOnLog)); // this bit is factored out of this variable, always
					Value = It.Value()->Verbosity | (It.Value()->DebugBreakOnLog ? ELogVerbosity::BreakOnLog : 0);
					CategoryVerbosities.Add(It.Value());
				}					
			}
			if (CommandParts.Num() == 1)
			{
				// only possibility is the reset and toggle command which is meaningless at boot
				if (!FromBoot)
				{
					if (Category == NAME_Reset)
					{
						for (TMap<FLogCategoryBase*, FName>::TIterator It(Associations); It; ++It)
						{
							FLogCategoryBase* Verb = It.Key();
							Verb->ResetFromDefault();
							// store off the last non-zero one for toggle
							checkSlow(!(Verb->Verbosity & ELogVerbosity::BreakOnLog)); // this bit is factored out of this variable, always
							if (Verb->Verbosity)
							{
								// currently on, store this in the pending and clear it
								ToggleAssociations.Add(Category, Verb->Verbosity);
							}
						}
					}
					else
					{
						if (Value & ELogVerbosity::VerbosityMask)
						{
							// currently on, toggle it
							Value = Value & ~ELogVerbosity::VerbosityMask;
						}
						else
						{
							// try to get a non-zero value from the toggle backup
							uint8* Toggle = ToggleAssociations.Find(Category);
							if (Toggle && *Toggle)
							{
								Value |= *Toggle;
							}
							else
							{
								Value |= ELogVerbosity::All;
							}
						}
					}
				}
			}
			else
			{

				// now we have the current value, lets change it!
				for (int32 PartIndex = 1; PartIndex < CommandParts.Num(); PartIndex++)
				{
					FName CmdToken = FName(*CommandParts[PartIndex]);
					static FName NAME_Verbose(TEXT("Verbose"));
					static FName NAME_VeryVerbose(TEXT("VeryVerbose"));
					static FName NAME_All(TEXT("All"));
					static FName NAME_Default(TEXT("Default"));
					static FName NAME_On(TEXT("On"));
					static FName NAME_Off(TEXT("Off"));
					static FName NAME_Break(TEXT("Break"));
					static FName NAME_Fatal(TEXT("Fatal"));
					static FName NAME_Log(TEXT("Log"));
					static FName NAME_Display(TEXT("Display"));
					if (CmdToken == NAME_None)
					{
						Value &= ~ELogVerbosity::VerbosityMask;
						Value |= ELogVerbosity::Fatal;
					}
					else if (CmdToken == NAME_Fatal)
					{
						Value &= ~ELogVerbosity::VerbosityMask;
						Value |= ELogVerbosity::Fatal;
					}
					else if (CmdToken == NAME_Error)
					{
						Value &= ~ELogVerbosity::VerbosityMask;
						Value |= ELogVerbosity::Error;
					}
					else if (CmdToken == NAME_Warning)
					{
						Value &= ~ELogVerbosity::VerbosityMask;
						Value |= ELogVerbosity::Warning;
					}
					else if (CmdToken == NAME_Log)
					{
						Value &= ~ELogVerbosity::VerbosityMask;
						Value |= ELogVerbosity::Log;
					}
					else if (CmdToken == NAME_Display)
					{
						Value &= ~ELogVerbosity::VerbosityMask;
						Value |= ELogVerbosity::Display;
					}
					else if (CmdToken == NAME_Verbose)
					{
						Value &= ~ELogVerbosity::VerbosityMask;
						Value |= ELogVerbosity::Verbose;
					}
					else if (CmdToken == NAME_VeryVerbose || CmdToken == NAME_All)
					{
						Value &= ~ELogVerbosity::VerbosityMask;
						Value |= ELogVerbosity::VeryVerbose;
					}
					else if (CmdToken == NAME_Default)
					{
						if (CategoryVerbosities.Num() && !FromBoot)
						{
							Value = CategoryVerbosities[0]->DefaultVerbosity;
						}
					}
					else if (CmdToken == NAME_Off)
					{
						Value &= ~ELogVerbosity::VerbosityMask;
						Value |= ELogVerbosity::Fatal;
					}
					else if (CmdToken == NAME_On)
					{
						Value &= ~ELogVerbosity::VerbosityMask;
							// try to get a non-zero value from the toggle backup
							uint8* Toggle = ToggleAssociations.Find(Category);
							if (Toggle && *Toggle)
							{
								Value |= *Toggle;
							}
							else
							{
								Value |= ELogVerbosity::All;
							}
					}
					else if (CmdToken == NAME_Break)
					{
						Value ^= ELogVerbosity::BreakOnLog;
					}
				}
			}
			if (Category != NAME_Reset)
			{
				if (FromBoot)
				{
					if (Category == NAME_BootGlobal)
					{
						// changing the global at boot removes everything set up so far
						BootAssociations.Empty();
					}
					BootAssociations.Add(Category, Value);
				}
				else
				{
					for (int32 CategoryIndex = 0; CategoryIndex < CategoryVerbosities.Num(); CategoryIndex++)
					{
						FLogCategoryBase* Verb = CategoryVerbosities[CategoryIndex];
						Verb->SetVerbosity(ELogVerbosity::Type(Value));
					}
					if (Category == NAME_Global)
					{
						// if this was a global change, we need to change them all
						ApplyGlobalChanges();
					}
				}
				// store off the last non-zero one for toggle
				if (Value & ELogVerbosity::VerbosityMask)
				{
					// currently on, store this in the pending and clear it
					ToggleAssociations.Add(Category, Value & ELogVerbosity::VerbosityMask);
				}
			}
		}
	}
	/** Called after a change is made to the global verbosity...Iterates over all logging categories and adjusts them accordingly **/
	void ApplyGlobalChanges()
	{
		static uint8 LastGlobalVerbosity = ELogVerbosity::All;
		bool bVerbosityGoingUp = GlobalVerbosity.Verbosity > LastGlobalVerbosity;
		bool bVerbosityGoingDown = GlobalVerbosity.Verbosity < LastGlobalVerbosity;
		checkSlow(!(GlobalVerbosity.Verbosity & ELogVerbosity::BreakOnLog)); // this bit is factored out of this variable, always
		LastGlobalVerbosity = GlobalVerbosity.Verbosity;

		static bool bOldGlobalBreakValue = false;
		bool bForceBreak = (!GlobalVerbosity.DebugBreakOnLog) != !bOldGlobalBreakValue;
		bOldGlobalBreakValue = GlobalVerbosity.DebugBreakOnLog;
		for (TMap<FLogCategoryBase*, FName>::TIterator It(Associations); It; ++It)
		{
			FLogCategoryBase* Verb = It.Key();
			uint8 NewVerbosity = Verb->Verbosity;
			checkSlow(!(NewVerbosity & ELogVerbosity::BreakOnLog)); // this bit is factored out of this variable, always

			if (bVerbosityGoingDown)
			{
				NewVerbosity = FMath::Min<uint8>(GlobalVerbosity.Verbosity, Verb->Verbosity);
			}
			if (bVerbosityGoingUp)
			{
				NewVerbosity = FMath::Max<uint8>(GlobalVerbosity.Verbosity, Verb->Verbosity);
				NewVerbosity = FMath::Min<uint8>(Verb->CompileTimeVerbosity, NewVerbosity);
			}
			// store off the last non-zero one for toggle
			if (NewVerbosity)
			{
				// currently on, store this in the toggle for future use
				ToggleAssociations.Add(It.Value(), NewVerbosity);
			}
			Verb->Verbosity = NewVerbosity;
			if (bForceBreak)
			{
				Verb->DebugBreakOnLog = GlobalVerbosity.DebugBreakOnLog;
			}
			checkSlow(!(Verb->Verbosity & ELogVerbosity::BreakOnLog)); // this bit is factored out of this variable, always
		}
	}

	/** 
	 * Called twice typically. Once when a log category is constructed, and then once after we have processed the command line.
	 * The second call is needed to make sure the default is set correctly when it is changed on the command line or config file.
	 **/
	void SetupSuppress(FLogCategoryBase* Destination, FName NameFName)
	{
		// now maybe this was set at boot, in which case we override what it had
		uint8* Boot = BootAssociations.Find(NameFName);
		if (Boot)
		{
			Destination->DefaultVerbosity = *Boot;
			Destination->ResetFromDefault();
		}
		else
		{
			// see if we had a boot global override
			static FName NAME_BootGlobal(TEXT("BootGlobal"));
			Boot = BootAssociations.Find(NAME_BootGlobal);
			if (Boot)
			{
				Destination->DefaultVerbosity = *Boot;
				Destination->ResetFromDefault();
			}
		}
		// store off the last non-zero one for toggle
		checkSlow(!(Destination->Verbosity & ELogVerbosity::BreakOnLog)); // this bit is factored out of this variable, always
		if (Destination->Verbosity)
		{
			// currently on, store this in the pending and clear it
			ToggleAssociations.Add(NameFName, Destination->Verbosity);
		}
	}

public:

	virtual void AssociateSuppress(FLogCategoryBase* Destination)
	{
		FName NameFName(Destination->CategoryFName);
		check(Destination);
		check(!Associations.Find(Destination)); // should not have this address already registered
		Associations.Add(Destination, NameFName);
		bool bFoundExisting = false;
		for (TMultiMap<FName, FLogCategoryBase*>::TKeyIterator It(ReverseAssociations, NameFName); It; ++It)
		{
			if (It.Value() == Destination)
			{
				UE_LOG(LogHAL, Fatal,TEXT("Log suppression category %s was somehow declared twice with the same data."), *NameFName.ToString());
			}
			// if it is registered, it better be the same
			if (It.Value()->CompileTimeVerbosity != Destination->CompileTimeVerbosity)
			{
				UE_LOG(LogHAL, Fatal,TEXT("Log suppression category %s is defined multiple times with different compile time verbosity."), *NameFName.ToString());
			}
			// we take whatever the existing one has to keep them in sync always
			checkSlow(!(It.Value()->Verbosity & ELogVerbosity::BreakOnLog)); // this bit is factored out of this variable, always
			Destination->Verbosity = It.Value()->Verbosity;
			Destination->DebugBreakOnLog = It.Value()->DebugBreakOnLog;
			Destination->DefaultVerbosity = It.Value()->DefaultVerbosity;
			bFoundExisting = true;
		}
		ReverseAssociations.Add(NameFName, Destination);
		if (bFoundExisting)
		{
			return; // in no case is there anything more to do...we want to match the other ones
		}
		SetupSuppress(Destination, NameFName); // this might be done again later if this is being set up before appInit is called
	}
	virtual void DisassociateSuppress(FLogCategoryBase* Destination)
	{
		FName* Name = Associations.Find(Destination);
		if (Name)
		{
			verify(ReverseAssociations.Remove(*Name, Destination)==1);
			verify(Associations.Remove(Destination) == 1);
		}
	}

	virtual void ProcessConfigAndCommandLine()
	{
		// first we do the config values
		FConfigSection* RefTypes = GConfig->GetSectionPrivate(TEXT("Core.Log"), false, true, GEngineIni);
		if (RefTypes != NULL)
		{
			for( FConfigSectionMap::TIterator It(*RefTypes); It; ++It )
			{
				ProcessCmdString(It.Key().ToString() + TEXT(" ") + It.Value().GetValue(), true);
			}
		}
#if !UE_BUILD_SHIPPING
		// and the command line overrides the config values
		FString CmdLine(FCommandLine::Get());
		FString LogCmds(TEXT("-LogCmds="));
		int32 IndexOfEnv = CmdLine.Find(TEXT("-EnvAfterHere"));
		if (IndexOfEnv != INDEX_NONE)
		{
			// if we have env variable stuff set on the command line, we want to process that FIRST
			FString CmdLineEnv = CmdLine.Mid(IndexOfEnv);
			while (1)
			{
				FString Cmds;
				if (!FParse::Value(*CmdLineEnv, *LogCmds, Cmds, false))
				{
					break;
				}
				ProcessCmdString(Cmds, true);
				// remove this command so that we can try for other ones...for example one on the command line and one coming from env vars
				int32 Index = CmdLineEnv.Find(*LogCmds);
				ensure(Index >= 0);
				if (Index == INDEX_NONE)
				{
					break;
				}
				CmdLineEnv = CmdLineEnv.Mid(Index + LogCmds.Len());
			}
			// now strip off the environment arg part
			CmdLine = CmdLine.Mid(0, IndexOfEnv);
		}
		while (1)
		{
			FString Cmds;
			if (!FParse::Value(*CmdLine, *LogCmds, Cmds, false))
			{
				break;
			}
			ProcessCmdString(Cmds, true);
			// remove this command so that we can try for other ones...for example one on the command line and one coming from env vars
			int32 Index = CmdLine.Find(*LogCmds);
			ensure(Index >= 0);
			if (Index == INDEX_NONE)
			{
				break;
			}
			CmdLine = CmdLine.Mid(Index + LogCmds.Len());
		}
#endif // !UE_BUILD_SHIPPING

		// and then the compiled in defaults are overridden with those
		for (TMultiMap<FName, FLogCategoryBase*>::TIterator It(ReverseAssociations); It; ++It)
		{
			SetupSuppress(It.Value(), It.Key());
		}
	}

	/** Console commands, see embeded usage statement **/
	virtual bool Exec( UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar )
	{
		if(FParse::Command(&Cmd,TEXT("LOG")))
		{
			if(FParse::Command(&Cmd,TEXT("LIST"))) // if they didn't use the list command, we will show usage
			{
				TArray<FLogCategoryPtrs> Found;

				FString Cat(FParse::Token(Cmd, 0));
				for (TMap<FLogCategoryBase*, FName>::TIterator It(Associations); It; ++It)
				{
					FLogCategoryBase* Verb = It.Key();
					FString Name = It.Value().ToString();
					if (!Cat.Len() || Name.Contains(Cat) )
					{
						Found.Add(FLogCategoryPtrs(Name, ELogVerbosity::Type(Verb->Verbosity), Verb->DebugBreakOnLog));
					}
				}

				Found.Sort();

				for (TArray<FLogCategoryPtrs>::TConstIterator It = Found.CreateConstIterator(); It; ++It)
				{
					Ar.Logf(TEXT("%-40s  %-12s  %s"), *It->Name, FOutputDeviceHelper::VerbosityToString(It->Verbosity), It->Postfix ? TEXT(" - DebugBreak") : TEXT(""));
				}
			}
			else
			{
				FString Rest(Cmd);
				Rest = Rest.Trim();
				if (Rest.Len())
				{
					if (ProcessLogOnly(Rest, Ar))
					{
						return true;
					}

					TMap<FName, uint8> OldValues;
					for (TMap<FLogCategoryBase*, FName>::TIterator It(Associations); It; ++It)
					{
						FLogCategoryBase* Verb = It.Key();
						FName Name = It.Value();
						OldValues.Add(Name, Verb->Verbosity);
					}
					ProcessCmdString(Rest);
					for (TMap<FLogCategoryBase*, FName>::TIterator It(Associations); It; ++It)
					{
						FLogCategoryBase* Verb = It.Key();
						FName Name = It.Value();
						uint8 OldValue = OldValues.FindRef(Name);
						if (Verb->Verbosity != OldValue)
						{
							Ar.Logf(TEXT("%-40s  %-12s  %s"), *Name.ToString(), FOutputDeviceHelper::VerbosityToString(ELogVerbosity::Type(Verb->Verbosity)), Verb->DebugBreakOnLog ? TEXT(" - DebugBreak") : TEXT(""));
						}
					}
				}
				else
				{
					Ar.Logf( TEXT("------- Log conventions") );
					Ar.Logf( TEXT("[cat]   = a category for the command to operate on, or 'global' for all categories.") );
					Ar.Logf( TEXT("[level] = verbosity level, one of: none, error, warning, display, log, verbose, all, default") );
					Ar.Logf( TEXT("At boot time, compiled in default is overridden by ini files setting, which is overridden by command line") );
					Ar.Logf( TEXT("------- Log console command usage") );
					Ar.Logf( TEXT("Log list            - list all log categories") );
					Ar.Logf( TEXT("Log list [string]   - list all log categories containing a substring") );
					Ar.Logf( TEXT("Log reset           - reset all log categories to their boot-time default") );
					Ar.Logf( TEXT("Log [cat]           - toggle the display of the category [cat]") );
					Ar.Logf( TEXT("Log [cat] off       - disable display of the category [cat]") );
					Ar.Logf( TEXT("Log [cat] on        - resume display of the category [cat]") );
					Ar.Logf( TEXT("Log [cat] only      - enables [cat] and disables all other categories"));
					Ar.Logf( TEXT("Log [cat] [level]   - set the verbosity level of the category [cat]") );
					Ar.Logf( TEXT("Log [cat] break     - toggle the debug break on display of the category [cat]") );
					Ar.Logf( TEXT("------- Log command line") );
					Ar.Logf( TEXT("-LogCmds=\"[arguments],[arguments]...\"           - applies a list of console commands at boot time") );
					Ar.Logf( TEXT("-LogCmds=\"foo verbose, bar off\"         - turns on the foo category and turns off the bar category") );					
					Ar.Logf( TEXT("------- Environment variables") );
					Ar.Logf( TEXT("Any command line option can be set via the environment variable UE-CmdLineArgs") );
					Ar.Logf( TEXT("set UE-CmdLineArgs=\"-LogCmds=foo verbose breakon, bar off\"") );
					Ar.Logf( TEXT("------- Config file") );
					Ar.Logf( TEXT("[Core.Log]") );
					Ar.Logf( TEXT("global=[default verbosity for things not listed later]") );					
					Ar.Logf( TEXT("[cat]=[level]") );					
					Ar.Logf( TEXT("foo=verbose break") );					
				}
			}
			return true;
		}
		return false;
	}
};

FLogSuppressionInterface& FLogSuppressionInterface::Get()
{
	static FLogSuppressionImplementation* Singleton = NULL;
	if (!Singleton)
	{
		Singleton = new FLogSuppressionImplementation;
	}
	return *Singleton;
}


FLogCategoryBase::FLogCategoryBase(const TCHAR *CategoryName, ELogVerbosity::Type InDefaultVerbosity, ELogVerbosity::Type InCompileTimeVerbosity)
	: DefaultVerbosity(InDefaultVerbosity)
	, CompileTimeVerbosity(InCompileTimeVerbosity)
	, CategoryFName(CategoryName)
{
	ResetFromDefault();
	if (CompileTimeVerbosity > ELogVerbosity::NoLogging)
	{
		FLogSuppressionInterface::Get().AssociateSuppress(this);
	}
	checkSlow(!(Verbosity & ELogVerbosity::BreakOnLog)); // this bit is factored out of this variable, always
}

FLogCategoryBase::~FLogCategoryBase()
{
	checkSlow(!(Verbosity & ELogVerbosity::BreakOnLog)); // this bit is factored out of this variable, always
	if (CompileTimeVerbosity > ELogVerbosity::NoLogging)
	{
		FLogSuppressionInterface::Get().DisassociateSuppress(this);
	}
}

void FLogCategoryBase::SetVerbosity(ELogVerbosity::Type NewVerbosity)
{
	// regularize the verbosity to be at most whatever we were compiled with
	Verbosity = FMath::Min<uint8>(CompileTimeVerbosity, (NewVerbosity & ELogVerbosity::VerbosityMask));
	DebugBreakOnLog = !!(NewVerbosity & ELogVerbosity::BreakOnLog);
	checkSlow(!(Verbosity & ELogVerbosity::BreakOnLog)); // this bit is factored out of this variable, always
}

void FLogCategoryBase::ResetFromDefault()
{
	// regularize the default verbosity to be at most whatever we were compiled with
	SetVerbosity(ELogVerbosity::Type(DefaultVerbosity));
}


void FLogCategoryBase::PostTrigger(ELogVerbosity::Type VerbosityLevel)
{
	checkSlow(!(Verbosity & ELogVerbosity::BreakOnLog)); // this bit is factored out of this variable, always
	check(VerbosityLevel <= CompileTimeVerbosity); // we should have never gotten here, the compile-time version should ALWAYS be checked first
	if (DebugBreakOnLog || (VerbosityLevel & ELogVerbosity::BreakOnLog))  // we break if either the suppression level on this message is set to break or this log statement is set to break
	{
		GLog->FlushThreadedLogs();
		DebugBreakOnLog = false; // toggle this off automatically
		FPlatformMisc::DebugBreak();
	}
}

FScopedCategoryAndVerbosityOverride::FScopedCategoryAndVerbosityOverride(FName Category, ELogVerbosity::Type Verbosity)
{
	FOverride* TLS = GetTLSCurrent();
	Backup = *TLS;
	*TLS = FOverride(Category, Verbosity);
}

FScopedCategoryAndVerbosityOverride::~FScopedCategoryAndVerbosityOverride()
{
	FOverride* TLS = GetTLSCurrent();
	*TLS = Backup;
}

static uint32 OverrrideTLSID = FPlatformTLS::AllocTlsSlot();
FScopedCategoryAndVerbosityOverride::FOverride* FScopedCategoryAndVerbosityOverride::GetTLSCurrent()
{
	FOverride* TLS = (FOverride*)FPlatformTLS::GetTlsValue(OverrrideTLSID);
	if (!TLS)
	{
		TLS = new FOverride;
		FPlatformTLS::SetTlsValue(OverrrideTLSID, TLS);
	}	
	return TLS;
}


/** Back up the existing verbosity for the category then sets new verbosity.*/
FLogScopedVerbosityOverride::FLogScopedVerbosityOverride(FLogCategoryBase * Category, ELogVerbosity::Type Verbosity)
	:	SavedCategory(Category)
{
	check (SavedCategory);
	SavedVerbosity = SavedCategory->GetVerbosity();
	SavedCategory->SetVerbosity(Verbosity);
}

/** Restore the verbosity overrides for the category to the previous value.*/
FLogScopedVerbosityOverride::~FLogScopedVerbosityOverride()
{
	SavedCategory->SetVerbosity(SavedVerbosity);
}

/*-----------------------------------------------------------------------------
	FOutputDeviceRedirector.
-----------------------------------------------------------------------------*/

/** Initialization constructor. */
FOutputDeviceRedirector::FOutputDeviceRedirector()
:	MasterThreadID(FPlatformTLS::GetCurrentThreadId())
,	bEnableBacklog(false)
{
}

FOutputDeviceRedirector* FOutputDeviceRedirector::Get()
{
	static FOutputDeviceRedirector Singleton;
	return &Singleton;
}

/**
 * Adds an output device to the chain of redirections.	
 *
 * @param OutputDevice	output device to add
 */
void FOutputDeviceRedirector::AddOutputDevice( FOutputDevice* OutputDevice )
{
	FScopeLock ScopeLock( &SynchronizationObject );

	if( OutputDevice )
	{
		OutputDevices.AddUnique( OutputDevice );
	}
}

/**
 * Removes an output device from the chain of redirections.	
 *
 * @param OutputDevice	output device to remove
 */
void FOutputDeviceRedirector::RemoveOutputDevice( FOutputDevice* OutputDevice )
{
	FScopeLock ScopeLock( &SynchronizationObject );
	OutputDevices.Remove( OutputDevice );
}

/**
 * Returns whether an output device is currently in the list of redirectors.
 *
 * @param	OutputDevice	output device to check the list against
 * @return	true if messages are currently redirected to the the passed in output device, false otherwise
 */
bool FOutputDeviceRedirector::IsRedirectingTo( FOutputDevice* OutputDevice )
{
	FScopeLock ScopeLock( &SynchronizationObject );

	return OutputDevices.Find( OutputDevice ) == INDEX_NONE ? false : true;
}

/**
 * The unsynchronized version of FlushThreadedLogs.
 * Assumes that the caller holds a lock on SynchronizationObject.
 */
void FOutputDeviceRedirector::UnsynchronizedFlushThreadedLogs( bool bUseAllDevices )
{
	for(int32 LineIndex = 0;LineIndex < BufferedLines.Num();LineIndex++)
	{
		FBufferedLine BufferedLine = BufferedLines[LineIndex];

		for( int32 OutputDeviceIndex=0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++ )
		{
			FOutputDevice* OutputDevice = OutputDevices[OutputDeviceIndex];
			if( OutputDevice->CanBeUsedOnAnyThread() || bUseAllDevices )
			{
				OutputDevice->Serialize( *BufferedLine.Data, BufferedLine.Verbosity, BufferedLine.Category, BufferedLine.Time );
			}
		}
	}

	BufferedLines.Empty();
}

/**
 * Flushes lines buffered by secondary threads.
 */
void FOutputDeviceRedirector::FlushThreadedLogs()
{
	SCOPE_CYCLE_COUNTER(STAT_FlushThreadedLogs);
	// Acquire a lock on SynchronizationObject and call the unsynchronized worker function.
	FScopeLock ScopeLock( &SynchronizationObject );
	check(IsInGameThread());
	UnsynchronizedFlushThreadedLogs( true );
}

void FOutputDeviceRedirector::PanicFlushThreadedLogs()
{
//	SCOPE_CYCLE_COUNTER(STAT_FlushThreadedLogs);
	// Acquire a lock on SynchronizationObject and call the unsynchronized worker function.
	FScopeLock ScopeLock( &SynchronizationObject );
	
	// Flush threaded logs, but use the safe version.
	UnsynchronizedFlushThreadedLogs( false );

	// Flush devices.
	for (int32 OutputDeviceIndex = 0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++)
	{
		FOutputDevice* OutputDevice = OutputDevices[OutputDeviceIndex];
		if (OutputDevice->CanBeUsedOnAnyThread())
		{
			OutputDevice->Flush();
		}
	}

	BufferedLines.Empty();
}

/**
 * Serializes the current backlog to the specified output device.
 * @param OutputDevice	- Output device that will receive the current backlog
 */
void FOutputDeviceRedirector::SerializeBacklog( FOutputDevice* OutputDevice )
{
	FScopeLock ScopeLock( &SynchronizationObject );

	for (int32 LineIndex = 0; LineIndex < BacklogLines.Num(); LineIndex++)
	{
		const FBufferedLine& BacklogLine = BacklogLines[ LineIndex ];
		OutputDevice->Serialize( *BacklogLine.Data, BacklogLine.Verbosity, BacklogLine.Category, BacklogLine.Time );
	}
}

/**
 * Enables or disables the backlog.
 * @param bEnable	- Starts saving a backlog if true, disables and discards any backlog if false
 */
void FOutputDeviceRedirector::EnableBacklog( bool bEnable )
{
	FScopeLock ScopeLock( &SynchronizationObject );

	bEnableBacklog = bEnable;
	if ( bEnableBacklog == false )
	{
		BacklogLines.Empty();
	}
}

/**
 * Sets the current thread to be the master thread that prints directly
 * (isn't queued up)
 */
void FOutputDeviceRedirector::SetCurrentThreadAsMasterThread()
{
	FScopeLock ScopeLock( &SynchronizationObject );

	// make sure anything queued up is flushed out, this may be called from a background thread, so use the safe version.
	UnsynchronizedFlushThreadedLogs( false );

	// set the current thread as the master thread
	MasterThreadID = FPlatformTLS::GetCurrentThreadId();
}

void FOutputDeviceRedirector::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time )
{
	const double RealTime = Time == -1.0f ? FPlatformTime::Seconds() - GStartTime : Time;

	FScopeLock ScopeLock( &SynchronizationObject );

#if PLATFORM_DESKTOP
	// this is for errors which occur after shutdown we might be able to salvage information from stdout 
	if ((OutputDevices.Num() == 0)&& GIsRequestingExit)
	{
#if PLATFORM_WINDOWS
		_tprintf(_T("%s\n"), Data);
#else
		FGenericPlatformMisc::LocalPrint(Data);
		// printf("%s\n", TCHAR_TO_ANSI(Data));
#endif
		return;
	}
#endif


	if ( bEnableBacklog )
	{
		new(BacklogLines)FBufferedLine( Data, Category, Verbosity, RealTime );
	}

	if(FPlatformTLS::GetCurrentThreadId() != MasterThreadID || OutputDevices.Num() == 0)
	{
		new(BufferedLines)FBufferedLine( Data, Category, Verbosity, RealTime );
	}
	else
	{
		// Flush previously buffered lines from secondary threads.
		// Since we already hold a lock on SynchronizationObject, call the unsynchronized version.
		UnsynchronizedFlushThreadedLogs( true );

		for( int32 OutputDeviceIndex=0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++ )
		{
			OutputDevices[OutputDeviceIndex]->Serialize( Data, Verbosity, Category, RealTime );
		}
	}
}

void FOutputDeviceRedirector::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	Serialize( Data, Verbosity, Category, -1.0 );
}

/**
 * Passes on the flush request to all current output devices.
 */
void FOutputDeviceRedirector::Flush()
{
	if(FPlatformTLS::GetCurrentThreadId() == MasterThreadID)
	{
		FScopeLock ScopeLock( &SynchronizationObject );

		// Flush previously buffered lines from secondary threads.
		// Since we already hold a lock on SynchronizationObject, call the unsynchronized version.
		UnsynchronizedFlushThreadedLogs( true );

		for( int32 OutputDeviceIndex=0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++ )
		{
			OutputDevices[OutputDeviceIndex]->Flush();
		}
	}
}

/**
 * Closes output device and cleans up. This can't happen in the destructor
 * as we might have to call "delete" which cannot be done for static/ global
 * objects.
 */
void FOutputDeviceRedirector::TearDown()
{
	check(FPlatformTLS::GetCurrentThreadId() == MasterThreadID);

	FScopeLock ScopeLock( &SynchronizationObject );

	// Flush previously buffered lines from secondary threads.
	// Since we already hold a lock on SynchronizationObject, call the unsynchronized version.
	UnsynchronizedFlushThreadedLogs( false );

	for( int32 OutputDeviceIndex=0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++ )
	{
		OutputDevices[OutputDeviceIndex]->TearDown();
	}
	OutputDevices.Empty();
}


/*-----------------------------------------------------------------------------
	FOutputDevice subclasses.
-----------------------------------------------------------------------------*/

/**
* Thread heartbeat check class.
* Used by crash handling code to check for hangs.
*/
class CORE_API FAsyncWriter : public FRunnable, public FArchive
{
	enum EConstants
	{
		InitialBufferSize = 128 * 1024
	};

	/** Thread to run the worker FRunnable on. Serializes the ring buffer to disk. */
	FRunnableThread* Thread;
	/** Stops this thread */
	FThreadSafeCounter StopTaskCounter;

	/** Writer archive */
	FArchive& Ar;
	/** Data ring buffer */
	TArray<uint8> Buffer;
	/** Position where the unserialized data starts in the buffer */
	int32 BufferStartPos;
	/** Position where the unserialized data ends in the buffer (such as if (BufferEndPos > BufferStartPos) Length = BufferEndPos - BufferStartPos; */
	int32 BufferEndPos;
	/** Sync object for the buffer pos */
	FCriticalSection BufferPosCritical;
	/** Event to let the worker thread know there is data to save to disk. */
	FEvent* BufferDirtyEvent;
	/** Event flagged when the worker thread finished processing data */
	FEvent* BufferEmptyEvent;

	/** Serialize the contents of the ring buffer to disk */
	void SerializeBufferToArchive()
	{
		// Grab a local copy of the end pos. It's ok if it changes on the client thread later on.
		// We won't be modifying it anyway and will later serialize new data in the next iteration.
		// Here we only serialize what we know exists at the beginning of this function.
		int32 ThisThreadEndPos = BufferEndPos;
		if (ThisThreadEndPos >= BufferStartPos)
		{
			Ar.Serialize(Buffer.GetData() + BufferStartPos, ThisThreadEndPos - BufferStartPos);
		}
		else
		{
			// Data is wrapped around the ring buffer
			Ar.Serialize(Buffer.GetData() + BufferStartPos, Buffer.Num() - BufferStartPos);
			Ar.Serialize(Buffer.GetData(), BufferEndPos);
		}
		// Modify the start pos. Only the worker thread modifies this value so it's ok to not guard it with a critical section.
		BufferStartPos = ThisThreadEndPos;
		// Let the client threads know we're done (this is used by FlushBuffer).
		BufferEmptyEvent->Trigger();
	}

	/** Flush the memory buffer (doesn't force the archive to flush) */
	void FlushBuffer()
	{
		BufferDirtyEvent->Trigger();
		if (!Thread)
		{
			SerializeBufferToArchive();
		}
		BufferEmptyEvent->Wait();
	}

public:

	FAsyncWriter(FArchive& InAr)
		: Thread(nullptr)
		, Ar(InAr)
		, BufferStartPos(0)
		, BufferEndPos(0)
	{
		Buffer.AddUninitialized(InitialBufferSize);
		BufferDirtyEvent = FPlatformProcess::GetSynchEventFromPool();
		BufferEmptyEvent = FPlatformProcess::GetSynchEventFromPool(true);
		BufferEmptyEvent->Trigger();

		if (FPlatformProcess::SupportsMultithreading())
		{
			FString WriterName = FString::Printf(TEXT("FAsyncWriter_%s"), *FPaths::GetBaseFilename(Ar.GetArchiveName()));
			Thread = FRunnableThread::Create(this, *WriterName, 0, TPri_BelowNormal);
		}
	}

	virtual ~FAsyncWriter()
	{
		Flush();
		delete Thread;
		Thread = nullptr;
		FPlatformProcess::ReturnSynchEventToPool(BufferDirtyEvent);
		BufferDirtyEvent = nullptr;
		FPlatformProcess::ReturnSynchEventToPool(BufferEmptyEvent);
		BufferEmptyEvent = nullptr;
	}

	/** Serialize data to buffer that will later be saved to disk by the async thread */
	virtual void Serialize(void* InData, int64 Length) override
	{
		if (!InData || Length <= 0)
		{
			return;
		}

		const uint8* Data = (uint8*)InData;

		BufferEmptyEvent->Reset();
		FScopeLock WriteLock(&BufferPosCritical);

		// Store the local copy of the current buffer start pos. It may get moved by the worker thread but we don't
		// care about it too much because we only modify BufferEndPos. Copy should be atomic enough. We only use it
		// for checking the remaining space in the buffer so underestimating is ok.
		{
			const int32 ThisThreadStartPos = BufferStartPos;
			// Calculate the remaining size in the ring buffer
			const int32 BufferFreeSize = ThisThreadStartPos <= BufferEndPos ? (Buffer.Num() - BufferEndPos + ThisThreadStartPos) : (ThisThreadStartPos - BufferEndPos);
			// Make sure the buffer is BIGGER than we require otherwise we may calculate the wrong (0) buffer EndPos for StartPos = 0 and Length = Buffer.Num()
			if (BufferFreeSize <= Length)
			{
				// Force the async thread to call SerializeBufferToArchive even if it's currently empty
				BufferDirtyEvent->Trigger();
				FlushBuffer();

				// Resize the buffer if needed
				if (Length >= Buffer.Num())
				{
					// Keep the buffer bigger than we require so that % Buffer.Num() does not return 0 for Lengths = Buffer.Num()
					Buffer.SetNumUninitialized(Length + 1);
				}
			}
		}

		// We now know there's enough space in the buffer to copy data
		const int32 WritePos = BufferEndPos;
		if ((WritePos + Length) <= Buffer.Num())
		{
			// Copy straight into the ring buffer
			FMemory::Memcpy(Buffer.GetData() + WritePos, Data, Length);
		}
		else
		{
			// Wrap around the ring buffer
			int32 BufferSizeToEnd = Buffer.Num() - WritePos;
			FMemory::Memcpy(Buffer.GetData() + WritePos, Data, BufferSizeToEnd);
			FMemory::Memcpy(Buffer.GetData(), Data + BufferSizeToEnd, Length - BufferSizeToEnd);
		}

		// Update the end position and let the async thread know we need to write to disk
		BufferEndPos = (BufferEndPos + Length) % Buffer.Num();
		BufferDirtyEvent->Trigger();

		// No async thread? Serialize now.
		if (!Thread)
		{
			SerializeBufferToArchive();
		}
	}

	/** Flush all buffers to disk */
	void Flush()
	{
		FScopeLock WriteLock(&BufferPosCritical);
		FlushBuffer();
		Ar.Flush();
	}

	//~ Begin FRunnable Interface.
	virtual bool Init() 
	{
		return true;
	}
	virtual uint32 Run()
	{
		while (StopTaskCounter.GetValue() == 0)
		{
			if (BufferDirtyEvent->Wait(10))
			{
				SerializeBufferToArchive();
			}
		}
		return 0;
	}
	virtual void Stop()
	{
		StopTaskCounter.Increment();
	}
	//~ End FRunnable Interface
};

/** 
 * Constructor, initializing member variables.
 *
 * @param InFilename		Filename to use, can be NULL
 * @param bInDisableBackup	If true, existing files will not be backed up
 */
FOutputDeviceFile::FOutputDeviceFile( const TCHAR* InFilename, bool bInDisableBackup  )
: AsyncWriter(nullptr)
, WriterArchive(nullptr)
, Opened(false)
, Dead(false)
, bDisableBackup(bInDisableBackup)
{
	if( InFilename )
	{
		FCString::Strncpy( Filename, InFilename, ARRAY_COUNT(Filename) );
	}
	else
	{
		Filename[0]	= 0;
	}
}

void FOutputDeviceFile::SetFilename(const TCHAR* InFilename)
{
	// Close any existing file.
	TearDown();

	FCString::Strncpy( Filename, InFilename, ARRAY_COUNT(Filename) );
}

/**
 * Closes output device and cleans up. This can't happen in the destructor
 * as we have to call "delete" which cannot be done for static/ global
 * objects.
 */
void FOutputDeviceFile::TearDown()
{
	if (AsyncWriter)
	{
		if (!bSuppressEventTag)
		{			
			Logf(TEXT("Log file closed, %s"), FPlatformTime::StrTimestamp());
		}
		delete AsyncWriter;
		AsyncWriter = nullptr;
	}
	delete WriterArchive;
	WriterArchive = nullptr;
}

/**
 * Flush the write cache so the file isn't truncated in case we crash right
 * after calling this function.
 */
void FOutputDeviceFile::Flush()
{
	if (AsyncWriter)
	{
		AsyncWriter->Flush();
	}
}

/** if the passed in file exists, makes a timestamped backup copy
 * @param Filename the name of the file to check
 */
void FOutputDeviceFile::CreateBackupCopy(const TCHAR* Filename)
{
	if (IFileManager::Get().FileSize(Filename) > 0)
	{
		FString SystemTime = FDateTime::Now().ToString();
		FString Name, Extension;
		FString(Filename).Split(TEXT("."), &Name, &Extension, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		FString BackupFilename = FString::Printf(TEXT("%s%s%s.%s"), *Name, BACKUP_LOG_FILENAME_POSTFIX, *SystemTime, *Extension);
		IFileManager::Get().Copy(*BackupFilename, Filename, false);
	}
}

bool FOutputDeviceFile::IsBackupCopy(const TCHAR* Filename)
{
	return Filename != nullptr && FCString::Stristr(const_cast<TCHAR*>(Filename), BACKUP_LOG_FILENAME_POSTFIX) != nullptr;
}

void FOutputDeviceFile::WriteByteOrderMarkToArchive(EByteOrderMark ByteOrderMark)
{
	switch (ByteOrderMark)
	{
	case EByteOrderMark::UTF8:
		AsyncWriter->Serialize(UTF8BOM, sizeof(UTF8BOM));
		break;

	case EByteOrderMark::Unspecified:
	default:
		check(false);
		break;
	}
}

bool FOutputDeviceFile::CreateWriter(uint32 MaxAttempts)
{
	uint32 WriteFlags = FILEWRITE_AllowRead | (Opened ? FILEWRITE_Append : 0);

	// Open log file.
	FArchive* Ar = IFileManager::Get().CreateFileWriter(Filename, WriteFlags);

	// If that failed, append an _2 and try again (unless we don't want extra copies). This 
	// happens in the case of running a server and client on same computer for example.
	if (!bDisableBackup && !Ar)
	{
		FString FilenamePart = FPaths::GetBaseFilename(Filename, false) + "_";
		FString ExtensionPart = FPaths::GetExtension(Filename, true);
		FString FinalFilename;
		uint32 FileIndex = 2;
		do
		{
			// Continue to increment indices until a valid filename is found
			FinalFilename = FilenamePart + FString::FromInt(FileIndex++) + ExtensionPart;
			if (!Opened)
			{
				CreateBackupCopy(*FinalFilename);
			}
			Ar = IFileManager::Get().CreateFileWriter(*FinalFilename, WriteFlags);
		} while (!Ar && FileIndex < MaxAttempts);
	}

	FAsyncWriter* Result = nullptr;
	if (Ar)
	{
		WriterArchive = Ar;
		AsyncWriter = new FAsyncWriter(*WriterArchive);
	}

	return !!AsyncWriter;
}

/**
 * Serializes the passed in data unless the current event is suppressed.
 *
 * @param	Data	Text to log
 * @param	Event	Event name used for suppression purposes
 */
void FOutputDeviceFile::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time )
{
#if ALLOW_LOG_FILE && !NO_LOGGING
	static bool Entry = false;
	if( !GIsCriticalError || Entry )
	{
		if (!AsyncWriter && !Dead)
		{
			// Make log filename.
			if( !Filename[0] )
			{
				FCString::Strcpy(Filename, *FPlatformOutputDevices::GetAbsoluteLogFilename());
			}

			// if the file already exists, create a backup as we are going to overwrite it
			if (!bDisableBackup && !Opened)
			{
				CreateBackupCopy(Filename);
			}

			// Open log file and create the worker thread.
			if (CreateWriter())
			{
				Opened = true;

				WriteByteOrderMarkToArchive(EByteOrderMark::UTF8);

				if (!bSuppressEventTag)
				{
					Logf( TEXT("Log file open, %s"), FPlatformTime::StrTimestamp() );
				}
			}
			else 
			{
				Dead = true;
			}
		}

		if (AsyncWriter && Verbosity != ELogVerbosity::SetColor)
		{
			FOutputDeviceHelper::FormatCastAndSerializeLine(*AsyncWriter, Data, Verbosity, Category, Time, bSuppressEventTag, bAutoEmitLineTerminator);

			static bool GForceLogFlush = false;
			static bool GTestedCmdLine = false;
			if (!GTestedCmdLine)
			{
				GTestedCmdLine = true;
				// Force a log flush after each line
				GForceLogFlush = FParse::Param( FCommandLine::Get(), TEXT("FORCELOGFLUSH") );
			}
			if (GForceLogFlush)
			{
				AsyncWriter->Flush();
			}
		}
	}
	else
	{
		Entry = true;
		Serialize(Data, Verbosity, Category, Time);
		Entry = false;
	}
#endif
}

void FOutputDeviceFile::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	Serialize(Data, Verbosity, Category, -1.0);
}


void FOutputDeviceFile::WriteRaw( const TCHAR* C )
{
	AsyncWriter->Serialize((uint8*)const_cast<TCHAR*>(C), FCString::Strlen(C)*sizeof(TCHAR));
}

/**
 * Serializes the passed in data unless the current event is suppressed.
 *
 * @param	Data	Text to log
 * @param	Event	Event name used for suppression purposes
 */
void FOutputDeviceDebug::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time )
{
	static bool Entry=false;
	if( !GIsCriticalError || Entry )
	{
		if (Verbosity != ELogVerbosity::SetColor)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s%s"),*FOutputDeviceHelper::FormatLogLine(Verbosity, Category, Data, GPrintLogTimes, Time),LINE_TERMINATOR);
		}
	}
	else
	{
		Entry=true;
		Serialize( Data, Verbosity, Category );
		Entry=false;
	}
}

void FOutputDeviceDebug::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	Serialize( Data, Verbosity, Category, -1.0 );
}

/*-----------------------------------------------------------------------------
	FOutputDeviceError subclasses.
-----------------------------------------------------------------------------*/

/** Constructor, initializing member variables */
FOutputDeviceAnsiError::FOutputDeviceAnsiError()
:	ErrorPos(0)
{}

/**
 * Serializes the passed in data unless the current event is suppressed.
 *
 * @param	Data	Text to log
 * @param	Event	Event name used for suppression purposes
 */
void FOutputDeviceAnsiError::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	// Display the error and exit.
	FPlatformMisc::LocalPrint( TEXT("\nappError called: \n") );
	FPlatformMisc::LocalPrint( Msg );
	FPlatformMisc::LocalPrint( TEXT("\n") );

	if( !GIsCriticalError )
	{
		// First appError.
		GIsCriticalError = 1;
		UE_LOG(LogHAL, Error, TEXT("appError called: %s"), Msg );
		FCString::Strncpy( GErrorHist, Msg, ARRAY_COUNT(GErrorHist) );
		FCString::Strncat( GErrorHist, TEXT("\r\n\r\n"), ARRAY_COUNT(GErrorHist) );
		ErrorPos = FCString::Strlen(GErrorHist);
	}
	else
	{
		UE_LOG(LogHAL, Error, TEXT("Error reentered: %s"), Msg );
	}

	FPlatformMisc::DebugBreak();

	if( GIsGuarded )
	{
		// Propagate error so structured exception handler can perform necessary work.
#if PLATFORM_EXCEPTIONS_DISABLED
		FPlatformMisc::DebugBreak();
#endif
		FPlatformMisc::RaiseException( 1 );
	}
	else
	{
		// We crashed outside the guarded code (e.g. appExit).
		HandleError();
		// pop up a crash window if we are not in unattended mode
		if( FApp::IsUnattended() == false )
		{
			FPlatformMisc::RequestExit( true );
		}
		else
		{
			UE_LOG(LogHAL, Error, TEXT("%s"), Msg );
		}		
	}
}

/**
 * Error handling function that is being called from within the system wide global
 * error handler, e.g. using structured exception handling on the PC.
 */
void FOutputDeviceAnsiError::HandleError()
{
	GIsGuarded			= 0;
	GIsRunning			= 0;
	GIsCriticalError	= 1;
	GLogConsole			= NULL;
	GErrorHist[ARRAY_COUNT(GErrorHist)-1]=0;

	if (GLog)
	{
		// print to log and flush it
		UE_LOG( LogHAL, Log, TEXT( "=== Critical error: ===" ) LINE_TERMINATOR LINE_TERMINATOR TEXT( "%s" ) LINE_TERMINATOR, GErrorExceptionDescription );
		UE_LOG(LogHAL, Log, GErrorHist);

		GLog->Flush();
	}
	else
	{
		FPlatformMisc::LocalPrint( GErrorHist );
	}

	FPlatformMisc::LocalPrint( TEXT("\n\nExiting due to error\n") );

	FCoreDelegates::OnShutdownAfterError.Broadcast();
}

/*-----------------------------------------------------------------------------
FBufferedOutputDevice subclasses.
-----------------------------------------------------------------------------*/

void FBufferedOutputDevice::Serialize(const TCHAR* InData, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	if (Verbosity > FilterLevel)
		return;

	FScopeLock ScopeLock(&SynchronizationObject);
	new(BufferedLines)FBufferedLine(InData, Category, Verbosity);
}

void FBufferedOutputDevice::GetContents(TArray<FBufferedLine>& DestBuffer, bool ClearDevice/*=true*/)
{
	FScopeLock ScopeLock(&SynchronizationObject);
	DestBuffer = BufferedLines;

	if (ClearDevice)
	{
		BufferedLines.Empty();
	}
}


/*-----------------------------------------------------------------------------
	FOutputDeviceArchiveWrapper subclasses.
-----------------------------------------------------------------------------*/

void FOutputDeviceArchiveWrapper::Flush()
{
	LogAr->Flush();
}

void FOutputDeviceArchiveWrapper::Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	if (Verbosity != ELogVerbosity::SetColor)
	{
		FOutputDeviceHelper::FormatCastAndSerializeLine(*LogAr, Data, Verbosity, Category, -1.0, true, bAutoEmitLineTerminator);
	}
}

