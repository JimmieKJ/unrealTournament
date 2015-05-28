/*
 * Copyright 2011-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO LICENSEE:
 *
 * This source code and/or documentation ("Licensed Deliverables") are
 * subject to NVIDIA intellectual property rights under U.S. and
 * international Copyright laws.
 *
 * These Licensed Deliverables contained herein is PROPRIETARY and
 * CONFIDENTIAL to NVIDIA and is being provided under the terms and
 * conditions of a form of NVIDIA software license agreement by and
 * between NVIDIA and Licensee ("License Agreement") or electronically
 * accepted by Licensee.  Notwithstanding any terms or conditions to
 * the contrary in the License Agreement, reproduction or disclosure
 * of the Licensed Deliverables to any third party without the express
 * written consent of NVIDIA is prohibited.
 *
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, NVIDIA MAKES NO REPRESENTATION ABOUT THE
 * SUITABILITY OF THESE LICENSED DELIVERABLES FOR ANY PURPOSE.  IT IS
 * PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.
 * NVIDIA DISCLAIMS ALL WARRANTIES WITH REGARD TO THESE LICENSED
 * DELIVERABLES, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY,
 * NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY
 * SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THESE LICENSED DELIVERABLES.
 *
 * U.S. Government End Users.  These Licensed Deliverables are a
 * "commercial item" as that term is defined at 48 C.F.R. 2.101 (OCT
 * 1995), consisting of "commercial computer software" and "commercial
 * computer software documentation" as such terms are used in 48
 * C.F.R. 12.212 (SEPT 1995) and is provided to the U.S. Government
 * only as a commercial end item.  Consistent with 48 C.F.R.12.212 and
 * 48 C.F.R. 227.7202-1 through 227.7202-4 (JUNE 1995), all
 * U.S. Government End Users acquire the Licensed Deliverables with
 * only those rights set forth herein.
 *
 * Any use of the Licensed Deliverables in individual and commercial
 * software must include, in the user documentation and internal
 * comments to the code, the above Disclaimer and U.S. Government End
 * Users Notice.
 */

#if !defined(_CUPTI_ACTIVITY_H_)
#define _CUPTI_ACTIVITY_H_

#include <cupti_callbacks.h>
#include <cupti_events.h>
#include <cupti_metrics.h>
#include <cupti_result.h>

#ifndef CUPTIAPI
#ifdef _WIN32
#define CUPTIAPI __stdcall
#else
#define CUPTIAPI
#endif
#endif

#if defined(__LP64__)
#define CUPTILP64 1
#elif defined(_WIN64)
#define CUPTILP64 1
#else
#undef CUPTILP64
#endif

