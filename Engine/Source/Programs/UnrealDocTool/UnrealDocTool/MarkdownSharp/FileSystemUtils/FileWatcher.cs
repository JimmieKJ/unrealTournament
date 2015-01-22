// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace MarkdownSharp.FileSystemUtils
{
    using System;
    using System.IO;

    public class FileWatcher : IDisposable
    {
        private event FileSystemEventHandler changed;
        public event FileSystemEventHandler Changed
        {
            add
            {
                FailWhenDisposed();
                changed += value;
            }
            remove
            {
                FailWhenDisposed();
                changed -= value;
            }
        }

        private void FailWhenDisposed()
        {
            if (disposed)
            {
                throw new ObjectDisposedException("FileWatcher");
            }
        }

        public FileWatcher(string path)
        {
            var file = new FileInfo(path);

            var fileDirectory = file.DirectoryName;
            var fileName = file.Name;

            if (!file.Exists || fileDirectory == null || fileName == null)
            {
                throw new FileNotFoundException(path);
            }

            watcher = new FileSystemWatcher(fileDirectory, fileName) {EnableRaisingEvents = true};
            watcher.Changed += (sender, args) =>
                {
                    try
                    {
                        watcher.EnableRaisingEvents = false;
                        if (changed != null)
                        {
                            changed(sender, args);
                        }
                    }
                    finally
                    {
                        watcher.EnableRaisingEvents = true;
                    }
                };
        }

        private FileSystemWatcher watcher;

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

            changed = null;

            watcher.EnableRaisingEvents = false;
            watcher.Dispose();
            watcher = null;

            disposed = true;
        }
    }
}
