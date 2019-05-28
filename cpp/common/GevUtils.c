/*
  ---------------------------------------------
  GigE-V Framework Utility Functions
  -----------------------------------------------
*/

#include "SapX11Util.h"
#include "gevapi.h"
#include "PFNC.h"
#include "FileUtil.h"

//=============================================================================
// Translation of pixel format information between GigE-Vision formats and
// both the X11_Display_Utils implementation and the basic RGB888  (no alpha 
//	channel) and RGB8888 (with alpha_channel) formats used by the GdkPixbuf.
//
// X11_Display_Utils (currently) only supports the following formats:
//	CORX11_DATA_FORMAT_MONO      - Any Monochrome (unpacked) format (8, 10, 12, 14, 16 bits).
//	CORX11_DATA_FORMAT_RGB888    - RGB (packed) 8 bits per channel in 24 bits.
//	CORX11_DATA_FORMAT_RGB8888   - RGB (unpacked) 8 bits per channel in 32 bits.
//	CORX11_DATA_FORMAT_RGB5551   - RGB (packed) "hicolor" mode (5:5:5:1)
//	CORX11_DATA_FORMAT_RGB565    - RGB (packed) "hicolor" mode (5:6:5)
//	CORX11_DATA_FORMAT_RGB101010 - RGB (packed) 10 bits per channel (packed into 32 bits)
// CORX11_DATA_FORMAT_YUV411		- composite RGB -> one RGB pixel is 12 bits (all packed together) -> RGB/Mono conversion available.
// CORX11_DATA_FORMAT_YUV422		- composite RGB -> one RGB pixel is 16 bits (all packed together) -> RGB/Mono conversion available.
// CORX11_DATA_FORMAT_YUV444		- composite RGB -> one RGB pixel is 24 bits (all packed together) -> RGB/Mono conversion available.

// GEV API supports formats in the enumGevPixelFormat type :
//
// MONO formats
//
// fmtMono8          (supported as CORX11_DATA_FORMAT_MONO with depth 8)
// fmtMono8Signed    (supported as CORX11_DATA_FORMAT_MONO with depth 8 - might look funny)
// fmtMono10         (supported as CORX11_DATA_FORMAT_MONO with depth 10)
// fmtMono10Packed   (8 pixels packed into 10 bytes -> Conversion to MONO available)
// fmtMono12         (supported as CORX11_DATA_FORMAT_MONO with depth 12)
// fmtMono12Packed   (4 12-bit pixels packed into 6 bytes -> Conversion to MONO available)
// fmtMono14         (supported as CORX11_DATA_FORMAT_MONO with depth 14)
// fmtMono16         (supported as CORX11_DATA_FORMAT_MONO with depth 16)
//
// Bayer filter formats (color conversion not supported -> treated as monochrome)
//
// fmtBayerGR8       (becomes MONO8)
// fmtBayerRG8       (becomes MONO8)
// fmtBayerGB8       (becomes MONO8)
// fmtBayerBG8       (becomes MONO8)
// fmtBayerGR10      (becomes MONO10)
// fmtBayerRG10      (becomes MONO10)
// fmtBayerGB10      (becomes MONO10)
// fmtBayerBG10      (becomes MONO10)
// fmtBayerGR12      (becomes MONO12)
// fmtBayerRG12	   (becomes MONO12)
// fmtBayerGB12      (becomes MONO12)
// fmtBayerBG12      (becomes MONO12)
//
// RGB Packed formats
//
// fmtRGB8Packed     (RGB unpacking conversion available -> converts to CORX11_DATA_FORMAT_RGB8888)
// fmtBGR8Packed     (RGB unpacking conversion available -> converts to CORX11_DATA_FORMAT_RGB8888)
// fmtRGBA8Packed    (supported as CORX11_DATA_FORMAT_RGB8888)
// fmtBGRA8Packed    (conversion available -> converts to CORX11_DATA_FORMAT_RGB8888)
// fmtRGB10Packed    (RGB unpacking conversion available -> converts to CORX11_DATA_FORMAT_RGB8888)
// fmtBGR10Packed    (RGB unpacking conversion available -> converts to CORX11_DATA_FORMAT_RGB8888)
// fmtRGB12Packed    (RGB unpacking conversion available -> converts to CORX11_DATA_FORMAT_RGB8888)
// fmtBGR12Packed    (RGB unpacking conversion available -> converts to CORX11_DATA_FORMAT_RGB8888)
// fmtRGB10V1Packed  (RGB unpackign conversion available -> converts to CORX11_DATA_FORMAT_RGB8888)
// fmtRGB10V2Packed  (RGB unpackign conversion available -> converts to CORX11_DATA_FORMAT_RGB8888)
//
// YUV Packed formats (not supported - conversion routines supplied).
//
// fmtYUV411packed   (conversion required for X11 display -> RGB/Mono conversion available.)
// fmtYUV422packed   (conversion required for X11 display -> RGB/Mono conversion available.)
// fmtYUV444packed   (conversion required for X11 display -> RGB/Mono conversion available.)
//
// Planar formats : (Not supported yet - Each image plane a separate (sequential) image region.)
//
// fmtRGB8Planar		(No Support)
// fmtRGB10Planar		(No Support)
// fmtRGB12Planar		(No Support)
// fmtRGB16Planar		(No Support)
//
//
// New PFNC formats:  
//
// PFNC_BiColorBGRG8      Bi-color Blue/Green - Red/Green 8-bit 
// PFNC_BiColorBGRG10     Bi-color Blue/Green - Red/Green 10-bit unpacked 
// PFNC_BiColorBGRG10p    Bi-color Blue/Green - Red/Green 10-bit packed 
// PFNC_BiColorBGRG12     Bi-color Blue/Green - Red/Green 12-bit unpacked 
// PFNC_BiColorBGRG12p    Bi-color Blue/Green - Red/Green 12-bit packed 
// PFNC_BiColorRGBG8      Bi-color Red/Green - Blue/Green 8-bit 
// PFNC_BiColorRGBG10     Bi-color Red/Green - Blue/Green 10-bit unpacked 
// PFNC_BiColorRGBG10p    Bi-color Red/Green - Blue/Green 10-bit packed 
// PFNC_BiColorRGBG12     Bi-color Red/Green - Blue/Green 12-bit unpacked 
// PFNC_BiColorRGBG12p    Bi-color Red/Green - Blue/Green 12-bit packed 

//=============================================================================
//
int IsGevPixelTypeX11Displayable(int format)
{
	int displayable = FALSE;
	switch(format)
	{
		case fmtYUV411packed:
		case fmtYUV422packed:
		case fmtYUV444packed:
      case fmtMono10Packed:
      case fmtMono12Packed:
		case fmtBayerGR10Packed:
		case fmtBayerRG10Packed:
		case fmtBayerGB10Packed:
		case fmtBayerBG10Packed:
		case fmtBayerGR12Packed:
		case fmtBayerRG12Packed:
		case fmtBayerGB12Packed:
		case fmtBayerBG12Packed:
      case fmtRGB10V1Packed:
      case fmtRGB10V2Packed:
      case fmtRGB8Planar:
      case fmtRGB10Planar:
      case fmtRGB12Planar:
      case fmtRGB16Planar:
      case fmt_PFNC_BiColorBGRG8:
      case fmt_PFNC_BiColorBGRG10:
      case fmt_PFNC_BiColorBGRG12:
      case fmt_PFNC_BiColorRGBG8:
      case fmt_PFNC_BiColorRGBG10:
      case fmt_PFNC_BiColorRGBG12:
			displayable = FALSE;
			break;
		default:
			displayable = TRUE;
			break;
	}
	return displayable;
}

// Translate/convert an input GigeVision pixel format to one that is displayable by these X11 display functions.
// If the input pixel format is able to be converted to a different GigeVision format, the converted GigeVision
// format is returned.
// This lets the caller know the proper pixel format for any unpacking / decoding performed by the API.
// (The convertBayer flag, if 1, enables the function to return an equivalent RGB format for the an input Bayer
//  encoded format. This format can then be used by a Bayer conversion function).
//
int GetX11DisplayablePixelFormat( int convertBayer, UINT32 rawGevPixelFormat, u_int32_t *convertedGevPixelFormat, u_int32_t *displayableSaperaPixelFormat)
{
	UINT32 cvtPixFmt = GevGetConvertedPixelType( convertBayer, rawGevPixelFormat);

	if (!IsGevPixelTypeX11Displayable(cvtPixFmt))
	{
		// Our X11 functions will not display this image - need to set up to convert it.
		if ( GevIsPixelTypePacked(rawGevPixelFormat) )
		{
			// Get the supported output format from the in-line unpacker.
			// (It will return Mono8 if it does not understand).
			cvtPixFmt = GevGetUnpackedPixelType(rawGevPixelFormat);
			
			// See if this is now a Bayer format.
			if ( GevIsPixelTypeBayer(cvtPixFmt) && convertBayer )
			{
				// Get the converted pixel format for the bayer.
				cvtPixFmt = GevGetBayerAsRGBPixelType(cvtPixFmt);				
			}
			
			// Set up the output formats.
			if (convertedGevPixelFormat != NULL)
			{
				*convertedGevPixelFormat = cvtPixFmt;
			}
			if (displayableSaperaPixelFormat != NULL)
			{
				*displayableSaperaPixelFormat = Convert_GevFormat_To_Sapera(cvtPixFmt);
			}			
		}
		else if (GevIsPixelTypeRGB(rawGevPixelFormat))
		{
			// A packed color image (various YUV ones) - Convert to RGB8888.
			// Set up the output formats.
			
			if (convertedGevPixelFormat != NULL)
			{
				*convertedGevPixelFormat = cvtPixFmt;
			}
			if (displayableSaperaPixelFormat != NULL)
			{
				*displayableSaperaPixelFormat = Convert_GevFormat_To_Sapera(cvtPixFmt);
			}			
		}
		else 
		{
			if (convertedGevPixelFormat != NULL)
			{
				*convertedGevPixelFormat = cvtPixFmt;
			}
			if (displayableSaperaPixelFormat != NULL)
			{
				*displayableSaperaPixelFormat = Convert_GevFormat_To_Sapera(cvtPixFmt);
			}			
		}
	}
	else
	{
		// It is displayable - as either color (RGB8888) or MONO8/10/12/14/16.
		// (That's all our display function  will do).
		//
		// Check if converted output is 10 or 12 bit RGB - need to convert to 8 bit bayer.
		if ( GevIsPixelTypeRGB(cvtPixFmt) && (GevGetPixelDepthInBits(cvtPixFmt) > 8) )
		{ 
			// Override the output format for display to BGRA8!! (same as CORDATA_FORMAT_RGB8888)
			cvtPixFmt = fmtBGRA8Packed;
		}
		if (convertedGevPixelFormat != NULL)
		{
			*convertedGevPixelFormat = cvtPixFmt;
		}
		if (displayableSaperaPixelFormat != NULL)
		{
			// Trap the RGB101010 case here - make it RGB8888 (or display won't work properly).
			UINT32 dpyFormat = Convert_GevFormat_To_Sapera(cvtPixFmt);
			if (dpyFormat == CORDATA_FORMAT_RGB101010)
			{
				dpyFormat = CORDATA_FORMAT_RGB8888;
			} 
			*displayableSaperaPixelFormat = dpyFormat;
		}			
	}
	return 0;
}


