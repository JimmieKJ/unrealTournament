// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.FileSystemUtils
{
    using System.IO;
    using System.Threading;

    public static class FileHelper
    {
        public static string SafeReadAllText(string path, int numberOfAttempts = 5, int timeOutInMilliseconds = 500)
        {
            // avoiding [saving -> file watcher event -> reading] clash
            for (var i = 0; i < numberOfAttempts - 1; ++i)
            {
                try
                {
                    return File.ReadAllText(path);
                }
                catch (IOException)
                {
                    Thread.Sleep(timeOutInMilliseconds);
                }
            }

            return File.ReadAllText(path);
        }

        public static string GetDirectoryIfFile(string path)
        {
            return !File.GetAttributes(path).HasFlag(FileAttributes.Directory) ? Path.GetDirectoryName(path) : path;
        }

        public static string GetRelativePath(string from, string to)
        {
            if (from == to)
            {
                return "";
            }

            AppendSeparatorIfDirectory(ref from);
            AppendSeparatorIfDirectory(ref to);

            var fromUri = new Uri(from);
            var toUri = new Uri(to);

            var relativeUri = fromUri.MakeRelativeUri(toUri);

            return relativeUri.ToString().Replace(Path.AltDirectorySeparatorChar, Path.DirectorySeparatorChar);
        }

        private static void AppendSeparatorIfDirectory(ref string path)
        {
            if (path[path.Length - 1] == Path.DirectorySeparatorChar
                || path[path.Length - 1] == Path.AltDirectorySeparatorChar)
            {
                return;
            }

            if (!File.Exists(path) || File.GetAttributes(path).HasFlag(FileAttributes.Directory))
            {
                path = path + Path.DirectorySeparatorChar;
            }
        }

        public static bool IsExistingDirectory(string path)
        {
            var pathInfo = new DirectoryInfo(path);

            return pathInfo.Exists;
        }
    }
}
