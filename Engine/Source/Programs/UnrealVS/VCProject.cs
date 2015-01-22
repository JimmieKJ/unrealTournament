// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using Collections = System.Collections;
using System.Collections.Generic;
using System.Linq;
using VCProjectDll = Microsoft.VisualStudio.VCProject;
using VCProjectEngineDll = Microsoft.VisualStudio.VCProjectEngine;
using EnvDTE;

namespace UnrealVS
{
	public enum DebuggerFlavor
	{
		Local,
		Remote
	}

	public enum ConfigType
	{
		Application,
		Generic,
		Other
	}

	public class VCProjectConfig
	{
		public string Name { get; set; }
		public ConfigType ConfigType { get; set; }
		public bool DebugAttach { get; set; }
		public DebuggerFlavor DebugFlavor { get; set; }
		public string DebugRemoteCommand { get; set; }
		public string DebugCommand { get; set; }
		public string NMakeToolOutput { get; set; }
	}

	public class VCProject
	{
		/// <summary>
		/// Create a VCProject helper from a DTE Project.
		/// </summary>
		/// <param name="Project">DTE Project to extract VC project settings from.</param>
		/// <param name="ConfigNameFilter">(Optional) Config name to return settings for. Leave null or empty to return all configs.</param>
		public VCProject(Project Project, string ConfigNameFilter)
		{
			VCProjectEngineDll.VCProject VCProject = Project.Object as VCProjectEngineDll.VCProject;

			if (VCProject == null) return;

			List<VCProjectConfig> ConfigList = new List<VCProjectConfig>();

			foreach (VCProjectEngineDll.VCConfiguration VCConfig in (Collections.IEnumerable)VCProject.Configurations)
			{
				if (ConfigNameFilter == null || ConfigNameFilter.Length == 0 || VCConfig.Name == ConfigNameFilter)
				{
					VCProjectEngineDll.VCNMakeTool NMakeTool = VCConfig.Tools.Item("VCNMakeTool");

					var DTEConfigs =
						(from Configuration DTEConfig in Project.ConfigurationManager select DTEConfig).ToArray();
					var DTEConfigMatch = DTEConfigs.FirstOrDefault(
						delegate(Configuration DTEConfig)
						{
							string ConfigName = string.Format(
								"{0}|{1}",
								DTEConfig.ConfigurationName,
								DTEConfig.PlatformName);

							return ConfigName == VCConfig.Name;
						});
					ConfigType ConfigType = ConfigType.Other;
					if (DTEConfigMatch != null)
					{
						Property ConfigTypeProp = DTEConfigMatch.Properties.Item("ConfigurationType");
						if (ConfigTypeProp != null)
						{
							VCProjectDll.ConfigurationTypes ConfigTypeVal =
								(VCProjectDll.ConfigurationTypes)ConfigTypeProp.Value;

							if (ConfigTypeVal == VCProjectDll.ConfigurationTypes.typeApplication)
							{
								ConfigType = ConfigType.Application;
							}
							else if (ConfigTypeVal == VCProjectDll.ConfigurationTypes.typeGeneric ||
									 ConfigTypeVal == VCProjectDll.ConfigurationTypes.typeUnknown)
							{
								ConfigType = ConfigType.Generic;
							}
						}
					}
					DebuggerFlavor Flavor = DebuggerFlavor.Local;
					if (VCConfig.DebugSettings.DebuggerFlavor == VCProjectEngineDll.eDebuggerTypes.eRemoteDebugger)
					{
						Flavor = DebuggerFlavor.Remote;
					}

					string NMakeToolOutput = NMakeTool != null ? NMakeTool.Output : string.Empty;

					ConfigList.Add(new VCProjectConfig
									   {
										   Name = VCConfig.Name,
										   ConfigType = ConfigType,
										   DebugAttach = VCConfig.DebugSettings.Attach,
										   DebugFlavor = Flavor,
										   DebugRemoteCommand = VCConfig.DebugSettings.RemoteCommand,
										   DebugCommand = VCConfig.DebugSettings.Command,
										   NMakeToolOutput = NMakeToolOutput
									   });
				}
			}

			_Configurations = ConfigList.ToArray();
		}

		public IEnumerable<VCProjectConfig> Configurations
		{
			get { return _Configurations; }
		}

		private readonly VCProjectConfig[] _Configurations;
	}
}
