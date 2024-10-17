// Copyright 2024 Dmitry Ignatiev. All rights reserved

#ifndef __JXR_DATA_H__
#define __JXR_DATA_H__

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

typedef struct
{
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    size_t buffer_size;
    uint8_t bytes_per_pixel;
    uint8_t* pixels;
} jxr_data;

int jxr_load_data(const wchar_t* filename, jxr_data* data);

void jxr_free_data(jxr_data* data);

int jxr_init_loader_thread(void);

void jxr_deinit_loader_thread(void);

#ifdef __cplusplus
}
#endif

#endif // __JXR_DATA_H__
