// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Xml;
using System.Text.RegularExpressions;
using System.Linq;
using System.Reflection;

namespace UnrealBuildTool
{
	public class XGE
	{
		private const string ProgressMarkupPrefix = "@action";

		public static void SaveXGEFile(string XGETaskFilePath)
		{
			int FileNum = 0;
			string OutFile;
			while (true)
			{
				OutFile = Path.Combine(BuildConfiguration.BaseIntermediatePath, String.Format("UBTExport.{0}.xge.xml", FileNum));
				FileInfo ItemInfo = new FileInfo(OutFile);
				if (!ItemInfo.Exists)
				{
					break;
				}
				FileNum++;
			}
			File.Copy(XGETaskFilePath, OutFile);
			Log.TraceInformation("XGEEXPORT: Exported '{0}'", OutFile);
		}

		// precompile the Regex needed to parse the XGE output (the ones we want are of the form "File (Duration at +time)"
		private static Regex XGEDurationRegex = new Regex(@"(?<Filename>.*) *\((?<Duration>[0-9:\.]+) at [0-9\+:\.]+\)", RegexOptions.ExplicitCapture);

		public static ExecutionResult ExecuteActions(List<Action> Actions)
		{
			ExecutionResult XGEResult = ExecutionResult.TasksSucceeded;
			if (Actions.Count > 0)
			{
				// Write the actions to execute to a XGE task file.
				string XGETaskFilePath = Path.Combine(BuildConfiguration.BaseIntermediatePath, "XGETasks.xml");
				WriteTaskFile(Actions, XGETaskFilePath, ProgressWriter.bWriteMarkup);

				if (BuildConfiguration.bXGEExport)
				{
					SaveXGEFile(XGETaskFilePath);
				}
				else
				{
					// Try to execute the XGE tasks, and if XGE is available, skip the local execution fallback.
					if (Telemetry.IsAvailable())
					{
						// Add a custom output handler to determine the build duration for each task and map it back to the action that generated it.
						XGEResult = XGE.ExecuteTaskFileWithProgressMarkup(XGETaskFilePath, Actions.Count, (sender, args) =>
							{
								// sometimes the args comes in as null
								var match = XGEDurationRegex.Match(args.Data ?? "");

								// This is considered fragile and risky parsing of undocumented XGE output, so protect this code from taking down UBT
								try
								{
									// Use LINQ to evaluate the terms so we don't end up with a 
									// huge nested conditional expression just to verify the input.
									foreach (var PCHEvent in
										// first, convert the input line into an enumerable
										from RegexMatch in new[] { match }
										where RegexMatch.Success
										let Filename = RegexMatch.Groups["Filename"].Value
										// find the mapped action. (On PS4 at least, XGE appears to use the full filenane, so we need to strip it back down.
										let Action = Actions.Find(a => a.StatusDescription == Path.GetFileName(Filename) && a.ActionType == ActionType.Compile)
										// We should ALWAYS find the action, so maybe this should throw if we can't
										where Action != null
										// see if the mapped action produces a PCH file
										// (there is currently no compile environment left around by which to tell absolutely, so we infer by the extension. Ugh).
										let ActionProducedPCH = Action.ProducedItems.Find(fileItem => new[] { ".PCH", ".GCH" }.Contains(Path.GetExtension(fileItem.AbsolutePath).ToUpperInvariant()))
										where ActionProducedPCH != null
										// we found a valid PCH action and output file, so parse the duration and send an event.
										let durationMatchStr = RegexMatch.Groups["Duration"].Value
										// if there's no hour designator, add one so .NET can parse the time.
										let durationStr = durationMatchStr.Count(c => c == ':') == 1 ? "0:" + durationMatchStr : durationMatchStr
										let duration = TimeSpan.Parse(durationStr)
										select new
										{
											// actually use the filename here because it is coerced to be PCH.MODULE.HEADER.cpp.
											// This allows us an easy way to determine shared status (or source module) and the real header source.
											FileName = Filename,//Path.GetFileName(ActionProducedPCH.AbsolutePath),
											// Get the length from the OS as the FileItem.Length is really for when the file is used as an input,
											// so the stored length is absent for a new for or out of date at best.
											GeneratedFileLength = new FileInfo(ActionProducedPCH.AbsolutePath).Length,
											GeneratedFileDuration = duration,
										})
									{
										// If we had a valid match for a PCH item, send an event.
										Telemetry.SendEvent("PCHTime.2",
											"ExecutorType", "XGE",
											"Filename", PCHEvent.FileName,
											"FileSize", PCHEvent.GeneratedFileLength.ToString(),
											"Duration", PCHEvent.GeneratedFileDuration.TotalSeconds.ToString("0.00"));
									}
								}
								catch (Exception ex)
								{
									// report that something went wrong so we can diagnose.
									Telemetry.SendEvent("PCHTimeError.2",
										"OutputLine", args.Data,
										"Exception", ex.ToString());
								}

								// XGE outputs the duration info in a format that makes VC think it's an error file/line notation if the full filename is used.
								// So munge it a bit so we don't confuse VC.
								var updatedData = match.Success ? args.Data.Replace('(', '[').Replace(')', ']') : args.Data;

								// forward the output on to the normal handler or the console like normal
								if (Actions[0].OutputEventHandler != null)
								{
									DataReceivedEventArgs EventArgs = ConstructDataReceivedEventArgs(updatedData);
									Actions[0].OutputEventHandler(sender, EventArgs);
								}
								else
								{
									Console.WriteLine(updatedData);
								}
							});
					}
					else
					{
						XGEResult = XGE.ExecuteTaskFileWithProgressMarkup(XGETaskFilePath, Actions.Count, (Sender, Args) => 
							{
								if(Actions[0].OutputEventHandler != null)
								{
									Actions[0].OutputEventHandler(Sender, Args);
								}
								else
								{
									Console.WriteLine(Args.Data);
								}
							});
					}
				}
			}
			return XGEResult;
		}

