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
// Copyright ((c)) 2002-2019, Rice University
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


#ifndef _HPCTOOLKIT_GPU_NVIDIA_SANITIZER_BUFFER_H_
#define _HPCTOOLKIT_GPU_NVIDIA_SANITIZER_BUFFER_H_

#include <stddef.h>
#include <lib/prof-lean/stdatomic.h>

#include <gpu-patch.h>

typedef struct sanitizer_buffer_channel_t sanitizer_buffer_channel_t;

typedef struct sanitizer_buffer_t sanitizer_buffer_t;


void
sanitizer_buffer_process
(
 sanitizer_buffer_t *b
);


sanitizer_buffer_t *
sanitizer_buffer_alloc
(
 sanitizer_buffer_channel_t *channel
);


void
sanitizer_buffer_produce
(
 sanitizer_buffer_t *b,
 uint32_t thread_id,
 uint32_t cubin_id,
 uint32_t mod_id,
 int32_t kernel_id,
 uint64_t host_op_id,
 uint32_t type,
 size_t num_records,
 atomic_uint *balance,
 bool async
);


void
sanitizer_buffer_free
(
 sanitizer_buffer_channel_t *channel, 
 sanitizer_buffer_t *b,
 atomic_uint *balance
);


gpu_patch_buffer_t *
sanitizer_buffer_entry_gpu_patch_buffer_get
(
 sanitizer_buffer_t *b
);

#endif
