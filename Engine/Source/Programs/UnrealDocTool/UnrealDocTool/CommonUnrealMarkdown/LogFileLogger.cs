// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Text;
using System.IO;
using System.Reflection;
using System.Diagnostics;

namespace CommonUnrealMarkdown
{
    public class LogFileLogger : Logger
    {
        public LogFileLogger()
        {
            FileInfo LogFileInfo = new FileInfo(Path.Combine((new Uri(Path.GetDirectoryName(Assembly.GetAssembly(typeof(Logger)).CodeBase))).LocalPath, Path.Combine("UnrealDocToolLogs", "Log.txt")));

            DirectoryInfo LogDirectory = LogFileInfo.Directory;

            if (!LogDirectory.Exists)
            {
                LogDirectory.Create();
            }

            //if there is a previous log file check the date last accessed and if not today append the date last accessed to the logfile.
            if (LogFileInfo.Exists)
            {
                DateTime LastFileAccessed = LogFileInfo.LastWriteTimeUtc.Date;
                if (LastFileAccessed != System.DateTime.Now.Date)
                {
                    String NewFileName = String.Format("{0}{1:0000}{2:00}{3:00}", LogFileInfo.FullName, LastFileAccessed.Year, LastFileAccessed.Month, LastFileAccessed.Day);
                    if (File.Exists(NewFileName))
                    {
                        //Copy the newer text file to bottom of old log, this part of code should not really be entered if under normal running conditions
                        StreamWriter OldLogFile = new StreamWriter(NewFileName, true, Encoding.ASCII);
                        string[] NewLogFile = File.ReadAllLines(LogFileInfo.FullName);
                        foreach (string LogLine in NewLogFile)
                        {
                            OldLogFile.WriteLine(LogLine);
                        }
                        OldLogFile.Flush();
                        OldLogFile.Close();
                        LogFileInfo.Delete();
                    }
                    else
                    {
                        LogFileInfo.MoveTo(NewFileName);
                    }
                }
            }
        }

        override public void WriteToLog(string Text)
        {
            Trace.WriteLine(Text);
            Trace.Flush();
        }

        override public void ClearLog()
        {
        }
    }
}