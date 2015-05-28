using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace UnrealBuildTool
{
	enum ProjectDescriptorVersion
	{
		Invalid = 0,
		Initial = 1,
		NameHash = 2,
		ProjectPluginUnification = 3,
		// !!!!!!!!!! IMPORTANT: Remember to also update LatestPluginDescriptorFileVersion in Plugins.cs (and Plugin system documentation) when this changes!!!!!!!!!!!
		// -----<new versions can be added before this line>-------------------------------------------------
		// - this needs to be the last line (see note below)
		LatestPlusOne,
		Latest = LatestPlusOne - 1
	}

	public class ProjectDescriptor
	{
		// Descriptor version number. */
		public int FileVersion;

		// The engine to open this project with.
		public string EngineAssociation;

		// Category to show under the project browser
		public string Category;

		// Description to show in the project browser
		public string Description;

		// List of all modules associated with this project
		public ModuleDescriptor[] Modules;

		// List of plugins for this project (may be enabled/disabled)
		public PluginReferenceDescriptor[] Plugins;

		// Array of platforms that this project is targeting
		public string[] TargetPlatforms;

		// A hash that is used to determine if the project was forked from a sample
		public uint EpicSampleNameHash;

		/// <summary>
		/// Constructor.
		/// </summary>
		public ProjectDescriptor()
		{
			FileVersion = (int)ProjectDescriptorVersion.Latest;
		}

		/// <summary>
		/// Creates a plugin descriptor from a file on disk
		/// </summary>
		/// <param name="FileName">The filename to read</param>
		/// <returns>New plugin descriptor</returns>
		public static ProjectDescriptor FromFile(string FileName)
		{
			JsonObject RawObject = JsonObject.FromFile(FileName);
			try
			{
				ProjectDescriptor Descriptor = new ProjectDescriptor();

				// Read the version
				if(!RawObject.TryGetIntegerField("FileVersion", out Descriptor.FileVersion))
				{
					if(!RawObject.TryGetIntegerField("ProjectFileVersion", out Descriptor.FileVersion))
					{
						throw new BuildException("Project descriptor '{0}' does not contain a valid FileVersion entry", FileName);
					}
				}

				// Check it's not newer than the latest version we can parse
				if(Descriptor.FileVersion > (int)PluginDescriptorVersion.Latest)
				{
					throw new BuildException("Project descriptor '{0}' appears to be in a newer version ({1}) of the file format that we can load (max version: {2}).", FileName, Descriptor.FileVersion, (int)ProjectDescriptorVersion.Latest);
				}

				// Read simple fields
				RawObject.TryGetStringField("EngineAssociation", out Descriptor.EngineAssociation);
				RawObject.TryGetStringField("Category", out Descriptor.Category);
				RawObject.TryGetStringField("Description", out Descriptor.Description);

				// Read the modules
				JsonObject[] ModulesArray;
				if(RawObject.TryGetObjectArrayField("Modules", out ModulesArray))
				{
					Descriptor.Modules = Array.ConvertAll(ModulesArray, x => ModuleDescriptor.FromJsonObject(x));
				}

				// Read the plugins
				JsonObject[] PluginsArray;
				if(RawObject.TryGetObjectArrayField("Plugins", out PluginsArray))
				{
					Descriptor.Plugins = Array.ConvertAll(PluginsArray, x => PluginReferenceDescriptor.FromJsonObject(x));
				}

				// Read the target platforms
				RawObject.TryGetStringArrayField("TargetPlatforms", out Descriptor.TargetPlatforms);

				// Get the sample name hash
				RawObject.TryGetUnsignedIntegerField("EpicSampleNameHash", out Descriptor.EpicSampleNameHash);

				return Descriptor;
			}
			catch(JsonParseException ParseException)
			{
				throw new JsonParseException("{0} (in {1})", ParseException.Message, FileName);
			}
		}
	}
}
