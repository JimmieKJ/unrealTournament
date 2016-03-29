// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Configuration.Install;
using System.Linq;
using System.ServiceProcess;

namespace Tools.CrashReporter.CrashReportReceiver
{
	/// <summary>
	/// An installer class for the CrashReportReceiver.
	/// </summary>
	[RunInstaller( true )]
	public partial class CrashReportReceiverServiceInstaller : System.Configuration.Install.Installer
	{
		private ServiceInstaller Installer;
		private ServiceProcessInstaller ProcessInstaller;

		/// <summary>
		/// Set all the parameters required to install the service.
		/// </summary>
		public CrashReportReceiverServiceInstaller()
		{
			InitializeComponent();

			Installer = new ServiceInstaller();
			Installer.StartType = ServiceStartMode.Automatic;
			Installer.ServiceName = "CrashReportReceiverService";
			Installer.DisplayName = "CrashReport Receiver Service";
			Installer.Description = "A web service that receives files and copies them locally for later processing.";
			Installers.Add( Installer );

			ProcessInstaller = new ServiceProcessInstaller();
			ProcessInstaller.Account = ServiceAccount.NetworkService;
			Installers.Add( ProcessInstaller );
		}

		/// <summary>
		/// Must exist, because installation will fail without it.
		/// </summary>
		/// <param name="StateSaver">Passed through to the base method.</param>
		public override void Install( IDictionary StateSaver )
		{
			base.Install( StateSaver );
		}
	}
}
