// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.Preprocessor
{
    using System.IO;
    using System.Runtime.Serialization;
    using System.Text.RegularExpressions;

    using MarkdownSharp.EpicMarkdown;

    [Serializable]
    public abstract class ExcerptsManagerException : Exception
    {
        protected ExcerptsManagerException()
            : this(null)
        {
        }

        protected ExcerptsManagerException(Exception innerException)
            : base(null, innerException)
        {
        }

        protected ExcerptsManagerException(SerializationInfo info, StreamingContext context)
            : base(info, context)
        {
            MessageArgs = (string[])info.GetValue("MessageArgs", typeof(string[]));
        }

        public override void GetObjectData(SerializationInfo info, StreamingContext context)
        {
            base.GetObjectData(info, context);

            info.AddValue("MessageArgs", MessageArgs);
        }

        public abstract string MessageId { get; }
        public string[] MessageArgs { get; protected set; }

        public override string Message { get { return Language.Message(MessageId, MessageArgs); } }
    }

    [Serializable]
    public class ExcerptNotFoundException : ExcerptsManagerException
    {
        public ExcerptNotFoundException()
            : this(null, null)
        {
        }

        public ExcerptNotFoundException(string excerptName)
            : this(excerptName, null)
        {
        }

        public ExcerptNotFoundException(string excerptName, Exception innerException)
            : base(innerException)
        {
            MessageArgs = new[] { excerptName };
        }

        protected ExcerptNotFoundException(SerializationInfo info, StreamingContext context)
            : base(info, context)
        {
        }

        public override string MessageId { get { return "NotAbleToFindRegionInFile"; } }
    }

    [Serializable]
    public class ExcerptFolderNotFoundException : ExcerptsManagerException
    {
        public ExcerptFolderNotFoundException()
            : this(null, null)
        {
        }

        public ExcerptFolderNotFoundException(string folderPath)
            : this(folderPath, null)
        {
        }

        public ExcerptFolderNotFoundException(string folderPath, Exception innerException)
            : base(innerException)
        {
            MessageArgs = new[] { folderPath };
        }

        protected ExcerptFolderNotFoundException(SerializationInfo info, StreamingContext context)
            : base(info, context)
        {
        }

        public override string MessageId { get { return "BadPathForIncludeFile"; } }
    }

    [Serializable]
    public class ExcerptLocFileNotFoundException : ExcerptsManagerException
    {
        public ExcerptLocFileNotFoundException()
            : this(null, null)
        {
        }

        public ExcerptLocFileNotFoundException(string fileName)
            : this(fileName, null)
        {
        }

        public ExcerptLocFileNotFoundException(string fileName, Exception innerException)
            : base(innerException)
        {
            MessageArgs = new[] { fileName };
        }

        protected ExcerptLocFileNotFoundException(SerializationInfo info, StreamingContext context)
            : base(info, context)
        {
        }

        public override string MessageId { get { return "​BadIncludeOrMissingMarkdownFileAndNoINTFile"; } }
    }

    [Serializable]
    public class ExcerptFileNotFoundException : ExcerptsManagerException
    {
        public ExcerptFileNotFoundException()
            : this(null, null)
        {
        }

        public ExcerptFileNotFoundException(string fileName)
            : this(fileName, null)
        {
        }

        public ExcerptFileNotFoundException(string fileName, Exception innerException)
            : base(innerException)
        {
            MessageArgs = new[] { fileName };
        }

        protected ExcerptFileNotFoundException(SerializationInfo info, StreamingContext context)
            : base(info, context)
        {
        }

        public override string MessageId { get { return "BadIncludeOrMissingMarkdownFile"; } }
    }


    public class ExcerptsManager
    {
        public ExcerptsManager(PreprocessedData data)
        {
            this.data = data;
        }

        public static EMExcerpt Get(
            ProcessedDocumentCache manager, string folderPath, string lang, string excerptName, out bool languageChanged)
        {
            var folder = new DirectoryInfo(folderPath);

            if (!folder.Exists)
            {
                throw new ExcerptFolderNotFoundException(folder.FullName);
            }

            FileInfo chosenFile;
            languageChanged = false;

            if (lang.ToLower() != "int")
            {
                var languageFileInfo = folder.GetFiles("*." + lang + ".udn");

                if (languageFileInfo.Length == 0)
                {
                    var intFileInfo = folder.GetFiles("*.INT.udn");

                    if (intFileInfo.Length == 0)
                    {
                        throw new ExcerptLocFileNotFoundException(Path.Combine(folder.FullName, "*." + lang + ".udn"));
                    }

                    languageChanged = true;
                    chosenFile = intFileInfo[0];
                }
                else
                {
                    chosenFile = languageFileInfo[0];
                }
            }
            else
            {
                var intFileInfo = folder.GetFiles("*.INT.udn");

                if (intFileInfo.Length == 0)
                {
                    throw new ExcerptFileNotFoundException(Path.Combine(folder.FullName, "*.INT.udn"));
                }

                chosenFile = intFileInfo[0];
            }

            return manager.Get(chosenFile.FullName).Excerpts.GetExcerpt(excerptName);
        }

        private EMExcerpt GetExcerpt(string excerptName)
        {
            try
            {
                return excerptsMap[excerptName];
            }
            catch (KeyNotFoundException)
            {
                throw new ExcerptNotFoundException(excerptName);
            }
        }

        private static readonly Regex ExcerptBlock = new Regex(@"
                    (?<=^)(?<prefix>\[EXCERPT:(?<name>[^\]]*?)\])          #Begins with EXCERPT:NameOfExcerpt
                    (?<content>[\s\S]*?)                                   #The Excerpt Text
                    (?<=^)(?<postfix>\[/EXCERPT(:\k<name>)?\])             #End with EXCERPT matching the NameOfExcerpt found in the start line
                    ", RegexOptions.IgnoreCase | RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        private static readonly Regex ExcerptInline = new Regex(@"
                    (?<=^)(?<prefix>\[EXCERPT:(?<name>[^\]]*?)\])(?<content>.*?)(?<postfix>\[/EXCERPT(:\k<name>)?\])
                    ", RegexOptions.IgnoreCase | RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        class ExcerptInfo
        {
            public int Index { get; private set; }
            public int ContentStart { get; private set; }
            public StringBuilder Content { get; private set; }

            public ExcerptInfo(int index, int contentStart, string initialContent)
            {
                Index = index;
                ContentStart = contentStart;
                Content = new StringBuilder();
                Content.Append(initialContent);
            }
        }

        public string ParseExcerpts(string text, TransformationData data, PreprocessedTextLocationMap map)
        {
            var excerptInfos = new Dictionary<string, ExcerptInfo>();

            text = ProcessExcerpt(ExcerptBlock, text, data, excerptInfos, map);
            text = ProcessExcerpt(ExcerptInline, text, data, excerptInfos, map);

            foreach (var excerptInfo in excerptInfos)
            {
                AddExcerpt(data, excerptInfo.Value, excerptInfo.Key);
            }

            AddExcerpt(data, new ExcerptInfo(map.GetOriginalPosition(0, PositionRounding.Down), 0, text), "");

            return text;
        }

        private static string ProcessExcerpt(Regex pattern, string text, TransformationData data, Dictionary<string, ExcerptInfo> excerptsInfos, PreprocessedTextLocationMap map)
        {
            var changes = new List<PreprocessingTextChange>();

            text = pattern.Replace(text, match => ParseExcerpt(match, excerptsInfos, data, changes));

            map.ApplyChanges(changes);

            return text;
        }

        private static string ParseExcerpt(Match match, Dictionary<string, ExcerptInfo> excerptsInfos, TransformationData data, List<PreprocessingTextChange> changes)
        {
            var name = match.Groups["name"].Value.ToLower();
            var contentGroup = match.Groups["content"];
            var content = contentGroup.Value;

            content = ExcerptBlock.Replace(content, m => ParseExcerpt(m, excerptsInfos, data, null));
            content = ExcerptInline.Replace(content, m => ParseExcerpt(m, excerptsInfos, data, null));

            if (excerptsInfos.ContainsKey(name))
            {
                excerptsInfos[name].Content.Append("\n" + content);
            }
            else
            {
                excerptsInfos.Add(name, new ExcerptInfo(match.Index, contentGroup.Index - match.Index, content));
            }

            if (changes != null)
            {
                changes.Add(PreprocessingTextChange.CreateRemove(match.Groups["prefix"]));
                changes.Add(PreprocessingTextChange.CreateRemove(match.Groups["postfix"]));
            }
            
            return content;
        }

        public bool Exists(string name)
        {
            return excerptsMap.ContainsKey(name);
        }

        public void AddExcerpt(string name, EMExcerpt excerpt)
        {
            excerptsMap.Add(name, excerpt);
        }

        private void AddExcerpt(TransformationData transData, ExcerptInfo info, string name)
        {
            var content = data.ReferenceLinks.Parse(info.Content.ToString(), null);

            var currentDoc = data.Document;
            AddExcerpt(name, new EMExcerpt(currentDoc, info.Index, info.ContentStart, content, transData, name == ""));
        }

        public List<string> GetExcerptNames()
        {
            return excerptsMap.Keys.Where(e => !string.IsNullOrWhiteSpace(e)).ToList();
        }

        private readonly Dictionary<string, EMExcerpt> excerptsMap = new Dictionary<string, EMExcerpt>();
        private readonly PreprocessedData data;
    }
}
