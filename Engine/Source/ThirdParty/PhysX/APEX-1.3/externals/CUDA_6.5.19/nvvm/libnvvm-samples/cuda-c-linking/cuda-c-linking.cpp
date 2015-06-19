/*
 * Copyright 1993-2012 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

#include "llvm/IRBuilder.h"
#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"

#include <memory>

#if __APPLE__
#include "CUDA/cuda.h"
#else
#include "cuda.h"
#endif
 
#include "nvvm.h"

#include "drvapi_error_string.h"
#include "DDSWriter.h"



using namespace llvm;



static cl::opt<bool>
SaveCubin("save-cubin", cl::desc("Write linker cubin to disk"),
          cl::init(false));
static cl::opt<bool>
SaveIR("save-ir", cl::desc("Write LLVM IR to disk"),
       cl::init(false));
static cl::opt<bool>
SavePTX("save-ptx", cl::desc("Write PTX to disk"),
        cl::init(false));




// width/height of output image
const unsigned width = 1024;
const unsigned height = 512;






// This will output the proper CUDA error strings in the event that a CUDA
// host call returns an error
#define checkCudaErrors(err)  __checkCudaErrors (err, __FILE__, __LINE__)

// These are the inline versions for all of the SDK helper functions
void __checkCudaErrors(CUresult err, const char *file, const int line) {
  if(CUDA_SUCCESS != err) {
    errs() << "checkCudeErrors() Driver API error = " << err << "\""
           << getCudaDrvErrorString(err) << "\" from file <" << file
           << ", line " << line << "\n";
    exit(-1);
  }
}

/// checkNVVMCall - Verifies that NVVM result code is success, or terminate
/// otherwise.
void checkNVVMCall(nvvmResult Res) {
  if (Res != NVVM_SUCCESS) {
    errs() << "libnvvm call failed\n";
    exit(1);
  }
}



/// generateModule - Generate and LLVM IR module that calls an
/// externally-defined function
Module* generateModule() {

  LLVMContext &context = getGlobalContext();

  // Create the module
  Module *mod = new Module("nvvm-module", context);

  // Set appropriate data layout
  if (sizeof(void*) == 8)
    mod->setDataLayout("e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-"
                       "i64:64:64-f32:32:32-f64:64:64-v16:16:16-v32:32:32-"
                       "v64:64:64-v128:128:128-n16:32:64");
  else
    mod->setDataLayout("e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-"
                       "i64:64:64-f32:32:32-f64:64:64-v16:16:16-v32:32:32-"
                       "v64:64:64-v128:128:128-n16:32:64");

  // Get pointers to some commonly-used types
  Type *voidTy = Type::getVoidTy(context);
  Type *floatTy = Type::getFloatTy(context);
  Type *i32Ty = Type::getInt32Ty(context);
  Type *floatGenericPtrTy = PointerType::get(floatTy, /* address space */ 0);
  Type *floatGlobalPtrTy = PointerType::get(floatTy, /* address space */ 1);

  // float* @llvm.nvvm.ptr.global.to.gen.p0f32.p1f32(float addrspace(1)*)
  Type *ptrToGenParamTys[] = { floatGlobalPtrTy };
  FunctionType *ptrToGenTy = FunctionType::get(floatGenericPtrTy,
                                               ptrToGenParamTys, false);
  Constant *ptrToGenFunc =
    mod->getOrInsertTargetIntrinsic("llvm.nvvm.ptr.global.to.gen.p0f32.p1f32",
                                    ptrToGenTy, AttrListPtr());

  // void @mandelbrot(float*)
  Type *mandelbrotParamTys[] = { floatGenericPtrTy };
  FunctionType *mandelbrotTy = FunctionType::get(voidTy, mandelbrotParamTys,
                                                 false);
  Constant *mandelbrotFunc = mod->getOrInsertTargetIntrinsic("mandelbrot",
                                                             mandelbrotTy,
                                                             AttrListPtr());  

  // Kernel argument types
  Type *paramTys[] = { floatGlobalPtrTy };

  // Kernel function type
  FunctionType *funcTy = FunctionType::get(voidTy, paramTys, false);

  // Kernel function
  Function *func = Function::Create(funcTy, GlobalValue::ExternalLinkage,
                                    "kernel", mod);
  func->arg_begin()->setName("ptr");

  // 'entry' basic block in kernel function
  BasicBlock *entry = BasicBlock::Create(context, "entry", func);

  // Build the entry block
  IRBuilder<> builder(entry);

  Value *genPtr = builder.CreateCall(ptrToGenFunc, func->arg_begin());
  builder.CreateCall(mandelbrotFunc, genPtr);

  builder.CreateRetVoid();

  // Create kernel metadata
  Value *mdVals[] = {
    func, MDString::get(context, "kernel") , ConstantInt::get(i32Ty, 1)
  };
  MDNode *kernelMD = MDNode::get(context, mdVals);

  NamedMDNode *nvvmAnnot = mod->getOrInsertNamedMetadata("nvvm.annotations");
  nvvmAnnot->addOperand(kernelMD);

  return mod;
}


