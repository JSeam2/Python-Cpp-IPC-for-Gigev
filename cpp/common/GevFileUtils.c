/*
  ---------------------------------------------
  GigE-V Framework Image File Utility Functions
  -----------------------------------------------
*/

#include "SapX11Util.h"
#include "gevapi.h"
#include "PFNC.h"
#include "FileUtil.h"

// ! 
// Read_TIFF_ToGevImage
//
/*! 
	Read the input TIFF file to the image data buffer (of type "pixel_format").
	(Read Mono files to Mono formats, RGB files to 3 component color formats, RGBA files to 4 component color formats).

	\param filename      Name of the file to be read (string).
	\param width         Pointer to storage to hold the Width (in pixels) of the image read.
	\param height        Pointer to storage to hold the Height (in rows of pixels) of the image read. 
	\param pixel_format  Pixel format desired for the image data after it is read.
	\param size          The size of the buffer to hold the image read.
	\param imageData		Pointer to the storage for the image to be written.
								(It is assumed that the size is correct for the given image parameters).
	
	\return Error status
		0   = Success
		GEVLIB_ERROR_NULL_PTR              Data pointer is NULL.
		GEVLIB_ERROR_INVALID_PIXEL_FORMAT  Invalid pixel format. The specified pixel format is not supported.
									              (Packed pixel formats cannot be saved to TIFF).
		(Errors from TIFF functions - see FileUtil.h)

*/
int Read_TIFF_ToGevImage( char *filename, uint32_t *width, uint32_t *height, int pixel_format, int size, void *imageData)
{
	int ret = GEVLIB_ERROR_NULL_PTR;	
#if defined(LIBTIFF_AVAILABLE)
	
	if (imageData != NULL)
	{
		int num_components, component_order;
		int component_depth = 0;
		ret = 0;
		
		// Set up the desired component_order.
		switch( pixel_format )
		{
			case CORDATA_FORMAT_RGB8888: 
			case fmtBGR8Packed:
			case fmtBGRA8Packed:
			case fmtBGR10Packed:
			case fmtBGR12Packed:
			case PFNC_BGR14:
			case PFNC_BGR16:
				component_order = FILETIFF_COMPONENT_ORDER_REVERSE;
				break;
			case fmtRGB8Planar:	
			case fmtRGB10Planar:	
			case fmtRGB12Planar:	
			case fmtRGB16Planar:	
				// This is an error!!! Cannot read into Planar or YUV
				ret = GEVLIB_ERROR_INVALID_PIXEL_FORMAT;
				break;
			default:
				if ( GevIsPixelTypePacked(pixel_format) )
				{
					// Cannot read into a packed format.
					ret = GEVLIB_ERROR_INVALID_PIXEL_FORMAT;
				}
				else
				{
					component_order = FILETIFF_COMPONENT_ORDER_NORMAL;
				}
				break;
		}
		
		if ( ret == 0)
		{
			int reverse = (component_order == FILETIFF_COMPONENT_ORDER_REVERSE) ? 1 : 0;
			ret = File_ReadFromTIFF( filename, width, height, &num_components, &component_depth, reverse, size, imageData);
			
			// Check if we got the format correct
			// (Component depth is either 8 or 16 bits.)
			if ( ((GevGetPixelDepthInBits(pixel_format) + 7)/8) == component_depth)
			{
				// See if we have to shift any pixel components to match the pixel format.
				int shift = (component_depth == 16) ? (component_depth - GevGetPixelDepthInBits(pixel_format)) : 0;
				
				if (shift != 0)
				{
					// Shift all the input pixels to match the specified input GigeVision pixel format.
					int i = 0;
					uint16_t *ptr = imageData;
					for (i = 0; i < size; i++)
					{
						*ptr++ >>= shift;
					}
				}
			}
			else
			{
				ret = GEVLIB_ERROR_INVALID_PIXEL_FORMAT;
			}
		}
	}
#else
	ret = FILETIFF_ERROR_NOT_SUPPORTED;
#endif
	return ret;
}


