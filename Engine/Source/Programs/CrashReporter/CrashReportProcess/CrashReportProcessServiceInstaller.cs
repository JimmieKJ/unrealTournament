// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Configuration.Install;
using System.Linq;
using System.ServiceProcess;

namespace Tools.CrashReporter.CrashReportProcess
{
	/// <summary>
	/// The containing class for the Crash Report Processor installer.
	/// </summary>
	[RunInstaller( true )]
	public partial class CrashReportProcessServiceInstaller : System.Configuration.Install.Installer
	{
		private ServiceInstaller Installer;
		private ServiceProcessInstaller ProcessInstaller;

		/// <summary>
		/// The installer to handle the installation of the Crash Report Processor service.
		/// </summary>
		public CrashReportProcessServiceInstaller()
		{
			InitializeComponent();

			Installer = new ServiceInstaller();
			Installer.StartType = ServiceStartMode.Automatic;
			Installer.ServiceName = "CrashReportProcessService";
			Installer.DisplayName = "CrashReport Processor Service";
			Installer.Description = "A web service that processes crash reports for display on the crash report website.";
			Installers.Add( Installer );

			ProcessInstaller = new ServiceProcessInstaller();
			ProcessInstaller.Account = ServiceAccount.NetworkService;
			Installers.Add( ProcessInstaller );
		}

		/// <summary>
		/// Must exists, because installation will fail without it.
		/// </summary>
		/// <param name="StateSaver">Dictionary to pass down to the base class.</param>
		public override void Install( IDictionary StateSaver )
		{
			base.Install( StateSaver );
		}
	}
}