int Convert_GevFormat_To_X11( int GevDataFormat)
{
   int format = CORX11_DATA_FORMAT_DEFAULT;
   
   if ( GevIsPixelTypeRGB(GevDataFormat) )
   {
		// Color format - see if we can handle it directly (without conversion).
		// (Note: there are many (many) formats supported by the GigE standard that 
		//  would require conversion before display).
      switch (GevDataFormat)
      {
 			case fmtRGB8Packed:
			case fmtBGR8Packed:
         case fmtYUV444packed:
            format = CORX11_DATA_FORMAT_RGB888;
            break; 
			case fmtRGBA8Packed:
			case fmtBGRA8Packed:
            format = CORX11_DATA_FORMAT_RGB8888;
            break; 
         case fmtRGB10Packed:
         case fmtBGR10Packed:
         case fmtRGB10V1Packed:
         case fmtRGB10V2Packed:
            format = CORX11_DATA_FORMAT_RGB101010;
            break; 
         case fmtYUV411packed:
					format = CORX11_DATA_FORMAT_MONO;  // 12 bit YUV pixel - conversion required.
					break;
         case fmtYUV422packed:
            format = CORX11_DATA_FORMAT_RGB5551;
            break; 
         default:
            format = CORX11_DATA_FORMAT_DEFAULT;
            break;
      }
   }
   else if ( GevIsPixelTypeMono(GevDataFormat) )
   {
      // Its monochrome (only 10/12 bit packed is not handled yet)
      switch (GevDataFormat)
      {
         case fmtMono10Packed:
         case fmtMono12Packed:
            format = CORX11_DATA_FORMAT_DEFAULT;
            break; 
         default:
            format = CORX11_DATA_FORMAT_MONO;
            break;
      }
   }
	else
	{
		// Not supported - just handle it as default (copy to display).
		format = CORX11_DATA_FORMAT_DEFAULT;
	}
   return format;
}

int Convert_GevFormat_To_Sapera( int GevDataFormat)
{
   int format = CORX11_DATA_FORMAT_DEFAULT;
   
   if ( GevIsPixelTypeRGB(GevDataFormat) )
   {
		// Color format - see if we can handle it directly (without conversion).
		// (Note: there are many (many) formats supported by the GigE standard that 
		//  would require conversion before display).
      switch (GevDataFormat)
      {
 			case fmtRGB8Packed:
			case fmtBGR8Packed:
            format = CORDATA_FORMAT_RGB888;
            break; 
			case fmtRGBA8Packed:
			case fmtBGRA8Packed:
            format = CORDATA_FORMAT_RGB8888;
            break; 
         case fmtRGB10Packed:
         case fmtBGR10Packed:
         case fmtRGB12Packed:
         case fmtBGR12Packed:
         case fmtRGB10V1Packed:
         case fmtRGB10V2Packed:
            format = CORDATA_FORMAT_RGB101010;
            break; 
			case fmtYUV411packed:
				format = CORDATA_FORMAT_Y411;
				break;
			case fmtYUV422packed:
				format = CORDATA_FORMAT_YUY2;
				break;
			case fmtYUV444packed:
				format = CORDATA_FORMAT_YUYV;
				break;
         default:
            format = CORDATA_FORMAT_UINT8;
            break;
      }
   }
   else if ( GevIsPixelTypeMono(GevDataFormat) )
   {
      // Its monochrome 
      switch (GevDataFormat)
      {
			case fmtMono8Signed:
					format = CORDATA_FORMAT_INT8;
				break;
			case fmtMono10:
			case fmtMono10Packed:
			case fmtMono12:
			case fmtMono12Packed:
			case fmtMono14:
			case fmtMono16:
			case fmtBayerGR10:
			case fmtBayerRG10:
			case fmtBayerGB10:
			case fmtBayerBG10:
			case fmtBayerGR12:
			case fmtBayerRG12:
			case fmtBayerGB12:
			case fmtBayerBG12:
					format = CORDATA_FORMAT_MONO16;
				break;			
         default:
            format = CORDATA_FORMAT_UINT8;
            break;
      }
   }
	else
	{
		// Not supported - just handle it as mono / default (copy to display).
		format = CORDATA_FORMAT_MONO8;
	}
   return format;
}

static void Convert_YUV411_To_Mono(int pixelCount, void *in, int outDepth, void *out)
{
	int i;
	if ((in != NULL) && (out != NULL))
	{
		switch (outDepth)
		{
			case 16:
			case 12:
			case 10:
				{
					unsigned char *pIn = (unsigned char *)in;
					unsigned short *pOut = (unsigned short *)out;
					int lshift = outDepth - 8;
					for (i = 0; i < pixelCount; i+= 4)
					{
						pIn++;				// Skip U
						*pOut++ = ((unsigned short)*pIn++) << lshift;	// Store Y0
						*pOut++ = ((unsigned short)*pIn++) << lshift; 	// Store Y1
						pIn++;				// Skip V
						*pOut++ = ((unsigned short)*pIn++) << lshift;	// Store Y2
						*pOut++ = ((unsigned short)*pIn++) << lshift;	// Store Y3
					}
				}
				break;
			case 8:
			default:
				{
					unsigned char *pIn = (unsigned char *)in;
					unsigned char *pOut = (unsigned char *)out;
					for (i = 0; i < pixelCount; i+= 4)
					{
						pIn++;				// Skip U
						*pOut++ = *pIn++;	// Store Y0
						*pOut++ = *pIn++; // Store Y1
						pIn++;				// Skip V
						*pOut++ = *pIn++;	// Store Y2
						*pOut++ = *pIn++; // Store Y3
					}
				}
				break;
		}
	}
}

static void Convert_YUV422_To_Mono(int pixelCount, void *in, int outDepth, void *out)
{
	int i;
	if ((in != NULL) && (out != NULL))
	{
		switch (outDepth)
		{
			case 16:
			case 12:
			case 10:
				{
					unsigned char *pIn = (unsigned char *)in;
					unsigned short *pOut = (unsigned short *)out;
					int lshift = outDepth - 8;
					for (i = 0; i < pixelCount; i+= 4)
					{
						pIn++;				// Skip U
						*pOut++ = ((unsigned short)*pIn++) << lshift;	// Store Y0
						pIn++;				// Skip V
						*pOut++ = ((unsigned short)*pIn++) << lshift; 	// Store Y1
					}
				}
				break;
			case 8:
			default:
				{
					unsigned char *pIn = (unsigned char *)in;
					unsigned char *pOut = (unsigned char *)out;
					for (i = 0; i < pixelCount; i+= 4)
					{
						pIn++;				// Skip U0
						*pOut++ = *pIn++;	// Store Y0
						pIn++;				// Skip V0
						*pOut++ = *pIn++; // Store Y1
					}
				}
				break;
		}
	}
}

static void Convert_YUV444_To_Mono(int pixelCount, void *in, int outDepth, void *out)
{
	int i;
	if ((in != NULL) && (out != NULL))
	{
		switch (outDepth)
		{
			case 16:
			case 12:
			case 10:
				{
					unsigned char *pIn = (unsigned char *)in;
					unsigned short *pOut = (unsigned short *)out;
					int lshift = outDepth - 8;
					for (i = 0; i < pixelCount; i+= 3)
					{
						pIn++;				// Skip U
						*pOut++ = ((unsigned short)*pIn++) << lshift;	// Store Y
						pIn++;				// Skip V
					}
				}
				break;
			case 8:
			default:
				{
					unsigned char *pIn = (unsigned char *)in;
					unsigned char *pOut = (unsigned char *)out;
					for (i = 0; i < pixelCount; i+= 3)
					{
						pIn++;				// Skip U
						*pOut++ = *pIn++;	// Store Y
						pIn++;				// Skip V
					}
				}
				break;
		}
	}
}

#define YUV_SCALE_FACTOR	14