#define ACTIVITY_RECORD_ALIGNMENT 8
#if defined(_WIN32) // Windows 32- and 64-bit
#define START_PACKED_ALIGNMENT __pragma(pack(push,1)) // exact fit - no padding
#define PACKED_ALIGNMENT __declspec(align(ACTIVITY_RECORD_ALIGNMENT))
#define END_PACKED_ALIGNMENT __pragma(pack(pop))
#elif defined(__GNUC__) // GCC
#define START_PACKED_ALIGNMENT
#define PACKED_ALIGNMENT __attribute__ ((__packed__)) __attribute__ ((aligned (ACTIVITY_RECORD_ALIGNMENT)))
#define END_PACKED_ALIGNMENT
#else // all other compilers
#define START_PACKED_ALIGNMENT
#define PACKED_ALIGNMENT
#define END_PACKED_ALIGNMENT
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * \defgroup CUPTI_ACTIVITY_API CUPTI Activity API
 * Functions, types, and enums that implement the CUPTI Activity API.
 * @{
 */

/**
 * \brief The kinds of activity records.
 *
 * Each activity record kind represents information about a GPU or an
 * activity occurring on a CPU or GPU. Each kind is associated with a
 * activity record structure that holds the information associated
 * with the kind.
 * \see CUpti_Activity
 * \see CUpti_ActivityAPI
 * \see CUpti_ActivityContext
 * \see CUpti_ActivityDevice
 * \see CUpti_ActivityDeviceAttribute
 * \see CUpti_ActivityEvent
 * \see CUpti_ActivityEventInstance
 * \see CUpti_ActivityKernel
 * \see CUpti_ActivityKernel2
 * \see CUpti_ActivityKernel3
 * \see CUpti_ActivityCdpKernel
 * \see CUpti_ActivityPreemption
 * \see CUpti_ActivityMemcpy
 * \see CUpti_ActivityMemcpy2
 * \see CUpti_ActivityMemset
 * \see CUpti_ActivityMetric
 * \see CUpti_ActivityMetricInstance
 * \see CUpti_ActivityName
 * \see CUpti_ActivityMarker
 * \see CUpti_ActivityMarkerData
 * \see CUpti_ActivitySourceLocator
 * \see CUpti_ActivityGlobalAccess
 * \see CUpti_ActivityGlobalAccess2
 * \see CUpti_ActivityBranch
 * \see CUpti_ActivityBranch2
 * \see CUpti_ActivityOverhead
 * \see CUpti_ActivityEnvironment
 * \see CUpti_ActivityInstructionExecution
 * \see CUpti_ActivityUnifiedMemoryCounter
 * \see CUpti_ActivityFunction
 * \see CUpti_ActivityModule
 * \see CUpti_ActivitySharedAccess
 */
typedef enum {
  /**
   * The activity record is invalid.
   */
  CUPTI_ACTIVITY_KIND_INVALID  = 0,
  /**
   * A host<->host, host<->device, or device<->device memory copy. The
   * corresponding activity record structure is \ref
   * CUpti_ActivityMemcpy.
   */
  CUPTI_ACTIVITY_KIND_MEMCPY   = 1,
  /**
   * A memory set executing on the GPU. The corresponding activity
   * record structure is \ref CUpti_ActivityMemset.
   */
  CUPTI_ACTIVITY_KIND_MEMSET   = 2,
  /**
   * A kernel executing on the GPU. The corresponding activity record
   * structure is \ref CUpti_ActivityKernel3.
   */
  CUPTI_ACTIVITY_KIND_KERNEL   = 3,
  /**
   * A CUDA driver API function execution. The corresponding activity
   * record structure is \ref CUpti_ActivityAPI.
   */
  CUPTI_ACTIVITY_KIND_DRIVER   = 4,
  /**
   * A CUDA runtime API function execution. The corresponding activity
   * record structure is \ref CUpti_ActivityAPI.
   */
  CUPTI_ACTIVITY_KIND_RUNTIME  = 5,
  /**
   * An event value. The corresponding activity record structure is
   * \ref CUpti_ActivityEvent.
   */
  CUPTI_ACTIVITY_KIND_EVENT    = 6,
  /**
   * A metric value. The corresponding activity record structure is
   * \ref CUpti_ActivityMetric.
   */
  CUPTI_ACTIVITY_KIND_METRIC   = 7,
  /**
   * Information about a device. The corresponding activity record
   * structure is \ref CUpti_ActivityDevice.
   */
  CUPTI_ACTIVITY_KIND_DEVICE   = 8,
  /**
   * Information about a context. The corresponding activity record
   * structure is \ref CUpti_ActivityContext.
   */
  CUPTI_ACTIVITY_KIND_CONTEXT  = 9,
  /**
   * A (potentially concurrent) kernel executing on the GPU. The
   * corresponding activity record structure is \ref
   * CUpti_ActivityKernel3.
   */
  CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL = 10,
  /**
   * Thread, device, context, etc. name. The corresponding activity
   * record structure is \ref CUpti_ActivityName.
   */
  CUPTI_ACTIVITY_KIND_NAME     = 11,
  /**
   * Instantaneous, start, or end marker. The corresponding activity
   * record structure is \ref CUpti_ActivityMarker.
   */
  CUPTI_ACTIVITY_KIND_MARKER = 12,
  /**
   * Extended, optional, data about a marker. The corresponding
   * activity record structure is \ref CUpti_ActivityMarkerData.
   */
  CUPTI_ACTIVITY_KIND_MARKER_DATA = 13,
  /**
   * Source information about source level result. The corresponding
   * activity record structure is \ref CUpti_ActivitySourceLocator.
   */
  CUPTI_ACTIVITY_KIND_SOURCE_LOCATOR = 14,
  /**
   * Results for source-level global acccess. The
   * corresponding activity record structure is \ref
   * CUpti_ActivityGlobalAccess2.
   */
  CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS = 15,
  /**
   * Results for source-level branch. The corresponding
   * activity record structure is \ref CUpti_ActivityBranch2.
   */
  CUPTI_ACTIVITY_KIND_BRANCH = 16,
  /**
   * Overhead activity records. The
   * corresponding activity record structure is
   * \ref CUpti_ActivityOverhead.
   */
  CUPTI_ACTIVITY_KIND_OVERHEAD = 17,
  /**
   * A CDP (CUDA Dynamic Parallel) kernel executing on the GPU. The
   * corresponding activity record structure is \ref
   * CUpti_ActivityCdpKernel.  This activity can not be directly
   * enabled or disabled. It is enabled and disabled through
   * concurrent kernel activity \ref
   * CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL
   */
  CUPTI_ACTIVITY_KIND_CDP_KERNEL = 18,
  /**
   * Preemption activity record indicating a preemption of a CDP (CUDA
   * Dynamic Parallel) kernel executing on the GPU. The corresponding
   * activity record structure is \ref CUpti_ActivityPreemption.
   */
  CUPTI_ACTIVITY_KIND_PREEMPTION = 19,
  /**
   * Environment activity records indicating power, clock, thermal,
   * etc. levels of the GPU. The corresponding activity record
   * structure is \ref CUpti_ActivityEnvironment.
   */
  CUPTI_ACTIVITY_KIND_ENVIRONMENT = 20,
  /**
   * An event value associated with a specific event domain
   * instance. The corresponding activity record structure is \ref
   * CUpti_ActivityEventInstance.
   */
  CUPTI_ACTIVITY_KIND_EVENT_INSTANCE = 21,
  /**
   * A peer to peer memory copy. The corresponding activity record
   * structure is \ref CUpti_ActivityMemcpy2.
   */
  CUPTI_ACTIVITY_KIND_MEMCPY2 = 22,
  /**
   * A metric value associated with a specific metric domain
   * instance. The corresponding activity record structure is \ref
   * CUpti_ActivityMetricInstance.
   */
  CUPTI_ACTIVITY_KIND_METRIC_INSTANCE = 23,
  /**
   * SASS/Source line-by-line correlation record.
   * The corresponding activity record structure is \ref
   * CUpti_ActivityInstructionExecution.
   */
  CUPTI_ACTIVITY_KIND_INSTRUCTION_EXECUTION = 24,
  /**
   * Unified Memory counter record. The corresponding activity
   * record structure is \ref CUpti_ActivityUnifiedMemoryCounter.
   */
  CUPTI_ACTIVITY_KIND_UNIFIED_MEMORY_COUNTER = 25,
  /**
   * Device global/function record. The corresponding activity
   * record structure is \ref CUpti_ActivityFunction.
   */
  CUPTI_ACTIVITY_KIND_FUNCTION = 26,
  /**
   * CUDA Module record. The corresponding activity
   * record structure is \ref CUpti_ActivityModule.
   */
  CUPTI_ACTIVITY_KIND_MODULE = 27,
  /**
   * A device attribute value. The corresponding activity record
   * structure is \ref CUpti_ActivityDeviceAttribute.
   */
  CUPTI_ACTIVITY_KIND_DEVICE_ATTRIBUTE   = 28,
  /**
   * Results for source-level shared acccess. The
   * corresponding activity record structure is \ref
   * CUpti_ActivitySharedAccess.
   */
  CUPTI_ACTIVITY_KIND_SHARED_ACCESS = 29,
  CUPTI_ACTIVITY_KIND_FORCE_INT     = 0x7fffffff
} CUpti_ActivityKind;

/**
 * \brief The kinds of activity objects.
 * \see CUpti_ActivityObjectKindId
 */
typedef enum {
  /**
   * The object kind is not known.
   */
  CUPTI_ACTIVITY_OBJECT_UNKNOWN  = 0,
  /**
   * A process.
   */
  CUPTI_ACTIVITY_OBJECT_PROCESS  = 1,
  /**
   * A thread.
   */
  CUPTI_ACTIVITY_OBJECT_THREAD   = 2,
  /**
   * A device.
   */
  CUPTI_ACTIVITY_OBJECT_DEVICE   = 3,
  /**
   * A context.
   */
  CUPTI_ACTIVITY_OBJECT_CONTEXT  = 4,
  /**
   * A stream.
   */
  CUPTI_ACTIVITY_OBJECT_STREAM   = 5,

  CUPTI_ACTIVITY_OBJECT_FORCE_INT = 0x7fffffff
} CUpti_ActivityObjectKind;

/**
 * \brief Identifiers for object kinds as specified by
 * CUpti_ActivityObjectKind.
 * \see CUpti_ActivityObjectKind
 */
  typedef union {
    /**
     * A process object requires that we identify the process ID. A
     * thread object requires that we identify both the process and
     * thread ID.
     */
    struct {
      uint32_t processId;
      uint32_t threadId;
    } pt;
    /**
     * A device object requires that we identify the device ID. A
     * context object requires that we identify both the device and
     * context ID. A stream object requires that we identify device,
     * context, and stream ID.
     */
    struct {
      uint32_t deviceId;
      uint32_t contextId;
      uint32_t streamId;
    } dcs;
} CUpti_ActivityObjectKindId;

/**
 * \brief The kinds of activity overhead.
 */
typedef enum {
  /**
   * The overhead kind is not known.
   */
  CUPTI_ACTIVITY_OVERHEAD_UNKNOWN               = 0,
  /**
   * Compiler(JIT) overhead.
   */
  CUPTI_ACTIVITY_OVERHEAD_DRIVER_COMPILER       = 1,
  /**
   * Activity buffer flush overhead.
   */
  CUPTI_ACTIVITY_OVERHEAD_CUPTI_BUFFER_FLUSH    = 1<<16,
  /**
   * CUPTI instrumentation overhead.
   */
  CUPTI_ACTIVITY_OVERHEAD_CUPTI_INSTRUMENTATION = 2<<16,
  /**
   * CUPTI resource creation and destruction overhead.
   */
  CUPTI_ACTIVITY_OVERHEAD_CUPTI_RESOURCE        = 3<<16,
  CUPTI_ACTIVITY_OVERHEAD_FORCE_INT             = 0x7fffffff
} CUpti_ActivityOverheadKind;

/**
 * \brief The kind of a compute API.
 */
typedef enum {
  /**
   * The compute API is not known.
   */
  CUPTI_ACTIVITY_COMPUTE_API_UNKNOWN    = 0,
  /**
   * The compute APIs are for CUDA.
   */
  CUPTI_ACTIVITY_COMPUTE_API_CUDA       = 1,
  /**
   * The compute APIs are for CUDA running
   * in MPS (Multi-Process Service) environment.
   */
  CUPTI_ACTIVITY_COMPUTE_API_CUDA_MPS   = 2,

  CUPTI_ACTIVITY_COMPUTE_API_FORCE_INT  = 0x7fffffff
} CUpti_ActivityComputeApiKind;

/**
 * \brief Flags associated with activity records.
 *
 * Activity record flags. Flags can be combined by bitwise OR to
 * associated multiple flags with an activity record. Each flag is
 * specific to a certain activity kind, as noted below.
 */
typedef enum {
  /**
   * Indicates the activity record has no flags.
   */
  CUPTI_ACTIVITY_FLAG_NONE          = 0,

  /**
   * Indicates the activity represents a device that supports
   * concurrent kernel execution. Valid for
   * CUPTI_ACTIVITY_KIND_DEVICE.
   */
  CUPTI_ACTIVITY_FLAG_DEVICE_CONCURRENT_KERNELS  = 1 << 0,

  /**
   * Indicates if the activity represents a CUdevice_attribute value
   * or a CUpti_DeviceAttribute value. Valid for
   * CUPTI_ACTIVITY_KIND_DEVICE_ATTRIBUTE.
   */
  CUPTI_ACTIVITY_FLAG_DEVICE_ATTRIBUTE_CUDEVICE  = 1 << 0,

  /**
   * Indicates the activity represents an asynchronous memcpy
   * operation. Valid for CUPTI_ACTIVITY_KIND_MEMCPY.
   */
  CUPTI_ACTIVITY_FLAG_MEMCPY_ASYNC  = 1 << 0,

  /**
   * Indicates the activity represents an instantaneous marker. Valid
   * for CUPTI_ACTIVITY_KIND_MARKER.
   */
  CUPTI_ACTIVITY_FLAG_MARKER_INSTANTANEOUS  = 1 << 0,

  /**
   * Indicates the activity represents a region start marker. Valid
   * for CUPTI_ACTIVITY_KIND_MARKER.
   */
  CUPTI_ACTIVITY_FLAG_MARKER_START  = 1 << 1,

  /**
   * Indicates the activity represents a region end marker. Valid for
   * CUPTI_ACTIVITY_KIND_MARKER.
   */
  CUPTI_ACTIVITY_FLAG_MARKER_END  = 1 << 2,

  /**
   * Indicates the activity represents a marker that does not specify
   * a color. Valid for CUPTI_ACTIVITY_KIND_MARKER_DATA.
   */
  CUPTI_ACTIVITY_FLAG_MARKER_COLOR_NONE  = 1 << 0,

  /**
   * Indicates the activity represents a marker that specifies a color
   * in alpha-red-green-blue format. Valid for
   * CUPTI_ACTIVITY_KIND_MARKER_DATA.
   */
  CUPTI_ACTIVITY_FLAG_MARKER_COLOR_ARGB  = 1 << 1,

  /**
   * The number of bytes requested by each thread
   * Valid for CUpti_ActivityGlobalAccess2.
   */
  CUPTI_ACTIVITY_FLAG_GLOBAL_ACCESS_KIND_SIZE_MASK  = 0xFF << 0,
  /**
   * If bit in this flag is set, the access was load, else it is a
   * store access. Valid for CUpti_ActivityGlobalAccess2.
   */
  CUPTI_ACTIVITY_FLAG_GLOBAL_ACCESS_KIND_LOAD       = 1 << 8,
  /**
   * If this bit in flag is set, the load access was cached else it is
   * uncached. Valid for CUpti_ActivityGlobalAccess2.
   */
  CUPTI_ACTIVITY_FLAG_GLOBAL_ACCESS_KIND_CACHED     = 1 << 9,
  /**
   * If this bit in flag is set, the metric value overflowed. Valid
   * for CUpti_ActivityMetric and CUpti_ActivityMetricInstance.
   */
  CUPTI_ACTIVITY_FLAG_METRIC_OVERFLOWED     = 1 << 0,
  /**
   * If this bit in flag is set, the metric value couldn't be
   * calculated. This occurs when a value(s) required to calculate the
   * metric is missing.  Valid for CUpti_ActivityMetric and
   * CUpti_ActivityMetricInstance.
   */
  CUPTI_ACTIVITY_FLAG_METRIC_VALUE_INVALID  = 1 << 1,
    /**
   * If this bit in flag is set, the source level metric value couldn't be
   * calculated. This occurs when a value(s) required to calculate the
   * source level metric cannot be evaluated.
   * Valid for CUpti_ActivityInstructionExecution.
   */
  CUPTI_ACTIVITY_FLAG_INSTRUCTION_VALUE_INVALID  = 1 << 0,
  /**
   * The mask for the instruction class, \ref CUpti_ActivityInstructionClass
   * Valid for CUpti_ActivityInstructionExecution.
   */
  CUPTI_ACTIVITY_FLAG_INSTRUCTION_CLASS_MASK    = 0xFF << 1,
  /**
   * When calling cuptiActivityFlushAll, this flag
   * can be set to force CUPTI to flush all records in the buffer, whether
   * finished or not
   */
  CUPTI_ACTIVITY_FLAG_FLUSH_FORCED = 1 << 0,

  /**
   * The number of bytes requested by each thread
   * Valid for CUpti_ActivitySharedAccess.
   */
  CUPTI_ACTIVITY_FLAG_SHARED_ACCESS_KIND_SIZE_MASK  = 0xFF << 0,
  /**
   * If bit in this flag is set, the access was load, else it is a
   * store access.  Valid for CUpti_ActivitySharedAccess.
   */
  CUPTI_ACTIVITY_FLAG_SHARED_ACCESS_KIND_LOAD       = 1 << 8,

  CUPTI_ACTIVITY_FLAG_FORCE_INT = 0x7fffffff
} CUpti_ActivityFlag;

/**
 * \brief The kind of a memory copy, indicating the source and
 * destination targets of the copy.
 *
 * Each kind represents the source and destination targets of a memory
 * copy. Targets are host, device, and array.
 */
typedef enum {
  /**
   * The memory copy kind is not known.
   */
  CUPTI_ACTIVITY_MEMCPY_KIND_UNKNOWN = 0,
  /**
   * A host to device memory copy.
   */
  CUPTI_ACTIVITY_MEMCPY_KIND_HTOD    = 1,
  /**
   * A device to host memory copy.
   */
  CUPTI_ACTIVITY_MEMCPY_KIND_DTOH    = 2,
  /**
   * A host to device array memory copy.
   */
  CUPTI_ACTIVITY_MEMCPY_KIND_HTOA    = 3,
  /**
   * A device array to host memory copy.
   */
  CUPTI_ACTIVITY_MEMCPY_KIND_ATOH    = 4,
  /**
   * A device array to device array memory copy.
   */
  CUPTI_ACTIVITY_MEMCPY_KIND_ATOA    = 5,
  /**
   * A device array to device memory copy.
   */
  CUPTI_ACTIVITY_MEMCPY_KIND_ATOD    = 6,
  /**
   * A device to device array memory copy.
   */
  CUPTI_ACTIVITY_MEMCPY_KIND_DTOA    = 7,
  /**
   * A device to device memory copy on the same device.
   */
  CUPTI_ACTIVITY_MEMCPY_KIND_DTOD    = 8,
  /**
   * A host to host memory copy.
   */
  CUPTI_ACTIVITY_MEMCPY_KIND_HTOH    = 9,
  /**
   * A peer to peer memory copy across different devices.
   */
  CUPTI_ACTIVITY_MEMCPY_KIND_PTOP    = 10,

  CUPTI_ACTIVITY_MEMCPY_KIND_FORCE_INT = 0x7fffffff
} CUpti_ActivityMemcpyKind;

/**
 * \brief The kinds of memory accessed by a memory copy.
 *
 * Each kind represents the type of the source or destination memory
 * accessed by a memory copy.
 */
typedef enum {
  /**
   * The source or destination memory kind is unknown.
   */
  CUPTI_ACTIVITY_MEMORY_KIND_UNKNOWN      = 0,
  /**
   * The source or destination memory is pageable.
   */
  CUPTI_ACTIVITY_MEMORY_KIND_PAGEABLE     = 1,
  /**
   * The source or destination memory is pinned.
   */
  CUPTI_ACTIVITY_MEMORY_KIND_PINNED       = 2,
  /**
   * The source or destination memory is on the device.
   */
  CUPTI_ACTIVITY_MEMORY_KIND_DEVICE       = 3,
  /**
   * The source or destination memory is an array.
   */
  CUPTI_ACTIVITY_MEMORY_KIND_ARRAY        = 4,
  CUPTI_ACTIVITY_MEMORY_KIND_FORCE_INT    = 0x7fffffff
} CUpti_ActivityMemoryKind;

/**
 * \brief The kind of a preemption activity.
 */
typedef enum {
  /**
   * The preemption kind is not known.
   */
  CUPTI_ACTIVITY_PREEMPTION_KIND_UNKNOWN    = 0,
  /**
   * Preemption to save CDP block.
   */
  CUPTI_ACTIVITY_PREEMPTION_KIND_SAVE       = 1,
  /**
   * Preemption to restore CDP block.
   */
  CUPTI_ACTIVITY_PREEMPTION_KIND_RESTORE    = 2,
  CUPTI_ACTIVITY_PREEMPTION_KIND_FORCE_INT  = 0x7fffffff
} CUpti_ActivityPreemptionKind;

/**
 * \brief The kind of environment data. Used to indicate what type of
 * data is being reported by an environment activity record.
 */
typedef enum {
  /**
   * Unknown data.
   */
  CUPTI_ACTIVITY_ENVIRONMENT_UNKNOWN = 0,
  /**
   * The environment data is related to speed.
   */
  CUPTI_ACTIVITY_ENVIRONMENT_SPEED = 1,
  /**
   * The environment data is related to temperature.
   */
  CUPTI_ACTIVITY_ENVIRONMENT_TEMPERATURE = 2,
  /**
   * The environment data is related to power.
   */
  CUPTI_ACTIVITY_ENVIRONMENT_POWER = 3,
  /**
   * The environment data is related to cooling.
   */
  CUPTI_ACTIVITY_ENVIRONMENT_COOLING = 4,

  CUPTI_ACTIVITY_ENVIRONMENT_COUNT,
  CUPTI_ACTIVITY_ENVIRONMENT_KIND_FORCE_INT    = 0x7fffffff
} CUpti_ActivityEnvironmentKind;

/**
 * \brief Reasons for clock throttling.
 *
 * The possible reasons that a clock can be throttled. There can be
 * more than one reason that a clock is being throttled so these types
 * can be combined by bitwise OR.  These are used in the
 * clocksThrottleReason field in the Environment Activity Record.
 */
typedef enum {
  /**
   * Nothing is running on the GPU and the clocks are dropping to idle
   * state.
   */
  CUPTI_CLOCKS_THROTTLE_REASON_GPU_IDLE              = 0x00000001,
  /**
   * The GPU clocks are limited by a user specified limit.
   */
  CUPTI_CLOCKS_THROTTLE_REASON_USER_DEFINED_CLOCKS   = 0x00000002,
  /**
   * A software power scaling algorithm is reducing the clocks below
   * requested clocks.
   */
  CUPTI_CLOCKS_THROTTLE_REASON_SW_POWER_CAP          = 0x00000004,
  /**
   * Hardware slowdown to reduce the clock by a factor of two or more
   * is engaged.  This is an indicator of one of the following: 1)
   * Temperature is too high, 2) External power brake assertion is
   * being triggered (e.g. by the system power supply), 3) Change in
   * power state.
   */
  CUPTI_CLOCKS_THROTTLE_REASON_HW_SLOWDOWN           = 0x00000008,
  /**
   * Some unspecified factor is reducing the clocks.
   */
  CUPTI_CLOCKS_THROTTLE_REASON_UNKNOWN               = 0x80000000,
  /**
   * Throttle reason is not supported for this GPU.
   */
  CUPTI_CLOCKS_THROTTLE_REASON_UNSUPPORTED           = 0x40000000,
  /**
   * No clock throttling.
   */
  CUPTI_CLOCKS_THROTTLE_REASON_NONE                  = 0x00000000,

  CUPTI_CLOCKS_THROTTLE_REASON_FORCE_INT             = 0x7fffffff
} CUpti_EnvironmentClocksThrottleReason;

/**
 * \brief Scope of the unified memory counter
 */
typedef enum {
  /**
   * The unified memory counter scope is not known.
   */
  CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_SCOPE_UNKNOWN = 0,
  /**
   * Collect unified memory counter for single process on one device
   */
  CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_SCOPE_PROCESS_SINGLE_DEVICE = 1,
  /**
   * Collect unified memory counter for single process across all devices
   */
  CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_SCOPE_PROCESS_ALL_DEVICES = 2,

  CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_SCOPE_COUNT,
  CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_SCOPE_FORCE_INT = 0x7fffffff
} CUpti_ActivityUnifiedMemoryCounterScope;

/**
 * \brief Kind of the Unified Memory counter
 *
 * Many activities are associated with Unified Memory mechanism; among them
 * are tranfer from host to device, device to host, page fault at
 * host side.
 */
typedef enum {
  /**
   * The unified memory counter kind is not known.
   */
  CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_KIND_UNKNOWN = 0,
  /**
   * Number of bytes transfered from host to device
   */
  CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_KIND_BYTES_TRANSFER_HTOD = 1,
  /**
   * Number of bytes transfered from device to host
   */
  CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_KIND_BYTES_TRANSFER_DTOH = 2,
  /**
   * Number of CPU page faults
   */
  CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_KIND_CPU_PAGE_FAULT_COUNT = 3,

  CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_KIND_COUNT,
  CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_KIND_FORCE_INT = 0x7fffffff
} CUpti_ActivityUnifiedMemoryCounterKind;

/**
 * \brief SASS instruction classification.
 *
 * The sass instruction are broadly divided into different class. Each enum represents a classification.
 */
typedef enum {
  /**
   * The instruction class is not known.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_UNKNOWN = 0,
  /**
   * Represents a 32 bit floating point operation.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_FP_32 = 1,
  /**
   * Represents a 64 bit floating point operation.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_FP_64 = 2,
  /**
   * Represents an integer operation.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_INTEGER = 3,
  /**
   * Represents a bit conversion operation.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_BIT_CONVERSION = 4,
  /**
   * Represents a control flow instruction.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_CONTROL_FLOW = 5,
  /**
   * Represents a global load-store instruction.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_GLOBAL = 6,
  /**
   * Represents a shared load-store instruction.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_SHARED = 7,
  /**
   * Represents a local load-store instruction.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_LOCAL = 8,
  /**
   * Represents a generic load-store instruction.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_GENERIC = 9,
  /**
   * Represents a surface load-store instruction.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_SURFACE = 10,
  /**
   * Represents a constant load instruction.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_CONSTANT = 11,
  /**
   * Represents a texture load-store instruction.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_TEXTURE = 12,
  /**
   * Represents a global atomic instruction.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_GLOBAL_ATOMIC = 13,
  /**
   * Represents a shared atomic instruction.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_SHARED_ATOMIC = 14,
  /**
   * Represents a surface atomic instruction.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_SURFACE_ATOMIC = 15,
  /**
   * Represents a inter-thread communication instruction.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_INTER_THREAD_COMMUNICATION = 16,
  /**
   * Represents a barrier instruction.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_BARRIER = 17,
  /**
   * Represents some miscellaneous instructions which do not fit in the above classification.
   */
  CUPTI_ACTIVITY_INSTRUCTION_CLASS_MISCELLANEOUS = 18,

  CUPTI_ACTIVITY_INSTRUCTION_CLASS_KIND_FORCE_INT     = 0x7fffffff
} CUpti_ActivityInstructionClass;

/**
 * \brief Partitioned global caching option
 */
typedef enum {
  /**
   * Partitioned global cache config unknown.
   */
  CUPTI_ACTIVITY_PARTITIONED_GLOBAL_CACHE_CONFIG_UNKNOWN = 0,
  /**
   * Partitioned global cache not supported.
   */
  CUPTI_ACTIVITY_PARTITIONED_GLOBAL_CACHE_CONFIG_NOT_SUPPORTED = 1,
  /**
   * Partitioned global cache config off.
   */
  CUPTI_ACTIVITY_PARTITIONED_GLOBAL_CACHE_CONFIG_OFF = 2,
  /**
   * Partitioned global cache config on.
   */
  CUPTI_ACTIVITY_PARTITIONED_GLOBAL_CACHE_CONFIG_ON = 3,
  CUPTI_ACTIVITY_PARTITIONED_GLOBAL_CACHE_CONFIG_FORCE_INT  = 0x7fffffff
} CUpti_ActivityPartitionedGlobalCacheConfig;

/**
 * The source-locator ID that indicates an unknown source
 * location. There is not an actual CUpti_ActivitySourceLocator object
 * corresponding to this value.
 */
#define CUPTI_SOURCE_LOCATOR_ID_UNKNOWN 0

/**
 * An invalid/unknown correlation ID. A correlation ID of this value
 * indicates that there is no correlation for the activity record.
 */
#define CUPTI_CORRELATION_ID_UNKNOWN 0

/**
 * An invalid/unknown grid ID.
 */
#define CUPTI_GRID_ID_UNKNOWN 0LL

/**
 * An invalid/unknown timestamp for a start, end, queued, submitted,
 * or completed time.
 */
#define CUPTI_TIMESTAMP_UNKNOWN 0LL

/**
 * An invalid/unknown process id.
 */
#define CUPTI_AUTO_BOOST_INVALID_CLIENT_PID 0

START_PACKED_ALIGNMENT
/**
 * \brief Unified Memory counters configuration structure
 *
 * This structure controls the enable/disable of the various
 * Unified Memory counters consisting of scope, kind and other parameters.
 * See function /ref cuptiActivityConfigureUnifiedMemoryCounter
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * Unified Memory counter Counter scope
   */
  CUpti_ActivityUnifiedMemoryCounterScope scope;

  /**
   * Unified Memory counter Counter kind
   */
  CUpti_ActivityUnifiedMemoryCounterKind kind;

  /**
   * Device id of the traget device. This is relevant only
   * for single device scopes.
   */
  uint32_t deviceId;

  /**
   * Control to enable/disable the counter. To enable the counter
   * set it to non-zero value while disable is indicated by zero.
   */
  uint32_t enable;
} CUpti_ActivityUnifiedMemoryCounterConfig;

