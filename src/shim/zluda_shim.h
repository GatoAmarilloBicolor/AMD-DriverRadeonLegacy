/*
 * zluda_shim.h - ZLUDA native shim: CUDA driver API -> radeon_gpud
 * Traduce CUDA Driver API (cu*) a radeon_gpud IPC.
 * Permite ejecutar aplicaciones CUDA nativas en AMD GPUs via radeon_gpud.
 */
#ifndef ZLUDA_SHIM_H
#define ZLUDA_SHIM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CUDA Driver API types (subset compatible with CUDA 11.x/12.x)
 * ======================================================================== */

typedef struct CUctx_st *CUcontext;
typedef struct CUmod_st *CUmodule;
typedef struct CUfunc_st *CUfunction;
typedef struct CUdevptr_st *CUdeviceptr;
typedef struct CUevent_st *CUevent;
typedef struct CUstream_st *CUstream;
typedef struct CUdev_st *CUdevice;
typedef struct CUarray_st *CUarray;
typedef struct CUmipmappedArray_st *CUmipmappedArray;
typedef struct CUtexref_st *CUtexref;
typedef struct CUsurfref_st *CUsurfref;
typedef struct CUgraphicsResource_st *CUgraphicsResource;
typedef struct CUoccupancy_st *CUoccupancy;

typedef int CUresult;

#define CUDA_SUCCESS                          0
#define CUDA_ERROR_INVALID_VALUE              1
#define CUDA_ERROR_OUT_OF_MEMORY              2
#define CUDA_ERROR_NOT_INITIALIZED            3
#define CUDA_ERROR_DEINITIALIZED              4
#define CUDA_ERROR_NO_DEVICE                  100
#define CUDA_ERROR_INVALID_DEVICE             101
#define CUDA_ERROR_INVALID_IMAGE              200
#define CUDA_ERROR_INVALID_CONTEXT            201
#define CUDA_ERROR_CONTEXT_ALREADY_CURRENT    202
#define CUDA_ERROR_MAP_FAILED                 205
#define CUDA_ERROR_UNMAP_FAILED               206
#define CUDA_ERROR_ARRAY_IS_MAPPED            207
#define CUDA_ERROR_ALREADY_MAPPED             208
#define CUDA_ERROR_NO_BINARY_FOR_GPU          209
#define CUDA_ERROR_ALREADY_ACQUIRED           209
#define CUDA_ERROR_NOT_MAPPED                 210
#define CUDA_ERROR_NOT_MAPPED_AS_ARRAY        211
#define CUDA_ERROR_ALREADY_MAPPED_AS_ARRAY    212
#define CUDA_ERROR_INVALID_SOURCE             300
#define CUDA_ERROR_FILE_NOT_FOUND             301
#define CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND 302
#define CUDA_ERROR_SHARED_OBJECT_INIT_FAILED  303
#define CUDA_ERROR_OPERATING_SYSTEM           304
#define CUDA_ERROR_INVALID_HANDLE             400
#define CUDA_ERROR_ILLEGAL_STATE              401
#define CUDA_ERROR_NOT_FOUND                  500
#define CUDA_ERROR_NOT_READY                  600
#define CUDA_ERROR_ILLEGAL_ADDRESS            700
#define CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES    701
#define CUDA_ERROR_LAUNCH_TIMEOUT             702
#define CUDA_ERROR_LAUNCH_INCOMPATIBLE_TEXTURING 703
#define CUDA_ERROR_PEER_ACCESS_ALREADY_ENABLED 704
#define CUDA_ERROR_PEER_ACCESS_NOT_ENABLED    705
#define CUDA_ERROR_PRIMARY_CONTEXT_ACTIVE     708
#define CUDA_ERROR_CONTEXT_IS_DESTROYED       709
#define CUDA_ERROR_ASSERT                     710
#define CUDA_ERROR_TOO_MANY_PEERS             711
#define CUDA_ERROR_HOST_MEMORY_ALREADY_REGISTERED 712
#define CUDA_ERROR_HOST_MEMORY_NOT_REGISTERED 713
#define CUDA_ERROR_HARDWARE_STACK_ERROR       714
#define CUDA_ERROR_ILLEGAL_INSTRUCTION        715
#define CUDA_ERROR_MISALIGNED_ADDRESS         716
#define CUDA_ERROR_INVALID_COMPUTER           717
#define CUDA_ERROR_UNSUPPORTED_LIMIT          718
#define CUDA_ERROR_DEVICE_UNAVAILABLE         719
#define CUDA_ERROR_INVALID_PTX                720
#define CUDA_ERROR_INVALID_GRAPHICS_CONTEXT   721
#define CUDA_ERROR_NVLINK_UNCORRECTABLE       722
#define CUDA_ERROR_JIT_COMPILER_NOT_FOUND     723
#define CUDA_ERROR_UNSUPPORTED_PTX_VERSION    724
#define CUDA_ERROR_JIT_COMPILATION_DISABLED   725
#define CUDA_ERROR_UNSUPPORTED_EXEC_AFFINITY  726
#define CUDA_ERROR_UNSUPPORTED_DEV_SIDE_SYNC  727
#define CUDA_ERROR_UNKNOWN                    999

