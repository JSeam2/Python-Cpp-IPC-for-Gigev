// This simple X11 display handler has no dependency on the window 
//	manager. As long as a GUI is available with X underneath, this will 
// display data.
//
//	Note : 
//		- it is a work in progress.
//
#ifndef _GNU_SOURCE
	// This define us necessary for controlling "thread affinity".
	#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <sys/time.h>
#include <sched.h>

#include "corenv.h"
#include "X_Display_utils.h"
#include "SapX11Util.h"

// Define a limit on the number of times we copy to the display per second.
#define MAX_FRAME_UPDATE_RATE 30

#define FALSE 0
#define TRUE !FALSE

#define FONT_NAME  "8x13"
#define PAGE_SIZE  4096
extern int pthread_yield();

static u_int32_t m_lut_888[256]  = {0};
static u_int32_t m_lut_8888[256] = {0};

#if 0
// Debugging helper
XDPYINFO g_dpyinfo = {0};

int getX11displayinfo( PXDPYINFO info)
{
	if (info != NULL)
	{
		info->called = g_dpyinfo.called;
		info->sent = g_dpyinfo.sent;
		info->recvd = g_dpyinfo.recvd;
		info->spurious = g_dpyinfo.spurious;
		info->location = g_dpyinfo.location;
	}
	return 0;
}
#endif

static void _SendPokeToWindow(X_VIEW_HANDLE xhandle, Window w)
{
	if ( xhandle != NULL)
	{
		XKeyEvent xev;
		xev.display = xhandle->display;
		xev.window = w;
		xev.keycode = 0x42;
		xev.serial = 0;
		xev.type = KeyPress;
		xev.root = None;
		xev.subwindow = None;
		xev.time = 0;
		xev.same_screen = 1;
		xev.state = 0;
		xev.x = 1;
		xev.y = 1;
		xev.x_root = 1;
		xev.y_root = 1;
		XSendEvent( xhandle->display, xhandle->window, TRUE, KeyPressMask, (XEvent *)&xev);
		XFlush(xhandle->display);
		xev.type = KeyRelease;
		XSendEvent( xhandle->display, xhandle->window, TRUE, KeyPressMask, (XEvent *)&xev);
		XFlush(xhandle->display);
	
	}
}

static void _SendHandshakeMsg(X_VIEW_HANDLE xhandle)
{
	if ( xhandle != NULL)
	{
		XClientMessageEvent xev = {0};
		xev.type = ClientMessage;
		xev.serial = 0;
		xev.format = 32;
		xev.send_event = True;
		xev.display = xhandle->display;
		xev.window = xhandle->window;
		xev.message_type = xhandle->atomHandshake;

		xhandle->handshake_msg_count++;
	
		XSendEvent( xhandle->display, xhandle->window, TRUE, NoEventMask, (XEvent *)&xev);
		XFlush(xhandle->display);
	}
}

static void Clean_Up ( X_VIEW_HANDLE xhandle)
{
   if ( xhandle != NULL)
   {
      if (xhandle->valid)
      {
         xhandle->destroyInProgress = 1;
         pthread_mutex_lock(&xhandle->display_update_mutex);
                  
			pthread_mutex_lock(&xhandle->display_signal_mutex); 

         xhandle->destroyInProgress = 1;
         xhandle->valid = FALSE;

			pthread_cond_signal(&xhandle->display_signal_cv);  
			pthread_mutex_unlock(&xhandle->display_signal_mutex); 
 
			// Wait for handler threads to exit.
			_SendPokeToWindow(xhandle, xhandle->window);
			pthread_join(xhandle->handler_thread_id, NULL);
 			pthread_join(xhandle->display_thread_id, NULL);

			pthread_mutex_destroy(&xhandle->display_signal_mutex); 
			pthread_cond_destroy(&xhandle->display_signal_cv); 

         pthread_yield();

          XUnmapWindow (xhandle->display, xhandle->window);

			// Clean up the display objects                           
         if (xhandle->shminfo.shmaddr)
         {
            XShmDetach (xhandle->display, &xhandle->shminfo);
            shmdt (xhandle->shminfo.shmaddr);
            shmctl (xhandle->shminfo.shmid, IPC_RMID, 0);
            xhandle->ximage->data = NULL;
         }
   
         if (xhandle->display != NULL)
         {
            XDestroyWindow (xhandle->display, xhandle->window);
           
            if (xhandle->ximage != NULL)
            {
               XDestroyImage (xhandle->ximage);
               xhandle->ximage = NULL;
            }

            if (xhandle->context != 0)
            {
               XFreeGC (xhandle->display, xhandle->context);
               xhandle->context = 0;  
            }   
            
				while(XPending(xhandle->display))
				{	
					XEvent event;
					XNextEvent( xhandle->display, &event);
				}

            XCloseDisplay (xhandle->display);
            xhandle->display = NULL;         
         }
         pthread_mutex_unlock(&xhandle->display_update_mutex);
         pthread_mutex_destroy(&xhandle->display_update_mutex);
      }
   }
   return;
}


void DestroyX11DisplayWindow( X_VIEW_HANDLE xhandle)
{
   if (xhandle != NULL)
   {
      Clean_Up(xhandle);
      free(xhandle);
      xhandle = NULL;
   }
}

static void display_update_locked( X_VIEW_HANDLE xhandle)
{
   if ((xhandle->valid) && (xhandle->destroyInProgress == 0))
   {
      if ( xhandle->useshm)
      {
         if (!xhandle->shmInProgress)
         {
            xhandle->shmInProgress = TRUE;

				if (xhandle->use_screen_buffer)
				{
					// Data is copied to intermediate screen buffer from image source.
					// Scroll settings handled by data copy section.
					XShmPutImage (xhandle->display, xhandle->window, xhandle->context,
							  xhandle->ximage, 0, 0, 0, 0, \
							  xhandle->display_width, xhandle->display_height, TRUE);
				}
				else
				{
					// Display is directly from image source.
					// Scroll settings handled by "PutImage".
					XShmPutImage (xhandle->display, xhandle->window, xhandle->context, \
							 xhandle->ximage, xhandle->x_origin, xhandle->y_origin, 0, 0, \
							 (xhandle->img_width - xhandle->x_origin), (xhandle->img_height - xhandle->y_origin), TRUE);
				}
				XFlush(xhandle->display); 
			}
      }
      else
      {
			if( xhandle->handshake_msg_count <= 2)
			{
			
				if (xhandle->use_screen_buffer)
				{
					// Data is copied to intermediate screen buffer from image source.
					// Scroll settings handled by data copy section.
					XPutImage ( xhandle->display, xhandle->window, xhandle->context, \
									xhandle->ximage, 0, 0, 0, 0, \
									xhandle->display_width, xhandle->display_height);
				}
				else
				{
					// Display is directly from image source.
					// Scroll settings handled by "PutImage".
					XPutImage ( xhandle->display, xhandle->window, xhandle->context, \
									xhandle->ximage, xhandle->x_origin, xhandle->y_origin, 0, 0, \
									(xhandle->img_width - xhandle->x_origin), (xhandle->img_height - xhandle->y_origin));
				}
				// Handle possible hang (grrrr...) in X11 event queue by sending a handshake to synchronize.
				_SendHandshakeMsg(xhandle);			
			}
      }
      
   }
}

