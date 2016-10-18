// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;

namespace UnrealBuildTool
{
	public enum EProjectType
	{
		Unknown,
		Any,
		Code,
		Content,
	};

	public class InstalledPlatformInfo
	{
		/// <summary>
		/// Information about a single installed platform configuration
		/// </summary>
		public struct InstalledPlatformConfiguration
		{
			/// Build Configuration of this combination
			public UnrealTargetConfiguration Configuration;

			/// Platform for this combination
			public UnrealTargetPlatform Platform;

			/// Type of Platform for this combination
			public TargetRules.TargetType PlatformType;

			/// Architecture for this combination
			public string Architecture;

			/// Location of a file that must exist for this combination to be valid (optional)
			public string RequiredFile;

			/// Type of project this configuration can be used for
			public EProjectType ProjectType;

			/// Whether to display this platform as an option even if it is not valid
			public bool bCanBeDisplayed;

			public InstalledPlatformConfiguration(UnrealTargetConfiguration InConfiguration, UnrealTargetPlatform InPlatform, TargetRules.TargetType InPlatformType, string InArchitecture, string InRequiredFile, EProjectType InProjectType, bool bInCanBeDisplayed)
			{
				Configuration = InConfiguration;
				Platform = InPlatform;
				PlatformType = InPlatformType;
				Architecture = InArchitecture;
				RequiredFile = InRequiredFile;
				ProjectType = InProjectType;
				bCanBeDisplayed = bInCanBeDisplayed;
			}
		}

		private static InstalledPlatformInfo InfoSingleton;

		private List<InstalledPlatformConfiguration> InstalledPlatformConfigurations = new List<InstalledPlatformConfiguration>();

		public static InstalledPlatformInfo Current
		{
			get
			{
				if (InfoSingleton == null)
				{
					InfoSingleton = new InstalledPlatformInfo();
				}
				return InfoSingleton;
			}
		}

		private InstalledPlatformInfo()
		{
			List<string> InstalledPlatforms;
			ConfigCacheIni Ini = ConfigCacheIni.CreateConfigCacheIni(UnrealTargetPlatform.Unknown, "Engine", (DirectoryReference)null);

			if (Ini.GetArray("InstalledPlatforms", "InstalledPlatformConfigurations", out InstalledPlatforms))
			{
				foreach (string InstalledPlatform in InstalledPlatforms)
				{
					ParsePlatformConfiguration(InstalledPlatform);
				}
			}
		}

		private void ParsePlatformConfiguration(string PlatformConfiguration)
		{
			// Trim whitespace at the beginning.
			PlatformConfiguration = PlatformConfiguration.Trim();
			// Remove brackets.
			PlatformConfiguration = PlatformConfiguration.TrimStart('(');
			PlatformConfiguration = PlatformConfiguration.TrimEnd(')');

			bool bCanCreateEntry = true;

			string ConfigurationName;
			UnrealTargetConfiguration Configuration = UnrealTargetConfiguration.Unknown;
			if (ParseSubValue(PlatformConfiguration, "Configuration=", out ConfigurationName))
			{
				Enum.TryParse(ConfigurationName, out Configuration);
			}
			if (Configuration == UnrealTargetConfiguration.Unknown)
			{
				Log.TraceWarning("Unable to read configuration from {0}", PlatformConfiguration);
				bCanCreateEntry = false;
			}

			string	PlatformName;
			UnrealTargetPlatform Platform = UnrealTargetPlatform.Unknown;
			if (ParseSubValue(PlatformConfiguration, "PlatformName=", out PlatformName))
			{
				Enum.TryParse(PlatformName, out Platform);
			}
			if (Platform == UnrealTargetPlatform.Unknown)
			{
				Log.TraceWarning("Unable to read platform from {0}", PlatformConfiguration);
				bCanCreateEntry = false;
			}

			string PlatformTypeName;
			TargetRules.TargetType PlatformType = TargetRules.TargetType.Game;
			if (ParseSubValue(PlatformConfiguration, "PlatformTypeName=", out PlatformTypeName))
			{
				if (!Enum.TryParse(PlatformTypeName, out PlatformType))
				{
					Log.TraceWarning("Unable to read Platform Type from {0}, defaulting to Game", PlatformConfiguration);
					PlatformType = TargetRules.TargetType.Game;
				}
			}
			if (PlatformType == TargetRules.TargetType.Program)
			{
				Log.TraceWarning("Program is not a valid PlatformType for an Installed Platform, defaulting to Game");
				PlatformType = TargetRules.TargetType.Game;
			}

			string Architecture;
			ParseSubValue(PlatformConfiguration, "Architecture=", out Architecture);

			string RequiredFile;
			if (ParseSubValue(PlatformConfiguration, "RequiredFile=", out RequiredFile))
			{
				RequiredFile = FileReference.Combine(UnrealBuildTool.RootDirectory, RequiredFile).ToString();
			}

			string ProjectTypeName;
			EProjectType ProjectType =  EProjectType.Any;
			if (ParseSubValue(PlatformConfiguration, "ProjectType=", out ProjectTypeName))
			{
				Enum.TryParse(ProjectTypeName, out ProjectType);
			}
			if (ProjectType == EProjectType.Unknown)
			{
				Log.TraceWarning("Unable to read project type from {0}", PlatformConfiguration);
				bCanCreateEntry = false;
			}

			string CanBeDisplayedString;
			bool bCanBeDisplayed = false;
			if (ParseSubValue(PlatformConfiguration, "bCanBeDisplayed=", out CanBeDisplayedString))
			{
				bCanBeDisplayed = Convert.ToBoolean(CanBeDisplayedString);
			}

			if (bCanCreateEntry)
			{
				InstalledPlatformConfigurations.Add(new InstalledPlatformConfiguration(Configuration, Platform, PlatformType, Architecture, RequiredFile, ProjectType, bCanBeDisplayed));
			}
		}