/* Device attributes */
typedef enum CUdevice_attribute_enum {
    CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK = 1,
    CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X = 2,
    CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Y = 3,
    CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Z = 4,
    CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_X = 5,
    CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Y = 6,
    CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Z = 7,
    CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK = 8,
    CU_DEVICE_ATTRIBUTE_TOTAL_CONSTANT_MEMORY = 9,
    CU_DEVICE_ATTRIBUTE_WARP_SIZE = 10,
    CU_DEVICE_ATTRIBUTE_MAX_PITCH = 11,
    CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_BLOCK = 12,
    CU_DEVICE_ATTRIBUTE_CLOCK_RATE = 13,
    CU_DEVICE_ATTRIBUTE_TEXTURE_ALIGNMENT = 14,
    CU_DEVICE_ATTRIBUTE_GPU_OVERLAP = 15,
    CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT = 16,
    CU_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT = 17,
    CU_DEVICE_ATTRIBUTE_INTEGRATED = 18,
    CU_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY = 19,
    CU_DEVICE_ATTRIBUTE_COMPUTE_MODE = 20,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_WIDTH = 21,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_WIDTH = 22,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_HEIGHT = 23,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH = 24,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT = 25,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH = 26,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_ARRAY_WIDTH = 27,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_ARRAY_HEIGHT = 28,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_ARRAY_NUMSLICES = 29,
    CU_DEVICE_ATTRIBUTE_SURFACE_ALIGNMENT = 30,
    CU_DEVICE_ATTRIBUTE_CONCURRENT_KERNELS = 31,
    CU_DEVICE_ATTRIBUTE_ECC_ENABLED = 32,
    CU_DEVICE_ATTRIBUTE_PCI_BUS_ID = 33,
    CU_DEVICE_ATTRIBUTE_PCI_DEVICE_ID = 34,
    CU_DEVICE_ATTRIBUTE_TCC_DRIVER = 35,
    CU_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE = 36,
    CU_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH = 37,
    CU_DEVICE_ATTRIBUTE_L2_CACHE_SIZE = 38,
    CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR = 39,
    CU_DEVICE_ATTRIBUTE_ASYNC_ENGINE_COUNT = 38,
    CU_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING = 41,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_WIDTH = 42,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_LAYERS = 43,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_GATHER_WIDTH = 44,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_GATHER_HEIGHT = 45,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH_ALTERNATE = 46,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT_ALTERNATE = 47,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH_ALTERNATE = 48,
    CU_DEVICE_ATTRIBUTE_PCI_DOMAIN_ID = 49,
    CU_DEVICE_ATTRIBUTE_MAX_PITCH = 11,
    CU_DEVICE_ATTRIBUTE_MANAGED_MEMORY = 71,
    CU_DEVICE_ATTRIBUTE_MULTI_GPU_BOARD = 72,
    CU_DEVICE_ATTRIBUTE_MULTI_GPU_BOARD_GROUP_ID = 73,
    CU_DEVICE_ATTRIBUTE_HOST_NATIVE_ATOMIC_SUPPORTED = 74,
    CU_DEVICE_ATTRIBUTE_SINGLE_TO_DOUBLE_PRECISION_PERF_RATIO = 75,
    CU_DEVICE_ATTRIBUTE_PAGEABLE_MEMORY_ACCESS = 76,
    CU_DEVICE_ATTRIBUTE_CONCURRENT_MANAGED_ACCESS = 77,
    CU_DEVICE_ATTRIBUTE_COMPUTE_PREEMPTION_SUPPORTED = 78,
    CU_DEVICE_ATTRIBUTE_CAN_USE_HOST_POINTER_FOR_REGISTERED_MEM = 79,
    CU_DEVICE_ATTRIBUTE_CAN_USE_STREAM_MEM_OPS = 80,
    CU_DEVICE_ATTRIBUTE_CAN_USE_64_BIT_STREAM_MEM_OPS = 81,
    CU_DEVICE_ATTRIBUTE_CAN_FLUSH_REMOTE_WRITES = 82,
    CU_DEVICE_ATTRIBUTE_HOST_REGISTER_SUPPORTED = 83,
    CU_DEVICE_ATTRIBUTE_PAGEABLE_MEMORY_ACCESS_USES_HOST_PAGE_TABLES = 84,
    CU_DEVICE_ATTRIBUTE_DIRECT_MANAGED_MEM_ACCESS_FROM_HOST = 85,
    CU_DEVICE_ATTRIBUTE_MAX_BLOCKS_PER_MULTIPROCESSOR = 86,
    CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_MULTIPROCESSOR = 87,
    CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_MULTIPROCESSOR = 88,
    CU_DEVICE_ATTRIBUTE_MAX_WARPS_PER_MULTIPROCESSOR = 89,
} CUdevice_attribute;

