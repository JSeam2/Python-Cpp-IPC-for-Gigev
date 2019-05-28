/****************************************************************************** 
Copyright (c) 2018, Teledyne DALSA Inc.
All rights reserved.

File : convertBayer.c
	Naive and simple Bayer to RGB conversion functions.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions 
are met:
	-Redistributions of source code must retain the above copyright 
	notice, this list of conditions and the following disclaimer. 
	-Redistributions in binary form must reproduce the above 
	copyright notice, this list of conditions and the following 
	disclaimer in the documentation and/or other materials provided 
	with the distribution. 
	-Neither the name of Teledyne DALSA nor the names of its contributors 
	may be used to endorse or promote products derived 
	from this software without specific prior written permission. 

===============================================================================
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/
/*! \file convertBayer.c
\brief Bayer conversion functions (naive and simple).

*/
#include "gevapi.h"

// bayerAlignment: 0=B1G1,  1=B1G0,  2=B0G0,  3=B0G1  
// bayerAlignemnt: 0=GB_RG, 1=BG_GR, 2=RG_GB, 3=GR_BG
// bayerAlignemnt: 0=GB,    1=BG,    2=RG,    3=GR_BG


// Do a horizontal swath (row) - 8 bit (special case - fastest).
static void _convBayer8ToRGB8_2x2( void* pSrcLine0, void* pSrcLine1, void* pDstR, void *pDstG, void *pDstB, unsigned int dstInc,
												unsigned int count, unsigned int iAlignment, int bIsLastLine, int bIncludeLastPixel )
 {
   UINT32 i;
   uint32_t vR = 0;
   uint32_t vG = 0;
   uint32_t vB = 0;
	uint32_t currentAlignment = (uint32_t)iAlignment;
	unsigned char *pSrc0  = (unsigned char *)pSrcLine0;
	unsigned char *pSrc1  = (unsigned char *)pSrcLine1;
	unsigned char *pRed   = (unsigned char *)pDstR;
	unsigned char *pGreen = (unsigned char *)pDstG;
	unsigned char *pBlue  = (unsigned char *)pDstB;

	if (bIncludeLastPixel)
	{
		count--;
	}
	
	if ( !bIsLastLine )
	{
 		// Not the last line.
		for(i = 0; i < count; ++i)
		{
			switch(currentAlignment)
			{
				case 0: // GB_RG
					vR = ((uint32_t)pSrc1[0]); 
					vB = ((uint32_t)pSrc0[1]);
					vG = ((vR + vB + 1) >> 1);  
					break;

				case 1: // BG_GR
					vR = ((uint32_t)pSrc1[1]);
					vB = ((uint32_t)pSrc0[0]);
					vG = ((vR + vB + 1) >> 1); 
					break;

				case 2: // RG_GB
					vR = ((uint32_t)pSrc0[0]);
					vB = ((uint32_t)pSrc1[1]);
					vG = ((vR + vB + 1) >> 1); 
					break;

				case 3: // GR_BG
					vR = ((uint32_t)pSrc0[1]);
					vB = ((uint32_t)pSrc1[0]);
					vG = ((vR + vB + 1) >> 1); 
					break;
			}

			*pRed   = (unsigned char)vR;
			*pGreen = (unsigned char)vG;
			*pBlue  = (unsigned char)vB;
		
			pRed   += dstInc;
			pGreen += dstInc;
			pBlue  += dstInc;
				
			pSrc0++;
			pSrc1++;
			currentAlignment ^= 1;
		}

		// Do the last pixel (if requested).
      if( bIncludeLastPixel )
		{
			switch(currentAlignment)
			{
				case 0: // GB_RG
					vR = pSrc1[0];
					vG = pSrc0[0];
					vB = pSrc0[-1];
					break;

				case 1: // BG_GR
					vR = pSrc1[-1];
					vG = pSrc1[0];
					vB = pSrc0[0];
					break;

				case 2: // RG_GB
					vR = pSrc0[0];
					vG = pSrc1[0];
					vB = pSrc1[-1];
					break;

				case 3: // GR_BG
					vR = pSrc0[-1];
					vG = pSrc0[0];
					vB = pSrc1[0];
					break;
			}

			*pRed   = (unsigned char)vR;
			*pGreen = (unsigned char)vG;
			*pBlue  = (unsigned char)vB;
		}
	}
	else
	{
		// Last line.
		// Here pSrc0 is the real last line while pSrc1 must point to the last before last line
		for(i = 0; i < count; ++i)
		{
			switch(currentAlignment)
			{
				case 0: // GB_RG
					vR = pSrc1[0];
					vG = pSrc0[0];
					vB = pSrc0[1];
					break;

				case 1: // BG_GR
					vR = pSrc1[1];
					vG = pSrc0[1];
					vB = pSrc0[0];
					break;

				case 2: // RG_GB
					vR = pSrc0[0];
					vG = pSrc0[1];
					vB = pSrc1[1];
					break;

				case 3: // GR_BG
					vR = pSrc0[1];
					vG = pSrc0[0];
					vB = pSrc1[0];
					break;
			}

			*pRed   = (unsigned char)vR;
			*pGreen = (unsigned char)vG;
			*pBlue  = (unsigned char)vB;
			
			pRed   += dstInc;
			pGreen += dstInc;
			pBlue  += dstInc;
					
			pSrc0++;
			pSrc1++;
			currentAlignment ^= 1;
		}

		// Do the last pixel (if requested).
		if( bIncludeLastPixel )
		{
			switch(currentAlignment)
			{
				case 0: // GB_RG
					vR = pSrc1[0];
					vG = pSrc0[0];
					vB = pSrc0[-1];
					break;

				case 1: // BG_GR
					vR = pSrc1[-1];
					vG = pSrc1[0];
					vB = pSrc0[0];
					break;

				case 2: // RG_GB
					vR = pSrc0[0];
					vG = pSrc1[0];
					vB = pSrc1[-1];
					break;

				case 3: // GR_BG
					vR = pSrc0[-1];
					vG = pSrc0[0];
					vB = pSrc1[0];
					break;
			}

			*pRed   = (unsigned char)vR;
			*pGreen = (unsigned char)vG;
			*pBlue  = (unsigned char)vB;
		}			
	}
}

