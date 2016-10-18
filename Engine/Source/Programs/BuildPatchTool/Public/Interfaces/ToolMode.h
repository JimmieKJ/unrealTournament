// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

namespace BuildPatchTool
{
	enum class EReturnCode : int32;

	class IToolMode
	{
	public:
		virtual EReturnCode Execute() = 0;

		/**
		 * Helper for parsing a switch from an array of switches, usually produced using FCommandLine::Parse(..)
		 *
		 * @param InSwitch      The switch name, ending with =. E.g. option=, foo=. It would usually be a compile time const.
		 * @param Value         FString to receive the value for the switch
		 * @param Switches      The array of switches to search through
		 * @return true if the switch was found
		**/
		bool ParseSwitch(const TCHAR* InSwitch, FString& Value, const TArray<FString>& Switches)
		{
			// Debug check requirements for InSwitch
			checkSlow(InSwitch != nullptr);
			checkSlow(InSwitch[FCString::Strlen(InSwitch)-1] == TEXT('='));

			static const FString Equals(TEXT("="));
			static const FString Quote(TEXT("\""));
			for (const FString& Switch : Switches)
			{
				if (Switch.StartsWith(InSwitch))
				{
					Switch.Split(Equals, nullptr, &Value);
					if (Value.StartsWith(Quote))
					{
						Value = Value.TrimQuotes();
					}
					return true;
				}
			}
			return false;
		}

		bool ParseOption(const TCHAR* InSwitch, const TArray<FString>& Switches)
		{
			return Switches.Contains(InSwitch);
		}
	};

	typedef TSharedRef<IToolMode> IToolModeRef;
	typedef TSharedPtr<IToolMode> IToolModePtr;

	class FToolModeFactory
	{
	public:
		static IToolModeRef Create(const TSharedRef<IBuildPatchServicesModule>& BpsInterface);
	};
}
