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
		private struct InstalledPlatformConfiguration
		{
			/// Build Configuration of this combination
			public UnrealTargetConfiguration Configuration;

			/// Platform for this combination
			public UnrealTargetPlatform Platform;

			/// Location of a file that must exist for this combination to be valid (optional)
			public string RequiredFile;

			/// Type of project this configuration can be used for
			public EProjectType ProjectType;

			/// Whether to display this platform as an option even if it is not valid
			public bool bCanBeDisplayed;

			public InstalledPlatformConfiguration(UnrealTargetConfiguration InConfiguration, UnrealTargetPlatform InPlatform, string InRequiredFile, EProjectType InProjectType, bool bInCanBeDisplayed)
			{
				Configuration = InConfiguration;
				Platform = InPlatform;
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
				InstalledPlatformConfigurations.Add(new InstalledPlatformConfiguration(Configuration, Platform, RequiredFile, ProjectType,bCanBeDisplayed));
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
			if (UnrealBuildTool.IsEngineInstalled())
			{
				foreach (InstalledPlatformConfiguration PlatformConfiguration in InstalledPlatformConfigurations)
				{
					if (PlatformConfiguration.Configuration == Configuration)
					{
						if (ProjectType == EProjectType.Any || PlatformConfiguration.ProjectType == EProjectType.Any
						 || PlatformConfiguration.ProjectType == ProjectType)
						{
							if (string.IsNullOrEmpty(PlatformConfiguration.RequiredFile)
								|| File.Exists(PlatformConfiguration.RequiredFile))
							{
								return true;
							}
						}
					}
				}

				return false;
			}
			return true;
		}

		public bool IsValidPlatform(UnrealTargetPlatform Platform, EProjectType ProjectType = EProjectType.Any)
		{
			if (UnrealBuildTool.IsEngineInstalled())
			{
				foreach (InstalledPlatformConfiguration PlatformConfiguration in InstalledPlatformConfigurations)
				{
					if (PlatformConfiguration.Platform == Platform)
					{
						if (ProjectType == EProjectType.Any || PlatformConfiguration.ProjectType == EProjectType.Any
						 || PlatformConfiguration.ProjectType == ProjectType)
						{
							if (string.IsNullOrEmpty(PlatformConfiguration.RequiredFile)
								|| File.Exists(PlatformConfiguration.RequiredFile))
							{
								return true;
							}
						}
					}
				}

				return false;
			}
			return true;
		}

		public bool IsValidPlatformAndConfiguration(UnrealTargetConfiguration Configuration, UnrealTargetPlatform Platform, EProjectType ProjectType = EProjectType.Any)
		{
			if (UnrealBuildTool.IsEngineInstalled())
			{
				foreach (InstalledPlatformConfiguration PlatformConfiguration in InstalledPlatformConfigurations)
				{
					if (PlatformConfiguration.Configuration == Configuration
					 && PlatformConfiguration.Platform == Platform)
					{
						if (ProjectType == EProjectType.Any || PlatformConfiguration.ProjectType == EProjectType.Any
						 || PlatformConfiguration.ProjectType == ProjectType)
						{
							if (string.IsNullOrEmpty(PlatformConfiguration.RequiredFile)
							 || File.Exists(PlatformConfiguration.RequiredFile))
							{
								return true;
							}
						}
					}
				}

				return false;
			}
			return true;
		}
	}
}
