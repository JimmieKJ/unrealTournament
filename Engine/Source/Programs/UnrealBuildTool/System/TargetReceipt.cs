// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Linq;
using System.Xml.Serialization;
using System.ComponentModel;
using System.Runtime.Serialization;

namespace UnrealBuildTool
{
	public enum BuildProductType
	{
		Executable,
		DynamicLibrary,
		StaticLibrary,
		ImportLibrary,
		SymbolFile,
		RequiredResource,
	}

	[Serializable]
	public class BuildProduct
	{
		public string Path;
		public BuildProductType Type;
		public bool IsPrecompiled;

		private BuildProduct()
		{
		}

		public BuildProduct(string InPath, BuildProductType InType)
		{
			Path = InPath;
			Type = InType;
		}

		public BuildProduct(BuildProduct Other)
		{
			Path = Other.Path;
			Type = Other.Type;
			IsPrecompiled = Other.IsPrecompiled;
		}

		public override string ToString()
		{
			return Path;
		}
	}

	[Serializable]
	public class RuntimeDependency
	{
		/// <summary>
		/// The file that should be staged. Should use $(EngineDir) and $(ProjectDir) variables as a root, so that the target can be relocated to different machines.
		/// </summary>
		public string Path;

		/// <summary>
		/// The path that the file should be staged to, if different. Leave as null if the file should be staged to the same relative path.
		/// </summary>
		public string StagePath;

		private RuntimeDependency()
		{
		}

		public RuntimeDependency(string InPath)
		{
			Path = InPath;
		}

		public RuntimeDependency(string InPath, string InStagePath)
		{
			Path = InPath;
			StagePath = InStagePath;
		}

		public RuntimeDependency(RuntimeDependency InOther)
		{
			Path = InOther.Path;
			StagePath = InOther.StagePath;
		}

		public override string ToString()
		{
			return (StagePath == null) ? Path : String.Format("{0} -> {1}", Path, StagePath);
		}
	}

	/// <summary>
	/// Stores a record of a built target, with all metadata that other tools may need to know about the build.
	/// </summary>
	[Serializable]
	public class TargetReceipt
	{
		/// <summary>
		/// The name of this target
		/// </summary>
		public string TargetName;

		/// <summary>
		/// Which platform the target is compiled for
		/// </summary>
		public UnrealTargetPlatform Platform;

		/// <summary>
		/// Which configuration this target is compiled in
		/// </summary>
		public UnrealTargetConfiguration Configuration;

		/// <summary>
		/// The unique ID for this build.
		/// </summary>
		public string BuildId;

		/// <summary>
		/// The changelist that this target was compiled with.
		/// </summary>
		public BuildVersion Version;

		/// <summary>
		/// The build products which are part of this target
		/// </summary>
		public List<BuildProduct> BuildProducts = new List<BuildProduct>();

		/// <summary>
		/// All the runtime dependencies that this target relies on
		/// </summary>
		public List<RuntimeDependency> RuntimeDependencies = new List<RuntimeDependency>();

		/// <summary>
		/// Default constructor
		/// </summary>
		public TargetReceipt()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InTargetName">The name of the target being compiled</param>
		/// <param name="InPlatform">Platform for the target being compiled</param>
		/// <param name="InConfiguration">Configuration of the target being compiled</param>
		public TargetReceipt(string InTargetName, UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, string InBuildId, BuildVersion InVersion)
		{
			TargetName = InTargetName;
			Platform = InPlatform;
			Configuration = InConfiguration;
			BuildId = InBuildId;
			Version = InVersion;
		}

		/// <summary>
		/// Copy constructor
		/// </summary>
		/// <param name="InOther">Receipt to copy from</param>
		public TargetReceipt(TargetReceipt Other)
		{
			foreach (BuildProduct OtherBuildProduct in Other.BuildProducts)
			{
				BuildProducts.Add(new BuildProduct(OtherBuildProduct));
			}
			foreach (RuntimeDependency OtherRuntimeDependency in Other.RuntimeDependencies)
			{
				RuntimeDependencies.Add(new RuntimeDependency(OtherRuntimeDependency));
			}
		}

		/// <summary>
		/// Adds a build product to the receipt. Does not check whether it already exists.
		/// </summary>
		/// <param name="Path">Path to the build product.</param>
		/// <param name="Type">Type of build product.</param>
		/// <returns>The BuildProduct object that was created</returns>
		public BuildProduct AddBuildProduct(string Path, BuildProductType Type)
		{
			BuildProduct NewBuildProduct = new BuildProduct(Path, Type);
			BuildProducts.Add(NewBuildProduct);
			return NewBuildProduct;
		}

		/// <summary>
		/// Constructs a runtime dependency object and adds it to the receipt.
		/// </summary>
		/// <param name="Path">Source path for the dependency</param>
		/// <param name="StagePath">Location for the dependency in the staged build</param>
		/// <returns>The RuntimeDependency object that was created</returns>
		public RuntimeDependency AddRuntimeDependency(string Path, string StagePath)
		{
			RuntimeDependency NewRuntimeDependency = new RuntimeDependency(Path, StagePath);
			RuntimeDependencies.Add(NewRuntimeDependency);
			return NewRuntimeDependency;
		}

