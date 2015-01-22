// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnrealBuildTool;

namespace AutomationTool
{
	/// <summary>
	/// Local P4 environment settings
	/// </summary>
	class LocalP4Environment : P4Environment
	{
		internal LocalP4Environment(P4Connection Connection, CommandEnvironment CmdEnv)
			: base(Connection, CmdEnv)
		{
		}

		/// <summary>
		/// Initializes the environment. Tries to autodetect all source control settings.
		/// </summary>
		/// <param name="CompilationEnv">Compilation environment</param>
		protected override void InitEnvironment(P4Connection Connection, CommandEnvironment CmdEnv)
		{
			var HostName = Environment.MachineName.ToLower();
			var P4PortEnv = Environment.GetEnvironmentVariable("P4PORT");
			if (String.IsNullOrEmpty(P4PortEnv))
			{
				P4PortEnv = DetectP4Port();
			}

			var UserName = CommandUtils.GetEnvVar(EnvVarNames.User);
			if (String.IsNullOrEmpty(UserName))
			{
				UserName = DetectUserName(Connection);
			}

			var CommandLineClient = CommandUtils.GetEnvVar(EnvVarNames.Client);
			P4ClientInfo ThisClient = null;
			if (String.IsNullOrEmpty(CommandLineClient) == false)
			{
				ThisClient = Connection.GetClientInfo(CommandLineClient);
				if (ThisClient == null)
				{
					throw new AutomationException("Unable to find client {0}", CommandLineClient);
				}
				if (String.Compare(ThisClient.Owner, UserName, true) != 0)
				{
					throw new AutomationException("Client specified with {0}={1} has a different owner then the detected user name (has: {2}, expected: {3})",
						EnvVarNames.Client, CommandLineClient, ThisClient.Owner, UserName);
				}
			}
			else
			{
				ThisClient = DetectClient(Connection, UserName, HostName, CmdEnv.UATExe);
			}

			Log.TraceInformation("Using user {0} clientspec {1} {2}", UserName, ThisClient.Name, ThisClient.RootPath);
			Environment.SetEnvironmentVariable("P4CLIENT", ThisClient.Name);

			string BuildRootPath;
			string ClientRootPath;
			DetectRootPaths(Connection, CmdEnv.LocalRoot, ThisClient, out BuildRootPath, out ClientRootPath);

			CommandUtils.ConditionallySetEnvVar(EnvVarNames.P4Port, P4PortEnv);
			CommandUtils.ConditionallySetEnvVar(EnvVarNames.User, UserName);
			CommandUtils.ConditionallySetEnvVar(EnvVarNames.Client, ThisClient.Name);
			CommandUtils.ConditionallySetEnvVar(EnvVarNames.BuildRootP4, BuildRootPath);
			CommandUtils.ConditionallySetEnvVar(EnvVarNames.ClientRoot, ClientRootPath);

			var CLString = CommandUtils.GetEnvVar(EnvVarNames.Changelist, null);
			if (String.IsNullOrEmpty(CLString) && CommandUtils.P4CLRequired)
			{
                CLString = DetectCurrentCL(Connection, ClientRootPath);
			}
			if (!String.IsNullOrEmpty(CLString))
			{
				CommandUtils.ConditionallySetEnvVar(EnvVarNames.Changelist, CLString);
			}

			CommandUtils.ConditionallySetEnvVar(EnvVarNames.LabelToSync, "");
			CommandUtils.ConditionallySetEnvVar("P4USER", UserName);
			CommandUtils.ConditionallySetEnvVar("P4CLIENT", ThisClient.Name);

			var P4Password = Environment.GetEnvironmentVariable(EnvVarNames.P4Password);
			if (!String.IsNullOrEmpty(P4Password))
			{
				CommandUtils.ConditionallySetEnvVar("P4PASSWD", P4Password);
			}

			SetBuildRootEscaped();

			base.InitEnvironment(Connection, CmdEnv);
		}

		/// <summary>
		/// Sets the escaped build root environment variable. If the build root is not set, UAT's location UE4 root will be used.
		/// </summary>
		private void SetBuildRootEscaped()
		{
			var BuildRoot = CommandUtils.GetEnvVar(EnvVarNames.BuildRootP4);
			if (String.IsNullOrEmpty(BuildRoot))
			{
				throw new AutomationException("Build root is empty");
			}
			BuildRoot = CommandUtils.EscapePath(BuildRoot);
			CommandUtils.ConditionallySetEnvVar(EnvVarNames.BuildRootEscaped, BuildRoot);
		}