static void display_update( X_VIEW_HANDLE xhandle)
{
   if ((xhandle->valid) && (xhandle->destroyInProgress == 0))
   {
      if (pthread_mutex_trylock( &xhandle->display_update_mutex) == 0)
      {
			if (xhandle->use_screen_buffer)
			{
     //printf("left=%d  : top=%d : dw=%d : dh=%d : buf=0x%x\n", xhandle->x_origin, xhandle->y_origin, xhandle->display_width, xhandle->display_height, xhandle->img_ptr);
				// Copy image data from the current source buffer to the screen buffer.
            CopyDataToX11Image( xhandle, xhandle->display_width, xhandle->display_height, \
							xhandle->x_origin, xhandle->y_origin, xhandle->img_width, \
							xhandle->img_height, xhandle->img_depth, xhandle->img_ptr); 
			}
			display_update_locked(xhandle);
         pthread_mutex_unlock(&xhandle->display_update_mutex);
      }
   }
}

static int scroll_handler( X_VIEW_HANDLE xhandle, int delta_x, int delta_y)
{
   int h_scroll = FALSE;
   int v_scroll = FALSE;
   int update = FALSE;
 
   // Check the ability for scrolling.
   
   h_scroll = (xhandle->display_width < xhandle->img_width);
   v_scroll = (xhandle->display_height < xhandle->img_height);
   
   // Handle the horizontal scrolling.
   if (h_scroll)
   {
      if ( delta_x < 0 )
      {
         xhandle->x_origin = ( (xhandle->x_origin + delta_x) < 0 ) ? 0 : (xhandle->x_origin + delta_x);
         update = TRUE;
      }
      if ( delta_x > 0 )
      {
         if ((xhandle->display_width + xhandle->x_origin) < (xhandle->img_width-1))
         {
            if ( (xhandle->display_width + xhandle->x_origin + delta_x) > (xhandle->img_width-1))
            {
               xhandle->x_origin = xhandle->img_width - xhandle->display_width;
            }
            else
            {
               xhandle->x_origin += delta_x;
            }                             
            update= TRUE;
         }
			else
			{
				xhandle->x_origin = xhandle->img_width - xhandle->display_width;
				if (xhandle->use_screen_buffer)
				{
					update= TRUE;
				}
			}
      }
   }
      
   // Handle the vertical scrolling.
   if (v_scroll)
   {
      if ( delta_y < 0 )
      {
         xhandle->y_origin = ( (xhandle->y_origin + delta_y) < 0 ) ? 0 : (xhandle->y_origin + delta_y);
         update = TRUE;
      }
      if ( delta_y > 0 )
      {
         if ((xhandle->display_height + xhandle->y_origin) < (xhandle->img_height-1))
         {
            if ( (xhandle->display_height + xhandle->y_origin + delta_y) > (xhandle->img_height-1))
            {
               xhandle->y_origin = xhandle->img_height - xhandle->display_height;
            }
            else
            {
               xhandle->y_origin += delta_y;
            }                             
            update= TRUE;
         }
         else
         {
				xhandle->y_origin = xhandle->img_height - xhandle->display_height;
 				if (xhandle->use_screen_buffer)
				{
					update= TRUE;
				}
			}
      }
   }   

   return update;
}