/**
 * \brief Device auto boost state structure
 *
 * This structure defines auto boost state for a device.
 * See function /ref cuptiGetAutoBoostState
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * Returned auto boost state. 1 is returned in case auto boost is enabled, 0
   * otherwise
   */
  uint32_t enabled;

  /**
   * Id of process that has set the current boost state. The value will be
   * CUPTI_AUTO_BOOST_INVALID_CLIENT_PID if the user does not have the
   * permission to query process ids or there is an error in querying the
   * process id.
   */
  uint32_t pid;

} CUpti_ActivityAutoBoostState;

/**
 * \brief The base activity record.
 *
 * The activity API uses a CUpti_Activity as a generic representation
 * for any activity. The 'kind' field is used to determine the
 * specific activity kind, and from that the CUpti_Activity object can
 * be cast to the specific activity record type appropriate for that kind.
 *
 * Note that all activity record types are padded and aligned to
 * ensure that each member of the record is naturally aligned.
 *
 * \see CUpti_ActivityKind
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The kind of this activity.
   */
  CUpti_ActivityKind kind;
} CUpti_Activity;

/**
 * \brief The activity record for memory copies.
 *
 * This activity record represents a memory copy
 * (CUPTI_ACTIVITY_KIND_MEMCPY).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_MEMCPY.
   */
  CUpti_ActivityKind kind;

  /**
   * The kind of the memory copy, stored as a byte to reduce record
   * size.  \see CUpti_ActivityMemcpyKind
   */
  /*CUpti_ActivityMemcpyKind*/ uint8_t copyKind;

  /**
   * The source memory kind read by the memory copy, stored as a byte
   * to reduce record size.  \see CUpti_ActivityMemoryKind
   */
  /*CUpti_ActivityMemoryKind*/ uint8_t srcKind;

  /**
   * The destination memory kind read by the memory copy, stored as a
   * byte to reduce record size.  \see CUpti_ActivityMemoryKind
   */
  /*CUpti_ActivityMemoryKind*/ uint8_t dstKind;

  /**
   * The flags associated with the memory copy. \see CUpti_ActivityFlag
   */
  uint8_t flags;

  /**
   * The number of bytes transferred by the memory copy.
   */
  uint64_t bytes;

  /**
   * The start timestamp for the memory copy, in ns. A value of 0 for
   * both the start and end timestamps indicates that timestamp
   * information could not be collected for the memory copy.
   */
  uint64_t start;

  /**
   * The end timestamp for the memory copy, in ns. A value of 0 for
   * both the start and end timestamps indicates that timestamp
   * information could not be collected for the memory copy.
   */
  uint64_t end;

  /**
   * The ID of the device where the memory copy is occurring.
   */
  uint32_t deviceId;

  /**
   * The ID of the context where the memory copy is occurring.
   */
  uint32_t contextId;

  /**
   * The ID of the stream where the memory copy is occurring.
   */
  uint32_t streamId;

  /**
   * The correlation ID of the memory copy. Each memory copy is
   * assigned a unique correlation ID that is identical to the
   * correlation ID in the driver API activity record that launched
   * the memory copy.
   */
  uint32_t correlationId;

  /**
   * The runtime correlation ID of the memory copy. Each memory copy
   * is assigned a unique runtime correlation ID that is identical to
   * the correlation ID in the runtime API activity record that
   * launched the memory copy.
   */
  uint32_t runtimeCorrelationId;

#ifdef CUPTILP64
  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
#endif

  /**
   * Undefined. Reserved for internal use.
   */
  void *reserved0;
} CUpti_ActivityMemcpy;

/**
 * \brief The activity record for peer-to-peer memory copies.
 *
 * This activity record represents a peer-to-peer memory copy
 * (CUPTI_ACTIVITY_KIND_MEMCPY2).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_MEMCPY2.
   */
  CUpti_ActivityKind kind;

  /**
   * The kind of the memory copy, stored as a byte to reduce record
   * size.  \see CUpti_ActivityMemcpyKind
   */
  uint8_t copyKind;

  /**
   * The source memory kind read by the memory copy, stored as a byte
   * to reduce record size.  \see CUpti_ActivityMemoryKind
   */
  uint8_t srcKind;

  /**
   * The destination memory kind read by the memory copy, stored as a
   * byte to reduce record size.  \see CUpti_ActivityMemoryKind
   */
  uint8_t dstKind;

  /**
   * The flags associated with the memory copy. \see
   * CUpti_ActivityFlag
   */
  uint8_t flags;

  /**
   * The number of bytes transferred by the memory copy.
   */
  uint64_t bytes;

  /**
   * The start timestamp for the memory copy, in ns. A value of 0 for
   * both the start and end timestamps indicates that timestamp
   * information could not be collected for the memory copy.
   */
  uint64_t start;

  /**
   * The end timestamp for the memory copy, in ns. A value of 0 for
   * both the start and end timestamps indicates that timestamp
   * information could not be collected for the memory copy.
   */
  uint64_t end;

  /**
  * The ID of the device where the memory copy is occurring.
  */
  uint32_t deviceId;

  /**
   * The ID of the context where the memory copy is occurring.
   */
  uint32_t contextId;

  /**
   * The ID of the stream where the memory copy is occurring.
   */
  uint32_t streamId;

  /**
   * The ID of the device where memory is being copied from.
   */
  uint32_t srcDeviceId;

  /**
   * The ID of the context owning the memory being copied from.
   */
  uint32_t srcContextId;

  /**
   * The ID of the device where memory is being copied to.
   */
  uint32_t dstDeviceId;

  /**
   * The ID of the context owning the memory being copied to.
   */
  uint32_t dstContextId;

  /**
   * The correlation ID of the memory copy. Each memory copy is
   * assigned a unique correlation ID that is identical to the
   * correlation ID in the driver and runtime API activity record that
   * launched the memory copy.
   */
  uint32_t correlationId;