		/// <summary>
		/// Detects the current changelist the workspace is synced to.
		/// </summary>
		/// <param name="ClientRootPath">Workspace path.</param>
		/// <returns>Changelist number as a string.</returns>
		private static string DetectCurrentCL(P4Connection Connection, string ClientRootPath)
		{
			CommandUtils.Log("uebp_CL not set, detecting 'have' CL...");

			// Retrieve the current changelist 
			var P4Result = Connection.P4("changes -m 1 " + CommandUtils.CombinePaths(PathSeparator.Depot, ClientRootPath, "/...#have"), AllowSpew: false);
			var CLTokens = P4Result.Output.Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
			var CLString = CLTokens[1];
			var CL = Int32.Parse(CLString);
			if (CLString != CL.ToString())
			{
				throw new AutomationException("Failed to retrieve current changelist.");
			}
			return CLString;
		}

		/// <summary>
		/// Detects root paths for the specified client.
		/// </summary>
		/// <param name="UATLocation">AutomationTool.exe location</param>
		/// <param name="ThisClient">Client to detect the root paths for</param>
		/// <param name="BuildRootPath">Build root</param>
		/// <param name="LocalRootPath">Local root</param>
		/// <param name="ClientRootPath">Client root</param>
		private static void DetectRootPaths(P4Connection Connection, string LocalRootPath, P4ClientInfo ThisClient, out string BuildRootPath, out string ClientRootPath)
		{
			// Figure out the build root
			string KnownFilePathFromRoot = CommandEnvironment.KnownFileRelativeToRoot;
			string KnownLocalPath = CommandUtils.MakePathSafeToUseWithCommandLine(CommandUtils.CombinePaths(PathSeparator.Slash, LocalRootPath, KnownFilePathFromRoot));
			ProcessResult P4Result = Connection.P4(String.Format("files -m 1 {0}", KnownLocalPath), AllowSpew: false);

			string KnownFileDepotMapping = P4Result.Output;

			// Get the build root
			Log.TraceVerbose("Looking for {0} in {1}", KnownFilePathFromRoot, KnownFileDepotMapping);
			int EndIdx = KnownFileDepotMapping.IndexOf(KnownFilePathFromRoot, StringComparison.CurrentCultureIgnoreCase);
			if (EndIdx < 0)
			{
				EndIdx = KnownFileDepotMapping.IndexOf(CommandUtils.ConvertSeparators(PathSeparator.Slash, KnownFilePathFromRoot), StringComparison.CurrentCultureIgnoreCase);
			}
			// Get the root path without the trailing path separator
			BuildRootPath = KnownFileDepotMapping.Substring(0, EndIdx - 1);

			// Get the client root
			if (LocalRootPath.StartsWith(CommandUtils.CombinePaths(PathSeparator.Slash, ThisClient.RootPath, "/"), StringComparison.InvariantCultureIgnoreCase) || LocalRootPath == ThisClient.RootPath)
			{
				ClientRootPath = CommandUtils.CombinePaths(PathSeparator.Depot, String.Format("//{0}/", ThisClient.Name), LocalRootPath.Substring(ThisClient.RootPath.Length));
			}
			else
			{
				throw new AutomationException("LocalRootPath ({0}) does not start with the client root path ({1})", LocalRootPath, ThisClient.RootPath);
			}
		}