/* Memory types */
typedef enum CUmemorytype_enum {
    CU_MEMORYTYPE_HOST = 1,
    CU_MEMORYTYPE_DEVICE = 2,
    CU_MEMORYTYPE_ARRAY = 3,
    CU_MEMORYTYPE_UNIFIED = 4,
} CUmemorytype;

/* Memory allocation flags */
typedef enum CUmemAllocationFlags_enum {
    CU_MEM_ALLOC_GRANULARITY_MINIMUM = 0x1,
    CU_MEM_ALLOC_GRANULARITY_RECOMMENDED = 0x2,
} CUmemAllocationFlags;

/* Memory allocation properties */
typedef enum CUmemAllocationProp_type {
    CU_MEM_ALLOCATION_TYPE_PINNED = 1,
    CU_MEM_ALLOCATION_TYPE_INVALID = 0,
} CUmemAllocationProp_type;

/* Stream flags */
typedef enum CUstream_flags_enum {
    CU_STREAM_DEFAULT = 0,
    CU_STREAM_NON_BLOCKING = 1,
} CUstream_flags;

/* Event flags */
typedef enum CUevent_flags_enum {
    CU_EVENT_DEFAULT = 0,
    CU_EVENT_BLOCKING_SYNC = 1,
    CU_EVENT_DISABLE_TIMING = 2,
    CU_EVENT_INTERPROCESS = 4,
} CUevent_flags;

/* Function attributes */
typedef enum CUfunction_attribute_enum {
    CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK = 0,
    CU_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES = 1,
    CU_FUNC_ATTRIBUTE_CONST_SIZE_BYTES = 2,
    CU_FUNC_ATTRIBUTE_LOCAL_SIZE_BYTES = 3,
    CU_FUNC_ATTRIBUTE_NUM_REGS = 4,
    CU_FUNC_ATTRIBUTE_PTX_VERSION = 5,
    CU_FUNC_ATTRIBUTE_BINARY_VERSION = 6,
    CU_FUNC_ATTRIBUTE_CACHE_CONFIG = 7,
    CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES = 8,
    CU_FUNC_ATTRIBUTE_PREFERRED_SHARED_MEMORY_CARVEOUT = 9,
} CUfunction_attribute;

/* Cache config */
typedef enum CUfunc_cache_enum {
    CU_FUNC_CACHE_PREFER_NONE = 0,
    CU_FUNC_CACHE_PREFER_SHARED = 1,
    CU_FUNC_CACHE_PREFER_L1 = 2,
    CU_FUNC_CACHE_PREFER_EQUAL = 3,
} CUfunc_cache;

