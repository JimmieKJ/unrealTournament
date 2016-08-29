// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;

namespace UnrealBuildTool
{
	public enum PluginDescriptorVersion
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
	};

	public class PluginDescriptor
	{

		/// <summary>
		/// Descriptor version number
		/// </summary>
		public int FileVersion;

		/// <summary>
		/// Version number for the plugin.  The version number must increase with every version of the plugin, so that the system 
		/// can determine whether one version of a plugin is newer than another, or to enforce other requirements.  This version
		/// number is not displayed in front-facing UI.  Use the VersionName for that.
		/// </summary>
		public int Version;

		/// <summary>
		/// Name of the version for this plugin.  This is the front-facing part of the version number.  It doesn't need to match
		/// the version number numerically, but should be updated when the version number is increased accordingly.
		/// </summary>
		public string VersionName;

		/// <summary>
		/// Friendly name of the plugin
		/// </summary>
		public string FriendlyName;

		/// <summary>
		/// Description of the plugin
		/// </summary>
		public string Description;

		/// <summary>
		/// The name of the category this plugin
		/// </summary>
		public string Category;

		/// <summary>
		/// The company or individual who created this plugin.  This is an optional field that may be displayed in the user interface.
		/// </summary>
		public string CreatedBy;

		/// <summary>
		/// Hyperlink URL string for the company or individual who created this plugin.  This is optional.
		/// </summary>
		public string CreatedByURL;

		/// <summary>
		/// Documentation URL string.
		/// </summary>
		public string DocsURL;

		/// <summary>
		/// Marketplace URL for this plugin. This URL will be embedded into projects that enable this plugin, so we can redirect to the marketplace if a user doesn't have it installed.
		/// </summary>
		public string MarketplaceURL;

		/// <summary>
		/// Support URL/email for this plugin.
		/// </summary>
		public string SupportURL;

		/// <summary>
		/// List of all modules associated with this plugin
		/// </summary>
		public ModuleDescriptor[] Modules;

		/// <summary>
		/// List of all localization targets associated with this plugin
		/// </summary>
		public LocalizationTargetDescriptor[] LocalizationTargets;

		/// <summary>
		/// Whether this plugin should be enabled by default for all projects
		/// </summary>
		public bool bEnabledByDefault;

		/// <summary>
		/// Can this plugin contain content?
		/// </summary>
		public bool bCanContainContent;

		/// </summary>
		/// Marks the plugin as beta in the UI
		/// </summary>
		public bool bIsBetaVersion;

		/// <summary>
		/// Whether this plugin can be used by UnrealHeaderTool
		/// </summary>
		public bool bCanBeUsedWithUnrealHeaderTool;

		/// <summary>
		/// Set for plugins which are installed
		/// </summary>
		public bool bInstalled;

		/// <summary>
		/// For plugins that are under a platform folder (eg. /PS4/), determines whether compiling the plugin requires the build platform and/or SDK to be available
		/// </summary>
		public bool bRequiresBuildPlatform;

		/// <summary>
		/// Set of pre-build steps to execute, keyed by host platform name.
		/// </summary>
		public CustomBuildSteps PreBuildSteps;

		/// <summary>
		/// Set of post-build steps to execute, keyed by host platform name.
		/// </summary>
		public CustomBuildSteps PostBuildSteps;

		/// <summary>
		/// Private constructor. This object should not be created directly; read it from disk using FromFile() instead.
		/// </summary>
		private PluginDescriptor()
		{
			FileVersion = (int)PluginDescriptorVersion.Latest;
			bRequiresBuildPlatform = true;
		}

		/// <summary>
		/// Creates a plugin descriptor from a file on disk
		/// </summary>
		/// <param name="FileName">The filename to read</param>
		/// <returns>New plugin descriptor</returns>
		public static PluginDescriptor FromFile(FileReference FileName)
		{
			JsonObject RawObject = JsonObject.Read(FileName.FullName);
			try
			{
				PluginDescriptor Descriptor = new PluginDescriptor();

				// Read the version
				if (!RawObject.TryGetIntegerField("FileVersion", out Descriptor.FileVersion))
				{
					if (!RawObject.TryGetIntegerField("PluginFileVersion", out Descriptor.FileVersion))
					{
						throw new BuildException("Plugin descriptor file '{0}' does not contain a valid FileVersion entry", FileName);
					}
				}

				// Check it's not newer than the latest version we can parse
				if (Descriptor.FileVersion > (int)PluginDescriptorVersion.Latest)
				{
					throw new BuildException("Plugin descriptor file '{0}' appears to be in a newer version ({1}) of the file format that we can load (max version: {2}).", FileName, Descriptor.FileVersion, (int)PluginDescriptorVersion.Latest);
				}

				// Read the other fields
				RawObject.TryGetIntegerField("Version", out Descriptor.Version);
				RawObject.TryGetStringField("VersionName", out Descriptor.VersionName);
				RawObject.TryGetStringField("FriendlyName", out Descriptor.FriendlyName);
				RawObject.TryGetStringField("Description", out Descriptor.Description);

				if (!RawObject.TryGetStringField("Category", out Descriptor.Category))
				{
					// Category used to be called CategoryPath in .uplugin files
					RawObject.TryGetStringField("CategoryPath", out Descriptor.Category);
				}

				// Due to a difference in command line parsing between Windows and Mac, we shipped a few Mac samples containing
				// a category name with escaped quotes. Remove them here to make sure we can list them in the right category.
				if (Descriptor.Category != null && Descriptor.Category.Length >= 2 && Descriptor.Category.StartsWith("\"") && Descriptor.Category.EndsWith("\""))
				{
					Descriptor.Category = Descriptor.Category.Substring(1, Descriptor.Category.Length - 2);
				}

				RawObject.TryGetStringField("CreatedBy", out Descriptor.CreatedBy);
				RawObject.TryGetStringField("CreatedByURL", out Descriptor.CreatedByURL);
				RawObject.TryGetStringField("DocsURL", out Descriptor.DocsURL);
				RawObject.TryGetStringField("MarketplaceURL", out Descriptor.MarketplaceURL);
				RawObject.TryGetStringField("SupportURL", out Descriptor.SupportURL);

				JsonObject[] ModulesArray;
				if (RawObject.TryGetObjectArrayField("Modules", out ModulesArray))
				{
					Descriptor.Modules = Array.ConvertAll(ModulesArray, x => ModuleDescriptor.FromJsonObject(x));
				}

				JsonObject[] LocalizationTargetsArray;
				if (RawObject.TryGetObjectArrayField("LocalizationTargets", out LocalizationTargetsArray))
				{
					Descriptor.LocalizationTargets = Array.ConvertAll(LocalizationTargetsArray, x => LocalizationTargetDescriptor.FromJsonObject(x));
				}

				RawObject.TryGetBoolField("EnabledByDefault", out Descriptor.bEnabledByDefault);
				RawObject.TryGetBoolField("CanContainContent", out Descriptor.bCanContainContent);
				RawObject.TryGetBoolField("IsBetaVersion", out Descriptor.bIsBetaVersion);
				RawObject.TryGetBoolField("Installed", out Descriptor.bInstalled);
				RawObject.TryGetBoolField("CanBeUsedWithUnrealHeaderTool", out Descriptor.bCanBeUsedWithUnrealHeaderTool);
				RawObject.TryGetBoolField("RequiresBuildPlatform", out Descriptor.bRequiresBuildPlatform);

				CustomBuildSteps.TryRead(RawObject, "PreBuildSteps", out Descriptor.PreBuildSteps);
				CustomBuildSteps.TryRead(RawObject, "PostBuildSteps", out Descriptor.PostBuildSteps);

				return Descriptor;
			}
			catch (JsonParseException ParseException)
			{
				throw new JsonParseException("{0} (in {1})", ParseException.Message, FileName);
			}
		}

		/// <summary>
		/// Saves the descriptor to disk
		/// </summary>
		/// <param name="FileName">The filename to write to</param>
		public void Save(string FileName)
		{
			using (JsonWriter Writer = new JsonWriter(FileName))
			{
				Writer.WriteObjectStart();

				Writer.WriteValue("FileVersion", (int)ProjectDescriptorVersion.Latest);
				Writer.WriteValue("Version", Version);
				Writer.WriteValue("VersionName", VersionName);
				Writer.WriteValue("FriendlyName", FriendlyName);
				Writer.WriteValue("Description", Description);
				Writer.WriteValue("Category", Category);
				Writer.WriteValue("CreatedBy", CreatedBy);
				Writer.WriteValue("CreatedByURL", CreatedByURL);
				Writer.WriteValue("DocsURL", DocsURL);
				Writer.WriteValue("MarketplaceURL", MarketplaceURL);
				Writer.WriteValue("SupportURL", SupportURL);
				Writer.WriteValue("EnabledByDefault", bEnabledByDefault);
				Writer.WriteValue("CanContainContent", bCanContainContent);
				Writer.WriteValue("IsBetaVersion", bIsBetaVersion);
				Writer.WriteValue("Installed", bInstalled);
				Writer.WriteValue("RequiresBuildPlatform", bRequiresBuildPlatform);

				ModuleDescriptor.WriteArray(Writer, "Modules", Modules);

				if(PreBuildSteps != null)
				{
					PreBuildSteps.Write(Writer, "PreBuildSteps");
				}

				if(PostBuildSteps != null)
				{
					PostBuildSteps.Write(Writer, "PostBuildSteps");
				}

				Writer.WriteObjectEnd();
			}
		}
	}

	[DebuggerDisplay("Name={Name}")]
	public class PluginReferenceDescriptor
	{
		// Name of the plugin
		public string Name;

		// Whether it should be enabled by default
		public bool bEnabled;

		// Whether this plugin is optional, and the game should silently ignore it not being present
		public bool bOptional;

		// Description of the plugin for users that do not have it installed.
		public string Description;

		// URL for this plugin on the marketplace, if the user doesn't have it installed.
		public string MarketplaceURL;

		// If enabled, list of platforms for which the plugin should be enabled (or all platforms if blank).
		UnrealTargetPlatform[] WhitelistPlatforms;

		// If enabled, list of platforms for which the plugin should be disabled.
		UnrealTargetPlatform[] BlacklistPlatforms;

		// If enabled, list of targets for which the plugin should be enabled (or all targets if blank).
		TargetRules.TargetType[] WhitelistTargets;

		// If enabled, list of targets for which the plugin should be disabled.
		TargetRules.TargetType[] BlacklistTargets;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InName">Name of the plugin</param>
		/// <param name="bInEnabled">Whether the plugin is enabled</param>
		public PluginReferenceDescriptor(string InName, string InMarketplaceURL, bool bInEnabled)
		{
			Name = InName;
			MarketplaceURL = InMarketplaceURL;
			bEnabled = bInEnabled;
		}

		/// <summary>
		/// Construct a PluginReferenceDescriptor from a Json object
		/// </summary>
		/// <param name="RawObject">The Json object containing a plugin reference descriptor</param>
		/// <returns>New PluginReferenceDescriptor object</returns>
		public static PluginReferenceDescriptor FromJsonObject(JsonObject RawObject)
		{
			PluginReferenceDescriptor Descriptor = new PluginReferenceDescriptor(RawObject.GetStringField("Name"), null, RawObject.GetBoolField("Enabled"));
			RawObject.TryGetBoolField("Optional", out Descriptor.bOptional);
			RawObject.TryGetStringField("Description", out Descriptor.Description);
			RawObject.TryGetStringField("MarketplaceURL", out Descriptor.MarketplaceURL);
			RawObject.TryGetEnumArrayField<UnrealTargetPlatform>("WhitelistPlatforms", out Descriptor.WhitelistPlatforms);
			RawObject.TryGetEnumArrayField<UnrealTargetPlatform>("BlacklistPlatforms", out Descriptor.BlacklistPlatforms);
			RawObject.TryGetEnumArrayField<TargetRules.TargetType>("WhitelistTargets", out Descriptor.WhitelistTargets);
			RawObject.TryGetEnumArrayField<TargetRules.TargetType>("BlacklistTargets", out Descriptor.BlacklistTargets);
			return Descriptor;
		}

		/// <summary>
		/// Determines if this reference enables the plugin for a given platform
		/// </summary>
		/// <param name="Platform">The platform to check</param>
		/// <returns>True if the plugin should be enabled</returns>
		public bool IsEnabledForPlatform(UnrealTargetPlatform Platform)
		{
			if (!bEnabled)
			{
				return false;
			}
			if (WhitelistPlatforms != null && WhitelistPlatforms.Length > 0 && !WhitelistPlatforms.Contains(Platform))
			{
				return false;
			}
			if (BlacklistPlatforms != null && BlacklistPlatforms.Contains(Platform))
			{
				return false;
			}
			return true;
		}

		/// <summary>
		/// Determines if this reference enables the plugin for a given target
		/// </summary>
		/// <param name="Target">The target to check</param>
		/// <returns>True if the plugin should be enabled</returns>
		public bool IsEnabledForTarget(TargetRules.TargetType Target)
		{
			if (!bEnabled)
			{
				return false;
			}
			if (WhitelistTargets != null && WhitelistTargets.Length > 0 && !WhitelistTargets.Contains(Target))
			{
				return false;
			}
			if (BlacklistTargets != null && BlacklistTargets.Contains(Target))
			{
				return false;
			}
			return true;
		}
	}
}
