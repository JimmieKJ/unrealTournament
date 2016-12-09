// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace Tools.CrashReporter.CrashReportProcess
{
	partial class CrashReporterProcessServicer
	{
		/// <summary> 
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose( bool disposing )
		{
			if( disposing && ( components != null ) )
			{
				components.Dispose();

				// If the process has started, all three disposable objects will be valid
				// I would just call OnStop, but the code analysis tool doesn't look inside
				// it and thinks the members aren't being Disposed.
				if (!bDisposing && !bDisposed)
				{
					OnDispose();
				}
			}
			base.Dispose( disposing );
		}

		#region Component Designer generated code

		/// <summary> 
		/// Required method for Designer support - do not modify 
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			components = new System.ComponentModel.Container();
			this.ServiceName = "CrashReportProcessService";
		}

		#endregion
	}
}
