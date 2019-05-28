
#include "stdio.h"
#include "cordef.h"
#include "GenApi/GenApi.h"		//!< GenApi lib definitions.
#include "gevapi.h"				//!< GEV lib definitions.
#include "SapX11Util.h"
#include "X_Display_utils.h"
#include "FileUtil.h"
#include <sched.h>

//using namespace std;
//using namespace GenICam;
//using namespace GenApi;

// Turbo Drive mode https://www.teledynedalsa.com/en/learn/knowledge-center/turbodrive/
#define TURBO_DRIVE	0

// Display camera view in display window
#define DISPLAY_WINDOW 1 


#define MAX_NETIF					8
#define MAX_CAMERAS_PER_NETIF	32
#define MAX_CAMERAS		(MAX_NETIF * MAX_CAMERAS_PER_NETIF)

// Enable/disable Bayer to RGB conversion
// (If disabled - Bayer format will be treated as Monochrome).
#define ENABLE_BAYER_CONVERSION 1

// Enable/disable buffer FULL/EMPTY handling (cycling)
#define USE_SYNCHRONOUS_BUFFER_CYCLING	0

// Enable/disable transfer tuning (buffering, timeouts, thread affinity).
#define TUNE_STREAMING_THREADS 1

#define NUM_BUF	8
void *m_latestBuffer = NULL;

typedef struct tagMY_CONTEXT
{
   X_VIEW_HANDLE     View;
	GEV_CAMERA_HANDLE camHandle;
	int					depth;
	int 					format;
	void 					*convertBuffer;
	BOOL					convertFormat;
	BOOL              exit;
}MY_CONTEXT, *PMY_CONTEXT;

static unsigned long us_timer_init( void )
{
   struct timeval tm;
   unsigned long msec;
   
   // Get the time and turn it into a millisecond counter.
   gettimeofday( &tm, NULL);
   
   msec = (tm.tv_sec * 1000000) + (tm.tv_usec);
   return msec;
}
static unsigned long ms_timer_init( void )
{
   struct timeval tm;
   unsigned long msec;
   
   // Get the time and turn it into a millisecond counter.
   gettimeofday( &tm, NULL);
   
   msec = (tm.tv_sec * 1000) + (tm.tv_usec / 1000);
   return msec;
}

static int ms_timer_interval_elapsed( unsigned long origin, unsigned long timeout)
{
   struct timeval tm;
   unsigned long msec;
   
   // Get the time and turn it into a millisecond counter.
   gettimeofday( &tm, NULL);
   
   msec = (tm.tv_sec * 1000) + (tm.tv_usec / 1000);
      
   // Check if the timeout has expired.
   if ( msec > origin )
   {
      return ((msec - origin) >= timeout) ? TRUE : FALSE;
   }
   else
   {
      return ((origin - msec) >= timeout) ? TRUE : FALSE;
   }
}

static void _GetUniqueFilename( char *filename, size_t size, char *basename)
{
	// Create a filename based on the current time (to 0.01 seconds)
	struct timeval tm;
	uint32_t years, days, hours, seconds;

	if ((filename != NULL) && (basename != NULL) )
	{
		if (size > (16 + sizeof(basename)) )
		{
	
			// Get the time and turn it into a 10 msec resolution counter to use as an index.
			gettimeofday( &tm, NULL);
			years = ((tm.tv_sec / 86400) / 365);
			tm.tv_sec = tm.tv_sec - (years*86400*365);
			days  = (tm.tv_sec / 86400);
			tm.tv_sec = tm.tv_sec - (days * 86400);
			hours = (tm.tv_sec / 3600);
			seconds = tm.tv_sec - (hours * 3600);						
															
			snprintf(filename, size, "%s_%03d%02d%04d%02d", basename, days,hours, (int)seconds, (int)(tm.tv_usec/10000));
		}
	}
}


