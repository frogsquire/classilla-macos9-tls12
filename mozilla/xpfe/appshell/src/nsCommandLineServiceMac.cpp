/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser   <sfraser@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Special stuff for the Macintosh implementation of command-line service.

#include "nsCommandLineServiceMac.h"

// Mozilla
#include "nsDebug.h"
#include "nsILocalFileMac.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsDebug.h"
#include "nsNetUtil.h"
#include "nsIAppShellService.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNetCID.h"
#include "nsIWebShellWindow.h"
#include "nsIWebShell.h"
#include "nsIDOMWindow.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"
#include "jsapi.h"
#include "nsReadableUtils.h"
#include "nsICloseAllWindows.h"

#include "nsAEEventHandling.h"

// NSPR
#include "prmem.h"
#include "plstr.h"
#include "prenv.h"
#ifdef XP_MAC
#include "pprio.h"  // PR_Init_Log
#endif

#include "nsAppShellCIDs.h"
static NS_DEFINE_IID(kAppShellServiceCID,   NS_APPSHELL_SERVICE_CID);

// the static instance
nsMacCommandLine nsMacCommandLine::sMacCommandLine;

/*
 * ReadLine --
 *
 * Read in a line of text, terminated by CR or LF, from inStream into buf.
 * The terminating CR or LF is not included.  The text in buf is terminated
 * by a null byte.
 * Returns the number of bytes in buf.  If EOF and zero bytes were read, returns -1.
 */

static PRInt32 ReadLine(FILE* inStream, char* buf, PRInt32 bufSize)
{
  PRInt32 charsRead = 0;
  int c;
  
  if (bufSize < 2)
    return -1;

  while (charsRead < (bufSize-1)) {
    c = getc(inStream);
    if (c == EOF || c == '\n' || c == '\r')
      break;
    buf[charsRead++] = c;
  }
  buf[charsRead] = '\0';
  
  return (c == EOF && !charsRead) ? -1 : charsRead; 
}

//----------------------------------------------------------------------------------------
nsMacCommandLine::nsMacCommandLine()
: mArgs(NULL)
, mArgsAllocated(0)
, mArgsUsed(0)
, mStartedUp(PR_FALSE)
//----------------------------------------------------------------------------------------
{
}


//----------------------------------------------------------------------------------------
nsMacCommandLine::~nsMacCommandLine()
//----------------------------------------------------------------------------------------
{
  ShutdownAEHandlerClasses();
  if (mArgs) {
    for (PRUint32 i = 0; i < mArgsUsed; i++)
      free(mArgs[i]);
    free(mArgs);
  }
}


//----------------------------------------------------------------------------------------
nsresult nsMacCommandLine::Initialize(int& argc, char**& argv)
//----------------------------------------------------------------------------------------
{
  mArgs = static_cast<char **>(malloc(kArgsGrowSize * sizeof(char *)));
  if (!mArgs)
    return NS_ERROR_FAILURE;
  mArgs[0] = nsnull;
  mArgsAllocated = kArgsGrowSize;
  mArgsUsed = 0;
  
#if defined(XP_MACOSX)
  // Here, we may actually get useful args.
  // Copy them first to mArgv.
  for (int arg = 0; arg < argc; arg++)
    AddToCommandLine(argv[arg]);
#else
  // init the args buffer with the program name
  AddToCommandLine("mozilla");
#endif

  // Set up AppleEvent handling.
  OSErr err = CreateAEHandlerClasses(false);
  if (err != noErr) return NS_ERROR_FAILURE;

  // Snarf all the odoc and pdoc apple-events.
  //
  // 1. If they are odoc for 'CMDL' documents, read them into the buffer ready for
  //    parsing (concatenating multiple files).
  //
  // 2. If they are any other kind of document, convert them into -url command-line
  //    parameters or -print parameters, with file URLs.

  EventRecord anEvent;
  for (short i = 1; i < 5; i++)
    ::WaitNextEvent(0, &anEvent, 0, nsnull);

  while (::EventAvail(highLevelEventMask, &anEvent))
  {
    ::WaitNextEvent(highLevelEventMask, &anEvent, 0, nsnull);
    if (anEvent.what == kHighLevelEvent)
    {
      // here we process startup odoc/pdoc events, which can 
      // add items to the command line.
      err = ::AEProcessAppleEvent(&anEvent);
    }
  }
#ifdef XP_MACOSX  
  if (GetCurrentKeyModifiers() & optionKey)
    AddToCommandLine("-p");
#endif
  // we've started up now
  mStartedUp = PR_TRUE;
  
  argc = mArgsUsed;
  argv = mArgs;
  
  return NS_OK;
}

