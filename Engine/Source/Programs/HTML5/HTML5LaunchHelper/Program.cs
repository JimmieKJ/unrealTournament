using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;
using UnrealBuildTool;

// Example Command Lines
// HTML5LaunchHelper -m -h -b /Applications/Safari.app localhost:8000
// HTML5LaunchHelper -m -h -b /Applications/Firefox.app http://localhost:8000 -silent -profile /Users/james.moran/webporfiles/nightly/UE4HTML5 --jsconsole
// HTML5LaunchHelper -m -h -b /Applications/Google\ Chrome.app http://localhost:8000 --user-data-dir="/Users/james.moran/webporfiles/chrome/UE4HTML5" --enable-logging --no-first-run
namespace HTML5LaunchHelper
{
	class Program
	{
		static private bool 	bMultiprocessBrowser = false;
		static private bool 	bStartPythonServer = false;
		static private int  	PythonServerPort = 8000;
		static private string 	PythonPath = "python";
		static private string 	BrowserPath = null;
		static private string	ServerDirectory = null;
		static private string 	Url = null;
		static private string   AdditionalArgs = "";
		static private List<Process> ProcessesToKill = new List<Process>();
		static private List<Process> ProcessesToWatch = new List<Process>();

		static Process SpawnBrowserProcess(string bpath, string args)
		{
			var Result = new Process();
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
			{
	            Result.StartInfo.FileName = "/usr/bin/open";
				Result.StartInfo.UseShellExecute = false;
				Result.StartInfo.RedirectStandardOutput = true;
				Result.StartInfo.RedirectStandardInput = true;
				Result.StartInfo.Arguments = String.Format("-nW \"{0}\" --args {1}", bpath, args);
				Result.EnableRaisingEvents = true;
			}
			else
			{
	            Result.StartInfo.FileName = bpath;
				Result.StartInfo.UseShellExecute = false;
				Result.StartInfo.RedirectStandardOutput = true;
				Result.StartInfo.RedirectStandardInput = true;
				Result.StartInfo.Arguments = args;
				Result.EnableRaisingEvents = true;
			}
			System.Console.WriteLine("Spawning Browser Process {0} with args {1}\n", bpath, args);
			return Result;
		}

		static int Main(string[] args)
		{
			if (!ParseArgs(args))
			{
				// TODO: print error & options.
				System.Console.Write("Invalid args\n");
				return -1;
			}

			if (bStartPythonServer)
			{
				BeginPythonServer();
				// Give the server time to start. 1 sec should be enough
				System.Threading.Thread.Sleep(1000);
			}

			var bIsSafari = BrowserPath.Contains("Safari");

			// Browsers can be multiprocess programs (Chrome, basically)
			// which we need to catch spawning of other child processes. The trick is
			// they aren't really child-processes. There appears to be no real binding between the two
			// so we kinda fudge it a bit here
			var PrevProcesses = Process.GetProcesses();
			var FirstProcess = SpawnBrowserProcess(BrowserPath, !bIsSafari ? Url + " " + AdditionalArgs : "");
			ProcessesToWatch.Add(FirstProcess);
			FirstProcess.Start();
			var ProcName = FirstProcess.ProcessName;

			if (bIsSafari)
			{
				// Give Safari time to open...
				System.Threading.Thread.Sleep(2000);
				var Proc = new Process();
				Proc.StartInfo.FileName = "/usr/bin/osascript";
				Proc.StartInfo.UseShellExecute = false;
				Proc.StartInfo.RedirectStandardOutput = true;
				Proc.StartInfo.RedirectStandardInput = true;
				Proc.StartInfo.Arguments = String.Format("-e 'tell application \"Safari\" to open location \"{1}\"'", FirstProcess.Id, Url);
				Proc.EnableRaisingEvents = true;
				Proc.Start();
				Proc.WaitForExit();	
			}

			// We should now have a list of processes to watch to exit.
			// Loop over the calling WaitForExit() until the list is empty.
			while (ProcessesToWatch.Count() > 0)
			{
				for (var i=0; i<ProcessesToWatch.Count(); ++i)
				{
					if (ProcessesToWatch[i].HasExited)
					{
						ProcessesToWatch.RemoveAt(i);
					}
				}

				if (bMultiprocessBrowser && FirstProcess != null && FirstProcess.HasExited)
				{
					var CurrentProcesses = Process.GetProcesses();
					foreach (var item in CurrentProcesses)
					{
						var bWasAlive = false;
						foreach (var pitem in PrevProcesses)
						{
							if (pitem.Id == item.Id)
							{
								bWasAlive = true;
							}
						}
						if (!bWasAlive)
						{
							try {
								if (!item.HasExited && item.ProcessName.StartsWith(ProcName))
								{
									var PID = item.Id;
									System.Console.WriteLine("Found Process {0} with PID {1} which started at {2}. Waiting on that process to end.", item.ProcessName, item.Id, item.StartTime.ToString());
									ProcessesToWatch.Add(item);
								}
							} catch {}
						}
					}
					FirstProcess = null;
				}

				//If any service processes have died this is considered an error
				foreach (var proc in ProcessesToKill)
				{
					if (proc.HasExited)
					{
						System.Console.WriteLine("A spawned thread has died. Do you have a python server instance running?");
						HardShutdown();
						return -1;
					}
				}

				System.Threading.Thread.Sleep(250);
			}
			System.Console.WriteLine("All processes being watched have exited.\n");

			//All processes we cared about have finished so clean up the services we spawned for them.
			foreach (var proc in ProcessesToKill)
			{
				if (!proc.HasExited)
				{
					System.Console.WriteLine("Killing spawned process {0}.\n", proc.Id);
					proc.Kill();
					proc.WaitForExit();
				}
			}

			return 0;
		}