#ifndef CUPTILP64
  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
#endif

  /**
   * Undefined. Reserved for internal use.
   */
  void *reserved0;
} CUpti_ActivityMemcpy2;

/**
 * \brief The activity record for memset.
 *
 * This activity record represents a memory set operation
 * (CUPTI_ACTIVITY_KIND_MEMSET).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_MEMSET.
   */
  CUpti_ActivityKind kind;

  /**
   * The value being assigned to memory by the memory set.
   */
  uint32_t value;

  /**
   * The number of bytes being set by the memory set.
   */
  uint64_t bytes;

  /**
   * The start timestamp for the memory set, in ns. A value of 0 for
   * both the start and end timestamps indicates that timestamp
   * information could not be collected for the memory set.
   */
  uint64_t start;

  /**
   * The end timestamp for the memory set, in ns. A value of 0 for
   * both the start and end timestamps indicates that timestamp
   * information could not be collected for the memory set.
   */
  uint64_t end;

  /**
   * The ID of the device where the memory set is occurring.
   */
  uint32_t deviceId;

  /**
   * The ID of the context where the memory set is occurring.
   */
  uint32_t contextId;

  /**
   * The ID of the stream where the memory set is occurring.
   */
  uint32_t streamId;

  /**
   * The correlation ID of the memory set. Each memory set is assigned
   * a unique correlation ID that is identical to the correlation ID
   * in the driver API activity record that launched the memory set.
   */
  uint32_t correlationId;

  /**
   * The runtime correlation ID of the memory set. Each memory set
   * is assigned a unique runtime correlation ID that is identical to
   * the correlation ID in the runtime API activity record that
   * launched the memory set.
   */
  uint32_t runtimeCorrelationId;

#ifdef CUPTILP64
  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
#endif

  /**
   * Undefined. Reserved for internal use.
   */
  void *reserved0;
} CUpti_ActivityMemset;

/**
 * \brief The activity record for kernel. (deprecated)
 *
 * This activity record represents a kernel execution
 * (CUPTI_ACTIVITY_KIND_KERNEL and
 * CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL) but is no longer generated
 * by CUPTI. Kernel activities are now reported using the
 * CUpti_ActivityKernel3 activity record.
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_KERNEL
   * or CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL.
   */
  CUpti_ActivityKind kind;

  /**
   * The cache configuration requested by the kernel. The value is one
   * of the CUfunc_cache enumeration values from cuda.h.
   */
  uint8_t cacheConfigRequested;

  /**
   * The cache configuration used for the kernel. The value is one of
   * the CUfunc_cache enumeration values from cuda.h.
   */
  uint8_t cacheConfigExecuted;

  /**
   * The number of registers required for each thread executing the
   * kernel.
   */
  uint16_t registersPerThread;

  /**
   * The start timestamp for the kernel execution, in ns. A value of 0
   * for both the start and end timestamps indicates that timestamp
   * information could not be collected for the kernel.
   */
  uint64_t start;

  /**
   * The end timestamp for the kernel execution, in ns. A value of 0
   * for both the start and end timestamps indicates that timestamp
   * information could not be collected for the kernel.
   */
  uint64_t end;

  /**
   * The ID of the device where the kernel is executing.
   */
  uint32_t deviceId;

  /**
   * The ID of the context where the kernel is executing.
   */
  uint32_t contextId;

  /**
   * The ID of the stream where the kernel is executing.
   */
  uint32_t streamId;

  /**
   * The X-dimension grid size for the kernel.
   */
  int32_t gridX;

  /**
   * The Y-dimension grid size for the kernel.
   */
  int32_t gridY;

  /**
   * The Z-dimension grid size for the kernel.
   */
  int32_t gridZ;

  /**
   * The X-dimension block size for the kernel.
   */
  int32_t blockX;

  /**
   * The Y-dimension block size for the kernel.
   */
  int32_t blockY;

  /**
   * The Z-dimension grid size for the kernel.
   */
  int32_t blockZ;

  /**
   * The static shared memory allocated for the kernel, in bytes.
   */
  int32_t staticSharedMemory;

  /**
   * The dynamic shared memory reserved for the kernel, in bytes.
   */
  int32_t dynamicSharedMemory;

  /**
   * The amount of local memory reserved for each thread, in bytes.
   */
  uint32_t localMemoryPerThread;

  /**
   * The total amount of local memory reserved for the kernel, in
   * bytes.
   */
  uint32_t localMemoryTotal;

  /**
   * The correlation ID of the kernel. Each kernel execution is
   * assigned a unique correlation ID that is identical to the
   * correlation ID in the driver API activity record that launched
   * the kernel.
   */
  uint32_t correlationId;

  /**
   * The runtime correlation ID of the kernel. Each kernel execution
   * is assigned a unique runtime correlation ID that is identical to
   * the correlation ID in the runtime API activity record that
   * launched the kernel.
   */
  uint32_t runtimeCorrelationId;

  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;

  /**
   * The name of the kernel. This name is shared across all activity
   * records representing the same kernel, and so should not be
   * modified.
   */
  const char *name;

  /**
   * Undefined. Reserved for internal use.
   */
  void *reserved0;
} CUpti_ActivityKernel;

/**
 * \brief The activity record for kernel. (deprecated)
 *
 * This activity record represents a kernel execution
 * (CUPTI_ACTIVITY_KIND_KERNEL and
 * CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL) but is no longer generated
 * by CUPTI. Kernel activities are now reported using the
 * CUpti_ActivityKernel3 activity record.
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_KERNEL or
   * CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL.
   */
  CUpti_ActivityKind kind;

  union {
    uint8_t both;
    struct {
      /**
       * The cache configuration requested by the kernel. The value is one
       * of the CUfunc_cache enumeration values from cuda.h.
       */
      uint8_t requested:4;
      /**
       * The cache configuration used for the kernel. The value is one of
       * the CUfunc_cache enumeration values from cuda.h.
       */
      uint8_t executed:4;
    } config;
  } cacheConfig;

  /**
   * The shared memory configuration used for the kernel. The value is one of
   * the CUsharedconfig enumeration values from cuda.h.
   */
  uint8_t sharedMemoryConfig;

  /**
   * The number of registers required for each thread executing the
   * kernel.
   */
  uint16_t registersPerThread;

  /**
   * The start timestamp for the kernel execution, in ns. A value of 0
   * for both the start and end timestamps indicates that timestamp
   * information could not be collected for the kernel.
   */
  uint64_t start;

  /**
   * The end timestamp for the kernel execution, in ns. A value of 0
   * for both the start and end timestamps indicates that timestamp
   * information could not be collected for the kernel.
   */
  uint64_t end;

  /**
   * The completed timestamp for the kernel execution, in ns.  It
   * represents the completion of all it's child kernels and the
   * kernel itself. A value of CUPTI_TIMESTAMP_UNKNOWN indicates that
   * the completion time is unknown.
   */
  uint64_t completed;

  /**
   * The ID of the device where the kernel is executing.
   */
  uint32_t deviceId;

  /**
   * The ID of the context where the kernel is executing.
   */
  uint32_t contextId;

  /**
   * The ID of the stream where the kernel is executing.
   */
  uint32_t streamId;

  /**
   * The X-dimension grid size for the kernel.
   */
  int32_t gridX;

  /**
   * The Y-dimension grid size for the kernel.
   */
  int32_t gridY;

  /**
   * The Z-dimension grid size for the kernel.
   */
  int32_t gridZ;

  /**
   * The X-dimension block size for the kernel.
   */
  int32_t blockX;

  /**
   * The Y-dimension block size for the kernel.
   */
  int32_t blockY;

  /**
   * The Z-dimension grid size for the kernel.
   */
  int32_t blockZ;

  /**
   * The static shared memory allocated for the kernel, in bytes.
   */
  int32_t staticSharedMemory;

  /**
   * The dynamic shared memory reserved for the kernel, in bytes.
   */
  int32_t dynamicSharedMemory;

  /**
   * The amount of local memory reserved for each thread, in bytes.
   */
  uint32_t localMemoryPerThread;

  /**
   * The total amount of local memory reserved for the kernel, in
   * bytes.
   */
  uint32_t localMemoryTotal;

  /**
   * The correlation ID of the kernel. Each kernel execution is
   * assigned a unique correlation ID that is identical to the
   * correlation ID in the driver or runtime API activity record that
   * launched the kernel.
   */
  uint32_t correlationId;

  /**
   * The grid ID of the kernel. Each kernel is assigned a unique
   * grid ID at runtime.
   */
  int64_t gridId;

  /**
   * The name of the kernel. This name is shared across all activity
   * records representing the same kernel, and so should not be
   * modified.
   */
  const char *name;

  /**
   * Undefined. Reserved for internal use.
   */
  void *reserved0;
} CUpti_ActivityKernel2;

/**
 * \brief The activity record for a kernel (CUDA 6.5(with sm_52 support) onwards).
 *
 * This activity record represents a kernel execution
 * (CUPTI_ACTIVITY_KIND_KERNEL and
 * CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_KERNEL or
   * CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL.
   */
  CUpti_ActivityKind kind;

  union {
    uint8_t both;
    struct {
      /**
       * The cache configuration requested by the kernel. The value is one
       * of the CUfunc_cache enumeration values from cuda.h.
       */
      uint8_t requested:4;
      /**
       * The cache configuration used for the kernel. The value is one of
       * the CUfunc_cache enumeration values from cuda.h.
       */
      uint8_t executed:4;
    } config;
  } cacheConfig;

  /**
   * The shared memory configuration used for the kernel. The value is one of
   * the CUsharedconfig enumeration values from cuda.h.
   */
  uint8_t sharedMemoryConfig;

  /**
   * The number of registers required for each thread executing the
   * kernel.
   */
  uint16_t registersPerThread;

  /**
   * The partitioned global caching requested for the kernel. Partitioned
   * global caching is required to enable caching on certain chips, such as
   * devices with compute capability 5.2.
   */
  CUpti_ActivityPartitionedGlobalCacheConfig partitionedGlobalCacheRequested;

  /**
   * The partitioned global caching executed for the kernel. Partitioned
   * global caching is required to enable caching on certain chips, such as
   * devices with compute capability 5.2. Partitioned global caching can be
   * automatically disabled if the occupancy requirement of the launch cannot
   * support caching.
   */
  CUpti_ActivityPartitionedGlobalCacheConfig partitionedGlobalCacheExecuted;

  /**
   * The start timestamp for the kernel execution, in ns. A value of 0
   * for both the start and end timestamps indicates that timestamp
   * information could not be collected for the kernel.
   */
  uint64_t start;

  /**
   * The end timestamp for the kernel execution, in ns. A value of 0
   * for both the start and end timestamps indicates that timestamp
   * information could not be collected for the kernel.
   */
  uint64_t end;

  /**
   * The completed timestamp for the kernel execution, in ns.  It
   * represents the completion of all it's child kernels and the
   * kernel itself. A value of CUPTI_TIMESTAMP_UNKNOWN indicates that
   * the completion time is unknown.
   */
  uint64_t completed;

  /**
   * The ID of the device where the kernel is executing.
   */
  uint32_t deviceId;

  /**
   * The ID of the context where the kernel is executing.
   */
  uint32_t contextId;

  /**
   * The ID of the stream where the kernel is executing.
   */
  uint32_t streamId;

  /**
   * The X-dimension grid size for the kernel.
   */
  int32_t gridX;

  /**
   * The Y-dimension grid size for the kernel.
   */
  int32_t gridY;

  /**
   * The Z-dimension grid size for the kernel.
   */
  int32_t gridZ;

  /**
   * The X-dimension block size for the kernel.
   */
  int32_t blockX;

  /**
   * The Y-dimension block size for the kernel.
   */
  int32_t blockY;

  /**
   * The Z-dimension grid size for the kernel.
   */
  int32_t blockZ;

  /**
   * The static shared memory allocated for the kernel, in bytes.
   */
  int32_t staticSharedMemory;

  /**
   * The dynamic shared memory reserved for the kernel, in bytes.
   */
  int32_t dynamicSharedMemory;

  /**
   * The amount of local memory reserved for each thread, in bytes.
   */
  uint32_t localMemoryPerThread;

  /**
   * The total amount of local memory reserved for the kernel, in
   * bytes.
   */
  uint32_t localMemoryTotal;

  /**
   * The correlation ID of the kernel. Each kernel execution is
   * assigned a unique correlation ID that is identical to the
   * correlation ID in the driver or runtime API activity record that
   * launched the kernel.
   */
  uint32_t correlationId;

  /**
   * The grid ID of the kernel. Each kernel is assigned a unique
   * grid ID at runtime.
   */
  int64_t gridId;

  /**
   * The name of the kernel. This name is shared across all activity
   * records representing the same kernel, and so should not be
   * modified.
   */
  const char *name;

  /**
   * Undefined. Reserved for internal use.
   */
  void *reserved0;
} CUpti_ActivityKernel3;

