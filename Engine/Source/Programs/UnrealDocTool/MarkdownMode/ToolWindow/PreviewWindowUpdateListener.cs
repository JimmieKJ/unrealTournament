// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using MarkdownSharp.Doxygen;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Text;
using System.Windows.Threading;
using System.Threading;
using System.IO;
using Microsoft.VisualStudio.Shell.Interop;
using System.ComponentModel.Composition;
using Microsoft.VisualStudio.Utilities;
using Microsoft.VisualStudio.Shell;
using System.Text.RegularExpressions;
using MarkdownSharp;
using CommonUnrealMarkdown;
using MarkdownMode.Properties;

namespace MarkdownMode
{
    using MarkdownMode.ToolWindow;

    using MarkdownSharp.EpicMarkdown;

    [Export(typeof(IWpfTextViewCreationListener))]
    [MarginContainer(PredefinedMarginNames.Top)]
    [TextViewRole(PredefinedTextViewRoles.Document)]
    [ContentType(ContentType.Name)]
    sealed class PreviewWindowViewCreationListener : IWpfTextViewCreationListener
    {
        [Import]
        SVsServiceProvider GlobalServiceProvider = null;

        public void  TextViewCreated(IWpfTextView textView)
        {
            ITextDocument document;

            if (!textView.TextDataModel.DocumentBuffer.Properties.TryGetProperty(typeof(ITextDocument), out document))
            {
                document = null;
            }

            MarkdownPackage package = null;

            // If there is a shell service (which there should be, in VS), force the markdown package to load
            IVsShell shell = GlobalServiceProvider.GetService(typeof(SVsShell)) as IVsShell;
            if (shell != null)
                package = MarkdownPackage.ForceLoadPackage(shell);

            textView.Properties.GetOrCreateSingletonProperty(() => new PreviewWindowUpdateListener(textView, package, document));
        }
    }

    sealed class PreviewWindowUpdateListener
    {
        public const string Name = "MarkdownMargin";

        readonly IWpfTextView textView;
        readonly ITextDocument document;
        readonly MarkdownPackage package;

        private static readonly OutputPaneLogger log = new OutputPaneLogger();

        MarkdownPreviewToolWindow GetPreviewWindow(bool create)
        {
            return (package != null) ? package.GetMarkdownPreviewToolWindow(create) : null;
        }

        static Regex RemoveScript = new Regex("<script(\\s|\\S)*?</script>", RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        private static void LogMarkdownErrors(string FileName, List<ErrorDetail> errorList, Logger log)
        {
            if (errorList.Count > 0)
            {
                Console.Write("\n");

                log.Info(MarkdownSharp.Language.Message("FileFailed", FileName));

                foreach (MarkdownSharp.ErrorDetail ErrorInfo in errorList)
                {
                    switch (ErrorInfo.ClassOfMessage)
                    {
                        case MarkdownSharp.MessageClass.Error:
                            log.Error(FileName, ErrorInfo.Line, ErrorInfo.Column, ErrorInfo.Message);
                            break;
                        case MarkdownSharp.MessageClass.Warning:
                            log.Warn(FileName, ErrorInfo.Line, ErrorInfo.Column, ErrorInfo.Message);
                            break;
                        default:
                            log.Info(FileName, ErrorInfo.Line, ErrorInfo.Column, ErrorInfo.Message);
                            break;
                    }
                }
            }
        }

        private readonly object anchorPositionLock = new object();
        private string oldHtml = "";
        private int lastDifferencePosition = 0;

        private int AnchorPosition(string newHtml)
        {
            lock (anchorPositionLock)
            {
                var diffPosition = 0;
                for (; diffPosition < Math.Min(newHtml.Length, oldHtml.Length); ++diffPosition)
                {
                    if (newHtml[diffPosition] != oldHtml[diffPosition])
                    {
                        break;
                    }
                }

                oldHtml = string.Copy(newHtml);

                if (diffPosition == newHtml.Length)
                {
                    diffPosition = lastDifferencePosition;
                }

                lastDifferencePosition = diffPosition;

                for (var i = diffPosition; i > 0; --i)
                {
                    if (newHtml[i] == '<')
                    {
                        diffPosition = i;
                        break;
                    }

                    if (newHtml[i] == '>')
                    {
                        break;
                    }
                }

                return diffPosition;
            }
        }

        string GetHTMLText(UDNParsingResults results)
        {
            log.ClearLog();

            var html = new StringBuilder();

            var newHtml = results.Document.ThreadSafeRender();

            var anchorPosition = AnchorPosition(newHtml);

            if (anchorPosition != 0)
            {
                newHtml = newHtml.Insert(
                    anchorPosition, "<span name=\"MARKDOWNANCHORNOTUSEDELSEWHERE\"></span>");
            }

            //html.AppendLine(RemoveScript.Replace(newHtml, ""));
            html.AppendLine(newHtml);

            LogMarkdownErrors(document.FilePath, results.Errors, log);

            return html.ToString();
        }

        public PreviewWindowUpdateListener(IWpfTextView wpfTextView, MarkdownPackage package, ITextDocument document)
        {
            this.textView = wpfTextView;
            this.package = package;
            this.document = document;

            EventHandler updateHandler =
                (sender, args) => UDNDocRunningTableMonitor.CurrentUDNDocView.ParsingResultsCache.AsyncForceReparse();

            BufferIdleEventUtil.AddBufferIdleEventListener(wpfTextView.TextBuffer, updateHandler);

            document.FileActionOccurred +=
                (sender, args) => UDNDocRunningTableMonitor.CurrentUDNDocView.ParsingResultsCache.AsyncForceReparse();

            textView.Closed += (sender, args) =>
                {
                    ClearPreviewWindow();
                    BufferIdleEventUtil.RemoveBufferIdleEventListener(wpfTextView.TextBuffer, updateHandler);
                };

            //On updating the publish flags re-run the preview
            package.PublishFlags.CollectionChanged +=
                (sender, args) => UDNDocRunningTableMonitor.CurrentUDNDocView.ParsingResultsCache.AsyncForceReparse();

            DoxygenHelper.TrackedFilesChanged +=
                () => UDNDocRunningTableMonitor.CurrentUDNDocView.ParsingResultsCache.AsyncForceReparse();

            Templates.TemplatesChanged += () => UpdatePreviewWindow(UDNDocRunningTableMonitor.CurrentUDNDocView.ParsingResultsCache.Results);
            UDNDocRunningTableMonitor.CurrentOutputIsChanged += UpdatePreviewWindow;
        }

        string GetDocumentName()
        {
            if (document == null)
                return Language.Message("NoName");
            else
                return Path.GetFileName(document.FilePath);
        }

        void UpdatePreviewWindow(UDNParsingResults results)
        {
            ThreadPool.QueueUserWorkItem(state =>
                {
                    var content = GetHTMLText(results);

                    if (string.IsNullOrWhiteSpace(content))
                    {
                        return;
                    }

                    textView.VisualElement.Dispatcher.Invoke(() =>
                        {
                            var previewWindow = GetPreviewWindow(true);

                            if (previewWindow.CurrentSource == this || previewWindow.CurrentSource == null)
                            {
                                previewWindow.SetPreviewContent(this, content, GetDocumentName());
                            }
                        }, DispatcherPriority.ApplicationIdle);
                });
        }

        void ClearPreviewWindow()
        {
            var window = GetPreviewWindow(create: false);
            if (window != null && window.CurrentSource == this)
                window.ClearPreviewContent();
        }
    }
}