/// generatePtx - Uses libnvvm to compile an LVVM IR module to PTX.
std::string generatePtx(const std::string &module,
                        int devMajor, int devMinor,
                        const char *moduleName) {
  nvvmProgram compileUnit;
  nvvmResult res;

  // NVVM Initialization
  checkNVVMCall(nvvmCreateProgram(&compileUnit));

  // Create NVVM compilation unit from LLVM IR
  checkNVVMCall(nvvmAddModuleToProgram(compileUnit,
                                       module.c_str(), module.size(),
                                       moduleName));


  std::string computeArg = "-arch=compute_";
  computeArg += utostr(devMajor);
  computeArg += utostr(devMinor);

  const char *options[] = { computeArg.c_str() };

  // Compile LLVM IR into PTX
  res = nvvmCompileProgram(compileUnit, 1, options);
  if (res != NVVM_SUCCESS) {
    errs() << "nvvmCompileProgram failed!\n";
    size_t logSize;
    nvvmGetProgramLogSize(compileUnit, &logSize);
    char *msg = new char[logSize];
    nvvmGetProgramLog(compileUnit, msg);
    errs() << msg << "\n";
    delete [] msg;
    exit(1);
  }

  // Get final PTX size
  size_t ptxSize = 0;
  checkNVVMCall(nvvmGetCompiledResultSize(compileUnit, &ptxSize));

  // Get final PTX
  char *ptx = new char[ptxSize];
  checkNVVMCall(nvvmGetCompiledResult(compileUnit, ptx));

  // Clean-up libNVVM
  checkNVVMCall(nvvmDestroyProgram(&compileUnit));

  return std::string(ptx);
}


