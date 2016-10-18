// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml;
using System.Xml.Linq;
using System.Diagnostics;
using System.IO;

using Ionic.Zip;

namespace UnrealBuildTool
{
	public class AndroidAARHandler
	{
		private List<string> Repositories = null;
		private List<string> AARList = null;
		private List<string> JARList = null;

		/// <summary>
		/// Handler for AAR and JAR dependency determination and staging
		/// </summary>
		public AndroidAARHandler()
		{
			Repositories = new List<string>();
			AARList = new List<string>();
			JARList = new List<string>();
		}

		/// <summary>
		/// Add a new respository path to search for AAR and JAR files
		/// </summary>
		/// <param name="RepositoryPath">Directory containing the repository</param>
		public void AddRepository(string RepositoryPath)
		{
			if (Directory.Exists(RepositoryPath))
			{
				Log.TraceInformation("Added repository: {0}", RepositoryPath);
				Repositories.Add(RepositoryPath);
			}
			else
			{
				Log.TraceWarning("AddRepository: Directory {0} not found!", RepositoryPath);
			}
		}

		/// <summary>
		/// Add new respository paths to search for AAR and JAR files (recursive)
		/// </summary>
		/// <param name="RepositoryPath">Root directory containing the repository</param>
		/// <param name="SearchPattern">Search pattern to match</param>
		public void AddRepositories(string RepositoryPath, string SearchPattern)
		{
			List<string> ToCheck = new List<string>();
			ToCheck.Add(RepositoryPath);
			while (ToCheck.Count > 0)
			{
				int LastIndex = ToCheck.Count - 1;
				string CurrentDir = ToCheck[LastIndex];
				ToCheck.RemoveAt(LastIndex);
				foreach (string SearchPath in Directory.GetDirectories(CurrentDir))
				{
					if (SearchPath.Contains(SearchPattern))
					{
						Log.TraceInformation("Added repository: {0}", SearchPath);
						Repositories.Add(SearchPath);
					}
					else
					{
						ToCheck.Add(SearchPath);
					}
				}
			}
		}

		public void DumpAAR()
		{
			Log.TraceInformation("ALL DEPENDENCIES");
			foreach (string Name in AARList)
			{
				Log.TraceInformation("{0}", Name);
			}
			foreach (string Name in JARList)
			{
				Log.TraceInformation("{0}", Name);
			}
		}

		private string GetElementValue(XElement SourceElement, XName ElementName, string DefaultValue)
		{
			XElement Element = SourceElement.Element(ElementName);
			return (Element != null) ? Element.Value : DefaultValue;
		}

		private string FindPackageFile(string PackageName, string BaseName, string Version)
		{
			string[] Sections = PackageName.Split('.');
			string PackagePath = Path.Combine(Sections);

			foreach (string Repository in Repositories)
			{
				string PackageDirectory = Path.Combine(Repository, PackagePath, BaseName, Version);

				if (Directory.Exists(PackageDirectory))
				{
					return PackageDirectory;
				}
			}

			return null;
		}
		
		/// <summary>
		/// Adds a new required JAR file and resolves dependencies
		/// </summary>
		/// <param name="PackageName">Name of the package the JAR belongs to in repository</param>
		/// <param name="BaseName">Directory in repository containing the JAR</param>
		/// <param name="Version">Version of the AAR to use</param>
		public void AddNewJAR(string PackageName, string BaseName, string Version)
		{
			string BasePath = FindPackageFile(PackageName, BaseName, Version);
			if (BasePath == null)
			{
				Log.TraceError("AAR: Unable to find package {0}!", PackageName + "/" + BaseName);
				return;
			}
			string BaseFilename = Path.Combine(BasePath, BaseName + "-" + Version);

			// Do we already have this dependency?
			if (JARList.Contains(BaseFilename))
			{
				return;
			}

			//Log.TraceInformation("JAR: {0}", BaseName);
			JARList.Add(BaseFilename);

			// Check for dependencies
			XDocument DependsXML;
			string DependencyFilename = BaseFilename + ".pom";
			if (File.Exists(DependencyFilename))
			{
				try
				{
					DependsXML = XDocument.Load(DependencyFilename);
				}
				catch (Exception e)
				{
					Log.TraceError("AAR Dependency file {0} parsing error! {1}", DependencyFilename, e);
					return;
				}
			}
			else
			{
				Log.TraceError("AAR: Dependency file {0} missing!", DependencyFilename);
				return;
			}

			string NameSpace = DependsXML.Root.Name.NamespaceName;
			XName DependencyName = XName.Get("dependency", NameSpace);
			XName GroupIdName = XName.Get("groupId", NameSpace);
			XName ArtifactIdName = XName.Get("artifactId", NameSpace);
			XName VersionName = XName.Get("version", NameSpace);
			XName ScopeName = XName.Get("scope", NameSpace);
			XName TypeName = XName.Get("type", NameSpace);

			foreach (XElement DependNode in DependsXML.Descendants(DependencyName))
			{
				string DepGroupId = GetElementValue(DependNode, GroupIdName, "");
				string DepArtifactId = GetElementValue(DependNode, ArtifactIdName, "");
				string DepVersion = GetElementValue(DependNode, VersionName, "");
				string DepScope = GetElementValue(DependNode, ScopeName, "compile");
				string DepType = GetElementValue(DependNode, TypeName, "jar");

				//Log.TraceInformation("Dependency: {0} {1} {2} {3} {4}", DepGroupId, DepArtifactId, DepVersion, DepScope, DepType);

				if (DepType == "aar")
				{
					AddNewAAR(DepGroupId, DepArtifactId, DepVersion);
				}
				else if (DepType == "jar")
				{
					AddNewJAR(DepGroupId, DepArtifactId, DepVersion);
				}
			}
		}