		private bool ParseSubValue(string TrimmedLine, string Match, out string Result)
		{
			Result = string.Empty;
			int MatchIndex = TrimmedLine.IndexOf(Match);
			if (MatchIndex < 0)
			{
				return false;
			}
			// Get the remainder of the string after the match
			MatchIndex += Match.Length;
			TrimmedLine = TrimmedLine.Substring(MatchIndex);
			if (String.IsNullOrEmpty(TrimmedLine))
			{
				return false;
			}
			// get everything up to the first comma and trim any new whitespace
			Result = TrimmedLine.Split(',')[0];
			Result = Result.Trim();
			if (Result.StartsWith("\""))
			{
				// Remove quotes
				int QuoteEnd = Result.LastIndexOf('\"');
				if (QuoteEnd > 0)
				{
					Result = Result.Substring(1, QuoteEnd - 1);
				}
			}
			return true;
		}

		public bool IsValidConfiguration(UnrealTargetConfiguration Configuration, EProjectType ProjectType = EProjectType.Any)
		{
			return ContainsValidConfiguration(
				(InstalledPlatformConfiguration CurConfig) =>
				{
					return CurConfig.Configuration == Configuration
						&& (ProjectType == EProjectType.Any || CurConfig.ProjectType == EProjectType.Any
						|| CurConfig.ProjectType == ProjectType);
				}
			);
		}

		public bool IsValidPlatform(UnrealTargetPlatform Platform, EProjectType ProjectType = EProjectType.Any)
		{
			return ContainsValidConfiguration(
				(InstalledPlatformConfiguration CurConfig) =>
				{
					return CurConfig.Platform == Platform
						&& (ProjectType == EProjectType.Any || CurConfig.ProjectType == EProjectType.Any
						|| CurConfig.ProjectType == ProjectType);
				}
			);
		}

		public bool IsValidPlatformAndConfiguration(UnrealTargetConfiguration Configuration, UnrealTargetPlatform Platform, EProjectType ProjectType = EProjectType.Any)
		{
			return ContainsValidConfiguration(
				(InstalledPlatformConfiguration CurConfig) =>
				{
					return CurConfig.Configuration == Configuration && CurConfig.Platform == Platform
						&& (ProjectType == EProjectType.Any || CurConfig.ProjectType == EProjectType.Any
						|| CurConfig.ProjectType == ProjectType);
				}
			);
		}

		private bool ContainsValidConfiguration(Predicate<InstalledPlatformConfiguration> ConfigFilter)
		{
			if (UnrealBuildTool.IsEngineInstalled())
			{
				foreach (InstalledPlatformConfiguration PlatformConfiguration in InstalledPlatformConfigurations)
				{
					// Check whether filter accepts this configuration and it has required file
					if (ConfigFilter(PlatformConfiguration)
					&& (string.IsNullOrEmpty(PlatformConfiguration.RequiredFile)
					|| File.Exists(PlatformConfiguration.RequiredFile)))
					{
						return true;
					}
				}

				return false;
			}
			return true;
		}

		public static void WriteConfigFileEntries(List<InstalledPlatformConfiguration> Configs, ref List<String> OutEntries)
		{
			// Write config section header
			OutEntries.Add("[InstalledPlatforms]");

			foreach (InstalledPlatformConfiguration Config in Configs)
			{
				WriteConfigFileEntry(Config, ref OutEntries);
			}
		}

		private static void WriteConfigFileEntry(InstalledPlatformConfiguration Config, ref List<String> OutEntries)
		{
			string ConfigDescription = "+InstalledPlatformConfigurations=(";
			if (Config.Platform != UnrealTargetPlatform.Unknown)
			{
				ConfigDescription += string.Format("PlatformName=\"{0}\", ", Config.Platform.ToString());
			}
			if (Config.Configuration != UnrealTargetConfiguration.Unknown)
			{
				ConfigDescription += string.Format("Configuration=\"{0}\", ", Config.Configuration.ToString());
			}
			if (Config.PlatformType != TargetRules.TargetType.Program)
			{
				ConfigDescription += string.Format("PlatformType=\"{0}\", ", Config.PlatformType.ToString());
			}
			if (!string.IsNullOrEmpty(Config.Architecture))
			{
				ConfigDescription += string.Format("Architecture=\"{0}\", ", Config.Architecture);
			}
			if (!string.IsNullOrEmpty(Config.RequiredFile))
			{
				ConfigDescription += string.Format("RequiredFile=\"{0}\", ", Config.RequiredFile);
			}
			if (Config.ProjectType != EProjectType.Unknown)
			{
				ConfigDescription += string.Format("ProjectType=\"{0}\", ", Config.ProjectType.ToString());
			}
			ConfigDescription += string.Format("bCanBeDisplayed={0})", Config.bCanBeDisplayed.ToString());

			OutEntries.Add(ConfigDescription);
		}
	}
}
