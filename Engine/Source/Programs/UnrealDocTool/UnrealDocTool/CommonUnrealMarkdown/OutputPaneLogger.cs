// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio;

namespace CommonUnrealMarkdown
{
    public class OutputPaneLogger : Logger
    {
        //The output pane window
        private static IVsOutputWindowPane GeneralOutputWindowPane;

        /// <summary>
        /// Constructor
        /// </summary>
        public OutputPaneLogger()
        {

            IVsOutputWindow OutputWindow = (IVsOutputWindow)Package.GetGlobalService(typeof(SVsOutputWindow));

            Guid GeneralPaneGuid = VSConstants.GUID_OutWindowGeneralPane;

            if (OutputWindow != null)
            {
                OutputWindow.CreatePane(GeneralPaneGuid, "General", 1, 0);
                OutputWindow.GetPane(GeneralPaneGuid, out GeneralOutputWindowPane);
                GeneralOutputWindowPane.Activate();
            }
        }

        // Write text to output pane or logfile
        override public void WriteToLog(string Text)
        {
            GeneralOutputWindowPane.OutputString(Text + "\n");
        }

        // Clears logging output window if running in visual studio environment, if not does nothing.
        override public void ClearLog()
        {
            GeneralOutputWindowPane.Clear();
        }
    }
}