		/// <summary>
		/// Adds a new required AAR file and resolves dependencies
		/// </summary>
		/// <param name="PackageName">Name of the package the AAR belongs to in repository</param>
		/// <param name="BaseName">Directory in repository containing the AAR</param>
		/// <param name="Version">Version of the AAR to use</param>
		public void AddNewAAR(string PackageName, string BaseName, string Version)
		{
			string BasePath = FindPackageFile(PackageName, BaseName, Version);
			if (BasePath == null)
			{
				Log.TraceError("AAR: Unable to find package {0}!", PackageName + "/" + BaseName);
				return;
			}
			string BaseFilename = Path.Combine(BasePath, BaseName + "-" + Version);

			// Do we already have this dependency?
			if (AARList.Contains(BaseFilename))
			{
				return;
			}

			//Log.TraceInformation("AAR: {0}", BaseName);
			AARList.Add(BaseFilename);

			// Check for dependencies
			XDocument DependsXML;
			string DependencyFilename = BaseFilename + ".pom";
			if (File.Exists(DependencyFilename))
			{
				try
				{
					DependsXML = XDocument.Load(DependencyFilename);
				}
				catch (Exception e)
				{
					Log.TraceError("AAR Dependency file {0} parsing error! {1}", DependencyFilename, e);
					return;
				}
			}
			else
			{
				Log.TraceError("AAR: Dependency file {0} missing!", DependencyFilename);
				return;
			}

			string NameSpace = DependsXML.Root.Name.NamespaceName;
			XName DependencyName = XName.Get("dependency", NameSpace);
			XName GroupIdName = XName.Get("groupId", NameSpace);
			XName ArtifactIdName = XName.Get("artifactId", NameSpace);
			XName VersionName = XName.Get("version", NameSpace);
			XName ScopeName = XName.Get("scope", NameSpace);
			XName TypeName = XName.Get("type", NameSpace);

			foreach (XElement DependNode in DependsXML.Descendants(DependencyName))
			{
				string DepGroupId = GetElementValue(DependNode, GroupIdName, "");
				string DepArtifactId = GetElementValue(DependNode, ArtifactIdName, "");
				string DepVersion = GetElementValue(DependNode, VersionName, "");
				string DepScope = GetElementValue(DependNode, ScopeName, "compile");
				string DepType = GetElementValue(DependNode, TypeName, "jar");

				//Log.TraceInformation("Dependency: {0} {1} {2} {3} {4}", DepGroupId, DepArtifactId, DepVersion, DepScope, DepType);

				if (DepType == "aar")
				{
					AddNewAAR(DepGroupId, DepArtifactId, DepVersion);
				}
				else
				if (DepType == "jar")
				{
					AddNewJAR(DepGroupId, DepArtifactId, DepVersion);
				}
			}
		}

		private void MakeDirectoryIfRequiredForFile(string DestFilename)
		{
			string DestSubdir = Path.GetDirectoryName(DestFilename);
			if (!Directory.Exists(DestSubdir))
			{
				Directory.CreateDirectory(DestSubdir);
			}
		}

		private void MakeDirectoryIfRequired(string DestDirectory)
		{
			if (!Directory.Exists(DestDirectory))
			{
				Directory.CreateDirectory(DestDirectory);
			}
		}

