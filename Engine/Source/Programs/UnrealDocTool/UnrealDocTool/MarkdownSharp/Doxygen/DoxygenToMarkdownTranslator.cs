// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Xml;
using System.Xml.XPath;

namespace MarkdownSharp.Doxygen
{
    public static class DoxygenToMarkdownTranslator
    {
        public delegate string ReferenceLinkGeneratorDelegate(XmlNode refNode);

        public static string Translate(XPathNavigator descriptionNode, ReferenceLinkGeneratorDelegate refLinkGenerator)
        {
            // obtaining desc parent node
            var descParentNode = descriptionNode.Clone();
            descParentNode.MoveToParent();

            return ParseXML((XmlNode)descParentNode.UnderlyingObject,
                (XmlNode)descriptionNode.UnderlyingObject, refLinkGenerator);
        }


        #region Jeff Wilson's APIDocTool contribution

        public static int TabDepth = 4;

        /** Parses markdown text out of an XML node 
         * 
         * @param parent parent node to give context to the child we want to parse 
         * @param child xml node we want to parse into Markdown 
         * @return a string containing the text from the child node 
         */
        public static String ParseXML(XmlNode parent, XmlNode child, ReferenceLinkGeneratorDelegate refLinkGenerator)
        {
            string output = "";

            if (child.HasChildNodes)
            {
                switch (child.Name)
                {
                    case "para":
                        if (parent.Name != "listitem" && parent.Name != "parameterdescription" && parent.Name != "simplesect")
                        {
                            output += Environment.NewLine;
                            output += ParseChildren(child, refLinkGenerator);
                        }
                        else if (parent.Name == "listitem")
                        {
                            output += Environment.NewLine;
                            if (child == parent.FirstChild)
                            {
                                output += GetTabs(TabDepth) + "* " + ParseChildren(child, refLinkGenerator);
                            }
                            else
                            {
                                output += Environment.NewLine + GetTabs(TabDepth + 1) + ParseChildren(child, refLinkGenerator);
                            }
                        }
                        else if (parent.Name == "parameterdescription" || parent.Name == "simplesect")
                        {
                            output += ParseChildren(child, refLinkGenerator);
                        }
                        break;
                    case "ulink":
                        output += GetTabs(TabDepth) + String.Format("[{0}]({1})", ParseChildren(child, refLinkGenerator), child.Attributes.GetNamedItem("url").InnerText);
                        break;
                    case "ref":
                        output += refLinkGenerator(child);
                        /*
                        String RefName = child.InnerText;
                        String linkpath = refLinkGenerator(child, RefName, child.Attributes.GetNamedItem("kindref").InnerText);
                        if (linkpath != "")
                        {
                            output += String.Format("[{0}]({1})", RefName, linkpath);
                        }
                        else
                        {
                            output += ParseChildren(child, refLinkGenerator);
                        }
                         */
                        break;
                    case "bold":
                        output += GetTabs(TabDepth) + String.Format("**{0}**", ParseChildren(child, refLinkGenerator));
                        break;
                    case "emphasis":
                        output += GetTabs(TabDepth) + String.Format("_{0}_", ParseChildren(child, refLinkGenerator));
                        break;
                    case "computeroutput":
                        output += GetTabs(TabDepth) + String.Format("`{0}`", ParseChildren(child, refLinkGenerator));
                        break;
                    case "itemizedlist":
                        if (parent.ParentNode.Name == "listitem")
                        {
                            TabDepth++;
                        }
                        else
                        {
                            output += Environment.NewLine;
                        }
                        output += ParseChildren(child, refLinkGenerator);
                        if (parent.ParentNode.Name == "listitem")
                        {
                            TabDepth--;
                        }
                        else
                        {
                            output += Environment.NewLine + Environment.NewLine;
                        }
                        break;
                    case "parameterlist":
                        break;
                    case "parameteritem":
                        break;
                    case "parameternamelist":
                        break;
                    case "parameterdescription":
                        break;
                    case "simplesect":
                        break;
                    case "xrefsect":
                        break;
                    default:
                        output += ParseChildren(child, refLinkGenerator);
                        break;
                }
            }
            else
            {
                output += GetTabs(TabDepth) + child.InnerText;
            }

            return output;
        }

