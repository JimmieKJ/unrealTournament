// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace UnrealBuildTool
{
	/**
	 *	Base class for platform-specific project generators 
	 */
	public abstract class UEPlatformProjectGenerator
	{
		static Dictionary<UnrealTargetPlatform, UEPlatformProjectGenerator> ProjectGeneratorDictionary = new Dictionary<UnrealTargetPlatform, UEPlatformProjectGenerator>();

		/**
		 *	Register the given platforms UEPlatformProjectGenerator instance
		 *	
		 *	@param	InPlatform			The UnrealTargetPlatform to register with
		 *	@param	InProjectGenerator	The UEPlatformProjectGenerator instance to use for the InPlatform
		 */
		public static void RegisterPlatformProjectGenerator(UnrealTargetPlatform InPlatform, UEPlatformProjectGenerator InProjectGenerator)
		{
			// Make sure the build platform is legal
			var BuildPlatform = UEBuildPlatform.GetBuildPlatform(InPlatform, true);
			if (BuildPlatform != null)
			{
				if (ProjectGeneratorDictionary.ContainsKey(InPlatform) == true)
				{
					Log.TraceInformation("RegisterPlatformProjectGenerator Warning: Registering project generator {0} for {1} when it is already set to {2}",
						InProjectGenerator.ToString(), InPlatform.ToString(), ProjectGeneratorDictionary[InPlatform].ToString());
					ProjectGeneratorDictionary[InPlatform] = InProjectGenerator;
				}
				else
				{
					ProjectGeneratorDictionary.Add(InPlatform, InProjectGenerator);
				}
			}
			else
			{
				Log.TraceVerbose("Skipping project file generator registration for {0} due to no valid BuildPlatform.", InPlatform.ToString());
			}
		}

		/**
		 *	Retrieve the UEPlatformProjectGenerator instance for the given TargetPlatform
		 *	
		 *	@param	InPlatform					The UnrealTargetPlatform being built
		 *	@param	bInAllowFailure				If true, do not throw an exception and return null
		 *	
		 *	@return	UEPlatformProjectGenerator	The instance of the project generator
		 */
		public static UEPlatformProjectGenerator GetPlatformProjectGenerator(UnrealTargetPlatform InPlatform, bool bInAllowFailure = false)
		{
			if (ProjectGeneratorDictionary.ContainsKey(InPlatform) == true)
			{
				return ProjectGeneratorDictionary[InPlatform];
			}
			if (bInAllowFailure == true)
			{
				return null;
			}
			throw new BuildException("GetPlatformProjectGenerator: No PlatformProjectGenerator found for {0}", InPlatform.ToString());
		}

		/// <summary>
		/// Allow various platform project generators to generate stub projects if required
		/// </summary>
		/// <param name="InTargetName"></param>
		/// <param name="InTargetFilepath"></param>
		/// <returns></returns>
		public static bool GenerateGameProjectStubs(ProjectFileGenerator InGenerator, string InTargetName, string InTargetFilepath, TargetRules InTargetRules,
			List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations)
		{
			foreach (KeyValuePair<UnrealTargetPlatform, UEPlatformProjectGenerator> Entry in ProjectGeneratorDictionary)
			{
				UEPlatformProjectGenerator ProjGen = Entry.Value;
				ProjGen.GenerateGameProjectStub(InGenerator, InTargetName, InTargetFilepath, InTargetRules, InPlatforms, InConfigurations);
			}
			return true;
		}

		/// <summary>
		/// Allow various platform project generators to generate any special project properties if required
		/// </summary>
		/// <param name="InPlatform"></param>
		/// <returns></returns>
		public static bool GenerateGamePlatformSpecificProperties(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration Configuration, TargetRules.TargetType TargetType, StringBuilder VCProjectFileContent, string RootDirectory, string TargetFilePath)
		{
			if (ProjectGeneratorDictionary.ContainsKey(InPlatform) == true)
			{
				ProjectGeneratorDictionary[InPlatform].GenerateGameProperties(Configuration, VCProjectFileContent, TargetType, RootDirectory, TargetFilePath); ;
			}
			return true;
		}

		public static bool PlatformRequiresVSUserFileGeneration(List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations)
		{
			bool bRequiresVSUserFileGeneration = false;
			foreach (KeyValuePair<UnrealTargetPlatform, UEPlatformProjectGenerator> Entry in ProjectGeneratorDictionary)
			{
				if (InPlatforms.Contains(Entry.Key))
				{
					UEPlatformProjectGenerator ProjGen = Entry.Value;
					bRequiresVSUserFileGeneration |= ProjGen.RequiresVSUserFileGeneration();
				}
			}
			return bRequiresVSUserFileGeneration;
		}

		/**
		 *	Register the platform with the UEPlatformProjectGenerator class
		 */
		public abstract void RegisterPlatformProjectGenerator();

		public virtual void GenerateGameProjectStub(ProjectFileGenerator InGenerator, string InTargetName, string InTargetFilepath, TargetRules InTargetRules,
			List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations)
		{
			// Do nothing
		}

		public virtual void GenerateGameProperties(UnrealTargetConfiguration Configuration, StringBuilder VCProjectFileContent, TargetRules.TargetType TargetType, string RootDirectory, string TargetFilePath)
		{
			// Do nothing
		}

		public virtual bool RequiresVSUserFileGeneration()
		{
			return false;
		}


		///
		///	VisualStudio project generation functions
		///	
		/**
		 *	Whether this build platform has native support for VisualStudio
		 *	
		 *	@param	InPlatform			The UnrealTargetPlatform being built
		 *	@param	InConfiguration		The UnrealTargetConfiguration being built
		 *	
		 *	@return	bool				true if native VisualStudio support (or custom VSI) is available
		 */
		public virtual bool HasVisualStudioSupport(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			// By default, we assume this is true
			return true;
		}

		/**
		 *	Return the VisualStudio platform name for this build platform
		 *	
		 *	@param	InPlatform			The UnrealTargetPlatform being built
		 *	@param	InConfiguration		The UnrealTargetConfiguration being built
		 *	
		 *	@return	string				The name of the platform that VisualStudio recognizes
		 */
		public virtual string GetVisualStudioPlatformName(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			// By default, return the platform string
			return InPlatform.ToString();
		}

		/**
		 *	Return the platform toolset string to write into the project configuration
		 *	
		 *	@param	InPlatform			The UnrealTargetPlatform being built
		 *	@param	InConfiguration		The UnrealTargetConfiguration being built
		 *	
		 *	@return	string				The custom configuration section for the project file; Empty string if it doesn't require one
		 */
		public virtual string GetVisualStudioPlatformToolsetString(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, VCProjectFile InProjectFile)
		{
			return "";
		}

		/**
		 * Return any custom property group lines
		 *	
		 *	@param	InPlatform			The UnrealTargetPlatform being built
		 *	
		 *	@return	string				The custom property import lines for the project file; Empty string if it doesn't require one
		 */
		public virtual string GetAdditionalVisualStudioPropertyGroups(UnrealTargetPlatform InPlatform)
		{
			return "";
		}

		/**
		 * Return any custom property group lines
		 *	
		 *	@param	InPlatform			The UnrealTargetPlatform being built
		 *	
		 *	@return	string				The platform configuration type.  Defaults to "Makefile" unless overridden
		 */
		public virtual string GetVisualStudioPlatformConfigurationType(UnrealTargetPlatform InPlatform)
		{
			return "Makefile";
		}

		/**
		 * Return any custom paths for VisualStudio this platform requires
		 * This include ReferencePath, LibraryPath, LibraryWPath, IncludePath and ExecutablePath.
		 *	
		 *	@param	InPlatform			The UnrealTargetPlatform being built
		 *	@param	TargetType			The type of target (game or program)
		 *	
		 *	@return	string				The custom path lines for the project file; Empty string if it doesn't require one
		 */
		public virtual string GetVisualStudioPathsEntries(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, TargetRules.TargetType TargetType, string TargetRulesPath, string ProjectFilePath, string NMakeOutputPath)
		{
			// NOTE: We are intentionally overriding defaults for these paths with empty strings.  We never want Visual Studio's
			//       defaults for these fields to be propagated, since they are version-sensitive paths that may not reflect
			//       the environment that UBT is building in.  We'll set these environment variables ourselves!
			// NOTE: We don't touch 'ExecutablePath' because that would result in Visual Studio clobbering the system "Path"
			//       environment variable
			string PathsLines = 
				"		<IncludePath />\n" +
				"		<ReferencePath />\n" +
				"		<LibraryPath />\n" +
				"		<LibraryWPath />\n" +
				"		<SourcePath />\n" +
				"		<ExcludePath />\n";

			return PathsLines;
		}

		/**
		 * Return any custom property import lines
		 *	
		 *	@param	InPlatform			The UnrealTargetPlatform being built
		 *	
		 *	@return	string				The custom property import lines for the project file; Empty string if it doesn't require one
		 */
		public virtual string GetAdditionalVisualStudioImportSettings(UnrealTargetPlatform InPlatform)
		{
			return "";
		}

		/**
		 * Return any custom layout directory sections
		 * 
		 *	@param	InPlatform			The UnrealTargetPlatform being built
		 *	@param	TargetType			The type of target (game or program)
		 *	
		 *	@return	string				The custom property import lines for the project file; Empty string if it doesn't require one
		 */
		public virtual string GetVisualStudioLayoutDirSection(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, string InConditionString, TargetRules.TargetType TargetType, string TargetRulesPath, string ProjectFilePath, string NMakeOutputPath)
		{
			return "";
		}

		/**
		 *	Get the output manifest section, if required
		 *	
		 *	@param	InPlatform			The UnrealTargetPlatform being built
		 *	
		 *	@return	string				The output manifest section for the project file; Empty string if it doesn't require one
		 */
        public virtual string GetVisualStudioOutputManifestSection(UnrealTargetPlatform InPlatform, TargetRules.TargetType TargetType, string TargetRulesPath, string ProjectFilePath)
		{
			return "";
		}

		/**
		 * Get whether this platform deploys 
		 * 
		 * @return	bool		true if the 'Deploy' option should be enabled
		 */
		public virtual bool GetVisualStudioDeploymentEnabled(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return false;
		}

		/// <summary>
		/// Get the text to insert into the user file for the given platform/configuration/target
		/// </summary>
		/// <param name="InPlatform">The platform being added</param>
		/// <param name="InConfiguration">The configuration being added</param>
		/// <param name="InConditionString">The condition string </param>
		/// <param name="InTargetRules">The target rules </param>
		/// <param name="TargetRulesPath">The target rules path</param>
		/// <param name="ProjectFilePath">The project file path</param>
		/// <returns>The string to append to the user file</returns>
		public virtual string GetVisualStudioUserFileStrings(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration,
			string InConditionString, TargetRules InTargetRules, string TargetRulesPath, string ProjectFilePath)
		{
			return "";
		}
	}
}
