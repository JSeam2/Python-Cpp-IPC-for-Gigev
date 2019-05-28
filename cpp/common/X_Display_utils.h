
#ifndef __X_DISPLAY_UTILS_H__
#define __X_DISPLAY_UTILS_H__

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>

typedef struct VIDEO_RECT_t
{
	int 	left;			// Start pixel for roi
	int 	top;			// Start line for roi
	int	height;		// Height of roi
	int 	width;		// Width of roi
	int	format;		// Format the data is in.
	int 	pixel_size;	// # of bytes per pixel
	int	stride;		// # of bytes to same pixel on next line.
	void	*data;		// Data for roi.
} VIDEO_RECT, *PVIDEO_RECT;

typedef struct X_VIEW_OBJECT_t
{
   int               version;
   Display           *display;
   Window            window;
   GC                context;
   XImage            *ximage;
   int               useshm;
   int               shmInProgress;
   int               shmEventCompleteType;
   XShmSegmentInfo   shminfo;
   long	             lut[256];
   pthread_t         handler_thread_id;
   pthread_t         display_thread_id;
   pthread_mutex_t   display_update_mutex;
   int               display_thread_affinity;
   //sem_t             display_semaphore;
   pthread_cond_t    display_signal_cv;
   pthread_mutex_t   display_signal_mutex;
   int					display_signal_flag;
   int               valid;
   volatile int      destroyInProgress;
   Atom              atomDelete;
   Atom              atomHandshake;
   int 					handshake_msg_count;
   int               format;
   int               screen_width;
   int               screen_height;
   int               display_width;
   int               display_height;
   int               x_origin;
   int               y_origin;
   int               img_width;
   int               img_height;
   int               img_depth;
   void *            img_ptr;
	int					use_screen_buffer;
	VIDEO_RECT			screenDst;
} X_VIEW_OBJECT, *PX_VIEW_OBJECT, *X_VIEW_HANDLE;

#ifdef __cplusplus
extern "C" {
#endif 


extern Status XShmQueryExtension (Display *display);

#define DestroyDisplayWindow DestroyX11DisplayWindow
void DestroyX11DisplayWindow( X_VIEW_HANDLE xhandle);
int Display_Image( X_VIEW_HANDLE xhandle, int depth, int width, int height, 
			void *image);

int CopyDataToX11Image( X_VIEW_HANDLE xhandle, int depth, 
			int width, int height, int src_left, int src_top, 
			int src_width, int src_height, void *src_image);

X_VIEW_HANDLE CreateDisplayWindow( const char *title, int visible, 
			int height, int width, int depth, int sapera_format, int use_shared_memory);

X_VIEW_HANDLE CreateX11DisplayWindow( int height, int width, int depth, int use_shared_memory);
X_VIEW_HANDLE CreateNamedX11DisplayWindow( const char *title, int visible, 
			int height, int width, int depth, int use_shared_memory);

int SetX11DisplayThreadAffinity(X_VIEW_HANDLE xhandle, int cpu_index);

#if 0
// Some helpers for debugging
typedef struct 
{
	int called;
	int sent;
	int recvd;
	int spurious;
	int location;
} XDPYINFO, *PXDPYINFO;
int getX11displayinfo( PXDPYINFO info);
#endif

#ifdef __cplusplus
}
#endif 


#endif
