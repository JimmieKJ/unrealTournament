// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Net;
using System.IO;
using System.Diagnostics;
using System.Collections.Generic;

namespace AutoReporter
{
	class UploadReportFiles
	{

		string StripPath(string PathName)
		{
			int LastBackSlashIndex = PathName.LastIndexOf("\\");
			int LastForwardSlashIndex = PathName.LastIndexOf("/");

			if (LastBackSlashIndex > LastForwardSlashIndex && LastBackSlashIndex >= 0)
			{
				return PathName.Substring(LastBackSlashIndex + 1);
			}
			else if (LastForwardSlashIndex >= 0)
			{
				return PathName.Substring(LastForwardSlashIndex + 1);
			}
			return PathName;
		}

		public bool CanAccessFile(string ProcessName, bool bWasAnEnsure, Int32 InProcessID, string Filename, OutputLogFile LogFile)
		{
			LogFile.WriteLine("Checking to see if " + Filename + " is Accessible");

			bool bWasSuccessful = true;

			string CheckTempFilename = Filename + "_AutoReportCheck.bak";

			FileInfo CheckTempFileInfo = new FileInfo(CheckTempFilename);
			if (CheckTempFileInfo.Exists)
			{
				// Delete any potentially old files laying around
				CheckTempFileInfo.IsReadOnly = false;
				CheckTempFileInfo.Delete();
			}

			FileInfo SourceFileInfo = new FileInfo(Filename);

			if(SourceFileInfo.Exists)
			{
				Int32 CopyCount = 0;
				Int64 CopySize = 0;
				while ((CopySize == 0) && (CopyCount < 10))
				{
					try
					{
						File.Copy(Filename, CheckTempFilename, true);
					}
					catch (Exception e)
					{
						LogFile.WriteLine(e.Message);
						LogFile.WriteLine("Couldn't copy file " + Filename + " to " + CheckTempFilename + ", continuing...");
					}
					CheckTempFileInfo = new FileInfo(CheckTempFilename);
					if(CheckTempFileInfo.Exists)
					{
						try
						{
							CopySize = CheckTempFileInfo.Length;
						}
						catch (IOException)
						{
							LogFile.WriteLine("Could not Access file: " + CheckTempFilename + " to get it's size.");
						}
					}
				
					CopyCount++;
					if (CopySize == 0)
					{
						// Sleep for a bit before trying again
						System.Threading.Thread.Sleep(30);
					}
				}

				LogFile.WriteLine("Temp file file of " + CopySize + " bytes saved in " + CopyCount + " tries to " + CheckTempFilename + ".");
				if (CopySize == 0)
				{
					LogFile.WriteLine("*** Failed to copy file file: " + CheckTempFilename);

					// Find any of these processes and log out the results.
					// This is to assist in determining possible fixes for the 'missing crash log' cases.
					if (bWasAnEnsure == false)
					{
						LogFile.WriteLine("Looking for the process " + ProcessName + "...");

						// Kill the process so we can get the log file...
						Process[] processList = Process.GetProcesses();
						List<Process> potentialProcesses = new List<Process>();
						foreach (Process CheckProcess in processList)
						{
							LogFile.WriteLine("\t" + CheckProcess.ProcessName);
							if (CheckProcess.ProcessName == ProcessName)
							{
								potentialProcesses.Add(CheckProcess);
							}
						}

						LogFile.WriteLine("Found " + potentialProcesses.Count + " apps...");
						foreach (Process foundProcess in potentialProcesses)
						{
							bool bKillIt = false;
							try
							{
								LogFile.WriteLine("*** " + foundProcess.Id.ToString());
								if (foundProcess.HasExited == false)
								{
									LogFile.WriteLine("\tHandle        " + foundProcess.Handle.ToString());
									LogFile.WriteLine("\tWorkingDir    " + foundProcess.StartInfo.WorkingDirectory);
									// If the app is not responding, it is likely our crashed app...
									// We could potentially kill it to allow for the log to be uploaded.
									LogFile.WriteLine("\tResponding    " + foundProcess.Responding.ToString());
									bKillIt = !foundProcess.Responding;
								}
								else
								{
									LogFile.WriteLine("\tEXITED!");
								}

								// Only kill it if it matches the process Id passed in!
								bKillIt &= (foundProcess.Id == InProcessID);
							}
							catch (System.Exception /*ex*/)
							{
								// Process my have exited by now...
							}

							if (bKillIt)
							{
								LogFile.WriteLine("Killing process to ensure log file upload: " + foundProcess.Id.ToString() + " " +
												  foundProcess.ProcessName);
								foundProcess.Kill();
								foundProcess.WaitForExit();

								// See if killing the thread worked 
								try
								{
									File.Copy(Filename, CheckTempFilename, true);
									bWasSuccessful = true;
								}
								catch (Exception e)
								{
									LogFile.WriteLine(e.Message);
									LogFile.WriteLine("Couldn't copy log " + Filename + " to " + CheckTempFilename + ", continuing...");
									bWasSuccessful = false;
								}
								// Temporary - trying to determine how some people are uploading empty log files...
								{
									FileInfo TempInfo = new FileInfo(CheckTempFilename);
									if (TempInfo.Exists)
									{
										LogFile.WriteLine("----------");
										try
										{
											LogFile.WriteLine("TempFile: Size = " + TempInfo.Length + " for " + CheckTempFilename);
										}
										catch (IOException)
										{
											LogFile.WriteLine("Could not Access file: " + CheckTempFilename + " to get it's size."); 
										}
										
										LogFile.WriteLine("----------");
									}
									else
									{
										LogFile.WriteLine("TempLog: Does not exist!");
									}
								}
							}
						}
					}
				}
			}
			else
			{
				LogFile.WriteLine("File: " + Filename + " does not exist.");
				bWasSuccessful = false;
			}

			return bWasSuccessful;
		}