/**
 * \brief The activity record for CDP (CUDA Dynamic Parallelism)
 * kernel.
 *
 * This activity record represents a CDP kernel execution.
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_CDP_KERNEL
   */
  CUpti_ActivityKind kind;

  union {
    uint8_t both;
    struct {
      /**
       * The cache configuration requested by the kernel. The value is one
       * of the CUfunc_cache enumeration values from cuda.h.
       */
      uint8_t requested:4;
      /**
       * The cache configuration used for the kernel. The value is one of
       * the CUfunc_cache enumeration values from cuda.h.
       */
      uint8_t executed:4;
    } config;
  } cacheConfig;

  /**
   * The shared memory configuration used for the kernel. The value is one of
   * the CUsharedconfig enumeration values from cuda.h.
   */
  uint8_t sharedMemoryConfig;

  /**
   * The number of registers required for each thread executing the
   * kernel.
   */
  uint16_t registersPerThread;

  /**
   * The start timestamp for the kernel execution, in ns. A value of 0
   * for both the start and end timestamps indicates that timestamp
   * information could not be collected for the kernel.
   */
  uint64_t start;

  /**
   * The end timestamp for the kernel execution, in ns. A value of 0
   * for both the start and end timestamps indicates that timestamp
   * information could not be collected for the kernel.
   */
  uint64_t end;

  /**
   * The ID of the device where the kernel is executing.
   */
  uint32_t deviceId;

  /**
   * The ID of the context where the kernel is executing.
   */
  uint32_t contextId;

  /**
   * The ID of the stream where the kernel is executing.
   */
  uint32_t streamId;

  /**
   * The X-dimension grid size for the kernel.
   */
  int32_t gridX;

  /**
   * The Y-dimension grid size for the kernel.
   */
  int32_t gridY;

  /**
   * The Z-dimension grid size for the kernel.
   */
  int32_t gridZ;

  /**
   * The X-dimension block size for the kernel.
   */
  int32_t blockX;

  /**
   * The Y-dimension block size for the kernel.
   */
  int32_t blockY;

  /**
   * The Z-dimension grid size for the kernel.
   */
  int32_t blockZ;

  /**
   * The static shared memory allocated for the kernel, in bytes.
   */
  int32_t staticSharedMemory;

  /**
   * The dynamic shared memory reserved for the kernel, in bytes.
   */
  int32_t dynamicSharedMemory;

  /**
   * The amount of local memory reserved for each thread, in bytes.
   */
  uint32_t localMemoryPerThread;

  /**
   * The total amount of local memory reserved for the kernel, in
   * bytes.
   */
  uint32_t localMemoryTotal;

  /**
   * The correlation ID of the kernel. Each kernel execution is
   * assigned a unique correlation ID that is identical to the
   * correlation ID in the driver API activity record that launched
   * the kernel.
   */
  uint32_t correlationId;

  /**
   * The grid ID of the kernel. Each kernel execution
   * is assigned a unique grid ID.
   */
  int64_t gridId;

  /**
   * The grid ID of the parent kernel.
   */
  int64_t parentGridId;

  /**
   * The timestamp when kernel is queued up, in ns. A value of
   * CUPTI_TIMESTAMP_UNKNOWN indicates that the queued time is
   * unknown.
   */
  uint64_t queued;

  /**
   * The timestamp when kernel is submitted to the gpu, in ns. A value
   * of CUPTI_TIMESTAMP_UNKNOWN indicates that the submission time is
   * unknown.
   */
  uint64_t submitted;

  /**
   * The timestamp when kernel is marked as completed, in ns. A value
   * of CUPTI_TIMESTAMP_UNKNOWN indicates that the completion time is
   * unknown.
   */
  uint64_t completed;

  /**
   * The X-dimension of the parent block.
   */
  uint32_t parentBlockX;

  /**
   * The Y-dimension of the parent block.
   */
  uint32_t parentBlockY;

  /**
   * The Z-dimension of the parent block.
   */
  uint32_t parentBlockZ;

#ifdef CUPTILP64
  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
#endif

  /**
   * The name of the kernel. This name is shared across all activity
   * records representing the same kernel, and so should not be
   * modified.
   */
  const char *name;
} CUpti_ActivityCdpKernel;

/**
 * \brief The activity record for a preemption of a CDP kernel.
 *
 * This activity record represents a preemption of a CDP kernel.
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_PREEMPTION
   */
  CUpti_ActivityKind kind;

  /**
  * kind of the preemption
  */
  CUpti_ActivityPreemptionKind preemptionKind;

  /**
   * The timestamp of the preemption, in ns. A value of 0 indicates
   * that timestamp information could not be collected for the
   * preemption.
   */
  uint64_t timestamp;

  /**
  * The grid-id of the block that is preempted
  */
  int64_t gridId;

  /**
   * The X-dimension of the block that is preempted
   */
  uint32_t blockX;

  /**
   * The Y-dimension of the block that is preempted
   */
  uint32_t blockY;

  /**
   * The Z-dimension of the block that is preempted
   */
  uint32_t blockZ;

  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
} CUpti_ActivityPreemption;

/**
 * \brief The activity record for a driver or runtime API invocation.
 *
 * This activity record represents an invocation of a driver or
 * runtime API (CUPTI_ACTIVITY_KIND_DRIVER and
 * CUPTI_ACTIVITY_KIND_RUNTIME).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_DRIVER or
   * CUPTI_ACTIVITY_KIND_RUNTIME.
   */
  CUpti_ActivityKind kind;

  /**
   * The ID of the driver or runtime function.
   */
  CUpti_CallbackId cbid;

  /**
   * The start timestamp for the function, in ns. A value of 0 for
   * both the start and end timestamps indicates that timestamp
   * information could not be collected for the function.
   */
  uint64_t start;

  /**
   * The end timestamp for the function, in ns. A value of 0 for both
   * the start and end timestamps indicates that timestamp information
   * could not be collected for the function.
   */
  uint64_t end;

  /**
   * The ID of the process where the driver or runtime CUDA function
   * is executing.
   */
  uint32_t processId;

  /**
   * The ID of the thread where the driver or runtime CUDA function is
   * executing.
   */
  uint32_t threadId;

  /**
   * The correlation ID of the driver or runtime CUDA function. Each
   * function invocation is assigned a unique correlation ID that is
   * identical to the correlation ID in the memcpy, memset, or kernel
   * activity record that is associated with this function.
   */
  uint32_t correlationId;

  /**
   * The return value for the function. For a CUDA driver function
   * with will be a CUresult value, and for a CUDA runtime function
   * this will be a cudaError_t value.
   */
  uint32_t returnValue;
} CUpti_ActivityAPI;

/**
 * \brief The activity record for a CUPTI event.
 *
 * This activity record represents a CUPTI event value
 * (CUPTI_ACTIVITY_KIND_EVENT). This activity record kind is not
 * produced by the activity API but is included for completeness and
 * ease-of-use. Profile frameworks built on top of CUPTI that collect
 * event data may choose to use this type to store the collected event
 * data.
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_EVENT.
   */
  CUpti_ActivityKind kind;

  /**
   * The event ID.
   */
  CUpti_EventID id;

  /**
   * The event value.
   */
  uint64_t value;

  /**
   * The event domain ID.
   */
  CUpti_EventDomainID domain;

  /**
   * The correlation ID of the event. Use of this ID is user-defined,
   * but typically this ID value will equal the correlation ID of the
   * kernel for which the event was gathered.
   */
  uint32_t correlationId;
} CUpti_ActivityEvent;

/**
 * \brief The activity record for a CUPTI event with instance
 * information.
 *
 * This activity record represents the a CUPTI event value for a
 * specific event domain instance
 * (CUPTI_ACTIVITY_KIND_EVENT_INSTANCE). This activity record kind is
 * not produced by the activity API but is included for completeness
 * and ease-of-use. Profile frameworks built on top of CUPTI that
 * collect event data may choose to use this type to store the
 * collected event data. This activity record should be used when
 * event domain instance information needs to be associated with the
 * event.
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be
   * CUPTI_ACTIVITY_KIND_EVENT_INSTANCE.
   */
  CUpti_ActivityKind kind;

  /**
   * The event ID.
   */
  CUpti_EventID id;

  /**
   * The event domain ID.
   */
  CUpti_EventDomainID domain;

  /**
   * The event domain instance.
   */
  uint32_t instance;

  /**
   * The event value.
   */
  uint64_t value;

  /**
   * The correlation ID of the event. Use of this ID is user-defined,
   * but typically this ID value will equal the correlation ID of the
   * kernel for which the event was gathered.
   */
  uint32_t correlationId;

  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
} CUpti_ActivityEventInstance;

/**
 * \brief The activity record for a CUPTI metric.
 *
 * This activity record represents the collection of a CUPTI metric
 * value (CUPTI_ACTIVITY_KIND_METRIC). This activity record kind is not
 * produced by the activity API but is included for completeness and
 * ease-of-use. Profile frameworks built on top of CUPTI that collect
 * metric data may choose to use this type to store the collected metric
 * data.
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_METRIC.
   */
  CUpti_ActivityKind kind;

  /**
   * The metric ID.
   */
  CUpti_MetricID id;

  /**
   * The metric value.
   */
  CUpti_MetricValue value;

  /**
   * The correlation ID of the metric. Use of this ID is user-defined,
   * but typically this ID value will equal the correlation ID of the
   * kernel for which the metric was gathered.
   */
  uint32_t correlationId;

  /**
   * The properties of this metric. \see CUpti_ActivityFlag
   */
  uint8_t flags;

  /**
   * Undefined. Reserved for internal use.
   */
  uint8_t pad[3];
} CUpti_ActivityMetric;

/**
 * \brief The activity record for a CUPTI metric with instance
 * information.  This activity record represents a CUPTI metric value
 * for a specific metric domain instance
 * (CUPTI_ACTIVITY_KIND_METRIC_INSTANCE).  This activity record kind
 * is not produced by the activity API but is included for
 * completeness and ease-of-use. Profile frameworks built on top of
 * CUPTI that collect metric data may choose to use this type to store
 * the collected metric data. This activity record should be used when
 * metric domain instance information needs to be associated with the
 * metric.
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be
   * CUPTI_ACTIVITY_KIND_METRIC_INSTANCE.
   */
  CUpti_ActivityKind kind;

  /**
   * The metric ID.
   */
  CUpti_MetricID id;

  /**
   * The metric value.
   */
  CUpti_MetricValue value;

  /**
   * The metric domain instance.
   */
  uint32_t instance;

  /**
   * The correlation ID of the metric. Use of this ID is user-defined,
   * but typically this ID value will equal the correlation ID of the
   * kernel for which the metric was gathered.
   */
  uint32_t correlationId;

  /**
   * The properties of this metric. \see CUpti_ActivityFlag
   */
  uint8_t flags;

  /**
   * Undefined. Reserved for internal use.
   */
  uint8_t pad[7];
} CUpti_ActivityMetricInstance;

/**
 * \brief The activity record for source locator.
 *
 * This activity record represents a source locator
 * (CUPTI_ACTIVITY_KIND_SOURCE_LOCATOR).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_SOURCE_LOCATOR.
   */
  CUpti_ActivityKind kind;

  /**
   * The ID for the source path, will be used in all the source level
   * results.
   */
  uint32_t id;

  /**
   * The line number in the source .
   */
  uint32_t lineNumber;

#ifdef CUPTILP64
  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
#endif

  /**
   * The path for the file.
   */
  const char *fileName;
} CUpti_ActivitySourceLocator;

/**
 * \brief The activity record for source-level global
 * access. (deprecated)
 *
 * This activity records the locations of the global
 * accesses in the source (CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS).
 * Global access activities are now reported using the
 * CUpti_ActivityGlobalAccess2 activity record.
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS.
   */
  CUpti_ActivityKind kind;

  /**
   * The properties of this global access.
   */
  CUpti_ActivityFlag flags;

  /**
   * The ID for source locator.
   */
  uint32_t sourceLocatorId;

  /**
   * The correlation ID of the kernel to which this result is associated.
   */
  uint32_t correlationId;

  /**
   * The pc offset for the access.
   */
  uint32_t pcOffset;

  /**
   * The number of times this instruction was executed
   */
  uint32_t executed;

  /**
   * This increments each time when this instruction is executed by number
   * of threads that executed this instruction with predicate and condition code evaluating to true.
   */
  uint64_t threadsExecuted;

  /**
   * The total number of 32 bytes transactions to L2 cache generated by this access
   */
  uint64_t l2_transactions;
} CUpti_ActivityGlobalAccess;

