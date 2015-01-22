// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;

namespace UnrealDocTranslator
{    
    public class Logger
    {
        // Allow Info level messages
        private bool bInfoVerbosityLevel = true;

        // Allow Warning level messages
        private bool bWarnVerbosityLevel = true;

        // Allow Error level messages
        private bool bErrorVerbosityLevel = true;

        //Set verbosity of log level to Info and above
        public void SetInfoVerbosityLogLevel()
        {
            bInfoVerbosityLevel = true;
            bWarnVerbosityLevel = true;
            bErrorVerbosityLevel = true;
        }
        
        //Set verbosity of log level to Warn and above (not info)
        public void SetWarnVerbosityLogLevel()
        {
            bInfoVerbosityLevel = false;
            bWarnVerbosityLevel = true;
            bErrorVerbosityLevel = true;
        }

        //Set verbosity of log level to Error and above (not info, or warning)
        public void SetErrorVerbosityLogLevel()
        {
            bInfoVerbosityLevel = false;
            bWarnVerbosityLevel = false;
            bErrorVerbosityLevel = true;
        }

        // Write text to trace location
        public void WriteToLog(string Text)
        {
            Trace.WriteLine(Text);
            Trace.Flush();
        }

        // Writes an information text string to console and to the output pane or logfile
        public void Info(string Text)
        {
            if (bInfoVerbosityLevel)
            {
                WriteToLog(Text);
            }

        }

        // Writes an information vs output formatted text string to console and to the output pane or logfile
        public void Info(string InputFileName, int LineNumber, int ColumnNumber, string Text)
        {
            if (bInfoVerbosityLevel)
            {
                Text = InputFileName + "(" + LineNumber.ToString() + "," + ColumnNumber.ToString() + "):\tInfo: " + Text;
                WriteToLog(Text);
            }
        }

        // Writes a warning text string to console and to the output pane or logfile
        public void Warn(string Text)
        {
            if (bWarnVerbosityLevel)
            {
                WriteToLog(Text);
            }
        }

        // Writes a warning vs output formatted text string to console and to the output pane or logfile
        public void Warn(string InputFileName, int LineNumber, int ColumnNumber, string Text)
        {
            if (bWarnVerbosityLevel)
            {
                Text = InputFileName + "(" + LineNumber.ToString() + "," + ColumnNumber.ToString() + "):\tWarn: " + Text;
                WriteToLog(Text);
            }
        }

        // Writes an error text string to console and to the output pane or logfile
        public void Error(string Text)
        {
            if (bErrorVerbosityLevel)
            {
                WriteToLog(Text);
            }
        }

        // Writes an error vs output formatted text string to console and to the output pane or logfile
        public void Error(string InputFileName, int LineNumber, int ColumnNumber, string Text)
        {
            if (bErrorVerbosityLevel)
            {
                Text = InputFileName + "(" + LineNumber.ToString() + "," + ColumnNumber.ToString() + "):\tError: " + Text;
                WriteToLog(Text);
            }
        }

        // Writes an error text string and exception to console and to the output pane or logfile
        public void Error(string Text, Exception e)
        {
            if (bErrorVerbosityLevel)
            {
                Error(Text);
                if (e != null)
                {
                    Error(e.ToString());
                }
            }
        }
    }
}