// X11 Event handler thread function.
// It is used when the application does not have a main event loop with X11 callbacks.
// It is only used for events for the window we created to hold the image.
static void* eventHandler(void *context)
{
   int h_scroll = FALSE;
   int v_scroll = FALSE;
   int x_offset = 0;
   int y_offset = 0;
   int pan_mode = FALSE;
   int image_moved = FALSE;
   int base_time = 0;

   pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
   pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);

   // Get handle to cnotrol structure.
   volatile X_VIEW_HANDLE xhandle = (volatile X_VIEW_HANDLE)context; 
   Display *display = NULL;
   
   if (xhandle != NULL)
   {
      display = xhandle->display;
   }

   //xhandle->display_width = xhandle->img_width;
   //xhandle->display_height = xhandle->img_height;
   xhandle->display_width = xhandle->screenDst.width;
   xhandle->display_height = xhandle->screenDst.height;
   xhandle->x_origin = 0;
   xhandle->y_origin = 0;
   
   // Event Handler loop
   for(;;)
   {
      //static XEvent event;
      XEvent event;

      if ( xhandle != NULL)
      {
         if (xhandle->destroyInProgress != 0)
         {         
            pthread_exit(0);
         }
         // Wait for the next X11 event.
         pthread_testcancel();
         XNextEvent(display, &event);
         pthread_testcancel();

         // After waiting : re-check if we are being destroyed.
         if ( xhandle == NULL)
         {
            pthread_exit(0);
         }
         else
         {
            if (xhandle->destroyInProgress != 0)
            {        
               pthread_exit(0);
            }
         }
      }
      else
      {
         // xhandle is NULL. We are done.
        pthread_exit(0);
      }
      
      // Handle window system event notifications.
      switch (event.type) 
      {
         case ConfigureNotify: // Creation notification
            xhandle->valid = TRUE;
            break;

         case ResizeRequest:  // User has resized the window. Pick off the coordinates.
            {
               int update = FALSE;
               int clip = FALSE;
               XResizeRequestEvent *size = (XResizeRequestEvent *)&event;
                  
//printf("RR : x=%d, y=%d, w = %d, h = %d\n", xhandle->x_origin, xhandle->y_origin, size->width, size->height); //??????????
			      pthread_mutex_lock(&xhandle->display_update_mutex); 
               if (size->width > xhandle->screenDst.width)
               {
						// Window width is larger than image width -> clip.
                  if (xhandle->x_origin != 0)
                  {
                     update = TRUE;  // Origin changed (scrolling) - update.
                  }
                  xhandle->x_origin = 0;
                  h_scroll = FALSE;
						size->width = xhandle->screenDst.width;
						clip = TRUE;
               }
               else
               {
						// Window width is smaller than image width.
                  h_scroll = TRUE;
                  if (xhandle->x_origin != 0)
                  {
							// Check if we need to move the image origin.
                     if ( xhandle->x_origin + size->width > xhandle->img_width )
                     {
                        // Hit the right side of the image - adjust the origin.
                        //xhandle->x_origin += xhandle->display_width - size->width;
                        xhandle->x_origin = xhandle->img_width - size->width;
                        if (xhandle->x_origin < 0)
                        {
                           xhandle->x_origin = 0;
                        }
                    	}
                    	update = TRUE;
                 	  	h_scroll = TRUE;
                  }
               }
               if (size->height > xhandle->screenDst.height)
               {
						// Window height is larger then image heigth - clip
                  if (xhandle->y_origin != 0)
                  {
                     update = TRUE;  // Origin changed - update.
                  }
                  xhandle->y_origin = 0;
                  v_scroll = FALSE;
						size->height = xhandle->screenDst.height;
						clip = TRUE;
               }
               else
               {
						// Window height is less than image height.
                  if (xhandle->y_origin != 0)
                  {
							// Check if we need to move the image origin.
                     if ( xhandle->y_origin + size->height > xhandle->img_height )
                     {
                        // Hit the bottom side of the image - adjust the origin.                     
                        xhandle->y_origin = xhandle->img_height - size->height;
                        if (xhandle->y_origin < 0)
                        {
                           xhandle->y_origin = 0;
                        }
                    	}
	                  update = TRUE;
                    	v_scroll = TRUE;
						}
               }
              
               xhandle->display_width = size->width;
               xhandle->display_height = size->height;
			      pthread_mutex_unlock(&xhandle->display_update_mutex); 
					if (clip)
					{
						//printf("Resize : \n");
               	XResizeWindow(xhandle->display, xhandle->window, xhandle->display_width, xhandle->display_height);
               	update = TRUE;
					}
             	if (update)
              	{
						//printf("Update : \n");
                  display_update(xhandle); 
					}
            }
            break;

         case KeyPress:  // User has pressed a key. Pick off the keycode.
            {
               XKeyEvent *key = (XKeyEvent *)&event; 
               KeySym ksym;
               XComposeStatus xstatus;
               char keybuf[16];
               int update = FALSE;
               
               XLookupString( key, keybuf, 16, &ksym, &xstatus);
               
			      pthread_mutex_lock(&xhandle->display_update_mutex); 
               if ( (ksym >= XK_Home) && (ksym <= XK_Begin) )
               {
                  switch (ksym)
                  {
                     case XK_Left: 
                        update = scroll_handler( xhandle, -1, 0);
                        break;
                     
                     case XK_Up: 
                        update = scroll_handler( xhandle, 0, -1);
                        break;
                     
                     case XK_Right: 
                        update = scroll_handler( xhandle, 1, 0);
                        break;
                     
                     case XK_Down: 
                        update = scroll_handler( xhandle, 0, 1);
                        break;
                        
                     case XK_Page_Up: 
                        update = scroll_handler( xhandle, 0, -(xhandle->display_height/2));
                        break;
                     
                     case XK_Page_Down: 
                        update = scroll_handler( xhandle, 0, (xhandle->display_height/2));
                        break;
                     
                     case XK_Home: 
                     case XK_Begin: 
                        update = scroll_handler( xhandle, -(xhandle->display_width/2), 0);
                        break;
                     
                     case XK_End: 
                        update = scroll_handler( xhandle, (xhandle->display_width/2), 0);
                        break;
                  }
               }
 			      pthread_mutex_unlock(&xhandle->display_update_mutex); 
               if (update)
               {
                 display_update(xhandle); 
               }
          }
            break;

         case ButtonPress:  // Button is pressed.
            {
               XButtonEvent *button = (XButtonEvent *)&event;
               pan_mode = TRUE;
               x_offset = button->x;
               y_offset = button->y;
               base_time = button->time;
            }
            break;

         case ButtonRelease:  // Button is released.
            {
               pan_mode = FALSE;
               if (image_moved)
               {
                 display_update(xhandle);
                  image_moved = FALSE;
               }               
            }
            break;

         case MotionNotify:  // Pointer has been moved with button down !!!
            {
               int update = FALSE;
               XPointerMovedEvent *pointer = (XPointerMovedEvent *)&event;
  
			      pthread_mutex_lock(&xhandle->display_update_mutex); 

               update = scroll_handler( xhandle, (x_offset - pointer->x), (y_offset - pointer->y));
               if ((pointer->time - base_time) > 50)
               {
                  base_time = pointer->time;
               }
               else
               {
                  // Too many events.
                  update = FALSE;
               }
               image_moved = TRUE;
               x_offset = pointer->x;
               y_offset = pointer->y;
               if (update)
               {
                  display_update_locked(xhandle);               
               }              
			      pthread_mutex_unlock(&xhandle->display_update_mutex); 
//printf("MN : x=%d, y=%d, w = %d, h = %d\n", xhandle->x_origin, xhandle->y_origin, xhandle->display_width, xhandle->display_height); //??????????
            }
            break;

         case Expose: // Visual update required notification (redraw).
            {
               //XExposeEvent *expose = (XExposeEvent *)&event;

               display_update( xhandle);
            }  
            break;
            
         case DestroyNotify:  // Window has been destroyed cleanly by application
            pthread_exit(0);
            break;

         case ClientMessage:
            if (event.xclient.data.l[0] == (int)xhandle->atomDelete)
            {
               // WM_DESTROY_WINDOW event : X11 server has killed window ("close" or "x" from WM control).
               Clean_Up(xhandle);
               pthread_exit(0);
            }
            if (event.xclient.data.l[0] == (int)xhandle->atomHandshake)
            {
					// Handshake message - keeps queue from filling (and blocking).
					xhandle->handshake_msg_count--;
					if (xhandle->handshake_msg_count > 2)
					{
						XSync(xhandle->display, False); 
					}
            }
            break;
            
         default:  // Other events we just ignore (mostly - except for the shmCompletion (a handshake for shm image updating).
            if (xhandle->useshm && (event.type == xhandle->shmEventCompleteType))
            {
               xhandle->shmInProgress = FALSE;
            }
            break;
      }
   }
   pthread_exit(0);
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

// Copy To Display thread function.
// It is used to remove the actual data copy from the image buffer in memory  to
// the display surface from the callback function. Delaying too long in the callback
// function causes events to be lost.
static void* displayHandler(void *context)
{
   unsigned long cur_time = 0;
   unsigned long timeout = 1000 / MAX_FRAME_UPDATE_RATE;
      
   pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
   pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);

   // Get handle to control structure.
   volatile X_VIEW_HANDLE xhandle = (volatile X_VIEW_HANDLE)context; 
   
   if (xhandle == NULL)
   {
     pthread_exit(0);
   }

   // Display update Handler loop
   for(;;)
   {
		void *image_ptr = NULL;
      // Wait to be signalled to update the display.
		pthread_mutex_lock(&xhandle->display_signal_mutex); 
		while ( (xhandle->display_signal_flag == 0) && (xhandle->destroyInProgress != 1))
		{
			pthread_cond_wait(&xhandle->display_signal_cv, &xhandle->display_signal_mutex);
		} 
		xhandle->display_signal_flag--;
		pthread_mutex_unlock(&xhandle->display_signal_mutex); 

      if (xhandle != NULL)
      {
         if (xhandle->destroyInProgress == 1)
         {
           pthread_exit(0);
         }
      }
      else
      {
        pthread_exit(0);      
      }
      
      // Update the display data (one thread at a time).
      if (xhandle->valid == TRUE)
      {
			xhandle->display_signal_flag = 0;
			image_ptr = xhandle->img_ptr; 
         // Only display if enough time has passed since the last time to avoid
         // swamping the X server with updates.
         if ( ms_timer_interval_elapsed( cur_time, timeout) )
         {
            cur_time = ms_timer_init();

            //pthread_mutex_lock( &xhandle->display_update_mutex);
            if (pthread_mutex_trylock( &xhandle->display_update_mutex) == 0)  
            {
               if (xhandle->destroyInProgress == 1)
               {
						pthread_mutex_unlock(&xhandle->display_update_mutex);
                  pthread_exit(0);
               }

					if (xhandle->use_screen_buffer)
					{
						// Copy image data from the current source buffer to the screen buffer.
						if ( !CopyDataToX11Image( xhandle, xhandle->display_width, xhandle->display_height, \
									xhandle->x_origin, xhandle->y_origin, xhandle->img_width, \
									xhandle->img_height, xhandle->img_depth, xhandle->img_ptr) )
						{
							display_update_locked(xhandle);
						}
					}
					else
					{
						// Copy image data from the current source buffer to the entire drawable.
						if ( !CopyDataToX11Image( xhandle, xhandle->screenDst.width, xhandle->screenDst.height, \
									0, 0, xhandle->img_width, xhandle->img_height, xhandle->img_depth, xhandle->img_ptr) )
						{
							display_update_locked(xhandle);
						}
					}
               pthread_mutex_unlock(&xhandle->display_update_mutex);
         }
         }
      }
   }
   pthread_exit(0);
}