		public bool UploadFile(string HeaderName, string FileNameTemplate, string Filename, OutputLogFile LogFile, WebClient Client)
		{
			//If debugging use local server
#if DEBUG
			string UploadReportURL = Properties.Settings.Default.UploadURL_Debug;
#else
			string UploadReportURL = Properties.Settings.Default.UploadURL;
#endif

			byte[] responseArray;
			bool bWasSuccessful = false;

			if (Client == null)
			{
				LogFile.WriteLine("Null WebClient Passed to UploadFile.");
				return false;
			}
			if (Client.IsBusy)
			{
				LogFile.WriteLine("Client is Busy");
			}

			// Must add Header for the web service to know what to do with the file
			Client.Headers.Add(HeaderName, FileNameTemplate);

			var CheckFileInfo = new FileInfo(Filename);
			long FileSize = 0;

			if(CheckFileInfo.Exists)
			{
				try
				{
					FileSize = CheckFileInfo.Length;
				}
				catch (IOException)
				{
					LogFile.WriteLine("Could not Access file: " + Filename +" to get it's size.");
					bWasSuccessful = false;
				}
				
			}

			if (FileSize > 0)
			{
				LogFile.WriteLine("Attempting to upload File: " + Filename + " of size:" + FileSize/1024/1024 + "MB to " +
				                  UploadReportURL);
				try
				{
					if(UploadReportURL != null && Filename != null)
					{
						responseArray = Client.UploadFile(UploadReportURL, "POST", Filename);
						bWasSuccessful = true;
						LogFile.WriteLine("Successfully to uploaded file to: " + UploadReportURL);
					}
					else
					{
                        bWasSuccessful = false;
						LogFile.WriteLine("UploadReportURL or Filename were NULL");
					}
				}
				catch(NullReferenceException nrE)
				{
                    bWasSuccessful = false;
					LogFile.WriteLine("Upload File : "+Filename+"Caused a Null Reference exception" + nrE.Message);
				}
				catch (FileNotFoundException fnfE)
				{
                    bWasSuccessful = false;
					LogFile.WriteLine("Could Not Find file: " + fnfE.FileName);
				}
				catch (WebException ex)
				{
                    bWasSuccessful = false;
					WebException webEx = (WebException) ex;
					LogFile.WriteLine("-------");
					LogFile.WriteLine("Response: " + webEx.Response.ToString());
					LogFile.WriteLine("Status: " + webEx.Status.ToString());
					LogFile.WriteLine("Message: " + webEx.Message);
					if(webEx.InnerException != null)
					{
						LogFile.WriteLine("InnerException: " + webEx.InnerException.Message);
					}
					LogFile.WriteLine("-------");
				}
				catch (Exception e)
				{
                    bWasSuccessful = false;
					LogFile.WriteLine("Couldn't upload due to Unknown Exception: " + e.Message);
				}
			}
			else
			{
				LogFile.WriteLine("Could Not upload file because"+ Filename + " was 0 bytes. Likely because it has been overwritten or cleared between testing the filesize and uploading it.");
			}

			// Remove the header so that the next upload call starts afresh
			Client.Headers.Remove(HeaderName);

			return bWasSuccessful;
		}


