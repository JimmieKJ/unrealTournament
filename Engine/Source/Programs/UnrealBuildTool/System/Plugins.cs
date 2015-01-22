// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Linq;

namespace UnrealBuildTool
{
	public class PluginInfo
	{
		public enum PluginModuleType
		{
			Runtime,
			RuntimeNoCommandlet,
			Developer,
			Editor,
			EditorNoCommandlet,
			/** Program-only plugin */
			Program,
		}

		public enum LoadedFromType
		{
			// Plugin is built-in to the engine
			Engine,

			// Project-specific plugin, stored within a game project directory
			GameProject
		};

		public struct PluginModuleInfo
		{
			// Name of this module
			public string Name;

			// Type of module
			public PluginModuleType Type;

			// List of platforms supported by this modules
			public List<UnrealTargetPlatform> Platforms;
		}
		
		// Whether or not the plugin should be built
		public bool bShouldBuild;

		// Plugin name
		public string Name;

		// Path to the plugin's root directory
		public string Directory;

		// List of modules in this plugin
		public readonly List<PluginModuleInfo> Modules = new List<PluginModuleInfo>();

		// Whether this plugin is enabled by default
		public bool bEnabledByDefault;

		// Where does this plugin live?
		public LoadedFromType LoadedFrom;

		public override string ToString()
		{
			return Path.Combine(Directory, Name + ".uplugin");
		}
	}

	class PluginReferenceDescriptor
	{
		public string Name;
		public bool bEnabled;
		public List<UnrealTargetPlatform> WhitelistPlatforms = new List<UnrealTargetPlatform>();
		public List<UnrealTargetPlatform> BlacklistPlatforms = new List<UnrealTargetPlatform>();

		public PluginReferenceDescriptor(string InName, bool bInEnabled)
		{
			Name = InName;
			bEnabled = bInEnabled;
		}

		public static PluginReferenceDescriptor FromJson(Dictionary<string, object> Dictionary)
		{
			PluginReferenceDescriptor Descriptor = new PluginReferenceDescriptor((string)Dictionary["Name"], (bool)Dictionary["Enabled"]);
			TryParsePlatformList(Dictionary, "WhitelistPlatforms", Descriptor.WhitelistPlatforms);
			TryParsePlatformList(Dictionary, "BlacklistPlatforms", Descriptor.BlacklistPlatforms);
			return Descriptor;
		}

		public bool IsEnabledForPlatform(UnrealTargetPlatform Platform)
		{
			if(!bEnabled)
			{
				return false;
			}
			if(WhitelistPlatforms.Count > 0 && !WhitelistPlatforms.Contains(Platform))
			{
				return false;
			}
			if(BlacklistPlatforms.Contains(Platform))
			{
				return false;
			}
			return true;
		}

		static void TryParsePlatformList(Dictionary<string, object> Dictionary, string FieldName, List<UnrealTargetPlatform> Platforms)
		{
			object ArrayObject;
			if(Dictionary.TryGetValue(FieldName, out ArrayObject))
			{
				foreach(string PlatformName in ((object[])ArrayObject).Select(x => (string)x))
				{
					UnrealTargetPlatform Platform;
					if(Enum.TryParse(PlatformName, true, out Platform))
					{
						Platforms.Add(Platform);
					}
					else
					{
						throw new BuildException("Unknown platform name '{0}'", PlatformName);
					}
				}
			}
		}
	}

	public class Plugins
	{
		/// Latest supported version for plugin descriptor files.  We can still usually load older versions, but not newer version.
		/// NOTE: This constant exists in PluginManager C++ code as well.
		private static int LatestPluginDescriptorFileVersion = 3;	// IMPORTANT: Remember to also update EProjectFileVersion in PluginManagerShared.h when this changes!
		
		/// File extension of plugin descriptor files.  NOTE: This constant exists in UnrealBuildTool code as well.
		/// NOTE: This constant exists in PluginManager C++ code as well.
		private static string PluginDescriptorFileExtension = ".uplugin";



