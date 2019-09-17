//
// Created by aleksa on 8/23/19.
//

#ifndef HPCTOOLKIT_GPU_CORRELATION_ID_MAP_H
#define HPCTOOLKIT_GPU_CORRELATION_ID_MAP_H

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/cct/cct.h>
#include "gpu-record.h"

/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef struct gpu_correlation_id_map_entry_s gpu_correlation_id_map_entry_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

gpu_correlation_id_map_entry_t *
gpu_correlation_id_map_lookup
        (
                uint32_t id
        );


void
gpu_correlation_id_map_insert
        (
                uint32_t correlation_id,
                uint64_t external_id
        );


void
gpu_correlation_id_map_delete
        (
                uint32_t correlation_id
        );


void
gpu_correlation_id_map_external_id_replace
        (
                uint32_t correlation_id,
                uint64_t external_id
        );


void
gpu_correlation_id_map_kernel_update
        (
                uint32_t correlation_id,
                uint32_t device_id,
                uint64_t start,
                uint64_t end
        );


uint64_t
gpu_correlation_id_map_entry_external_id_get
        (
                gpu_correlation_id_map_entry_t *entry
        );


uint64_t
gpu_correlation_id_map_entry_start_get
        (
                gpu_correlation_id_map_entry_t *entry
        );


uint64_t
gpu_correlation_id_map_entry_end_get
        (
                gpu_correlation_id_map_entry_t *entry
        );


uint32_t
gpu_correlation_id_map_entry_device_id_get
        (
                gpu_correlation_id_map_entry_t *entry
        );



#endif //HPCTOOLKIT_GPU_CORRELATION_ID_MAP_H