// Do a horizontal swath (row) - 16 bit.
static void _convBayer16ToRGB16_2x2( void* pSrcLine0, void* pSrcLine1, unsigned int srcDepth, void* pDstR, void *pDstG, void *pDstB, 
												unsigned int dstInc, unsigned int dstDepth,
												unsigned int count, unsigned int iAlignment, int bIsLastLine, int bIncludeLastPixel )
 {
   UINT32 i;
   uint32_t vR = 0;
   uint32_t vG = 0;
   uint32_t vB = 0;
	uint32_t currentAlignment = (uint32_t)iAlignment;
	uint16_t	*pSrc0  = (uint16_t *)pSrcLine0;
	uint16_t	*pSrc1  = (uint16_t *)pSrcLine1;
	uint16_t *pRed   = (uint16_t *)pDstR;
	uint16_t *pGreen = (uint16_t *)pDstG;
	uint16_t *pBlue  = (uint16_t *)pDstB;
	
	int outputShift = srcDepth - dstDepth;
	
	if (bIncludeLastPixel)
	{
		count--;
	}
	if ( !bIsLastLine )
	{
 		// Not the last line.
		for(i = 0; i < count; ++i)
		{
			switch(currentAlignment)
			{
				case 0: // GB_RG
					vR = ((uint32_t)pSrc1[0]); 
					vB = ((uint32_t)pSrc0[1]);
					vG = ((vR + vB + 1) >> 1);  
					break;

				case 1: // BG_GR
					vR = ((uint32_t)pSrc1[1]);
					vB = ((uint32_t)pSrc0[0]);
					vG = ((vR + vB + 1) >> 1); 
					break;

				case 2: // RG_GB
					vR = ((uint32_t)pSrc0[0]);
					vB = ((uint32_t)pSrc1[1]);
					vG = ((vR + vB + 1) >> 1); 
					break;

				case 3: // GR_BG
					vR = ((uint32_t)pSrc0[1]);
					vB = ((uint32_t)pSrc1[0]);
					vG = ((vR + vB + 1) >> 1); 
					break;
			}

			*pRed   = (uint16_t)(vR >> outputShift);
			*pGreen = (uint16_t)(vG >> outputShift);
			*pBlue  = (uint16_t)(vB >> outputShift);
		
			pRed   += dstInc;
			pGreen += dstInc;
			pBlue  += dstInc;
				
			pSrc0++;
			pSrc1++;
			currentAlignment ^= 1;
		}

		// Do the last pixel (if requested).
      if( bIncludeLastPixel )
		{
			switch(currentAlignment)
			{
				case 0: // GB_RG
					vR = pSrc1[0];
					vG = pSrc0[0];
					vB = pSrc0[-1];
					break;

				case 1: // BG_GR
					vR = pSrc1[-1];
					vG = pSrc1[0];
					vB = pSrc0[0];
					break;

				case 2: // RG_GB
					vR = pSrc0[0];
					vG = pSrc1[0];
					vB = pSrc1[-1];
					break;

				case 3: // GR_BG
					vR = pSrc0[-1];
					vG = pSrc0[0];
					vB = pSrc1[0];
					break;
			}

			*pRed   = (uint16_t)(vR >> outputShift);
			*pGreen = (uint16_t)(vG >> outputShift);
			*pBlue  = (uint16_t)(vB >> outputShift);
		}
	}
	else
	{
		// Last line.
		// Here pSrc0 is the real last line while pSrc1 must point to the last before last line
		for(i = 0; i < count; ++i)
		{
			switch(currentAlignment)
			{
				case 0: // GB_RG
					vR = pSrc1[0];
					vG = pSrc0[0];
					vB = pSrc0[1];
					break;

				case 1: // BG_GR
					vR = pSrc1[1];
					vG = pSrc0[1];
					vB = pSrc0[0];
					break;

				case 2: // RG_GB
					vR = pSrc0[0];
					vG = pSrc0[1];
					vB = pSrc1[1];
					break;

				case 3: // GR_BG
					vR = pSrc0[1];
					vG = pSrc0[0];
					vB = pSrc1[0];
					break;
			}

			*pRed   = (uint16_t)(vR >> outputShift);
			*pGreen = (uint16_t)(vG >> outputShift);
			*pBlue  = (uint16_t)(vB >> outputShift);
		
			pRed   += dstInc;
			pGreen += dstInc;
			pBlue  += dstInc;
				
			pSrc0++;
			pSrc1++;
			currentAlignment ^= 1;
		}

		// Do the last pixel (if requested).
		if( bIncludeLastPixel )
		{
			switch(currentAlignment)
			{
				case 0: // GB_RG
					vR = pSrc1[0];
					vG = pSrc0[0];
					vB = pSrc0[-1];
					break;

				case 1: // BG_GR
					vR = pSrc1[-1];
					vG = pSrc1[0];
					vB = pSrc0[0];
					break;

				case 2: // RG_GB
					vR = pSrc0[0];
					vG = pSrc1[0];
					vB = pSrc1[-1];
					break;

				case 3: // GR_BG
					vR = pSrc0[-1];
					vG = pSrc0[0];
					vB = pSrc1[0];
					break;
			}

			*pRed   = (uint16_t)(vR >> outputShift);
			*pGreen = (uint16_t)(vG >> outputShift);
			*pBlue  = (uint16_t)(vB >> outputShift);
		}			
	}
}

