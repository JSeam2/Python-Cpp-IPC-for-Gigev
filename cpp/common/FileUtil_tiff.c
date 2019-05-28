// Utility functions for writing and reading TIFF files.
//
// File_GetTIFFInfo  -> Gets the dimensions, pixel component depth (bits), and the number of pixel components (1 for Mono, 3 or 4 for Color).
//
// File_ReadFromTIFF -> Reads the file into the buffer passed in by pointer with optional color component reversal (RGB to BGR). 
//                      Returns the dimensions, # of pixel components, and depth of pixel components.
//
// File_WriteToTIFF  -> Writes the image to the TIFF file given the dimensions,number of pixel components, depth of pixel components, and component layout.
//                      Color TIFF files are RGB/RGBA. The component layout specifes Normal, Reverse (BGR/BGRA), or Planar. 
//
// Legacy:
// File_ReadTIFF     -> Reads the file into the data area passed in by pointer. Returns the dimensions, pixel depth (bytes)/number of color components (for color), and a flag indicating if the image is color.
// File_WriteTIFF    -> Writes the image to the TIFF file given the dimensions, pixel depth (bytes)/number of color components (for color), a flag indicating if the image is color and a pointer to the image data.
//
// Note : It supports 
// 	- Monochrome images (depth = 1 for 8 bit data, depth = 2 for 10,12,14,16 bit data)
// 	- Color images (8 bits per pixels component : depth = 3 for RGB888, depth = 4 for RGB8888/RGBA8)
//
// Return values are :
// -1 : Unable to open the image (permissions or file not found).
// -2 : Bad parameter passed in (usually a NULL pointer when one was expected).
// -3 : Reading this TIFF file is not supported (usually the "photometric" tag is not supported).
// -4 : Writing this TIFF file is not supported (usually the depth is wrong (10 or 16 bit color color components).
// others : passed from TIFF library.
//

#if defined(LIBTIFF_AVAILABLE)

#include "FileUtil.h"

int File_GetTIFFInfo( char *filename, uint32_t *width, uint32_t *height, int *bitdepth, int *components )
{
	TIFF *image;

	// Open the TIFF image
	if((image = TIFFOpen(filename, "r")) != NULL)
	{
		if ( (width != NULL) && (height != NULL) && (bitdepth != NULL) && (components != NULL))
		{
			uint16_t bps, spp, photometric;
			
			// Get the tag information
			TIFFGetField(image, TIFFTAG_IMAGEWIDTH, width);
			TIFFGetField(image, TIFFTAG_IMAGELENGTH, height);
			
			TIFFGetField(image, TIFFTAG_SAMPLESPERPIXEL, &spp);
			TIFFGetField(image, TIFFTAG_BITSPERSAMPLE, &bps);
			TIFFGetField(image, TIFFTAG_PHOTOMETRIC, &photometric);
			
			switch (photometric)
			{
				case PHOTOMETRIC_MINISBLACK:
				case PHOTOMETRIC_MINISWHITE:
					*components = 1;
					break;
					
				case PHOTOMETRIC_RGB:
				case PHOTOMETRIC_PALETTE:
					*components = spp;
					break;
					
				default:
					TIFFClose(image);
					return -3;
			}
			
			*bitdepth = bps;
			TIFFClose(image);
			return 0;	
		}
		else
		{
			// Bad parameter.
			TIFFClose(image);
			return -2;
		}
	}
	// No such file.
	return -1;  	
}


// Set up the palette (palette is used for mapping colors into subset of 256 levels).
static void _SetupTIFFPalette( TIFF *image, uint16_t *redmap, uint16_t *greenmap, uint16_t *bluemap)
{
	uint16_t *rmap = NULL;
	uint16_t *gmap = NULL;
	uint16_t *bmap = NULL;
	int i;
							
	// Set up colormap
	TIFFGetField(image, TIFFTAG_COLORMAP, &rmap, &gmap, &bmap);
	if ( (rmap != NULL) && (gmap != NULL) && (bmap != NULL) )
	{
		for ( i = 0; i < 256; i++)
		{
			redmap[i]   = (int) (rmap[i] * 256) /  65535L;
			greenmap[i] = (int) (gmap[i] * 256) /  65535L;
			bluemap[i]  = (int) (bmap[i] * 256) /  65535L;
		}
	}
	else
	{
		// Should not have an error - make a unity colormap
		for ( i = 0; i < 256; i++)
		{
			redmap[i]   = i;
			greenmap[i] = i;
			bluemap[i]  = i;
		}
	}
}