int Display_Image( X_VIEW_HANDLE xhandle, int depth, int width, int height, void *image)
{
   if (xhandle != NULL)
   {
		if (xhandle->valid)
		{
			if ( pthread_mutex_trylock(&xhandle->display_signal_mutex) == 0)
			{ 
				xhandle->img_ptr = image;
				xhandle->display_signal_flag++;  
				pthread_cond_signal(&xhandle->display_signal_cv);  
				pthread_mutex_unlock(&xhandle->display_signal_mutex); 
			}
		}
   }
   return 0;
}

static int Copy16BitMonoDataToX11Image(X_VIEW_HANDLE xhandle, int width, int height, 
								int src_left, int src_top, int src_width, int src_height, 
								int src_depth, void *src_image)
{
   XImage *ximage;
   int line, pixel, shift;
   int dstbpp;
   int dstbpl;
   int srcbpp = src_depth;
   int srcbpl;
   u_int8_t *dst_8 = NULL;
   u_int16_t *src_16 = NULL, *dst_16 = NULL;
   u_int32_t *dst_32 = NULL;

   srcbpl = src_width * ((srcbpp + 7)/8);

   ximage = xhandle->ximage;
   dstbpp  = ximage->bits_per_pixel;
   dstbpl  = ximage->bytes_per_line;

   /*
    * 16-bit source - mono format - shift them down to 8 bits mono.
    */
   src_16 = (u_int16_t *)(src_image);
	src_16 += (src_left) + (src_top * srcbpl/2);

   shift = src_depth - 8;

   switch (dstbpp)
   {
            case 8:
               /*
                * 16-bit source, 8-bit destination
                */
               dst_8 = (u_int8_t *)(ximage->data);

               for (line = 0; line < height; line++)
               {
                  for (pixel = 0; pixel < width; pixel++, dst_8++, src_16++)
                  {
                     *dst_8 = xhandle->lut[*src_16 >> shift];
                  }
                  src_16 += srcbpl / 2 - width;
                  dst_8  += dstbpl - width;
               }
            break;

         case 16:
            /*
             * 16-bit source, 16-bit destination
             */
            dst_16 = (u_int16_t *)(ximage->data);

            for (line = 0; line < height; line++)
            {
               for (pixel = 0; pixel < width; pixel++, dst_16++, src_16++)
               {
                  *dst_16 = xhandle->lut[*src_16 >> shift];
               }
               src_16 += srcbpl / 2 - width;
               dst_16 += dstbpl / 2 - width;
            }
            break;

         case 24:
         case 32:
            /*
             * 16-bit source, 24/32-bit destination
             */
            dst_32 = (u_int32_t *)(ximage->data);
         
            for (line = 0; line < height; line++)
            {
               for (pixel = 0; pixel < width; pixel++, dst_32++, src_16++)
               {
					#if 1
                  u_int8_t *putC = (u_int8_t *)dst_32;
                  putC[0] = putC[1] = putC[2] = putC[3] = (*src_16)>>shift;
               #else
		  				*dst_32 = m_lut_888[(*src_16)>>shift];
					#endif
               }
               src_16 += srcbpl / 2 - width;
               dst_32 += dstbpl / 4 - width;
            }
            break;

         default:
            fprintf(stderr, "Unsupported depth combination."
               "Source: %d bpp, destination: %d bpp\n", src_depth, dstbpp);
               return -1;
            break;
   } 
   return 0;
}

static int Copy16BitColorDataToX11Image(X_VIEW_HANDLE xhandle, int width, int height,
               int src_left, int src_top, int src_width, int src_height, int src_depth, void *src_image)
{
   XImage *ximage;
   int line, pixel, shift;
   int dstbpp;
   int dstbpl;
   int srcbpp = src_depth;
   int srcbpl;
   u_int8_t  *dst_8 = NULL;
   u_int16_t *src_16 = NULL, *dst_16 = NULL;
   u_int32_t *dst_32 = NULL;


   srcbpl = width * ((srcbpp + 7)/8);

   ximage = xhandle->ximage;
   dstbpp  = ximage->bits_per_pixel;
   dstbpl  = ximage->bytes_per_line;

   /*
    * 16-bit source - mono format - shift them down to 8 bits mono.
    */

   src_16 = (u_int16_t *)(src_image);
	src_16 += (src_left) + (src_top * srcbpl/2);

   shift = src_depth - 8;
   switch (dstbpp)
   {
            case 8:
               /*
                * 16-bit source, 8-bit destination
                */
               dst_8 = (u_int8_t *)(ximage->data);

               for (line = 0; line < height; line++)
               {
                  for (pixel = 0; pixel < width; pixel++, dst_8++, src_16++)
                  {
                     *dst_8 = *src_16 >> shift;
                  }
                  src_16 += srcbpl / 2 - width;
                  dst_8  += dstbpl - width;
               }
            break;

         case 16:
            /*
             * 16-bit source, 16-bit destination
             */
            dst_16 = (u_int16_t *)(ximage->data);

            for (line = 0; line < height; line++)
            {
               for (pixel = 0; pixel < width; pixel++, dst_16++, src_16++)
               {
                  u_int16_t putpix = 0;
                  if (xhandle->format == CORX11_DATA_FORMAT_RGB5551)
                  {
                     putpix = (*src_16 & 0x0000003e) >> 1;
                     putpix |= (*src_16 & 0x000007c0);
                     putpix |= (*src_16 & 0x0000f800);
                     *dst_16 = putpix << 1;
                  }
                  else
                  {
                     *dst_16 = *src_16;
                  }
               }
               src_16 += srcbpl / 2 - width;
               dst_16 += dstbpl / 2 - width;
            }
            break;

         case 24:
         case 32:
            /*
             * 16-bit source, 24/32-bit destination
             */
            dst_32 = (u_int32_t *)(ximage->data);
         
            for (line = 0; line < height; line++)
            {
               for (pixel = 0; pixel < width; pixel++, dst_32++, src_16++)
               {
                  u_int8_t *putC = (u_int8_t *)dst_32;
                  if (xhandle->format == CORX11_DATA_FORMAT_RGB565)
                  {
                     putC[0] = (*src_16 & 0x0000001f) << 3;
                     putC[1] = (*src_16 & 0x000007e0) >> 3;
                     putC[2] = (*src_16 & 0x0000f800) >> 8;
                     putC[3] = 0;
                  }
                  else if (xhandle->format == CORX11_DATA_FORMAT_RGB5551)
                  {
                     putC[0] = (*src_16 & 0x0000001f) << 3;
                     putC[1] = (*src_16 & 0x000003e0) >> 2;
                     putC[2] = (*src_16 & 0x00007c00) >> 7;
                     putC[3] = 0;
                  }
                  else
                  {
                     putC[0] = putC[1] = putC[2] = putC[3] = *src_16>>shift;
                  }
               }
               src_16 += srcbpl / 2 - width;
               dst_32 += dstbpl / 4 - width;
            }
            break;

         default:
            fprintf(stderr, "Unsupported depth combination."
               "Source: %d bpp, destination: %d bpp\n", src_depth, dstbpp);
               return -1;
            break;
   }
   return 0;
}

