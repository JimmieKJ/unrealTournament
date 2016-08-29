// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Linq;

namespace UnrealBuildTool
{
	public enum PluginLoadedFrom
	{
		// Plugin is built-in to the engine
		Engine,

		// Project-specific plugin, stored within a game project directory
		GameProject
	}

	[DebuggerDisplay("\\{{File}\\}")]
	public class PluginInfo
	{
		// Plugin name
		public readonly string Name;

		// Path to the plugin
		public readonly FileReference File;

		// Path to the plugin's root directory
		public readonly DirectoryReference Directory;

		// The plugin descriptor
		public PluginDescriptor Descriptor;

		// Where does this plugin live?
		public PluginLoadedFrom LoadedFrom;

		/// <summary>
		/// Constructs a PluginInfo object
		/// </summary>
		/// <param name="InFile"></param>
		/// <param name="InLoadedFrom">Where this pl</param>
		public PluginInfo(FileReference InFile, PluginLoadedFrom InLoadedFrom)
		{
			Name = Path.GetFileNameWithoutExtension(InFile.FullName);
			File = InFile;
			Directory = File.Directory;
			Descriptor = PluginDescriptor.FromFile(File);
			LoadedFrom = InLoadedFrom;
		}
	}

	public class Plugins
	{
		/// <summary>
		/// Cache of plugins under each directory
		/// </summary>
		static Dictionary<DirectoryReference, List<PluginInfo>> PluginInfoCache = new Dictionary<DirectoryReference, List<PluginInfo>>();

		/// <summary>
		/// Cache of plugin filenames under each directory
		/// </summary>
		static Dictionary<DirectoryReference, List<FileReference>> PluginFileCache = new Dictionary<DirectoryReference, List<FileReference>>();

		/// <summary>
		/// Filters the list of plugins to ensure that any game plugins override engine plugins with the same name, and otherwise that no two
		/// plugins with the same name exist. 
		/// </summary>
		/// <param name="Plugins">List of plugins to filter</param>
		/// <returns>Filtered list of plugins in the original order</returns>
		public static IEnumerable<PluginInfo> FilterPlugins(IEnumerable<PluginInfo> Plugins)
		{
			Dictionary<string, PluginInfo> NameToPluginInfo = new Dictionary<string, PluginInfo>(StringComparer.InvariantCultureIgnoreCase);
			foreach(PluginInfo Plugin in Plugins)
			{
				PluginInfo ExistingPluginInfo;
				if(!NameToPluginInfo.TryGetValue(Plugin.Name, out ExistingPluginInfo))
				{
					NameToPluginInfo.Add(Plugin.Name, Plugin);
				}
				else if(ExistingPluginInfo.LoadedFrom == PluginLoadedFrom.Engine && Plugin.LoadedFrom == PluginLoadedFrom.GameProject)
				{
					NameToPluginInfo[Plugin.Name] = Plugin;
				}
				else if(ExistingPluginInfo.LoadedFrom != PluginLoadedFrom.GameProject || Plugin.LoadedFrom != PluginLoadedFrom.Engine)
				{
					throw new BuildException(String.Format("Found '{0}' plugin in two locations ({1} and {2}). Plugin names must be unique.", Plugin.Name, ExistingPluginInfo.File, Plugin.File));
				}
			}
			return Plugins.Where(x => NameToPluginInfo[x.Name] == x);
		}

		/// <summary>
		/// Read all the plugins available to a given project
		/// </summary>
		/// <param name="EngineDir">Path to the engine directory</param>
		/// <param name="ProjectFileName">Path to the project file (or null)</param>
		/// <returns>Sequence of PluginInfo objects, one for each discovered plugin</returns>
		public static List<PluginInfo> ReadAvailablePlugins(DirectoryReference EngineDirectoryName, FileReference ProjectFileName)
		{
			List<PluginInfo> Plugins = new List<PluginInfo>();

			// Read all the engine plugins
			DirectoryReference EnginePluginsDirectoryName = DirectoryReference.Combine(EngineDirectoryName, "Plugins");
			Plugins.AddRange(ReadPluginsFromDirectory(EnginePluginsDirectoryName, PluginLoadedFrom.Engine));

			// Read all the project plugins
			if (ProjectFileName != null)
			{
				DirectoryReference ProjectPluginsDir = DirectoryReference.Combine(ProjectFileName.Directory, "Plugins");
				Plugins.AddRange(ReadPluginsFromDirectory(ProjectPluginsDir, PluginLoadedFrom.GameProject));
			}

			return FilterPlugins(Plugins).ToList();
		}

