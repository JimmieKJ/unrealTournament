// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;

using System.Runtime.InteropServices;

using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Language.Intellisense;
using Microsoft.VisualStudio.OLE.Interop;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.TextManager.Interop;

namespace MarkdownMode.AutoCompletion
{
    using MarkdownMode.Properties;
    using System.IO;


    public class AutoCompletionHandler : IOleCommandTarget
    {
        public AutoCompletionHandler(IVsTextView textView, IWpfTextView wpfTextView, AutoCompletionHandlerProvider autoCompletionHandlerProvider)
        {
            this.autoCompletionHandlerProvider = autoCompletionHandlerProvider;
            this.wpfTextView = wpfTextView;
            textView.AddCommandFilter(this, out nextCommandTarget);
        }

        public int QueryStatus(ref Guid pguidCmdGroup, uint cCmds, OLECMD[] prgCmds, IntPtr pCmdText)
        {
            return nextCommandTarget.QueryStatus(ref pguidCmdGroup, cCmds, prgCmds, pCmdText);
        }

        public int Exec(ref Guid pguidCmdGroup, uint nCmdID, uint nCmdexecopt, IntPtr pvaIn, IntPtr pvaOut)
        {
            if (VsShellUtilities.IsInAutomationFunction(autoCompletionHandlerProvider.ServiceProvider)
                || Settings.Default.DisableAllParsing)
            {
                return nextCommandTarget.Exec(pguidCmdGroup, nCmdID, nCmdexecopt, pvaIn, pvaOut);
            }

            var separatorInserted = false;

            if (nCmdID == (uint)VSConstants.VSStd2KCmdID.TYPECHAR)
            {
                var typedChar = (char)(ushort)Marshal.GetObjectForNativeVariant(pvaIn);

                separatorInserted = IsPathSeparator(typedChar);
            }

            if (nCmdID == (uint)VSConstants.VSStd2KCmdID.RETURN || separatorInserted)
            {
                if (session != null)
                {
                    if (session.SelectedCompletionSet.SelectionStatus.IsSelected)
                    {
                        session.Commit();

                        if (!separatorInserted)
                        {
                            return VSConstants.S_OK;
                        }

                        // Replace typed in character (which could be '/' or '\') to
                        // enforce '/' in the paths.
                        Marshal.GetNativeVariantForObject('/', pvaIn);
                    }
                    else
                    {
                        session.Dismiss();
                    }
                }
            }

            var retVal = nextCommandTarget.Exec(ref pguidCmdGroup, nCmdID, nCmdexecopt, pvaIn, pvaOut);
            var handled = false;

            if (pguidCmdGroup == VSConstants.VSStd2K && nCmdID == (uint)VSConstants.VSStd2KCmdID.TYPECHAR)
            {
                if (session == null)
                {
                    StartSession();
                }
                else if (session != null && session.IsStarted)
                {
                    session.Filter();

                    if (session == null)
                    {
                        StartSession();
                    }
                }

                handled = true;
            }
            else if(nCmdID == (uint) VSConstants.VSStd2KCmdID.BACKSPACE
                || nCmdID == (uint) VSConstants.VSStd2KCmdID.DELETE)
            {
                if (session != null)
                {
                    session.Filter();
                }

                handled = true;
            }

            return handled ? VSConstants.S_OK : retVal;
        }

        private bool IsPathSeparator(char typedChar)
        {
            return typedChar == Path.DirectorySeparatorChar || typedChar == Path.AltDirectorySeparatorChar;
        }

        private void StartSession()
        {
            var caretPosition = wpfTextView.Caret.Position.Point.GetPoint(
                buffer => (!buffer.ContentType.IsOfType("projection")), PositionAffinity.Predecessor);

            if (!caretPosition.HasValue)
            {
                throw new InvalidOperationException("Cannot get carret point.");
            }

            session = autoCompletionHandlerProvider.CompletionBroker.CreateCompletionSession(
                wpfTextView,
                caretPosition.Value.Snapshot.CreateTrackingPoint(caretPosition.Value.Position, PointTrackingMode.Positive),
                true);

            session.Dismissed += SessionDismissed;
            session.Committed += SessionCommitted;

            session.Start();

            if (session != null)
            {
                session.Filter();
            }
        }

        private void ClearSession()
        {
            session.Dismissed -= SessionDismissed;
            session.Committed -= SessionCommitted;

            session = null;
        }

        private void SessionCommitted(object sender, EventArgs eventArgs)
        {
            ClearSession();
        }

        private void SessionDismissed(object sender, EventArgs eventArgs)
        {
            ClearSession();
        }

        private readonly AutoCompletionHandlerProvider autoCompletionHandlerProvider;
        private readonly IOleCommandTarget nextCommandTarget;
        private readonly ITextView wpfTextView;

        private ICompletionSession session;
    }
}