// ! 
// File_ReadFromTIFF
//
/*! 
	Read a TIFF file into the supplied buffer with optional color component re-ordering.
	Information on image is returned as well (width, height, pixel component information).

	\param filename        Name of the file to be written (string).
	\param width           Pointer to storage to hold the Width (in pixels) of the image read.
	\param height          Pointer to storage to hold the Height (in rows of pixels) of the image read. 
	\param num_components  Pointer to storage to hold the number of pixel components found (1 for Mono, 3 for RGB, 4 for RGBA).
	\param component_depth Pointer to storage to hold the number of bits for each pixel component.
	\param reverse_order   The order to store the pixel components in the buffer (0 for RGB/RGBA, 1 for BGR/BGRA)
	\param size            The size of the buffer to hold the image read.
	\param imageData       Pointer to the storage for the image to be read into.
	
	\return Error status
		> 0 = Success (on success, the number of bytes read from the file).
	
		FILETIFF_ERROR_FILE_ACCESS     An error occurred opening the file for reading.
		FILETIFF_ERROR_NULL_PTR 		 A pointer passed in is NULL.
		FILETIFF_ERROR_BAD_BUFFER		 Buffer has invalid size (too small) to containt the image.
		FILETIFF_ERROR_BAD_TIFF_PARAMS Invalid values for the TIFF components settings were specified.
		FILETIFF_ERROR_READ_FAILED     The actual read from the file failed.
		FILETIFF_ERROR_BAD_TIFF_FILE   Something unexpected in the TIFF file (spp incorrect, palette with bps > 8,  etc...)

	\Notes: 
		Although TIFF supports planar images (PLANARCONFIG_SEPARATE) it is not widely used and 
		TIFF readers are not required to support it. This one does not.
*/
int File_ReadFromTIFF( char *filename, uint32_t *width, uint32_t *height, int *num_components, int *bits_per_component, int reverse_order, int size, void *imageData)
{
	TIFF *image;
	int samples = 0;
	int bitdepth = 0;
	int endianswap = 0;  // Network order is BIG ENDIAN. (x86 is LITTLE ENDIAN).
	int ret = -1;

	if ( (filename == NULL) || (width == NULL) || (height == NULL) || (num_components == NULL) \
			|| (bits_per_component == NULL) || (imageData == NULL) )
	{
		return FILETIFF_ERROR_NULL_PTR;
	}

	ret = File_GetTIFFInfo( filename, width, height, &bitdepth, &samples);
	if (ret == 0)
	{
		uint32_t imgsize;
		imgsize = (*width)*(*height)*samples*((bitdepth + 7)/8);
		
		if (imgsize <= size)
		{
			return FILETIFF_ERROR_BAD_BUFFER;
		}
		
		// Check for endianness of file (and host) to see if we need to 
		// swap when component bitdepths are larger than 8.
		if (bitdepth > 8)
		{
			FILE *fp = fopen(filename, "r");
			if (fp != NULL)
			{
				uint16_t hdr = 0;
				fread(&hdr, sizeof(hdr), 1, fp);
				if (hdr == 0x4d4d)
				{
					endianswap = 1;
				}
				fclose(fp);
			}
		}
		
		// Open the TIFF image
		if((image = TIFFOpen(filename, "r")) != NULL)
		{
			// See if we need to go line by line.
			if ( (samples == 4) && !endianswap && !reverse_order && (bitdepth == 8) ) 
			{
				// RGBA with no endianswap and no component order reversal.
				ret = TIFFReadRGBAImageOriented( image, *width, *height, imageData, ORIENTATION_TOPLEFT, 1);
				if (ret == 0) ret = FILETIFF_ERROR_READ_FAILED;
				if (ret == 1) ret = 0;
			}
			else if ( (samples == 4) || (samples == 3) )
			{
				// RGBA or RGB
				// RGB - Do it a line at a time (check palette)
				uint16_t photometric;
				uint32_t lineSize = TIFFScanlineSize(image);
				uint16_t redcolormap[256], greencolormap[256], bluecolormap[256];
				void *buf = NULL;
				int i, j;
				int use_alpha = (samples == 4) ? 1 : 0;
					
				// Check for a palette.
				TIFFGetField(image, TIFFTAG_PHOTOMETRIC, &photometric);
				if (photometric == PHOTOMETRIC_PALETTE)
				{
					_SetupTIFFPalette( image, redcolormap, greencolormap, bluecolormap);
				}

				// Need a line buffer.
				buf = malloc(lineSize);
				if (buf != NULL)
				{
					if ( bitdepth == 8 )
					{
						uint8_t *rptr  = (uint8_t *)imageData;
						uint8_t *gptr  = rptr+1;
						uint8_t *bptr  = gptr+1;
						uint8_t *alpha = bptr+1;
							
						if (reverse_order)
						{
							// Swap blue and red.
							rptr = bptr;
							bptr = (uint8_t *)imageData;
						} 
							
						for (i = 0; i < *height; i++)
						{								
							ret = TIFFReadScanline(image, buf, i, 0);
							if (ret < 0)
							{
								ret = FILETIFF_ERROR_READ_FAILED;
								break;
							}
							else
							{
								uint8_t *component = (uint8_t *)buf;
									
								// Handle the RGB for this row
								for (j = 0; j < *width; j++)
								{									
									if (photometric == PHOTOMETRIC_PALETTE)
									{
										// Lookup via palette.
										*rptr++ = (uint8_t) redcolormap[*component];
										*gptr++ = (uint8_t) greencolormap[*component]; 
										*bptr++ = (uint8_t) redcolormap[*component];
										component++;
									}
									else
									{
										// RGB (8 bit components ONLY).
										*rptr++ = *component++;
										*gptr++ = *component++; 
										*bptr++ = *component++;
									}
									if (use_alpha) 
									{
										*alpha++ = *component++;
									}
								}
								ret = 0;
							}						
						}
					}
					else
					{
						// Same thing with 16 bit data.
						uint16_t *rptr  = (uint16_t *)imageData;
						uint16_t *gptr  = rptr+1;
						uint16_t *bptr  = gptr+1;
						uint16_t *alpha = bptr+1;
						
						if (reverse_order)
						{
							// Swap blue and red.
							rptr = bptr;
							bptr = (uint16_t *)imageData;
						} 
														
						if (photometric == PHOTOMETRIC_PALETTE)
						{
							// No palette support for bitdepth > 8.
							ret = FILETIFF_ERROR_BAD_TIFF_FILE;
						}
						else
						{
							for (i = 0; i < *height; i++)
							{								
								ret = TIFFReadScanline(image, buf, i, 0);
								if (ret < 0)
								{
									ret = FILETIFF_ERROR_READ_FAILED;
									break;
								}
								else
								{
									uint16_t *component = (uint16_t *)buf;
									
									// Handle the RGB for this row
									for (j = 0; j < *width; j++)
									{									
										// RGB (16 bit components ONLY).
										if (endianswap)
										{
											*rptr++ = ntohs(*component++);
											*gptr++ = ntohs(*component++); 
											*bptr++ = ntohs(*component++);
										}
										else
										{
											*rptr++ = *component++;
											*gptr++ = *component++; 
											*bptr++ = *component++;
										}
									}
									if (use_alpha) 
									{
										*alpha++ = *component++;
									}
									ret = 0;									
								}
							}						
						}
					}
					// Free line buffer.
					free(buf);
				}
				else
				{
					// Error allocating (assume a BAD SIZE)
					// (If "out of memory" program will die anyway).
					ret = FILETIFF_ERROR_BAD_TIFF_FILE;
				}
			}
			else if ( samples == 1 )
			{
				// Monochrome
				int i, j;
				ret = 0;
				// Grayscale image - do it a row at a time.
				// (It's either 8 bit or 16 bit buffer)
				if ( bitdepth == 8 )
				{
					uint8_t *cptr = (uint8_t *)imageData;
					for (i = 0; i < *height; i++)
					{
						if ( 1 == TIFFReadScanline(image, cptr, i, 0) )
						{
							cptr += *width;
						}
						else
						{
							ret = FILETIFF_ERROR_READ_FAILED;
							break;
						}
					}
				}
				else
				{
					// 16-bit data.
					uint16_t *sptr = (uint16_t *)imageData;
					
					for (i = 0; i < *height; i++)
					{
						if ( 1 == TIFFReadScanline(image, sptr, i, 0) )
						{
							if (endianswap)
							{
								uint16_t val = 0;
								for (j = 0; j < *width; j++)
								{
									val = sptr[j];
									sptr[j] = ntohs(val);
								}
							}
							sptr += *width;
						}
						else
						{
							ret = FILETIFF_ERROR_READ_FAILED;
							break;
						}
					}
				}
			}
			else
			{
				ret = FILETIFF_ERROR_BAD_TIFF_FILE;
			}
			if (ret == 0) ret = imgsize;
		}
	}
	return ret;
}