// ! 
// Write_GevImage_ToTIFF
//
/*! 
	Write the input image data (of type "pixel_format") to the specified TIFF file.

	\param filename      Name of the file to be written (string).
	\param width         The Width (in pixels) of the image to be written. 
	\param height        The Height (in rows of pixels) of the image to be written. 
	\param pixel_format  Pixel format for the image data.
	\param imageData		Pointer to the storage for the image to be written.
								(It is assumed that the size is correct for the given image parameters).
	
	\return Error status
		> 0   = Success (Number of bytes written to the file).
		GEVLIB_ERROR_NULL_PTR              Data pointer is NULL.
		GEVLIB_ERROR_INVALID_PIXEL_FORMAT  Invalid pixel format. The specified pixel format is not supported.
									              (Packed pixel formats cannot be saved to TIFF).
		(Errors from TIFF functions - see FileUtil.h)

*/
int Write_GevImage_ToTIFF( char *filename, uint32_t width, uint32_t height, uint32_t pixel_format, void *imageData)
{
	int ret = GEVLIB_ERROR_NULL_PTR;
#if defined(LIBTIFF_AVAILABLE)

	if (imageData != NULL)
	{
		uint32_t num_components, component_depth, component_order;
		uint32_t size;
		
		// Set up parameters based on the input pixel_format.
		component_depth = GevGetPixelDepthInBits(pixel_format);
		if ( GevIsPixelTypeMono(pixel_format) )
		{
			num_components = 1;
			//component_depth = GevGetPixelDepthInBits(pixel_format);
			component_order = 0;
		}
		else
		{
			// Color formats
			switch (pixel_format)
			{
				case fmtRGB8Packed:
					num_components  = 3;
					//component_depth = 8;
					component_order = FILETIFF_COMPONENT_ORDER_NORMAL;
					break;
				
				case fmtRGBA8Packed:
					num_components  = 4;
					//component_depth = 8;
					component_order = FILETIFF_COMPONENT_ORDER_NORMAL;
					break;

				case fmtRGB10Packed:
				case fmtRGB12Packed:
				case PFNC_RGB14:
				case PFNC_RGB16:
					num_components  = 3;
					//component_depth = 16;
					component_order = FILETIFF_COMPONENT_ORDER_NORMAL;
					break;

				case fmtBGR8Packed:
					num_components  = 3;
					//component_depth = 8;
					component_order = FILETIFF_COMPONENT_ORDER_REVERSE;
					break;
					
				case CORDATA_FORMAT_RGB8888: 
				case fmtBGRA8Packed:
					num_components  = 4;
					//component_depth = 8;
					component_order = FILETIFF_COMPONENT_ORDER_REVERSE;
					break;

				case fmtBGR10Packed:
				case fmtBGR12Packed:
				case PFNC_BGR14:
				case PFNC_BGR16:
					num_components  = 3;
					//component_depth = 16;
					component_order = FILETIFF_COMPONENT_ORDER_REVERSE;
					break;

				case fmtRGB8Planar:
					num_components  = 3;
					//component_depth = 8;
					component_order = FILETIFF_COMPONENT_ORDER_PLANAR;
					break;

				case fmtRGB10Planar:
				case fmtRGB12Planar:
				case fmtRGB16Planar:
					num_components  = 3;
					//component_depth = 16;
					component_order = FILETIFF_COMPONENT_ORDER_PLANAR;
					break;

				// The Raw "BiColor" formats are treated as Monochrome here.
				// (They need conversion to RGB to be used for color).
				
				case fmt_PFNC_BiColorBGRG10:   // Bi-color Blue/Green - Red/Green 10-bit unpacked 
				case fmt_PFNC_BiColorBGRG12:   // Bi-color Blue/Green - Red/Green 12-bit unpacked 
				case fmt_PFNC_BiColorRGBG10:   // Bi-color Red/Green - Blue/Green 10-bit unpacked 
				case fmt_PFNC_BiColorRGBG12:   // Bi-color Red/Green - Blue/Green 12-bit unpacked 
					num_components  = 1;
					//component_depth = 16;
					component_order = FILETIFF_COMPONENT_ORDER_NORMAL;
					break;
				
				case fmt_PFNC_BiColorBGRG8:   
				case fmt_PFNC_BiColorRGBG8:    
				default:							/* Handle default as Mono 8 bit */
					num_components  = 1;
					component_depth = 8;
					component_order = FILETIFF_COMPONENT_ORDER_NORMAL;
					break;
			}
		}
		
		// Write the TIFF file based on the input format.
		size = height * width * num_components * ((component_depth+7)/8);		
		ret = File_WriteToTIFF( filename, width, height, num_components, component_depth, component_order, size, imageData);
	
	}
#else
	ret = FILETIFF_ERROR_NOT_SUPPORTED;
#endif
	return ret;
}
	