/* sigh ... Cameron */
#ifdef XP_MAC
char *strdup(const char *str)
{
    char *copy = (char *) malloc(strlen(str)+1);
    if (copy)
        strcpy(copy, str);
    return copy;
}
#endif

//----------------------------------------------------------------------------------------
nsresult nsMacCommandLine::AddToCommandLine(const char* inArgText)
//----------------------------------------------------------------------------------------
{
  if (mArgsUsed >= mArgsAllocated) {
    // realloc does not free the given pointer if allocation fails.
    char **temp = static_cast<char **>(realloc(mArgs, (mArgsAllocated + kArgsGrowSize) * sizeof(char *)));
    if (!temp)
      return NS_ERROR_OUT_OF_MEMORY;
    mArgs = temp;
    mArgsAllocated += kArgsGrowSize;
  }
  char *temp2 = strdup(inArgText);
  if (!temp2)
    return NS_ERROR_OUT_OF_MEMORY;
  mArgs[mArgsUsed++] = temp2;
  return NS_OK;
}


//----------------------------------------------------------------------------------------
nsresult nsMacCommandLine::AddToCommandLine(const char* inOptionString, const FSSpec& inFileSpec)
//----------------------------------------------------------------------------------------
{
  // Convert the filespec to a URL
  FSSpec nonConstSpec = inFileSpec;
  nsCOMPtr<nsILocalFileMac> inFile;
  nsresult rv = NS_NewLocalFileWithFSSpec(&nonConstSpec, PR_TRUE, getter_AddRefs(inFile));
  if (NS_FAILED(rv))
    return rv;
  nsCAutoString specBuf;
  rv = NS_GetURLSpecFromFile(inFile, specBuf);
  if (NS_FAILED(rv))
    return rv;
  AddToCommandLine(inOptionString);  
  AddToCommandLine(specBuf.get());
  return NS_OK;
}

//----------------------------------------------------------------------------------------
nsresult nsMacCommandLine::AddToEnvironmentVars(const char* inArgText)
//----------------------------------------------------------------------------------------
{
  (void)PR_SetEnv(inArgText);
  return NS_OK;
}


//----------------------------------------------------------------------------------------
OSErr nsMacCommandLine::HandleOpenOneDoc(const FSSpec& inFileSpec, OSType inFileType)
//----------------------------------------------------------------------------------------
{
  nsCOMPtr<nsILocalFileMac> inFile;
  nsresult rv = NS_NewLocalFileWithFSSpec(&inFileSpec, PR_TRUE, getter_AddRefs(inFile));
  if (NS_FAILED(rv))
    return errAEEventNotHandled;

  if (!mStartedUp)
  {
    // Is it the right type to be a command-line file?
    if (inFileType == 'TEXT' || inFileType == 'CMDL')
    {
      // Can we open the file?
      FILE *fp = 0;
      rv = inFile->OpenANSIFileDesc("r", &fp);
      if (NS_SUCCEEDED(rv))
      {
        Boolean foundArgs = false;
        Boolean foundEnv = false;
        char chars[1024];
        static const char kCommandLinePrefix[] = "ARGS:";
        static const char kEnvVarLinePrefix[] = "ENV:";

        while (ReadLine(fp, chars, sizeof(chars)) != -1)
        {       // See if there are any command line or environment var settings
          if (PL_strstr(chars, kCommandLinePrefix) == chars)
          {
            (void)AddToCommandLine(chars + sizeof(kCommandLinePrefix) - 1);
            foundArgs = true;
          }
          else if (PL_strstr(chars, kEnvVarLinePrefix) == chars)
          {
            (void)AddToEnvironmentVars(chars + sizeof(kEnvVarLinePrefix) - 1);
            foundEnv = true;
          }
        }

        fclose(fp);

/* Cameron sez: a better way is just to build the app for debug and run it from the Finder
   with the usual files; it'll still start in MWerks */
#ifdef MWERKS_DEBUG_LOG

#define MODULOGOUS "NSPR_LOG_MODULES=nsComponentManager:5"
#define MODULOGFILE "NSPR_LOG_FILE=classilla_nspr.log"

		char *envbuf = (char *)malloc(256);
		sprintf(envbuf, MODULOGOUS);
		(void)PR_SetEnv(envbuf);
		sprintf(envbuf, MODULOGFILE);
		(void)PR_SetEnv(envbuf);
		foundEnv = true;
		PR_Init_Log();
#endif

#ifndef XP_MACOSX
        // If we found any environment vars we need to re-init NSPR's logging
        // so that it knows what the new vars are
        if (foundEnv)
          PR_Init_Log();
#endif
        // If we found a command line or environment vars we want to return now
        // raather than trying to open the file as a URL
        if (foundArgs || foundEnv)
          return noErr;
      }
    }
    // If it's not a command-line argument, and we are starting up the application,
    // add a command-line "-url" argument to the global list. This means that if
    // the app is opened with documents on the mac, they'll be handled the same
    // way as if they had been typed on the command line in Unix or DOS.
    return AddToCommandLine("-url", inFileSpec);
  }

  // Final case: we're not just starting up. How do we handle this?
  nsCAutoString specBuf;
  rv = NS_GetURLSpecFromFile(inFile, specBuf);
  if (NS_FAILED(rv))
    return errAEEventNotHandled;
  nsAutoString urlString;
  CopyASCIItoUCS2(specBuf, urlString);    
  rv = OpenWindow( "chrome://navigator/content", urlString.get() );
  if (NS_FAILED(rv))
    return errAEEventNotHandled;
  return noErr;
}



