
#ifndef __FILE_UTIL_H__
#define __FILE_UTIL_H__

#include <stdint.h>
#include <stdlib.h>
#include <arpa/inet.h>  // Net<=>Host ordering macros

#define FILETIFF_COMPONENT_ORDER_NORMAL	0
#define FILETIFF_COMPONENT_ORDER_REVERSE	1
#define FILETIFF_COMPONENT_ORDER_PLANAR	2

#define FILETIFF_ERROR_NOT_SUPPORTED   -1100 // Image file format support library (libtiff) not present.
#define FILETIFF_ERROR_FILE_ACCESS     -1101 // An error occurred creating the file for writing.
#define FILETIFF_ERROR_NULL_PTR			-1102 // A pointer passed in is NULL.
#define FILETIFF_ERROR_BAD_BUFFER		-1103 // Buffer has invalid size (too small) to containt the image.
#define FILETIFF_ERROR_BAD_TIFF_PARAMS -1104 // Invalid values for the TIFF components settings were specified.
#define FILETIFF_ERROR_WRITE_FAILED    -1105 // The actual write to the file failed.
#define FILETIFF_ERROR_READ_FAILED     -1106 // The actual read from the file failed.
#define FILETIFF_ERROR_BAD_TIFF_FILE   -1107 // Something unexpected in the TIFF file (spp incorrect, palette with bps > 8,  etc...)

#if defined(LIBTIFF_AVAILABLE)

#ifdef __cplusplus
extern "C" {
#endif

#include "tiffio.h"


int File_GetTIFFInfo( char *filename, uint32_t *width, uint32_t *height, int *bitdepth, int *num_components );

// Functions to support both Monochrome and 8/16 bit RGB/RGBA and reversed BGR/BGRA 
int File_ReadFromTIFF( char *filename, uint32_t *width, uint32_t *height, int *num_components, int *bits_per_component, int reverse_order, int size, void *imageData);
int File_WriteToTIFF( char *filename, uint32_t width, uint32_t height, uint32_t num_components, uint32_t component_depth, uint32_t component_order, int size, void *imageData);

// Legacy functions - fine for Monochrome and 8 bit RGB/RGBA
int File_ReadTIFF( char *filename, uint32_t *width, uint32_t *height, uint32_t *depth, int *color, int size, void *data);
int File_WriteTIFF( char *filename, uint32_t width, uint32_t height, uint32_t depth, int color, void *imageData );

#ifdef __cplusplus
}
#endif


#endif


#endif