		/// <summary>
		/// Loads a plugin descriptor file and fills out a new PluginInfo structure.  Throws an exception on failure.
		/// </summary>
		/// <param name="PluginFile">The path to the plugin file to load</param>
		/// <param name="LoadedFrom">Where the plugin was loaded from</param>
		/// <returns>New PluginInfo for the loaded descriptor.</returns>
		private static PluginInfo LoadPluginDescriptor( FileInfo PluginFileInfo, PluginInfo.LoadedFromType LoadedFrom )
		{
			// Load the file up (JSon format)
			Dictionary<string, object> PluginDescriptorDict;
			{
				string FileContent;
				using( var StreamReader = new StreamReader( PluginFileInfo.FullName ) )
				{
					FileContent = StreamReader.ReadToEnd();
				}

				// Parse the Json into a dictionary
				var CaseSensitiveJSonDict = fastJSON.JSON.Instance.ToObject< Dictionary< string, object > >( FileContent );

				// Convert to a case-insensitive dictionary, so that we can be more tolerant of hand-typed files
				PluginDescriptorDict = new Dictionary<string, object>( CaseSensitiveJSonDict, StringComparer.InvariantCultureIgnoreCase );
			}


			// File version check
			long PluginVersionNumber;
			{
				// Try to get the version of the plugin
				object PluginVersionObject;
				if( !PluginDescriptorDict.TryGetValue( "FileVersion", out PluginVersionObject ) )
				{
					if( !PluginDescriptorDict.TryGetValue( "PluginFileVersion", out PluginVersionObject ) )
					{
						throw new BuildException( "Plugin descriptor file '{0}' does not contain a valid FileVersion entry", PluginFileInfo.FullName );
					}
				}

				if( !( PluginVersionObject is long ) )
				{
					throw new BuildException( "Unable to parse the version number of the plugin descriptor file '{0}'", PluginFileInfo.FullName );
				}

				PluginVersionNumber = (long)PluginVersionObject;
				if( PluginVersionNumber > LatestPluginDescriptorFileVersion )
				{
					throw new BuildException( "Plugin descriptor file '{0}' appears to be in a newer version ({1}) of the file format that we can load (max version: {2}).", PluginFileInfo.FullName, PluginVersionNumber, LatestPluginDescriptorFileVersion );
				}

				// @todo plugin: Should we also test the engine version here?  (we would need to load it from build.properties)
			}
			

			// NOTE: At this point, we can use PluginVersionNumber to handle backwards compatibility when loading the rest of the file!


			var PluginInfo = new PluginInfo();
			PluginInfo.LoadedFrom = LoadedFrom;
			PluginInfo.Directory = PluginFileInfo.Directory.FullName;
			PluginInfo.Name = Path.GetFileName(PluginInfo.Directory);

			// Determine whether the plugin should be enabled by default
			object EnabledByDefaultObject;
			if(PluginDescriptorDict.TryGetValue("EnabledByDefault", out EnabledByDefaultObject) && (EnabledByDefaultObject is bool))
			{
				PluginInfo.bEnabledByDefault = (bool)EnabledByDefaultObject;
			}

			// This plugin might have some modules that we need to know about.  Let's take a look.
			{
				object ModulesObject;
				if( PluginDescriptorDict.TryGetValue( "Modules", out ModulesObject ) )
				{
					if( !( ModulesObject is Array ) )
					{
						throw new BuildException( "Found a 'Modules' entry in plugin descriptor file '{0}', but it doesn't appear to be in the array format that we were expecting.", PluginFileInfo.FullName );
					}
					
					var ModulesArray = (Array)ModulesObject;
					foreach( var ModuleObject in ModulesArray )
					{
						var ModuleDict = new Dictionary<string,object>( (Dictionary< string, object >)ModuleObject, StringComparer.InvariantCultureIgnoreCase );

						var PluginModuleInfo = new PluginInfo.PluginModuleInfo();

						// Module name
						{
							// All modules require a name to be set
							object ModuleNameObject;
							if( !ModuleDict.TryGetValue( "Name", out ModuleNameObject ) )
							{
								throw new BuildException( "Found a 'Module' entry with a missing 'Name' field in plugin descriptor file '{0}'", PluginFileInfo.FullName );
							}
							string ModuleName = (string)ModuleNameObject;

							// @todo plugin: Locate this module right now and validate it?  Repair case?
							PluginModuleInfo.Name = ModuleName;
						}

						
						// Module type
						{
							// Check to see if the user specified the module's type
							object ModuleTypeObject;
							if( !ModuleDict.TryGetValue( "Type", out ModuleTypeObject ) )
							{
								throw new BuildException( "Found a Module entry '{0}' with a missing 'Type' field in plugin descriptor file '{1}'", PluginModuleInfo.Name, PluginFileInfo.FullName );
							}
							string ModuleTypeString = (string)ModuleTypeObject;

							// Check to see if this is a valid type
							bool FoundValidType = false;
							foreach( PluginInfo.PluginModuleType PossibleType in Enum.GetValues( typeof( PluginInfo.PluginModuleType ) ) )
							{
								if( ModuleTypeString.Equals( PossibleType.ToString(), StringComparison.InvariantCultureIgnoreCase ) )
								{
									FoundValidType = true;
									PluginModuleInfo.Type = PossibleType;
									break;
								}
							}
							if( !FoundValidType )
							{
								throw new BuildException( "Module entry '{0}' specified an unrecognized module Type '{1}' in plugin descriptor file '{0}'", PluginModuleInfo.Name, ModuleTypeString, PluginFileInfo.FullName );
							}
						}

						// Supported platforms
						PluginModuleInfo.Platforms = new List<UnrealTargetPlatform>();

						// look for white and blacklists
						object WhitelistObject, BlacklistObject;
						ModuleDict.TryGetValue( "WhitelistPlatforms", out WhitelistObject );
						ModuleDict.TryGetValue( "BlacklistPlatforms", out BlacklistObject );

						if (WhitelistObject != null && BlacklistObject != null)
						{
							throw new BuildException( "Found a module '{0}' with both blacklist and whitelist platform lists in plugin file '{1}'", PluginModuleInfo.Name, PluginFileInfo.FullName );
						}

						// now process the whitelist
						if (WhitelistObject != null)
						{
							if (!(WhitelistObject is Array))
							{
								throw new BuildException("Found a 'WhitelistPlatforms' entry in plugin descriptor file '{0}', but it doesn't appear to be in the array format that we were expecting.", PluginFileInfo.FullName);
							}

							// put the whitelist array directly into the plugin's modulelist
							ConvertPlatformArrayToList((Array)WhitelistObject, ref PluginModuleInfo.Platforms, PluginFileInfo.FullName);
						}
						// handle the blacklist (or lack of blacklist and whitelist which means all platforms)
						else
						{
							// start with all platforms supported
							foreach (UnrealTargetPlatform Platform in Enum.GetValues( typeof( UnrealTargetPlatform ) ) )
							{
								PluginModuleInfo.Platforms.Add(Platform);
							}

							// if we want to disallow some platforms, then pull them out now
							if (BlacklistObject != null)
							{
								if (!(BlacklistObject is Array))
								{
									throw new BuildException("Found a 'BlacklistPlatforms' entry in plugin descriptor file '{0}', but it doesn't appear to be in the array format that we were expecting.", PluginFileInfo.FullName);
								}

								// put the whitelist array directly into the plugin's modulelist
								List<UnrealTargetPlatform> Blacklist = new List<UnrealTargetPlatform>();
								ConvertPlatformArrayToList((Array)BlacklistObject, ref Blacklist, PluginFileInfo.FullName);

								// now remove them from the module platform list
								foreach (UnrealTargetPlatform Platform in Blacklist)
								{
									PluginModuleInfo.Platforms.Remove(Platform);
								}
							}
						}
						
						object ModuleShouldBuild;
						if( ModuleDict.TryGetValue( "bShouldBuild", out ModuleShouldBuild ) )
						{
							PluginInfo.bShouldBuild = (Int64)ModuleShouldBuild == 1 ? true : false;
						}
						else
						{
							PluginInfo.bShouldBuild = true;
						}

						if (PluginInfo.bShouldBuild)
						{
							// add to list of modules
							PluginInfo.Modules.Add(PluginModuleInfo);
						}
					}
				}
				else
				{
					// Plugin contains no modules array.  That's fine.
				}
			}

			return PluginInfo;
		}