		static void HardShutdown()
		{
			foreach (var proc in ProcessesToKill)
			{
				if (!proc.HasExited)
				{
					proc.Kill();
					proc.WaitForExit();
				}
			}

			foreach (var proc in ProcessesToWatch)
			{
				if (!proc.HasExited)
				{
					proc.Kill();
					proc.WaitForExit();
				}
			}
		}

		static void BeginPythonServer()
		{
			var Result = new Process();
            Result.StartInfo.FileName = PythonPath;
			Result.StartInfo.UseShellExecute = false;
			Result.StartInfo.RedirectStandardOutput = true;
			Result.StartInfo.RedirectStandardInput = true;
			Result.StartInfo.Arguments = String.Format("-m SimpleHTTPServer {0}", PythonServerPort);
			Result.StartInfo.WorkingDirectory = ServerDirectory;
			Result.EnableRaisingEvents = true;
			Result.Start();
			System.Console.Write("Started New Process (PID={0} Name={1})\n", Result.Id, Result.ProcessName);
			ProcessesToKill.Add(Result);
		}

		static bool ParseArgs(string[] args)
		{
			bool bOptionalPythonName = false;
			bool bParseBrowserPath = false;
			bool bParseServerDir = false;
			bool bParseServerPort = false;

			foreach (var arg in args)
			{
				var a = arg.Trim();
				if (bOptionalPythonName || bParseBrowserPath || bParseServerDir || bParseServerPort)
				{
					if (!a.StartsWith("-"))
					{
						if (bOptionalPythonName)
						{
							PythonPath = a;
						}
						else if (bParseBrowserPath)
						{
							BrowserPath = a;
						}
						else if (bParseServerDir)
						{
							ServerDirectory = a;
						}
						else if (bParseServerPort)
						{
							PythonServerPort = Convert.ToInt32(a);
						}
					}
					bOptionalPythonName = bParseBrowserPath = bParseServerDir = bParseServerPort = false;
				}
				else if (a == "-m")
				{
					bMultiprocessBrowser = true;
				}
				else if (a == "-p")
				{
					bOptionalPythonName = true;
				}
				else if (a == "-h")
				{
					bStartPythonServer = true;
				}
				else if (a == "-b")
				{
					bParseBrowserPath = true;
				}
				else if (a == "-w")
				{
					bParseServerDir = true;
				}
				else if (a == "-s")
				{
					bParseServerPort = true;
				}
				else if (string.IsNullOrEmpty(Url))
				{
					Url = a;
				}
				else
				{
					if (a.StartsWith ("-") && a.Contains('=') && a.Contains(' ')) 
					{
						var Parts = a.Split('=');
						a = "";
						for (var p = 0; p < Parts.Count(); ++p)
						{
							if (Parts [p].Contains(' '))
							{
								a += "\""+Parts[p]+"\"";
							}
							else
							{
								a += Parts[p];
							}
							if (p < Parts.Count() - 1)
							{
								a += '=';
							}
						}
					}
					else if (a.Contains(' '))
					{
						a = "\""+a+"\"";
					}
					AdditionalArgs += a + " ";
				}
			}

			return !string.IsNullOrEmpty(BrowserPath) && !string.IsNullOrEmpty(Url) && !string.IsNullOrEmpty(ServerDirectory);
		}
	}
}
