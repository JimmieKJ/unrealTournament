// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Reflection;
using System.IO;
using System.Text.RegularExpressions;
using Microsoft.VisualBasic.FileIO;

namespace MarkdownSharp
{
    using System.Threading;

    using MarkdownSharp.FileSystemUtils;

    public class Language
    {
        public delegate void LoadedHandle();

        public static void Init(string internationalizationFilesDirectory)
        {
            Language.Instance.InstInit(internationalizationFilesDirectory);
        }

        public void InstInit(string internationalizationFilesDirectory)
        {
            if (internationalizationFilesDirectory == null || internationalizationFilesDirectory != InternationalizationFilesDirectory)
            {
                InternationalizationFilesDirectory = internationalizationFilesDirectory;

                string[] files = Directory.GetFiles(internationalizationFilesDirectory);

                LinkedList<string> availableLangIDs = new LinkedList<string>();
                foreach (string file in files)
                {
                    Match m = langIDFromFileNameRegex.Match(Path.GetFileName(file));
                    availableLangIDs.AddLast(m.Groups["LangID"].Value);
                }

                this.availableLangIDs = availableLangIDs.ToArray();

                Reload();
                ReCreateWatcher();
            }
        }

        public static string Message(string messageId, params string[] arguments)
        {
            return Instance.InstMessage(messageId, arguments);
        }

        public string InstMessage(string messageId, string[] arguments)
        {
            // Critical section on data to avoid LoadData clash
            // Usable only in MarkdownMode as it is multi-threaded

            string outputMessage;

            rawMessagesMapLock.EnterReadLock();
            try
            {
                if (!rawMessagesMap.TryGetValue(messageId, out outputMessage))
                {
                    return string.Format("Message \"{0}\" not found in translation file.", messageId);
                }
            }
            finally
            {
                rawMessagesMapLock.ExitReadLock();
            }

            return string.Format(outputMessage, arguments);
        }

        public static Language Instance { get { return lang; } }

        public static string LangID
        {
            get { return langID; }
            set
            {
                langID = value;

                if (Instance.InternationalizationFilesDirectory != null)
                {
                    Instance.Reload();
                }
            }
        }

        public static string[] AvailableLangIDs { get { return Instance.availableLangIDs; } }

        public static event LoadedHandle Loaded;

        private Language()
        {

        }

        private void ReCreateWatcher()
        {
            // ensure old watcher is disposed
            if (_watchDataFileForChanges != null)
            {
                _watchDataFileForChanges.Dispose();
            }

            // setting FileSystemWatcher to watch messagesFilePath dir for given file

            var messagesFileName = Path.GetFileName(string.Format("messages.{0}.csv", LangID));

            if (messagesFileName != null)
            {
                _watchDataFileForChanges =
                    new FileWatcher(Path.Combine(InternationalizationFilesDirectory, messagesFileName));
                _watchDataFileForChanges.Changed += (sender, args) => Reload();
            }
        }

        private void Reload()
        {
            string messagesFileName = string.Format("messages.{0}.csv", LangID);

            LoadData(Path.Combine(InternationalizationFilesDirectory, messagesFileName));

            if (Loaded != null)
            {
                Loaded();
            }
        }

        private void LoadData(string messagesFileName)
        {
            // try to open data file
            FileStream stream = null;

            try
            {
                // to avoid an editor saving process and FileSystemWatcher clash
                // if IOException is thrown, recheck opening in 0.5 second 5 times
                for (int i = 0; i < 5 && stream == null; i++)
                {
                    try
                    {
                        stream = new FileStream(messagesFileName, FileMode.Open, FileAccess.Read);
                    }
                    catch (IOException)
                    {
                        // wait until other app stops using the file
                        System.Threading.Thread.Sleep(500);
                    }
                }

                if (stream == null)
                {
                    throw new UnauthorizedAccessException("Could not open language file.");
                }

                // Critical section on data to avoid GetData clash
                // Usable only in MarkdownMode as it is multi-threaded
                rawMessagesMapLock.EnterWriteLock();
                try
                {
                    // clear the data
                    rawMessagesMap.Clear();

                    TextFieldParser parser = new TextFieldParser(stream, Encoding.UTF8);
                    parser.HasFieldsEnclosedInQuotes = true;
                    parser.Delimiters = new string[] { "," };

                    // for every line in file
                    while (!parser.EndOfData)
                    {
                        string[] cells = parser.ReadFields();

                        rawMessagesMap[cells[0]] = cells[1].Replace("\\n", "\n");
                    }
                }
                finally
                {
                    rawMessagesMapLock.ExitWriteLock();
                }
            }
            finally
            {
                // ensure file is closed
                if (stream != null)
                {
                    stream.Close();
                }
            }
            
        }

        private string InternationalizationFilesDirectory;
        private static string langID = "INT";
        private string[] availableLangIDs = new string[] { };
        private static readonly Language lang = new Language();
        private Regex langIDFromFileNameRegex = new Regex("^messages\\.(?<LangID>[_a-zA-Z]+)\\.csv$", RegexOptions.Compiled);
        private FileWatcher _watchDataFileForChanges;

        private ReaderWriterLockSlim rawMessagesMapLock = new ReaderWriterLockSlim();
        private Dictionary<string, string> rawMessagesMap = new Dictionary<string, string>();
    }
}