static void Convert_YUV411_To_RGB8x(int alpha_channel, int pixelCount, void *in, int outDepth, void *out)
{
	unsigned char *pIn = (unsigned char *)in;
	unsigned char *pOut = (unsigned char *)out;
	int i;
	uint32_t iPixel;
	int32_t	lCValue;
	int32_t 	Y[4];	
	int32_t 	V0;	
	int32_t 	U0;
	int32_t  Const16384 = (1 << YUV_SCALE_FACTOR);
	
	if ( (pIn != NULL) && (pOut != NULL))
	{
		for (i = 0; i < pixelCount; i+=4)
		{
			U0 = (int32_t) *pIn++ - 128;
			Y[0] = (int32_t) *pIn++;
			Y[1] = (int32_t) *pIn++;
			V0 = (int32_t) *pIn++ - 128;
			Y[2] = (int32_t) *pIn++;
			Y[3] = (int32_t) *pIn++;

			for (iPixel = 0; iPixel < 4; i++)
			{
				// Blue conversion.
				lCValue = (Y[iPixel]*Const16384 + 29147*U0) >> YUV_SCALE_FACTOR;
				*pOut++ = (lCValue < 0) ? 0 : ( (lCValue > 255) ? 255 : (unsigned char)lCValue);

				// Green conversion.
				lCValue = (Y[iPixel]*Const16384 - 5661*U0 -11746*V0) >> YUV_SCALE_FACTOR;
				*pOut++ = (lCValue < 0) ? 0 : ( (lCValue > 255) ? 255 : (unsigned char)lCValue);

				// Red conversion.
				lCValue = (Y[iPixel]*Const16384 + 23060*V0) >> YUV_SCALE_FACTOR;
				*pOut++ = (lCValue < 0) ? 0 : ( (lCValue > 255) ? 255 : (unsigned char)lCValue);
		
				// Pad (alpha channel)
				if (alpha_channel)
				{
					*pOut++ = 0xff;
				}
			}
		}
	}
}


static void Convert_YUV411_To_RGB888(int pixelCount, void *in, int inDepth, void *out)
{
	Convert_YUV411_To_RGB8x(FALSE, pixelCount, in, inDepth, out);
}
static void Convert_YUV411_To_RGB8888(int pixelCount, void *in, int inDepth, void *out)
{
	Convert_YUV411_To_RGB8x(TRUE, pixelCount, in, inDepth, out);
}


static void Convert_YUV422_To_RGB8x(int alpha_channel, int pixelCount, void *in, int outDepth, void *out)
{
	unsigned char *pIn = (unsigned char *)in;
	unsigned char *pOut = (unsigned char *)out;
	int i;
	int32_t	lCValue;
	int32_t 	Y0;	
	int32_t 	Y1;	
	int32_t 	V0;	
	int32_t 	U0;
	int32_t  Const16384 = (1 << YUV_SCALE_FACTOR);
	
	if ( (pIn != NULL) && (pOut != NULL))
	{
		for (i = 0; i < pixelCount; i+=2)
		{
			U0 = (int32_t) *pIn++ - 128;
			Y0 = (int32_t) *pIn++;
			V0 = (int32_t) *pIn++ - 128;
			Y1 = (int32_t) *pIn++;

			// Blue conversion.
			lCValue = (Y0*Const16384 + 29147*U0) >> YUV_SCALE_FACTOR;
			*pOut++ = (lCValue < 0) ? 0 : ( (lCValue > 255) ? 255 : (unsigned char)lCValue);

			// Green conversion.
			lCValue = (Y0*Const16384 - 5661*U0 -11746*V0) >> YUV_SCALE_FACTOR;
			*pOut++ = (lCValue < 0) ? 0 : ( (lCValue > 255) ? 255 : (unsigned char)lCValue);

			// Red conversion.
			lCValue = (Y0*Const16384 + 23060*V0) >> YUV_SCALE_FACTOR;
			*pOut++ = (lCValue < 0) ? 0 : ( (lCValue > 255) ? 255 : (unsigned char)lCValue);
		
			// Pad (alpha channel)
			pOut++;

			// Blue conversion.
			lCValue = (Y1*Const16384 + 29147*U0) >> YUV_SCALE_FACTOR;
			*pOut++ = (lCValue < 0) ? 0 : ( (lCValue > 255) ? 255 : (unsigned char)lCValue);

			// Green conversion.
			lCValue = (Y1*Const16384 - 5661*U0 -11746*V0) >> YUV_SCALE_FACTOR;
			*pOut++ = (lCValue < 0) ? 0 : ( (lCValue > 255) ? 255 : (unsigned char)lCValue);

			// Red conversion.
			lCValue = (Y1*Const16384 + 23060*V0) >> YUV_SCALE_FACTOR;
			*pOut++ = (lCValue < 0) ? 0 : ( (lCValue > 255) ? 255 : (unsigned char)lCValue);

			// Pad (alpha channel)
			if (alpha_channel)
			{
				*pOut++ = 0xff;
			}
		}
	}
}

static void Convert_YUV422_To_RGB888(int pixelCount, void *in, int inDepth, void *out)
{
	Convert_YUV422_To_RGB8x(FALSE, pixelCount, in, inDepth, out);
}
static void Convert_YUV422_To_RGB8888(int pixelCount, void *in, int inDepth, void *out)
{
	Convert_YUV422_To_RGB8x(TRUE, pixelCount, in, inDepth, out);
}


static void Convert_YUV444_To_RGB8x(int alpha_channel, int pixelCount, void *in, int outDepth, void *out)
{
	unsigned char *pIn = (unsigned char *)in;
	unsigned char *pOut = (unsigned char *)out;
	int i;
	int32_t	lCValue;
	int32_t 	Y0;	
	int32_t 	V0;	
	int32_t 	U0;
	int32_t  Const16384 = (1 << YUV_SCALE_FACTOR);
	
	if ( (pIn != NULL) && (pOut != NULL))
	{
		for (i = 0; i < pixelCount; i++)
		{
			Y0 = (int32_t) *pIn++;
			V0 = (int32_t) *pIn++ - 128;
			U0 = (int32_t) *pIn++ - 128;

			// Blue conversion.
			lCValue = (Y0*Const16384 + 29147*U0) >> YUV_SCALE_FACTOR;
			*pOut++ = (lCValue < 0) ? 0 : ( (lCValue > 255) ? 255 : (unsigned char)lCValue);

			// Green conversion.
			lCValue = (Y0*Const16384 - 5661*U0 -11746*V0) >> YUV_SCALE_FACTOR;
			*pOut++ = (lCValue < 0) ? 0 : ( (lCValue > 255) ? 255 : (unsigned char)lCValue);

			// Red conversion.
			lCValue = (Y0*Const16384 + 23060*V0) >> YUV_SCALE_FACTOR;
			*pOut++ = (lCValue < 0) ? 0 : ( (lCValue > 255) ? 255 : (unsigned char)lCValue);
		
			// Pad (alpha channel)
			if (alpha_channel)
			{
				*pOut++ = 0xff;
			}
		}
	}
}

static void Convert_YUV444_To_RGB888(int pixelCount, void *in, int inDepth, void *out)
{
	Convert_YUV444_To_RGB8x(FALSE, pixelCount, in, inDepth, out);
}
static void Convert_YUV444_To_RGB8888(int pixelCount, void *in, int inDepth, void *out)
{
	Convert_YUV444_To_RGB8x(TRUE, pixelCount, in, inDepth, out);
}


static void Convert_RGBPacked_To_RGB8x(int alpha_channel, int pixelCount, void *in, int inDepth, void *out)
{
	unsigned char *pOut = (unsigned char *)out;
	int i;
	
	if ( (in != NULL) && (out != NULL))
	{
		switch(inDepth)
		{
			case 10:
			case 12:
				{
					int shift = inDepth - 8;
					unsigned short *pIn = (unsigned short *)in;
					for (i = 0; i < pixelCount; i+=3)
					{
						*pOut++ = (unsigned char) ((*pIn++) >> shift); // R
						*pOut++ = (unsigned char) ((*pIn++) >> shift); // G
						*pOut++ = (unsigned char) ((*pIn++) >> shift); // B
						if (alpha_channel)
						{
							*pOut++ = 0xff;
						}
					}
				}
				break;
			case 8:
			default:
				{
					unsigned char *pIn = (unsigned char *)in;
					for (i = 0; i < pixelCount; i+=3)
					{
						*pOut++ = (*pIn++); // R
						*pOut++ = (*pIn++); // G
						*pOut++ = (*pIn++); // B
						if (alpha_channel)
						{
							*pOut++ = 0xff;
						}
					}
				}
				break;
		}
	}
}

static void Convert_RGBPacked_To_RGB888(int pixelCount, void *in, int inDepth, void *out)
{
	Convert_RGBPacked_To_RGB8x(FALSE, pixelCount, in, inDepth, out);
}
static void Convert_RGBPacked_To_RGB8888(int pixelCount, void *in, int inDepth, void *out)
{
	Convert_RGBPacked_To_RGB8x(TRUE, pixelCount, in, inDepth, out);
}




static void Convert_BGRPacked_To_RGB8x(int alpha_channel, int pixelCount, void *in, int inDepth, void *out)
{
	unsigned char *pOut = (unsigned char *)out;
	int i;
	unsigned char red;
	unsigned char green;
	unsigned char blue;
	
	if ( (in != NULL) && (out != NULL))
	{
		switch(inDepth)
		{
			case 10:
			case 12:
				{
					int shift = inDepth - 8;
					unsigned short *pIn = (unsigned short *)in;
					for (i = 0; i < pixelCount; i+=3)
					{
						blue  = (unsigned char) ((*pIn++) >> shift); // B
						green = (unsigned char) ((*pIn++) >> shift); // G
						red   = (unsigned char) ((*pIn++) >> shift); // R
						*pOut++ = red;   // R
						*pOut++ = green; // G
						*pOut++ = blue;  // B
						if (alpha_channel)
						{
							*pOut++ = 0xff;
						}
					}
				}
				break;
			case 8:
			default:
				{
					unsigned char *pIn = (unsigned char *)in;
					for (i = 0; i < pixelCount; i+=3)
					{
						blue  = *pIn++; // B
						green = *pIn++; // G
						red   = *pIn++; // R
						*pOut++ = red;   // R
						*pOut++ = green; // G
						*pOut++ = blue;  // B
						if (alpha_channel)
						{
							*pOut++ = 0xff;
						}
					}
				}
				break;
		}
	}
}

