// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MarkdownMode.ToolWindow
{
    using System.IO;
    using System.Text.RegularExpressions;
    using System.Threading;

    using CommonUnrealMarkdown;

    using MarkdownMode.Properties;

    using MarkdownSharp;
    using MarkdownSharp.Doxygen;
    using MarkdownSharp.EpicMarkdown;

    using Microsoft.VisualStudio.Text;
    using Microsoft.VisualStudio.Text.Editor;

    public class UDNParsingResults
    {
        // empty results constructor
        private UDNParsingResults()
        {
            Document = EMDocument.Empty;
            Errors = new List<ErrorDetail>();
            Images = new List<ImageConversion>();
            Attachments = new List<AttachmentConversionDetail>();
        }

        public UDNParsingResults(string path, ITextSnapshot snapshot, MarkdownPackage package, Markdown markdown, FolderDetails folderDetails)
        {
            var log = new OutputPaneLogger();
            ParsedSnapshot = snapshot;

            // Use the Publish Flag combo box to set the markdown details.
            markdown.PublishFlags.Clear();

            // Always include public
            markdown.PublishFlags.Add(Settings.Default.PublicAvailabilitiesString);

            foreach (var flagName in package.PublishFlags)
            {
                markdown.PublishFlags.Add(flagName);
            }

            Errors = new List<ErrorDetail>();
            Images = new List<ImageConversion>();
            Attachments = new List<AttachmentConversionDetail>();

            Document = markdown.ParseDocument(ParsedSnapshot.GetText(), Errors, Images, Attachments, folderDetails);

            DoxygenHelper.SetTrackedSymbols(Document.TransformationData.FoundDoxygenSymbols);

            CommonUnrealFunctions.CopyDocumentsImagesAndAttachments(
                path, log, folderDetails.AbsoluteHTMLPath, folderDetails.Language,
                folderDetails.CurrentFolderFromMarkdownAsTopLeaf, Images, Attachments);

            // Create common directories like css includes top level images etc. 
            // Needs to be created everytime the document is generated to allow
            // changes to these files to show in the preview window without
            // restarting VS.
            CommonUnrealFunctions.CreateCommonDirectories(
                folderDetails.AbsoluteHTMLPath, folderDetails.AbsoluteMarkdownPath, log);
        }

        public EMDocument Document { get; private set; }
        public List<ErrorDetail> Errors { get; private set; }
        public List<ImageConversion> Images { get; private set; }
        public List<AttachmentConversionDetail> Attachments { get; private set; }
        public ITextSnapshot ParsedSnapshot { get; private set; }

        public static UDNParsingResults Empty = new UDNParsingResults();
    }

    public delegate void AfterParsingEventHandler(UDNParsingResults results);

    public class UDNParsingResultsCache
    {
        private object thisLock = new object();

        private readonly MarkdownPackage package;
        private readonly string path;
        private readonly ITextView view;
        private readonly FolderDetails folderDetails = new FolderDetails();
        private readonly Markdown markdown = new Markdown();

        public UDNParsingResultsCache(MarkdownPackage package, string path, ITextView view)
        {
            this.package = package;
            this.path = path;
            this.view = view;

            markdownTransformSetup(path, markdown, folderDetails);

            AfterParsingEvent += (results) =>
                {
                    UDNDocRunningTableMonitor.CurrentUDNDocView.NavigateToComboData.RefreshComboItems(results);
                    UDNDocRunningTableMonitor.CurrentUDNDocView.TextEditorView.TextBuffer.Properties
                                             .GetProperty<OutliningTagger>(typeof(OutliningTagger)).ReparseFile(results);
                    UDNDocRunningTableMonitor.CurrentUDNDocView.TextEditorView.TextBuffer.Properties
                                             .GetProperty<MarkdownClassifier>(typeof(MarkdownClassifier)).Reclassify(results);
                };
        }

        //Setup defaults and file values for this documents markdown transform
        private void markdownTransformSetup(string FileName, Markdown markdownTransform, FolderDetails CurrentFolderDetails)
        {
            DirectoryInfo SourceDirectoryInfo = new DirectoryInfo(FileName).Parent;

            //Source directory path is found using the SourceDirectoryName setting and the file
            while (SourceDirectoryInfo != null && SourceDirectoryInfo.Name != Settings.Default.SourceDirectoryName)
            {
                SourceDirectoryInfo = SourceDirectoryInfo.Parent;
            }

            if (SourceDirectoryInfo != null)
            {
                string SourceDirectory = SourceDirectoryInfo.FullName;

                string OutputDirectory = Path.Combine(Path.GetTempPath(), "UDTMarkdownMode") + "/";


                string FileOutputDirectory = (new DirectoryInfo(FileName).Parent).FullName.Replace(SourceDirectory + Path.DirectorySeparatorChar, "").Replace(SourceDirectory, "");

                CurrentFolderDetails.RelativeHTMLPath = OutputDirectory.Replace(@"C:\", @"file://127.0.0.1/c$/");
                CurrentFolderDetails.AbsoluteHTMLPath = OutputDirectory;
                CurrentFolderDetails.AbsoluteMarkdownPath = SourceDirectory;
                CurrentFolderDetails.CurrentFolderFromMarkdownAsTopLeaf = FileOutputDirectory;
                CurrentFolderDetails.DocumentTitle = (new DirectoryInfo(FileName).Parent).FullName.Replace(SourceDirectory + Path.DirectorySeparatorChar.ToString(), "").Replace(SourceDirectory, "").Replace(Path.DirectorySeparatorChar.ToString(), " - ");
                CurrentFolderDetails.Language = Regex.Replace(Path.GetFileNameWithoutExtension(FileName).ToUpper(), @"[^\.]*?\.(.*$)", "$1");
                CurrentFolderDetails.APIFolderLocationFromMarkdownAsTop = Settings.Default.APIFolderLocation;

                Language.Init(Path.Combine(CurrentFolderDetails.AbsoluteMarkdownPath, "Include", "Internationalization"));

                DoxygenHelper.DoxygenInputFilter =
                    new FileInfo(
                        Path.Combine(
                            CurrentFolderDetails.AbsoluteMarkdownPath,
                            "..",
                            "..",
                            "Binaries",
                            "DotNET",
                            "UnrealDocToolDoxygenInputFilter.exe")).FullName;

                if (CurrentFolderDetails.Language != "INT")
                {
                    CurrentFolderDetails.DocumentTitle += " - " + CurrentFolderDetails.Language;
                }
                CurrentFolderDetails.DocumentTitle += " - " + Language.Message("Preview");

                Markdown.MetadataErrorIfMissing = Settings.MetadataErrorIfMissing;
                Markdown.MetadataInfoIfMissing = Settings.MetadataInfoIfMissing;

                markdownTransform.DefaultTemplate = Settings.Default.DefaultTemplate;

                markdownTransform.ThisIsPreview = true;

                markdownTransform.SupportedLanguages = Settings.SupportedLanguages;
                markdownTransform.SupportedLanguageLabels = Settings.SupportedLanguageLabels;
                for (int i = 0; i < markdownTransform.SupportedLanguages.Length; i++)
                {
                    if (!markdownTransform.SupportedLanguageMap.ContainsKey(markdownTransform.SupportedLanguages[i]))
                    {
                        markdownTransform.SupportedLanguageMap.Add(markdownTransform.SupportedLanguages[i], markdownTransform.SupportedLanguageLabels[i]);
                    }
                }

                markdownTransform.AllSupportedAvailability = new List<string>();
                foreach (string Availablity in Settings.SupportedAvailabilities)
                {
                    markdownTransform.AllSupportedAvailability.Add(Availablity);
                }
                //Add public into list
                markdownTransform.AllSupportedAvailability.Add(Settings.Default.PublicAvailabilitiesString);

                markdownTransform.PublishFlags = new List<string>();
                markdownTransform.DoNotPublishAvailabilityFlag = Settings.Default.DoNotPublishAvailabilitiesFlag.ToLower();

                //Pass the default conversion settings to Markdown for use in the imagedetails creation.
                markdownTransform.DefaultImageDoCompress = Settings.Default.DoCompressImages;
                markdownTransform.DefaultImageFormatExtension = Settings.Default.CompressImageType;
                markdownTransform.DefaultImageFillColor = Settings.DefaultImageFillColor;
                markdownTransform.DefaultImageFormat = ImageConversion.GetImageFormat(Settings.Default.CompressImageType);
                markdownTransform.DefaultImageQuality = Settings.Default.JpegCompressionRate;
            }
        }

        private ReaderWriterLockSlim dirtyLock = new ReaderWriterLockSlim();
        private bool dirty = true;

        private ReaderWriterLockSlim resultsLock = new ReaderWriterLockSlim();
        private UDNParsingResults currentResults;

        public UDNParsingResults Results
        {
            get
            {
                if (Settings.Default.DisableAllParsing)
                {
                    return UDNParsingResults.Empty;
                }

                resultsLock.EnterUpgradeableReadLock();
                try
                {
                    if (GetDirtyFlag())
                    {
                        ForceReparseCurrent();
                    }

                    return currentResults;
                }
                finally
                {
                    resultsLock.ExitUpgradeableReadLock();
                }
            }
        }

        public event AfterParsingEventHandler AfterParsingEvent;

        private void ForceReparseCurrent()
        {
            var snapshot = view.TextSnapshot;
            UDNParsingResults results;

            resultsLock.EnterWriteLock();

            try
            {
                results = Parse(snapshot);
            }
            finally
            {
                resultsLock.ExitWriteLock();
            }

            TriggerAfterParsingEvent(results);
        }

        private UDNParsingResults Parse(ITextSnapshot snapshot)
        {
            currentResults = new UDNParsingResults(path, snapshot, package, markdown, folderDetails);

            SetDirtyFlag(false);

            return currentResults;
        }

        public void TriggerAfterParsingEvent()
        {
            TriggerAfterParsingEvent(Results);
        }

        public void TriggerAfterParsingEvent(UDNParsingResults results)
        {
            if (AfterParsingEvent != null)
            {
                AfterParsingEvent(results);
            }
        }

        private bool GetDirtyFlag()
        {
            dirtyLock.EnterReadLock();
            try
            {
                return dirty;
            }
            finally
            {
                dirtyLock.ExitReadLock();
            }
        }

        private void SetDirtyFlag(bool value)
        {
            dirtyLock.EnterWriteLock();
            dirty = value;
            dirtyLock.ExitWriteLock();
        }

        private void ForceReparse()
        {
            ForceReparseCurrent();
        }

        private void ReparseIfDirty()
        {
            if (GetDirtyFlag())
            {
                ForceReparseCurrent();
            }
            else
            {
                TriggerAfterParsingEvent(Results);
            }
        }

        private static void AsyncDo(Action action, ThreadPriority priority = ThreadPriority.Lowest)
        {
            var ts = new ThreadStart(action);
            var t = new Thread(ts) { Priority = priority };
            t.IsBackground = true;
            t.Name = "EpicMarkdownParsing";

            t.Start();
        }

        public void AsyncReparseIfDirty(ThreadPriority priority = ThreadPriority.Lowest)
        {
            AsyncDo(ReparseIfDirty, priority);
        }

        public void AsyncForceReparse(ThreadPriority priority = ThreadPriority.Lowest)
        {
            AsyncDo(ForceReparse, priority);
        }
    }
}
