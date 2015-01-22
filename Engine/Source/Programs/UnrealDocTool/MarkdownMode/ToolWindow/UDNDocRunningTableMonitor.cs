// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio.Editor;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.TextManager.Interop;
using System.IO;

namespace MarkdownMode.ToolWindow
{
    using System.ComponentModel.Composition;
    using System.Threading;

    using Microsoft.VisualStudio.Shell;
    using Microsoft.VisualStudio.Text;

    public delegate void CurrentOutputIsChangedHandler(UDNParsingResults results);

    internal class UDNDocRunningTableMonitor : IVsRunningDocTableEvents
    {
        public UDNDocRunningTableMonitor(IVsRunningDocumentTable rdt, IVsMonitorSelection ms, IVsEditorAdaptersFactoryService eafs, IVsUIShell uiShell, MarkdownPackage package)
        {
            this.RunningDocumentTable = rdt;
            this.MonitorSelection = ms;
            this.EditorAdaptersFactoryService = eafs;
            this.UIShell = uiShell;
            this.package = package;
            ms.GetCmdUIContextCookie(GuidList.guidMarkdownUIContext, out MarkdownModeUIContextCookie);
        }

        #region IVsRunnningDocTableEvents implementation

        public static event CurrentOutputIsChangedHandler CurrentOutputIsChanged;

        public int OnAfterDocumentWindowHide(uint docCookie, IVsWindowFrame pFrame)
        {
            if (CookieDocumentMap.ContainsKey(docCookie))
            {
                MonitorSelection.SetCmdUIContext(MarkdownModeUIContextCookie, 0);
                CommandsEnabled = false;
            }

            return VSConstants.S_OK;
        }

        public int OnBeforeDocumentWindowShow(uint docCookie, int fFirstShow, IVsWindowFrame pFrame)
        {
            var currentDoc = CurrentDoc;

            if (CookieDocumentMap.ContainsKey(docCookie))
            {
                CurrentDoc = CookieDocumentMap[docCookie];
                MonitorSelection.SetCmdUIContext(MarkdownModeUIContextCookie, 1);
                CommandsEnabled = true;
            }
            else
            {
                string fullname = GetDocFullname(docCookie);
                if (IsUDNDocument(fullname))
                {
                    object codeWindow;
                    pFrame.GetProperty((int)__VSFPROPID.VSFPROPID_DocView, out codeWindow);

                    IVsTextView view;
                    (codeWindow as IVsCodeWindow).GetPrimaryView(out view);

                    var wpfView = EditorAdaptersFactoryService.GetWpfTextView(view);

                    var docView = new UDNDocView(fullname, wpfView, pFrame, package, UIShell);
                    CookieDocumentMap[docCookie] = docView;
                    docView.ParsingResultsCache.AfterParsingEvent += PassResultsToChangedEvent;

                    CurrentDoc = CookieDocumentMap[docCookie];

                    MonitorSelection.SetCmdUIContext(MarkdownModeUIContextCookie, 1);
                    CommandsEnabled = true;
                }
                else
                {
                    MonitorSelection.SetCmdUIContext(MarkdownModeUIContextCookie, 0);
                    CommandsEnabled = false;
                }
            }

            if (currentDoc != CurrentDoc)
            {
                CurrentUDNDocView.ParsingResultsCache.AsyncReparseIfDirty();
            }

            return VSConstants.S_OK;
        }

        private static void PassResultsToChangedEvent(UDNParsingResults results)
        {
            if (CurrentOutputIsChanged != null)
            {
                CurrentOutputIsChanged(results);
            }
        }

        #region Dummy functions

        public int OnAfterAttributeChange(uint docCookie, uint grfAttribs)
        {
            return VSConstants.S_OK;
        }

        public int OnAfterFirstDocumentLock(uint docCookie, uint dwRDTLockType, uint dwReadLocksRemaining, uint dwEditLocksRemaining)
        {
            return VSConstants.S_OK;
        }

        public int OnAfterSave(uint docCookie)
        {
            return VSConstants.S_OK;
        }

        public int OnBeforeLastDocumentUnlock(uint docCookie, uint dwRDTLockType, uint dwReadLocksRemaining, uint dwEditLocksRemaining)
        {
            return VSConstants.S_OK;
        }

        #endregion

        #endregion

        public static UDNDocView CurrentUDNDocView { get { return CurrentDoc; } }
        public bool CommandsEnabled { get; private set; }

        private string GetDocFullname(uint docCookie)
        {
            uint pgrfRDTFlags;
            uint pdwReadLocks;
            uint pdwEditLocks;
            string pbstrMkDocument;
            IVsHierarchy ppHier;
            uint pitemid;
            IntPtr ppunkDocData;

            RunningDocumentTable.GetDocumentInfo(docCookie, out pgrfRDTFlags, out pdwReadLocks, out pdwEditLocks, out pbstrMkDocument, out ppHier, out pitemid, out ppunkDocData);

            return pbstrMkDocument;
        }

        private bool IsUDNDocument(string fullname)
        {
            if (string.IsNullOrWhiteSpace(fullname))
                return false;

            return Path.GetFileName(fullname).EndsWith(".udn");
        }

        IVsRunningDocumentTable RunningDocumentTable;
        IVsMonitorSelection MonitorSelection;
        IVsEditorAdaptersFactoryService EditorAdaptersFactoryService;
        IVsUIShell UIShell;

        private readonly MarkdownPackage package;

        static UDNDocView CurrentDoc;

        Dictionary<uint, UDNDocView> CookieDocumentMap = new Dictionary<uint, UDNDocView>();

        uint MarkdownModeUIContextCookie;
    }

}