/**
 * \brief The activity record for source-level global
 * access.
 *
 * This activity records the locations of the global
 * accesses in the source (CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS.
   */
  CUpti_ActivityKind kind;

  /**
   * The properties of this global access.
   */
  CUpti_ActivityFlag flags;

  /**
   * The ID for source locator.
   */
  uint32_t sourceLocatorId;

  /**
   * The correlation ID of the kernel to which this result is associated.
   */
  uint32_t correlationId;

 /**
  * Correlation ID with global/device function name
  */
  uint32_t functionId;

  /**
   * The pc offset for the access.
   */
  uint32_t pcOffset;

  /**
   * This increments each time when this instruction is executed by number
   * of threads that executed this instruction with predicate and condition code evaluating to true.
   */
  uint64_t threadsExecuted;

  /**
   * The total number of 32 bytes transactions to L2 cache generated by this access
   */
  uint64_t l2_transactions;

  /**
   * The minimum number of L2 transactions possible based on the access pattern.
   */
  uint64_t theoreticalL2Transactions;

  /**
   * The number of times this instruction was executed
   */
  uint32_t executed;

  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
} CUpti_ActivityGlobalAccess2;

/**
 * \brief The activity record for source level result
 * branch. (deprecated)
 *
 * This activity record the locations of the branches in the
 * source (CUPTI_ACTIVITY_KIND_BRANCH).
 * Branch activities are now reported using the
 * CUpti_ActivityBranch2 activity record.
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_BRANCH.
   */
  CUpti_ActivityKind kind;

  /**
   * The ID for source locator.
   */
  uint32_t sourceLocatorId;

  /**
   * The correlation ID of the kernel to which this result is associated.
   */
  uint32_t correlationId;

  /**
   * The pc offset for the branch.
   */
  uint32_t pcOffset;

  /**
   * The number of times this branch was executed
   */
  uint32_t executed;

  /**
   * Number of times this branch diverged
   */
  uint32_t diverged;

  /**
   * This increments each time when this instruction is executed by number
   * of threads that executed this instruction
   */
  uint64_t threadsExecuted;
} CUpti_ActivityBranch;

/**
 * \brief The activity record for source level result
 * branch.
 *
 * This activity record the locations of the branches in the
 * source (CUPTI_ACTIVITY_KIND_BRANCH).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_BRANCH.
   */
  CUpti_ActivityKind kind;

  /**
   * The ID for source locator.
   */
  uint32_t sourceLocatorId;

  /**
   * The correlation ID of the kernel to which this result is associated.
   */
  uint32_t correlationId;

 /**
  * Correlation ID with global/device function name
  */
  uint32_t functionId;

  /**
   * The pc offset for the branch.
   */
  uint32_t pcOffset;

  /**
   * Number of times this branch diverged
   */
  uint32_t diverged;

  /**
   * This increments each time when this instruction is executed by number
   * of threads that executed this instruction
   */
  uint64_t threadsExecuted;

  /**
   * The number of times this branch was executed
   */
  uint32_t executed;

  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
} CUpti_ActivityBranch2;

/**
 * \brief The activity record for a device.
 *
 * This activity record represents information about a GPU device
 * (CUPTI_ACTIVITY_KIND_DEVICE).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_DEVICE.
   */
  CUpti_ActivityKind kind;

  /**
   * The flags associated with the device. \see CUpti_ActivityFlag
   */
  CUpti_ActivityFlag flags;

  /**
   * The global memory bandwidth available on the device, in
   * kBytes/sec.
   */
  uint64_t globalMemoryBandwidth;

  /**
   * The amount of global memory on the device, in bytes.
   */
  uint64_t globalMemorySize;

  /**
   * The amount of constant memory on the device, in bytes.
   */
  uint32_t constantMemorySize;

  /**
   * The size of the L2 cache on the device, in bytes.
   */
  uint32_t l2CacheSize;

  /**
   * The number of threads per warp on the device.
   */
  uint32_t numThreadsPerWarp;

  /**
   * The core clock rate of the device, in kHz.
   */
  uint32_t coreClockRate;

  /**
   * Number of memory copy engines on the device.
   */
  uint32_t numMemcpyEngines;

  /**
   * Number of multiprocessors on the device.
   */
  uint32_t numMultiprocessors;

  /**
   * The maximum "instructions per cycle" possible on each device
   * multiprocessor.
   */
  uint32_t maxIPC;

  /**
   * Maximum number of warps that can be present on a multiprocessor
   * at any given time.
   */
  uint32_t maxWarpsPerMultiprocessor;

  /**
   * Maximum number of blocks that can be present on a multiprocessor
   * at any given time.
   */
  uint32_t maxBlocksPerMultiprocessor;

  /**
   * Maximum number of registers that can be allocated to a block.
   */
  uint32_t maxRegistersPerBlock;

  /**
   * Maximum amount of shared memory that can be assigned to a block,
   * in bytes.
   */
  uint32_t maxSharedMemoryPerBlock;

  /**
   * Maximum number of threads allowed in a block.
   */
  uint32_t maxThreadsPerBlock;

  /**
   * Maximum allowed X dimension for a block.
   */
  uint32_t maxBlockDimX;

  /**
   * Maximum allowed Y dimension for a block.
   */
  uint32_t maxBlockDimY;

  /**
   * Maximum allowed Z dimension for a block.
   */
  uint32_t maxBlockDimZ;

  /**
   * Maximum allowed X dimension for a grid.
   */
  uint32_t maxGridDimX;

  /**
   * Maximum allowed Y dimension for a grid.
   */
  uint32_t maxGridDimY;

  /**
   * Maximum allowed Z dimension for a grid.
   */
  uint32_t maxGridDimZ;

  /**
   * Compute capability for the device, major number.
   */
  uint32_t computeCapabilityMajor;

  /**
   * Compute capability for the device, minor number.
   */
  uint32_t computeCapabilityMinor;

  /**
   * The device ID.
   */
  uint32_t id;

#ifdef CUPTILP64
  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
#endif

  /**
   * The device name. This name is shared across all activity records
   * representing instances of the device, and so should not be
   * modified.
   */
  const char *name;
} CUpti_ActivityDevice;

/**
 * \brief The activity record for a device attribute.
 *
 * This activity record represents information about a GPU device:
 * either a CUpti_DeviceAttribute or CUdevice_attribute value
 * (CUPTI_ACTIVITY_KIND_DEVICE_ATTRIBUTE).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be
   * CUPTI_ACTIVITY_KIND_DEVICE_ATTRIBUTE.
   */
  CUpti_ActivityKind kind;

  /**
   * The flags associated with the device. \see CUpti_ActivityFlag
   */
  CUpti_ActivityFlag flags;

  /**
   * The ID of the device that this attribute applies to.
   */
  uint32_t deviceId;

  /**
   * The attribute, either a CUpti_DeviceAttribute or
   * CUdevice_attribute. Flag
   * CUPTI_ACTIVITY_FLAG_DEVICE_ATTRIBUTE_CUDEVICE is used to indicate
   * what kind of attribute this is. If
   * CUPTI_ACTIVITY_FLAG_DEVICE_ATTRIBUTE_CUDEVICE is 1 then
   * CUdevice_attribute field is value, otherwise
   * CUpti_DeviceAttribute field is valid.
   */
  union {
    CUdevice_attribute cu;
    CUpti_DeviceAttribute cupti;
  } attribute;

  /**
   * The value for the attribute. See CUpti_DeviceAttribute and
   * CUdevice_attribute for the type of the value for a given
   * attribute.
   */
  union {
    double vDouble;
    uint32_t vUint32;
    uint64_t vUint64;
    int32_t vInt32;
    int64_t vInt64;
  } value;
} CUpti_ActivityDeviceAttribute;

/**
 * \brief The activity record for a context.
 *
 * This activity record represents information about a context
 * (CUPTI_ACTIVITY_KIND_CONTEXT).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_CONTEXT.
   */
  CUpti_ActivityKind kind;

  /**
   * The context ID.
   */
  uint32_t contextId;

  /**
   * The device ID.
   */
  uint32_t deviceId;

  /**
   * The compute API kind. \see CUpti_ActivityComputeApiKind
   */
  uint16_t computeApiKind;

  /**
   * The ID for the NULL stream in this context
   */
  uint16_t nullStreamId;
} CUpti_ActivityContext;

/**
 * \brief The activity record providing a name.
 *
 * This activity record provides a name for a device, context, thread,
 * etc.  (CUPTI_ACTIVITY_KIND_NAME).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_NAME.
   */
  CUpti_ActivityKind kind;

  /**
   * The kind of activity object being named.
   */
  CUpti_ActivityObjectKind objectKind;

  /**
   * The identifier for the activity object. 'objectKind' indicates
   * which ID is valid for this record.
   */
  CUpti_ActivityObjectKindId objectId;

#ifdef CUPTILP64
  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
#endif

  /**
   * The name.
   */
  const char *name;

} CUpti_ActivityName;

/**
 * \brief The activity record providing a marker which is an
 * instantaneous point in time.
 *
 * The marker is specified with a descriptive name and unique id
 * (CUPTI_ACTIVITY_KIND_MARKER).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_MARKER.
   */
  CUpti_ActivityKind kind;

  /**
   * The flags associated with the marker. \see CUpti_ActivityFlag
   */
  CUpti_ActivityFlag flags;

  /**
   * The timestamp for the marker, in ns. A value of 0 indicates that
   * timestamp information could not be collected for the marker.
   */
  uint64_t timestamp;

  /**
   * The marker ID.
   */
  uint32_t id;

  /**
   * The kind of activity object associated with this marker.
   */
  CUpti_ActivityObjectKind objectKind;

  /**
   * The identifier for the activity object associated with this
   * marker. 'objectKind' indicates which ID is valid for this record.
   */
  CUpti_ActivityObjectKindId objectId;

#ifdef CUPTILP64
  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
#endif

  /**
   * The marker name for an instantaneous or start marker. This will
   * be NULL for an end marker.
   */
  const char *name;

} CUpti_ActivityMarker;

/**
 * \brief The activity record providing detailed information for a marker.
 *
 * The marker data contains color, payload, and category.
 * (CUPTI_ACTIVITY_KIND_MARKER_DATA).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be
   * CUPTI_ACTIVITY_KIND_MARKER_DATA.
   */
  CUpti_ActivityKind kind;

  /**
   * The flags associated with the marker. \see CUpti_ActivityFlag
   */
  CUpti_ActivityFlag flags;

  /**
   * The marker ID.
   */
  uint32_t id;

  /**
   * Defines the payload format for the value associated with the marker.
   */
  CUpti_MetricValueKind payloadKind;

  /**
   * The payload value.
   */
  CUpti_MetricValue payload;

  /**
   * The color for the marker.
   */
  uint32_t color;

  /**
   * The category for the marker.
   */
  uint32_t category;

} CUpti_ActivityMarkerData;

/**
 * \brief The activity record for CUPTI and driver overheads.
 *
 * This activity record provides CUPTI and driver overhead information
 * (CUPTI_ACTIVITY_OVERHEAD).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_OVERHEAD.
   */
  CUpti_ActivityKind kind;

  /**
   * The kind of overhead, CUPTI, DRIVER, COMPILER etc.
   */
  CUpti_ActivityOverheadKind overheadKind;

  /**
   * The kind of activity object that the overhead is associated with.
   */
  CUpti_ActivityObjectKind objectKind;

  /**
   * The identifier for the activity object. 'objectKind' indicates
   * which ID is valid for this record.
   */
  CUpti_ActivityObjectKindId objectId;

  /**
   * The start timestamp for the overhead, in ns. A value of 0 for
   * both the start and end timestamps indicates that timestamp
   * information could not be collected for the overhead.
   */
  uint64_t start;

  /**
   * The end timestamp for the overhead, in ns. A value of 0 for both
   * the start and end timestamps indicates that timestamp information
   * could not be collected for the overhead.
   */
  uint64_t end;
} CUpti_ActivityOverhead;

/**
 * \brief The activity record for CUPTI environmental data.
 *
 * This activity record provides CUPTI environmental data, include
 * power, clocks, and thermals.  This information is sampled at
 * various rates and returned in this activity record.  The consumer
 * of the record needs to check the environmentKind field to figure
 * out what kind of environmental record this is.
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_ENVIRONMENT.
   */
  CUpti_ActivityKind kind;

  /**
   * The ID of the device
   */
  uint32_t deviceId;

  /**
   * The timestamp when this sample was retrieved, in ns. A value of 0
   * indicates that timestamp information could not be collected for
   * the marker.
   */
  uint64_t timestamp;

  /**
   * The kind of data reported in this record.
   */
  CUpti_ActivityEnvironmentKind environmentKind;

  union {
    /**
     * Data returned for CUPTI_ACTIVITY_ENVIRONMENT_SPEED environment
     * kind.
     */
    struct {
      /**
       * The SM frequency in MHz
       */
      uint32_t smClock;

      /**
       * The memory frequency in MHz
       */
      uint32_t memoryClock;

      /**
       * The PCIe link generation.
       */
      uint32_t pcieLinkGen;

      /**
       * The PCIe link width.
       */
      uint32_t pcieLinkWidth;

      /**
       * The clocks throttle reasons.
       */
      CUpti_EnvironmentClocksThrottleReason clocksThrottleReasons;
    } speed;
    /**
     * Data returned for CUPTI_ACTIVITY_ENVIRONMENT_TEMPERATURE
     * environment kind.
     */
    struct {
      /**
       * The GPU temperature in degrees C.
       */
      uint32_t gpuTemperature;
    } temperature;
    /**
     * Data returned for CUPTI_ACTIVITY_ENVIRONMENT_POWER environment
     * kind.
     */
    struct {
      /**
       * The power in milliwatts consumed by GPU and associated
       * circuitry.
       */
      uint32_t power;

      /**
       * The power in milliwatts that will trigger power management
       * algorithm.
       */
      uint32_t powerLimit;
    } power;
    /**
     * Data returned for CUPTI_ACTIVITY_ENVIRONMENT_COOLING
     * environment kind.
     */
    struct {
      /**
       * The fan speed as percentage of maximum.
       */
      uint32_t fanSpeed;
    } cooling;
  } data;
} CUpti_ActivityEnvironment;

