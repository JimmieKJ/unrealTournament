Unreal Engine HTML5 SDK. 
 
  - Download emscripten SDK at a seperate location. 
  - Replicate current directory structure.
  - Delete some unsused clang binaries to save space. 
	clang-cl
	clang-check
	llvm-c-test
	llvm-objdump
	llvm-mv
	llvm-rtdyld
	diagtool
	clang-format
  - Delete emscripten/tests folder from the emscripten directory to save space. 
  - Copy folders for each platforms to this folder, 
  - Update version string in HTML5SDKInfo.cs
  - Perforce: Delete older SDK, Checkin new SDK. 

   

