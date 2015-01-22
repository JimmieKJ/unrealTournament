// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;

namespace MarkdownMode
{
    static class GuidList
    {
        public const string guidMarkdownPackagePkgString = "06fa6fbc-681e-4fdc-bd58-81e8401c5e00";
        public const string guidMarkdownPackageCmdSetString = "36c5243f-48c6-4666-bc21-8c398b312f0f";
        public const string guidToolWindowPersistanceString = "acd82a5f-9c35-400b-b9d0-f97925f3b312";
        public const string guidMarkdownUIContextString = "E12EADF7-291A-4E55-AD07-390DAC69BB4D";

        public static readonly Guid guidMarkdownPackageCmdSet = new Guid(guidMarkdownPackageCmdSetString);
        public static readonly Guid guidMarkdownUIContext = new Guid(guidMarkdownUIContextString);
    };

    static class PkgCmdId
    {
        public const int cmdidMarkdownOpenImagesFolder = 0x0101;
        public const int cmdidMarkdownPreviewWindow = 0x0102;
        public const int cmdidMarkdownRebuildDoxygenDatabase = 0x0103;
        public const int cmdidMarkdownCheckP4Images = 0x0104;
        public const int cmdidMarkdownBoldText = 0x0105;
        public const int cmdidMarkdownItalicText = 0x0106;
        public const int cmdidMarkdownCodeText = 0x0107;
        public const int cmdidMarkdownPreviewPage = 0x0108;
        public const int cmdidMarkdownPublishPage = 0x0109;
        public const int cmdidMarkdownPublishRecursive = 0x01110;
        public const int CmdidMarkdownPublishRecursiveCtxt = 0x0111;
        public const int cmdidMarkdownPublishPagesCtxt = 0x0112;
        public const int cmdidMarkdownDisableParsing = 0x0113;
        public const int cmdidMarkdownDisableHighlighting = 0x0114;

        public const int MarkdownToolbar = 0x1020;
        public const int MarkdownMenuGroup = 0x1021;
        public const int MarkdownMenuDoxygenGroup = 0x1022;
        public const int MarkdownMenuP4Group = 0x1023;
        public const int MarkdownContextMenuGroup = 0x1024;
        public const int MarkdownContextSLNEXPItem = 0x1025;
        public const int MarkdownContextSLNEXPMultiItem = 0x1026;
        public const int MarkdownParseOptions = 0x1027;

        public const int cmdidMarkdownNavigateToCombo = 0x0131;
        public const int cmdidMarkdownNavigateToComboGetList = 0x0132;

        public const int cmdidMarkdownPublishList = 0x9000;
    };
}
