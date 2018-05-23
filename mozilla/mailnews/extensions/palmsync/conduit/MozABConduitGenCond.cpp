/*****************************************************************************
 *
 * EXPORTED FUNCTIONS Generic Conduit Entry Points Source File
 *
 * THIS FILE IS GENERATED BY PALM CDK
 *
 ****************************************************************************/
#define STRICT 1
#define ASSERT(f)          ((void)0)
#define TRACE0(sz)
#define TRACE(sz)

#include <malloc.h>


#include <CPalmRec.cpp>
#include <CPString.cpp>
#include <CPCatgry.cpp>

#include <windows.h>
#include <string.h>
#include <stdio.h>
#ifdef METROWERKS_WIN
#include <wmem.h>
#else
#include <memory.h>
#endif
#include <sys/stat.h>
#include <TCHAR.H>
#include <COMMCTRL.H>

#include <syncmgr.h>
#include "MozABConduitGenCond.h"

#include <logstrng.h>

// TODO - Include custom sync header

#include <PALM_CMN.H>
#define CONDUIT_NAME "MozABConduit"
#include "MozABConduitSync.h"

HANDLE hLangInstance;
HANDLE hAppInstance;
extern HANDLE hLangInstance;
extern HANDLE hAppInstance;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
//
//     Function:        DllMain()
//
//     Description:    main entry point to the MozABConduitProto component
//
//     Parameters:    hInstance - instance handle of the DLL
//                    dwReason  - why the entry point was called
//                    lpReserved - reserved
//
//     Returns:        1 if okay
//
/////////////////////////////////////////////////////////////////////////////
#ifndef _68K_

static int iTerminationCount = 0;

DWORD tId = 0;

extern "C" int APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{

    if (dwReason == DLL_PROCESS_ATTACH)
    {
        TRACE0("EXPORTED FUNCTIONS Initializing!\n");
        
        if (!iTerminationCount ) {
            hAppInstance = hInstance;
			// use PalmLoadLanguage here to load different languages
            hLangInstance = hInstance;
		        }
        ++iTerminationCount;

		tId = TlsAlloc();
		if (tId == 0xFFFFFFFF)
		  return FALSE;
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        TRACE0("EXPORTED FUNCTIONS Terminating!\n");

        --iTerminationCount;
        if (!iTerminationCount ) {
			// use PalmFreeLanguage here to unload different languages
        }
		TlsFree(tId);
    }
    return 1;   // ok
}
#endif


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
//
//     Function:        OpenConduit()
//
//     Description:  Extern "C" entry point into this conduit which starts the
//                 process of synchronizing the local database table with
//                 a remote conterpart residing on the remote view device. 
//
//     Parameters:   Pointer to a callback function used to report progress.
//                    
//                
//
//     Returns:        
//
/////////////////////////////////////////////////////////////////////////////
ExportFunc long OpenConduit(PROGRESSFN pFn, CSyncProperties& rProps)
{
    long retval = -1;
    if (pFn)
    {
	CMozABConduitSync * pABSync = new CMozABConduitSync(rProps);
        if (pABSync){
            retval = pABSync->Perform();
            delete pABSync;
        }
    }
    return(retval);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
//
//       Function:              GetConduitName()
//
//       Description:  Extern "C" entry point into this conduit which returns
//                                 the name to be used when display messages regarding
//                                 this conduit.
//
//       Parameters:   pszName - buffer in which to place the name
//                                 nLen - maximum number of bytes of buffer     
//                                      
//                              
//
//       Returns:          -1 indicates erros
//
/////////////////////////////////////////////////////////////////////////////
ExportFunc long GetConduitName(char* pszName,WORD nLen)
{
    pszName = CONDUIT_NAME;    

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
//
//       Function:     GetConduitVersion()
//
//       Description:  Extern "C" entry point into this conduit which returns
//                     the conduits version
//
//       Parameters:   none
//
//       Returns:      DWORD indicating major and minor version number
//                        HIWORD - reserved
//                        HIBYTE(LOWORD) - major number
//                        LOBYTE(LOWORD) - minor number
//
/////////////////////////////////////////////////////////////////////////////
ExportFunc DWORD GetConduitVersion()
{
    return GENERIC_CONDUIT_VERSION;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
//
//       Function:     ConfigureConduit
//
//       Description:  Extern "C" entry point into this conduit which returns
//                     should display the UI necessary to configure this 
//                       conduit.
//
//       Parameters:   none
//
//       Returns:      0 - success, !0 - failure
//
/////////////////////////////////////////////////////////////////////////////
ExportFunc long ConfigureConduit(CSyncPreference& pref)
{

    long nRtn = -1;
    return nRtn;
}

/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////
//
//	 Method:		GetConduitInfo
//
//	 Description:	This function provides a way for a Conduit to provide info
//                  to the caller. 
//                  In this version of the call, MFC Version, Conduit Name, and
//                  Default sync action are the types of information this call
//                  will return.
//
//	 Parameters:	ConduitInfoEnum infoType - enum specifying what info is being
//                          requested.
//                  void *pInArgs - This parameter may be null, except for the Conduit
//                          name enum, this value will be a ConduitRequestInfoType structure.
//                  This following to parameters vary depending upon the info being requested. 
//                  For enum eConduitName
//                  void *pOut - will be a pointer to a character buffer
//                  DWORD *pdwOutSize - will be a pointer to a DWORD specifying the size of the character buffer.
//
//                  For enum eMfcVersion
//                  void *pOut - will be a pointer to a DWORD
//                  DWORD *pdwOutSize - will be a pointer to a DWORD specifying the size of pOut.
//
//                  For enum eDefaultAction
//                  void *pOut - will be a pointer to a eSyncType variable
//                  DWORD *pdwOutSize - will be a pointer to a DWORD specifying the size of pOut.
//
//	 Returns:		0		- Success.
//					!0      - error code.
//
/////////////////////////////////////////////////////////////////////////////
ExportFunc long GetConduitInfo(ConduitInfoEnum infoType, void *pInArgs, void *pOut, DWORD *pdwOutSize)
{
    if (!pOut)
        return CONDERR_INVALID_PTR;
    if (!pdwOutSize)
        return CONDERR_INVALID_OUTSIZE_PTR;

    switch (infoType) {
        case eConduitName:

            // This code is for example. This conduit does not use this code
            
            if (!pInArgs)
                return CONDERR_INVALID_INARGS_PTR;
            ConduitRequestInfoType *pInfo;
            pInfo = (ConduitRequestInfoType *)pInArgs;
            if ((pInfo->dwVersion != CONDUITREQUESTINFO_VERSION_1) ||
                (pInfo->dwSize != SZ_CONDUITREQUESTINFO))
                return CONDERR_INVALID_INARGS_STRUCT;
           
	        pOut = CONDUIT_NAME;
                return CONDERR_CONDUIT_RESOURCE_FAILURE;
            break;
        case eDefaultAction:
            if (*pdwOutSize != sizeof(eSyncTypes))
                return CONDERR_INVALID_BUFFER_SIZE;
            (*(eSyncTypes*)pOut) = eFast;
            break;
        case eMfcVersion:
            if (*pdwOutSize != sizeof(DWORD))
                return CONDERR_INVALID_BUFFER_SIZE;
            (*(DWORD*)pOut) = MFC_NOT_USED;
            break;
        default:
            return CONDERR_UNSUPPORTED_CONDUITINFO_ENUM;
    }
    return 0;
}