/* JIT options */
typedef enum CUjit_option_enum {
    CU_JIT_MAX_REGISTERS = 0,
    CU_JIT_THREADS_PER_BLOCK = 1,
    CU_JIT_WALL_TIME = 2,
    CU_JIT_INFO_LOG_BUFFER = 3,
    CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES = 4,
    CU_JIT_ERROR_LOG_BUFFER = 5,
    CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES = 6,
    CU_JIT_OPTIMIZATION_LEVEL = 7,
    CU_JIT_TARGET_FROM_CUCONTEXT = 8,
    CU_JIT_TARGET = 9,
    CU_JIT_FALLBACK_STRATEGY = 10,
    CU_JIT_GENERATE_DEBUG_INFO = 11,
    CU_JIT_LOG_VERBOSE = 12,
    CU_JIT_GENERATE_LINE_INFO = 13,
    CU_JIT_GENERATE_DEBUG_INFO = 11,
    CU_JIT_NUM_OPTIONS = 14,
} CUjit_option;

/* Occupancy */
typedef struct CUoccupancy_st *CUoccupancy;

/* Launch config */
typedef struct CUlaunchConfig {
    int gridDimX;
    int gridDimY;
    int gridDimZ;
    int blockDimX;
    int blockDimY;
    int blockDimZ;
    size_t sharedMemBytes;
    CUstream hStream;
    void **kernelParams;
    void **extra;
} CUlaunchConfig;

/* Array descriptor */
typedef enum CUarray_format_enum {
    CU_AD_FORMAT_UNSIGNED_INT8 = 0x01,
    CU_AD_FORMAT_UNSIGNED_INT16 = 0x02,
    CU_AD_FORMAT_UNSIGNED_INT32 = 0x03,
    CU_AD_FORMAT_SIGNED_INT8 = 0x08,
    CU_AD_FORMAT_SIGNED_INT16 = 0x09,
    CU_AD_FORMAT_SIGNED_INT32 = 0x0a,
    CU_AD_FORMAT_HALF = 0x10,
    CU_AD_FORMAT_FLOAT = 0x20,
} CUarray_format;

typedef struct CUDA_ARRAY_DESCRIPTOR {
    size_t Width;
    size_t Height;
    CUarray_format Format;
    unsigned int NumChannels;
} CUDA_ARRAY_DESCRIPTOR;

typedef enum CUaddress_mode_enum {
    CU_TR_ADDRESS_MODE_WRAP = 0,
    CU_TR_ADDRESS_MODE_CLAMP = 1,
    CU_TR_ADDRESS_MODE_MIRROR = 2,
    CU_TR_ADDRESS_MODE_BORDER = 3,
} CUaddress_mode;

typedef enum CUfilter_mode_enum {
    CU_TR_FILTER_MODE_POINT = 0,
    CU_TR_FILTER_MODE_LINEAR = 1,
} CUfilter_mode;

/* ========================================================================
 * ZLUDA Context (internal)
 * ======================================================================== */

typedef struct zluda_context {
    /* Radeon server connection */
    int radeon_fd;
    uint32_t team_id;
    
    /* CUDA context state */
    CUcontext current_ctx;
    uint32_t device_id;
    uint32_t compute_capability_major;
    uint32_t compute_capability_minor;
    
    /* Memory management */
    void *host_mem_pool;
    size_t host_mem_pool_size;
    
    /* Module cache */
    void *module_cache;
    
    /* Stream management */
    CUstream default_stream;
    
    /* Event management */
    CUevent default_event;
} zluda_context_t;

/* ========================================================================
 * ZLUDA API
 * ======================================================================== */

/* Initialize ZLUDA - called once at process start */
CUresult zluda_init(void);

/* Shutdown ZLUDA - called at process exit */
CUresult zluda_shutdown(void);

/* Get ZLUDA context for current thread */
zluda_context_t *zluda_get_context(void);

/* Set current ZLUDA context */
CUresult zluda_set_context(zluda_context_t *ctx);

/* ========================================================================
 * CUDA Driver API Implementation
 * ======================================================================== */