int CopyDataToX11Image( X_VIEW_HANDLE xhandle, int width, int height, int src_left, int src_top, int src_width, int src_height, int src_depth, void *src_image)
{
	XImage *ximage;
	int line, pixel;
	int dstbpp;
	int dstbpl;
	int srcbpp = src_depth;
	int srcbpl;
	u_int8_t *src_8 = NULL, *dst_8 = NULL;
	u_int16_t *dst_16 = NULL;
	u_int32_t *src_32 = NULL, *dst_32 = NULL;


	if ( (xhandle == NULL) || (src_image == NULL))
	{
		return -1;
	}
	ximage = xhandle->ximage;
	dstbpp  = ximage->bits_per_pixel;
	dstbpl  = ximage->bytes_per_line;
	srcbpl = src_width * ((srcbpp + 7)/8);

	switch (src_depth)
	{
		case 8:
      	/*
       	*	 8-bit source
       	*/
      	src_8 = (u_int8_t *)(src_image);
	  		src_8 += src_left + (src_top * srcbpl); 

      	switch (dstbpp)
			{
				case 8:
	  				/*
	   		 	 * 8-bit source, 8-bit destination
	   			 */
	  				dst_8 = (u_int8_t *)(ximage->data);

	  				for (line = 0; line < height; line++)
	    			{
	      			for (pixel = 0; pixel < width; pixel++, dst_8++, src_8++)
	      			{
							*dst_8 = xhandle->lut[*src_8];
						}
	      			src_8 += srcbpl - width;
	      			dst_8 += dstbpl - width;
	    			}
	  				break;

				case 16:
	  				/*
	  				 * 8-bit source, 16-bit destination
	  				 */
	  				dst_16 = (u_int16_t *)(ximage->data);
	  				for (line = 0; line < height; line++)
	   			{
	      			for (pixel = 0; pixel < width; pixel++, dst_16++, src_8++)
	      			{
							*dst_16 = xhandle->lut[*src_8];
						}
	      			src_8  += srcbpl - width;
	      			dst_16 += dstbpl / 2 - width;
	    			}
	  				break;

				case 24:
				case 32:
	  				/*
	  				 * 8-bit source, 24/32-bit destination
	  				 */
	  				dst_32 = (u_int32_t *)(ximage->data);

	  				for (line = 0; line < height; line++)
	    			{
	      			for (pixel = 0; pixel < width; pixel++, dst_32++,src_8++)
						{
						#if 1
		  					u_int8_t *putC = (u_int8_t *)dst_32;
		  					putC[0] = putC[1] = putC[2] = putC[3] = *src_8;
		  				#else
		  					*dst_32 = m_lut_888[*src_8];
		  				#endif
						}
	      			src_8 += srcbpl - width;
	      			dst_32 += dstbpl / 4 - width;
	    			}
	  				break;

				default:
	  				fprintf(stderr, "Unsupported depth combination."
		  					"Source: %d bpp, destination: %d bpp\n", src_depth, dstbpp);
	  				return -1;
			}
      break;

		case 10:
    	case 12:
   	case 14:
    	case 16:
      	/*
      	 * 16-bit source
      	 */ 
          if (xhandle->format == CORX11_DATA_FORMAT_MONO)
         {
            return Copy16BitMonoDataToX11Image( xhandle, width, height, src_left, src_top, src_width, src_height, src_depth, src_image);
         }
         else
         {
           return Copy16BitColorDataToX11Image( xhandle, width, height, src_left, src_top, src_width, src_height, src_depth, src_image);
         }


		case 24:
      	/*
      	 * 24-bit source, 24/32-bit destination
      	 * 8/16-bit destination not yet supported
      	 */
      	switch (dstbpp)
			{
				case 8:
               fprintf(stderr, "Unsupported depth combination."
                  "Source: %d bpp, destination: %d bpp\n", src_depth, dstbpp);
               return -1;
				case 16:
               /* 24 bit source (color assumed), 16-bit destination. (RGB565)*/
               src_8  = (u_int8_t *)(src_image);
 	  				src_8 += src_left + (src_top * srcbpl); 
               dst_16 = (u_int16_t *)(ximage->data);

               for (line = 0; line < height; line++)
               {
                  for (pixel = 0; pixel < width; pixel++, dst_16++, src_8 += 3)
                  {
                     u_int16_t temp = 0;

                     temp = ((*src_8 >> 3) & 0x001f);
                     temp |= ((src_8[1] >> 2) & 0x003f) << 5;
                     temp |= ((src_8[2] >> 3) & 0x001f) << 11;
                     *dst_16 = temp;
                  }
                  src_8  += srcbpl / 3 - width;
                  dst_16 += dstbpl / 2 - width;
               }
               break;
               
				case 24:
				case 32:
	  				/*
	  				 * Same routine for 24/32-bit destination.
	  				 * This code won't work for packed 24-bit destinations.
	  				 * Assumption is that the most significant byte
	  				 * is unused in 32-bit mode.
	  				 */
	  				src_8  = (u_int8_t *)(src_image);
	  				src_8 += src_left + (src_top * srcbpl); 
	  				dst_32 = (u_int32_t *)(ximage->data);

	  				for (line = 0; line < height; line++)
	    			{
	      			for (pixel = 0; pixel < width; pixel++, dst_32++, src_8 += 3)
						{
		  					u_int32_t temp;

		  					temp = *src_8 + (src_8[1] << 8) + (src_8[2] << 16);
		  					*dst_32 = temp;
						}
	      			src_8  += srcbpl / 3 - width;
	      			dst_32 += dstbpl / 4 - width;
	    			}
	  				break;

				default:
	  				fprintf(stderr, "Unsupported depth combination."
		  				"Source: %d bpp, destination: %d bpp\n", src_depth, dstbpp);
	  				return -1;
			}
      	break;


    	case 32:
      	/*
      	 * 32-bit source, 16/24/32-bit destination
      	 * 8-bit destination not yet supported
      	 */
      	switch (dstbpp)
			{
				case 8:
	  				fprintf(stderr, "Unsupported depth combination."
		  				"Source: %d bpp, destination: %d bpp\n", src_depth, dstbpp);
	  				return -1;
            case 16:
               /* 32 bit source (color assumed), 16-bit destination. (RGB565)*/
               src_8   = (u_int8_t *)(src_image);
	  				src_8  += src_left + (src_top * srcbpl); 
               src_32  = (u_int32_t *)(src_image);
	  				src_32 += (src_left) + (src_top * srcbpl/4); 
               dst_16  = (u_int16_t *)(ximage->data);

               if ( xhandle->format == CORX11_DATA_FORMAT_RGB101010)
               {
                  for (line = 0; line < height; line++)
                  {
                     for (pixel = 0; pixel < width; pixel++, dst_16++, src_32++)
                     {
                        u_int16_t temp = 0;
   
                        temp = (*src_32 >> 5) & 0x001f;
                        temp |= ((*src_32 >> 14) & 0x003f) << 5;
                        temp |= ((*src_32 >> 25) & 0x001f) << 11;
                        *dst_16 = temp;
                     }
                     dst_16 += dstbpl / 2 - width;
                  }
               }
               else
               {
                  for (line = 0; line < height; line++)
                  {
                     for (pixel = 0; pixel < width; pixel++, dst_16++, src_8 += 4)
                     {
                        u_int16_t temp = 0;
   
                        temp = (*src_8 >> 3) & 0x001f;
                        temp |= ((src_8[1] >> 2) & 0x003f) << 5;
                        temp |= ((src_8[2] >> 3) & 0x001f) << 11;
                        *dst_16 = temp;
                     }
                     src_8  += srcbpl / 4 - width;
                     dst_16 += dstbpl / 2 - width;
                  }
               }
               break;

				case 24:
				case 32:
	  				/*
	  				 * 32-bit source, 24/32-bit destination
	  				 * This code won't work for packed 24-bit destinations.
	  				 * TRUE-COLOR MODE
	  				 */
	  				src_32 = (u_int32_t *)(src_image);
	  				src_32 += (src_left) + (src_top * srcbpl/4); 
	  				dst_32 = (u_int32_t *)(ximage->data);
 
               if ( xhandle->format == CORX11_DATA_FORMAT_RGB101010)
               {
                  for (line = 0; line < height; line++)
                  {
                     for (pixel = 0; pixel < width ; pixel++, dst_32++, src_32++)
                     {
                        u_int32_t temp = 0;
                        temp =  (*src_32 >> 2) & 0x00ff;
                        temp |= ((*src_32 >> 12) & 0x00ff) << 8;
                        temp |= ((*src_32 >> 22) & 0x00ff) << 16;
                        *dst_32 = temp;;
                     }
                     src_32 += srcbpl/4 - width;
                     dst_32 += dstbpl/4 - width;
                  }
               }
               else
               {
                  for (line = 0; line < height; line++)
                  {
					#if 0
                     for (pixel = 0; pixel < width ; pixel++, dst_32++, src_32++)
                     {
                        *dst_32 = *src_32;
                     }
							src_32 += srcbpl/4 - width;
							dst_32 += dstbpl/4 - width;
                 #else
							memcpy( dst_32, src_32, dstbpl);
							src_32 += srcbpl/4;
							dst_32 += dstbpl/4;
                #endif
                  }
               }
	  				break;

				default:
	  				fprintf(stderr, "Unsupported depth combination."
		  				"Source: %d bpp, destination: %d bpp\n", src_depth, dstbpp);
	  				return -1;
         }
      	break;

		default:
      	fprintf(stderr, "Unsupported depth combination."
	      	"Source: %d bpp, destination: %d bpp\n", src_depth, dstbpp);
      	return -1;
	}
  return 0;
}

