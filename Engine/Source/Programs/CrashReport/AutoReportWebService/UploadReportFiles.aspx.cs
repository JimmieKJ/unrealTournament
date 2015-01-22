// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Configuration;
using System.IO;
using System.Linq;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;

namespace AutoReportWebService
{
	public partial class UploadReportFiles : System.Web.UI.Page
	{
		/**
		 * Uploads files and places them in the folder specified by the NewFolderName key
		 * in the HTTP header.
		 */
		protected void Page_Load(object sender, EventArgs e)
		{
			string ErrorLogFileName = ConfigurationManager.AppSettings["LogFileName"];

			StreamWriter errorLogFile = null;
			try
			{
				errorLogFile = new StreamWriter(ErrorLogFileName, true);
			}
			catch (Exception)
			{
				// Ignore cases where the log file can't be opened (another instance may be writing to it already)
			}

			string NewFolderName = "";
			string SaveFileName = "";
			string VideoFileKeyword = "CrashVideo";
			string LogFileName = "Launch.log";

			foreach (string headerString in Request.Headers.AllKeys)
			{
				if (headerString.Equals("NewFolderName"))
				{
					NewFolderName = Request.Headers[headerString];
				}
				else if (headerString.Equals("SaveFileName"))
				{
					SaveFileName = Request.Headers[headerString];
				}
				else if (headerString.Equals("LogName"))
				{
					//get the log filename from the header
					LogFileName = Request.Headers[headerString];
				}
			}

			if (NewFolderName.Length > 0)
			{
				string SaveFilesPath = ConfigurationManager.AppSettings["SaveFilesPath"];
				string NewPath = SaveFilesPath + NewFolderName;

				foreach (string fileString in Request.Files.AllKeys)
				{
					HttpPostedFile file = Request.Files[fileString];
					if (SaveFileName.Length == 0)
					{
						SaveFileName = file.FileName;
					}
					//if this is the log file, rename it so that it can be identified by the web site
					if (file.FileName.Contains(LogFileName))
					{
						SaveFileName = "Launch.log";
					}

					// If this is the Video file, set the save path to the video save path.
					if (file.FileName.Contains(VideoFileKeyword))
					{
						SaveFilesPath = ConfigurationManager.AppSettings["VideoFilesPath"];
						NewPath = SaveFilesPath + NewFolderName;
					}

					try
					{
						file.SaveAs(NewPath + "_" + SaveFileName);
					}
					catch (IOException IOException)
					{
						//TODO Do something here
						if (errorLogFile != null)
						{
							errorLogFile.WriteLine("Could not Save Log File:");
							errorLogFile.WriteLine(IOException.Message);
							errorLogFile.Close();
						}
					}
				}
			}
		}
	}

}