void * ImageDisplayThread( void *context)
{
	MY_CONTEXT *displayContext = (MY_CONTEXT *)context;

	if (displayContext != NULL)
	{
		unsigned long prev_time = 0;
		//unsigned long cur_time = 0;
		//unsigned long deltatime = 0;
		prev_time = us_timer_init();

		// While we are still running.
		while(!displayContext->exit)
		{
			GEV_BUFFER_OBJECT *img = NULL;
			GEV_STATUS status = 0;
	
			// Wait for images to be received
			status = GevWaitForNextImage(displayContext->camHandle, &img, 1000);

			if ((img != NULL) && (status == GEVLIB_OK))
			{
				if (img->status == 0)
				{
					m_latestBuffer = img->address;
					// Can the acquired buffer be displayed?
					if ( IsGevPixelTypeX11Displayable(img->format) || displayContext->convertFormat )
					{
						// Convert the image format if required.
						if (displayContext->convertFormat)
						{
							int gev_depth = GevGetPixelDepthInBits(img->format);
							// Convert the image to a displayable format.
							//(Note : Not all formats can be displayed properly at this time (planar, YUV*, 10/12 bit packed).
							ConvertGevImageToX11Format( img->w, img->h, gev_depth, img->format, img->address, \
													displayContext->depth, displayContext->format, displayContext->convertBuffer);
					
							// Display the image in the (supported) converted format. 
#if DISPLAY_WINDOW
							Display_Image( displayContext->View, displayContext->depth, img->w, img->h, displayContext->convertBuffer );				
#endif
							// write the file to stdout for communication with other programs
							fwrite(displayContext->convertBuffer, 1, 1, stdout);
							fflush(stdout);  // flush buffer after writing file 

						}
						else
						{
#if DISPLAY_WINDOW
							// Display the image in the (supported) received format. 
							Display_Image( displayContext->View, img->d,  img->w, img->h, img->address );
#endif 
							// write the file to stdout for communication with other programs
							fwrite(img->address, 1, 1, stdout);
							fflush(stdout);
 
							// Transfer the image to socket
							// TransferImageToSocket( displaContext->View, display->depth, img->w, img->h, img->address);
						}
					}
					else
					{
						printf("Not displayable\n");
					}
				}
				else
				{
					// Image had an error (incomplete (timeout/overflow/lost)).
					// Do any handling of this condition necessary.
					// printf("Image had an error and is incomplete (timeout/overflow/lost).\n");
				}
			}
#if USE_SYNCHRONOUS_BUFFER_CYCLING
			if (img != NULL)
			{
				// Release the buffer back to the image transfer process.
				GevReleaseImage( displayContext->camHandle, img);
			}
#endif
		}
	}
	pthread_exit(0);	
}

int IsTurboDriveAvailable(GEV_CAMERA_HANDLE handle)
{
	int type;
	UINT32 val = 0;
	
	if ( 0 == GevGetFeatureValue( handle, "transferTurboCurrentlyAbailable",  &type, sizeof(UINT32), &val) )
	{
		// Current / Standard method present - this feature indicates if TurboMode is available.
		// (Yes - it is spelled that odd way on purpose).
		return (val != 0);
	}
	else
	{
		// Legacy mode check - standard feature is not there try it manually.
		char pxlfmt_str[64] = {0};

		// Mandatory feature (always present).
		GevGetFeatureValueAsString( handle, "PixelFormat", &type, sizeof(pxlfmt_str), pxlfmt_str);

		// Set the "turbo" capability selector for this format.
		if ( 0 != GevSetFeatureValueAsString( handle, "transferTurboCapabilitySelector", pxlfmt_str) )
		{
			// Either the capability selector is not present or the pixel format is not part of the 
			// capability set.
			// Either way - TurboMode is NOT AVAILABLE.....
			return 0; 
		}
		else
		{
			// The capabilty set exists so TurboMode is AVAILABLE.
			// It is up to the camera to send TurboMode data if it can - so we let it.
			return 1;
		}
	}
	return 0;
}

