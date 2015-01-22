// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace AutoReporter
{
    public partial class Form1 : Form
    {
        static public string unknown = "Unknown";
        static public string newLine = "\r\n";
        static public string seperator = "   |   ";

        public struct CSData
        {
            public string function;
            public string address;
            public string filename;
            public string filenameSml;
            public string line;
            public string module;
            public string moduleSml;

            public string GetCallStack(bool bFunctions, bool bAddresses, bool bFiles, bool bModules, bool bShortnames, bool bUnknowns)
            {
                string callStack = "";

                string callStackLine = "";
                if (bFunctions && (bUnknowns || function != unknown))
                {
                    callStackLine += function + seperator;
                }
                if (bAddresses && (bUnknowns || address != unknown))
                {
                    callStackLine += address + seperator;
                }
                if (bFiles)
                {
                    int iPreLen = callStackLine.Length;
                    if (!bShortnames && (bUnknowns || filename != unknown))
                    {
                        callStackLine += filename;
                    }
                    else if (bUnknowns || filenameSml != unknown)
                    {
                        callStackLine += filenameSml;
                    }
                    if (bUnknowns || line != unknown)
                    {
                        callStackLine += ":L" + line;
                    }
                    if (iPreLen != callStackLine.Length)
                    {
                        callStackLine += seperator;
                    }
                }
                if (bModules)
                {
                    int iPreLen = callStackLine.Length;
                    if (!bShortnames && (bUnknowns || module != unknown))
                    {
                        callStackLine += module;
                    }
                    else if (bUnknowns || moduleSml != unknown)
                    {
                        callStackLine += moduleSml;
                    }
                    if (iPreLen != callStackLine.Length)
                    {
                        callStackLine += seperator;
                    }
                }
                if (callStackLine.Length > 0)
                {
                    // Remove the end seperator
                    callStackLine = callStackLine.Substring(0, callStackLine.Length - seperator.Length);
                    callStack += callStackLine + newLine;
                }

                return callStack;
            }
        }

        public string summary;
        public string crashDesc;
        public string assertOri;            // The original crash
        public CSData assertData;           // The extracted crash components
        public string callStackOri;         // The original callstack
        public List<CSData> callStackData;  // The extracted callstack components

        public Form1()
        {
            InitializeComponent();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            crashDesc = textBox1.Text;
            summary = textBox2.Text;
            Close();
        }


        private void checkbox_Click(object sender, EventArgs e)
        {
            textBox3.Text = GetCallStack();  // Update the text box based on the users setting
        }

        private void form1_Resize(object sender, System.EventArgs e)
        {
            textBox1.Width = Width - ((textBox1.Location.X * 2) + (8 * 2)); // 8 is windows border
            textBox2.Width = Width - ((textBox2.Location.X * 2) + (8 * 2)); // 8 is windows border
            textBox3.Width = Width - ((textBox3.Location.X * 2) + (8 * 2)); // 8 is windows border
            textBox3.Height = MainPanel.Height - textBox3.Location.Y - ButtonPanel.Height;
        }

        public string GetCallStack()
        {
            string callStack = "";

            // Now append out callstack data depending on the users button choice

            bool bOriginal = checkbox7.Checked;
            if (bOriginal)
            {
                callStack += callStackOri;   // Just show the original layout
            }
            else
            {
                // If invalid callstack type, just display as normal
                if (callStackData == null)
                {
                    callStack += callStackOri;
                }
                else
                {
                    // Try to show a simplified crash, if we have it
                    string assert = "";
                    if (assertData.address != null)
                    {
                        if (assertData.module != null)
                        {
                            assert += assertData.module + newLine;
                        }
                        if (assertData.function != null)
                        {
                            assert += assertData.function + seperator;
                        }
                        assert += assertData.address + newLine + newLine;
                    }
                    else if (assertOri != null)
                    {
                        assert = assertOri + newLine;
                    }
                    callStack += assert;

                    // How add each line depending on the users settings
                    bool bFunctions = checkbox1.Checked;
                    bool bAddresses = checkbox2.Checked;
                    bool bFiles = checkbox3.Checked;
                    bool bModules = checkbox4.Checked;
                    bool bShortnames = checkbox5.Checked;
                    bool bUnknowns = checkbox6.Checked;
                    foreach (CSData csdata in callStackData)
                    {
                        callStack += csdata.GetCallStack(bFunctions, bAddresses, bFiles, bModules, bShortnames, bUnknowns);
                    }
                }
            }

            return callStack;
        }

        public void SetCallStack(string CallStack)
        {
            callStackOri = CallStack;       // Cache the original callstack
            GenerateMinCallStack();         // Generate a minimalist callstack display

            textBox3.Text = GetCallStack(); // Update the text box based on the users setting
            
            // Resize based on the new text length (only do this once tho, now we can resize correctly)
            int NewLabelHeight = textBox3.Lines.Length * 15;// (CallStack.Length / 80) * 15;
            NewLabelHeight = System.Math.Max(textBox3.Height, NewLabelHeight);
            NewLabelHeight = System.Math.Min(textBox3.Height + 300, NewLabelHeight);
            int LabelHeightIncrease = NewLabelHeight - textBox3.Height;
			
            Height += LabelHeightIncrease;
        }

        public bool GenerateMinCallStack()
        {
            // Take a copy of the callstack, we don't want to overwrite it
            string callStackCpy = callStackOri;

            // If this string is not empty
            if (callStackCpy.Length > 0)
            {
                const string file = "[File:";
                const string line = "[Line: ";
                const string rightBracket = "]";
                const string slash = "\\";
                const string space = " ";

                // There are two different types of header information the callstack file could have, attempt to resolve both...
                const string fatalError = "Fatal error!\r\n\r\n";
                int iFatalError = callStackCpy.IndexOf( fatalError );
                if (iFatalError != -1)
                {
                    // Trim the excess header
                    int iHeaderLength = iFatalError + fatalError.Length;
                    callStackCpy = callStackCpy.Remove(0, iHeaderLength);
                }
                else
                {
                    // If the first should fail, try to find the second...
                    const string stack = "Stack: ";
                    int iStack = callStackCpy.IndexOf( stack );
                    if (iStack != -1)
                    {
                        // Remove everything before (but keep the ensure error if it has one)
                        const string assertFailed = "Assertion failed: ";
                        int iassertFailed = callStackCpy.IndexOf( assertFailed );
                        if ((iassertFailed != -1) && (iassertFailed < iStack))
                        {
                            // Extract the crash
                            int iassertOffset = iassertFailed + assertFailed.Length;
                            int iassertLength = iStack - iassertOffset;
                            assertOri = callStackCpy.Substring(iassertOffset, iassertLength);

                            // Try the tidy the crash somewhat too
                            int iNewLine = assertOri.IndexOf(newLine);
                            if (iNewLine != -1)
                            {
                                int iNewLineOffset = iNewLine + newLine.Length;
                                int iNewLineLength = assertOri.Length - iNewLineOffset - newLine.Length;
                                if (iNewLineLength > 0)
                                {
                                    assertData.module = assertOri.Substring(iNewLineOffset, iNewLineLength);
                                }
                            }
                            int iLine = assertOri.IndexOf(line);
                            if (iLine != -1)
                            {
                                int iLineOffset = iLine + line.Length;
                                string assertLineTmp = assertOri.Substring(iLineOffset);

                                int iRightBracket = assertLineTmp.IndexOf(rightBracket);
                                if (iRightBracket != -1)
                                {
                                    assertData.line = assertLineTmp.Substring(0, iRightBracket);
                                }
                            }
                            int iFile = assertOri.IndexOf(file);
                            if (iFile != -1)
                            {
                                int iFileOffset = iFile + file.Length;
                                string assertFileTmp = assertOri.Substring(iFileOffset);

                                int iRightBracket2 = assertFileTmp.IndexOf(rightBracket);
                                if (iRightBracket2 != -1)
                                {
                                    assertData.filename = assertFileTmp.Substring(0, iRightBracket2);

                                    // Do a minimal version of the filename too
                                    assertData.filenameSml = assertData.filename;
                                    int iSlash = assertData.filenameSml.LastIndexOf(slash);
                                    if (iSlash != -1)
                                    {
                                        int iSlashOffset = iSlash + slash.Length;
                                        assertData.filenameSml = assertData.filenameSml.Substring(iSlashOffset);
                                    }
                                }

                                assertData.address = assertOri.Substring(0, iFile - space.Length);   // Remove trailing space too
                            }
                        }

                        // Trim the excess header
                        int iHeaderLength = iStack + stack.Length;
                        callStackCpy = callStackCpy.Remove(0, iHeaderLength);
                    }
                    else
                    {
                        // Unhandled header type
                        return false;
                    }
                }

                // Now the header has been removed, separate the component data
                
                const string parentheses = "() ";
                const string hex = "0x";
                const string bytes = " bytes ";
                const string filenameNotFound = " (filename not found) ";
                const string colon = ":";
                const string in_ = "[in ";

                // Allocate our array to hold the data
                if (callStackData == null)
                {
                    callStackData = new List<CSData>();
                }
                callStackData.Clear();

                // Now loop through all the callstack lines
                int iNewLine2 = -1;
                bool bAddToCS = false;
                string lastcsdatafunction = "";
                while (( ( iNewLine2 = callStackCpy.IndexOf(newLine) ) != -1 ))
                {
                    // Grab the top line of the callstack
                    CSData csdata;
                    string csline = callStackCpy.Substring(0, iNewLine2);
                    int iNewLineOffset = iNewLine2 + newLine.Length;
                    callStackCpy = callStackCpy.Remove(0, iNewLineOffset);

                    // Try to extract the function name (if any)
                    csdata.function = unknown;
                    int iParentheses = csline.IndexOf(parentheses);
                    if (iParentheses != -1)
                    {
                        csdata.function = csline.Substring(0, iParentheses);
                        int iParenthesesOffset = iParentheses + parentheses.Length;
                        csline = csline.Remove(0, iParenthesesOffset);
                    }

                    // Try to extract the address (could be one of two formats)
                    csdata.address = unknown;
                    int iHex = csline.IndexOf(hex);
                    if (iHex != -1)
                    {
                        int iBytes = csline.IndexOf(bytes);
                        if (iBytes != -1)
                        {
                            // Chop to bytes
                            int iBytesOffset = iBytes + bytes.Length;
                            csdata.address = csline.Substring(iHex, iBytesOffset - space.Length);   // Remove trailing space too
                            csline = csline.Remove(0, iHex + iBytesOffset);
                        }
                        else
                        {
                            int iSpace = csline.IndexOf(space);
                            if (iSpace != -1)
                            {
                                // Chop to space
                                int iSpaceOffset = iSpace + space.Length + 1;
                                csdata.address = csline.Substring(iHex, iSpaceOffset);
                                csline = csline.Remove(0, iHex + iSpaceOffset + space.Length);   // Remove trailing space too
                            }
                        }
                    }

                    // Try to extract the filename (if any)
                    csdata.filename = unknown;
                    csdata.filenameSml = unknown;
                    csdata.line = unknown;
                    int iFilenameNotFound = csline.IndexOf(filenameNotFound);
                    if (iFilenameNotFound != -1)
                    {
                        int iFilenameNotFoundOffset = iFilenameNotFound + filenameNotFound.Length;
                        csline = csline.Remove(0, iFilenameNotFoundOffset);
                    }
                    else
                    {
                        int iRightBracket = csline.IndexOf(rightBracket);
                        if (iRightBracket != -1)
                        {
                            csdata.filename = csline.Substring(file.Length, iRightBracket - file.Length);    // Remove "[File:" too
                            int iRightBracketOffset = iRightBracket + rightBracket.Length;
                            csline = csline.Remove(0, iRightBracketOffset + space.Length);   // Remove trailing space too

                            // Try to extract the line number
                            int iColon = csdata.filename.LastIndexOf(colon);
                            if (iColon != -1)
                            {
                                int iColonOffset = iColon + colon.Length;
                                csdata.line = csdata.filename.Substring(iColonOffset);
                                csdata.filename = csdata.filename.Remove(iColon);
                            }

                            // Do a minimal version of the filename too
                            csdata.filenameSml = csdata.filename;
                            int iSlash = csdata.filenameSml.LastIndexOf(slash);
                            if (iSlash != -1)
                            {
                                int iSlashOffset = iSlash + slash.Length;
                                csdata.filenameSml = csdata.filenameSml.Substring(iSlashOffset);
                            }
                        }
                    }

                    // Try to get the module
                    csdata.module = unknown;
                    csdata.moduleSml = unknown;
                    int iRightBracket2 = csline.IndexOf(rightBracket);
                    if (iRightBracket2 != -1)
                    {
                        csdata.module = csline.Substring(in_.Length, iRightBracket2 - in_.Length);    // Remove "[in " too
                        int iRightBracketOffset = iRightBracket2 + rightBracket.Length;
                        csline = csline.Remove(0, iRightBracketOffset);

                        // Do a minimal version of the module too
                        csdata.moduleSml = csdata.module;
                        int iSlash = csdata.moduleSml.LastIndexOf(slash);
                        if (iSlash != -1)
                        {
                            int iSlashOffset = iSlash + slash.Length;
                            csdata.moduleSml = csdata.moduleSml.Substring(iSlashOffset);
                        }
                    }

                    // Add this info to our array (skip any lines we don't need that may have occurred after the crash)
                    if (!bAddToCS && assertData.line != null && assertData.filename != null)
                    {
                        if ( ( string.Compare( csdata.line, assertData.line, true ) == 0 )
                          && ( string.Compare( csdata.filename, assertData.filename, true ) == 0 ) )
                        {
                            if (lastcsdatafunction.Length > 0)
                            {
                                assertData.function = lastcsdatafunction;    // The previous callstack's function is the crash function type
                            }
                            bAddToCS = true;
                        }                       
                    }
                    else
                    {
                        bAddToCS = true;    // Incase we don't have crash information
                    }
                    if ( bAddToCS )
                    {
                        callStackData.Add(csdata);
                    }
                    lastcsdatafunction = csdata.function;
                }
            }

            return true;
        }

        public void SetServiceError(string ErrorMsg)
        {
            textBox1.Visible = false;
            label4.Visible = false;
            textBox2.Visible = false;

            label1.Text = "An error occurred connecting to the Report Service: \n" + ErrorMsg;
            label1.Height += 20;
        }

		public void ShowBalloon( string Msg, int BalloonTimeInMs )
		{
			AppErrorNotifyInfo.BalloonTipText = Msg;
			AppErrorNotifyInfo.Visible = true;
			AppErrorNotifyInfo.ShowBalloonTip( BalloonTimeInMs );
		}

		public void HideBalloon()
		{
			AppErrorNotifyInfo.Visible = false;
		}
	}
}