// ! 
// File_WriteToTIFF
//
/*! 
	Write the input image data (of type "pixel_format") to the specified TIFF file.

	\param filename        Name of the file to be written (string).
	\param width           The Width (in pixels) of the image to be written. 
	\param height          The Height (in rows of pixels) of the image to be written. 
	\param num_components  The number of pixel components (1 for Mono, 3 for RGB, 4 for RGBA).
	\param component_depth The number of bits for each pixel component.
	\param component_order The arrangement of the components in the pixel (0=Normal(RGBRGBA), 1=Reverse(BGR,BGRA), 2=Planar.
	\param size            The size of the buffer containing the image to be written.
	\param imageData       Pointer to the storage for the image to be written.
	
	\return Error status
		> 0 = Success (on success, the number of bytes written to the file).
	
		FILETIFF_ERROR_FILE_ACCESS     An error occurred creating the file for writing.
		FILETIFF_ERROR_BAD_BUFFER		 Buffer pointer is NULL or has invalid size the parameters specified.
		FILETIFF_ERROR_BAD_TIFF_PARAMS Invalid values for the TIFF components settings were specified.
		FILETIFF_ERROR_WRITE_FAILED    The actual write to the file failed.
	
	\Note:
		For component_depth > 8, data is stored as 16 bits per sample and is scaled up to 16 bits.
		(This is for ease of use since it will be read as 16 bits with no way to determine the actual depth).
*/
int File_WriteToTIFF( char *filename, uint32_t width, uint32_t height, uint32_t num_components, uint32_t component_depth, uint32_t component_order, int size, void *imageData)
{
	TIFF *output;
	int ret = FILETIFF_ERROR_BAD_BUFFER;
	uint16_t bps = component_depth;
	uint16_t spp = num_components; 
	uint16_t photometric = (num_components == 1) ? PHOTOMETRIC_MINISBLACK : PHOTOMETRIC_RGB;
	uint32_t outputsize = width * height * spp * ((component_depth + 7)/8);

	// Sanity check (No more than 4 color components, no more than 16 bits per component).
	if ( (spp == 0) || (spp > 4) || (bps == 0) || (bps > 16) || (component_order > 2) )
	{
		return FILETIFF_ERROR_BAD_TIFF_PARAMS;
	}

	// Check input data buffer.
	if ( (imageData != NULL) && (outputsize <= size) )
	{		
		int use_alpha = 0;
		int shift = (bps > 8) ? (16 - bps) : 0;

		// "libtiff" only does 8 or 16 bit pixel components.
		bps = (bps > 8) ? 16 : 8;

		ret = FILETIFF_ERROR_FILE_ACCESS;
		
		// Open the TIFF image
		if((output = TIFFOpen(filename, "w")) != NULL)
		{
			TIFFSetField(output, TIFFTAG_IMAGEWIDTH, width);
			TIFFSetField(output, TIFFTAG_IMAGELENGTH, height);
			TIFFSetField(output, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
			TIFFSetField(output, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(output, TIFFTAG_PHOTOMETRIC, photometric);
			TIFFSetField(output, TIFFTAG_BITSPERSAMPLE, bps);
			TIFFSetField(output, TIFFTAG_SAMPLESPERPIXEL, spp);

			if ( spp == 4 )
			{
       		 	uint16 v[2] = {0};
            	v[0] = EXTRASAMPLE_ASSOCALPHA;
            	TIFFSetField(output, TIFFTAG_EXTRASAMPLES, 1, v);
            	use_alpha = 1;
			}
	
			
			// Write base on samples per pixel...
			if ( spp == 1 )
			{
				// Monochrome
				uint32_t bytes_per_line = width * ((bps + 7)/8);
				int row, col;
					
				// Handle based on the component depth (8 or 16 bit)
				if (bps <= 8)
				{
					// 8 bit handling - Write image as a single strip (no scaling required).
					ret = TIFFWriteEncodedStrip(output, 0, imageData, outputsize );
					if (ret == 0) ret = FILETIFF_ERROR_WRITE_FAILED;
				}
				else
				{
					// 16 bit handling (scaling).
					uint16_t *pixP, *px;
					void *pxLine = malloc( bytes_per_line );
				
					if (pxLine != NULL)
					{
						ret = 0;
						for (row = 0; row < height; row++)
						{
							pixP = (uint16_t *)pxLine;
							px = (uint16_t *)imageData + row*width;
							
							for (col = 0; col < width; col++)
							{
								*pixP++ = (*px++ << shift); // Scale data up to full 16 bits.
							}
							ret = TIFFWriteScanline(output, pxLine, row, 0);
							if (ret == -1) ret = FILETIFF_ERROR_WRITE_FAILED;
						}
						free(pxLine);
					}
				}
			}
			else
			{
				// Color. 
				uint32_t bytes_per_pixel = spp * ((component_depth + 7)/8);
				uint32_t bytes_per_line = width * bytes_per_pixel;
				void *pxLine = malloc( bytes_per_line );
				uint32_t componentOffset = (component_order == FILETIFF_COMPONENT_ORDER_PLANAR) ?  (width * height) : 1;
				uint32_t componentInc = spp;
				int reverse_order = (component_order == FILETIFF_COMPONENT_ORDER_REVERSE) ?  1 : 0;
				
				if (pxLine != NULL)
				{
					int row, col;
					
					// Handle based on the component depth (8 or 16 bit)
					if (bps <= 8)
					{
						// 8 bit handling.
						uint8_t *pixP, *rp, *gp, *bp, *alpha;
						
						for (row = 0; row < height; row++)
						{
							pixP = (uint8_t *)pxLine;
							
							if ( reverse_order )
							{
								bp = (uint8_t *)imageData + row*width*spp; 
								gp = bp + componentOffset;
								rp = gp + componentOffset;
								alpha = rp + componentOffset;
							}
							else
							{
								rp = (uint8_t *)imageData + row*width*spp; 
								gp = rp + componentOffset;
								bp = gp + componentOffset;
								alpha = bp + componentOffset;
							}
							
							for (col = 0; col < width; col++)
							{
								*pixP++ = *rp;
								*pixP++ = *gp;
								*pixP++ = *bp;
								rp += componentInc;
								gp += componentInc;
								bp += componentInc;
								if ( use_alpha )
								{
									*pixP++ = *alpha;
									alpha += componentInc;
								}
							}
							ret = TIFFWriteScanline(output, pxLine, row, 0);
							if (ret == -1) ret = FILETIFF_ERROR_WRITE_FAILED;
						} 
					}
					else
					{
						// 16 bit handling.
						uint16_t *pixP, *rp, *gp, *bp, *alpha;
						
						for (row = 0; row < height; row++)
						{
							pixP = (uint16_t *)pxLine;
							if ( reverse_order )
							{
								bp = (uint16_t *)imageData + row*width*spp; 
								gp = bp + componentOffset;
								rp = gp + componentOffset;
								alpha = rp + componentOffset;
							}
							else
							{
								rp = (uint16_t *)imageData + row*width*spp; 
								gp = rp + componentOffset;
								bp = gp + componentOffset;
								alpha = bp + componentOffset;
							}
							
							for (col = 0; col < width; col++)
							{
								*pixP++ = (*rp << shift);
								*pixP++ = (*gp << shift);
								*pixP++ = (*bp << shift);
								rp += componentInc;
								gp += componentInc;
								bp += componentInc;
								if ( use_alpha )
								{
									*pixP++ = (*alpha << shift);
									alpha += componentInc;
								}
							}
							ret = TIFFWriteScanline(output, pxLine, row, 0);
							if (ret == -1) ret = FILETIFF_ERROR_WRITE_FAILED;
						} 
					}
					free(pxLine);				
				}
			}
			if (ret == 1) ret = outputsize;
		}
		TIFFClose(output);
	}
	return ret;
}
	

//======================================================================
//
// Legacy code : Retained for backwords compatibility.
// 
// Only supports 8-bit RGB and RGBa data.
//
//
// (Note: Original versions swapped Red and Blue symmetrically so worked
//        together but not with images not saved with these functions. 
//        That is fixed now.)
//
//
//  For Monochrome images, "color" is 0 and "depth" is the number of bits per pixel
//  For Color(RGB) images, "color" is 1 and "depth" is the number of 8 bit components.
//  
//  These are not suitable for the multitude of AIA color formats (PFNC/GigeVision)
//
int File_ReadTIFF( char *filename, uint32_t *width, uint32_t *height, uint32_t *depth, int *color, int size, void *data)
{

	TIFF *image;
	int num_components = 0;
	int bitdepth = 0;
	int ret = -1;

	if ( 0 == File_GetTIFFInfo( filename, width, height, &bitdepth, &num_components) ) 
	{
		uint32_t imgsize;
		imgsize = (*width)*(*height)*num_components*((bitdepth + 7)/8);
		
		ret = -2;
		if ((size >= imgsize) && (data != NULL) && (color != NULL) && (depth != NULL) ) 
		{
			// Open the TIFF image
			ret = -1;
			if((image = TIFFOpen(filename, "r")) != NULL)
			{
				ret = 0;			
				if (num_components > 1)
				{
					*color = 1;
					*depth = num_components;

					// Color image (more than 1 component)
					if ( num_components == 4 ) 
					{
						// It fits into 4 bytes/pixel buffer so do it all at once.
						ret = TIFFReadRGBAImageOriented( image, *width, *height, data, ORIENTATION_TOPLEFT, 1);
						if (ret == 0) ret = -2;
						if (ret == 1) ret = 0;
					}
					else
					{
						// 24-bit RGB - Do it a line at a time (check palette)
						uint16_t bps, spp, photometric;
						uint32_t lineSize = TIFFScanlineSize(image);
						int redcolormap[256], greencolormap[256], bluecolormap[256];
						void *buf = NULL;
						int i;
						
						TIFFGetField(image, TIFFTAG_SAMPLESPERPIXEL, &spp);
						TIFFGetField(image, TIFFTAG_BITSPERSAMPLE, &bps);
						TIFFGetField(image, TIFFTAG_PHOTOMETRIC, &photometric);
						
						if (photometric == PHOTOMETRIC_PALETTE)
						{
							uint16_t *rmap = NULL;
							uint16_t *gmap = NULL;
							uint16_t *bmap = NULL;
							
							// Set up colormap
							TIFFGetField(image, TIFFTAG_COLORMAP, &rmap, &gmap, &bmap);
							if ( (rmap != NULL) && (gmap != NULL) && (bmap != NULL) )
							{
								for ( i = 0; i < 256; i++)
								{
									redcolormap[i]   = (int) (rmap[i] * 256) /  65535L;
									greencolormap[i] = (int) (gmap[i] * 256) /  65535L;
									bluecolormap[i]  = (int) (bmap[i] * 256) /  65535L;
								}
							}
							else
							{
								// Should not have an error - make a unity colormap
								for ( i = 0; i < 256; i++)
								{
									redcolormap[i]   = i;
									greencolormap[i] = i;
									bluecolormap[i]  = i;
								}
							}
						}
						
						buf = malloc(lineSize);
						if (buf != NULL)
						{
							unsigned char *rptr = (unsigned char *)data;
							unsigned char *gptr = rptr+1;
							unsigned char *bptr = gptr+1;
							
							for (i = 0; i < *height; i++)
							{
								
								ret = TIFFReadScanline(image, buf, i, 0);
								if (ret < 0)
								{
									printf("File_ReadTIFF : TIFFReadScanline returns %d, line = %d\n", ret, i); 
									break;
								}
								else
								{
									int j = 0;
									unsigned char *component = (unsigned char *)buf;
									
									// Handle the RGB for this row (its actually BGR)
									for (j = 0; j < *width; j++)
									{									
										if (photometric == PHOTOMETRIC_PALETTE)
										{
											// Lookup via palette.
											
											*rptr = (unsigned char) redcolormap[*component++];
											*gptr = (unsigned char) greencolormap[*component++]; 
											*bptr = (unsigned char) bluecolormap[*component++];
										}
										else
										{
											// RGB (8 bit components ONLY).
											*rptr = *component++;
											*gptr = *component++; 
											*bptr = *component++;
										}
										rptr += 3;
										gptr += 3;
										bptr += 3;										
									}								
								}						
							}
							ret = 0;
						}
						else
						{
							ret = -2;
						}
					}
				}
				else
				{
					int i;
					char *cptr = (char *)data;
					uint16_t *sptr = (uint16_t *)data;
					void *ptr = data;
					
					*depth = (bitdepth + 7)/8;
					
					// Grayscale image - do it a row at a time.
					// (It's either 8 bit or 16 bit buffer)
					for (i = 0; i < *height; i++)
					{
						ret = TIFFReadScanline(image, ptr, i, 0);
						if (ret < 0)
						{
							break;
						}
						else
						{
							cptr += *width;
							sptr += *width;
							ptr = (*depth == 1) ? (void *)cptr : (void *)sptr;
						}
					}
					*color = 0;				
				}
				if (ret == 1) ret = 0;
				TIFFClose(image);
			}
			else
			{
				ret = -2;
			}
		}
	}
	return ret;
}




// Only 8 bit color components (RGB/RGBa) are supported directly in TIFF.
// (Color components order are R then G, then B (Bytes 0,1,2...) which it 
//  the equivalent of 
// 	GigeVision :  fmtRGB8   / ftmRGBa8
//		PFNC       :  PFNC_RGB7 / PFNC_RGBa8
//		SaperaLT   : CORDATA_FORMAT_RGBR888 / CORDATA_FORMAT_RGB
//
int File_WriteTIFF( char *filename, uint32_t width, uint32_t height, uint32_t depth, int color, void *imageData )
{
	TIFF *output;
	int ret = 0;
	uint16_t bps, spp, photometric;
	uint32_t outputsize;
		
	if ( color == 0)
	{
		// Monochrome image data.
		photometric = PHOTOMETRIC_MINISBLACK;
		spp = 1;
		bps = depth * 8;
		outputsize = width * height * depth;
	}
	else
	{
		// Color image data - save as RGB 
		// (Assume 8 bit color components maximum)
		photometric = PHOTOMETRIC_RGB;
		bps = 8;
		spp = 4;
		switch(depth)
		{
			case 3:
				spp = 3;
				break;
			case 4:
				spp = 4;
				break;
			default:
				ret = -4;
				break;
		}
		
		outputsize = width * height * spp;
	}
		
	if (ret == 0)
	{
		ret = -1;
		// Open the TIFF image
		if((output = TIFFOpen(filename, "w")) != NULL)
		{
			TIFFSetField(output, TIFFTAG_IMAGEWIDTH, width);
			TIFFSetField(output, TIFFTAG_IMAGELENGTH, height);
			TIFFSetField(output, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
			TIFFSetField(output, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(output, TIFFTAG_PHOTOMETRIC, photometric);
			TIFFSetField(output, TIFFTAG_BITSPERSAMPLE, bps);
			TIFFSetField(output, TIFFTAG_SAMPLESPERPIXEL, spp);

			if ( spp == 4 )
			{
       		 	uint16 v[2] = {0};
            	v[0] = EXTRASAMPLE_ASSOCALPHA;
            	TIFFSetField(output, TIFFTAG_EXTRASAMPLES, 1, v);
			}

			ret = TIFFWriteEncodedStrip(output, 0, imageData, outputsize );
			if (ret == 0) ret = -2;
			if (ret == outputsize) ret = 0;
		}
		TIFFClose(output);
	}
	// Error opening file for writing.
	return ret;
}


#endif

