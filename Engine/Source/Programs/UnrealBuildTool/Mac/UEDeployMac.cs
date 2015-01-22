// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml;
using System.IO;
using Ionic.Zip;

namespace UnrealBuildTool.IOS
{
	class UEDeployMac : UEBuildDeploy
	{
		/**
		 *	Register the platform with the UEBuildDeploy class
		 */
		public override void RegisterBuildDeploy()
		{
			// TODO: print debug info and handle any cases that would keep this from registering
			UEBuildDeploy.RegisterBuildDeploy(UnrealTargetPlatform.Mac, this);
		}

		public override bool PrepTargetForDeployment(UEBuildTarget InTarget)
		{
			Log.TraceInformation("Deploying now!");

			string IntermediateDirectory = InTarget.AppName + "/Intermediate/Build/Mac/" + InTarget.AppName + "/" + InTarget.Configuration;
			if (!Directory.Exists("../../" + IntermediateDirectory))
			{
				IntermediateDirectory = "Engine/Intermediate/Build/Mac/" + InTarget.AppName + "/" + InTarget.Configuration;
			}

			MacToolChain Toolchain = UEToolChain.GetPlatformToolChain(CPPTargetPlatform.Mac) as MacToolChain;
			
			string FixDylibDepsScript = Path.Combine(IntermediateDirectory, "FixDylibDependencies.sh");
			string FinalizeAppBundleScript = Path.Combine(IntermediateDirectory, "FinalizeAppBundle.sh");

			string RemoteWorkingDir = "";

			bool bIsStaticLibrary = InTarget.OutputPath.EndsWith(".a");

			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				if (!bIsStaticLibrary)
				{
					// Copy the command scripts to the intermediate on the target Mac.
					string RemoteFixDylibDepsScript = Toolchain.ConvertPath(Path.GetFullPath(FixDylibDepsScript));
					RemoteFixDylibDepsScript = RemoteFixDylibDepsScript.Replace("../../../../", "../../");
					RPCUtilHelper.CopyFile("../../" + FixDylibDepsScript, RemoteFixDylibDepsScript, true);

					if (!InTarget.GlobalLinkEnvironment.Config.bIsBuildingConsoleApplication)
					{
						string RemoteFinalizeAppBundleScript = Toolchain.ConvertPath(Path.GetFullPath(FinalizeAppBundleScript));
						RemoteFinalizeAppBundleScript = RemoteFinalizeAppBundleScript.Replace("../../../../", "../../");
						RPCUtilHelper.CopyFile("../../" + FinalizeAppBundleScript, RemoteFinalizeAppBundleScript, true);
					}


					// run it remotely
					RemoteWorkingDir = Toolchain.ConvertPath(Path.GetDirectoryName(Path.GetFullPath(FixDylibDepsScript)));

					Log.TraceInformation("Running FixDylibDependencies.sh...");
					Hashtable Results = RPCUtilHelper.Command(RemoteWorkingDir, "/bin/sh", "FixDylibDependencies.sh", null);
					if (Results != null)
					{
						string Result = (string)Results["CommandOutput"];
						if (Result != null)
						{
							Log.TraceInformation(Result);
						}
					}

					if (!InTarget.GlobalLinkEnvironment.Config.bIsBuildingConsoleApplication)
					{
						Log.TraceInformation("Running FinalizeAppBundle.sh...");
						Results = RPCUtilHelper.Command(RemoteWorkingDir, "/bin/sh", "FinalizeAppBundle.sh", null);
						if (Results != null)
						{
							string Result = (string)Results["CommandOutput"];
							if (Result != null)
							{
								Log.TraceInformation(Result);
							}
						}
					}
				}

				
				// If it is requested, send the app bundle back to the platform executing these commands.
				if (BuildConfiguration.bCopyAppBundleBackToDevice)
				{
					Log.TraceInformation("Copying binaries back to this device...");

					try
					{
						string BinaryDir = Path.GetDirectoryName(InTarget.OutputPath) + "\\";
						if (BinaryDir.EndsWith(InTarget.AppName + "\\Binaries\\Mac\\") && InTarget.TargetType != TargetRules.TargetType.Game)
						{
							BinaryDir = BinaryDir.Replace(InTarget.TargetType.ToString(), "Game");
						}

						string RemoteBinariesDir = Toolchain.ConvertPath( BinaryDir );
						string LocalBinariesDir = BinaryDir;
						
						// Get the app bundle's name
						string AppFullName = InTarget.AppName;
						if (InTarget.Configuration != UnrealTargetConfiguration.Development)
						{
							AppFullName += "-" + InTarget.Platform.ToString();
							AppFullName += "-" + InTarget.Configuration.ToString();
						}

						if (!InTarget.GlobalLinkEnvironment.Config.bIsBuildingConsoleApplication)
						{
							AppFullName += ".app";
						}

						List<string> NotBundledBinaries = new List<string>();
						foreach (string BinaryPath in Toolchain.BuiltBinaries)
						{
							if (InTarget.GlobalLinkEnvironment.Config.bIsBuildingConsoleApplication || bIsStaticLibrary || !BinaryPath.StartsWith(LocalBinariesDir + AppFullName))
							{
								NotBundledBinaries.Add(BinaryPath);
							}
						}

						// Zip the app bundle for transferring.
						if (!InTarget.GlobalLinkEnvironment.Config.bIsBuildingConsoleApplication && !bIsStaticLibrary)
						{
							string ZipCommand = "zip -0 -r -y -T \"" + AppFullName + ".zip\" \"" + AppFullName + "\"";
							RPCUtilHelper.Command(RemoteBinariesDir, ZipCommand, "", null);

							// Copy the AppBundle back to the source machine
							string LocalZipFileLocation = LocalBinariesDir + AppFullName + ".zip ";
							string RemoteZipFileLocation = RemoteBinariesDir + AppFullName + ".zip";

							RPCUtilHelper.CopyFile(RemoteZipFileLocation, LocalZipFileLocation, false);

							// Extract the copied app bundle (in zip format) to the local binaries directory
							using (ZipFile AppBundleZip = ZipFile.Read(LocalZipFileLocation))
							{
								foreach (ZipEntry Entry in AppBundleZip)
								{
									Entry.Extract(LocalBinariesDir, ExtractExistingFileAction.OverwriteSilently);
								}
							}

							// Delete the zip as we no longer need/want it.
							File.Delete(LocalZipFileLocation);
							RPCUtilHelper.Command(RemoteBinariesDir, "rm -f \"" + AppFullName + ".zip\"", "", null);
						}

						if (NotBundledBinaries.Count > 0)
						{

							foreach (string BinaryPath in NotBundledBinaries)
							{
								RPCUtilHelper.CopyFile(Toolchain.ConvertPath(BinaryPath), BinaryPath, false);
							}
						}

						Log.TraceInformation("Copied binaries successfully.");
					}
					catch (Exception)
					{
						Log.TraceInformation("Copying binaries back to this device failed.");
					}
				}				
			}

			return true;
		}
	}
}
