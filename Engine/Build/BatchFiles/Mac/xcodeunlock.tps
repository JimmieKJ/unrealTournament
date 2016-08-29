<?xml version="1.0" encoding="utf-8"?>
<TpsData xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <Name>xcodeunlock script</Name>
  <Location>/Engine/Build/BatchFiles/Mac/xcodeunlock.sh</Location>
  <Date>2016-06-28T14:56:50.292582-04:00</Date>
  <Function>This is a script to check out files in perforce which is designed to be used with xcode to automatically check out files on edit.</Function>
  <Justification>XCode does not natively support Perforce integration to allow checking out files on edit, and this is the official solution provided by Perforce.</Justification>
  <Platforms>
    <Platform>Mac</Platform>
  </Platforms>
  <Products>
    <Product>UDK4</Product>
    <Product>UE4</Product>
  </Products>
  <TpsType>Source Code</TpsType>
  <Eula>http://answers.perforce.com/articles/KB/2997</Eula>
  <RedistributeTo>
    <EndUserGroup>Licensees</EndUserGroup>
    <EndUserGroup>Git</EndUserGroup>
    <EndUserGroup>P4</EndUserGroup>
  </RedistributeTo>
  <Redistribute>false</Redistribute>
  <IsSourceAvailable>false</IsSourceAvailable>
  <NoticeType>Full EULA Text</NoticeType>
  <Notification>###
# Copyright (c) Perforce Software, Inc., 1997-2012. All rights reserved
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the 
# documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PERFORCE
# SOFTWARE, INC. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.</Notification>
  <LicenseFolder>/Engine/Source/ThirdParty/Licenses/xcodeunlock_license.txt</LicenseFolder>
</TpsData>