		private static void ConvertPlatformArrayToList(Array PlatformNameList, ref List<UnrealTargetPlatform> Platforms, string ModuleFilename)
		{
			// look up each platform name in the array
			foreach (string PlatformName in PlatformNameList)
			{
				// case-insensitive enum matching
				bool bFoundValidType = false;
				foreach( UnrealTargetPlatform PossiblePlatform in Enum.GetValues( typeof( UnrealTargetPlatform ) ) )
				{
					if( PlatformName.Equals( PossiblePlatform.ToString(), StringComparison.InvariantCultureIgnoreCase ) )
					{
						bFoundValidType = true;
						// add it to the output!
						Platforms.Add(PossiblePlatform);
						break;
					}
				}

				if( !bFoundValidType )
				{
					throw new BuildException( "Module entry specified unknown platform '{0}' in plugin descriptor file '{1}'", PlatformName, ModuleFilename );
				}
			}
		}


		/// <summary>
		/// Recursively locates all plugins in the specified folder, appending to the incoming list
		/// </summary>
		/// <param name="PluginsDirectory">Directory to search</param>
		/// <param name="LoadedFrom">Where we're loading these plugins from</param>
		/// <param name="Plugins">List of plugins found so far</param>
		private static void FindPluginsRecursively(string PluginsDirectory, PluginInfo.LoadedFromType LoadedFrom, ref List<PluginInfo> Plugins)
		{
			// NOTE: The logic in this function generally matches that of the C++ code for FindPluginsRecursively
			//       in the core engine code.  These routines should be kept in sync.

			// Each sub-directory is possibly a plugin.  If we find that it contains a plugin, we won't recurse any
			// further -- you can't have plugins within plugins.  If we didn't find a plugin, we'll keep recursing.

			var PluginsDirectoryInfo = new DirectoryInfo(PluginsDirectory);
			foreach( var PossiblePluginDirectory in PluginsDirectoryInfo.EnumerateDirectories() )
			{
				if (!UnrealBuildTool.IsPathIgnoredForBuild(PossiblePluginDirectory.FullName))
				{
					// Do we have a plugin descriptor in this directory?
					bool bFoundPlugin = false;
					foreach (var PluginDescriptorFileName in Directory.GetFiles(PossiblePluginDirectory.FullName, "*" + PluginDescriptorFileExtension))
					{
						// Found a plugin directory!  No need to recurse any further, but make sure it's unique.
						if (!Plugins.Any(x => x.Directory == PossiblePluginDirectory.FullName))
						{
							// Load the plugin info and keep track of it
							var PluginDescriptorFile = new FileInfo(PluginDescriptorFileName);
							var PluginInfo = LoadPluginDescriptor(PluginDescriptorFile, LoadedFrom);

							Plugins.Add(PluginInfo);
							bFoundPlugin = true;
							Log.TraceVerbose("Found plugin in: " + PluginInfo.Directory);
						}

						// No need to search for more plugins
						break;
					}

					if (!bFoundPlugin)
					{
						// Didn't find a plugin in this directory.  Continue to look in subfolders.
						FindPluginsRecursively(PossiblePluginDirectory.FullName, LoadedFrom, ref Plugins);
					}
				}
			}
		}


