// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Shell.Interop;

namespace MarkdownMode.ToolWindow
{
    using MarkdownSharp.EpicMarkdown;

    internal class UDNDocView
    {
        public UDNDocView(string sourceFilePath, IWpfTextView textEditorView, IVsWindowFrame windowFrame, MarkdownPackage package, IVsUIShell uiShell)
        {
            SourceFilePath = sourceFilePath;
            TextEditorView = textEditorView;
            WindowFrame = windowFrame;

            NavigateToComboData = new NavigateToComboData(textEditorView, uiShell);
            ParsingResultsCache = new UDNParsingResultsCache(package, sourceFilePath, textEditorView);
        }

        public string SourceFilePath { get; private set; }
        public IWpfTextView TextEditorView { get; private set; }
        public IVsWindowFrame WindowFrame { get; private set; }
        public NavigateToComboData NavigateToComboData { get; private set; }

        public UDNParsingResultsCache ParsingResultsCache { get; private set; }

        public EMDocument CurrentEMDocument { get { return ParsingResultsCache.Results.Document; } }
    }
}