		private static DataReceivedEventArgs ConstructDataReceivedEventArgs(string NewData)
		{
			// NOTE: This is a pretty nasty hack with C# reflection, however it saves us from having to replace all the
			// handlers in various Toolchains with a wrapper handler that takes a string - it is certainly doable, but 
			// touches code outside of this class that I don't want to touch right now

			// DataReceivedEventArgs is not normally constructable, so work around it with creating a scratch Args object
			DataReceivedEventArgs EventArgs = (DataReceivedEventArgs)System.Runtime.Serialization.FormatterServices.GetUninitializedObject(typeof(DataReceivedEventArgs));

			// now we need to set the Data field using reflection, since it is read only
			FieldInfo[] ArgFields = typeof(DataReceivedEventArgs).GetFields(BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.DeclaredOnly);
			ArgFields[0].SetValue(EventArgs, NewData);
			return EventArgs;
		}

		/** Writes a XGE task file containing the specified actions to the specified file path. */
		public static void WriteTaskFile(List<Action> InActions, string TaskFilePath, bool bProgressMarkup)
		{
            Dictionary<string, string> ExportEnv = new Dictionary<string, string>();

            List<Action> Actions = InActions;
            if (BuildConfiguration.bXGEExport)
            {
                var CurrentEnvironment = Environment.GetEnvironmentVariables();
                foreach (System.Collections.DictionaryEntry Pair in CurrentEnvironment)
                {
                    if (!UnrealBuildTool.InitialEnvironment.Contains(Pair.Key) || (string)(UnrealBuildTool.InitialEnvironment[Pair.Key]) != (string)(Pair.Value))
                    {
                        ExportEnv.Add((string)(Pair.Key), (string)(Pair.Value));
                    }
                }

                int NumSortErrors = 0;
                for (int ActionIndex = 0; ActionIndex < InActions.Count; ActionIndex++)
                {
                    Action Action = InActions[ActionIndex];
                    foreach (FileItem Item in Action.PrerequisiteItems)
                    {
                        if (Item.ProducingAction != null && InActions.Contains(Item.ProducingAction))
                        {
                            int DepIndex = InActions.IndexOf(Item.ProducingAction);
                            if (DepIndex > ActionIndex)
                            {
                                //Console.WriteLine("Action is not topologically sorted.");
                                //Console.WriteLine("  {0} {1}", Action.CommandPath, Action.CommandArguments);
                                //Console.WriteLine("Dependency");
                                //Console.WriteLine("  {0} {1}", Item.ProducingAction.CommandPath, Item.ProducingAction.CommandArguments);
                                NumSortErrors++;
                            }
                        }
                    }
                }
                if (NumSortErrors > 0)
                {
                    //Console.WriteLine("The UBT action graph was not sorted. Sorting actions....");
                    Actions = new List<Action>();
                    var UsedActions = new HashSet<int>();
                    for (int ActionIndex = 0; ActionIndex < InActions.Count; ActionIndex++)
                    {
                        if (UsedActions.Contains(ActionIndex))
                        {
                            continue;
                        }
                        Action Action = InActions[ActionIndex];
                        foreach (FileItem Item in Action.PrerequisiteItems)
                        {
                            if (Item.ProducingAction != null && InActions.Contains(Item.ProducingAction))
                            {
                                int DepIndex = InActions.IndexOf(Item.ProducingAction);
                                if (UsedActions.Contains(DepIndex))
                                {
                                    continue;
                                }
                                Actions.Add(Item.ProducingAction);
                                UsedActions.Add(DepIndex);
                            }
                        }
                        Actions.Add(Action);
                        UsedActions.Add(ActionIndex);
                    }
                    for (int ActionIndex = 0; ActionIndex < Actions.Count; ActionIndex++)
                    {
                        Action Action = Actions[ActionIndex];
                        foreach (FileItem Item in Action.PrerequisiteItems)
                        {
                            if (Item.ProducingAction != null && Actions.Contains(Item.ProducingAction))
                            {
                                int DepIndex = Actions.IndexOf(Item.ProducingAction);
                                if (DepIndex > ActionIndex)
                                {
                                    Console.WriteLine("Action is not topologically sorted.");
                                    Console.WriteLine("  {0} {1}", Action.CommandPath, Action.CommandArguments);
                                    Console.WriteLine("Dependency");
                                    Console.WriteLine("  {0} {1}", Item.ProducingAction.CommandPath, Item.ProducingAction.CommandArguments);
                                    throw new BuildException("Cyclical Dependency in action graph.");
                                }
                            }
                        }
                    }
                }

            }


			XmlDocument XGETaskDocument = new XmlDocument();

			// <BuildSet FormatVersion="1">...</BuildSet>
			XmlElement BuildSetElement = XGETaskDocument.CreateElement("BuildSet");
			XGETaskDocument.AppendChild(BuildSetElement);
			BuildSetElement.SetAttribute("FormatVersion", "1");

			// <Environments>...</Environments>
			XmlElement EnvironmentsElement = XGETaskDocument.CreateElement("Environments");
			BuildSetElement.AppendChild(EnvironmentsElement);

			// <Environment Name="Default">...</CompileEnvironment>
			XmlElement EnvironmentElement = XGETaskDocument.CreateElement("Environment");
			EnvironmentsElement.AppendChild(EnvironmentElement);
			EnvironmentElement.SetAttribute("Name", "Default");
			
			// <Tools>...</Tools>
			XmlElement ToolsElement = XGETaskDocument.CreateElement("Tools");
			EnvironmentElement.AppendChild(ToolsElement);

            if (ExportEnv.Count > 0)
            {
                // <Variables>...</Variables>
                XmlElement VariablesElement = XGETaskDocument.CreateElement("Variables");
                EnvironmentElement.AppendChild(VariablesElement);

                foreach (var Pair in ExportEnv)
                {
                    // <Variable>...</Variable>
                    XmlElement VariableElement = XGETaskDocument.CreateElement("Variable");
                    VariablesElement.AppendChild(VariableElement);
                    VariableElement.SetAttribute("Name", Pair.Key);
                    VariableElement.SetAttribute("Value", Pair.Value);
                }
            }

            for (int ActionIndex = 0; ActionIndex < Actions.Count; ActionIndex++)
			{
				Action Action = Actions[ActionIndex];

				// <Tool ... />
				XmlElement ToolElement = XGETaskDocument.CreateElement("Tool");
				ToolsElement.AppendChild(ToolElement);
				ToolElement.SetAttribute("Name", string.Format("Tool{0}", ActionIndex));
				ToolElement.SetAttribute("AllowRemote", Action.bCanExecuteRemotely.ToString());

				// The XGE documentation says that 'AllowIntercept' must be set to 'true' for all tools where 'AllowRemote' is enabled
				ToolElement.SetAttribute("AllowIntercept", Action.bCanExecuteRemotely.ToString());

				string OutputPrefix = "";
				if (bProgressMarkup)
				{
					OutputPrefix += ProgressMarkupPrefix;
				}
				if( Action.bShouldOutputStatusDescription )
				{
					OutputPrefix += Action.StatusDescription;
				}
				if(OutputPrefix.Length > 0)
				{
					ToolElement.SetAttribute("OutputPrefix", OutputPrefix);
				}

				// batch files (.bat, .cmd) need to be run via or cmd /c or shellexecute, 
				// the latter which we can't use because we want to redirect input/output

				bool bLaunchViaCmdExe = !Path.GetExtension(Action.CommandPath).ToLower().EndsWith("exe");

				string CommandPath = "";
				string CommandArguments = "";

				if (bLaunchViaCmdExe)
				{
					CommandPath = "cmd.exe";
					CommandArguments = string.Format
					(
						"/c \"{0} {1}\"",
						(Action.CommandPath),
						(Action.CommandArguments)
					);
				}
				else
				{
					CommandPath = Action.CommandPath;
					CommandArguments = Action.CommandArguments;
				}

				ToolElement.SetAttribute("Params", CommandArguments);
				ToolElement.SetAttribute("Path", CommandPath);
				ToolElement.SetAttribute("SkipIfProjectFailed", "true");
				if (Action.bIsGCCCompiler)
				{
					ToolElement.SetAttribute( "AutoReserveMemory", "*.gch" );
				}
				ToolElement.SetAttribute(
					"OutputFileMasks", 
					string.Join(
						",",
						Action.ProducedItems.ConvertAll<string>(
							delegate(FileItem ProducedItem) { return ProducedItem.ToString(); }
							).ToArray()
						)
					);
			}

			// <Project Name="Default" Env="Default">...</Project>
			XmlElement ProjectElement = XGETaskDocument.CreateElement("Project");
			BuildSetElement.AppendChild(ProjectElement);
			ProjectElement.SetAttribute("Name", "Default");
			ProjectElement.SetAttribute("Env", "Default");

			for (int ActionIndex = 0; ActionIndex < Actions.Count; ActionIndex++)
			{
				Action Action = Actions[ActionIndex];

				// <Task ... />
				XmlElement TaskElement = XGETaskDocument.CreateElement("Task");
				ProjectElement.AppendChild(TaskElement);
				TaskElement.SetAttribute("SourceFile", "");
				if( !Action.bShouldOutputStatusDescription )
				{
					// If we were configured to not output a status description, then we'll instead
					// set 'caption' text for this task, so that the XGE coordinator has something
					// to display within the progress bars.  For tasks that are outputting a
					// description, XGE automatically displays that text in the progress bar, so we
					// only need to do this for tasks that output their own progress.
					TaskElement.SetAttribute( "Caption", Action.StatusDescription );
				}
				TaskElement.SetAttribute("Name", string.Format("Action{0}",ActionIndex));
				TaskElement.SetAttribute("Tool", string.Format("Tool{0}", ActionIndex));
				TaskElement.SetAttribute("WorkingDir", Action.WorkingDirectory);
				TaskElement.SetAttribute("SkipIfProjectFailed", "true");

				// Create a semi-colon separated list of the other tasks this task depends on the results of.
				List<string> DependencyNames = new List<string>();
				foreach(FileItem Item in Action.PrerequisiteItems)
				{
					if(Item.ProducingAction != null && Actions.Contains(Item.ProducingAction))
					{
						DependencyNames.Add(string.Format("Action{0}", Actions.IndexOf(Item.ProducingAction)));
					}
				}

				if (DependencyNames.Count > 0)
				{
					TaskElement.SetAttribute("DependsOn", string.Join(";", DependencyNames.ToArray()));
                }
			}

			// Write the XGE task XML to a temporary file.
			using (FileStream OutputFileStream = new FileStream(TaskFilePath, FileMode.Create, FileAccess.Write))
			{
				XGETaskDocument.Save(OutputFileStream);
			}
		}