		/// <summary>
		/// Read all the plugin descriptors under the given engine directory
		/// </summary>
		/// <param name="EngineDirectory">The parent directory to look in.</param>
		/// <returns>Sequence of the found PluginInfo object.</returns>
		public static IReadOnlyList<PluginInfo> ReadEnginePlugins(DirectoryReference EngineDirectory)
		{
			DirectoryReference EnginePluginsDirectory = DirectoryReference.Combine(EngineDirectory, "Plugins");
			return ReadPluginsFromDirectory(EnginePluginsDirectory, PluginLoadedFrom.Engine);
		}

		/// <summary>
		/// Read all the plugin descriptors under the given project directory
		/// </summary>
		/// <param name="EngineDirectory">The parent directory to look in.</param>
		/// <returns>Sequence of the found PluginInfo object.</returns>
		public static IReadOnlyList<PluginInfo> ReadProjectPlugins(DirectoryReference ProjectDirectory)
		{
			DirectoryReference ProjectPluginsDirectory = DirectoryReference.Combine(ProjectDirectory, "Plugins");
			return ReadPluginsFromDirectory(ProjectPluginsDirectory, PluginLoadedFrom.GameProject);
		}

		/// <summary>
		/// Read all the plugin descriptors under the given directory
		/// </summary>
		/// <param name="ParentDirectory">The parent directory to look in.</param>
		/// <param name="LoadedFrom">The directory type</param>
		/// <returns>Sequence of the found PluginInfo object.</returns>
		public static IReadOnlyList<PluginInfo> ReadPluginsFromDirectory(DirectoryReference ParentDirectory, PluginLoadedFrom LoadedFrom)
		{
			List<PluginInfo> Plugins;
			if (!PluginInfoCache.TryGetValue(ParentDirectory, out Plugins))
			{
				Plugins = new List<PluginInfo>();
				foreach (FileReference PluginFileName in EnumeratePlugins(ParentDirectory))
				{
					PluginInfo Plugin = new PluginInfo(PluginFileName, LoadedFrom);
					Plugins.Add(Plugin);
				}
				PluginInfoCache.Add(ParentDirectory, Plugins);
			}
			return Plugins;
		}

		/// <summary>
		/// Find paths to all the plugins under a given parent directory (recursively)
		/// </summary>
		/// <param name="ParentDirectory">Parent directory to look in. Plugins will be found in any *subfolders* of this directory.</param>
		public static IEnumerable<FileReference> EnumeratePlugins(DirectoryReference ParentDirectory)
		{
			List<FileReference> FileNames;
			if (!PluginFileCache.TryGetValue(ParentDirectory, out FileNames))
			{
				FileNames = new List<FileReference>();
				if (ParentDirectory.Exists())
				{
					EnumeratePluginsInternal(ParentDirectory, FileNames);
				}
				PluginFileCache.Add(ParentDirectory, FileNames);
			}
			return FileNames;
		}

		/// <summary>
		/// Find paths to all the plugins under a given parent directory (recursively)
		/// </summary>
		/// <param name="ParentDirectory">Parent directory to look in. Plugins will be found in any *subfolders* of this directory.</param>
		/// <param name="FileNames">List of filenames. Will have all the discovered .uplugin files appended to it.</param>
		static void EnumeratePluginsInternal(DirectoryReference ParentDirectory, List<FileReference> FileNames)
		{
			foreach (DirectoryReference ChildDirectory in ParentDirectory.EnumerateDirectoryReferences())
			{
				int InitialFileNamesCount = FileNames.Count;
				foreach (FileReference PluginFile in ChildDirectory.EnumerateFileReferences("*.uplugin"))
				{
					FileNames.Add(PluginFile);
				}
				if (FileNames.Count == InitialFileNamesCount)
				{
					EnumeratePluginsInternal(ChildDirectory, FileNames);
				}
			}
		}
	}
}
