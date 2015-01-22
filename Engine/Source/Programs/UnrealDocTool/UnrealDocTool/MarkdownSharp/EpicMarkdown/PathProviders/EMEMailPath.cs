// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace MarkdownSharp.EpicMarkdown.PathProviders
{
    using System.Text.RegularExpressions;

    public class EMEMailPath : EMPathProvider
    {
        private readonly string userPath;

        private static readonly Regex EMailPattern = new Regex(
            @"^mailto\:[_.%+-a-z0-9]+@[_.a-z0-9]+\.[a-z0-9]+$",
            RegexOptions.Singleline | RegexOptions.IgnoreCase | RegexOptions.Singleline);

        public EMEMailPath(string userPath)
        {
            this.userPath = userPath;
        }

        public static bool Verify(string userPath)
        {
            return EMailPattern.IsMatch(userPath);
        }

        public override string GetPath(TransformationData data)
        {
            return userPath;
        }
    }
}