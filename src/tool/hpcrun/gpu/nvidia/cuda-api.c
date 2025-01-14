// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2020, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//   cuda-api.c
//
// Purpose:
//   wrapper around NVIDIA CUDA layer
//
//***************************************************************************


//*****************************************************************************
// system include files
//*****************************************************************************

#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <stdio.h>     // sprintf
#include <unistd.h>
#include <sys/stat.h>  // mkdir

#ifndef HPCRUN_STATIC_LINK
#include <dlfcn.h>
#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <link.h>          // dl_iterate_phdr
#include <linux/limits.h>  // PATH_MAX
#include <string.h>        // strstr
#endif

#include <cuda.h>
#include <cuda_runtime.h>


//*****************************************************************************
// local include files
//*****************************************************************************

#include <lib/prof-lean/spinlock.h>

#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/files.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/messages/messages.h>

#include "cuda-api.h"
#include "cubin-hash-map.h"
#include "cubin-id-map.h"

//*****************************************************************************
// macros
//*****************************************************************************

#define CUDA_FN_NAME(f) DYN_FN_NAME(f)

#define CUDA_FN(fn, args) \
  static CUresult (*CUDA_FN_NAME(fn)) args

#define CUDA_RUNTIME_FN(fn, args) \
  static cudaError_t (*CUDA_FN_NAME(fn)) args

#define HPCRUN_CUDA_API_CALL(fn, args)                              \
{                                                                   \
  CUresult error_result = CUDA_FN_NAME(fn) args;		    \
  if (error_result != CUDA_SUCCESS) {				    \
    fprintf(stderr, "cuda api %s returned %d", #fn,                 \
          (int) error_result);                                      \
    ETMSG(CUDA, "cuda api %s returned %d", #fn,                     \
          (int) error_result);                                      \
    exit(-1);							    \
  }								    \
}


#define HPCRUN_CUDA_RUNTIME_CALL(fn, args)                          \
{                                                                   \
  cudaError_t error_result = CUDA_FN_NAME(fn) args;		    \
  if (error_result != cudaSuccess) {				    \
    ETMSG(CUDA, "cuda runtime %s returned %d", #fn,                 \
          (int) error_result);                                      \
    exit(-1);							    \
  }								    \
}


//----------------------------------------------------------------------
// device capability
//----------------------------------------------------------------------

#define COMPUTE_MAJOR_TURING 	7
#define COMPUTE_MINOR_TURING 	5

#define DEVICE_IS_TURING(major, minor)      \
  ((major == COMPUTE_MAJOR_TURING) && (minor == COMPUTE_MINOR_TURING))


//----------------------------------------------------------------------
// runtime version
//----------------------------------------------------------------------

#define CUDA11 11

// according to https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART____VERSION.html,
// CUDA encodes the runtime version number as (1000 * major + 10 * minor)

#define RUNTIME_MAJOR_VERSION(rt_version) (rt_version / 1000) 
#define RUNTIME_MINOR_VERSION(rt_version) (rt_version % 10) 


//******************************************************************************
// static data
//******************************************************************************

static spinlock_t files_lock = SPINLOCK_UNLOCKED;
static __thread bool cuda_internal = false;

#ifndef HPCRUN_STATIC_LINK

CUDA_FN
(
 cuDeviceGetAttribute,
 (
  int *pi,
  CUdevice_attribute attrib,
  CUdevice dev
 )
);


CUDA_FN
(
 cuCtxGetCurrent,
 (
  CUcontext *ctx
 )
);


CUDA_FN
(
 cuCtxSetCurrent,
 (
  CUcontext ctx
 )
);


CUDA_RUNTIME_FN
(
 cudaGetDevice,
 (
  int *device_id
 )
);


CUDA_RUNTIME_FN
(
 cudaRuntimeGetVersion,
 ( 
  int *runtimeVersion
 )
);


CUDA_FN
(
 cuCtxGetStreamPriorityRange, 
 (
  int *leastPriority,
  int *greatestPriority
 ) 
);


CUDA_FN
(
 cuStreamCreateWithPriority,
 (
  CUstream *phStream,
  unsigned int flags,
  int priority
 );
);


CUDA_FN
(
 cuStreamCreate,
 (
  CUstream *phStream,
  unsigned int Flags
 );
);


CUDA_FN
(
 cuStreamSynchronize,
 (
  CUstream hStream
 );
);


CUDA_FN
(
 cuMemcpyDtoHAsync,
 (
  void *dst,
  CUdeviceptr src,
  size_t byteCount,
  CUstream stream
 );
);


CUDA_FN
(
 cuMemcpyHtoDAsync,
 (
  CUdeviceptr dst,
  void *src,
  size_t byteCount,
  CUstream stream
 );
);