		/// <summary>
		/// Merges another receipt to this one.
		/// </summary>
		/// <param name="Other">Receipt which should be merged</param>
		public void Merge(TargetReceipt Other)
		{
			foreach (BuildProduct OtherBuildProduct in Other.BuildProducts)
			{
				BuildProducts.Add(OtherBuildProduct);
			}
			foreach (RuntimeDependency OtherRuntimeDependency in Other.RuntimeDependencies)
			{
				if (!RuntimeDependencies.Any(x => x.Path == OtherRuntimeDependency.Path && x.StagePath == OtherRuntimeDependency.StagePath))
				{
					RuntimeDependencies.Add(OtherRuntimeDependency);
				}
			}
		}

		/// <summary>
		/// Expand all the path variables in the manifest
		/// </summary>
		/// <param name="EngineDir">Value for the $(EngineDir) variable</param>
		/// <param name="ProjectDir">Value for the $(ProjectDir) variable</param>
		public void ExpandPathVariables(DirectoryReference EngineDir, DirectoryReference ProjectDir)
		{
			ExpandPathVariables(EngineDir, ProjectDir, new Dictionary<string, string>());
		}

		/// <summary>
		/// Expand all the path variables in the manifest, including a list of supplied variable values.
		/// </summary>
		/// <param name="EngineDir">Value for the $(EngineDir) variable</param>
		/// <param name="ProjectDir">Value for the $(ProjectDir) variable</param>
		public void ExpandPathVariables(DirectoryReference EngineDir, DirectoryReference ProjectDir, IDictionary<string, string> OtherVariables)
		{
			// Build a dictionary containing the standard variable expansions
			Dictionary<string, string> Variables = new Dictionary<string, string>(OtherVariables);
			Variables["EngineDir"] = EngineDir.FullName;
			Variables["ProjectDir"] = ProjectDir.FullName;

			// Replace all the variables in the paths
			foreach (BuildProduct BuildProduct in BuildProducts)
			{
				BuildProduct.Path = Utils.ExpandVariables(BuildProduct.Path, Variables);
			}
			foreach (RuntimeDependency RuntimeDependency in RuntimeDependencies)
			{
				RuntimeDependency.Path = Utils.ExpandVariables(RuntimeDependency.Path, Variables);
				if (RuntimeDependency.StagePath != null)
				{
					RuntimeDependency.StagePath = Utils.ExpandVariables(RuntimeDependency.StagePath, Variables);
				}
			}
		}

		/// <summary>
		/// Inserts $(EngineDir) and $(ProjectDir) variables into a path string, so it can be used on different machines.
		/// </summary>
		/// <param name="InputPath">Input path</param>
		/// <param name="EngineDir">The engine directory. Relative paths are ok.</param>
		/// <param name="ProjectDir">The project directory. Relative paths are ok.</param>
		/// <returns>New string with the base directory replaced, or the original string</returns>
		public static string InsertPathVariables(string InputPath, DirectoryReference EngineDir, DirectoryReference ProjectDir)
		{
			string Result = InputPath;
			if (InputPath != null && !InputPath.StartsWith("$("))
			{
				Result = InsertPathVariables(new FileReference(InputPath), EngineDir, ProjectDir);
			}
			return Result;
		}

		/// <summary>
		/// Inserts variables to make a file relative to $(EngineDir) or $(ProjectDir)
		/// </summary>
		/// <param name="File">The file to insert variables into.</param>
		/// <param name="EngineDir">Value of the $(EngineDir) variable.</param>
		/// <param name="ProjectDir">Value of the $(ProjectDir) variable.</param>
		/// <returns>Converted path for the file.</returns>
		public static string InsertPathVariables(FileReference File, DirectoryReference EngineDir, DirectoryReference ProjectDir)
		{
			if (File.IsUnderDirectory(EngineDir))
			{
				return "$(EngineDir)" + Path.DirectorySeparatorChar + File.MakeRelativeTo(EngineDir);
			}
			else if (File.IsUnderDirectory(ProjectDir))
			{
				return "$(ProjectDir)" + Path.DirectorySeparatorChar + File.MakeRelativeTo(ProjectDir);
			}
			else
			{
				return File.FullName;
			}
		}

		/// <summary>
		/// Returns the standard path to the build receipt for a given target
		/// </summary>
		/// <param name="DirectoryName">Base directory for the target being built; either the project directory or engine directory.</param>
		/// <param name="TargetName">The target being built</param>
		/// <param name="Configuration">The target configuration</param>
		/// <param name="Platform">The target platform</param>
		/// <returns>Path to the receipt for this target</returns>
		public static string GetDefaultPath(string BaseDir, string TargetName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string BuildArchitecture)
		{
			if (String.IsNullOrEmpty(BuildArchitecture) && Configuration == UnrealTargetConfiguration.Development)
			{
				return Path.Combine(BaseDir, "Binaries", Platform.ToString(), String.Format("{0}.target", TargetName));
			}
			else
			{
				return Path.Combine(BaseDir, "Binaries", Platform.ToString(), String.Format("{0}-{1}-{2}{3}.target", TargetName, Platform.ToString(), Configuration.ToString(), BuildArchitecture));
			}
		}