static void Convert_BGRPacked_To_RGB888(int pixelCount, void *in, int inDepth, void *out)
{
	Convert_BGRPacked_To_RGB8x(FALSE, pixelCount, in, inDepth, out);
}
static void Convert_BGRPacked_To_RGB8888(int pixelCount, void *in, int inDepth, void *out)
{
	Convert_BGRPacked_To_RGB8x(TRUE, pixelCount, in, inDepth, out);
}

static void Convert_RGBPacked_To_RGB101010(int pixelCount, void *in, int inDepth, void *out)
{
	uint32_t *pOut = (uint32_t *)out;
	int i;
	
	if ( (in != NULL) && (out != NULL))
	{
		unsigned short vR, vG, vB;
		switch(inDepth)
		{
			case 10:
			case 12:
				{
					int shift = inDepth - 10;
					unsigned short *pIn = (unsigned short *)in;
					for (i = 0; i < pixelCount; i+=3)
					{
						vR = ((*pIn++) >> shift); // R
						vG = ((*pIn++) >> shift); // G
						vB = ((*pIn++) >> shift); // B
						
						*pOut++ = (vR << 20) | (vG << 10) | vB;
					}
				}
				break;
			case 8:
			default:
				{
					// Input is RGB8 packed in 24-bits.
					unsigned char *pIn = (unsigned char *)in;
					for (i = 0; i < pixelCount; i+=3)
					{
						vR = ((*pIn++) << 2); // R
						vG = ((*pIn++) << 2); // G
						vB = ((*pIn++) << 2); // B
						
						*pOut++ = (vR << 20) | (vG << 10) | vB;
					}
				}
				break;
		}
	}
}

static void Convert_BGRPacked_To_RGB101010(int pixelCount, void *in, int inDepth, void *out)
{
	uint32_t *pOut = (uint32_t *)out;
	int i;
	
	if ( (in != NULL) && (out != NULL))
	{
		unsigned short vR, vG, vB;
		switch(inDepth)
		{
			case 10:
			case 12:
				{
					int shift = inDepth - 10;
					unsigned short *pIn = (unsigned short *)in;
					for (i = 0; i < pixelCount; i+=3)
					{
						vB = ((*pIn++) >> shift); // R
						vG = ((*pIn++) >> shift); // G
						vR = ((*pIn++) >> shift); // B
						
						*pOut++ = (vR << 20) | (vG << 10) | vB;
					}
				}
				break;
			case 8:
			default:
				{
					// Input is RGB8 packed in 24-bits.
					unsigned char *pIn = (unsigned char *)in;
					for (i = 0; i < pixelCount; i+=3)
					{
						vB = ((*pIn++) << 2); // R
						vG = ((*pIn++) << 2); // G
						vR = ((*pIn++) << 2); // B
						
						*pOut++ = (vR << 20) | (vG << 10) | vB;
					}
				}
				break;
		}
	}
}




static void Convert_MonoPacked_To_Mono(int pixelCount, void *in, int inDepth, int outDepth, void *out)
{
	// 10 or 12 bit in (packed) -> 8, 10, or 12 bit out byte aligned).
	unsigned char *pIn = (unsigned char *)in;
	unsigned short input[4];
	int i, j;
	int shift = 0;
	int remainder = 0;
	
	if ( (in != NULL) && (out != NULL))
	{
		shift = inDepth - outDepth;
		remainder = pixelCount % 2;
		switch(outDepth)
		{
			case 10:
			case 12:
				{
					unsigned short *pOut = (unsigned short *)out;
					switch(inDepth)
					{
						case 12:
							{
								// 3 input bytes for 2 output pixels.
								for (i = 0; i < pixelCount; i+= 2)
								{
									for (j = 0; j < 3; j++)
									{
										input[j] = (unsigned short)*pIn++;
									}
									*pOut++ = ( (input[1] && 0x0F) | (input[0] << 4) ) >> shift;
									*pOut++ = (((input[1] && 0xF0) >> 4) | (input[2] << 4) ) >> shift;
								}
								// Handle the remainder.
								if (remainder != 0)
								{
									// There is one more output pixel to be extracted.
									*pOut = ( (pIn[1] && 0x0F) | (pIn[0] << 4) ) >> shift;
								}
							}
							break;
						default:
						case 10:
							{
								// 3 input bytes for 2 output pixels.
								for (i = 0; i < pixelCount; i+= 2)
								{
									for (j = 0; j < 3; j++)
									{
										input[j] = (unsigned short)*pIn++;
									}
									*pOut++ = ( (input[1] && 0x03) | (input[0] << 2) ) >> shift;
									*pOut++ = (((input[1] && 0x30) >> 4) | (input[2] << 2) ) >> shift;
								}						
								// Handle the remainder.
								if (remainder != 0)
								{
									// There is one more output pixel to be extracted.
									*pOut = ( (pIn[1] && 0x03) | (pIn[0] << 2) ) >> shift;
								}
							}
							break;
					}
				}
				break;
			case 8:
			default:
				{
					unsigned char *pOut = (unsigned char *)out;
					switch (inDepth)
					{
						case 12:
							// 3 input bytes for 2 output pixels.
							for (i = 0; i < pixelCount; i+= 2)
							{
								for (j = 0; j < 3; j++)
								{
									input[j] = (unsigned short)*pIn++;
								}							
								*pOut++ = (unsigned char)( (input[1] && 0x0F) | (input[0] << 4) ) >> shift;
								*pOut++ = (unsigned char)(((input[1] && 0xF0) >> 4) | (input[2] << 4 ) ) >> shift;
							}
							// Handle the remainder.
							if (remainder != 0)
							{
								// There is one more output pixel to be extracted.
								*pOut = (unsigned char)( (pIn[1] && 0x0F) | (pIn[0] << 4) ) >> shift;
							}
							break;
						default:
						case 10:
							// 3 input bytes for 2 output pixels.
							for (i = 0; i < pixelCount; i+= 2)
							{
								for (j = 0; j < 3; j++)
								{
									input[j] = (unsigned short)*pIn++;
								}							
								*pOut++ = (unsigned char)( (input[1] && 0x03) | (input[0] << 2) ) >> shift;
								*pOut++ = (unsigned char)(((input[1] && 0x30) >> 4) | (input[2] << 2 ) ) >> shift;
							}							
							// Handle the remainder.
							if (remainder != 0)
							{
								// There is one more output pixel to be extracted.
								*pOut = (unsigned char)( (pIn[1] && 0x03) | (pIn[0] << 2) ) >> shift;
							}
							break;
					}
				}
				break;
		}
	}
}

// Expand a mono8 value into and RGB triple with optional alpha_channel.
static inline void _expand_mono_to_rgb( unsigned char value, unsigned char *rgb_data, int alpha_channel)
{
	if ( rgb_data != NULL)
	{
		*rgb_data++ = value;
		*rgb_data++ = value;
		*rgb_data++ = value;
		if (alpha_channel)
		{
			*rgb_data++ = 0xFF;
		}
	}
}

static void Convert_MonoPacked_To_RGB8x(int alpha_channel, int pixelCount, void *in, int inDepth, void *out)
{
	// 10 or 12 bit in (packed) -> truncate to 8bpp and output as RGB888.
	unsigned short *pIn = (unsigned short *)in;
	unsigned short input[5];
	int i, j;
	int shift = 0;
	int remainder = 0;
	
	if ( (in != NULL) && (out != NULL))
	{
		unsigned char *pOut = (unsigned char *)out;
		unsigned short val = 0;
		shift = inDepth - 8;
		remainder = pixelCount % 2;
		switch (inDepth)
		{
			case 12:
				// 3 input bytes for 2 output pixels.
				for (i = 0; i < pixelCount; i+= 2)
				{
					for (j = 0; j < 3; j++)
					{
						input[j] = (unsigned short)*pIn++;
					}
					val = ( (input[1] && 0x0F) | (input[0] << 4) ) >> shift;
					_expand_mono_to_rgb( (unsigned char) val, pOut, alpha_channel);

					val = (((input[1] && 0xF0) >> 4) | (input[2] << 4) ) >> shift;
					_expand_mono_to_rgb( (unsigned char) val, pOut, alpha_channel);
				}
				// Handle the remainder.
				if (remainder != 0)
				{
					// There is one more output pixel to be extracted.
					val = ( (pIn[1] && 0x0F) | (pIn[0] << 4) ) >> shift;
					_expand_mono_to_rgb( (unsigned char) val, pOut, alpha_channel);
				}
				break;
			default:
			case 10:
				// 3 input bytes for 2 output pixels.
				for (i = 0; i < pixelCount; i+= 2)
				{
					for (j = 0; j < 3; j++)
					{
						input[j] = (unsigned short)*pIn++;
					}
					val = ( (input[1] && 0x03) | (input[0] << 2) ) >> shift;
					_expand_mono_to_rgb( (unsigned char) val, pOut, alpha_channel);

					val = (((input[1] && 0x30) >> 4) | (input[2] << 2) ) >> shift;
					_expand_mono_to_rgb( (unsigned char) val, pOut, alpha_channel);
				}
				// Handle the remainder.
				if (remainder != 0)
				{
					// There is one more output pixel to be extracted.
					val = ( (pIn[1] && 0x03) | (pIn[0] << 2) ) >> shift;
					_expand_mono_to_rgb( (unsigned char) val, pOut, alpha_channel);
				}
				break;
		}
	}
}

