#ifndef __JXR_SYS_HELPERS__H__
#define __JXR_SYS_HELPERS__H__

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

typedef struct
{
    int argc;
    wchar_t** argv;
} jxr_command_line;

uint32_t jxr_get_number_of_processors(void);

char* jxr_get_error_description(int code);

void jxr_free_error_description(char* desc);

int jxr_get_command_line(int argc, char* argv[], jxr_command_line* cmdline);

void jxr_free_command_line(jxr_command_line* cmdline);

int jxr_write_data_to_file(const wchar_t* filename, void* buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif // __JXR_SYS_HELPERS__H__