/**
 * \brief The activity record for source-level sass/source
 * line-by-line correlation.
 *
 * This activity records source level sass/source correlation
 * information.
 * (CUPTI_ACTIVITY_KIND_INSTRUCTION_EXECUTION).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_INSTRUCTION_EXECUTION.
   */
  CUpti_ActivityKind kind;

  /**
   * The properties of this instruction execution.
   * Check mask CUPTI_ACTIVITY_FLAG_INSTRUCTION_VALUE_INVALID to determine whether
   * threadsExecuted, notPredOffThreadsExecuted and executed are valid for the instruction.
   * Check mask CUPTI_ACTIVITY_FLAG_INSTRUCTION_CLASS_MASK to identify the instruction class.
   * See \ref CUpti_ActivityInstructionClass.
   */
  CUpti_ActivityFlag flags;

  /**
   * The ID for source locator.
   */
  uint32_t sourceLocatorId;

  /**
   * The correlation ID of the kernel to which this result is associated.
   */
  uint32_t correlationId;

 /**
  * Correlation ID with global/device function name
  */
  uint32_t functionId;

  /**
   * The pc offset for the instruction.
   */
  uint32_t pcOffset;

  /**
   * This increments each time when this instruction is executed by number
   * of threads that executed this instruction, regardless of predicate or condition code.
   */
  uint64_t threadsExecuted;

  /**
   * This increments each time when this instruction is executed by number
   * of threads that executed this instruction with predicate and condition code evaluating to true.
   */
  uint64_t notPredOffThreadsExecuted;

  /**
   * The number of times this instruction was executed.
   */
  uint32_t executed;

  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
} CUpti_ActivityInstructionExecution;

/**
 * \brief The activity record for Unified Memory counters
 *
 * This activity record represents a Unified Memory counter
 * (CUPTI_ACTIVITY_KIND_UNIFIED_MEMORY_COUNTER).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_UNIFIED_MEMORY_COUNTER
   */
  CUpti_ActivityKind kind;

  /**
   * The Unified Memory counter kind. See /ref CUpti_ActivityUnifiedMemoryCounterKind
   */
  CUpti_ActivityUnifiedMemoryCounterKind counterKind;

  /**
   * Scope of the Unified Memory counter. See /ref CUpti_ActivityUnifiedMemoryCounterScope
   */
  CUpti_ActivityUnifiedMemoryCounterScope scope;

  /**
   * The ID of the device involved in the memory transfer operation.
   * It is not relevant if the scope of the counter is global (all devices).
   */
  uint32_t deviceId;

  /**
   * Value of the counter
   *
   */
  uint64_t value;

  /**
   * The timestamp when this sample was retrieved, in ns. A value of 0
   * indicates that timestamp information could not be collected
   */
  uint64_t timestamp;

  /**
   * The ID of the process to which this record belongs to. In case of
   * global scope, processId is undefined.
   */
  uint32_t processId;

  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
} CUpti_ActivityUnifiedMemoryCounter;

/**
 * \brief The activity record for global/device functions.
 *
 * This activity records function name and corresponding module
 * information.
 * (CUPTI_ACTIVITY_KIND_FUNCTION).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_FUNCTION.
   */
  CUpti_ActivityKind kind;

  /**
  * ID to uniquely identify the record
  */
  uint32_t id;

  /**
   * The ID of the context where the function is launched.
   */
  uint32_t contextId;

  /**
   * The module ID in which this global/device function is present.
   */
  uint32_t moduleId;

  /**
   * The function's unique symbol index in the module.
   */
  uint32_t functionIndex;

#ifdef CUPTILP64
  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
#endif

  /**
   * The name of the function. This name is shared across all activity
   * records representing the same kernel, and so should not be
   * modified.
   */
  const char *name;
} CUpti_ActivityFunction;

/**
 * \brief The activity record for a CUDA module.
 *
 * This activity record represents a CUDA module
 * (CUPTI_ACTIVITY_KIND_MODULE). This activity record kind is not
 * produced by the activity API but is included for completeness and
 * ease-of-use. Profile frameworks built on top of CUPTI that collect
 * module data from the module callback may choose to use this type to
 * store the collected module data.
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_MODULE.
   */
  CUpti_ActivityKind kind;

  /**
   * The ID of the context where the module is loaded.
   */
  uint32_t contextId;

  /**
   * The module ID.
   */
  uint32_t id;

  /**
   * The cubin size.
   */
  uint32_t cubinSize;

#ifndef CUPTILP64
  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
#endif

  /**
   * The pointer to cubin.
   */
  const void *cubin;
} CUpti_ActivityModule;

/**
 * \brief The activity record for source-level shared
 * access.
 *
 * This activity records the locations of the shared
 * accesses in the source
 * (CUPTI_ACTIVITY_KIND_SHARED_ACCESS).
 */
typedef struct PACKED_ALIGNMENT {
  /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_SHARED_ACCESS.
   */
  CUpti_ActivityKind kind;

  /**
   * The properties of this shared access.
   */
  CUpti_ActivityFlag flags;

  /**
   * The ID for source locator.
   */
  uint32_t sourceLocatorId;

  /**
   * The correlation ID of the kernel to which this result is associated.
   */
  uint32_t correlationId;

 /**
  * Correlation ID with global/device function name
  */
  uint32_t functionId;

  /**
   * The pc offset for the access.
   */
  uint32_t pcOffset;

  /**
   * This increments each time when this instruction is executed by number
   * of threads that executed this instruction with predicate and condition code evaluating to true.
   */
  uint64_t threadsExecuted;

  /**
   * The total number of shared memory transactions generated by this access
   */
  uint64_t sharedTransactions;

  /**
   * The minimum number of shared memory transactions possible based on the access pattern.
   */
  uint64_t theoreticalSharedTransactions;

  /**
   * The number of times this instruction was executed
   */
  uint32_t executed;

  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad;
} CUpti_ActivitySharedAccess;

END_PACKED_ALIGNMENT

/**
 * \brief Activity attributes.
 *
 * These attributes are used to control the behavior of the activity
 * API.
 */
typedef enum {
    /**
     * The device memory size (in bytes) reserved for storing profiling
     * data for non-CDP operations for each buffer on a context. The
     * value is a size_t.
     *
     * Having larger buffer size means less flush operations but
     * consumes more device memory. Having smaller buffer size
     * increases the risk of dropping timestamps for kernel records
     * if too many kernels are launched/replayed at one time. This
     * value only applies to new buffer allocations.
     *
     * Set this value before initializing CUDA or before creating a
     * context to ensure it is considered for the following allocations.
     *
     * The default value is 4194304 (4MB).
     *
     * Note: The actual amount of device memory per buffer reserved by
     * CUPTI might be larger.
     */
    CUPTI_ACTIVITY_ATTR_DEVICE_BUFFER_SIZE          = 0,
    /**
     * The device memory size (in bytes) reserved for storing profiling
     * data for CDP operations for each buffer on a context. The
     * value is a size_t.
     *
     * Having larger buffer size means less flush operations but
     * consumes more device memory. This value only applies to new
     * allocations.
     *
     * Set this value before initializing CUDA or before creating a
     * context to ensure it is considered for the following allocations.
     *
     * The default value is 8388608 (8MB).
     *
     * Note: The actual amount of device memory per context reserved by
     * CUPTI might be larger.
     */
    CUPTI_ACTIVITY_ATTR_DEVICE_BUFFER_SIZE_CDP      = 1,
    /**
     * The maximum number of memory buffers per context. The value is
     * a size_t.
     *
     * Buffers can be reused by the context. Increasing this value
     * reduces the times CUPTI needs to flush the buffers. Setting this
     * value will not modify the number of memory buffers currently
     * stored.
     *
     * Set this value before initializing CUDA to ensure the limit is
     * not exceeded.
     *
     * The default value is 4.
     */
    CUPTI_ACTIVITY_ATTR_DEVICE_BUFFER_POOL_LIMIT    = 2,

    CUPTI_ACTIVITY_ATTR_DEVICE_BUFFER_FORCE_INT     = 0x7fffffff
} CUpti_ActivityAttribute;

/**
 * \brief Get the CUPTI timestamp.
 *
 * Returns a timestamp normalized to correspond with the start and end
 * timestamps reported in the CUPTI activity records. The timestamp is
 * reported in nanoseconds.
 *
 * \param timestamp Returns the CUPTI timestamp
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_INVALID_PARAMETER if \p timestamp is NULL
 */
CUptiResult CUPTIAPI cuptiGetTimestamp(uint64_t *timestamp);

/**
 * \brief Get the ID of a context.
 *
 * Get the ID of a context.
 *
 * \param context The context
 * \param contextId Returns a process-unique ID for the context
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_NOT_INITIALIZED
 * \retval CUPTI_ERROR_INVALID_CONTEXT The context is NULL or not valid.
 * \retval CUPTI_ERROR_INVALID_PARAMETER if \p contextId is NULL
 */
CUptiResult CUPTIAPI cuptiGetContextId(CUcontext context, uint32_t *contextId);

/**
 * \brief Get the ID of a stream.
 *
 * Get the ID of a stream. The stream ID is unique within a context
 * (i.e. all streams within a context will have unique stream
 * IDs).
 *
 * \param context If non-NULL then the stream is checked to ensure
 * that it belongs to this context. Typically this parameter should be
 * null.
 * \param stream The stream
 * \param streamId Returns a context-unique ID for the stream
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_NOT_INITIALIZED
 * \retval CUPTI_ERROR_INVALID_STREAM if unable to get stream ID, or
 * if \p context is non-NULL and \p stream does not belong to the
 * context
 * \retval CUPTI_ERROR_INVALID_PARAMETER if \p streamId is NULL
 *
 * \see cuptiActivityEnqueueBuffer
 * \see cuptiActivityDequeueBuffer
 */
CUptiResult CUPTIAPI cuptiGetStreamId(CUcontext context, CUstream stream, uint32_t *streamId);

/**
 * \brief Get the ID of a device
 *
 * If \p context is NULL, returns the ID of the device that contains
 * the currently active context. If \p context is non-NULL, returns
 * the ID of the device which contains that context. Operates in a
 * similar manner to cudaGetDevice() or cuCtxGetDevice() but may be
 * called from within callback functions.
 *
 * \param context The context, or NULL to indicate the current context.
 * \param deviceId Returns the ID of the device that is current for
 * the calling thread.
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_NOT_INITIALIZED
 * \retval CUPTI_ERROR_INVALID_DEVICE if unable to get device ID
 * \retval CUPTI_ERROR_INVALID_PARAMETER if \p deviceId is NULL
 */
CUptiResult CUPTIAPI cuptiGetDeviceId(CUcontext context, uint32_t *deviceId);

/**
 * \brief Enable collection of a specific kind of activity record.
 *
 * Enable collection of a specific kind of activity record. Multiple
 * kinds can be enabled by calling this function multiple times. By
 * default all activity kinds are disabled for collection.
 *
 * \param kind The kind of activity record to collect
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_NOT_INITIALIZED
 * \retval CUPTI_ERROR_NOT_COMPATIBLE if the activity kind cannot be enabled
 * \retval CUPTI_ERROR_INVALID_KIND if the activity kind is not supported
 */
CUptiResult CUPTIAPI cuptiActivityEnable(CUpti_ActivityKind kind);

/**
 * \brief Disable collection of a specific kind of activity record.
 *
 * Disable collection of a specific kind of activity record. Multiple
 * kinds can be disabled by calling this function multiple times. By
 * default all activity kinds are disabled for collection.
 *
 * \param kind The kind of activity record to stop collecting
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_NOT_INITIALIZED
 * \retval CUPTI_ERROR_INVALID_KIND if the activity kind is not supported
 */
CUptiResult CUPTIAPI cuptiActivityDisable(CUpti_ActivityKind kind);

