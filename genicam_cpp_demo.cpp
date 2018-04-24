
#include <QSharedMemory>
#include <QBuffer>
#include <QDataStream>
#include <QDebug>
#include <QtGui>

#include <opencv.hpp>
#include <iostream>


#include "stdio.h"
#include "cordef.h"
#include "GenApi/GenApi.h"		//!< GenApi lib definitions.
#include "gevapi.h"				//!< GEV lib definitions.
#include "SapX11Util.h"
#include "X_Display_utils.h"
#include <sched.h>



using namespace std;
using namespace cv;

//using namespace std;
using namespace GenICam;
using namespace GenApi;

#define MAX_NETIF					8
#define MAX_CAMERAS_PER_NETIF	32
#define MAX_CAMERAS		(MAX_NETIF * MAX_CAMERAS_PER_NETIF)


// Enable/disable buffer FULL/EMPTY handling (cycling)
#define USE_SYNCHRONOUS_BUFFER_CYCLING	0

// Enable/disable transfer tuning (buffering, timeouts, thread affinity).
#define TUNE_STREAMING_THREADS 0

#define NUM_BUF	8

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


char GetKey()
{
   char key = getchar();
   while ((key == '\r') || (key == '\n'))
   {
      key = getchar();
   }
   return key;
}

void PrintMenu()
{
   printf("GRAB CTL   : [S]=stop, [1-9]=snap N, [G]=continuous, [A]=Abort\n");
   printf("MISC       : [Q]or[ESC]=end,         [T]=Toggle TurboMode (if available)\n");
}