		/// <summary>
		/// Finds all plugins in the specified base directory
		/// </summary>
		/// <param name="PluginsDirectory">Base directory to search.  All subdirectories will be searched, except directories within other plugins.</param>
		/// <param name="LoadedFrom">Where we're loading these plugins from</param>
		/// <param name="Plugins">List of all of the plugins we found</param>
		public static void FindPluginsIn(string PluginsDirectory, PluginInfo.LoadedFromType LoadedFrom, ref List<PluginInfo> Plugins)
		{
			if (Directory.Exists(PluginsDirectory))
			{
				FindPluginsRecursively(PluginsDirectory, LoadedFrom, ref Plugins);
			}
		}


		/// <summary>
		/// Discovers all plugins
		/// </summary>
		private static void DiscoverAllPlugins()
		{
			if( AllPluginsVar == null )	// Only do this search once per session
			{
				AllPluginsVar = new List< PluginInfo >();

				// Engine plugins
				var EnginePluginsDirectory = Path.Combine( ProjectFileGenerator.EngineRelativePath, "Plugins" );
				Plugins.FindPluginsIn( EnginePluginsDirectory, PluginInfo.LoadedFromType.Engine, ref AllPluginsVar );

				// Game plugins
				foreach( var GameProjectFolder in RulesCompiler.AllGameFolders )
				{
					var GamePluginsDirectory = Path.Combine( GameProjectFolder, "Plugins" );
					Plugins.FindPluginsIn( GamePluginsDirectory, PluginInfo.LoadedFromType.GameProject, ref AllPluginsVar );
				}

				// Also keep track of which modules map to which plugins
				ModulesToPluginMapVar = new Dictionary<string,PluginInfo>( StringComparer.InvariantCultureIgnoreCase );
				foreach( var CurPluginInfo in AllPlugins )
				{
					foreach( var Module in CurPluginInfo.Modules )
					{
						// Make sure a different plugin doesn't already have a module with this name
						// @todo plugin: Collisions like this could happen because of third party plugins added to a project, which isn't really ideal.
						if( ModuleNameToPluginMap.ContainsKey( Module.Name ) )
						{
							throw new BuildException( "Found a plugin in '{0}' which describes a module '{1}', but a module with this name already exists in plugin '{2}'!", CurPluginInfo.Directory, Module.Name, ModuleNameToPluginMap[ Module.Name ].Directory );
						}
						ModulesToPluginMapVar.Add( Module.Name, CurPluginInfo );						
					}
				}
			}
		}


