// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using Microsoft.VisualStudio.Shell;

namespace MarkdownMode.ToolWindow
{
    public class MarkdownModeOptions : DialogPage
    {
        public delegate void MarkdownModeOptionsChangedHandler();
        public event MarkdownModeOptionsChangedHandler Changed;

        public string DoxygenBinaryPath { get; set; }
        public string DoxygenXmlCachePath { get; set; }
        public string UnrealDocToolPath { get; set; }

        protected override void OnApply(DialogPage.PageApplyEventArgs e)
        {
            base.OnApply(e);

            if (Changed != null)
            {
                Changed();
            }
        }
    }
}
