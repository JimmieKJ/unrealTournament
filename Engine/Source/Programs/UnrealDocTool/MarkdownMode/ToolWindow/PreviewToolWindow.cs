// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Runtime.InteropServices;
using Microsoft.VisualStudio.Shell;
using System.Windows.Controls;
using Microsoft.Win32;
using System.Windows.Navigation;
using System.IO;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio;
using EnvDTE;
using EnvDTE80;
using System.Windows.Threading;

namespace MarkdownMode
{
    using System.Text;

    [Guid("acd82a5f-9c35-400b-b9d0-f97925f3b312")]
    public class MarkdownPreviewToolWindow : ToolWindowPane
    {
        private WebBrowser browser;

        int? scrollBackTo = null;

        bool isLoading = false;


        static private void SetWebBrowserIE9DocMode()
        {
            RegistryKey FeatureBrowserEmulationKey = Registry.CurrentUser.CreateSubKey("Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\FEATURE_BROWSER_EMULATION");
            FeatureBrowserEmulationKey.SetValue(System.Diagnostics.Process.GetCurrentProcess().MainModule.ModuleName, 9999, RegistryValueKind.DWord);
            FeatureBrowserEmulationKey.Close();
        }        
        
        /// <summary>
        /// Standard constructor for the tool window.
        /// </summary>
        public MarkdownPreviewToolWindow() : base(null)
        {
            this.Caption = MarkdownSharp.Language.Message("DocumentPreview");
            this.BitmapResourceID = 301;
            this.BitmapIndex = 1;

            //force WebBrowser into IE9 mode
            SetWebBrowserIE9DocMode();

            browser = new WebBrowser();
            browser.NavigateToString(MarkdownSharp.Language.Message("OpenUDNDocumentFileToSeePreview"));
            browser.LoadCompleted += (sender, args) =>
            {
                if (scrollBackTo.HasValue)
                {
                    var document = browser.Document as mshtml.IHTMLDocument2;

                    if (document != null)
                    {
                        var element = document.body as mshtml.IHTMLElement2;
                        if (element != null)
                        {
                            element.scrollTop = scrollBackTo.Value;
                        }
                    }
                }

                scrollBackTo = null;
                isLoading = false;
            };

            browser.Navigating += new NavigatingCancelEventHandler(webBrowser_Navigating);
        }

        private void webBrowser_Navigating(object sender, NavigatingCancelEventArgs e)
        {
            if (e.Uri == null)
            {
                return;
            }

            if (e.Uri.AbsoluteUri.StartsWith("about:blank#"))
            {
                e.Cancel = false;
                return;
            }

            var udnFolder = GetUdnFolderPath(e.Uri);

            if (udnFolder == null)
            {
                e.Cancel = false;
                return;
            }

            // Cancel navigation if to external file.
            e.Cancel = true;

            var lang = ToolWindow.UDNDocRunningTableMonitor.CurrentUDNDocView
                .CurrentEMDocument.TransformationData.CurrentFolderDetails.Language;

            // Get list of files for the language.
            var navigatedToFileInfo = new DirectoryInfo(udnFolder).GetFiles("*." + lang + ".udn");

            // Failing that get the INT file.
            if (!navigatedToFileInfo[0].Exists)
            {
                navigatedToFileInfo = new DirectoryInfo(udnFolder).GetFiles("*.INT.udn");
            }

            if (!navigatedToFileInfo[0].Exists)
            {
                return;
            }

            var dte2 = (DTE2)GetService(typeof(DTE));

            // File found open it, this also makes the editor switch tabs to this file, so the preview window updates.
            dte2.ItemOperations.OpenFile(navigatedToFileInfo[0].FullName);
        }

        private string GetUdnFolderPath(Uri uri)
        {
            if (uri == null)
            {
                return null;
            }

            var doc = ToolWindow.UDNDocRunningTableMonitor.CurrentUDNDocView.CurrentEMDocument;

            var localPath = uri.LocalPath;

            var dirInfo =
                new DirectoryInfo(
                    Path.GetDirectoryName(doc.LocalPath) + Path.DirectorySeparatorChar
                    + Path.GetDirectoryName(localPath));

            return dirInfo.Exists ? dirInfo.FullName : null;
        }

        public override object Content
        {
            get { return browser; }
        }

        bool isVisible = false;
        public bool IsVisible
        {
            get { return isVisible; }
        }

        public override void OnToolWindowCreated()
        {
            isVisible = true;
        }

        public void SetPreviewContent(object source, string html, string title)
        {
            this.Caption = MarkdownSharp.Language.Message("MarkdownPreviewWithTitle", title);

            if (source == CurrentSource)
            {
                // If the scroll back to already has a value, it means the current content hasn't finished loading yet,
                // so the current scroll position isn't ready for us to use.  Just use the existing scroll position.
                if (!scrollBackTo.HasValue)
                {
                    var document = browser.Document as mshtml.IHTMLDocument2;
                    if (document != null)
                    {
                        var element = document.body as mshtml.IHTMLElement2;
                        if (element != null)
                        {
                            scrollBackTo = element.scrollTop;
                        }
                    }
                }
            }
            else
            {
                scrollBackTo = null;
            }

            if (!isLoading)
            {
                isLoading = true;

                CurrentSource = source;

                browser.NavigateToString(html);
            }
        }

        public void ClearPreviewContent()
        {
            this.Caption = MarkdownSharp.Language.Message("MarkdownPreview");
            scrollBackTo = null;
            CurrentSource = null;

            browser.NavigateToString(MarkdownSharp.Language.Message("OpenUDNDocumentFileToSeePreview"));
        }

        public object CurrentSource { get; private set; }
    }
}
