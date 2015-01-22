// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.Preprocessor
{
    using System.IO;
    using System.Threading;

    using MarkdownSharp.EpicMarkdown;
    using MarkdownSharp.FileSystemUtils;

    public class ProcessedDocumentCache : IDisposable
    {
        private class ProcessedDocumentCacheElement : IDisposable
        {
            private readonly string path;
            private readonly Preprocessor preprocessor;

            private readonly ProcessedDocumentCache cache;

            public ProcessedDocumentCacheElement(string path, Preprocessor preprocessor, ProcessedDocumentCache cache)
            {
                this.path = path;
                this.preprocessor = preprocessor;
                this.cache = cache;

                Reload();
                
                watcher = new FileWatcher(path);
                watcher.Changed += (sender, args) => Reload();
            }

            private bool disposed;

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

                watcher.Dispose();
                disposed = true;
            }

            private void Reload()
            {
                FailWhenDisposed();

                var text = FileHelper.SafeReadAllText(path);

                document = new EMDocument(path, cache.TransformationData);
                document.Parse(text,true,false);
            }

            private void FailWhenDisposed()
            {
                if (disposed)
                {
                    throw new ObjectDisposedException("ProcessedDocumentCacheElement");
                }
            }

            private readonly FileWatcher watcher;

            private EMDocument document;
            public EMDocument Document
            {
                get
                {
                    FailWhenDisposed();

                    return document;
                }
            }
        }

        public ProcessedDocumentCache(string path, TransformationData data)
        {
            TransformationData = data;
            currentFileDocument = new EMDocument(path, data);
        }

        public EMDocument Get(string path)
        {
            FailWhenDisposed();

            if (IsCurrentFile(path))
            {
                return CurrentFileDocument;
            }

            linkedFilesCacheLock.EnterWriteLock();
            try
            {
                ProcessedDocumentCacheElement output;

                if (!linkedFilesCache.TryGetValue(path, out output))
                {
                    output = new ProcessedDocumentCacheElement(path, TransformationData.Markdown.Preprocessor, this);
                    linkedFilesCache.Add(path, output);
                }

                return output.Document;
            }
            finally
            {
                linkedFilesCacheLock.ExitWriteLock();
            }
        }

        private bool IsCurrentFile(string path)
        {
            FailWhenDisposed();

            return new FileInfo(path).FullName == new FileInfo(TransformationData.Document.LocalPath).FullName;
        }

        private EMDocument currentFileDocument;
        public EMDocument CurrentFileDocument
        {
            get
            {
                FailWhenDisposed();

                return currentFileDocument;
            }
        }

        public VariableManager Variables { get { return CurrentFileDocument.Variables; } }
        public MetadataManager Metadata { get { return CurrentFileDocument.Metadata; } }
        public ExcerptsManager Excerpts { get { return CurrentFileDocument.Excerpts; } }
        public ReferenceLinksManager ReferenceLinkses { get { return CurrentFileDocument.ReferenceLinks; } }

        public TransformationData TransformationData { get; private set; }

        private readonly ReaderWriterLockSlim linkedFilesCacheLock = new ReaderWriterLockSlim(LockRecursionPolicy.SupportsRecursion);
        private readonly Dictionary<string, ProcessedDocumentCacheElement> linkedFilesCache = new Dictionary<string, ProcessedDocumentCacheElement>();

        public bool TryGetLinkedFileVariable(FileInfo fileInfo, string variableName, out string content)
        {
            FailWhenDisposed();

            var pd = Get(fileInfo.FullName);

            return pd.Variables.TryGet(variableName, out content);
        }

        public string GetClosestFilePath(string pathToLinkedFile, out ClosestFileStatus status)
        {
            FailWhenDisposed();

            if (!String.IsNullOrWhiteSpace(pathToLinkedFile))
            {
                status = ClosestFileStatus.ExactMatch;

                // Link other file and get metadata from there.
                var languageForFile = TransformationData.CurrentFolderDetails.Language;

                var linkedFileDir =
                    new DirectoryInfo(
                        Path.Combine(Path.Combine(TransformationData.CurrentFolderDetails.AbsoluteMarkdownPath, pathToLinkedFile)));

                if (!linkedFileDir.Exists
                    || linkedFileDir.GetFiles(string.Format("*.{0}.udn", TransformationData.CurrentFolderDetails.Language)).Length == 0)
                {
                    // if this is not an INT file check for the INT version.
                    if (TransformationData.CurrentFolderDetails.Language != "INT")
                    {
                        if (!linkedFileDir.Exists || linkedFileDir.GetFiles("*.INT.udn").Length == 0)
                        {
                            status = ClosestFileStatus.FileMissing;
                            return null;
                        }

                        status = ClosestFileStatus.ChangedToIntVersion;
                    }
                    else
                    {
                        status = ClosestFileStatus.FileMissing;
                        return null;
                    }
                }

                if (linkedFileDir.Exists && linkedFileDir.GetFiles(string.Format("*.{0}.udn", languageForFile)).Length > 0)
                {
                    return linkedFileDir.GetFiles(string.Format("*.{0}.udn", languageForFile))[0].FullName;
                }
            }

            status = ClosestFileStatus.FileMissing;
            return null;
        }

        // Ignoring status overload.
        public EMDocument GetClosest(string path)
        {
            FailWhenDisposed();

            ClosestFileStatus status;

            return GetClosest(path, out status);
        }

        public EMDocument GetClosest(string path, out ClosestFileStatus status)
        {
            FailWhenDisposed();

            if (!string.IsNullOrWhiteSpace(path))
            {
                var closestPath = GetClosestFilePath(path, out status);

                if (status == ClosestFileStatus.FileMissing)
                {
                    return null;
                }

                return Get(closestPath);
            }

            status = ClosestFileStatus.ExactMatch;
            return CurrentFileDocument;
        }

        private void FailWhenDisposed()
        {
            if (disposed)
            {
                throw new ObjectDisposedException("ProcessedDocumentCache");
            }
        }

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

            linkedFilesCacheLock.EnterReadLock();
            try
            {
                foreach (var cacheElement in linkedFilesCache)
                {
                    cacheElement.Value.Dispose();
                }

                currentFileDocument = null;
                disposed = true;
            }
            finally
            {
                linkedFilesCacheLock.ExitReadLock();
            }
        }
    }

    public enum ClosestFileStatus
    {
        ExactMatch,
        FileMissing,
        ChangedToIntVersion
    }
}
