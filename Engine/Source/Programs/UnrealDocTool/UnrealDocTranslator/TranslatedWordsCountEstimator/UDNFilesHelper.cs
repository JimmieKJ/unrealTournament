// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using Perforce.P4;

namespace TranslatedWordsCountEstimator
{
    using System.Text.RegularExpressions;

    public static class UDNFilesHelper
    {
        public static string ChangeLanguage(string path, string langId)
        {
            string prefix;
            string oldLangId;
            string postfix;

            SplitByLang(path, out prefix, out oldLangId, out postfix);

            if (oldLangId != langId)
            {
                return prefix + langId.ToUpper() + postfix;
            }

            return path;
        }

        private static readonly Regex LangPattern = new Regex(@"^(?<prefix>.+\.)(?<lang>[A-Z]{3})?(?<postfix>\.udn)$", RegexOptions.Compiled);

        public static void SplitByLang(string path, out string prefix, out string lang, out string postfix)
        {
            if (!path.EndsWith(".udn"))
            {
                throw new ArgumentException("UDN file path have to end with '.udn'.");
            }

            var m = LangPattern.Match(path);

            prefix = m.Groups["prefix"].Value;
            lang = m.Groups["lang"].Value;
            postfix = m.Groups["postfix"].Value;
        }

        public static string ToInt(string path)
        {
            return ChangeLanguage(path, "INT");
        }

        public static string Get(FileMetaData file)
        {
            if (file.IsInClient)
            {
                return System.IO.File.ReadAllText(file.ClientPath.Path);
            }

            throw new InvalidOperationException("File not in the workspace.");
        }

        public static int GetWordCount(string content)
        {
            var wordsMatches = WordPattern.Matches(content);

            return wordsMatches.Count;
        }

        private static readonly Regex WordPattern = new Regex(@"\w+", RegexOptions.Compiled);
    }
}
