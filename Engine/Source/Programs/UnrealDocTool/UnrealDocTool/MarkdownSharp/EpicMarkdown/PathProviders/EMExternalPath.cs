// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using MarkdownSharp.Preprocessor;

namespace MarkdownSharp.EpicMarkdown.PathProviders
{
    using System.Text.RegularExpressions;

    public class EMExternalPath : EMPathProvider
    {
        private readonly string path;

        public EMExternalPath(string path)
        {
            this.path = Preprocessor.Preprocessor.UnescapeChars(path, true);
        }

        public override string GetPath(TransformationData data)
        {
            return Normalizer.NormalizePath(path);
        }

        private static readonly Regex ExternalPathPattern = new Regex(@"^\w+\:\/\/", RegexOptions.Compiled);

        public static bool Verify(string verifyPath)
        {
            return ExternalPathPattern.IsMatch(verifyPath);
        }
    }
}