static void Convert_MonoPacked_To_RGB888(int pixelCount, void *in, int inDepth, void *out)
{
	Convert_MonoPacked_To_RGB8x(FALSE, pixelCount, in, inDepth, out);
}
static void Convert_MonoPacked_To_RGB8888(int pixelCount, void *in, int inDepth, void *out)
{
	Convert_MonoPacked_To_RGB8x(TRUE, pixelCount, in, inDepth, out);
}


static void Convert_Mono_To_RGB8x(int alpha_channel, int pixelCount, void *in, int inDepth, void *out)
{
	// 8, 10, 12, 14, 16 bit in -> truncate to 8bpp and output as RGB888.
	unsigned char *pOut = (unsigned char *)out;
	int shift = inDepth - 8;
	int i;
	
	if ( (in != NULL) && (out != NULL))
	{
		if (inDepth == 8)
		{
			unsigned char *pIn = (unsigned char *)in;
			for (i = 0; i < pixelCount; i+= 4)
			{
				*pOut++ = *pIn;
				*pOut++ = *pIn;
				*pOut++ = *pIn++;
				if (alpha_channel)
				{
					*pOut++ = 0xff;
				}
			}
		}
		else
		{
			unsigned short *pIn = (unsigned short *)in;
			unsigned short val = 0;
			for (i = 0; i < pixelCount; i+= 4)
			{
				val = (*pIn++ >> shift) & 0xff;
				*pOut++ = (unsigned char)val;
				*pOut++ = (unsigned char)val;
				*pOut++ = (unsigned char)val;
				if (alpha_channel)
				{
					*pOut++ = 0xff;
				}
			}
		}
	}
}

static void Convert_Mono_To_RGB888(int pixelCount, void *in, int inDepth, void *out)
{
	Convert_Mono_To_RGB8x(FALSE, pixelCount, in, inDepth, out);
}

static void Convert_Mono_To_RGB8888(int pixelCount, void *in, int inDepth, void *out)
{
	Convert_Mono_To_RGB8x(TRUE, pixelCount, in, inDepth, out);
}


static void Convert_RGB10V1Packed_To_RGB8x(int alpha_channel, int pixelCount, void *in, void *out)
{
	unsigned char *pOut = (unsigned char *)out;
	int i;
	
	if ( (in != NULL) && (out != NULL))
	{
		unsigned char *pIn = (unsigned char *)in;
		for (i = 0; i < pixelCount; i+=3)
		{
			pIn++;				// Skip 2 LSBs for each color.
			*pOut++ = *pIn++; // R
			*pOut++ = *pIn++; // G
			*pOut++ = *pIn++; // B
			if (alpha_channel)
			{
				*pOut++ = 0xff;
			}
		}
	}
}

static void Convert_RGB10V1Packed_To_RGB888(int pixelCount, void *in, void *out)
{
	Convert_RGB10V1Packed_To_RGB8x(FALSE, pixelCount, in, out);
}
static void Convert_RGB10V1Packed_To_RGB8888(int pixelCount, void *in, void *out)
{
	Convert_RGB10V1Packed_To_RGB8x(TRUE, pixelCount, in, out);
}


static void Convert_RGB10V2Packed_To_RGB8x(int alpha_channel, int pixelCount, void *in, void *out)
{
	unsigned char *pOut = (unsigned char *)out;
	int i;
	
	if ( (in != NULL) && (out != NULL))
	{
		unsigned char *pIn = (unsigned char *)in;
		unsigned char data[4];
		for (i = 0; i < pixelCount; i+=3)
		{
			for (i = 0; i < 4; i++)
			{
				data[i] = *pIn++;
			}
			*pOut++ = (data[0] >> 2) | ((data[1] & 0x03) << 6); // R
			*pOut++ = (data[1] >> 4) | ((data[2] & 0x0F) << 4); // G
			*pOut++ = (data[2] >> 6) | ((data[3] & 0x3F) << 2); // B
			if (alpha_channel)
			{
				*pOut++ = 0xff;
			}
		}
	}
}
static void Convert_RGB10V2Packed_To_RGB888(int pixelCount, void *in, void *out)
{
	Convert_RGB10V2Packed_To_RGB8x( FALSE, pixelCount, in, out);	
}

static void Convert_RGB10V2Packed_To_RGB8888(int pixelCount, void *in, void *out)
{
	Convert_RGB10V2Packed_To_RGB8x( TRUE, pixelCount, in, out);	
}

//======================================================================
// Bi-Color formats (from PFNC)
//

#define CORUTILFUNC

/* *RGBG* */
#define __BICOLOR_RGBG_TO_BGR( _type, _dst, _src, blue )\
   if(range > 0) {\
         *_dst++ = (_type)( ( blue ) >>  range) ;\
	      *_dst++ = (_type)((*( _src+ 1 )) >>  range) ;\
	      *_dst++ = (_type)((*( _src )) >>  range) ;\
   } else { \
         *_dst++ = (_type)( blue );\
	      *_dst++ = (_type)*( _src+ 1 );\
	      *_dst++ = (_type)(*_src );\
   }\
	_src+=2;
	


#define _BICOLOR_RGBG_TO_BGR( _type, _dst, _src )\
   __BICOLOR_RGBG_TO_BGR( _type, _dst, _src,  (*(_src-2) + *(_src+2)) / 2 )
#define _BICOLOR_RGBG_TO_BGR_FIRST( _type, _dst, _src )\
   __BICOLOR_RGBG_TO_BGR( _type, _dst, _src, (*(_src+2)) )
#define _BICOLOR_RGBG_TO_BGR_LAST( _type, _dst, _src )\
   __BICOLOR_RGBG_TO_BGR( _type, _dst, _src, (*(_src-2)) )
#define _BICOLOR_RGBG_TO_BGR_SINGLE_PIXEL( _type, _dst, _src )\
   __BICOLOR_RGBG_TO_BGR( _type, _dst, _src, 0 )

#define _BICOLOR_RGBG_TO_BGRA( _type, _dst, _src )\
         _BICOLOR_RGBG_TO_BGR( _type, _dst, _src );\
         _dst++
#define _BICOLOR_RGBG_TO_BGRA_FIRST( _type, _dst, _src )\
         _BICOLOR_RGBG_TO_BGR_FIRST( _type, _dst, _src );\
         _dst++

#define _BICOLOR_RGBG_TO_BGRA_LAST( _type, _dst, _src )\
         _BICOLOR_RGBG_TO_BGR_LAST( _type, _dst, _src );\
         _dst++

#define _BICOLOR_RGBG_TO_BGRA_SINGLE_PIXEL( _type, _dst, _src )\
         _BICOLOR_RGBG_TO_BGR_SINGLE_PIXEL( _type, _dst, _src );\
         _dst++



/* *BGRG* */
#define __BICOLOR_BGRG_TO_BGR( _type, _dst, _src, red )\
   if(range > 0) {\
         *_dst++ = (_type)(( *(_src+0)) >>  range) ;\
	      *_dst++ = (_type)(( *(_src+1)) >>  range) ;\
         *_dst++ = (_type)( ( red ) >>  range) ;\
   } else { \
         *_dst++ = (_type)(*(_src+0)) ;\
	      *_dst++ = (_type)(*(_src+1)) ;\
         *_dst++ = (_type)( red );\
   }\
   _src+=2;


#define _BICOLOR_BGRG_TO_BGR( _type, _dst, _src )\
   __BICOLOR_BGRG_TO_BGR( _type, _dst, _src,  ( (*(_src-2)) + (*(_src+2)) ) / 2 )
#define _BICOLOR_BGRG_TO_BGR_FIRST( _type, _dst, _src )\
   __BICOLOR_BGRG_TO_BGR( _type, _dst, _src, (*(_src+2)) )
#define _BICOLOR_BGRG_TO_BGR_LAST( _type, _dst, _src )\
   __BICOLOR_BGRG_TO_BGR( _type, _dst, _src, (*(_src-2)) )
#define _BICOLOR_BGRG_TO_BGR_SINGLE_PIXEL( _type, _dst, _src )\
   __BICOLOR_BGRG_TO_BGR( _type, _dst, _src, 0 )

#define _BICOLOR_BGRG_TO_BGRA( _type, _dst, _src )\
         _BICOLOR_BGRG_TO_BGR( _type, _dst, _src );\
         _dst++
#define _BICOLOR_BGRG_TO_BGRA_FIRST( _type, _dst, _src )\
         _BICOLOR_BGRG_TO_BGR_FIRST( _type, _dst, _src );\
         _dst++
#define _BICOLOR_BGRG_TO_BGRA_LAST( _type, _dst, _src )\
         _BICOLOR_BGRG_TO_BGR_LAST( _type, _dst, _src );\
         _dst++
#define _BICOLOR_BGRG_TO_BGRA_SINGLE_PIXEL( _type, _dst, _src )\
         _BICOLOR_BGRG_TO_BGR_SINGLE_PIXEL( _type, _dst, _src );\
         _dst++



/* RGBG -- ARGB */
#define __BICOLOR_BGRG_TO_RGB( _type, _dst, _src, red )\
   if(range > 0) {\
         *_dst++ = (_type)(( red ) >>  range) ;\
	      *_dst++ = (_type)(( *(_src+1)) >>  range) ; \
	      *_dst++ = (_type)(( *_src) >>  range) ;\
   } else { \
         *_dst++ = (_type)( red ) ;\
	      *_dst++ = (_type)( *(_src+1) ) ; \
	      *_dst++ = (_type)( *_src ) ;\
   }\
		   _src+=2