		/// <summary>
		/// Copies the required JAR files to the provided directory
		/// </summary>
		/// <param name="DestinationPath">Destination path for JAR files</param>
		public void CopyJARs(string DestinationPath)
		{
			MakeDirectoryIfRequired(DestinationPath);
			DestinationPath = Path.Combine(DestinationPath, "libs");
			MakeDirectoryIfRequired(DestinationPath);

			foreach (string Name in JARList)
			{
				string Filename = Name + ".jar";
				string BaseName = Path.GetFileName(Filename);
				string TargetPath = Path.Combine(DestinationPath, BaseName);

				if (!File.Exists(TargetPath))
				{
					Log.TraceInformation("Copying JAR {0}", BaseName);
					File.Copy(Filename, TargetPath);
				}
			}
		}

		/// <summary>
		/// Extracts the required AAR files to the provided directory
		/// </summary>
		/// <param name="DestinationPath">Destination path for AAR files</param>
		public void ExtractAARs(string DestinationPath)
		{
			MakeDirectoryIfRequired(DestinationPath);
			DestinationPath = Path.Combine(DestinationPath, "JavaLibs");
			MakeDirectoryIfRequired(DestinationPath);

			foreach (string Name in AARList)
			{
				string BaseName = Path.GetFileName(Name);
				string TargetPath = Path.Combine(DestinationPath, BaseName);

				// Only extract if haven't before to prevent changing timestamps
				if (!Directory.Exists(TargetPath))
				{
					Log.TraceInformation("Extracting AAR {0}", BaseName);
					IEnumerable<string> FileNames = UnzipFiles(Name + ".aar", TargetPath);

					// Must have a src directory (even if empty)
					string SrcDirectory = Path.Combine(TargetPath, "src");
					if (!Directory.Exists(SrcDirectory))
					{
						Directory.CreateDirectory(SrcDirectory);
						//FileStream Placeholder = new FileStream(Path.Combine(SrcDirectory, ".placeholder_" + BaseName), FileMode.Create, FileAccess.Write);
					}

					// Move the class.jar file
					string ClassSourceFile = Path.Combine(TargetPath, "classes.jar");
					if (File.Exists(ClassSourceFile))
					{
						string ClassDestFile = Path.Combine(TargetPath, "libs", BaseName + ".jar");
						MakeDirectoryIfRequiredForFile(ClassDestFile);
						File.Move(ClassSourceFile, ClassDestFile);
					}

					// Now create the project.properties file
					string PropertyFilename = Path.Combine(TargetPath, "project.properties");
					if (!File.Exists(PropertyFilename))
					{
						// Try to get the SDK target from the AndroidManifest.xml
						string MinSDK = "9";
						string ManifestFilename = Path.Combine(TargetPath, "AndroidManifest.xml");
						XDocument ManifestXML;
						if (File.Exists(ManifestFilename))
						{
							try
							{
								ManifestXML = XDocument.Load(ManifestFilename);
								XElement UsesSdk = ManifestXML.Root.Element(XName.Get("uses-sdk", ManifestXML.Root.Name.NamespaceName));
								XAttribute Target = UsesSdk.Attribute(XName.Get("minSdkVersion", "http://schemas.android.com/apk/res/android"));
								MinSDK = Target.Value;
							}
							catch (Exception e)
							{
								Log.TraceError("AAR Manifest file {0} parsing error! {1}", ManifestFilename, e);
							}
						}

						File.WriteAllText(PropertyFilename, "target=android-" + MinSDK + "\nandroid.library=true\n");
					}
				}
			}
		}

		/// <summary>
		/// Extracts the contents of a zip file
		/// </summary>
		/// <param name="ZipFileName">Name of the zip file</param>
		/// <param name="BaseDirectory">Output directory</param>
		/// <returns>List of files written</returns>
		public static IEnumerable<string> UnzipFiles(string ZipFileName, string BaseDirectory)
		{
			// manually extract the files. There was a problem with the Ionic.Zip library that required this on non-PC at one point,
			// but that problem is now fixed. Leaving this code as is as we need to return the list of created files and fix up their permissions anyway.
			using (Ionic.Zip.ZipFile Zip = new Ionic.Zip.ZipFile(ZipFileName))
			{
				List<string> OutputFileNames = new List<string>();
				foreach (Ionic.Zip.ZipEntry Entry in Zip.Entries)
				{
					string OutputFileName = Path.Combine(BaseDirectory, Entry.FileName);
					Directory.CreateDirectory(Path.GetDirectoryName(OutputFileName));
					if (!Entry.IsDirectory)
					{
						using (FileStream OutputStream = new FileStream(OutputFileName, FileMode.Create, FileAccess.Write))
						{
							Entry.Extract(OutputStream);
						}
						OutputFileNames.Add(OutputFileName);
					}
				}
				return OutputFileNames;
			}
		}

	}
}