/* Initialization */
CUresult cuInit(unsigned int Flags);

/* Device management */
CUresult cuDeviceGet(CUdevice *device, int ordinal);
CUresult cuDeviceGetCount(int *count);
CUresult cuDeviceGetName(char *name, int len, CUdevice device);
CUresult cuDeviceGetAttribute(int *pi, CUdevice_attribute attrib, CUdevice device);
CUresult cuDeviceComputeCapability(int *major, int *minor, CUdevice device);
CUresult cuDeviceTotalMem_v2(size_t *bytes, CUdevice device);

/* Context management */
CUresult cuCtxCreate_v2(CUcontext *pctx, unsigned int flags, CUdevice device);
CUresult cuCtxDestroy_v2(CUcontext ctx);
CUresult cuCtxSetCurrent(CUcontext ctx);
CUresult cuCtxGetCurrent(CUcontext *pctx);
CUresult cuCtxGetDevice(CUdevice *device);
CUresult cuCtxSynchronize(void);

/* Module management */
CUresult cuModuleLoad(CUmodule *module, const char *fname);
CUresult cuModuleLoadData(CUmodule *module, const void *image);
CUresult cuModuleLoadDataEx(CUmodule *module, const void *image, unsigned int numOptions, CUjit_option *options, void **optionValues);
CUresult cuModuleUnload(CUmodule module);
CUresult cuModuleGetFunction(CUfunction *hfunc, CUmodule hmod, const char *name);
CUresult cuModuleGetGlobal_v2(CUdeviceptr *dptr, size_t *bytes, CUmodule hmod, const char *name);
CUresult cuModuleGetTexRef(CUtexref *pTexRef, CUmodule hmod, const char *name);

/* Memory management */
CUresult cuMemAlloc_v2(CUdeviceptr *dptr, size_t bytesize);
CUresult cuMemFree_v2(CUdeviceptr dptr);
CUresult cuMemAllocHost_v2(void **pp, size_t bytesize);
CUresult cuMemFreeHost(void *p);
CUresult cuMemcpyHtoD_v2(CUdeviceptr dst, const void *src, size_t ByteCount);
CUresult cuMemcpyDtoH_v2(void *dst, CUdeviceptr src, size_t ByteCount);
CUresult cuMemcpyDtoD_v2(CUdeviceptr dst, CUdeviceptr src, size_t ByteCount);
CUresult cuMemcpyDtoDAsync_v2(CUdeviceptr dst, CUdeviceptr src, size_t ByteCount, CUstream hStream);
CUresult cuMemcpyHtoDAsync_v2(CUdeviceptr dst, const void *src, size_t ByteCount, CUstream hStream);
CUresult cuMemcpyDtoHAsync_v2(void *dst, CUdeviceptr src, size_t ByteCount, CUstream hStream);
CUresult cuMemsetD8_v2(CUdeviceptr dst, unsigned char uc, size_t N);
CUresult cuMemsetD16_v2(CUdeviceptr dst, unsigned short us, size_t N);
CUresult cuMemsetD32_v2(CUdeviceptr dst, unsigned int ui, size_t N);
CUresult cuMemsetD8Async_v2(CUdeviceptr dst, unsigned char uc, size_t N, CUstream hStream);
CUresult cuMemsetD16Async_v2(CUdeviceptr dst, unsigned short us, size_t N, CUstream hStream);
CUresult cuMemsetD32Async_v2(CUdeviceptr dst, unsigned int ui, size_t N, CUstream hStream);
CUresult cuMemcpy2D_v2(const CUDA_MEMCPY2D *pCopy);
CUresult cuMemcpy2DAsync_v2(const CUDA_MEMCPY2D *pCopy, CUstream hStream);
CUresult cuMemcpy3D_v2(const CUDA_MEMCPY3D *pCopy);
CUresult cuMemcpy3DAsync_v2(const CUDA_MEMCPY3D *pCopy, CUstream hStream);

/* Memory allocation with properties (CUDA 11+) */
CUresult cuMemCreate(CUmemGenericAllocationHandle *handle, size_t size, 
                     const CUmemAllocationProp *prop, unsigned long long flags);