#define _BICOLOR_BGRG_TO_RGB( _type, _dst, _src )\
   __BICOLOR_BGRG_TO_RGB( _type, _dst, _src,  (*(_src-2) + *(_src+2)) / 2 )
#define _BICOLOR_BGRG_TO_RGB_FIRST( _type, _dst, _src )\
   __BICOLOR_BGRG_TO_RGB( _type, _dst, _src,  *(_src+2) )
#define _BICOLOR_BGRG_TO_RGB_LAST( _type, _dst, _src )\
   __BICOLOR_BGRG_TO_RGB( _type, _dst, _src,  *(_src-2) )
#define _BICOLOR_BGRG_TO_RGB_SINGLE_PIXEL( _type, _dst, _src )\
   __BICOLOR_BGRG_TO_RGB( _type, _dst, _src, 0 )

/* BGRG -- RGB */
#define __BICOLOR_RGBG_TO_RGB( _type, _dst, _src, blue )\
   if(range > 0) {\
	      *_dst++ = (_type)(( *_src++ ) >>  range) ; \
	      *_dst++ = (_type)(( *_src++ ) >>  range) ;\
         *_dst++ = (_type)(( blue ) >>  range) ;\
   } else { \
	      *_dst++ = (_type)( *_src++ ) ; \
	      *_dst++ = (_type)( *_src++ ) ;\
         *_dst++ = (_type)( blue ) ;\
   }

#define _BICOLOR_RGBG_TO_RGB( _type, _dst, _src )\
   __BICOLOR_RGBG_TO_RGB( _type, _dst, _src,  ((*_src) + *(_src-4)) / 2 )
#define _BICOLOR_RGBG_TO_RGB_FIRST( _type, _dst, _src )\
   __BICOLOR_RGBG_TO_RGB( _type, _dst, _src,  *(_src) )
#define _BICOLOR_RGBG_TO_RGB_LAST( _type, _dst, _src )\
   __BICOLOR_RGBG_TO_RGB( _type, _dst, _src,  *(_src-4) )
#define _BICOLOR_RGBG_TO_RGB_SINGLE_PIXEL( _type, _dst, _src )\
   __BICOLOR_RGBG_TO_RGB( _type, _dst, _src, 0 )

#ifdef __linux__
// Variadic macros definitions for gcc are slightly different than for Windows.
// gcc really did not like the nested variadic macros - so they are replaced by a single macro.

//#define __ADD_SUFIX( _name, _sufix ) _name##_sufix
//#define __BICOLOR_ADD_SUFIX( _format, _ALL, _WHERE ) __ADD_SUFIX(_BICOLOR_##_format, _TO_ )##__ADD_SUFIX(_ALL,_WHERE)

#define __BICOLOR_ADD_SUFIX( fmt, _ALL, _WHERE ) _BICOLOR_##fmt##_TO_##_ALL##_WHERE

#else

#define __ADD_SUFIX( _name, _sufix ) _name##_sufix
#define __BICOLOR_ADD_SUFIX( _format, _ALL, _WHERE ) __ADD_SUFIX(_BICOLOR_##_format, _TO_ )##__ADD_SUFIX(_ALL,_WHERE)

#endif

#define _BICOLOR_TO_LINE( _ALL, _ALL_INV, BUFFTYPE, _type, _count, _dst, _src ) \
	   if(_count > 2)\
      {\
         __BICOLOR_ADD_SUFIX( _ALL, BUFFTYPE, _FIRST) ( _type, _dst, _src);\
         _count--;\
      }\
      else if(_count)\
      {\
         __BICOLOR_ADD_SUFIX( _ALL, BUFFTYPE, _SINGLE_PIXEL) ( _type, _dst, _src);\
         _count--;\
      }\
	   while ( _count > 2)\
	   {\
         __BICOLOR_ADD_SUFIX( _ALL_INV, BUFFTYPE,) ( _type, _dst, _src);\
         __BICOLOR_ADD_SUFIX( _ALL, BUFFTYPE,) ( _type, _dst, _src);\
         _count-=2;\
	   }\
      if(_count == 2)\
      {\
         __BICOLOR_ADD_SUFIX( _ALL_INV, BUFFTYPE,) ( _type, _dst, _src);\
         __BICOLOR_ADD_SUFIX( _ALL, BUFFTYPE,_LAST) ( _type, _dst, _src);\
      }\
      else if(_count == 1)\
      {\
         __BICOLOR_ADD_SUFIX( _ALL_INV, BUFFTYPE,_LAST) ( _type, _dst, _src);\
      }



#define _RGB_TO_BICOLOR_LINE( BUFFTYPE, _ALL, _ALL_INV, _type, _count, _dst, _src ) \
	   while ( _count > 1)\
	   {\
         __BICOLOR_ADD_SUFIX( BUFFTYPE, _ALL, ) ( _type, _dst, _src);\
         __BICOLOR_ADD_SUFIX( BUFFTYPE, _ALL_INV, ) ( _type, _dst, _src);\
         _count-=2;\
	   }\
      if(_count)\
      {\
         __BICOLOR_ADD_SUFIX( BUFFTYPE, _ALL, ) ( _type, _dst, _src);\
      }
   
#define _BICOLOR_BGR_TO_BGRG( _type, _dst, _src )\
   if(range > 0) {\
         *_dst++ = (_type)( ( *_src++ ) >>   range);\
	      *_dst++ = (_type)( ( *_src++ ) >>  (_type) range);\
   } else { \
         *_dst++ =  (_type)( *_src++ ) ;\
	      *_dst++ =  (_type)( *_src++ ) ;\
   }\
         _src++;


#define _BICOLOR_BGRA_TO_BGRG( _type, _dst, _src )\
   _BICOLOR_BGR_TO_BGRG( _type, _dst, _src );\
   _src++;

#define _BICOLOR_BGR_TO_RGBG( _type, _dst, _src )\
   if(range > 0) {\
         *_dst++ = (_type)(*(_src+2) >> range);\
	      *_dst++ = (_type)(*(_src+1) >> range);\
   } else { \
         *_dst++ =  (_type)*(_src+2) ;\
	      *_dst++ =  (_type)*(_src+1) ;\
   }\
         _src+=3;

#define _BICOLOR_BGRA_TO_RGBG( _type, _dst, _src )\
   _BICOLOR_BGR_TO_RGBG( _type, _dst, _src );\
   _src++;


UINT32 CORUTILFUNC CorUtilConvertBicolor88toRGB888(void *pSrc, void *pDst, SIZE_T count, UINT32 alignmentBlueGrean)
{
	BYTE *pSrcData = (BYTE *)pSrc;
	BYTE *pDstData = (BYTE *)pDst;
   static const UINT32 range = 0;

   if( !alignmentBlueGrean)
   {
      _BICOLOR_TO_LINE(RGBG, BGRG, BGR, BYTE, count, pDstData,  pSrcData);
   }
   else
   {
      _BICOLOR_TO_LINE(BGRG, RGBG, BGR, BYTE, count, pDstData,  pSrcData);
   }

   return 0;
}

#ifdef RGB
#undef RGB
#endif
UINT32 CORUTILFUNC CorUtilConvertBicolor88toRGBR888(void *pSrc, void *pDst, SIZE_T count, UINT32 alignmentBlueGrean)
{
	BYTE *pSrcData = (BYTE *)pSrc;
	BYTE *pDstData = (BYTE *)pDst;
   static const UINT32 range = 0;

   if( !alignmentBlueGrean)
   {
      _BICOLOR_TO_LINE(RGBG, BGRG, RGB, BYTE, count, pDstData,  pSrcData);
   }
   else
   {
      _BICOLOR_TO_LINE(BGRG, RGBG, RGB, BYTE, count, pDstData,  pSrcData);
   }

   return 0;
}

UINT32 CORUTILFUNC CorUtilConvertBicolor88toRGB8888(void *pSrc, void *pDst, SIZE_T count, UINT32 alignmentBlueGrean)
{
	BYTE *pSrcData = (BYTE *)pSrc;
	BYTE *pDstData = (BYTE *)pDst;
   static const UINT32 range = 0;

   if( !alignmentBlueGrean)
   {
      _BICOLOR_TO_LINE(RGBG, BGRG, BGRA, BYTE, count, pDstData,  pSrcData);
   }
   else
   {
      _BICOLOR_TO_LINE(BGRG, RGBG, BGRA, BYTE, count, pDstData,  pSrcData);
   }

   return 0;
}


UINT32 CORUTILFUNC CorUtilConvertBicolor88toRGB161616(void *pSrc, void *pDst, SIZE_T count, UINT32 alignmentBlueGrean)
{
	BYTE *pSrcData = (BYTE *)pSrc;
	WORD *pDstData = (WORD *)pDst;
   static const UINT32 range = 0;

   if( !alignmentBlueGrean)
   {
      _BICOLOR_TO_LINE(RGBG, BGRG, BGR, WORD, count, pDstData,  pSrcData);
   }
   else
   {
      _BICOLOR_TO_LINE(BGRG, RGBG, BGR, WORD, count, pDstData,  pSrcData);
   }

   return 0;
}


UINT32 CORUTILFUNC CorUtilConvertBicolor88toRGB16161616(void *pSrc, void *pDst, SIZE_T count, UINT32 alignmentBlueGrean)
{
	BYTE *pSrcData = (BYTE *)pSrc;
	WORD *pDstData = (WORD *)pDst;
   static const UINT32 range = 0;

   if( !alignmentBlueGrean)
   {
      _BICOLOR_TO_LINE(RGBG, BGRG, BGRA, WORD, count, pDstData,  pSrcData);
   }
   else
   {
      _BICOLOR_TO_LINE(BGRG, RGBG, BGRA, WORD, count, pDstData,  pSrcData);
   }

   return 0;
}



