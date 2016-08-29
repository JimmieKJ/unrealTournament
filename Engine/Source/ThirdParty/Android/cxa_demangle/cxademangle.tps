<?xml version="1.0" encoding="utf-8"?>
<TpsData xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <Name>cxa demangle</Name>
  <Location>/Engine/Source/ThirdParty/Android/cxa_demangle/</Location>
  <Date>2016-04-12T19:25:57.3258913-04:00</Date>
  <Function />
  <Justification>We need callstack printing when running on Android devices and we need this function to convert a mangled function name into a human-readable one.  We link with cxx-stl from stlport, but that doesn’t include the abi::__cxa_demangle function in the library. The gnu-libstdc++ libraries also included with the NDK do include this function, but we don’t link against gnu-libstdc++ and I don’t want to do so just for this function. The easiest solution is to compile the __cxa_demangle source code file and link it manually.</Justification>
  <Platforms>
    <Platform>Android</Platform>
  </Platforms>
  <Products>
    <Product>UDK4</Product>
    <Product>UE4</Product>
  </Products>
  <TpsType>lib</TpsType>
  <Eula>https://android.googlesource.com/platform/external/libcxxabi_35a/+/master/LICENSE.TXT</Eula>
  <RedistributeTo>
    <EndUserGroup>Licensees</EndUserGroup>
    <EndUserGroup>Git</EndUserGroup>
    <EndUserGroup>P4</EndUserGroup>
  </RedistributeTo>
  <Redistribute>false</Redistribute>
  <IsSourceAvailable>false</IsSourceAvailable>
  <NoticeType>Full EULA Text</NoticeType>
  <Notification>The MIT License (MIT)

Copyright (c) &lt;year&gt; &lt;copyright holders&gt;

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.</Notification>
  <LicenseFolder>/Engine/Source/ThirdParty/Licenses/CXADemangle_License.txt</LicenseFolder>
</TpsData>