void * ImageDisplayThread( void *context)
{
        printf("ImageDisplayThread \n");
        QSharedMemory sharedMemory("MySharedMemory");
	if(sharedMemory.isAttached())
        {
            sharedMemory.detach();
	}


	int size = 1216 * 1936 * 1.5 * 5 ;
        if(!sharedMemory.create(size))
	{
	    sharedMemory.attach();
	}

        double times = 0;


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
			status = GevWaitForNextImage(displayContext->camHandle,&img, 1000);
			
			//int test = 0;

			//printf("w = %d , h = %d /n",img->w,img->h);

   			//int width = img->w;  
   			//int height = img->h;  
   		        //Mat Mat_img = Mat::zeros(cv::Size(width, height), CV_8U);  
                        //memcpy(Mat_img.data, displayContext->convertBuffer, width*height);  
			//imwrite("Mat_img.jpg",Mat_img);


			if ((img != NULL) && (status == GEVLIB_OK))
			{
				if (img->status == 0)
				{

					int d = img->d;
					int format = img->format;
					int recv_size = img->recv_size;
					//printf(" d = %d, format = %d , recv_size = %d \n",d,format,recv_size);
					
   					int width = img->w;  
   					int height = img->h;
					//printf("width = %d , height = %d /n",width,height); 
					
   		        		Mat Mat_img = Mat::zeros(cv::Size(width, height), CV_8U);  
                        		memcpy(Mat_img.data, img->address, width * height);
					Mat Mat_rgb = Mat::zeros(cv::Size(width, height), CV_8UC3);
                                        Mat Mat_yuv;

					cvtColor(Mat_img, Mat_rgb, CV_BayerRG2RGB );  
				        cvtColor(Mat_rgb, Mat_yuv, CV_RGB2YUV_I420 ); 

                                        
					//printf("width = %d \n",width);
					//printf("height = %d \n",height);
					int tmp = width*height*1.5;
					int buffer_num = (int)times%5;

                                        
                                                                               
					sharedMemory.lock();
                                        memcpy( ((char*)sharedMemory.data() +tmp * buffer_num ), (char*)Mat_yuv.data, tmp );
                                        sharedMemory.unlock();
                                        times++;


					
	/*				
					Mat Mat_B = Mat::zeros(cv::Size(width, height), CV_8U); 
					Mat Mat_G = Mat::zeros(cv::Size(width, height), CV_8U); 
					Mat Mat_R = Mat::zeros(cv::Size(width, height), CV_8U); 
					
					Mat out[]={Mat_B, Mat_G, Mat_R};
					split(Mat_yuv, out);

					memcpy(Mat_B.data, img->address, width * height);
					memcpy(Mat_G.data, img->address + width * height , width * height);
					memcpy(Mat_R.data, img->address + 2 * width * height , width * height);
					imshow("B",Mat_B);
					imshow("G",Mat_G);
					imshow("R",Mat_R);
					waitKey(10);
					
	*/
					// Convert the image format if required.
					if (displayContext->convertFormat)
					{
						int gev_depth = GevGetPixelDepthInBits(img->format);
						// Convert the image to a displayable format.
						//(Note : Not all formats can be displayed properly at this time (planar, YUV*, 10/12 bit packed).
						ConvertGevImageToX11Format( img->w, img->h, gev_depth, img->format, img->address, \
												displayContext->depth, displayContext->format, displayContext->convertBuffer);
				
						// Display the image in the (supported) converted format. 
						Display_Image( displayContext->View, displayContext->depth, img->w, img->h, displayContext->convertBuffer );				
					}
					else
					{
						// Display the image in the (supported) received format. 
						Display_Image( displayContext->View, img->d,  img->w, img->h, img->address );
					}
				}
				else
				{
					// Image had an error (incomplete (timeout/overflow/lost)).
					// Do any handling of this condition necessary.
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

	//Mat frame = imread("123.jpg");

	//printf ("have read jpg %d", frame.rows);
	//namedWindow("123", WINDOW_AUTOSIZE); 
	//imshow("123",frame);


	//waitKey(1000);  


	GEV_DEVICE_INTERFACE  pCamera[MAX_CAMERAS] = {0};
	UINT16 status;
	int numCamera = 0;
	int camIndex = 0;
   X_VIEW_HANDLE  View = NULL;
	MY_CONTEXT context = {0};
   pthread_t  tid;
	char c;
	int done = FALSE;
	int turboDriveAvailable = 0;

	// Greetings
	printf ("\nGigE Vision Library GenICam C++ Example Program (%s)\n", __DATE__);
	printf ("Copyright (c) 2015, DALSA.\nAll rights reserved.\n\n");

	//===================================================================================
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
			UINT32 height = 0;
			UINT32 width = 0;
			UINT32 x_offset = 0;
			UINT32 y_offset = 0;
			UINT32 format = 0;
			UINT32 maxHeight = 1600;
			UINT32 maxWidth = 2048;
			UINT32 maxDepth = 2;
			UINT32 size;
			int numBuffers = NUM_BUF;
			PUINT8 bufAddress[NUM_BUF];
			GEV_CAMERA_HANDLE handle = NULL;
			UINT32 pixFormat = 0;
			UINT32 pixDepth = 0;
			UINT32 pixelOrder = 0;

			//====================================================================
			// Open the camera.
			status = GevOpenCamera( &pCamera[camIndex], GevExclusiveMode, &handle);
			if (status == 0)
			{
				//=================================================================
				// Initiliaze access to GenICam features via Camera XML File 

				status = GevInitGenICamXMLFeatures( handle, TRUE);				
				if (status == GEVLIB_OK)
				{
					// Get the name of XML file name back (example only - in case you need it somewhere).
					char xmlFileName[MAX_PATH] = {0};
					status = GevGetGenICamXML_FileName( handle, (int)sizeof(xmlFileName), xmlFileName);
					if (status == GEVLIB_OK)
					{
						printf("XML stored as %s\n", xmlFileName);
					}
				}
			}
			
			// Go on to adjust some API related settings (for tuning / diagnostics / etc....).
			if ( status == 0 )
			{
				GEV_CAMERA_OPTIONS camOptions = {0};

				// Adjust the camera interface options if desired (see the manual)
				GevGetCameraInterfaceOptions( handle, &camOptions);
				camOptions.heartbeat_timeout_ms = 90000;		// For debugging (delay camera timeout while in debugger)

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
	/*
				GenApi::CEnumerationPtr ptrEnumNode = Camera->_GetNode("PixelFormat") ;
				format = (UINT32)ptrEnumNode->GetIntValue();
				printf("BF format = %d\n",format);	

				char feature_name[MAX_GEVSTRING_LENGTH+1] = "PixelFormat";
				char value_str[MAX_GEVSTRING_LENGTH+1] = "BGR8";

				GenApi::CNodePtr pNode = Camera->_GetNode(feature_name);
						if (pNode)
						{
							CValuePtr valNode(pNode);  
							try {
								valNode->FromString(value_str, false);
							}
							// Catch all possible exceptions from a node access.
							CATCH_GENAPI_ERROR(status);
						}
				
				//GenApi::CEnumerationPtr ptrEnumNode = Camera->_GetNode("PixelFormat") ;
				format = (UINT32)ptrEnumNode->GetIntValue();
				printf("AF format = %d\n",format);	
		*/		
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
					size = maxDepth * maxWidth * maxHeight;
					for (i = 0; i < numBuffers; i++)
					{
						bufAddress[i] = (PUINT8)malloc(size);
						memset(bufAddress[i], 0, size);

					}
					// Boost application RT response (not too high since GEV library boosts data receive thread to max allowed)
					// SCHED_FIFO can cause many unintentional side effects.
					// SCHED_RR has fewer side effects.
					// SCHED_OTHER (normal scheduler) is not too bad afer all.
					if (0)
					{
						struct sched_param param = {0};
						param.sched_priority = (sched_get_priority_max(SCHED_FIFO) - sched_get_priority_min(SCHED_FIFO)) / 2;
						sched_setscheduler(0, SCHED_FIFO, &param); // Don't care if it fails since we can't do anyting about it.
					}

#if USE_SYNCHRONOUS_BUFFER_CYCLING
					// Initialize a transfer with synchronous buffer handling.
					status = GevInitImageTransfer( handle, SynchronousNextEmpty, numBuffers, bufAddress);
#else
					// Initialize a transfer with asynchronous buffer handling.
					status = GevInitImageTransfer( handle, Asynchronous, numBuffers, bufAddress);
#endif

					// Make sure the transfer settings are consistent.
					if (status == GEVLIB_OK)
					{
						// Todo
					}

            	// Create an image display window.
		         // This works best for monochrome and RGB. The packed color formats (with Y, U, V, etc..) require conversion.
					// Translate the raw pixel format to one suitable for the (limited) Linux display routines.			
					context.convertFormat = FALSE;
					context.convertBuffer = NULL;
					pixDepth = GevGetPixelDepthInBits( format);
					pixelOrder = GevGetRGBPixelOrder( format);
					if (!IsGevPixelTypeX11Displayable(format))
					{
						// Our X11 functions will not display this image - need to set up to convert it.
						if (GevIsPixelTypeRGB(format))
						{
							// A packed color image - convert to unpacked color (or even Mono if desired).
						#if 1
							// Convert to RGB8888.
							pixFormat = CORDATA_FORMAT_RGB8888; // Use this for color.
							pixDepth = 8*sizeof(UINT32);
						#else
							// Convert to MONO.
							pixFormat = CORDATA_FORMAT_MONO; // Use this for color.
							pixDepth = 10;
						#endif
							context.convertFormat = TRUE;
							context.format = Convert_SaperaFormat_To_X11( pixFormat);
							context.depth = pixDepth;
							context.convertBuffer = malloc((maxWidth * maxHeight * ((pixDepth + 7)/8)));
						}
						else
						{
							// A packed Mono format.
							// It is converted (unpacked) internally during the transfer and becomes displayable.
							pixFormat = Convert_GevFormat_To_Sapera(format);
							context.format = Convert_SaperaFormat_To_X11( pixFormat);
							context.depth = pixDepth;
						}
					}
					else
					{
						if (GevIsPixelTypeRGB(format))
						{
							// A native RGB format (not packed).
							pixFormat = Convert_GevFormat_To_Sapera(format);
							pixDepth *= GetPixelSizeInBytes(format);
						}
						else
						{
							// A native mono format (not packed).
							pixFormat = Convert_GevFormat_To_Sapera(format);
						}
					}
					
					View = CreateDisplayWindow("GigE-V GenApi Console Demo", TRUE, height, width, pixDepth, pixFormat, FALSE ); 

					// Create a thread to receive images from the API and display them.
					context.View = View;
					context.camHandle = handle;
					context.exit = FALSE;
		   		pthread_create(&tid, NULL, ImageDisplayThread, &context); 

   				//int width = img->w;  
   				//int height = img->h;  
   		        	//Mat Mat_img = Mat::zeros(cv::Size(width, height), CV_8U);  
                        	//memcpy(Mat_img.data, context.convertBuffer, width*height);  
				//imwrite("Mat_img.jpg",Mat_img);

		         // Call the main command loop or the example.
		         PrintMenu();
		         while(!done)
		         {
		            c = GetKey();

		            // Toggle turboMode
		            if ((c == 'T') || (c=='t'))
		            {
							// See if TurboDrive is available.
							turboDriveAvailable = IsTurboDriveAvailable(handle);
							if (turboDriveAvailable)
							{
								int type;
								UINT32 val = 1;
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
		            }

		            // Stop
		            if ((c == 'S') || (c=='s') || (c == '0'))
		            {
							GevStopImageTransfer(handle);
		            }
		            //Abort
		            if ((c == 'A') || (c=='a'))
		            {
	 						GevAbortImageTransfer(handle);
						}
		            // Snap N (1 to 9 frames)
		            if ((c >= '1')&&(c<='9'))
		            {
							for (i = 0; i < numBuffers; i++)
							{
								memset(bufAddress[i], 0, size);
							}

							status = GevStartImageTransfer( handle, (UINT32)(c-'0'));
							if (status != 0) printf("Error starting grab - 0x%x  or %d\n", status, status); 
						}
		            // Continuous grab.
		            if ((c == 'G') || (c=='g'))
		            {
							for (i = 0; i < numBuffers; i++)
							{
								memset(bufAddress[i], 0, size);
							}
	 						status = GevStartImageTransfer( handle, -1);
							if (status != 0) printf("Error starting grab - 0x%x  or %d\n", status, status); 
		            }

		            if (c == '?')
		            {
		               PrintMenu();
		            }

		            if ((c == 0x1b) || (c == 'q') || (c == 'Q'))
		            {
							GevStopImageTransfer(handle);
		               done = TRUE;
							context.exit = TRUE;
		   				pthread_join( tid, NULL);      
		            }
		         }

					GevAbortImageTransfer(handle);
					status = GevFreeImageTransfer(handle);
					status = GevSetImageParameters(handle, 500, 500, x_offset,  y_offset, format);
					printf("format is : %d , status is : %d",(int64_t)format, status); //mnf 1
					DestroyDisplayWindow(View);

					status = GevFreeImageTransfer( handle);

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


	//printf("Hit any key to exit\n");
	//kbhit();

	return 0;
}