CUDA_FN
(
 cuModuleLoad,
 (
  CUmodule *module,
  const char *fname
 );
);


CUDA_FN
(
 cuModuleGetFunction,
 (
  CUfunction *hfunc,
  CUmodule hmod,
  const char *name
 );
);


CUDA_FN
(
 cuLaunchKernel,
 (
  CUfunction f,
  unsigned int gridDimX,
  unsigned int gridDimY,
  unsigned int gridDimZ,
  unsigned int blockDimX,
  unsigned int blockDimY,
  unsigned int blockDimZ,
  unsigned int sharedMemBytes,
  CUstream hStream,
  void **kernelParams,
  void **extra
 );
);


CUDA_FN
(
 cuFuncSetAttribute,
 (
  CUfunction hfunc,
  CUfunction_attribute attrib,
  int value
 );
);


CUDA_FN
(
 cuMemHostAlloc,
 (
  void** pp,
  size_t bytesize,
  unsigned int Flags
 );
);

#endif


//******************************************************************************
// private operations
//******************************************************************************

int
cuda_bind
(
 void
)
{
#ifndef HPCRUN_STATIC_LINK
  // dynamic libraries only availabile in non-static case
  CHK_DLOPEN(cuda, "libcuda.so", RTLD_NOW | RTLD_GLOBAL);

  CHK_DLSYM(cuda, cuDeviceGetAttribute); 
  CHK_DLSYM(cuda, cuCtxGetCurrent); 
  CHK_DLSYM(cuda, cuCtxSetCurrent); 

  CHK_DLOPEN(cudart, "libcudart.so", RTLD_NOW | RTLD_GLOBAL);

  CHK_DLSYM(cudart, cudaGetDevice);
  CHK_DLSYM(cudart, cudaRuntimeGetVersion);

  CHK_DLSYM(cuda, cuCtxGetStreamPriorityRange);

  CHK_DLSYM(cuda, cuStreamCreateWithPriority);

  CHK_DLSYM(cuda, cuStreamCreate);

  CHK_DLSYM(cuda, cuStreamSynchronize);

  CHK_DLSYM(cuda, cuMemcpyDtoHAsync);

  CHK_DLSYM(cuda, cuMemcpyHtoDAsync);

  CHK_DLSYM(cuda, cuModuleLoad);

  CHK_DLSYM(cuda, cuModuleGetFunction);

  CHK_DLSYM(cuda, cuLaunchKernel);

  CHK_DLSYM(cuda, cuFuncSetAttribute);

  CHK_DLSYM(cuda, cuMemHostAlloc);

  return 0;
#else
  return -1;
#endif // ! HPCRUN_STATIC_LINK
}


void
cuda_shared_mem_size_set
(
 CUfunction function,
 int size
)
{
  cuda_internal = true;
#ifndef HPCRUN_STATIC_LINK
  HPCRUN_CUDA_API_CALL(cuFuncSetAttribute, (function, CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, size));
#endif
  cuda_internal = false;
}


void
cuda_module_load
(
 CUmodule *module,
 const char *fname
)
{
  cuda_internal = true;
#ifndef HPCRUN_STATIC_LINK
  HPCRUN_CUDA_API_CALL(cuModuleLoad, (module, fname));
#endif
  cuda_internal = false;
}


void
cuda_module_function_get
(
 CUfunction *hfunc,
 CUmodule hmod,
 const char *name
)
{
  cuda_internal = true;
#ifndef HPCRUN_STATIC_LINK
  HPCRUN_CUDA_API_CALL(cuModuleGetFunction, (hfunc, hmod, name));
#endif
  cuda_internal = false;
}


void
cuda_kernel_launch
(
 CUfunction f,
 unsigned int gridDimX,
 unsigned int gridDimY,
 unsigned int gridDimZ,
 unsigned int blockDimX,
 unsigned int blockDimY,
 unsigned int blockDimZ,
 unsigned int sharedMemBytes,
 CUstream hStream,
 void **kernelParams
)
{
  cuda_internal = true;
#ifndef HPCRUN_STATIC_LINK
  HPCRUN_CUDA_API_CALL(cuLaunchKernel, (f, gridDimX, gridDimY, gridDimZ,
    blockDimX, blockDimY, blockDimZ, 0, hStream, kernelParams, NULL));
#endif
  cuda_internal = false;
}; 


CUstream
cuda_priority_stream_create
(
)
{
#ifndef HPCRUN_STATIC_LINK
  cuda_internal = true;
  int priority_high, priority_low;
  CUstream stream;
  HPCRUN_CUDA_API_CALL(cuCtxGetStreamPriorityRange,
    (&priority_low, &priority_high));
  HPCRUN_CUDA_API_CALL(cuStreamCreateWithPriority,
    (&stream, CU_STREAM_NON_BLOCKING, priority_high));
  cuda_internal = false;
  return stream;
#else
  return NULL;
#endif
}


