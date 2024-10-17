// Copyright 2024 Dmitry Ignatiev. All rights reserved

#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <wincodec.h>
#include "jxr_data.h"

#define V_HR() do{if(FAILED(hr)) { goto exit; }}while(0)

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) do{if(p){(p)->lpVtbl->Release(p); (p) = NULL;}}while(0)
#endif

int get_jxr_data(const wchar_t* filename, jxr_data* data)
{
    IWICImagingFactory* pFactory = NULL;
    IWICBitmapFrameDecode* pFrame = NULL;
    IWICBitmapDecoder* pDecoder = NULL;
    IWICBitmapSource* pBitmapSource = NULL;

    memset(data, 0, sizeof(jxr_data));

    HRESULT hr = CoCreateInstance(
        &CLSID_WICImagingFactory,
        NULL,
        CLSCTX_INPROC_SERVER,
        &IID_IWICImagingFactory,
        (void**)&pFactory);

    V_HR();

    hr = pFactory->lpVtbl->CreateDecoderFromFilename(
        pFactory,
        filename,                        // Image to be decoded
        NULL,                            // Do not prefer a particular vendor
        GENERIC_READ,                    // Desired read access to the file
        WICDecodeMetadataCacheOnDemand,  // Cache metadata when needed
        &pDecoder                        // Pointer to the decoder
    );

    V_HR();

    hr = pDecoder->lpVtbl->GetFrame(pDecoder, 0, &pFrame);

    V_HR();

    hr = pFrame->lpVtbl->QueryInterface(pFrame, &IID_IWICBitmapSource, (void**)&pBitmapSource);

    V_HR();

    WICPixelFormatGUID pixelFormat;

    hr = pBitmapSource->lpVtbl->GetPixelFormat(pBitmapSource, &pixelFormat);

    V_HR();

    if(IsEqualGUID(&pixelFormat, &GUID_WICPixelFormat128bppRGBAFloat))
    {
        data->bytes_per_pixel = 4 * 4;
    }
    else if(IsEqualGUID(&pixelFormat, &GUID_WICPixelFormat64bppRGBAHalf))
    {
        data->bytes_per_pixel = 2 * 4;
    }
    else
    {
        hr = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        goto exit;
    }

    hr = pBitmapSource->lpVtbl->GetSize(pBitmapSource, &data->width, &data->height);

    V_HR();

    data->stride = data->width * data->bytes_per_pixel;
    data->buffer_size = (size_t)data->stride * (size_t)data->height;

    data->pixels = (uint8_t*)malloc(data->buffer_size);
    if(!data->pixels)
    {
        hr = HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY);
        goto exit;
    }

    WICRect rc;
    rc.Y = 0;
    rc.X = 0;
    rc.Width = (int)data->width;
    rc.Height = (int)data->height;
    hr = pBitmapSource->lpVtbl->CopyPixels(pBitmapSource, &rc, data->stride, (UINT)data->buffer_size, data->pixels);

    V_HR();

    hr = S_OK;

exit:
    SAFE_RELEASE(pFactory);
    SAFE_RELEASE(pDecoder);
    SAFE_RELEASE(pFrame);
    SAFE_RELEASE(pBitmapSource);

    if(FAILED(hr))
    {
        free_jxr_data(data);
    }

    return hr;
}

void free_jxr_data(jxr_data* data)
{
    if(data && data->pixels)
    {
        free(data->pixels);
    }
    memset(data, 0, sizeof(jxr_data));
}

wchar_t* get_jxr_error_description(int code)
{
    static const wchar_t defaultMessage[] = L"Unidentified error.";
    LPWSTR buffer = NULL;
    DWORD rv = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        (DWORD)code,
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        (LPWSTR)&buffer,
        0,
        NULL);
    if(!rv)
    {
        buffer = LocalAlloc(LMEM_ZEROINIT, sizeof(defaultMessage));
        wcscpy_s(buffer, ARRAYSIZE(defaultMessage) - 1, defaultMessage);
    }
    return buffer;
}

void free_jxr_error_description(wchar_t* desc)
{
    if (desc)
        LocalFree(desc);
}