		/// <summary>
		/// Detects a workspace given the current user name, host name and depot path.
		/// </summary>
		/// <param name="UserName">User name</param>
		/// <param name="HostName">Host</param>
		/// <param name="UATLocation">Path to UAT exe, this will be checked agains the root path.</param>
		/// <returns>Client to use.</returns>
		private static P4ClientInfo DetectClient(P4Connection Connection, string UserName, string HostName, string UATLocation)
		{
			CommandUtils.Log("uebp_CLIENT not set, detecting current client...");

			var MatchingClients = new List<P4ClientInfo>();
			P4ClientInfo[] P4Clients = Connection.GetClientsForUser(UserName, UATLocation);
			foreach (var Client in P4Clients)
			{
				if (!String.IsNullOrEmpty(Client.Host) && String.Compare(Client.Host, HostName, true) != 0)
				{
					Log.TraceInformation("Rejecting client because of different Host {0} \"{1}\" != \"{2}\"", Client.Name, Client.Host, HostName);
					continue;
				}
				
				MatchingClients.Add(Client);
			}

			P4ClientInfo ClientToUse = null;
			if (MatchingClients.Count == 0)
			{
				throw new AutomationException("No matching clientspecs found!");
			}
			else if (MatchingClients.Count == 1)
			{
				ClientToUse = MatchingClients[0];
			}
			else
			{
				// We may have empty host clients here, so pick the first non-empty one if possible
				foreach (var Client in MatchingClients)
				{
					if (!String.IsNullOrEmpty(Client.Host) && String.Compare(Client.Host, HostName, true) == 0)
					{
						ClientToUse = Client;
						break;
					}
				}
				if (ClientToUse == null)
				{
					Log.TraceWarning("{0} clients found that match the current host and root path. The most recently accessed client will be used.", MatchingClients.Count);
					ClientToUse = GetMostRecentClient(MatchingClients);
				}
			}

			return ClientToUse;
		}

		/// <summary>
		/// Given a list of clients with the same owner and root path, tries to find the most recently accessed one.
		/// </summary>
		/// <param name="Clients">List of clients with the same owner and path.</param>
		/// <returns>The most recent client from the list.</returns>
		private static P4ClientInfo GetMostRecentClient(List<P4ClientInfo> Clients)
		{
			Log.TraceVerbose("Detecting the most recent client.");
			P4ClientInfo MostRecentClient = null;
			var MostRecentAccessTime = DateTime.MinValue;
			foreach (var ClientInfo in Clients)
			{
				if (ClientInfo.Access.Ticks > MostRecentAccessTime.Ticks)
				{
					MostRecentAccessTime = ClientInfo.Access;
					MostRecentClient = ClientInfo;
				}
			}
			if (MostRecentClient == null)
			{
				throw new AutomationException("Failed to determine the most recent client in {0}", Clients[0].RootPath);
			}
			return MostRecentClient;
		}

		/// <summary>
		/// Detects current user name.
		/// </summary>
		/// <returns></returns>
		private static string DetectUserName(P4Connection Connection)
		{
			var UserName = String.Empty;
			var P4Result = Connection.P4("info", AllowSpew: false);
			if (P4Result.ExitCode != 0)
			{
				throw new AutomationException("Perforce command failed: {0}. Please make sure your P4PORT or {1} is set properly.", P4Result.Output, EnvVarNames.P4Port);
			}

			// Retrieve the P4 user name			
			var Tags = Connection.ParseTaggedP4Output(P4Result.Output);
			Tags.TryGetValue("User name", out UserName);

			if (String.IsNullOrEmpty(UserName))
			{
				UserName = Environment.GetEnvironmentVariable(EnvVarNames.User);

				if (!String.IsNullOrEmpty(UserName))
				{
					Log.TraceWarning("Unable to retrieve perforce user name. Trying to fall back to {0} which is set to {1}.", EnvVarNames.User, UserName);
				}
				else
				{
					throw new AutomationException("Failed to retrieve user name.");
				}
			}

			Environment.SetEnvironmentVariable("P4USER", UserName);

			return UserName;
		}

		/// <summary>
		/// Attempts to detect source control server address from environment variables.
		/// </summary>
		/// <returns>Source control server address.</returns>
		private static string DetectP4Port()
		{
			// Try to read the P4PORT environment and check if it is set correctly
			var P4PortEnv = Environment.GetEnvironmentVariable(EnvVarNames.P4Port);

			// If not, try to fallback to Mapping.P4Port and set this as P4PORT before continueing
			if (!String.IsNullOrEmpty(P4PortEnv))
			{
				Log.TraceWarning("P4PORT is not set. Falling back to {0} which is set to {1}.", EnvVarNames.P4Port, P4PortEnv);
			}
			else
			{
				// If that fails as well, we just give it a shot with perforce:1666 and hope that this works
				Log.TraceWarning("P4PORT is not set. Trying to fallback to perforce:1666");
				P4PortEnv = "perforce:1666";
			}
			Environment.SetEnvironmentVariable("P4PORT", P4PortEnv);

			return P4PortEnv;
		}
	}
}
