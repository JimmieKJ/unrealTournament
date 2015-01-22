// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnrealBuildTool;

namespace AutomationTool
{
	/// <summary>
	/// Environment to allow access to commonly used environment variables.
	/// </summary>
	public class P4Environment
	{
		
		public string P4Port { get; protected set; }
		public string ClientRoot { get; protected set; }
		public string User { get; protected set; }


		private string ChangelistStringInternal;
		public string ChangelistString 
		{ 
			get
			{
				if (String.IsNullOrEmpty(ChangelistStringInternal))
				{
					throw new AutomationException("P4Environment.ChangelistString has not been initialized but is requested. Set uebp_CL env var or run UAT with -P4CL to automatically detect changelist.");
				}
				return ChangelistStringInternal;
			}
			protected set
			{
				ChangelistStringInternal = value;
			}
		}

		private int ChangelistInternal;
		public int Changelist
		{
			get
			{
				if (ChangelistInternal <= 0)
				{
					throw new AutomationException("P4Environment.Changelist has not been initialized but is requested. Set uebp_CL env var or run UAT with -P4CL to automatically detect changelist.");
				}
				return ChangelistInternal;
			}
			protected set
			{
				ChangelistInternal = value;
			}
		}

		public string Client { get; protected set; }
		public string BuildRootP4 { get; protected set; }
		public string BuildRootEscaped { get; protected set; }
		public string LabelToSync { get; protected set; }
		public string LabelPrefix { get; protected set; }
		public string BranchName { get; protected set; }


		internal P4Environment(P4Connection Connection, CommandEnvironment CmdEnv)
		{
			InitEnvironment(Connection, CmdEnv);
		}

		protected virtual void InitEnvironment(P4Connection Connection, CommandEnvironment CmdEnv)
		{
			//
			// P4 Environment
			//

			P4Port = CommandUtils.GetEnvVar(EnvVarNames.P4Port);
			ClientRoot = CommandUtils.GetEnvVar(EnvVarNames.ClientRoot);
			User = CommandUtils.GetEnvVar(EnvVarNames.User);
			ChangelistStringInternal = CommandUtils.GetEnvVar(EnvVarNames.Changelist, null);
			Client = CommandUtils.GetEnvVar(EnvVarNames.Client);
			BuildRootP4 = CommandUtils.GetEnvVar(EnvVarNames.BuildRootP4);
			if (BuildRootP4.EndsWith("/", StringComparison.InvariantCultureIgnoreCase) || BuildRootP4.EndsWith("\\", StringComparison.InvariantCultureIgnoreCase))
			{
				// We expect the build root to not end with a path separator
				BuildRootP4 = BuildRootP4.Substring(0, BuildRootP4.Length - 1);
				CommandUtils.SetEnvVar(EnvVarNames.BuildRootP4, BuildRootP4);
			}
			BuildRootEscaped = CommandUtils.GetEnvVar(EnvVarNames.BuildRootEscaped);
			LabelToSync = CommandUtils.GetEnvVar(EnvVarNames.LabelToSync);

			if (((CommandUtils.P4Enabled || CommandUtils.IsBuildMachine) && (ClientRoot == String.Empty || User == String.Empty ||
				(String.IsNullOrEmpty(ChangelistStringInternal) && CommandUtils.IsBuildMachine) || Client == String.Empty || BuildRootP4 == String.Empty)))
			{
                Log.TraceInformation("P4Enabled={0}", CommandUtils.P4Enabled);
                Log.TraceInformation("ClientRoot={0}",ClientRoot );
                Log.TraceInformation("User={0}", User);
                Log.TraceInformation("ChangelistString={0}", ChangelistStringInternal);
                Log.TraceInformation("Client={0}", Client);
                Log.TraceInformation("BuildRootP4={0}", BuildRootP4);

				throw new AutomationException("BUILD FAILED Perforce Environment is not set up correctly. Please check your environment variables.");
			}

			LabelPrefix = BuildRootP4 + "/";

			if (CommandUtils.P4Enabled)
			{
				if (CommandUtils.IsBuildMachine || ChangelistStringInternal != null)
				{
					// We may not always need the changelist number if we're not a build machine.
					// In local runs, changelist initialization can be really slow!
					VerifyChangelistStringAndSetChangelistNumber();
				}

				// Setup branch name
				string DepotSuffix = "//depot/";
				if (BuildRootP4.StartsWith(DepotSuffix))
				{
					BranchName = BuildRootP4.Substring(DepotSuffix.Length);
				}
				else
				{
					throw new AutomationException("Needs update to work with a stream");
				}

				if (String.IsNullOrWhiteSpace(BranchName))
				{
					throw new AutomationException("BUILD FAILED no branch name.");
				}
			}

			LogSettings();
		}

		void LogSettings()
		{
			// Log all vars
			const bool bQuiet = true;
			Log.TraceInformation("Perforce Environment settings:");
			Log.TraceInformation("{0}={1}", EnvVarNames.P4Port, InternalUtils.GetEnvironmentVariable(EnvVarNames.P4Port, "", bQuiet));
			Log.TraceInformation("{0}={1}", EnvVarNames.User, InternalUtils.GetEnvironmentVariable(EnvVarNames.User, "", bQuiet));
			Log.TraceInformation("{0}={1}", EnvVarNames.Client, InternalUtils.GetEnvironmentVariable(EnvVarNames.Client, "", bQuiet));
			Log.TraceInformation("{0}={1}", EnvVarNames.BuildRootP4, InternalUtils.GetEnvironmentVariable(EnvVarNames.BuildRootP4, "", bQuiet));
			Log.TraceInformation("{0}={1}", EnvVarNames.BuildRootEscaped, InternalUtils.GetEnvironmentVariable(EnvVarNames.BuildRootEscaped, "", bQuiet));
			Log.TraceInformation("{0}={1}", EnvVarNames.ClientRoot, InternalUtils.GetEnvironmentVariable(EnvVarNames.ClientRoot, "", bQuiet));
			Log.TraceInformation("{0}={1}", EnvVarNames.Changelist, InternalUtils.GetEnvironmentVariable(EnvVarNames.Changelist, "", bQuiet));
			Log.TraceInformation("{0}={1}", EnvVarNames.LabelToSync, InternalUtils.GetEnvironmentVariable(EnvVarNames.LabelToSync, "", bQuiet));
			Log.TraceInformation("{0}={1}", "P4USER", InternalUtils.GetEnvironmentVariable("P4USER", "", bQuiet));
			Log.TraceInformation("{0}={1}", "P4CLIENT", InternalUtils.GetEnvironmentVariable("P4CLIENT", "", bQuiet));
			Log.TraceInformation("LabelPrefix={0}", LabelPrefix);
		}

		protected void VerifyChangelistStringAndSetChangelistNumber()
		{
			if (String.IsNullOrEmpty(ChangelistStringInternal))
			{
				throw new AutomationException("BUILD FAILED null CL ");
			}

			if (String.IsNullOrEmpty(ChangelistStringInternal) || ChangelistStringInternal.Length < 2)
			{
				throw new AutomationException("BUILD FAILED bad CL {0}.", ChangelistStringInternal);
			}

			ChangelistInternal = Int32.Parse(ChangelistStringInternal);

			if (ChangelistInternal <= 10)
			{
				throw new AutomationException("BUILD FAILED bad CL {0} {1}.", ChangelistStringInternal, ChangelistInternal);
			}

			ChangelistStringInternal = ChangelistInternal.ToString();		// Make sure they match
		}
	}
}
