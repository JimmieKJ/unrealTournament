/**
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Runtime.Remoting;
using System.Runtime.Remoting.Channels;
using System.Runtime.Remoting.Channels.Ipc;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace DeploymentServer
{
	internal class CoreFoundation
	{
		static public CoreFoundationImpl CoreImpl;

		static CoreFoundation()
		{
			if (Environment.OSVersion.Platform == PlatformID.MacOSX || Environment.OSVersion.Platform == PlatformID.Unix)
			{
				CoreImpl = new CoreFoundationOSX();
			}
			else
			{
				CoreImpl = new CoreFoundationWin32();
			}
		}

		public static int RunLoopRunInMode(IntPtr mode, double seconds, int returnAfterSourceHandled)
		{
			return CoreImpl.RunLoopInMode(mode, seconds, returnAfterSourceHandled);
		}

		public static IntPtr kCFRunLoopDefaultMode()
		{
			return CoreImpl.DefaultMode();
		}
	};

	internal interface CoreFoundationImpl
	{
		int RunLoopInMode(IntPtr mode, double seconds, int returnAfterSourceHandled);
		IntPtr DefaultMode();
	}

	internal class CoreFoundationOSX : CoreFoundationImpl
	{
		public int RunLoopInMode(IntPtr mode, double seconds, int returnAfterSourceHandled)
		{
			return CFRunLoopRunInMode(mode, seconds, returnAfterSourceHandled);
		}

		public IntPtr DefaultMode()
		{
			return kCFRunLoopDefaultMode;
		}

		[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")]
		public extern static IntPtr CFStringCreateWithCString(IntPtr allocator, string value, int encoding);

		[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")]
		public extern static void CFRunLoopRun();

		[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")]
		public extern static IntPtr CFRunLoopGetMain();

		[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")]
		public extern static IntPtr CFRunLoopGetCurrent();

		[DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")]
		public extern static int CFRunLoopRunInMode(IntPtr mode, double seconds, int returnAfterSourceHandled);

		public static IntPtr kCFRunLoopDefaultMode = CFStringCreateWithCString(IntPtr.Zero, "kCFRunLoopDefaultMode", 0);
	}

	internal class CoreFoundationWin32 : CoreFoundationImpl
	{
		public int RunLoopInMode(IntPtr mode, double seconds, int returnAfterSourceHandled)
		{
			return CFRunLoopRunInMode(mode, seconds, returnAfterSourceHandled);
		}

		public IntPtr DefaultMode()
		{
			return kCFRunLoopDefaultMode;
		}

		[DllImport("CoreFoundation.dll")]
		public extern static IntPtr CFStringCreateWithCString(IntPtr allocator, string value, int encoding);

		[DllImport("CoreFoundation.dll")]
		public extern static void CFRunLoopRun();

		[DllImport("CoreFoundation.dll")]
		public extern static IntPtr CFRunLoopGetMain();

		[DllImport("CoreFoundation.dll")]
		public extern static IntPtr CFRunLoopGetCurrent();

		[DllImport("CoreFoundation.dll")]
		public extern static int CFRunLoopRunInMode(IntPtr mode, double seconds, int returnAfterSourceHandled);

		public static IntPtr kCFRunLoopDefaultMode = CFStringCreateWithCString(IntPtr.Zero, "kCFRunLoopDefaultMode", 0);
	}

	class Program
    {
		static int ExitCode = 0;

        static int Main(string[] args)
        {
            if ((args.Length == 2) && (args[0].Equals("-iphonepackager")))
            {
				try
				{
					// We were run as a 'child' process, quit when our 'parent' process exits
					// There is no parent-child relationship WRT windows, it's self-imposed.
					int ParentPID = int.Parse(args[1]);

					IpcServerChannel Channel = new IpcServerChannel("iPhonePackager");
					ChannelServices.RegisterChannel(Channel, false);
					RemotingConfiguration.RegisterWellKnownServiceType(typeof(DeploymentImplementation), "DeploymentServer_PID" + ParentPID.ToString(), WellKnownObjectMode.Singleton);

					Process ParentProcess = Process.GetProcessById(ParentPID);
					while (!ParentProcess.HasExited)
					{
						System.Threading.Thread.Sleep(1000);
					}
				}
				catch (System.Exception)
				{
					
				}
            }
            else
            {
				// Parse the command
				if (ParseCommand(args))
				{
					Deployer = new DeploymentImplementation();
					bool bCommandComplete = false;
					System.Threading.Thread enumerateLoop = new System.Threading.Thread(delegate()
					{
						RunCommand();
						bCommandComplete = true;
					});
					enumerateLoop.Start();
					while (!bCommandComplete)
					{
						CoreFoundation.RunLoopRunInMode(CoreFoundation.kCFRunLoopDefaultMode(), 1.0, 0);
					}
				}
                Console.WriteLine("Exiting.");
            }

			Environment.ExitCode = Program.ExitCode;
			return Program.ExitCode;
        }

		private static string Command = "";
		private static List<string> FileList = new List<string>();
		private static string Bundle = "";
		private static string Manifest = "";
		private static string ipaPath = "";
		private static string Device = "";
		private static bool ParseCommand(string[] Arguments)
		{
			if (Arguments.Length >= 1)
			{
				Command = Arguments[0].ToLowerInvariant();
				for (int ArgIndex = 1; ArgIndex < Arguments.Length; ArgIndex++)
				{
					string Arg = Arguments[ArgIndex].ToLowerInvariant();
					if (Arg.StartsWith("-"))
					{
						switch (Arg)
						{
							case "-file":
								if (Arguments.Length > ArgIndex + 1)
								{
									FileList.Add(Arguments[++ArgIndex]);
								}
								else
								{
									return false;
								}
								break;

							case "-bundle":
								if (Arguments.Length > ArgIndex + 1)
								{
									Bundle = Arguments[++ArgIndex];
								}
								else
								{
									return false;
								}
								break;

							case "-manifest":
								if (Arguments.Length > ArgIndex + 1)
								{
									Manifest = Arguments[++ArgIndex];
								}
								else
								{
									return false;
								}
								break;

							case "-ipa":
								if (Arguments.Length > ArgIndex + 1)
								{
									ipaPath = Arguments[++ArgIndex];
								}
								else
								{
									return false;
								}
								break;

							case "-device":
								if (Arguments.Length > ArgIndex + 1)
								{
									Device = Arguments[++ArgIndex];
								}
								else
								{
									return false;
								}
								break;
						}
					}
				}
			}
			return true;
		}

		private static DeploymentImplementation Deployer = null;
		private static void RunCommand()
		{
			if (!String.IsNullOrEmpty(Device) && !Device.Contains("All_iOS_On"))
			{
				Deployer.DeviceId = Device;
			}

			bool bResult = true;
			switch (Command)
			{
				case "backup":
					bResult = Deployer.BackupFiles(Bundle, FileList.ToArray());
					break;

				case "deploy":
					bResult = Deployer.InstallFilesOnDevice(Bundle, Manifest);
					break;

				case "install":
					bResult = Deployer.InstallIPAOnDevice(ipaPath);
					break;

				case "enumerate":
					Deployer.EnumerateConnectedDevices();
					break;
			}

			Program.ExitCode = bResult ? 0 : 1;
		}
    }
}