// Do a horizontal swath (row) - 16 bit in - 8 bit (x3) out.
static void _convBayer16ToRGB8_2x2( void* pSrcLine0, void* pSrcLine1, unsigned int srcDepth, void* pDstR, void *pDstG, void *pDstB, 
											unsigned int dstInc,	unsigned int count, unsigned int iAlignment, int bIsLastLine, int bIncludeLastPixel )
 {
   UINT32 i;
   uint32_t vR = 0;
   uint32_t vG = 0;
   uint32_t vB = 0;
	uint32_t currentAlignment = (uint32_t)iAlignment;
	uint16_t	*pSrc0  = (uint16_t *)pSrcLine0;
	uint16_t	*pSrc1  = (uint16_t *)pSrcLine1;
	unsigned char *pRed   = (unsigned char *)pDstR;
	unsigned char *pGreen = (unsigned char *)pDstG;
	unsigned char *pBlue  = (unsigned char *)pDstB;
	
	int outputShift = srcDepth - 8;
	
	if (bIncludeLastPixel)
	{
		count--;
	}
	if ( !bIsLastLine )
	{
 		// Not the last line.
		for(i = 0; i < count; ++i)
		{
			switch(currentAlignment)
			{
				case 0: // GB_RG
					vR = ((uint32_t)pSrc1[0]); 
					vB = ((uint32_t)pSrc0[1]);
					vG = ((vR + vB + 1) >> 1);  
					break;

				case 1: // BG_GR
					vR = ((uint32_t)pSrc1[1]);
					vB = ((uint32_t)pSrc0[0]);
					vG = ((vR + vB + 1) >> 1); 
					break;

				case 2: // RG_GB
					vR = ((uint32_t)pSrc0[0]);
					vB = ((uint32_t)pSrc1[1]);
					vG = ((vR + vB + 1) >> 1); 
					break;

				case 3: // GR_BG
					vR = ((uint32_t)pSrc0[1]);
					vB = ((uint32_t)pSrc1[0]);
					vG = ((vR + vB + 1) >> 1); 
					break;
			}

			*pRed   = (unsigned char)(vR >> outputShift);
			*pGreen = (unsigned char)(vG >> outputShift);
			*pBlue  = (unsigned char)(vB >> outputShift);
		
			pRed   += dstInc;
			pGreen += dstInc;
			pBlue  += dstInc;
				
			pSrc0++;
			pSrc1++;
			currentAlignment ^= 1;
		}

		// Do the last pixel (if requested).
      if( bIncludeLastPixel )
		{
			switch(currentAlignment)
			{
				case 0: // GB_RG
					vR = pSrc1[0];
					vG = pSrc0[0];
					vB = pSrc0[-1];
					break;

				case 1: // BG_GR
					vR = pSrc1[-1];
					vG = pSrc1[0];
					vB = pSrc0[0];
					break;

				case 2: // RG_GB
					vR = pSrc0[0];
					vG = pSrc1[0];
					vB = pSrc1[-1];
					break;

				case 3: // GR_BG
					vR = pSrc0[-1];
					vG = pSrc0[0];
					vB = pSrc1[0];
					break;
			}

			*pRed   = (unsigned char)(vR >> outputShift);
			*pGreen = (unsigned char)(vG >> outputShift);
			*pBlue  = (unsigned char)(vB >> outputShift);
		}
	}
	else
	{
		// Last line.
		// Here pSrc0 is the real last line while pSrc1 must point to the last before last line
		for(i = 0; i < count; ++i)
		{
			switch(currentAlignment)
			{
				case 0: // GB_RG
					vR = pSrc1[0];
					vG = pSrc0[0];
					vB = pSrc0[1];
					break;

				case 1: // BG_GR
					vR = pSrc1[1];
					vG = pSrc0[1];
					vB = pSrc0[0];
					break;

				case 2: // RG_GB
					vR = pSrc0[0];
					vG = pSrc0[1];
					vB = pSrc1[1];
					break;

				case 3: // GR_BG
					vR = pSrc0[1];
					vG = pSrc0[0];
					vB = pSrc1[0];
					break;
			}

			*pRed   = (unsigned char)(vR >> outputShift);
			*pGreen = (unsigned char)(vG >> outputShift);
			*pBlue  = (unsigned char)(vB >> outputShift);
		
			pRed   += dstInc;
			pGreen += dstInc;
			pBlue  += dstInc;
				
			pSrc0++;
			pSrc1++;
			currentAlignment ^= 1;
		}

		// Do the last pixel (if requested).
		if( bIncludeLastPixel )
		{
			switch(currentAlignment)
			{
				case 0: // GB_RG
					vR = pSrc1[0];
					vG = pSrc0[0];
					vB = pSrc0[-1];
					break;

				case 1: // BG_GR
					vR = pSrc1[-1];
					vG = pSrc1[0];
					vB = pSrc0[0];
					break;

				case 2: // RG_GB
					vR = pSrc0[0];
					vG = pSrc1[0];
					vB = pSrc1[-1];
					break;

				case 3: // GR_BG
					vR = pSrc0[-1];
					vG = pSrc0[0];
					vB = pSrc1[0];
					break;
			}

			*pRed   = (unsigned char)(vR >> outputShift);
			*pGreen = (unsigned char)(vG >> outputShift);
			*pBlue  = (unsigned char)(vB >> outputShift);
		}			
	}
}