CUresult cuMemRelease(CUmemGenericAllocationHandle handle);
CUresult cuMemMap(CUdeviceptr ptr, size_t size, size_t offset,
                  CUmemGenericAllocationHandle handle, unsigned long long flags);
CUresult cuMemUnmap(CUdeviceptr ptr, size_t size);

/* Stream management */
CUresult cuStreamCreate(CUstream *phStream, unsigned int Flags);
CUresult cuStreamDestroy_v2(CUstream hStream);
CUresult cuStreamSynchronize(CUstream hStream);
CUresult cuStreamQuery(CUstream hStream);
CUresult cuStreamWaitEvent_v2(CUstream hStream, CUevent hEvent, unsigned int Flags);

/* Event management */
CUresult cuEventCreate(CUevent *phEvent, unsigned int Flags);
CUresult cuEventDestroy_v2(CUevent hEvent);
CUresult cuEventRecord(CUevent hEvent, CUstream hStream);
CUresult cuEventQuery(CUevent hEvent);
CUresult cuEventSynchronize(CUevent hEvent);
CUresult cuEventElapsedTime(float *ms, CUevent hStart, CUevent hEnd);

/* Kernel launch */
CUresult cuLaunchKernel(CUfunction f,
                        unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ,
                        unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
                        unsigned int sharedMemBytes,
                        CUstream hStream,
                        void **kernelParams, void **extra);
CUresult cuLaunchKernelEx(const CUlaunchConfig *config, CUfunction f, void **kernelParams, void **extra);

/* Occupancy API */
CUresult cuOccupancyAvailable(CUoccupancy *occupancy, CUfunction func, int blockSize, size_t dynamicSmemSize);
CUresult cuOccupancyMaxActiveBlocksPerMultiprocessor(int *numBlocks, CUfunction func, int blockSize, size_t dynamicSmemSize);
CUresult cuOccupancyMaxActiveClusters(int *numClusters, CUfunction func, const CUlaunchConfig *config);

/* Function attributes */
CUresult cuFuncSetAttribute(CUfunction hfunc, CUfunction_attribute attrib, int value);
CUresult cuFuncGetAttribute(int *pi, CUfunction_attribute attrib, CUfunction hfunc);

/* Array/Texture/Surface */
CUresult cuArrayCreate_v2(CUarray *pHandle, const CUDA_ARRAY_DESCRIPTOR *pDesc);
CUresult cuArrayDestroy(CUarray hArray);
CUresult cuTexRefCreate(CUtexref *pTexRef);
CUresult cuTexRefDestroy(CUtexref hTexRef);
CUresult cuTexRefSetArray(CUtexref hTexRef, CUarray hArray, unsigned int Flags);
CUresult cuTexRefSetAddress_v2(size_t *ByteOffset, CUtexref hTexRef, CUdeviceptr dptr, size_t bytes);
CUresult cuTexRefSetFormat(CUtexref hTexRef, CUarray_format fmt, int NumPackedComponents);
CUresult cuTexRefSetAddressMode(CUtexref hTexRef, int dim, CUaddress_mode am);
CUresult cuTexRefSetFilterMode(CUtexref hTexRef, CUfilter_mode fm);
CUresult cuTexRefSetFlags(CUtexref hTexRef, unsigned int Flags);
CUresult cuTexRefSetBorderColor(CUtexref hTexRef, float *pBorderColor);

/* Graphics interop */
CUresult cuGraphicsMapResources(unsigned int count, CUgraphicsResource *resources, CUstream hStream);
CUresult cuGraphicsUnmapResources(unsigned int count, CUgraphicsResource *resources, CUstream hStream);
CUresult cuGraphicsResourceGetMappedPointer_v2(CUdeviceptr *pdptr, size_t *pSize, CUgraphicsResource resource);
CUresult cuGraphicsSubResourceGetMappedArray(CUarray *pArray, CUgraphicsResource resource, unsigned int arrayIndex, unsigned int mipLevel);

/* Version */
CUresult cuDriverGetVersion(int *driverVersion);

/* Error strings */
const char *cuGetErrorString(CUresult error);
const char *cuGetErrorName(CUresult error);

#ifdef __cplusplus
}
#endif

#endif /* ZLUDA_SHIM_H */