/// main - Program entry point
int main(int argc, char **argv) {
  CUdevice device;
  CUmodule cudaModule;
  CUcontext context;
  CUfunction function;
  CUlinkState linker;
  int devCount;

  cl::ParseCommandLineOptions(argc, argv, "cuda-c-linking");

  // Locate the pre-built library
  sys::Path libpath = sys::Path::GetMainExecutable(argv[0], (void*)main);
  const char *mathlibFile;
  if (sizeof(void*) == 8)
    mathlibFile = "libmathfuncs64.a";
  else
    mathlibFile = "libmathfuncs32.a";
  libpath.eraseComponent();
  libpath.appendComponent(mathlibFile);

  if (!libpath.canRead()) {
    errs() << "Unable to locate math library, expected at "
                 << libpath.str() << "\n";
    return 1;
  }

  outs() << "Using math library: " << libpath.str() << "\n";


  // CUDA initialization
  checkCudaErrors(cuInit(0));
  checkCudaErrors(cuDeviceGetCount(&devCount));
  checkCudaErrors(cuDeviceGet(&device, 0));

  char name[128];
  checkCudaErrors(cuDeviceGetName(name, 128, device));
  outs() << "Using CUDA Device [0]: " << name << "\n";

  int devMajor, devMinor;
  checkCudaErrors(cuDeviceComputeCapability(&devMajor, &devMinor, device));
  outs() << "Device Compute Capability: " 
         << devMajor << "." << devMinor << "\n";
  if (devMajor < 2) {
    errs() << "ERROR: Device 0 is not SM 2.0 or greater\n";
    return 1;
  }


  // Generate the IR module
  std::string moduleStr;
  std::auto_ptr<Module> module;
  module.reset(generateModule());

  if (SaveIR) {
    std::string err;
    raw_fd_ostream out("cuda-c-linking.kernel.ll", err);
    out << *(module.get());
  }

  // Write module to string
  {
    llvm::raw_string_ostream str(moduleStr);
    str << *module.get();
  }

  // Generate PTX
  std::string ptx = generatePtx(moduleStr, devMajor, devMinor,
                                module->getModuleIdentifier().c_str());

  if (SavePTX) {
    std::string err;
    raw_fd_ostream out("cuda-c-linking.kernel.ptx", err);
    out << ptx;
  }

  // Create driver context
  checkCudaErrors(cuCtxCreate(&context, 0, device));

  char linkerInfo[1024];
  char linkerErrors[1024];

  linkerInfo[0] = 0;
  linkerErrors[0] = 0;

  CUjit_option linkerOptions[] = {
    CU_JIT_INFO_LOG_BUFFER,
    CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES,
    CU_JIT_ERROR_LOG_BUFFER,
    CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES,
    CU_JIT_LOG_VERBOSE
  };

  void *linkerOptionValues[] = {
    linkerInfo,
    reinterpret_cast<void*>(1024),
    linkerErrors,
    reinterpret_cast<void*>(1024),
    reinterpret_cast<void*>(1)
  };

  // Create JIT linker and create final CUBIN
  checkCudaErrors(cuLinkCreate(5, linkerOptions, linkerOptionValues, &linker));
  checkCudaErrors(cuLinkAddData(linker, CU_JIT_INPUT_PTX, (void*)ptx.c_str(),
                                ptx.size(), "<compiled-ptx>", 0, NULL, NULL));
  checkCudaErrors(cuLinkAddFile(linker, CU_JIT_INPUT_LIBRARY, libpath.c_str(),
                                0, NULL, NULL));
 
  void *cubin;
  size_t cubinSize;
  checkCudaErrors(cuLinkComplete(linker, &cubin, &cubinSize));

  outs() << "Linker Log:\n" << linkerInfo << "\n" << linkerErrors << "\n";
  
  if (SaveCubin) {
    std::string err;
    raw_fd_ostream out("cuda-c-linking.linked.cubin", err,
                       raw_fd_ostream::F_Binary);
    out.write(reinterpret_cast<char*>(cubin), cubinSize);
  }

  // Create module for object
  checkCudaErrors(cuModuleLoadDataEx(&cudaModule, cubin, 0, 0, 0));

  // Now that the CUBIN is loaded, we can release the linker.
  checkCudaErrors(cuLinkDestroy(linker));

  // Get kernel function
  checkCudaErrors(cuModuleGetFunction(&function, cudaModule, "kernel"));



  // Device data
  CUdeviceptr devBuffer;
  checkCudaErrors(cuMemAlloc(&devBuffer, sizeof(float)*width*height*4));

  float* data = new float[width*height*4];

  // Each thread will generate one pixel, and we'll subdivide the problem into
  // 16x16 chunks
  unsigned blockSizeX = 16;
  unsigned blockSizeY = 16;
  unsigned blockSizeZ = 1;
  unsigned gridSizeX = width/16;
  unsigned gridSizeY = height/16;
  unsigned gridSizeZ = 1;


  // Kernel parameters
  void *KernelParams[] = { &devBuffer };

  outs() << "Launching kernel\n";

  // Kernel launch
  checkCudaErrors(cuLaunchKernel(function, gridSizeX, gridSizeY, gridSizeZ,
                                 blockSizeX, blockSizeY, blockSizeZ,
                                 0, NULL, KernelParams, NULL));

  // Retrieve device data
  checkCudaErrors(cuMemcpyDtoH(&data[0], devBuffer, sizeof(float)*width*height*4));


  writeDDS("mandelbrot.dds", data, width, height);
  outs() << "Output saved to mandelbrot.dds\n";

  // Clean up after ourselves
  delete [] data;

  // Clean-up
  checkCudaErrors(cuMemFree(devBuffer));
  checkCudaErrors(cuModuleUnload(cudaModule));
  checkCudaErrors(cuCtxDestroy(context));

  return 0;
}