int main(int argc, char* argv[])
{	
	// socket stuff



	GEV_DEVICE_INTERFACE  pCamera[MAX_CAMERAS] = {0};
	GEV_STATUS status;
	int numCamera = 0;
	int camIndex = 0;
#if DISPLAY_WINDOW
   X_VIEW_HANDLE  View = NULL;
#endif
	MY_CONTEXT context = {0};
   pthread_t  tid;
	int done = FALSE;
	int turboDriveAvailable = 0;
	char uniqueName[128];
	uint32_t macLow = 0; // Low 32-bits of the mac address (for file naming).

// Helper info
#if DISPLAY_WINDOW
	printf("*** DISPLAY WINDOW ENABLED, TO DISABLE SET DISPLAY_WINDOW VARIABLE TO 0 IN GENICAM.CPP ***\n");
#else
	printf("*** DISPLAY WINDOW DISABLED, TO DISABLE SET DISPLAY_WINDOW VARIABLE TO 1 IN GENICAM.CPP ***\n");
#endif

	// Boost application RT response (not too high since GEV library boosts data receive thread to max allowed)
	// SCHED_FIFO can cause many unintentional side effects.
	// SCHED_RR has fewer side effects.
	// SCHED_OTHER (normal default scheduler) is not too bad afer all.
	if (0)
	{
		//int policy = SCHED_FIFO;
		int policy = SCHED_RR;
		pthread_attr_t attrib;
		int inherit_sched = 0;
		struct sched_param param = {0};

		// Set an average RT priority (increase/decrease to tuner performance).
		param.sched_priority = (sched_get_priority_max(policy) - sched_get_priority_min(policy)) / 2;
		
		// Set scheduler policy
		pthread_setschedparam( pthread_self(), policy, &param); // Don't care if it fails since we can't do anyting about it.
		
		// Make sure all subsequent threads use the same policy.
		pthread_attr_init(&attrib);
		pthread_attr_getinheritsched( &attrib, &inherit_sched);
		if (inherit_sched != PTHREAD_INHERIT_SCHED)
		{
			inherit_sched = PTHREAD_INHERIT_SCHED;
			pthread_attr_setinheritsched(&attrib, inherit_sched);
		}
	}
	

	// Set default options for the library.
	{
		GEVLIB_CONFIG_OPTIONS options = {0};

		GevGetLibraryConfigOptions( &options);
		//options.logLevel = GEV_LOG_LEVEL_OFF;
		//options.logLevel = GEV_LOG_LEVEL_TRACE;
		options.logLevel = GEV_LOG_LEVEL_NORMAL;
		GevSetLibraryConfigOptions( &options);
	}

	//====================================================================================
	// DISCOVER Cameras
	//
	// Get all the IP addresses of attached network cards.

	status = GevGetCameraList( pCamera, MAX_CAMERAS, &numCamera);

	printf ("%d camera(s) on the network\n", numCamera);

	// Select the first camera found (unless the command line has a parameter = the camera index)
	if (numCamera != 0)
	{
		if (argc > 1)
		{
			sscanf(argv[1], "%d", &camIndex);
			if (camIndex >= (int)numCamera)
			{
				printf("Camera index out of range - only %d camera(s) are present\n", numCamera);
				camIndex = -1;
			}
		}

		if (camIndex != -1)
		{
			//====================================================================
			// Connect to Camera
			//
			//
			int i;
			int type;
			UINT32 height = 0;
			UINT32 width = 0;
			UINT32 format = 0;
			UINT32 maxHeight = 1600;
			UINT32 maxWidth = 2048;
			UINT32 maxDepth = 2;
			UINT64 size;
			UINT64 payload_size;
			int numBuffers = NUM_BUF;
			PUINT8 bufAddress[NUM_BUF];
			GEV_CAMERA_HANDLE handle = NULL;
			UINT32 pixFormat = 0;
			UINT32 pixDepth = 0;
			UINT32 convertedGevFormat = 0;
			
			//====================================================================
			// Open the camera.
			status = GevOpenCamera( &pCamera[camIndex], GevExclusiveMode, &handle);
			if (status == 0)
			{
				//=================================================================
				// GenICam feature access via Camera XML File enabled by "open"
				// 
				// Get the name of XML file name back (example only - in case you need it somewhere).
				//
				char xmlFileName[MAX_PATH] = {0};
				status = GevGetGenICamXML_FileName( handle, (int)sizeof(xmlFileName), xmlFileName);
				if (status == GEVLIB_OK)
				{
					printf("XML stored as %s\n", xmlFileName);
				}
				status = GEVLIB_OK;
			}
			// Get the low part of the MAC address (use it as part of a unique file name for saving images).
			// Generate a unique base name to be used for saving image files
			// based on the last 3 octets of the MAC address.
			macLow = pCamera[camIndex].macLow;
			macLow &= 0x00FFFFFF;
			snprintf(uniqueName, sizeof(uniqueName), "img_%06x", macLow); 
			
			
			// Go on to adjust some API related settings (for tuning / diagnostics / etc....).
			if ( status == 0 )
			{
				GEV_CAMERA_OPTIONS camOptions = {0};

				// Adjust the camera interface options if desired (see the manual)
				GevGetCameraInterfaceOptions( handle, &camOptions);
				//camOptions.heartbeat_timeout_ms = 60000;		// For debugging (delay camera timeout while in debugger)
				camOptions.heartbeat_timeout_ms = 10000;		// Disconnect detection (10 seconds)

#if TUNE_STREAMING_THREADS
				// Some tuning can be done here. (see the manual)
				camOptions.streamFrame_timeout_ms = 1001;				// Internal timeout for frame reception.
				camOptions.streamNumFramesBuffered = 4;				// Buffer frames internally.
				camOptions.streamMemoryLimitMax = 64*1024*1024;		// Adjust packet memory buffering limit.	
				camOptions.streamPktSize = 9180;							// Adjust the GVSP packet size.
				camOptions.streamPktDelay = 10;							// Add usecs between packets to pace arrival at NIC.
				
				// Assign specific CPUs to threads (affinity) - if required for better performance.
				{
					int numCpus = _GetNumCpus();
					if (numCpus > 1)
					{
						camOptions.streamThreadAffinity = numCpus-1;
						camOptions.serverThreadAffinity = numCpus-2;
					}
				}
#endif
				// Write the adjusted interface options back.
				GevSetCameraInterfaceOptions( handle, &camOptions);

				//=====================================================================
				// Get the GenICam FeatureNodeMap object and access the camera features.
				GenApi::CNodeMapRef *Camera = static_cast<GenApi::CNodeMapRef*>(GevGetFeatureNodeMap(handle));
				
				if (Camera)
				{
					// Access some features using the bare GenApi interface methods
					try 
					{
						//Mandatory features....
						GenApi::CIntegerPtr ptrIntNode = Camera->_GetNode("Width");
						width = (UINT32) ptrIntNode->GetValue();
						ptrIntNode = Camera->_GetNode("Height");
						height = (UINT32) ptrIntNode->GetValue();
						ptrIntNode = Camera->_GetNode("PayloadSize");
						payload_size = (UINT64) ptrIntNode->GetValue();
						GenApi::CEnumerationPtr ptrEnumNode = Camera->_GetNode("PixelFormat") ;
						format = (UINT32)ptrEnumNode->GetIntValue();
					}
					// Catch all possible exceptions from a node access.
					CATCH_GENAPI_ERROR(status);
				}

				if (status == 0)
				{
					//=================================================================
					// Set up a grab/transfer from this camera
					//
					printf("Camera ROI set for \n\tHeight = %d\n\tWidth = %d\n\tPixelFormat (val) = 0x%08x\n", height,width,format);

					maxHeight = height;
					maxWidth = width;
					maxDepth = GetPixelSizeInBytes(format);

					// Allocate image buffers
					// (Either the image size or the payload_size, whichever is larger - allows for packed pixel formats).
					size = maxDepth * maxWidth * maxHeight;
					size = (payload_size > size) ? payload_size : size;
					for (i = 0; i < numBuffers; i++)
					{
						bufAddress[i] = (PUINT8)malloc(size);
						memset(bufAddress[i], 0, size);

					}

#if USE_SYNCHRONOUS_BUFFER_CYCLING
					// Initialize a transfer with synchronous buffer handling.
					status = GevInitializeTransfer( handle, SynchronousNextEmpty, size, numBuffers, bufAddress);
#else
					// Initialize a transfer with asynchronous buffer handling.
					status = GevInitializeTransfer( handle, Asynchronous, size, numBuffers, bufAddress);
#endif

					// Create an image display window.
					// This works best for monochrome and RGB. The packed color formats (with Y, U, V, etc..) require 
					// conversion as do, if desired, Bayer formats.
					// (Packed pixels are unpacked internally unless passthru mode is enabled).

					// Translate the raw pixel format to one suitable for the (limited) Linux display routines.			

					status = GetX11DisplayablePixelFormat( ENABLE_BAYER_CONVERSION, format, &convertedGevFormat, &pixFormat);

					if (format != convertedGevFormat) 
					{
						// We MAY need to convert the data on the fly to display it.
						if (GevIsPixelTypeRGB(convertedGevFormat))
						{
							// Conversion to RGB888 required.
							pixDepth = 32;	// Assume 4 8bit components for color display (RGBA)
							context.format = Convert_SaperaFormat_To_X11( pixFormat);
							context.depth = pixDepth;
							context.convertBuffer = malloc((maxWidth * maxHeight * ((pixDepth + 7)/8)));
							context.convertFormat = TRUE;
						}
						else
						{
							// Converted format is MONO - generally this is handled
							// internally (unpacking etc...) unless in passthru mode.
							// (						
							pixDepth = GevGetPixelDepthInBits(convertedGevFormat);
							context.format = Convert_SaperaFormat_To_X11( pixFormat);
							context.depth = pixDepth;							
							context.convertBuffer = NULL;
							context.convertFormat = FALSE;
						}
					}
					else
					{
						pixDepth = GevGetPixelDepthInBits(convertedGevFormat);
						context.format = Convert_SaperaFormat_To_X11( pixFormat);
						context.depth = pixDepth;
						context.convertBuffer = NULL;
						context.convertFormat = FALSE;
					}
					
#if DISPLAY_WINDOW
					View = CreateDisplayWindow("GigE-V GenApi Console Demo", TRUE, height, width, pixDepth, pixFormat, FALSE ); 

					// Create a thread to receive images from the API and display them.
					context.View = View;
#endif
					context.camHandle = handle;
					context.exit = FALSE;
					pthread_create(&tid, NULL, ImageDisplayThread, &context); 


					// main loop
					int setup = 0;
					while(!done)
					{
						if (!setup) {
							// Check if turboMode works
							turboDriveAvailable = IsTurboDriveAvailable(handle);
							if(turboDriveAvailable)
							{
								UINT32 val = TURBO_DRIVE;
								GevGetFeatureValue(handle, "transferTurboMode", &type, sizeof(UINT32), &val);
								val = (val == 0) ? 1 : 0;
								GevSetFeatureValue(handle, "transferTurboMode", sizeof(UINT32), &val);
								GevGetFeatureValue(handle, "transferTurboMode", &type, sizeof(UINT32), &val);

								if (val == 1)
								{
									printf("TurboMode Enabled\n");
								}
								else
								{
									printf("TurboMode Disabled\n");
								}
							}
							else 
							{
								printf("*** TurboDrive is NOT Available for this device/pixel format combination ***\n");
							}

							// Stream images
							for (i = 0; i < numBuffers; i++)
							{
								memset(bufAddress[i], 0, size);
							}
							status = GevStartTransfer( handle, -1);
							if (status != 0) printf("Error starting grab - 0x%x  or %d\n", status, status); 

							// Setup complete
							setup = 1;
						}
					}

					GevAbortTransfer(handle);
					status = GevFreeTransfer(handle);

#if DISPLAY_WINDOW
					DestroyDisplayWindow(View);
#endif


					for (i = 0; i < numBuffers; i++)
					{	
						free(bufAddress[i]);
					}
					if (context.convertBuffer != NULL)
					{
						free(context.convertBuffer);
						context.convertBuffer = NULL;
					}
				}
				GevCloseCamera(&handle);
			}
			else
			{
				printf("Error : 0x%0x : opening camera\n", status);
			}
		}
	}

	// Close down the API.
	GevApiUninitialize();

	// Close socket API
	_CloseSocketAPI ();	// must close API even on error


	return 0;
}

