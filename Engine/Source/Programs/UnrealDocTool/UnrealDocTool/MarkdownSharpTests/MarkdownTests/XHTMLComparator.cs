// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Xml;
using System.Text.RegularExpressions;

namespace MarkdownSharpTests.MarkdownTests
{
    public static class XHTMLComparator
    {
        public static bool IsEqual(string left, string right)
        {
            var leftDocument = new XmlDocument();
            leftDocument.LoadXml(left);

            var rightDocument = new XmlDocument();
            rightDocument.LoadXml(right);

            return IsEqual(leftDocument.DocumentElement, rightDocument.DocumentElement);
        }

        private static bool IsEqual(XmlAttributeCollection left, XmlAttributeCollection right)
        {
            if (left == null)
            {
                return right == null;
            }

            if (right == null)
            {
                return false;
            }

            if (left.Count != right.Count)
            {
                return false;
            }

            foreach (XmlAttribute attr in left)
            {
                var rightAttrNode = right.GetNamedItem(attr.Name);

                if (rightAttrNode == null)
                {
                    return false;
                }

                if ((rightAttrNode as XmlAttribute).Value != attr.Value)
                {
                    return false;
                }
            }

            return true;
        }

        private static bool IsEqual(XmlNode left, XmlNode right, bool simplifiedWhitespaceComparison = true)
        {
            if (left.Name != right.Name || left.HasChildNodes != right.HasChildNodes
                || left.ChildNodes.Count != right.ChildNodes.Count || left.NodeType != right.NodeType
                || !IsEqual(left.Attributes, right.Attributes))
            {
                return false;
            }

            if (left.HasChildNodes)
            {
                for (var i = 0; i < left.ChildNodes.Count; ++i)
                {
                    var leftChild = left.ChildNodes[i];
                    var rightChild = right.ChildNodes[i];

                    if (!IsEqual(leftChild, rightChild, left.Name != "code" && simplifiedWhitespaceComparison))
                    {
                        return false;
                    }
                }
            }
            
            switch (left.NodeType)
            {
                case XmlNodeType.Text:
                    return IsTextEqual(left.InnerText, right.InnerText, simplifiedWhitespaceComparison);

                case XmlNodeType.Element:
                    return true;

                default:
                    throw new ArgumentOutOfRangeException();
            }
        }

        private static readonly Regex WhiteSpacePattern = new Regex(@"\s+", RegexOptions.Compiled | RegexOptions.Singleline);

        private static bool IsTextEqual(string left, string right, bool simplifiedWhitespaceComparison)
        {
            if (simplifiedWhitespaceComparison)
            {
                left = WhiteSpacePattern.Replace(left, " ").Trim();
                right = WhiteSpacePattern.Replace(right, " ").Trim();
            }

            return left.Equals(right);
        }
    }
}