		/** 
		 * UploadFiles - uploads the two files using HTTP POST
		 * 
		 * @param LogFilename - the local log file
		 * @param IniFilename - the local ini file
		 * @param uniqueID - the id of the crash report associated with these files. 
		 * @return bool - true if successful
		 */
		public bool UploadFiles(string ProcessName, bool bWasAnEnsure, Int32 InProcessID, string LogFilename, string IniFilename, string MiniDumpFilename, string CrashVideoFilename, int uniqueID, OutputLogFile logFile)
		{
			logFile.WriteLine("Uploading files...");
			logFile.WriteLine("\t" + LogFilename);
			logFile.WriteLine("\t" + IniFilename);
			logFile.WriteLine("\t" + MiniDumpFilename);
			logFile.WriteLine("\t" + CrashVideoFilename);

			

			Boolean bWasSuccessful = false;
			Boolean bUploadedLog = true;
	
			Boolean bUploadedVideo = true;
			Boolean bUploadedMiniDump = true;
			Boolean bCanAccessLog;
			Boolean bCanAccessVideo;
			Boolean bCanAccessMiniDump;
			
			bCanAccessLog = CanAccessFile(ProcessName, bWasAnEnsure, InProcessID, LogFilename, logFile);
			bCanAccessVideo = CanAccessFile(ProcessName, bWasAnEnsure, InProcessID, CrashVideoFilename, logFile);
			bCanAccessMiniDump = CanAccessFile(ProcessName, bWasAnEnsure, InProcessID, MiniDumpFilename, logFile);

			WebClient client = new WebClient();
				
			// Send the Crash Id up so that the uploader can name the files correctly.
			// This used to save crash files to their own unique folder, but that method has been replaced with appending _<crashId> to the filename.
			client.Headers.Add("NewFolderName", uniqueID.ToString());

			if(bCanAccessLog)
			{
				try
				{
					bUploadedLog = UploadFile("LogName", StripPath(LogFilename), LogFilename, logFile, client);
				}
				catch (NullReferenceException nrE)
				{
                    bUploadedLog = false;
					logFile.WriteLine("Uploading "+ LogFilename+ " caused Null Reference Exception: "+nrE.Message);
				}
			}
			else
			{
				bUploadedLog = false;
				logFile.WriteLine("Could not access File: " + LogFilename);
			}

			// Upload Crash Video
			if(bCanAccessVideo)
			{
				try
				{
					bUploadedVideo = UploadFile("SaveFileName", "CrashVideo.avi", CrashVideoFilename, logFile, client);
				}
				catch (NullReferenceException nrE)
				{
					logFile.WriteLine("Uploading "+ CrashVideoFilename+ " caused Null Reference Exception: "+nrE.Message);
				}

				if(!bUploadedVideo)
				{
					string NetworkPath = @"\\devweb-02\ReportVideos\";
					try
					{
						// Try copying over the network
						// \\devweb-02\ReportVideos
						
						if(!String.IsNullOrEmpty(CrashVideoFilename) )
						{
							logFile.WriteLine("Attempting to copy file " + CrashVideoFilename + " over the network to " +NetworkPath );
							File.Copy(CrashVideoFilename, NetworkPath + uniqueID.ToString()+ "_"+StripPath(CrashVideoFilename));
							bUploadedVideo = true;
						}
						else
						{
							logFile.WriteLine("Argument Null or Empty");
						}
					}
					catch(Exception e)
					{
						logFile.WriteLine(e.ToString());
						logFile.WriteLine("Couldn't copy crash video to network...");
						try
						{
							File.Copy(CrashVideoFilename, CrashVideoFilename+"_" + uniqueID.ToString(), true);
						}
						catch (Exception copyFileException)
						{
							logFile.WriteLine(copyFileException.Message);
							logFile.WriteLine("Couldn't copy File " + CrashVideoFilename + ", continuing...");
						}
						throw new Exception("Failed to upload Crash Video to " + NetworkPath + uniqueID.ToString() + "_" + StripPath(CrashVideoFilename) + "after a web and a network copy attempts");
					}
				}
			}
			else
			{
				bUploadedVideo  = false;

				var CheckFileInfo = new FileInfo(CrashVideoFilename);
				if (CheckFileInfo.Exists)
				{
					logFile.WriteLine("Could not access File: " + CrashVideoFilename);
				}
				else
				{
					logFile.WriteLine("File: " + CrashVideoFilename+" does not exist.");
				}
			}

			// Upload Minidump
			if (bCanAccessMiniDump)
			{
				try
				{
					bUploadedMiniDump = UploadFile("SaveFileName", "MiniDump.dmp", MiniDumpFilename, logFile, client);
				}
				catch (NullReferenceException nrE)
				{
                    bUploadedMiniDump = false;
                    logFile.WriteLine("Uploading "+ MiniDumpFilename+ " caused Null Reference Exception: "+nrE.Message);
				}
			}
			else
			{
				bUploadedMiniDump = false;
				logFile.WriteLine("Could not access File: " + LogFilename);
			}

			//Upload file successes
			try
			{
				ReportService.RegisterReport reportService = new ReportService.RegisterReport();
				try
				{
					bWasSuccessful = reportService.AddCrashFiles(uniqueID, bUploadedLog, bUploadedMiniDump, bUploadedVideo);
					
				}
				catch (Exception e)
				{
					throw e;
				}

			} catch (WebException webEx) {
				
				logFile.WriteLine(webEx.Message);
				logFile.WriteLine(webEx.InnerException.Message);
				bWasSuccessful = false;
			}

			// Temporary - write out log file contents to autoreporter log
			logFile.WriteLine("");
			logFile.WriteLine("----------");
			logFile.WriteLine("Log file: " + LogFilename);
			logFile.WriteLine("----------");

			Int32 FailCount = 0;
			bool bWroteLogFile = false;
			bool bFileExists = false;
			Int64 FileSize = 0;
			while ((FailCount < 10) && !bWroteLogFile)
			{
				FileInfo RealLogInfo = new FileInfo(LogFilename);
				bFileExists |= RealLogInfo.Exists;
				if(bFileExists)
				{
					try
					{
						FileSize = System.Math.Max(FileSize, RealLogInfo.Length);
					}
					catch (IOException)
					{
						logFile.WriteLine("Could not Access file: " + LogFilename + " to get it's size.");
					}
					
				}
				
				if (RealLogInfo.Exists && (FileSize > 0))
				{
					using (StreamReader sr = RealLogInfo.OpenText())
					{
						while (sr.Peek() > 0)
						{
							logFile.WriteLine(sr.ReadLine());
						}
					}
					logFile.WriteLine("----------");
					bWroteLogFile = true;
				}
				else
				{
					FailCount++;
					// Sleep for a second before trying again
					System.Threading.Thread.Sleep(1000);
				}
			}

			if (bWroteLogFile == false)
			{
				logFile.WriteLine("Attempted to write log file to autoreport log failed.");
				logFile.WriteLine("\tFile does " + (bFileExists ? "" : "NOT") + " Exist");
				logFile.WriteLine("\tFile size = " + FileSize);
			}

			return bWasSuccessful;
		}
	}
}
