// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace MarkdownSharp
{
    using System;
    using System.Collections.Generic;

    using MarkdownSharp.EpicMarkdown;
    using MarkdownSharp.Preprocessor;

    /// <summary>
    /// class for holding all the shared variables used by most of the functions in this translation
    /// </summary>
    public class TransformationData : IDisposable
    {
        public TransformationData(Markdown markdown, List<ErrorDetail> errorList, List<ImageConversion> imageDetails,
            List<AttachmentConversionDetail> attachNames, FolderDetails currentFolderDetails, IEnumerable<string> languagesLinksToGenerate = null, bool nonDynamicHTMLOutput = false)
        {
            NonDynamicHTMLOutput = nonDynamicHTMLOutput;
            AttachNames = attachNames;
            CurrentFolderDetails = new FolderDetails(currentFolderDetails);
            ErrorList = errorList;
            ImageDetails = imageDetails;
            LanguagesLinksToGenerate = languagesLinksToGenerate ?? markdown.SupportedLanguages;

            HtmlBlocks = new Dictionary<string, string>();
            FoundDoxygenSymbols = new HashSet<string>();
            Markdown = markdown;

            processedDocumentCache = new ProcessedDocumentCache(currentFolderDetails.GetThisFileName(), this);
        }

        public bool NonDynamicHTMLOutput { get; private set; }
        public List<ErrorDetail> ErrorList {get;set;}
        public List<ImageConversion> ImageDetails {get;set;}
        public List<AttachmentConversionDetail> AttachNames {get;set;}
        public FolderDetails CurrentFolderDetails { get; set; }
        public Dictionary<string, string> HtmlBlocks {get;set;}
        public IEnumerable<string> LanguagesLinksToGenerate { get; set; }
        public HashSet<string> FoundDoxygenSymbols { get; set; }

        private ProcessedDocumentCache processedDocumentCache;

        public ProcessedDocumentCache ProcessedDocumentCache
        {
            get
            {
                FailWhenDisposed();

                return processedDocumentCache;
            }
        }

        private void FailWhenDisposed()
        {
            if (disposed)
            {
                throw new ObjectDisposedException("TransformationData");
            }
        }

        public EMDocument Document { get { return ProcessedDocumentCache.CurrentFileDocument; } }

        public Markdown Markdown { get; private set; }

        public VariableManager Variables { get { return Document.Variables; } }
        public MetadataManager Metadata { get { return Document.Metadata; } }
        public ExcerptsManager Excerpts { get { return Document.Excerpts; } }
        public ReferenceLinksManager ReferenceLinks { get { return Document.ReferenceLinks; } }

        private bool disposed = false;

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposed || !disposing)
            {
                return;
            }

            ProcessedDocumentCache.Dispose();
            processedDocumentCache = null;

            disposed = true;
        }
    }
}