		/// <summary>
		/// Returns true if the specified module name is part of a plugin
		/// </summary>
		/// <param name="ModuleName">Name of the module to check</param>
		/// <returns>True if this is a plugin module</returns>
		public static bool IsPluginModule( string ModuleName )
		{
			return ModuleNameToPluginMap.ContainsKey( ModuleName );
		}

		
		/// <summary>
		/// Checks to see if this module is a plugin module, and if it is, returns the PluginInfo for that module, otherwise null.
		/// </summary>
		/// <param name="ModuleName">Name of the module to check</param>
		/// <returns>The PluginInfo for this module, if the module is a plugin module.  Otherwise, returns null</returns>
		public static PluginInfo GetPluginInfoForModule( string ModuleName )
		{
			PluginInfo FoundPluginInfo;
			if( ModuleNameToPluginMap.TryGetValue( ModuleName, out FoundPluginInfo ) )
			{
				return FoundPluginInfo;
			}
			else
			{
				return null;
			}
		}

	
		/// Access the list of all plugins.  We'll scan for plugins when this is called the first time.
		public static List< PluginInfo > AllPlugins
		{
			get
			{
				DiscoverAllPlugins();
				return AllPluginsVar;
			}
		}

		/// Access a mapping of modules to their respective owning plugin.  Dictionary is case-insensitive.
		private static Dictionary< string, PluginInfo > ModuleNameToPluginMap
		{
			get
			{
				DiscoverAllPlugins();
				return ModulesToPluginMapVar;
			}
		}

	

		/// List of all plugins we've found so far in this session
		private static List< PluginInfo > AllPluginsVar = null;

		/// Maps plugin modules to the plugin that owns them
		private static Dictionary< string, PluginInfo > ModulesToPluginMapVar = null;
	}
}
