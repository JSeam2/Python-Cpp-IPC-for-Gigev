/*
  ---------------------------------------------
  Sapera Examples Utility Functions
  -----------------------------------------------
*/

#include "stdio.h"
#include "corapi.h"
#include "SapExUtil.h"
#include "SapX11Util.h"
#include "signal.h"

static int s_Signalled_Exit = 0;
static int s_numTransferHandles = 0;
static PCORHANDLE s_TransferHandles = NULL;

void SaperaAbnormalTerminationHandler(int sig)
{
	struct sigaction sa0;
	struct sigaction sa1;
   int i;

	if ( (s_numTransferHandles > 0) && (s_TransferHandles != NULL) && (s_Signalled_Exit == 0))
	{
		s_Signalled_Exit += 1;

		// The CorXferAbort function needs the message threads in order to do the abort
		// through the API. So, uninstall the signal handler here to prevent the threads
		// from aborting before they can perform an orderly shutdown.
		// Mask the signal.
		sa1.sa_handler = SIG_IGN;
		sigaction(sig, &sa1, &sa0);

		// Kill all the dma transfers that might be active - all the memory is about to disappear.
      if (s_TransferHandles != NULL)
      {
         for (i = 0; i < s_numTransferHandles; i++)
         {
				CorXferAbort(s_TransferHandles[i]);
         }
      }
	}

	// Restore the default signal handling in order to kill all the remaining threads
	sa1.sa_handler = SIG_DFL;
	sigaction(sig, &sa1, NULL);
	raise(sig);
}

static void SaperaFreeSavedTerminationInfo(void)
{
   if (s_TransferHandles != NULL)
   {
      free(s_TransferHandles);
      s_TransferHandles = NULL;
      s_numTransferHandles = 0;
   }
}

int  SaperaRegisterTerminationHandler( int numXfer, PCORHANDLE xferHandles)
{
	struct sigaction sa = {0};
   int i;
   int status = -1;
	int sigs[] = { SIGHUP, SIGINT, SIGTERM, SIGABRT, SIGPIPE, SIGXCPU };

   if ( (numXfer > 0) && xferHandles != NULL)
   {
      // Ignore the signals while we set up the transfer handles.
      for (i = 0; i < (sizeof(sigs)/sizeof(int)); i++)
      {
         sigaction(sigs[i], NULL, &sa );
         sa.sa_handler = SIG_IGN;
         sigaction(sigs[i], &sa, NULL);
      }

      // Set up the global array of transfer handles.
      // (Overwrite any existing information).
      SaperaFreeSavedTerminationInfo();

      s_TransferHandles = (PCORHANDLE)malloc( numXfer * sizeof(PCORHANDLE) );
      if (s_TransferHandles == NULL)
      {
      	return status;
      }
      s_numTransferHandles = numXfer;

      for (i = 0; i < numXfer; i++)
      {
      	s_TransferHandles[i] = xferHandles[i];
      }

      // Register the cancellation handler.
      for (i = 0; i < (sizeof(sigs)/sizeof(int)); i++)
      {
         sigaction(sigs[i], NULL, &sa );
         sa.sa_handler = SaperaAbnormalTerminationHandler;
         sigaction(sigs[i], &sa, NULL);
      }
      status = 0;
   }
   return status;
}

void  SaperaUnregisterTerminationHandler( void )
{
	struct sigaction sa = {0};
   int i;
	int sigs[] = { SIGHUP, SIGINT, SIGTERM, SIGABRT, SIGPIPE, SIGXCPU };

	// Restore the default action for the signals.
	for (i = 0; i < (sizeof(sigs)/sizeof(int)); i++)
	{
		sigaction(sigs[i], NULL, &sa );
		sa.sa_handler = SIG_DFL;
		sigaction(sigs[i], &sa, NULL);
	}

	SaperaFreeSavedTerminationInfo();
}



CORSTATUS DisplayStatus(char *functionName, CORSTATUS status)
{
	if (status != CORSTATUS_OK)
	{
   	char szId[128], szInfo[128], szLevel[64], szModule[64];
		CorManGetStatusTextEx(status, szId, sizeof(szId), szInfo, sizeof(szInfo), szLevel, sizeof(szLevel), szModule, sizeof(szModule));
		printf("%s in \"%s\" <%s module> => %s (%s)\n", szLevel, functionName, szModule, szId, szInfo);
		return status;
	}

	return status;
}

BOOL AddSaperaPath(char *filename, int size)
{
#if COR_LINUX
	char path[1024] = "";
	char *sapdir = NULL;
	sapdir = getenv("SAPERADIR");
	if (sapdir == NULL)
		return FALSE;

	strcat(path, sapdir);
	strcat(path, "/");
	strcat(path, filename);
	
#else
	char path[MAX_PATH] = "";

	// Get Sapera installation directory
	if (!GetEnvironmentVariable("SAPERADIR", path, sizeof(path)))
      return FALSE;

   // Add filename to path
   strcat(path, "\\");
   strcat(path, filename);
#endif

   // Copy back to user string
   strncpy(filename, path, size);
   return TRUE;
}