        /** Class ParseXML for all child nodes of the given child
         * 
         * @param child xml node we want to parse all children into Markdown 
         * @return a string containing the text from the child node 
         */
        public static String ParseChildren(XmlNode child, ReferenceLinkGeneratorDelegate refLinkGenerator)
        {
            String output = "";

            foreach (XmlNode childNode in child.ChildNodes)
            {
                output += ParseXML(child, childNode, refLinkGenerator);
            }

            return output;
        }

        public static String ParseRefLink(XmlNode child, String RefName, String KindRef)
        {
            /*
            if (KindRef == "compound")
            {
                if (APIClass.Classes.ContainsKey(RefName))
                {
                    return APIClass.Classes[RefName].GetLinkPath();
                }
                else if (APIStruct.Structs.ContainsKey(RefName))
                {
                    return APIStruct.Structs[RefName].GetLinkPath();
                }
                else if (APIInterface.Interfaces.ContainsKey(RefName))
                {
                    return APIInterface.Interfaces[RefName].GetLinkPath();
                }
                else if (APIEnum.Enums.ContainsKey(RefName))
                {
                    return APIEnum.Enums[RefName].GetLinkPath();
                }
                else
                {
                    return "";
                }
            }
            else if (KindRef == "member")
            {
                if (APIEnum.Enums.ContainsKey(RefName))
                {
                    return APIEnum.Enums[RefName].GetLinkPath();
                }
                else if (APITypeDef.TypeDefs.ContainsKey(RefName))
                {
                    return ParseRefLink(child, APITypeDef.TypeDefs[RefName].Type, "compound");
                }
                else
                {
                    if (RefName.Contains("::"))
                    {
                        if (RefName.Contains("("))
                        {
                            RefName.Substring(0, RefName.IndexOf("("));
                        }
                        return String.Format("API:{0}", RefName);
                        //String ClassName = RefName.Substring(0, RefName.IndexOf("::"));
                        //String MemberName = RefName.Replace(ClassName + "::", "");
                        //if(APIClass.Classes.ContainsKey(ClassName))
                        //{
                        //    APIClass Class = APIClass.Classes[ClassName];
                        //    String Arguments = "";
                        //    if (MemberName.Contains("("))
                        //    {
                        //        Arguments = MemberName.Substring(MemberName.IndexOf("("));
                        //        MemberName = MemberName.Substring(0, MemberName.IndexOf("("));
                        //    }
                        //    if (Class.Functions.ContainsKey(MemberName))
                        //    {
                        //        if (Class.Functions[MemberName].Count > 1)
                        //        {
                        //            foreach (APIFunction func in Class.Functions[MemberName])
                        //            {
                        //                if (func.Arguments.Replace(" ", "") == Arguments.Replace(" ", ""))
                        //                {
                        //                    return func.GetBaseFunctionLinkPath();
                        //                }
                        //            }
                        //            return Class.Functions[MemberName][0].GetBaseFunctionLinkPath();
                        //        }
                        //        else
                        //        {
                        //            return Class.Functions[MemberName][0].GetLinkPath();
                        //        }
                        //    }
                        //    else
                        //    {
                        //        foreach (APIVariable var in Class.Variables)
                        //        {
                        //            if (var.Name == MemberName)
                        //            {
                        //                return var.GetLinkPath();
                        //            }
                        //        }
                        //    }
                        //}
                    }
                    return "";
                }
            }
            else
            {
                return ParseChildren(child);
            }
            */
            return "Link to " + RefName;
        }

        public static string GetTabs(int Depth)
        {
            String output = "";

            for (int i = Depth; i > 0; i--)
            {
                output += "\t";
            }

            return output;
        }

        #endregion
    }
}