//----------------------------------------------------------------------------------------
OSErr nsMacCommandLine::HandlePrintOneDoc(const FSSpec& inFileSpec, OSType fileType)
//----------------------------------------------------------------------------------------
{
  // If  we are starting up the application,
  // add a command-line "-print" argument to the global list. This means that if
  // the app is opened with documents on the mac, they'll be handled the same
  // way as if they had been typed on the command line in Unix or DOS.
  if (!mStartedUp)
    return AddToCommandLine("-print", inFileSpec);
  
  // Final case: we're not just starting up. How do we handle this?
  NS_NOTYETIMPLEMENTED("Write Me");
  return errAEEventNotHandled;
}



//----------------------------------------------------------------------------------------
nsresult nsMacCommandLine::OpenWindow(const char *chrome, const PRUnichar *url)
//----------------------------------------------------------------------------------------
{
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  nsCOMPtr<nsISupportsString> urlWrapper(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  if (!wwatch || !urlWrapper)
    return NS_ERROR_FAILURE;

  urlWrapper->SetData(nsDependentString(url));

  nsCOMPtr<nsIDOMWindow> newWindow;
  nsresult rv;
  rv = wwatch->OpenWindow(0, chrome, "_blank",
               "chrome,dialog=no,all", urlWrapper,
               getter_AddRefs(newWindow));

  return rv;
}

//----------------------------------------------------------------------------------------
OSErr nsMacCommandLine::DispatchURLToNewBrowser(const char* url)
//----------------------------------------------------------------------------------------
{
  OSErr err = errAEEventNotHandled;
  if (mStartedUp)
  {
    nsresult rv;
    rv = OpenWindow("chrome://navigator/content", NS_ConvertASCIItoUCS2(url).get());
    if (NS_FAILED(rv))
      return err;
        err = noErr;  // we handled it
  }
  else {
    err = AddToCommandLine("-url");
    if (err == noErr)
      err = AddToCommandLine(url);
  }
  
  return err;
}

//----------------------------------------------------------------------------------------
OSErr nsMacCommandLine::Quit(TAskSave askSave)
//----------------------------------------------------------------------------------------
{
  nsresult rv;
  
  nsCOMPtr<nsICloseAllWindows> closer =
           do_CreateInstance("@mozilla.org/appshell/closeallwindows;1", &rv);
  if (NS_FAILED(rv))
    return errAEEventNotHandled;

    PRBool doQuit;
    rv = closer->CloseAll(askSave != eSaveNo, &doQuit);
    if (NS_FAILED(rv) || !doQuit)
        return errAEEventNotHandled;
          
  nsCOMPtr<nsIAppShellService> appShellService = 
           do_GetService(kAppShellServiceCID, &rv);
  if (NS_FAILED(rv))
    return errAEEventNotHandled;
  
  (void)appShellService->Quit(nsIAppShellService::eAttemptQuit);
  return noErr;
}


//========================================================================================
//      InitializeMacCommandLine
//      The only external entry point to this file.
//========================================================================================

#pragma mark -

//----------------------------------------------------------------------------------------
nsresult InitializeMacCommandLine(int& argc, char**& argv)
//----------------------------------------------------------------------------------------
{

  nsMacCommandLine&  cmdLine = nsMacCommandLine::GetMacCommandLine();
  return cmdLine.Initialize(argc, argv);
} // InitializeMac