// A simple / naive 2x2 neighborhood Bayer to RGB converter.
// (Assume the caller got the output image allocated to the correct size - otherwise this will end badly).
GEV_STATUS ConvertBayerToRGB( int convAlgorithm, UINT32 h, UINT32 w, UINT32 inFormat, void *inImage, UINT32 outFormat, void *outImage)
{
	GEV_STATUS status = GEVLIB_ERROR_NULL_PTR;
	uint32_t inDepth = 8;
	uint32_t dstDepth = 8;
	uint32_t bytesPerInputLine = 0;
	uint32_t bytesPerOutputLine = 0;
	uint32_t bayerAlign = 0;
	uint32_t dstInc = 0;
	unsigned char *pDstBlue = NULL;
	unsigned char *pDstGreen = NULL;
	unsigned char *pDstRed = NULL;
	
	// Check for valid parameters....
	if ((inImage) != NULL && (outImage != NULL))
	{
		// Set up control info based on input format.
		status = 0;
		switch( inFormat )
		{
			case fmtBayerGR8:
					inDepth = 8;
					bytesPerInputLine = w;
					bayerAlign = BAYER_ALIGN_GR_BG;
				break;
			case fmtBayerRG8:
					inDepth = 8;
					bytesPerInputLine = w;
					bayerAlign = BAYER_ALIGN_RG_GB;
				break;			
			case fmtBayerGB8:
					inDepth = 8;
					bytesPerInputLine = w;
					bayerAlign = BAYER_ALIGN_GB_RG;
				break;			
			case fmtBayerBG8:
					inDepth = 8;
					bytesPerInputLine = w;
					bayerAlign = BAYER_ALIGN_BG_GR;
				break;			
			case fmtBayerGR10:
					inDepth = 10;
					bytesPerInputLine = 2*w;
					bayerAlign = BAYER_ALIGN_GR_BG;
				break;			
			case fmtBayerRG10:
					inDepth = 10;
					bytesPerInputLine = 2*w;
					bayerAlign = BAYER_ALIGN_RG_GB;
				break;			
			case fmtBayerGB10:
					inDepth = 10;
					bytesPerInputLine = 2*w;
					bayerAlign = BAYER_ALIGN_GB_RG;
				break;			
			case fmtBayerBG10 :
					inDepth = 10;
					bytesPerInputLine = 2*w;
					bayerAlign = BAYER_ALIGN_BG_GR;
				break;			
			case fmtBayerGR12:
					inDepth = 12;
					bytesPerInputLine = 2*w;
					bayerAlign = BAYER_ALIGN_GR_BG;
				break;			
			case fmtBayerRG12:
					inDepth = 12;
					bytesPerInputLine = 2*w;
					bayerAlign = BAYER_ALIGN_RG_GB;
				break;			
			case fmtBayerGB12:
					inDepth = 12;
					bytesPerInputLine = 2*w;
					bayerAlign = BAYER_ALIGN_GB_RG;
				break;			
			case fmtBayerBG12:
					inDepth = 12;
					bytesPerInputLine = 2*w;
					bayerAlign = BAYER_ALIGN_BG_GR;
				break;			
			default:
					status = GEVLIB_ERROR_PARAMETER_INVALID;  // Unsupported input format.
				break;
		}

		// Set up the control info based on the output format.
		switch( outFormat)
		{	
			case fmtRGB8Packed:
					// RGB888 - packed in 24 bits.
					dstDepth  = 8;
					dstInc    = 3;
					bytesPerOutputLine = 3*w;
					pDstRed   = (unsigned char *)outImage;
					pDstGreen = (unsigned char *)outImage + 1;
					pDstBlue  = (unsigned char *)outImage + 2;
				break;
			case fmtBGR8Packed:
					// BGR888 - packed in 24 bits.
					dstDepth  = 8;
					dstInc    = 3;
					bytesPerOutputLine = 3*w;
					pDstBlue  = (unsigned char *)outImage;
					pDstGreen = (unsigned char *)outImage + 1;
					pDstRed   = (unsigned char *)outImage + 2;
				break;
			default:
			case fmtRGBA8Packed:
					// RGB8888 (32 bits). (Most common - default)
					dstDepth  = 8;
					dstInc    = 4;
					bytesPerOutputLine = 4*w;
					pDstRed   = (unsigned char *)outImage;
					pDstGreen = (unsigned char *)outImage + 1;
					pDstBlue  = (unsigned char *)outImage + 2;
				break;
			case fmtBGRA8Packed:
					// BGR8888 (32 bits).
					dstDepth  = 8;
					dstInc    = 4;
					bytesPerOutputLine = 4*w;
					pDstBlue  = (unsigned char *)outImage;
					pDstGreen = (unsigned char *)outImage + 1;
					pDstRed   = (unsigned char *)outImage + 2;
				break;
			case fmtRGB10Packed:
					// RGB101010 - packed in 48 bits (3 x 16bit components)
					dstDepth  = 10;
					dstInc    = 3;  // In pixel components (not bytes)
					bytesPerOutputLine = 6*w;
					pDstRed   = (unsigned char *)outImage;
					pDstGreen = (unsigned char *)outImage + 2;
					pDstBlue  = (unsigned char *)outImage + 4;
				break;
			case fmtBGR10Packed:
					// BGR101010 - packed in 48 bits (3 x 16bit components)
					dstDepth  = 10;
					dstInc    = 3;  // In pixel components (not bytes)
					bytesPerOutputLine = 6*w;
					pDstBlue  = (unsigned char *)outImage;
					pDstGreen = (unsigned char *)outImage + 2;
					pDstRed   = (unsigned char *)outImage + 4;
				break;
			case fmtRGB12Packed:
					// RGB121212 - packed in 48 bits (3 x 16bit components)
					dstDepth  = 12;
					dstInc    = 3;  // In pixel components (not bytes)
					bytesPerOutputLine = 6*w;
					pDstRed   = (unsigned char *)outImage;
					pDstGreen = (unsigned char *)outImage + 2;
					pDstBlue  = (unsigned char *)outImage + 4;
				break;
			case fmtBGR12Packed:
					// BGR121212 - packed in 48 bits (3 x 16bit components)
					dstDepth  = 12;
					dstInc    = 3;  // In pixel components (not bytes)
					bytesPerOutputLine = 6*w;
					pDstBlue  = (unsigned char *)outImage;
					pDstGreen = (unsigned char *)outImage + 2;
					pDstRed   = (unsigned char *)outImage + 4;
				break;

			case fmtRGB8Planar:
					// RGB 8 bit Planar.
					dstDepth  = 8;
					dstInc    = 1;
					bytesPerOutputLine = w;
					pDstRed   = (unsigned char *)outImage;
					pDstGreen = (unsigned char *)outImage + h*w*bytesPerOutputLine;
					pDstBlue  = (unsigned char *)outImage + 2*h*w*bytesPerOutputLine;;
				break;
			case fmtRGB10Planar:
					// RGB 10 bit Planar.
					dstDepth  = 10;
					dstInc    = 2;
					bytesPerOutputLine = 2*w;
					pDstRed   = (unsigned char *)outImage;
					pDstGreen = (unsigned char *)outImage + h*w*bytesPerOutputLine;
					pDstBlue  = (unsigned char *)outImage + 2*h*w*bytesPerOutputLine;;
				break;
			case fmtRGB12Planar:
					// RGB 10 bit Planar.
					dstDepth  = 12;
					dstInc    = 2;
					bytesPerOutputLine = 2*w;
					pDstRed   = (unsigned char *)outImage;
					pDstGreen = (unsigned char *)outImage + h*w*bytesPerOutputLine;
					pDstBlue  = (unsigned char *)outImage + 2*h*w*bytesPerOutputLine;;
				break;
			case fmtRGB16Planar:
					// RGB 10 bit Planar.
					dstDepth  = 16;
					dstInc    = 2;
					bytesPerOutputLine = 2*w;
					pDstRed   = (unsigned char *)outImage;
					pDstGreen = (unsigned char *)outImage + h*w*bytesPerOutputLine;
					pDstBlue  = (unsigned char *)outImage + 2*h*w*bytesPerOutputLine;;
				break;			
		}

		// Process the data - Based on Algorithm.....
		if (status == 0)
		{
			// Only one algorithm at the moment - comment this out so it won't complain.
			// if ( convAlgorithm == BAYER_CONVERSION_2X2 )  
			if ( 1 )
			{
				int i;
				unsigned char *pSrcLine0 = (unsigned char *)inImage;
				unsigned char *pSrcLine1 = (unsigned char *)inImage + bytesPerInputLine;
				
				if ( (inDepth == 8) && (dstDepth == 8) )
				{
					
					//  8 bit components in / out.
					for (i = 0; i < (h-1); i++)
					{
						_convBayer8ToRGB8_2x2( (void *)pSrcLine0, (void *)pSrcLine1, (void *)pDstRed, (void *)pDstGreen, (void *)pDstBlue, 
														dstInc, w, bayerAlign, 0, 1 );
						
						pSrcLine0 += bytesPerInputLine;
						pSrcLine1 += bytesPerInputLine;
						pDstRed   += bytesPerOutputLine;
						pDstGreen += bytesPerOutputLine;
						pDstBlue  += bytesPerOutputLine;
						
						bayerAlign ^= 2;
					}
					
					// Do the last line;
					pSrcLine1 -= 2*bytesPerInputLine;
					_convBayer8ToRGB8_2x2( (void *)pSrcLine0, (void *)pSrcLine1, (void *)pDstRed, (void *)pDstGreen, (void *)pDstBlue, 
													dstInc, w, bayerAlign, 1, 1 );
					
				}
				else if ( (inDepth > 8) && (dstDepth == 8) )
				{
					// Use 16-bit input components to 8 bit RGB output (Usefull for conversions for display on-the-fly)
					for (i = 0; i < (h-1); i++)
					{
						_convBayer16ToRGB8_2x2( (void *)pSrcLine0, (void *)pSrcLine1, inDepth, (void *)pDstRed, (void *)pDstGreen, (void *)pDstBlue, 
														dstInc, w, bayerAlign, 0, 1 );
																		
						pSrcLine0 += bytesPerInputLine;
						pSrcLine1 += bytesPerInputLine;
						pDstRed   += bytesPerOutputLine;
						pDstGreen += bytesPerOutputLine;
						pDstBlue  += bytesPerOutputLine;
						
						bayerAlign ^= 2;
					}
					
					// Do the last line;
					pSrcLine1 -= 2*bytesPerInputLine;
					_convBayer16ToRGB8_2x2( (void *)pSrcLine0, (void *)pSrcLine1, inDepth, (void *)pDstRed, (void *)pDstGreen, (void *)pDstBlue, 
														dstInc, w, bayerAlign, 1, 1 );
					
				}
				else
				{
					// Use 16-bit components.
					for (i = 0; i < (h-1); i++)
					{
						_convBayer16ToRGB16_2x2( (void *)pSrcLine0, (void *)pSrcLine1, inDepth, (void *)pDstRed, (void *)pDstGreen, (void *)pDstBlue, 
														dstInc, dstDepth, w, bayerAlign, 0, 1 );
																		
						pSrcLine0 += bytesPerInputLine;
						pSrcLine1 += bytesPerInputLine;
						pDstRed   += bytesPerOutputLine;
						pDstGreen += bytesPerOutputLine;
						pDstBlue  += bytesPerOutputLine;
						
						bayerAlign ^= 2;
					}
					
					// Do the last line;
					pSrcLine1 -= 2*bytesPerInputLine;
						_convBayer16ToRGB16_2x2( (void *)pSrcLine0, (void *)pSrcLine1, inDepth, (void *)pDstRed, (void *)pDstGreen, (void *)pDstBlue, 
														dstInc, dstDepth, w, bayerAlign, 1, 1 );
					
				}
			}
			else
			{
					status = GEVLIB_ERROR_PARAMETER_INVALID;  // Unsupported algorithm.
			}
		}
	}
	return status;
}