static void init_8bit_dataluts()
{
	int i = 0;
	for (i = 0; i < 256; i++)
	{
		m_lut_888[i]  = i + (i << 8) + (i << 16);
		m_lut_8888[i] = i + (i << 8) + (i << 16) + (i << 24);
	} 
}

Window X11GetParent(X_VIEW_HANDLE xhandle, Window w) 
{ 
	Display* dsp = NULL; 
	Window parent = 0; 
	Window root = 0; 
	Window *wins = NULL;
	unsigned int n = 0;

	if (xhandle != NULL)
	{  
		dsp = xhandle->display; 
		if ( w >= 0 ) 
		{ 
			// Get the array of stacked windows
    		XQueryTree(dsp, w, &root, &parent, &wins, &n);

			if (wins != NULL)
			{
				XFree(wins);
			}
			return parent;
		}
	}
	return 0;
} 

Window X11GetWindowWithFocus(X_VIEW_HANDLE xhandle) 
{ 
	Display* dsp = NULL; 
	Window w = 0; 
	Window parent = 0; 
	Window root = 0; 
   Window *wins;
	unsigned int n;
  
   dsp = xhandle->display;
	if ( dsp ) 
	{ 
		w = DefaultRootWindow( dsp ); 
		if ( w >= 0 ) 
		{ 
			// Get the array of stacked windows
    		XQueryTree(dsp, w, &root, &parent, &wins, &n);

			//return parent;
			// Return the top most window (Last one on stack).
			if (wins != NULL)
			{
				Window wd = parent;
				if (n > 0)
				{
					wd = wins[n-1];
				}
				XFree(wins);
				return wd;
			}
		}
	}
	return 0;
} 

#if 0
void X11DumpWindowStack() 
{ 
	Display* dsp = NULL; 
	Window w = 0; 
	Window parent = 0; 
	Window root = 0; 
   Window *wins;
	unsigned int n;
  
	dsp = XOpenDisplay( NULL ); 
	if ( dsp ) 
	{ 
		w = DefaultRootWindow( dsp ); 
		if ( w >= 0 ) 
		{ 
			// Get the array of stacked windows
    		XQueryTree(dsp, w, &root, &parent, &wins, &n);

			// Return the top most window (Last one on stack).
			if (wins != NULL)
			{
				int i;
				for (i = 0; i < n; i++)
				{
					printf("stack win[%d] = 0x%x\n", i, (int)wins[i]);
				}

				XFree(wins);
			}
		}
	}
} 
#endif

int X11GetRootWindowSize(int *width, int *height) 
{ 
	Display* dsp = NULL; 
	Window w = 0; 
	XWindowAttributes xwAttr; 
	int status = -1;
  
	dsp = XOpenDisplay( NULL ); 
	if ( dsp ) 
	{ 
		w = DefaultRootWindow( dsp ); 
		if ( w >= 0 ) 
		{ 
			status = XGetWindowAttributes( dsp, w, &xwAttr ); 
			if (width != NULL)
			{
				*width = xwAttr.width; 
			}
			if (height != NULL)
			{
				*height = xwAttr.height;
			} 
			XCloseDisplay( dsp );
			status = 0; 
		}
	}
	return status; 
} 
  
int X11GetScreenSize(int *width, int *height) 
{ 
	Display* dsp = NULL; 
	Screen* scr = NULL; 
	int status = -1;
  
	dsp = XOpenDisplay( NULL ); 
	if ( dsp ) 
	{ 
		scr = DefaultScreenOfDisplay( dsp ); 
		if (scr)
		{
			if (width != NULL)
			{
				*width = scr->width; 
			}
			if (height != NULL)
			{
				*height = scr->height;
			} 
			XCloseDisplay( dsp );
			status = 0; 
		}
	}
	return status;
}
 
