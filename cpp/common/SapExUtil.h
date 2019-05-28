/*
  ---------------------------------------------
  Status reporting functionality
  -----------------------------------------------
*/

#ifdef __cplusplus
extern "C" {
#endif

extern void  SaperaAbnormalTerminationHandler(int sig);
extern int   SaperaRegisterTerminationHandler( int numXfer, PCORHANDLE xferHandles);
extern void  SaperaUnregisterTerminationHandler( void );

extern CORSTATUS DisplayStatus(char *functionName, CORSTATUS status);
extern BOOL AddSaperaPath(char *filename, int size);

#ifdef __cplusplus
}
#endif 