CUstream
cuda_stream_create
(
)
{
#ifndef HPCRUN_STATIC_LINK
  cuda_internal = true;
  CUstream stream;
  HPCRUN_CUDA_API_CALL(cuStreamCreate,
    (&stream, CU_STREAM_NON_BLOCKING));
  cuda_internal = false;
  return stream;
#else
  return NULL;
#endif
}


void
cuda_stream_synchronize
(
 CUstream stream
)
{
#ifndef HPCRUN_STATIC_LINK
  cuda_internal = true;
  HPCRUN_CUDA_API_CALL(cuStreamSynchronize, (stream));
  cuda_internal = false;
#endif
}


void
cuda_memcpy_dtoh
(
 void *dst,
 CUdeviceptr src,
 size_t byteCount,
 CUstream stream
)
{
#ifndef HPCRUN_STATIC_LINK
  cuda_internal = true;
  HPCRUN_CUDA_API_CALL(cuMemcpyDtoHAsync, (dst, src, byteCount, stream));
  cuda_internal = false;
#endif
}


void
cuda_memcpy_htod
(
 CUdeviceptr dst,
 void *src,
 size_t byteCount,
 CUstream stream
)
{
#ifndef HPCRUN_STATIC_LINK
  cuda_internal = true;
  HPCRUN_CUDA_API_CALL(cuMemcpyHtoDAsync, (dst, src, byteCount, stream));
  cuda_internal = false;
#endif
}


static int 
cuda_device_sm_blocks_query
(
 int major,
 int minor
)
{
  switch(major) {
  case 7:
  case 6:
    return 32;
  default:
    // TODO(Keren): add more devices
    return 8;
  }
}


static int __attribute__((unused))
cuda_device_sm_schedulers_query
(
 int major, 
 int minor
)
{
  switch(major) {
  case 7:
    return 4;
  default:
    // TODO(Keren): add more devices
    return 8;
  }
}



// returns 0 on success
static int
cuda_device_compute_capability
(
  int device_id,
  int *major,
  int *minor
)
{
#ifndef HPCRUN_STATIC_LINK
  cuda_internal = true;
  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, device_id));
  cuda_internal = false;
  return 0;
#else
  return -1;
#endif 
}


// returns 0 on success
static int 
cuda_device_id
(
  int *device_id
)
{
#ifndef HPCRUN_STATIC_LINK
  cuda_internal = true;
  HPCRUN_CUDA_RUNTIME_CALL(cudaGetDevice, (device_id));
  cuda_internal = false;
  return 0;
#else
  return -1;
#endif
}


// returns 0 on success
static int
cuda_runtime_version
(
  int *rt_version
)
{
#ifndef HPCRUN_STATIC_LINK
  cuda_internal = true;
  HPCRUN_CUDA_RUNTIME_CALL(cudaRuntimeGetVersion, (rt_version));
  cuda_internal = false;
  return 0;
#else
  return -1;
#endif
}



//******************************************************************************
// interface operations
//******************************************************************************

int
cuda_context
(
 CUcontext *ctx
)
{
#ifndef HPCRUN_STATIC_LINK
  cuda_internal = true;
  HPCRUN_CUDA_API_CALL(cuCtxGetCurrent, (ctx));
  cuda_internal = false;
  return 0;
#else
  return -1;
#endif
}


int
cuda_context_set
(
 CUcontext ctx
)
{
#ifndef HPCRUN_STATIC_LINK
  cuda_internal = true;
  HPCRUN_CUDA_API_CALL(cuCtxSetCurrent, (ctx));
  cuda_internal = false;
  return 0;
#else
  return -1;
#endif
}


int
cuda_host_alloc
(
 void** pHost,
 size_t size
)
{
#ifndef HPCRUN_STATIC_LINK
  cuda_internal = true;
  HPCRUN_CUDA_API_CALL(cuMemHostAlloc, (pHost, size, CU_MEMHOSTALLOC_PORTABLE));
  cuda_internal = false;
  return 0;
#else
  return -1;
#endif
}

int
cuda_device_property_query
(
 int device_id,
 cuda_device_property_t *property
)
{
#ifndef HPCRUN_STATIC_LINK
  cuda_internal = true;

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_count, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_clock_rate, CU_DEVICE_ATTRIBUTE_CLOCK_RATE, device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_shared_memory,
     CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_MULTIPROCESSOR, device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_registers,
     CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_MULTIPROCESSOR, device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_threads, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR,
     device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->num_threads_per_warp, CU_DEVICE_ATTRIBUTE_WARP_SIZE,
     device_id));

  int major = 0, minor = 0;

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, device_id));

  property->sm_blocks = cuda_device_sm_blocks_query(major, minor);

  property->sm_schedulers = cuda_device_sm_schedulers_query(major, minor);

  cuda_internal = false;

  return 0;
