// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownMode.Perforce
{
    using System.IO;
    using System.Runtime.Serialization;
    using System.Windows;
    using System.Xml;

    using MarkdownSharp;

    using global::Perforce.P4;

    [Serializable]
    public class PerforceLastConnectionSettingsAutoDetectionException : Exception
    {
        public PerforceLastConnectionSettingsAutoDetectionException()
            : this(null)
        {
        }

        public PerforceLastConnectionSettingsAutoDetectionException(string message)
            : this(message, null)
        {
        }

        public PerforceLastConnectionSettingsAutoDetectionException(string message, Exception innerException)
            : base(message, innerException)
        {
        }

        protected PerforceLastConnectionSettingsAutoDetectionException(SerializationInfo info, StreamingContext context)
            : base(info, context)
        {
        }
    }

    public static class PerforceHelper
    {
        private enum P4Setting
        {
            P4PORT,
            P4USER,
            P4CLIENT
        };

        private readonly static Dictionary<P4Setting, string> P4LastConnectionSettingsCache = new Dictionary<P4Setting, string>();

        private static string AutoDetectP4Setting(P4Setting type)
        {
            string value;
            if (P4LastConnectionSettingsCache.TryGetValue(type, out value))
            {
                return value;
            }

            FillP4LastConnectionSettingsCache();

            return P4LastConnectionSettingsCache[type];
        }

        private static Repository repository = null;

        public static Repository GetRepository()
        {
            if (repository == null)
            {
                var p4Port = AutoDetectP4Setting(P4Setting.P4PORT);
                var p4User = AutoDetectP4Setting(P4Setting.P4USER);
                var p4Workspace = AutoDetectP4Setting(P4Setting.P4CLIENT);

                var p4Server = new Server(new ServerAddress(p4Port));
                var p4Repository = new Repository(p4Server);
                var p4Connection = p4Repository.Connection;

                p4Connection.UserName = p4User;
                p4Connection.Client = new Client { Name = p4Workspace };

                p4Connection.Connect(null);
                repository = p4Repository;
            }

            return repository;
        }

        private static void FillP4LastConnectionSettingsCache()
        {
            var p4AppSettingsXmlDoc = new XmlDocument();

            try
            {
                p4AppSettingsXmlDoc.Load(
                    Path.Combine(
                        Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
                        ".p4qt",
                        "ApplicationSettings.xml"));
            }
            catch (FileNotFoundException e)
            {
                throw new PerforceLastConnectionSettingsAutoDetectionException("Could not find ApplicationSettings.xml for P4V.", e);
            }

            var nav = p4AppSettingsXmlDoc.CreateNavigator();

            var lastConnectionNode = nav.SelectSingleNode("/PropertyList/PropertyList[@varName='Connection']/String[@varName='LastConnection']");

            if (lastConnectionNode == null)
            {
                throw new PerforceLastConnectionSettingsAutoDetectionException("Cannot find LastConnection setting in P4 ApplicationSettings.");
            }

            var lastConnectionSettings =
                lastConnectionNode
                    .InnerXml.Split(',')
                    .Select(e => e.Trim())
                    .ToArray();

            P4LastConnectionSettingsCache[P4Setting.P4PORT] = lastConnectionSettings[0];
            P4LastConnectionSettingsCache[P4Setting.P4USER] = lastConnectionSettings[1];
            P4LastConnectionSettingsCache[P4Setting.P4CLIENT] = lastConnectionSettings[2];
        }

        private static bool ExcludedAction(FileAction fileAction)
        {
            switch (fileAction)
            {
                case FileAction.Delete:
                case FileAction.DeleteFrom:
                case FileAction.DeleteInto:
                case FileAction.MoveDelete:
                case FileAction.Purge:
                    return true;
                default:
                    return false;
            }
        }

        public static List<string> NonExistingDepotFiles(List<string> files)
        {
            var repo = GetRepository();

            var specs = files.Select(file => new FileSpec { LocalPath = new LocalPath(file) }).ToList();

            var existingFiles = repo.GetFileMetaData(specs, null).Where(md => !ExcludedAction(md.HeadAction)).ToArray();

            var output = new List<string>();

            foreach (var file in files)
            {
                if (
                    existingFiles.All(
                        md => !Path.GetFullPath(md.LocalPath.Path)
                            .Equals(Path.GetFullPath(file), StringComparison.InvariantCultureIgnoreCase)))
                {
                    output.Add(file);
                }
            }

            return output;
        }

        public static void AddFilesToChangelist(int clId, List<string> files)
        {
            var specs = files.Select(file => new FileSpec { LocalPath = new LocalPath(file) }).ToList();

            GetRepository().Connection.Client.AddFiles(
                specs, clId < 0 ? new Options() : new AddFilesCmdOptions(AddFilesCmdFlags.None, clId, null));
        }

        public static Dictionary<int, Changelist> GetPendingChangelists()
        {
            var repo = GetRepository();

            var options = new Options();
            options.Add("-s", "pending");
            options.Add("-u", AutoDetectP4Setting(P4Setting.P4USER));
            options.Add("-l", ""); // full text description

            return repo.GetChangelists(options).ToDictionary(changelist => changelist.Id);
        }

        public static void AddToDepotIfNotExisting(List<string> list)
        {
            var nonExistingFiles = NonExistingDepotFiles(list);

            if (nonExistingFiles.Count > 0)
            {
                AddImagesToChangelistWindow.ShowAddImagesToChangelistWindow(nonExistingFiles);
            }
            else
            {
                MessageBox.Show(Language.Message("AllImagesExistsInDepot"));
            }
        }

        public static int CreateNewChangelist()
        {
            var repo = GetRepository();

            var cl = new Changelist { Description = Language.Message("MissingImagesNewChangelistDesc") };

            return repo.CreateChangelist(cl).Id;
        }
    }
}