		/// <summary>
		/// Read a receipt from disk.
		/// </summary>
		/// <param name="FileName">Filename to read from</param>
		public static TargetReceipt Read(string FileName)
		{
			JsonObject RawObject = JsonObject.Read(FileName);

			// Read the initial fields
			string TargetName = RawObject.GetStringField("TargetName");
			UnrealTargetPlatform Platform = RawObject.GetEnumField<UnrealTargetPlatform>("Platform");
			UnrealTargetConfiguration Configuration = RawObject.GetEnumField<UnrealTargetConfiguration>("Configuration");
			string BuildId = RawObject.GetStringField("BuildId");

			// Try to read the build version
			BuildVersion Version;
			if (!BuildVersion.TryParse(RawObject.GetObjectField("Version"), out Version))
			{
				throw new JsonParseException("Invalid 'Version' field");
			}

			// Create the receipt
			TargetReceipt Receipt = new TargetReceipt(TargetName, Platform, Configuration, BuildId, Version);

			// Read the build products
			JsonObject[] BuildProductObjects;
			if (RawObject.TryGetObjectArrayField("BuildProducts", out BuildProductObjects))
			{
				foreach (JsonObject BuildProductObject in BuildProductObjects)
				{
					string Path;
					BuildProductType Type;
					if (BuildProductObject.TryGetStringField("Path", out Path) && BuildProductObject.TryGetEnumField("Type", out Type))
					{
						string Module;
						BuildProductObject.TryGetStringField("Module", out Module);

						BuildProduct NewBuildProduct = Receipt.AddBuildProduct(Path, Type);

						bool IsPrecompiled;
						if (BuildProductObject.TryGetBoolField("IsPrecompiled", out IsPrecompiled))
						{
							NewBuildProduct.IsPrecompiled = IsPrecompiled;
						}
					}
				}
			}

			// Read the runtime dependencies
			JsonObject[] RuntimeDependencyObjects;
			if (RawObject.TryGetObjectArrayField("RuntimeDependencies", out RuntimeDependencyObjects))
			{
				foreach (JsonObject RuntimeDependencyObject in RuntimeDependencyObjects)
				{
					string Path;
					if (RuntimeDependencyObject.TryGetStringField("Path", out Path))
					{
						string StagePath;
						if (!RuntimeDependencyObject.TryGetStringField("StagePath", out StagePath))
						{
							StagePath = null;
						}
						Receipt.AddRuntimeDependency(Path, StagePath);
					}
				}
			}

			return Receipt;
		}

		/// <summary>
		/// Try to read a receipt from disk, failing gracefully if it can't be read.
		/// </summary>
		/// <param name="FileName">Filename to read from</param>
		public static bool TryRead(string FileName, out TargetReceipt Receipt)
		{
			if (!File.Exists(FileName))
			{
				Receipt = null;
				return false;
			}

			try
			{
				Receipt = Read(FileName);
				return true;
			}
			catch (Exception)
			{
				Receipt = null;
				return false;
			}
		}

		/// <summary>
		/// Write the receipt to disk.
		/// </summary>
		/// <param name="FileName">Output filename</param>
		public void Write(string FileName)
		{
			using (JsonWriter Writer = new JsonWriter(FileName))
			{
				Writer.WriteObjectStart();
				Writer.WriteValue("TargetName", TargetName);
				Writer.WriteValue("Platform", Platform.ToString());
				Writer.WriteValue("Configuration", Configuration.ToString());
				Writer.WriteValue("BuildId", BuildId);

				Writer.WriteObjectStart("Version");
				Version.Write(Writer);
				Writer.WriteObjectEnd();

				Writer.WriteArrayStart("BuildProducts");
				foreach (BuildProduct BuildProduct in BuildProducts)
				{
					Writer.WriteObjectStart();
					Writer.WriteValue("Path", BuildProduct.Path);
					Writer.WriteValue("Type", BuildProduct.Type.ToString());
					if (BuildProduct.IsPrecompiled)
					{
						Writer.WriteValue("IsPrecompiled", BuildProduct.IsPrecompiled);
					}
					Writer.WriteObjectEnd();
				}
				Writer.WriteArrayEnd();

				Writer.WriteArrayStart("RuntimeDependencies");
				foreach (RuntimeDependency RuntimeDependency in RuntimeDependencies)
				{
					Writer.WriteObjectStart();
					Writer.WriteValue("Path", RuntimeDependency.Path);
					if (RuntimeDependency.StagePath != null)
					{
						Writer.WriteValue("StagePath", RuntimeDependency.StagePath);
					}
					Writer.WriteObjectEnd();
				}
				Writer.WriteArrayEnd();

				Writer.WriteObjectEnd();
			}
		}
	}
}