UINT32 CORUTILFUNC CorUtilConvertBicolor88toBicolor88(void *pSrc, void *pDst, SIZE_T count)
{
	WORD *pSrcData = (WORD *)pSrc;
	WORD *pDstData = (WORD *)pDst;
   //static const UINT32 range = 0;

   while ( count-- > 0)
   {
      *pDstData = ((*pSrcData >> 8) &0xFF) |  ((*pSrcData & 0xFF ) << 8);
      pSrcData++;
      pDstData++;
   }
   return 0;
}
UINT32 CORUTILFUNC CorUtilConvertBicolor88toBicolor1616(void *pSrc, void *pDst, SIZE_T count, UINT32 flipAlignment)
{
	WORD *pSrcData = (WORD *)pSrc;
	DWORD *pDstData = (DWORD *)pDst;

   if( flipAlignment )
   {
      while ( count-- > 0)
      {
         *pDstData = ((*pSrcData >> 8) &0xFF) |  (((DWORD)(*pSrcData & 0xFF )) << 16);
         pSrcData++;
         pDstData++;
      }
   }
   else
   {
      while ( count-- > 0)
      {
         *pDstData = ((*pSrcData) &0xFF) |  (((DWORD)(*pSrcData & 0xFF00 )) << 8);
         pSrcData++;
         pDstData++;
      }
   }
   return 0;
}

UINT32 CORUTILFUNC CorUtilConvertBicolor1616toRGB888(void *pSrc, void *pDst, SIZE_T count, UINT32 range,  BOOL32 alignmentBlueGrean)
{
	WORD *pSrcData = (WORD *)pSrc;
	BYTE *pDstData = (BYTE *)pDst;

   if( !alignmentBlueGrean)
   {
      _BICOLOR_TO_LINE(RGBG, BGRG, BGR, BYTE, count, pDstData,  pSrcData);
   }
   else
   {
      _BICOLOR_TO_LINE(BGRG, RGBG, BGR, BYTE, count, pDstData,  pSrcData);
   }
   return 0;

}
UINT32 CORUTILFUNC CorUtilConvertBicolor1616toRGB8888(void *pSrc, void *pDst, SIZE_T count, UINT32 range,  BOOL32 alignmentBlueGrean)
{
	WORD *pSrcData = (WORD *)pSrc;
	BYTE *pDstData = (BYTE *)pDst;

   if( !alignmentBlueGrean)
   {
      _BICOLOR_TO_LINE(RGBG, BGRG, BGRA, BYTE, count, pDstData,  pSrcData);
   }
   else
   {
      _BICOLOR_TO_LINE(BGRG, RGBG, BGRA, BYTE, count, pDstData,  pSrcData);
   }
   return 0;

}
UINT32 CORUTILFUNC CorUtilConvertBicolor1616toRGBR888(void *pSrc, void *pDst, SIZE_T count, UINT32 range,  BOOL32 alignmentBlueGrean)
{
	WORD *pSrcData = (WORD *)pSrc;
	BYTE *pDstData = (BYTE *)pDst;

   if( !alignmentBlueGrean)
   {
      _BICOLOR_TO_LINE(RGBG, BGRG, RGB, BYTE, count, pDstData,  pSrcData);
   }
   else
   {
      _BICOLOR_TO_LINE(BGRG, RGBG, RGB, BYTE, count, pDstData,  pSrcData);
   }
   return 0;

}


UINT32 CORUTILFUNC CorUtilConvertBicolor1616toRGB161616(void *pSrc, void *pDst, SIZE_T count, UINT32 range, UINT32 alignmentBlueGrean)
{
	WORD *pSrcData = (WORD *)pSrc;
	WORD *pDstData = (WORD *)pDst;

   if( !alignmentBlueGrean)
   {
      _BICOLOR_TO_LINE(RGBG, BGRG, BGR, WORD, count, pDstData,  pSrcData);
   }
   else
   {
      _BICOLOR_TO_LINE(BGRG, RGBG, BGR, WORD, count, pDstData,  pSrcData);
   }

   return 0;
}

UINT32 CORUTILFUNC CorUtilConvertBicolor1616toRGB16161616(void *pSrc, void *pDst, SIZE_T count, UINT32 range, UINT32 alignmentBlueGrean)
{
	WORD *pSrcData = (WORD *)pSrc;
	WORD *pDstData = (WORD *)pDst;

   if( !alignmentBlueGrean)
   {
      _BICOLOR_TO_LINE(RGBG, BGRG, BGRA, WORD, count, pDstData,  pSrcData);
   }
   else
   {
      _BICOLOR_TO_LINE(BGRG, RGBG, BGRA, WORD, count, pDstData,  pSrcData);
   }

   return 0;
}

UINT32 CORUTILFUNC CorUtilConvertBicolor1616toBicolor1616(void *pSrc, void *pDst, SIZE_T count, UINT32 range,  BOOL32 aligned)
{
	DWORD *pSrcData = (DWORD *)pSrc;
	DWORD *pDstData = (DWORD *)pDst;
   if( aligned && range == 0)
   {
      memcpy( pDst, pSrc, count * sizeof(DWORD));
   }
   else if (aligned && range > 0)
   {
      while ( count-- > 0)
      {
         *pDstData = ((*pSrcData & 0xFFFF ) >> (DWORD)range) | ((*pSrcData >> (DWORD)range) &0xFFFF0000);
         pSrcData++;
         pDstData++;
      }
   }
   else 
   {
      while ( count-- > 0)
      {
         DWORD tmpVal = ((*pSrcData >> 16) &0xFFFF) |  ((*pSrcData & 0xFFFF ) << 16);
         *pDstData = ((tmpVal & 0xFFFF ) >> (DWORD)range) | ((tmpVal >> (DWORD)range) &0xFFFF0000);
         pSrcData++;
         pDstData++;
      }
   }
   return 0;
}

// BiColor Conversion functions
void Convert_BiColor88toRGB8888( int w, int h, void *in, void *out, UINT32 alignmentBlueGreen)
{
	unsigned char *pIn = (unsigned char *)in;
	unsigned char *pOut = (unsigned char *)out;
	uint32_t inPitch = 2*w;
	uint32_t outPitch = 4*w;
	
	if ( (pIn != NULL) && (pOut != NULL))
	{
		int i = 0;
		for (i = 0; i < h; i++)
		{
			CorUtilConvertBicolor88toRGB8888( pIn, pOut, w, alignmentBlueGreen);
			pOut += outPitch;
			pIn  += inPitch;
		}
	}
}

void Convert_BiColor1616toRGB8888( int w, int h, void *in, void *out, UINT32 range, UINT32 alignmentBlueGreen)
{
	unsigned char *pIn = (unsigned char *)in;
	unsigned char *pOut = (unsigned char *)out;
	uint32_t inPitch = 4*w;
	uint32_t outPitch = 4*w;
	
	if ( (pIn != NULL) && (pOut != NULL))
	{
		int i = 0;
		for (i = 0; i < h; i++)
		{
			CorUtilConvertBicolor1616toRGB8888( pIn, pOut, w, range, alignmentBlueGreen);
			pOut += outPitch;
			pIn  += inPitch;
		}
	}
}