static X_VIEW_HANDLE PreCreateX11DisplayWindow( int height, int width, int image_depth, int data_format, int use_shared_memory)
{
	int i, j, screen, planes, screen_depth;
	long raw_size;
	int  image_height, image_width;
	Colormap colormap;
	XColor color;
	XGCValues values;
	Font timefont;
   int fmono;
   int depth;
   static int init_once = 0;

	X_VIEW_HANDLE	xhandle = NULL;

   if (init_once == 0)
   {
      init_once = 1;
      XInitThreads();
      init_8bit_dataluts();
   }
   
	xhandle = (X_VIEW_HANDLE)malloc(sizeof(X_VIEW_OBJECT));
	if ( xhandle == NULL)
	{
		return NULL;
	}
	if ((width == 0) || (height == 0) || (image_depth == 0))
	{
		return NULL;
	}

   xhandle->destroyInProgress = 0;
   
	xhandle->version = 1;
	xhandle->shminfo.shmaddr = NULL;
	xhandle->context = 0;
	xhandle->display = NULL;
	xhandle->ximage = NULL;
	xhandle->useshm = use_shared_memory;
	xhandle->valid = FALSE;
   xhandle->img_depth = image_depth;
   xhandle->img_width = width;
   xhandle->img_height = height;
   xhandle->img_ptr  = NULL;
   xhandle->display_signal_flag = 0;
	xhandle->handshake_msg_count = 0;
   pthread_mutex_init( &xhandle->display_update_mutex, NULL);
	pthread_mutex_init( &xhandle->display_signal_mutex, NULL); 
	pthread_cond_init( &xhandle->display_signal_cv, NULL); 
	pthread_mutex_unlock(&xhandle->display_signal_mutex); 

	// Get the screen size and decide if we are going to use a dedicated
	// screen buffer (instead of directly using the users buffer).
	X11GetScreenSize( &xhandle->screen_width, &xhandle->screen_height);
	if ( width < (2*xhandle->screen_width) && (height < (2*xhandle->screen_height)))
	{
		// Image is small enough to use the users memory directly.
		xhandle->use_screen_buffer = FALSE;
		xhandle->screenDst.left = 0;
		xhandle->screenDst.top = 0;			
		xhandle->screenDst.height = height;
		xhandle->screenDst.width = width;
		xhandle->screenDst.format = data_format;
		xhandle->screenDst.pixel_size = image_depth;
		xhandle->screenDst.stride = width * ((image_depth+7)/8); 	

		image_height = height;
		image_width = width;
	}
	else
	{
		// Image is large - use a dedicated screen buffer for display.
		xhandle->use_screen_buffer = TRUE;
		xhandle->screenDst.left = 0;
		xhandle->screenDst.top = 0;
		if (width < (2*xhandle->screen_width))
		{
			xhandle->screenDst.width = width;
		}
		else
		{
			xhandle->screenDst.width = xhandle->screen_width;
		}			
		if (height < (2*xhandle->screen_height))
		{
			xhandle->screenDst.height = height;
		}
		else
		{
			xhandle->screenDst.height = xhandle->screen_height;
		}			
		xhandle->screenDst.format = data_format;
		xhandle->screenDst.pixel_size = image_depth;
		xhandle->screenDst.stride = xhandle->screenDst.width * ((image_depth+7)/8); 	

		image_height = xhandle->screenDst.height;
		image_width = xhandle->screenDst.width;

	}


	if ((xhandle->display = XOpenDisplay (NULL)) == NULL)
   {
      fprintf (stderr, "Can't open display.\n");
      Clean_Up (xhandle);
      return NULL;
   }

	screen = DefaultScreen(xhandle->display);
	planes = DisplayPlanes (xhandle->display, screen);
	colormap = DefaultColormap (xhandle->display, screen);
	raw_size = PAGE_SIZE * (1 + ( ((image_width * image_height) + (PAGE_SIZE-1)) / PAGE_SIZE));
   
   // Check the image depth / image format combinations.
   
   xhandle->format = data_format;
   switch(data_format)
   {
      case CORX11_DATA_FORMAT_MONO:
          fmono = TRUE;
          depth = 8;     // Will be displayed as 8 bits.
          break;
      
      case CORX11_DATA_FORMAT_RGB888:
         fmono = FALSE;
         depth = 24;
         break;

      case CORX11_DATA_FORMAT_RGB8888:
         fmono = FALSE;
         depth = 32;
         break;
        
      case CORX11_DATA_FORMAT_RGB5551:
         fmono = FALSE;
         depth = 16;
         break;

      case CORX11_DATA_FORMAT_RGB565:
         fmono = FALSE;
         depth = 16;
         break;
      
      case CORX11_DATA_FORMAT_RGB101010:
         fmono = FALSE;
         depth = 24;       // Will be converted to 888.
         break;
   
      default:
         fmono = (image_depth > 8) ? FALSE : TRUE;
         depth = image_depth;
         xhandle->format = CORX11_DATA_FORMAT_DEFAULT;
         break;
   }
   
   // Calculate the raw size of the ximage storage.
	if ( ((depth+7) / 8) != 0)
	{
		raw_size  = (raw_size * depth) / 8;
	}

	screen_depth = 8;
	if ( planes > 8 && planes <= 16 )
	{
		screen_depth = 16;
	}
	else if ( planes > 16 )
	{
		screen_depth = 32;
	}
   
	if ((xhandle->window = XCreateSimpleWindow (xhandle->display, RootWindow (xhandle->display, screen),
				     0, 0, image_width, image_height, 8, BlackPixel (xhandle->display, screen),
				     WhitePixel (xhandle->display, screen))) == 0)
	{
   	fprintf (stderr, "Can't create window with XCreateSimpleWindow.\n");
      Clean_Up (xhandle);
      return NULL;
   }
   XSelectInput(xhandle->display, xhandle->window, SubstructureRedirectMask | StructureNotifyMask | ExposureMask | \
					KeyPressMask | ResizeRedirectMask | ButtonPressMask | ButtonReleaseMask | \
					Button2Mask | Button2MotionMask| Button3Mask | Button3MotionMask); 
	xhandle->valid = TRUE;
   
	if ( fmono == FALSE )
	{
		for (i = 0; i < 256; i++)
		{
			xhandle->lut[i] = i << (screen_depth - 8);
		}
	}
	else
	{
		// Set up the LUT to map monochrome input data (depth bits) to the output display.
      depth = 8;
		for (i = 0; i < (1 << depth); i++)
		{
			color.pad = 0;
			//color.red = color.green = color.blue = (65536 / (1 << depth)) * i;
			color.red = color.green = color.blue = (65536 / (1 << image_depth)) * i;
			
			if (XAllocColor (xhandle->display, colormap, &color) == 0)
			{
				fprintf (stderr, "Can't allocate color (%d/%d).\n", i, image_depth);
				Clean_Up (xhandle);
				return NULL;
			}
         
			switch ((1 << depth))
			{
				default:
				case 256:
					xhandle->lut[i] = color.pixel;
					break;
				case 128:
					xhandle->lut[i * 2] = color.pixel;
					xhandle->lut[i * 2 + 1] = color.pixel;
					break;
				case 64:
					for (j = 0; j < 4; j++ )
						xhandle->lut[i * 4 + j] = color.pixel;
					break;
				case 32:
					for (j = 0; j < 8; j++ )
						xhandle->lut[i * 8 + j] = color.pixel;
					break;
				case 16:
					for (j = 0; j < 16; j++ )
						xhandle->lut[i * 16 + j] = color.pixel;
					break;
				case 8:
					for (j = 0; j < 32; j++ )
						xhandle->lut[i * 32 + j] = color.pixel;
					break;
			}
      }
	}


	if ((xhandle->context = XCreateGC (xhandle->display, xhandle->window, 0, &values)) == NULL)
   {
   	fprintf (stderr, "Can't create context.\n");
   	Clean_Up (xhandle);
      return NULL;
   }

	XSetForeground (xhandle->display, xhandle->context, WhitePixel(xhandle->display, screen));
	XSetBackground (xhandle->display, xhandle->context, BlackPixel(xhandle->display, screen));

	{
		// Set up a fixed-point font.
		char *pattern = "*fixed*";
		char **fontlist = NULL;
		int numFont = 0;
		
		fontlist = XListFonts( xhandle->display, pattern, 1, &numFont);

		if (numFont > 0)
		{
		#if 0
			for (i = 0; i < numFont; i++)
			{
				printf("Font %d = %s\n", i, fontlist[i]);
			}
		#endif

			timefont = XLoadFont (xhandle->display, fontlist[0]);
			if (timefont != BadName)
			{
				XSetFont (xhandle->display, xhandle->context, timefont);
			}
			XFreeFontNames(fontlist);
		}
		
	}

	if (xhandle->useshm)
   {
      int useshm = XShmQueryExtension (xhandle->display);
      if (!useshm)
		{
	  		fprintf (stderr, "XSharedMemoryExtension not available!\n");
	  		xhandle->useshm = FALSE;
		}
	}

	//????BUGBUG??????X with shared memory is broken on some 64-bit systems ??????????
	// causes 100% CPU usage on idle.
	//xhandle->useshm = FALSE; //?????turn off shared memory????????

	if (xhandle->useshm)
	{
      if ((xhandle->ximage = XShmCreateImage (xhandle->display, DefaultVisual (xhandle->display, screen),
				     planes, ZPixmap, NULL, &xhandle->shminfo,
				     image_width, image_height)) == NULL)
      {
	  		fprintf (stderr, "Can't create image.\n");
	  		Clean_Up (xhandle);
      	return NULL;
      }

      xhandle->shminfo.shmid = shmget (IPC_PRIVATE,
			      			((raw_size * xhandle->ximage->bits_per_pixel) / (double)depth),
			      			(IPC_CREAT | 0777) );

      xhandle->shminfo.shmaddr = (char *)shmat (xhandle->shminfo.shmid, 0, 0);
      xhandle->ximage->data = (char *)xhandle->shminfo.shmaddr;
      if (!XShmAttach (xhandle->display, &xhandle->shminfo))
		{
			XDestroyImage(xhandle->ximage);
			shmdt (xhandle->shminfo.shmaddr);
			shmctl (xhandle->shminfo.shmid, IPC_RMID, 0);
			fprintf (stderr, "Can't attach shared memory");
			Clean_Up (xhandle);
      	return NULL;
		}
		xhandle->shmEventCompleteType = XShmGetEventBase(xhandle->display) + ShmCompletion;
		xhandle->shmInProgress = FALSE;
	}
	else
   {
		if ((xhandle->ximage = XCreateImage (xhandle->display, DefaultVisual (xhandle->display, screen),
				  planes, ZPixmap, 0, NULL,
				  image_width, image_height, 32, (image_width * (screen_depth / 8)))) == NULL)
		{
			fprintf (stderr, "Can't create image.\n");
			Clean_Up (xhandle);
      	return NULL;
      }

		if ((xhandle->ximage->data = (char *)malloc((raw_size * xhandle->ximage->bits_per_pixel)/(double)depth)) == NULL)
		{
			fprintf (stderr, "Virtual memory exhausted.\n");
			Clean_Up (xhandle);
      	return NULL;
		}
	}
   
   {
      xhandle->atomHandshake= XInternAtom( xhandle->display, "HANDSHAKE", True);
      xhandle->atomDelete = XInternAtom( xhandle->display, "WM_DELETE_WINDOW", True);
      XSetWMProtocols( xhandle->display, xhandle->window, &xhandle->atomDelete, 1);
   }
   return xhandle;
}