#else
  return -1;
#endif
}


static bool
cuda_write_cubin
(
 const char *file_name,
 const void *cubin,
 size_t cubin_size
)
{
  int fd;
  errno = 0;
  fd = open(file_name, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (errno == EEXIST) {
    close(fd);
    return true;
  }
  if (fd >= 0) {
    // Success
    if (write(fd, cubin, cubin_size) != cubin_size) {
      close(fd);
      return false;
    } else {
      close(fd);
      return true;
    }
  } else {
    // Failure to open is a fatal error.
    hpcrun_abort("hpctoolkit: unable to open file: '%s'", file_name);
    return false;
  }
}


void
cuda_load_callback
(
 uint32_t cubin_id, 
 const void *cubin, 
 size_t cubin_size
)
{
  // Compute hash for cubin and store it into a map
  cubin_hash_map_entry_t *entry = cubin_hash_map_lookup(cubin_id);
  unsigned char *hash;
  unsigned int hash_len;
  if (entry == NULL) {
    cubin_hash_map_insert(cubin_id, cubin, cubin_size);
    entry = cubin_hash_map_lookup(cubin_id);
  }
  hash = cubin_hash_map_entry_hash_get(entry, &hash_len);

  // Create file name
  size_t i;
  size_t used = 0;
  char file_name[PATH_MAX];
  used += sprintf(&file_name[used], "%s", hpcrun_files_output_directory());
  used += sprintf(&file_name[used], "%s", "/cubins/");
  mkdir(file_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  for (i = 0; i < hash_len; ++i) {
    used += sprintf(&file_name[used], "%02x", hash[i]);
  }
  used += sprintf(&file_name[used], "%s", ".cubin");

  // Write a file if does not exist
  bool file_flag;
  spinlock_lock(&files_lock);
  file_flag = cuda_write_cubin(file_name, cubin, cubin_size);
  spinlock_unlock(&files_lock);

  if (file_flag) {
    char device_file[PATH_MAX];
    sprintf(device_file, "%s", file_name);
    uint32_t hpctoolkit_module_id;
    load_module_t *module = NULL;
    hpcrun_loadmap_lock();
    if ((module = hpcrun_loadmap_findByName(device_file)) == NULL) {
      hpctoolkit_module_id = hpcrun_loadModule_add(device_file);
    } else {
      hpctoolkit_module_id = module->id;
    }
    hpcrun_loadmap_unlock();
    cubin_id_map_entry_t *entry = cubin_id_map_lookup(cubin_id);
    if (entry == NULL) {
      Elf_SymbolVector *vector = computeCubinFunctionOffsets(cubin, cubin_size);
      cubin_id_map_insert(cubin_id, hpctoolkit_module_id, vector);
    }
  }
}


void
cuda_unload_callback
(
 uint32_t cubin_id
)
{
}


int
cuda_global_pc_sampling_required
(
  int *required
)
{
  int device_id;
  if (cuda_device_id(&device_id)) return -1;

  int dev_major, dev_minor;
  if (cuda_device_compute_capability(device_id, &dev_major, &dev_minor)) return -1;

  int rt_version;
  if (cuda_runtime_version(&rt_version)) return -1;

#ifdef DEBUG
  printf("cuda_global_pc_sampling_required: "
         "device major = %d minor = %d cuda major = %d\n", 
         dev_major, dev_minor, RUNTIME_MAJOR_VERSION(rt_version));
#endif

  *required = ((DEVICE_IS_TURING(dev_major, dev_minor)) && 
               (RUNTIME_MAJOR_VERSION(rt_version) < CUDA11));

  return 0;
}


static int
cuda_path_exist
(
 struct dl_phdr_info *info,
 size_t size,
 void *data
)
{
  char *buffer = (char *) data;
  const char *suffix = strstr(info->dlpi_name, "libcudart");
  if (suffix) {
    // CUDA library organization after 9.0
    suffix = strstr(info->dlpi_name, "targets");
    if (!suffix) {
      // CUDA library organization in 9.0 or earlier
      suffix = strstr(info->dlpi_name, "lib64");
    }
  }
  if (suffix){
    int len = suffix - info->dlpi_name;
    strncpy(buffer, info->dlpi_name, len);
    buffer[len] = 0;
    return 1;
  }
  return 0;
}


int
cuda_path
(
 char *buffer
)
{
  return dl_iterate_phdr(cuda_path_exist, buffer);
}


bool
cuda_api_internal
(
)
{
  return cuda_internal;
}