//======================================================================
// Generic converter for Display
void ConvertGevImageToX11Format( int w, int h, int gev_depth, int gev_format, void *gev_input_data, 
											int x11_depth, int x11_format, void *x11_output_data)
{
	int numPixels = 0;

	if ((gev_input_data != NULL) && (x11_output_data != NULL))
	{
		// Only allow Mono and RGB8888 as X11 formats.
		switch (x11_format)
		{
			case CORX11_DATA_FORMAT_MONO:
				{
					switch(gev_format)
					{
						case fmtBayerBG10Packed:
						case fmtBayerGB10Packed:
						case fmtBayerGR10Packed:
						case fmtBayerRG10Packed:
						case fmtBayerBG12Packed:
						case fmtBayerGB12Packed:
						case fmtBayerGR12Packed:
						case fmtBayerRG12Packed:
						case fmtMono10Packed:
						case fmtMono12Packed:
							numPixels =  w*h;
							Convert_MonoPacked_To_Mono(numPixels, gev_input_data, gev_depth, x11_depth, x11_output_data);
							break;
						case fmtYUV411packed:
							numPixels =  w*h;
							Convert_YUV411_To_Mono(numPixels, gev_input_data, x11_depth, x11_output_data);
							break;
						case fmtYUV422packed:
							numPixels = w*h*2;
							Convert_YUV422_To_Mono(numPixels, gev_input_data, x11_depth, x11_output_data);
							break;
						case fmtYUV444packed:
							numPixels = w*h*3;
							Convert_YUV444_To_Mono(numPixels, gev_input_data, x11_depth, x11_output_data);
							break;
						default:
							// Zero the output buffer until these are supported.
							memset(x11_output_data, 0, numPixels*((x11_depth + 7)/8));
							break;
					}
				}
				break;
			case CORX11_DATA_FORMAT_RGB8888:
				{
					numPixels = w*h;
					switch(gev_format)
					{
						case fmtYUV411packed:
							Convert_YUV411_To_RGB8888(numPixels, gev_input_data, x11_depth, x11_output_data);
							break;
						case fmtYUV422packed:
							Convert_YUV422_To_RGB8888(numPixels, gev_input_data, x11_depth, x11_output_data);
							break;
						case fmtYUV444packed:
							Convert_YUV444_To_RGB8888(numPixels, gev_input_data, x11_depth, x11_output_data);
							break;

						case fmtRGB8Packed:
								Convert_RGBPacked_To_RGB8888(numPixels, gev_input_data, gev_depth, x11_output_data);
								break;
						case fmtRGB10Packed:
						case fmtRGB12Packed:
								Convert_RGBPacked_To_RGB101010(numPixels, gev_input_data, gev_depth, x11_output_data);
								break;
						case fmtBGR8Packed:
								Convert_BGRPacked_To_RGB8888(numPixels, gev_input_data, gev_depth, x11_output_data);
								break;
						case fmtBGR10Packed:
						case fmtBGR12Packed:
								Convert_BGRPacked_To_RGB101010(numPixels, gev_input_data, gev_depth, x11_output_data);
								break;
						case fmtRGB10V1Packed:
								Convert_RGB10V1Packed_To_RGB8888(numPixels, gev_input_data, x11_output_data);
								break;
						case fmtRGB10V2Packed:
								Convert_RGB10V2Packed_To_RGB8888(numPixels, gev_input_data, x11_output_data);
								break;
						case fmt_PFNC_BiColorBGRG8:      // Bi-color Blue/Green - Red/Green 8-bit 
								Convert_BiColor88toRGB8888( w, h, gev_input_data, x11_output_data, 1);
								break;
						case fmt_PFNC_BiColorRGBG8:      //Bi-color Red/Green - Blue/Green 8-bit 
								Convert_BiColor88toRGB8888( w, h, gev_input_data, x11_output_data, 0);
								break;
						case fmt_PFNC_BiColorBGRG10:   // Bi-color Blue/Green - Red/Green 10-bit unpacked 
						case fmt_PFNC_BiColorBGRG12:   // Bi-color Blue/Green - Red/Green 12-bit unpacked 
								Convert_BiColor1616toRGB8888( w, h, gev_input_data, x11_output_data, (16-gev_depth), 1);  
								break;
						case fmt_PFNC_BiColorRGBG10:   // Bi-color Red/Green - Blue/Green 10-bit unpacked 
						case fmt_PFNC_BiColorRGBG12:   // Bi-color Red/Green - Blue/Green 12-bit unpacked 
								Convert_BiColor1616toRGB8888( w, h, gev_input_data, x11_output_data, (16-gev_depth), 0);
								break;
								
						case fmtBayerBG8:
						case fmtBayerGB8:
						case fmtBayerGR8:
						case fmtBayerRG8:
						case fmtBayerBG10:
						case fmtBayerGB10:
						case fmtBayerGR10:
						case fmtBayerRG10:
						case fmtBayerBG12:
						case fmtBayerGB12:
						case fmtBayerGR12:
						case fmtBayerRG12:
								// Use generic Bayer converter - convert to RGB8888 (for display)...
								// (Note: SaperaLT's RGB8888 is actually BGR (Blue is in byte 0).
								ConvertBayerToRGB( 0, h, w, gev_format, gev_input_data, fmtBGRA8Packed, x11_output_data);
								break;

						// Add these as they are supported.
						case fmt_PFNC_BiColorBGRG10p:  // Bi-color Blue/Green - Red/Green 10-bit packed 
						case fmt_PFNC_BiColorBGRG12p:  // Bi-color Blue/Green - Red/Green 12-bit packed 
						case fmt_PFNC_BiColorRGBG10p:  // Bi-color Red/Green - Blue/Green 10-bit packed 
						case fmt_PFNC_BiColorRGBG12p:  // Bi-color Red/Green - Blue/Green 12-bit packed 
						default:
							// Zero the output buffer until these are supported.
							memset(x11_output_data, 0, numPixels*((x11_depth + 7)/8));
							break;
					}
				}
				break;
			default:
				break;
		}
	}
}

void ConvertGevImageToRGB8888Format( int w, int h, int gev_depth, int gev_format, void *gev_input_data, void *rgb_output_data)
{
	int numPixels = 0;
	int outdepth = 32;

	if ((gev_input_data != NULL) && (rgb_output_data != NULL))
	{
		// RGB8888 as output format.
		numPixels = w*h;
		switch(gev_format)
		{
			case fmtBayerGR8:		/* 8-bit Bayer    */
			case fmtBayerRG8:		/* 8-bit Bayer    */
			case fmtBayerGB8:		/* 8-bit Bayer    */
			case fmtBayerBG8:		/* 8-bit Bayer    */
			case fmtBayerGR10:	/* 10-bit Bayer   */
			case fmtBayerRG10:	/* 10-bit Bayer   */
			case fmtBayerGB10:	/* 10-bit Bayer   */
			case fmtBayerBG10:	/* 10-bit Bayer   */
			case fmtBayerGR12:	/* 12-bit Bayer   */
			case fmtBayerRG12:	/* 12-bit Bayer   */
			case fmtBayerGB12:	/* 12-bit Bayer   */
			case fmtBayerBG12:	/* 12-bit Bayer   */
				/* For Now : treat the bayer formats as monochrome - add bayer decoding later */
				Convert_Mono_To_RGB8888(numPixels, gev_input_data, gev_depth, rgb_output_data);
				break;
			case fmtMono8:
			case fmtMono8Signed:
			case fmtMono10:
			case fmtMono12:
			case fmtMono14:
			case fmtMono16:
				Convert_Mono_To_RGB8888(numPixels, gev_input_data, gev_depth, rgb_output_data);
				break;
			case fmtMono10Packed:
			case fmtMono12Packed:
				Convert_MonoPacked_To_RGB8888(numPixels, gev_input_data, gev_depth, rgb_output_data);
				break;
			case fmtYUV411packed:
				Convert_YUV411_To_RGB8888(numPixels, gev_input_data, outdepth, rgb_output_data);
				break;
			case fmtYUV422packed:
				Convert_YUV422_To_RGB8888(numPixels, gev_input_data, outdepth, rgb_output_data);
				break;
			case fmtYUV444packed:
				Convert_YUV444_To_RGB8888(numPixels, gev_input_data, outdepth, rgb_output_data);
				break;
			case fmtRGB8Packed:
			case fmtRGB10Packed:
			case fmtRGB12Packed:
				Convert_RGBPacked_To_RGB8888(numPixels, gev_input_data, outdepth, rgb_output_data);
				break;
			case fmtBGR8Packed:
			case fmtBGR10Packed:
			case fmtBGR12Packed:
				Convert_BGRPacked_To_RGB8888(numPixels, gev_input_data, outdepth, rgb_output_data);
				break;
		case fmtRGB10V1Packed:
				Convert_RGB10V1Packed_To_RGB8888(numPixels, gev_input_data, rgb_output_data);
				break;
		case fmtRGB10V2Packed:
				Convert_RGB10V2Packed_To_RGB8888(numPixels, gev_input_data, rgb_output_data);
				break;
			default:
				// Zero the output buffer until these are supported.
				memset(rgb_output_data, 0, numPixels*((outdepth + 7)/8));
				break;
		}
	}
}

void ConvertGevImageToRGB888Format( int w, int h, int gev_depth, int gev_format, void *gev_input_data, void *rgb_output_data)
{
	int numPixels = 0;
	int outdepth = 24;

	if ((gev_input_data != NULL) && (rgb_output_data != NULL))
	{
		// RGB888 as output format.
		numPixels = w*h;
		switch(gev_format)
		{
			case fmtBayerGR8:		/* 8-bit Bayer    */
			case fmtBayerRG8:		/* 8-bit Bayer    */
			case fmtBayerGB8:		/* 8-bit Bayer    */
			case fmtBayerBG8:		/* 8-bit Bayer    */
			case fmtBayerGR10:	/* 10-bit Bayer   */
			case fmtBayerRG10:	/* 10-bit Bayer   */
			case fmtBayerGB10:	/* 10-bit Bayer   */
			case fmtBayerBG10:	/* 10-bit Bayer   */
			case fmtBayerGR12:	/* 12-bit Bayer   */
			case fmtBayerRG12:	/* 12-bit Bayer   */
			case fmtBayerGB12:	/* 12-bit Bayer   */
			case fmtBayerBG12:	/* 12-bit Bayer   */
				/* For Now : treat the bayer formats as monochrome - add bayer decoding later */
				Convert_Mono_To_RGB888(numPixels, gev_input_data, gev_depth, rgb_output_data);
				break;
			case fmtMono8:
			case fmtMono8Signed:
			case fmtMono10:
			case fmtMono12:
			case fmtMono14:
			case fmtMono16:
				Convert_Mono_To_RGB888(numPixels, gev_input_data, gev_depth, rgb_output_data);
				break;
			case fmtMono10Packed:
			case fmtMono12Packed:
				Convert_MonoPacked_To_RGB888(numPixels, gev_input_data, gev_depth, rgb_output_data);
				break;
			case fmtYUV411packed:
				Convert_YUV411_To_RGB888(numPixels, gev_input_data, outdepth, rgb_output_data);
				break;
			case fmtYUV422packed:
				Convert_YUV422_To_RGB888(numPixels, gev_input_data, outdepth, rgb_output_data);
				break;
			case fmtYUV444packed:
				Convert_YUV444_To_RGB888(numPixels, gev_input_data, outdepth, rgb_output_data);
				break;
			case fmtRGB8Packed:
			case fmtRGB10Packed:
			case fmtRGB12Packed:
				Convert_RGBPacked_To_RGB888(numPixels, gev_input_data, outdepth, rgb_output_data);
				break;
			case fmtBGR8Packed:
			case fmtBGR10Packed:
			case fmtBGR12Packed:
				Convert_BGRPacked_To_RGB888(numPixels, gev_input_data, outdepth, rgb_output_data);
				break;
		case fmtRGB10V1Packed:
				Convert_RGB10V1Packed_To_RGB888(numPixels, gev_input_data, rgb_output_data);
				break;
		case fmtRGB10V2Packed:
				Convert_RGB10V2Packed_To_RGB888(numPixels, gev_input_data, rgb_output_data);
				break;
			default:
				// Zero the output buffer until these are supported.
				memset(rgb_output_data, 0, numPixels*((outdepth + 7)/8));
				break;
		}
	}
}


