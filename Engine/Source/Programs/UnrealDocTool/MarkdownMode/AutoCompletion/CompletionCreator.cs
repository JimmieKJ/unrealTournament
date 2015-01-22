// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownMode.AutoCompletion
{
    using System.Windows.Media;
    using System.Windows.Media.Imaging;

    using MarkdownSharp;

    using Microsoft.VisualStudio.Language.Intellisense;

    public static class CompletionCreator
    {
        public static Completion Create(PathElement element)
        {
            var text = ((element.Type == PathElementType.Bookmark) ? "#" : "") + element.Text;

            return new Completion(text, text, text, GetImageByElementType(element.Type), null);
        }

        private static ImageSource GetImageByElementType(PathElementType type)
        {
            switch (type)
            {
                case PathElementType.Attachment:
                    return AttachmentIcon;
                case PathElementType.Bookmark:
                    return BookmarkIcon;
                case PathElementType.Excerpt:
                    return ExcerptIcon;
                case PathElementType.Folder:
                    return FolderIcon;
                case PathElementType.Image:
                    return ImageIcon;
                case PathElementType.Variable:
                    return VariableIcon;
                default:
                    throw new NotSupportedException("Not supported path element type.");
            }
        }

        class PathElementComparator : IComparer<PathElement>
        {
            static readonly Dictionary<PathElementType, int> TypeOrder = new Dictionary<PathElementType, int>()
                                                                           {
                                                                               { PathElementType.Variable, 0 },
                                                                               { PathElementType.Image, 1 },
                                                                               { PathElementType.Bookmark, 2 },
                                                                               { PathElementType.Folder, 3 },
                                                                               { PathElementType.Attachment, 4 },
                                                                               { PathElementType.Excerpt, 5 }
                                                                           };

            static int Compare(PathElementType x, PathElementType y)
            {
                return TypeOrder[x] - TypeOrder[y];
            }

            public int Compare(PathElement x, PathElement y)
            {
                var typeCompare = Compare(x.Type, y.Type);

                return typeCompare != 0 ? typeCompare : Comparer<string>.Default.Compare(x.Text, y.Text);
            }
        }

        private static readonly PathElementComparator Comparator = new PathElementComparator();

        public static List<Completion> SortAndCreateCompletions(List<PathElement> list)
        {
            return list.OrderBy(e => e, Comparator).Select(CompletionCreator.Create).ToList();
        }

        private static readonly ImageSource AttachmentIcon = new BitmapImage(new Uri("pack://application:,,,/EpicMarkdownMode;component/Resources/IntelliSense_attachment_16x.png"));
        private static readonly ImageSource BookmarkIcon = new BitmapImage(new Uri("pack://application:,,,/EpicMarkdownMode;component/Resources/IntelliSense_bookmark_16x.png"));
        private static readonly ImageSource FolderIcon = new BitmapImage(new Uri("pack://application:,,,/EpicMarkdownMode;component/Resources/IntelliSense_folder_16x.png"));
        private static readonly ImageSource ImageIcon = new BitmapImage(new Uri("pack://application:,,,/EpicMarkdownMode;component/Resources/IntelliSense_image_16x.png"));
        private static readonly ImageSource ExcerptIcon = new BitmapImage(new Uri("pack://application:,,,/EpicMarkdownMode;component/Resources/IntelliSense_excerpt_16x.png"));
        private static readonly ImageSource VariableIcon = new BitmapImage(new Uri("pack://application:,,,/EpicMarkdownMode;component/Resources/IntelliSense_variable_16x.png"));
    }
}
