// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Reflection;

namespace LogTools
{
    class Logger
    {
        /// <summary>
        /// Instantiator
        /// </summary>
        public Logger()
        {

            LogFileName = Path.Combine((new Uri(Path.GetDirectoryName(Assembly.GetAssembly(typeof(Logger)).CodeBase))).LocalPath, Path.Combine("Logs", "Log.txt"));

            if (!Directory.Exists(Path.GetDirectoryName(LogFileName)))
            {
                Directory.CreateDirectory(Path.GetDirectoryName(LogFileName));
            }

            //if there is a previous log file check the date last accessed and if not today append the date last accessed to the logfile.
            if (File.Exists(LogFileName))
            {
                DateTime LastFileAccessed = (new FileInfo(LogFileName).LastWriteTimeUtc.Date);
                if (LastFileAccessed != System.DateTime.Now.Date)
                {          
                    String NewFileName = String.Format("{0}{1:0000}{2:00}{3:00}",LogFileName,LastFileAccessed.Year,LastFileAccessed.Month,LastFileAccessed.Day);
                    if(File.Exists(NewFileName))
                    {
                        //Copy the newer text file to bottom of old log, this part of code should not really be entered if under normal running conditions
                        StreamWriter OldLogFile = new StreamWriter(NewFileName, true, Encoding.ASCII);
                        string[] NewLogFile = File.ReadAllLines(LogFileName);
                        foreach (string LogLine in NewLogFile)
                        {
                            OldLogFile.WriteLine(LogLine);
                        }
                        OldLogFile.Flush();
                        OldLogFile.Close();
                        File.Delete(LogFileName);
                    }
                    else
                    {
                        File.Move(LogFileName, NewFileName);
                    }
                }
            }
        }

        private string LogFileName;

        private void WriteToLogFile(string Text)
        {
            StreamWriter LogFile = new StreamWriter(LogFileName, true, Encoding.ASCII);

            LogFile.WriteLine(Text);
            LogFile.Flush();
            LogFile.Close();
        }

        public void Info(string Text)
        {
            Text = "{INFO}    " + Text;
            Console.WriteLine(Text);
            WriteToLogFile(Text);
        }
        public void Warn(string Text)
        {
            Text = "{WARN}    " + Text;
            Console.WriteLine(Text);
            WriteToLogFile(Text);
        }

        public void Error(string Text)
        {
            Text = "{ERROR}   " + Text;
            Console.WriteLine(Text);
            WriteToLogFile(Text);
        }

        public void Error(string Text, Exception e)
        {
            Text = "{ERROR}   " + Text;
            Console.WriteLine(Text);
            WriteToLogFile(Text);
        }
    }
}