		/** The possible result of executing tasks with XGE. */
		public enum ExecutionResult
		{
			Unavailable,
			TasksFailed,
			TasksSucceeded,
		}

		/**
		 * Executes the tasks in the specified file.
		 * @param TaskFilePath - The path to the file containing the tasks to execute in XGE XML format.
		 * @return Indicates whether the tasks were successfully executed.
		 */
		public static ExecutionResult ExecuteTaskFile(string TaskFilePath, DataReceivedEventHandler OutputEventHandler )
		{
			bool bSilentCompileOutput = false;
			string SilentOption = bSilentCompileOutput ? "/Silent" : "";

            ProcessStartInfo XGEStartInfo = new ProcessStartInfo(
				"xgConsole",
				string.Format(@"{0} /Rebuild /NoWait {1} /NoLogo {2} /ShowTime",
					TaskFilePath,
					BuildConfiguration.bStopXGECompilationAfterErrors ? "/StopOnErrors" : "",
					SilentOption)
				);
			XGEStartInfo.UseShellExecute = false;

			// Use the IDE-integrated Incredibuild monitor to display progress.
			XGEStartInfo.Arguments += " /UseIdeMonitor";

			// Optionally display the external XGE monitor.
			if (BuildConfiguration.bShowXGEMonitor)
			{
				XGEStartInfo.Arguments += " /OpenMonitor";
			}

			try
			{
				// Start the process, redirecting stdout/stderr if requested.
				Process XGEProcess = new Process();
				XGEProcess.StartInfo = XGEStartInfo;
				bool bShouldRedirectOuput = OutputEventHandler != null;
				if (bShouldRedirectOuput)
				{
					XGEStartInfo.RedirectStandardError = true;
					XGEStartInfo.RedirectStandardOutput = true;
					XGEProcess.EnableRaisingEvents = true;
					XGEProcess.OutputDataReceived += OutputEventHandler;
					XGEProcess.ErrorDataReceived += OutputEventHandler;
				}
				XGEProcess.Start();
				if (bShouldRedirectOuput)
				{
					XGEProcess.BeginOutputReadLine();
					XGEProcess.BeginErrorReadLine();
				}

				// Wait until the process is finished and return whether it all the tasks successfully executed.
				XGEProcess.WaitForExit();
				return XGEProcess.ExitCode == 0 ? 
					ExecutionResult.TasksSucceeded :
					ExecutionResult.TasksFailed;
			}
			catch (Exception)
			{
				// If an exception is thrown while starting the process, return Unavailable.
				return ExecutionResult.Unavailable;
			}
		}

		/**
		 * Executes the tasks in the specified file, parsing progress markup as part of the output.
		 */
		public static ExecutionResult ExecuteTaskFileWithProgressMarkup(string TaskFilePath, int NumActions, DataReceivedEventHandler OutputEventHandler)
		{
			using (ProgressWriter Writer = new ProgressWriter("Compiling C++ source files...", false))
			{
				int NumCompletedActions = 0;

				// Create a wrapper delegate that will parse the output actions
				DataReceivedEventHandler EventHandlerWrapper = (Sender, Args) =>
				{
					if (Args.Data != null && Args.Data.StartsWith(ProgressMarkupPrefix))
					{
						Writer.Write(++NumCompletedActions, NumActions);
						Args = ConstructDataReceivedEventArgs(Args.Data.Substring(ProgressMarkupPrefix.Length));
					}
					if(OutputEventHandler != null)
					{
						OutputEventHandler(Sender, Args);
					}
				};

				// Run through the standard XGE executor
				return ExecuteTaskFile(TaskFilePath, EventHandlerWrapper);
			}
		}
	}
}