/**
 * \brief Enable collection of a specific kind of activity record for
 * a context.
 *
 * Enable collection of a specific kind of activity record for a
 * context.  This setting done by this API will supersede the global
 * settings for activity records enabled by \ref cuptiActivityEnable.
 * Multiple kinds can be enabled by calling this function multiple
 * times.
 *
 * \param context The context for which activity is to be enabled
 * \param kind The kind of activity record to collect
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_NOT_INITIALIZED
 * \retval CUPTI_ERROR_NOT_COMPATIBLE if the activity kind cannot be enabled
 * \retval CUPTI_ERROR_INVALID_KIND if the activity kind is not supported
 */
CUptiResult CUPTIAPI cuptiActivityEnableContext(CUcontext context, CUpti_ActivityKind kind);

/**
 * \brief Disable collection of a specific kind of activity record for
 * a context.
 *
 * Disable collection of a specific kind of activity record for a context.
 * This setting done by this API will supersede the global settings
 * for activity records.
 * Multiple kinds can be enabled by calling this function multiple times.
 *
 * \param context The context for which activity is to be disabled
 * \param kind The kind of activity record to stop collecting
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_NOT_INITIALIZED
 * \retval CUPTI_ERROR_INVALID_KIND if the activity kind is not supported
 */
CUptiResult CUPTIAPI cuptiActivityDisableContext(CUcontext context, CUpti_ActivityKind kind);

/**
 * \brief Get the number of activity records that were dropped of
 * insufficient buffer space.
 *
 * Get the number of records that were dropped because of insufficient
 * buffer space.  The dropped count includes records that could not be
 * recorded because CUPTI did not have activity buffer space available
 * for the record (because the CUpti_BuffersCallbackRequestFunc
 * callback did not return an empty buffer of sufficient size) and
 * also CDP records that could not be record because the device-size
 * buffer was full (size is controlled by the
 * CUPTI_ACTIVITY_ATTR_DEVICE_BUFFER_SIZE_CDP attribute). The dropped
 * count maintained for the queue is reset to zero when this function
 * is called.
 *
 * \param context The context, or NULL to get dropped count from global queue
 * \param streamId The stream ID
 * \param dropped The number of records that were dropped since the last call
 * to this function.
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_NOT_INITIALIZED
 * \retval CUPTI_ERROR_INVALID_PARAMETER if \p dropped is NULL
 */
CUptiResult CUPTIAPI cuptiActivityGetNumDroppedRecords(CUcontext context, uint32_t streamId,
                                                       size_t *dropped);

/**
 * \brief Iterate over the activity records in a buffer.
 *
 * This is a helper function to iterate over the activity records in a
 * buffer. A buffer of activity records is typically obtained by
 * using the cuptiActivityDequeueBuffer() function or by receiving a
 * CUpti_BuffersCallbackCompleteFunc callback.
 *
 * An example of typical usage:
 * \code
 * CUpti_Activity *record = NULL;
 * CUptiResult status = CUPTI_SUCCESS;
 *   do {
 *      status = cuptiActivityGetNextRecord(buffer, validSize, &record);
 *      if(status == CUPTI_SUCCESS) {
 *           // Use record here...
 *      }
 *      else if (status == CUPTI_ERROR_MAX_LIMIT_REACHED)
 *          break;
 *      else {
 *          goto Error;
 *      }
 *    } while (1);
 * \endcode
 *
 * \param buffer The buffer containing activity records
 * \param record Inputs the previous record returned by
 * cuptiActivityGetNextRecord and returns the next activity record
 * from the buffer. If input value is NULL, returns the first activity
 * record in the buffer. Records of kind CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL
 * may contain invalid (0) timestamps, indicating that no timing information could
 * be collected for lack of device memory.
 * \param validBufferSizeBytes The number of valid bytes in the buffer.
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_NOT_INITIALIZED
 * \retval CUPTI_ERROR_MAX_LIMIT_REACHED if no more records in the buffer
 * \retval CUPTI_ERROR_INVALID_PARAMETER if \p buffer is NULL.
 */
CUptiResult CUPTIAPI cuptiActivityGetNextRecord(uint8_t* buffer, size_t validBufferSizeBytes,
                                                CUpti_Activity **record);

/**
 * \brief Function type for callback used by CUPTI to request an empty
 * buffer for storing activity records.
 *
 * This callback function signals the CUPTI client that an activity
 * buffer is needed by CUPTI. The activity buffer is used by CUPTI to
 * store activity records. The callback function can decline the
 * request by setting \p *buffer to NULL. In this case CUPTI may drop
 * activity records.
 *
 * \param buffer Returns the new buffer. If set to NULL then no buffer
 * is returned.
 * \param size Returns the size of the returned buffer.
 * \param maxNumRecords Returns the maximum number of records that
 * should be placed in the buffer. If 0 then the buffer is filled with
 * as many records as possible. If > 0 the buffer is filled with at
 * most that many records before it is returned.
 */
typedef void (CUPTIAPI *CUpti_BuffersCallbackRequestFunc)(
    uint8_t **buffer,
    size_t *size,
    size_t *maxNumRecords);

/**
 * \brief Function type for callback used by CUPTI to return a buffer
 * of activity records.
 *
 * This callback function returns to the CUPTI client a buffer
 * containing activity records.  The buffer contains \p validSize
 * bytes of activity records which should be read using
 * cuptiActivityGetNextRecord. The number of dropped records can be
 * read using cuptiActivityGetNumDroppedRecords. After this call CUPTI
 * relinquished ownership of the buffer and will not use it
 * anymore. The client may return the buffer to CUPTI using the
 * CUpti_BuffersCallbackRequestFunc callback.
 * Note: CUDA 6.0 onwards, all buffers returned by this callback are
 * global buffers i.e. there is no context/stream specific buffer.
 * User needs to parse the global buffer to extract the context/stream
 * specific activity records.
 *
 * \param context The context this buffer is associated with. If NULL, the
 * buffer is associated with the global activities. This field is deprecated
 * as of CUDA 6.0 and will always be NULL.
 * \param streamId The stream id this buffer is associated with.
 * This field is deprecated as of CUDA 6.0 and will always be NULL.
 * \param buffer The activity record buffer.
 * \param size The total size of the buffer in bytes as set in
 * CUpti_BuffersCallbackRequestFunc.
 * \param validSize The number of valid bytes in the buffer.
 */
typedef void (CUPTIAPI *CUpti_BuffersCallbackCompleteFunc)(
    CUcontext context,
    uint32_t streamId,
    uint8_t *buffer,
    size_t size,
    size_t validSize);

/**
 * \brief Registers callback functions with CUPTI for activity buffer
 * handling.
 *
 * This function registers two callback functions to be used in asynchronous
 * buffer handling. If registered, activity record buffers are handled using
 * asynchronous requested/completed callbacks from CUPTI.
 *
 * Registering these callbacks prevents the client from using CUPTI's
 * blocking enqueue/dequeue functions.
 *
 * \param funcBufferRequested callback which is invoked when an empty
 * buffer is requested by CUPTI
 * \param funcBufferCompleted callback which is invoked when a buffer
 * containing activity records is available from CUPTI
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_INVALID_PARAMETER if either \p
 * funcBufferRequested or \p funcBufferCompleted is NULL
 */
CUptiResult CUPTIAPI cuptiActivityRegisterCallbacks(CUpti_BuffersCallbackRequestFunc funcBufferRequested,
        CUpti_BuffersCallbackCompleteFunc funcBufferCompleted);

/**
 * \brief Wait for all activity records are delivered via the
 * completion callback.
 *
 * This function does not return until all activity records associated
 * with the specified context/stream are returned to the CUPTI client
 * using the callback registered in cuptiActivityRegisterCallbacks. To
 * ensure that all activity records are complete, the requested
 * stream(s), if any, are synchronized.
 *
 * If \p context is NULL, the global activity records (i.e. those not
 * associated with a particular stream) are flushed (in this case no
 * streams are synchonized).  If \p context is a valid CUcontext and
 * \p streamId is 0, the buffers of all streams of this context are
 * flushed.  Otherwise, the buffers of the specified stream in this
 * context is flushed.
 *
 * Before calling this function, the buffer handling callback api
 * must be activated by calling cuptiActivityRegisterCallbacks.
 *
 * \param context A valid CUcontext or NULL.
 * \param streamId The stream ID.
 * \param flag The flag can be set to indicate a forced flush. See CUpti_ActivityFlag
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_NOT_INITIALIZED
 * \retval CUPTI_ERROR_CUPTI_ERROR_INVALID_OPERATION if not preceeded
 * by a successful call to cuptiActivityRegisterCallbacks
 * \retval CUPTI_ERROR_UNKNOWN an internal error occurred
 *
 * **DEPRECATED** This method is deprecated
 * CONTEXT and STREAMID will be ignored. Use cuptiActivityFlushAll
 * to flush all data.
 */
CUptiResult CUPTIAPI cuptiActivityFlush(CUcontext context, uint32_t streamId, uint32_t flag);

/**
 * \brief Wait for all activity records are delivered via the
 * completion callback.
 *
 * This function does not return until all activity records associated
 * with all contexts/streams (and the global buffers not associated
 * with any stream) are returned to the CUPTI client using the
 * callback registered in cuptiActivityRegisterCallbacks. To ensure
 * that all activity records are complete, the requested stream(s), if
 * any, are synchronized.
 *
 * Before calling this function, the buffer handling callback api must
 * be activated by calling cuptiActivityRegisterCallbacks.
 *
 * \param flag The flag can be set to indicate a forced flush. See CUpti_ActivityFlag
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_NOT_INITIALIZED
 * \retval CUPTI_ERROR_INVALID_OPERATION if not preceeded by a
 * successful call to cuptiActivityRegisterCallbacks
 * \retval CUPTI_ERROR_UNKNOWN an internal error occurred
 */
CUptiResult CUPTIAPI cuptiActivityFlushAll(uint32_t flag);

/**
 * \brief Read an activity API attribute.
 *
 * Read an activity API attribute and return it in \p *value.
 *
 * \param attr The attribute to read
 * \param valueSize Size of buffer pointed by the value, and
 * returns the number of bytes written to \p value
 * \param value Returns the value of the attribute
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_NOT_INITIALIZED
 * \retval CUPTI_ERROR_INVALID_PARAMETER if \p valueSize or \p value is NULL, or
 * if \p attr is not an activity attribute
 * \retval CUPTI_ERROR_PARAMETER_SIZE_NOT_SUFFICIENT Indicates that
 * the \p value buffer is too small to hold the attribute value.
 */
CUptiResult CUPTIAPI cuptiActivityGetAttribute(CUpti_ActivityAttribute attr,
        size_t *valueSize, void* value);

/**
 * \brief Write an activity API attribute.
 *
 * Write an activity API attribute.
 *
 * \param attr The attribute to write
 * \param valueSize The size, in bytes, of the value
 * \param value The attribute value to write
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_NOT_INITIALIZED
 * \retval CUPTI_ERROR_INVALID_PARAMETER if \p valueSize or \p value is NULL, or
 * if \p attr is not an activity attribute
 * \retval CUPTI_ERROR_PARAMETER_SIZE_NOT_SUFFICIENT Indicates that
 * the \p value buffer is too small to hold the attribute value.
 */
CUptiResult CUPTIAPI cuptiActivitySetAttribute(CUpti_ActivityAttribute attr,
        size_t *valueSize, void* value);


/**
 * \brief Set Unified Memory Counter configuration.
 *
 * \param config A pointer to \ref CUpti_ActivityUnifiedMemoryCounterConfig structures
 * containing Unified Memory counter configuration.
 * \param count Number of Unified Memory counter configuration structures
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_NOT_INITIALIZED
 * \retval CUPTI_ERROR_INVALID_PARAMETER if \p config is NULL or
 * any parameter in the \p config structures is not a valid value
 * \retval CUPTI_ERROR_NOT_SUPPORTED Indicates that the system/device
 * does not support the unified memory counters
 */
CUptiResult CUPTIAPI cuptiActivityConfigureUnifiedMemoryCounter(CUpti_ActivityUnifiedMemoryCounterConfig *config, uint32_t count);

/**
 * \brief Get auto boost state
 *
 * The profiling results can be inconsistent in case auto boost is enabled.
 * CUPTI tries to disable auto boost while profiling. It can fail to disable in
 * cases where user does not have the permissions or CUDA_AUTO_BOOST env
 * variable is set. The function can be used to query whether auto boost is
 * enabled.
 *
 * \param context A valid CUcontext.
 * \param state A pointer to \ref CUpti_ActivityAutoBoostState structure which
 * contains the current state and the id of the process that has requested the
 * current state
 *
 * \retval CUPTI_SUCCESS
 * \retval CUPTI_ERROR_INVALID_PARAMETER if \p CUcontext or \p state is NULL
 * \retval CUPTI_ERROR_NOT_SUPPORTED Indicates that the device does not support auto boost
 * \retval CUPTI_ERROR_UNKNOWN an internal error occurred
 */
CUptiResult CUPTIAPI cuptiGetAutoBoostState(CUcontext context, CUpti_ActivityAutoBoostState *state);

/** @} */ /* END CUPTI_ACTIVITY_API */

#if defined(__cplusplus)
}
#endif

#endif /*_CUPTI_ACTIVITY_H_*/