X_VIEW_HANDLE CreateNamedX11DisplayWindow( const char *title, int visible, int height, int width, int depth, int use_shared_memory)
{
	X_VIEW_HANDLE xhandle = PreCreateX11DisplayWindow(height, width, depth, CORX11_DATA_FORMAT_DEFAULT, use_shared_memory);

   if ( xhandle != NULL)
   {
		pthread_attr_t sched_attr;

      if (visible)
      {
         XMapWindow (xhandle->display, xhandle->window);
      }
      if (title != NULL)
      {
         XStoreName (xhandle->display, xhandle->window, title);
      }
      XSync (xhandle->display, FALSE);
      
		// Set up thread attributes so that the scheduler is explicitly set to
		// the SCHED_OTHER (default) scheduler. This is in case a high priority thread
		// calls this function - these display threads do not need to be high priority
		// or real time (SCHED_FIFO) since display is not a vital system function.
		pthread_attr_init(&sched_attr);
		pthread_attr_setinheritsched(&sched_attr, PTHREAD_EXPLICIT_SCHED);
		pthread_attr_setschedpolicy(&sched_attr, SCHED_OTHER);

      // Start the event handler and copy to display threads for this display.
      pthread_create(&xhandle->handler_thread_id, &sched_attr, eventHandler, xhandle); 
      pthread_create(&xhandle->display_thread_id, &sched_attr, displayHandler, xhandle); 
   }
   return xhandle;
}


X_VIEW_HANDLE CreateX11DisplayWindow( int height, int width, int depth, int use_shared_memory)
{
   return CreateNamedX11DisplayWindow("Live Video", TRUE, height, width, depth, use_shared_memory);
}

X_VIEW_HANDLE CreateDisplayWindow( const char *title, int visible, int height, int width, int depth, int sapera_format, int use_shared_memory)
{
   const char *default_title = "Live_Video";
   int data_format = Convert_SaperaFormat_To_X11( sapera_format);

   X_VIEW_HANDLE xhandle = PreCreateX11DisplayWindow(height, width, depth, data_format, use_shared_memory);

   if ( xhandle != NULL)
   {
      // Start the event handler and copy to display threads for this display.
      pthread_create(&xhandle->handler_thread_id, NULL, eventHandler, xhandle); 
      pthread_create(&xhandle->display_thread_id, NULL, displayHandler, xhandle); 

      if (visible)
      {
         XMapWindow (xhandle->display, xhandle->window);
      }
      if (title != NULL)
      {
         XStoreName (xhandle->display, xhandle->window, title);
      }
      else
      {
         XStoreName (xhandle->display, xhandle->window, default_title);
      }
      XSync (xhandle->display, FALSE);
   }
   return xhandle;   
}


int SetX11DisplayThreadAffinity(X_VIEW_HANDLE xhandle, int cpu_index)
{
	int ret = -1;
	if ((cpu_index >= 0) && (xhandle != NULL))
	{
		if (xhandle->display_thread_id != 0)
		{
			cpu_set_t cpuset;
			CPU_ZERO(&cpuset);
			CPU_SET(cpu_index, &cpuset);
			ret = pthread_setaffinity_np( xhandle->display_thread_id, sizeof(cpu_set_t), &cpuset);
			if (ret == 0)
			{
				xhandle->display_thread_affinity = cpu_index;
			}
			//ret = sched_setaffinity( xhandle->display_thread_id, sizeof(cpu_set_t), &cpuset);
		} 
	}
	return ret;
}

// Convert a Sapera format to one understood by these X11 display functions.
int Convert_SaperaFormat_To_X11( int SaperaDataFormat)
{
   int format = CORX11_DATA_FORMAT_DEFAULT;
   
   if ( (SaperaDataFormat & CORDATA_FORMAT_COLOR) != 0 )
   {
      switch (SaperaDataFormat)
      {
         case CORDATA_FORMAT_RGB5551:
            format = CORX11_DATA_FORMAT_RGB5551;
            break; 
         case CORDATA_FORMAT_RGB565:
            format = CORX11_DATA_FORMAT_RGB565;
            break; 
         case CORDATA_FORMAT_RGB888:
            format = CORX11_DATA_FORMAT_RGB888;
            break; 
         case CORDATA_FORMAT_RGB8888:
            format = CORX11_DATA_FORMAT_RGB8888;
            break; 
         case CORDATA_FORMAT_RGB101010:
            format = CORX11_DATA_FORMAT_RGB101010;
            break; 
         case CORDATA_FORMAT_YUYV:
            format = CORX11_DATA_FORMAT_RGB888;
            break; 
         case  CORDATA_FORMAT_Y411:
            format = CORX11_DATA_FORMAT_MONO;
            break; 
         case CORDATA_FORMAT_YUY2:
            format = CORX11_DATA_FORMAT_RGB5551;
            break; 
         default:
            format = CORX11_DATA_FORMAT_DEFAULT;
            break;
      }
   }
   else
   {
      // Its monochrome.
      format = CORX11_DATA_FORMAT_MONO;
   }
   return format;
}


