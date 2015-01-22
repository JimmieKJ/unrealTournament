// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using Microsoft.Win32;

namespace Tools.CrashReporter.RegisterPII
{
	/// <summary>
	/// The Windows form used to display basic feedback.
	/// </summary>
	/// <remarks>This retrieves a GUID from the registry key 'HKLM/SOFTWARE/Microsoft/Windows/Windows Error Reporting/MachineID' that is used in Windows Error Reporting.
	/// This allows the crash report web site to display user names and machine names, rather than 'Anonymous' and a GUID, for better tracking of local crashes.
	/// It first tries the 64 bit hive, and then the 32 bit hive if the 64 bit hive version was not found.</remarks>
	public partial class RegisterPII : Form
	{
		// A default empty guid
		private string EmptyGuid = "00000000-0000-0000-0000-000000000000";

		/// <summary>The local machine GUID used by Windows Error Reporting (a.k.a. WER).</summary>
		public string MachineIDString = "";

		/// <summary>
		/// Retrieve the machine guid from the HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting registry key.
		/// </summary>
		/// <param name="View">Whether to look in the x64 or x86 registry hive.</param>
		private void GetMachineGuid( RegistryView View )
		{
			// The the machine guid from the registry
			MachineIDString = EmptyGuid;

			try
			{
				RegistryKey Key = RegistryKey.OpenBaseKey( RegistryHive.LocalMachine, RegistryView.Registry64 );
				if( Key != null )
				{
					RegistryKey SubKey = Key.OpenSubKey( "SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting" );
					if( SubKey != null )
					{
						MachineIDString = ( string )SubKey.GetValue( "MachineID", EmptyGuid );
						SubKey.Close();
					}

					Key.Close();
				}
			}
			catch
			{
			}
		}

		/// <summary>
		/// Construct a dialog containing the relevant information we are mapping.
		/// </summary>
		public RegisterPII()
		{
			InitializeComponent();

			ContentLabel.Text = "Mapping user '" + Environment.UserName + "' and machine name '" + Environment.MachineName + "'";

			// Retrieve the machine guid from the registry
			GetMachineGuid( RegistryView.Registry64 );
			if( MachineIDString == EmptyGuid )
			{
				GetMachineGuid( RegistryView.Registry32 );
			}

			MachineGUIDLabel.Text = "to machine '" + MachineIDString + "'";
		}
	}
}
