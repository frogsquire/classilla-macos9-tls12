/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 *   Gagan Saksena <gagan@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Adrian Havill <havill@redhat.com>
 *   Gervase Markham <gerv@gerv.net>
 *   Bradley Baetz <bbaetz@netscape.com>
 */

#include "nsHttp.h"
#include "nsHttpHandler.h"
#include "nsHttpChannel.h"
#include "nsHttpConnection.h"
#include "nsHttpResponseHead.h"
#include "nsHttpTransaction.h"
#include "nsHttpAuthCache.h"
#include "nsStandardURL.h"
#include "nsIHttpChannel.h"
#include "nsIHttpNotify.h"
#include "nsIURL.h"
#include "nsIStandardURL.h"
#include "nsICacheService.h"
#include "nsICategoryManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsIObserverService.h"
#include "nsINetModRegEntry.h"
#include "nsICacheService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranchInternal.h"
#include "nsIPrefLocalizedString.h"
#include "nsISocketProviderService.h"
#include "nsISocketProvider.h"
#include "nsPrintfCString.h"
#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "nsAutoLock.h"
#include "prprf.h"
#include "nsReadableUtils.h"

#if defined(XP_UNIX) || defined(XP_BEOS)
#include <sys/utsname.h>
#endif

#if defined(XP_PC) && !defined(XP_OS2)
#include <windows.h>
#endif

#if defined(XP_MAC)
#include <Gestalt.h>
#endif

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *gSocketThread;
#endif

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kNetModuleMgrCID, NS_NETMODULEMGR_CID);
static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kCacheServiceCID, NS_CACHESERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kSocketProviderServiceCID, NS_SOCKETPROVIDERSERVICE_CID);

#define UA_PREF_PREFIX          "general.useragent."
#define UA_APPNAME              "Mozilla"
#define UA_APPVERSION           "5.0"
#define UA_APPSECURITY_FALLBACK "N"

#define HTTP_PREF_PREFIX        "network.http."
#define INTL_ACCEPT_LANGUAGES   "intl.accept_languages"
#define INTL_ACCEPT_CHARSET     "intl.charset.default"
#define NETWORK_ENABLEIDN       "network.enableIDN"

#define UA_PREF(_pref) UA_PREF_PREFIX _pref
#define HTTP_PREF(_pref) HTTP_PREF_PREFIX _pref

//-----------------------------------------------------------------------------

static nsresult
NewURI(const nsACString &aSpec,
       const char *aCharset,
       nsIURI *aBaseURI,
       PRInt32 aDefaultPort,
       nsIURI **aURI)
{
    nsStandardURL *url = new nsStandardURL();
    if (!url)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(url);

    nsresult rv = url->Init(nsIStandardURL::URLTYPE_AUTHORITY,
                            aDefaultPort, aSpec, aCharset, aBaseURI);
    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }

    *aURI = url; // no QI needed
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler <public>
//-----------------------------------------------------------------------------

nsHttpHandler *gHttpHandler = nsnull;

nsHttpHandler::nsHttpHandler()
    : mConnMgr(nsnull)
    , mHttpVersion(NS_HTTP_VERSION_1_1)
    , mProxyHttpVersion(NS_HTTP_VERSION_1_1)
    , mCapabilities(NS_HTTP_ALLOW_KEEPALIVE)
    , mProxyCapabilities(NS_HTTP_ALLOW_KEEPALIVE)
    , mReferrerLevel(0xff) // by default we always send a referrer
    , mIdleTimeout(10)
    , mMaxRequestAttempts(10)
    , mMaxRequestDelay(10)
    , mMaxConnections(24)
    , mMaxConnectionsPerServer(8)
    , mMaxPersistentConnectionsPerServer(2)
    , mMaxPersistentConnectionsPerProxy(4)
    , mMaxPipelinedRequests(2)
    , mRedirectionLimit(10)
    , mLastUniqueID(NowInSeconds())
    , mSessionStartTime(0)
    , mUserAgentIsDirty(PR_TRUE)
    , mUseCache(PR_TRUE)
    , mSendSecureXSiteReferrer(PR_TRUE)
{
#if defined(PR_LOGGING)
    gHttpLog = PR_NewLogModule("nsHttp");
#endif

    LOG(("Creating nsHttpHandler [this=%x].\n", this));

    NS_ASSERTION(!gHttpHandler, "HTTP handler already created!");
    gHttpHandler = this;
}

nsHttpHandler::~nsHttpHandler()
{
    // We do not deal with the timer cancellation in the destructor since
    // it is taken care of in xpcom shutdown event in the Observe method.

    LOG(("Deleting nsHttpHandler [this=%x]\n", this));

    // make sure the connection manager is shutdown
    if (mConnMgr) {
        mConnMgr->Shutdown();
        NS_RELEASE(mConnMgr);
    }

    nsHttp::DestroyAtomTable();

    gHttpHandler = nsnull;
}

nsresult
nsHttpHandler::Init()
{
    nsresult rv = NS_OK;

    LOG(("nsHttpHandler::Init\n"));

    mIOService = do_GetService(kIOServiceCID, &rv);
    if (NS_FAILED(rv)) {
        NS_WARNING("unable to continue without io service");
        return rv;
    }

    InitUserAgentComponents();

    // monitor some preference changes
    nsCOMPtr<nsIPrefBranch> prefBranch;
    GetPrefBranch(getter_AddRefs(prefBranch));
    if (prefBranch) {
        nsCOMPtr<nsIPrefBranchInternal> pbi = do_QueryInterface(prefBranch);
        if (pbi) {
            pbi->AddObserver(HTTP_PREF_PREFIX, this, PR_TRUE);
            pbi->AddObserver(UA_PREF_PREFIX, this, PR_TRUE);
            pbi->AddObserver(INTL_ACCEPT_LANGUAGES, this, PR_TRUE); 
            pbi->AddObserver(INTL_ACCEPT_CHARSET, this, PR_TRUE);
            pbi->AddObserver(NETWORK_ENABLEIDN, this, PR_TRUE);
        }
        PrefsChanged(prefBranch, nsnull);
    }

#if DEBUG
    // dump user agent prefs
    LOG(("> app-name = %s\n", mAppName.get()));
    LOG(("> app-version = %s\n", mAppVersion.get()));
    LOG(("> platform = %s\n", mPlatform.get()));
    LOG(("> oscpu = %s\n", mOscpu.get()));
    LOG(("> security = %s\n", mSecurity.get()));
    LOG(("> language = %s\n", mLanguage.get()));
    LOG(("> misc = %s\n", mMisc.get()));
    LOG(("> vendor = %s\n", mVendor.get()));
    LOG(("> vendor-sub = %s\n", mVendorSub.get()));
    LOG(("> vendor-comment = %s\n", mVendorComment.get()));
    LOG(("> product = %s\n", mProduct.get()));
    LOG(("> product-sub = %s\n", mProductSub.get()));
    LOG(("> product-comment = %s\n", mProductComment.get()));
    LOG(("> user-agent = %s\n", UserAgent().get()));
#endif

    mSessionStartTime = NowInSeconds();

    rv = mAuthCache.Init();
    if (NS_FAILED(rv)) return rv;

    rv = InitConnectionMgr();
    if (NS_FAILED(rv)) return rv;

    // Startup the http category
    // Bring alive the objects in the http-protocol-startup category
    NS_CreateServicesFromCategory(NS_HTTP_STARTUP_CATEGORY,
                                  NS_STATIC_CAST(nsISupports*,NS_STATIC_CAST(void*,this)),
                                  NS_HTTP_STARTUP_TOPIC);    
    
    nsCOMPtr<nsIObserverService> observerSvc =
        do_GetService("@mozilla.org/observer-service;1", &rv);
    if (observerSvc) {
        observerSvc->AddObserver(this, "profile-change-net-teardown", PR_TRUE);
        observerSvc->AddObserver(this, "profile-change-net-restore", PR_TRUE);
        observerSvc->AddObserver(this, "session-logout", PR_TRUE);
        observerSvc->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_TRUE);
    }
 
    StartPruneDeadConnectionsTimer();
    return NS_OK;
}

nsresult
nsHttpHandler::InitConnectionMgr()
{
    nsresult rv;

    if (!mConnMgr) {
        mConnMgr = new nsHttpConnectionMgr();
        if (!mConnMgr)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(mConnMgr);
    }

    rv = mConnMgr->Init(mMaxConnections,
                        mMaxConnectionsPerServer,
                        mMaxConnectionsPerServer,
                        mMaxPersistentConnectionsPerServer,
                        mMaxPersistentConnectionsPerProxy,
                        mMaxRequestDelay,
                        mMaxPipelinedRequests);
    return rv;
}

void
nsHttpHandler::StartPruneDeadConnectionsTimer()
{
    LOG(("nsHttpHandler::StartPruneDeadConnectionsTimer\n"));

    mTimer = do_CreateInstance("@mozilla.org/timer;1");
    NS_ASSERTION(mTimer, "no timer");
    // failure to create a timer is not a fatal error, but idle connections
    // will not be cleaned up until we try to use them.
    if (mTimer)
        mTimer->Init(this, 15*1000, // every 15 seconds
                     nsITimer::TYPE_REPEATING_SLACK);
}

void
nsHttpHandler::StopPruneDeadConnectionsTimer()
{
    LOG(("nsHttpHandler::StopPruneDeadConnectionsTimer\n"));

    if (mTimer) {
        mTimer->Cancel();
        mTimer = 0;
    }
}

nsresult
nsHttpHandler::AddStandardRequestHeaders(nsHttpHeaderArray *request,
                                         PRUint8 caps,
                                         PRBool useProxy)
{
    nsresult rv;

    LOG(("nsHttpHandler::AddStandardRequestHeaders\n"));

    // Add the "User-Agent" header
    rv = request->SetHeader(nsHttp::User_Agent, UserAgent());
    if (NS_FAILED(rv)) return rv;

    // MIME based content negotiation lives!
    // Add the "Accept" header
    rv = request->SetHeader(nsHttp::Accept, mAccept);
    if (NS_FAILED(rv)) return rv;

    // Add the "Accept-Language" header
    if (!mAcceptLanguages.IsEmpty()) {
        // Add the "Accept-Language" header
        rv = request->SetHeader(nsHttp::Accept_Language, mAcceptLanguages);
        if (NS_FAILED(rv)) return rv;
    }

    // Add the "Accept-Encoding" header
    rv = request->SetHeader(nsHttp::Accept_Encoding, mAcceptEncodings);
    if (NS_FAILED(rv)) return rv;

    // Add the "Accept-Charset" header
    rv = request->SetHeader(nsHttp::Accept_Charset, mAcceptCharsets);
    if (NS_FAILED(rv)) return rv;

    // RFC2616 section 19.6.2 states that the "Connection: keep-alive"
    // and "Keep-alive" request headers should not be sent by HTTP/1.1
    // user-agents.  Otherwise, problems with proxy servers (especially
    // transparent proxies) can result.
    //
    // However, we need to send something so that we can use keepalive
    // with HTTP/1.0 servers/proxies. We use "Proxy-Connection:" when 
    // we're talking to an http proxy, and "Connection:" otherwise
    
    NS_NAMED_LITERAL_CSTRING(close, "close");
    NS_NAMED_LITERAL_CSTRING(keepAlive, "keep-alive");

    const nsACString *connectionType = &close;
    if (caps & NS_HTTP_ALLOW_KEEPALIVE) {
        rv = request->SetHeader(nsHttp::Keep_Alive, nsPrintfCString("%u", mIdleTimeout));
        if (NS_FAILED(rv)) return rv;
        connectionType = &keepAlive;
    } else if (useProxy) {
        // Bug 92006
        request->SetHeader(nsHttp::Connection, close);
    }

    const nsHttpAtom &header = useProxy ? nsHttp::Proxy_Connection
                                        : nsHttp::Connection;
    return request->SetHeader(header, *connectionType);
}

PRBool
nsHttpHandler::IsAcceptableEncoding(const char *enc)
{
    if (!enc)
        return PR_FALSE;

    // HTTP 1.1 allows servers to send x-gzip and x-compress instead
    // of gzip and compress, for example.  So, we'll always strip off
    // an "x-" prefix before matching the encoding to one we claim
    // to accept.
    if (!PL_strncasecmp(enc, "x-", 2))
        enc += 2;
    
    return PL_strcasestr(mAcceptEncodings.get(), enc) != nsnull;
}

nsresult
nsHttpHandler::GetCacheSession(nsCacheStoragePolicy storagePolicy,
                               nsICacheSession **result)
{
    nsresult rv;

    // Skip cache if disabled in preferences
    if (!mUseCache)
        return NS_ERROR_NOT_AVAILABLE;

    if (!mCacheSession_ANY) {
        nsCOMPtr<nsICacheService> serv = do_GetService(kCacheServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = serv->CreateSession("HTTP",
                                 nsICache::STORE_ANYWHERE,
                                 nsICache::STREAM_BASED,
                                 getter_AddRefs(mCacheSession_ANY));
        if (NS_FAILED(rv)) return rv;

        rv = mCacheSession_ANY->SetDoomEntriesIfExpired(PR_FALSE);
        if (NS_FAILED(rv)) return rv;

        rv = serv->CreateSession("HTTP-memory-only",
                                 nsICache::STORE_IN_MEMORY,
                                 nsICache::STREAM_BASED,
                                 getter_AddRefs(mCacheSession_MEM));
        if (NS_FAILED(rv)) return rv;

        rv = mCacheSession_MEM->SetDoomEntriesIfExpired(PR_FALSE);
        if (NS_FAILED(rv)) return rv;
    }

    if (storagePolicy == nsICache::STORE_IN_MEMORY)
        NS_ADDREF(*result = mCacheSession_MEM);
    else
        NS_ADDREF(*result = mCacheSession_ANY);

    return NS_OK;
}

nsresult
nsHttpHandler::GetProxyObjectManager(nsIProxyObjectManager **result)
{
    if (!mProxyMgr) {
        nsresult rv;
        mProxyMgr = do_GetService(NS_XPCOMPROXY_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;
    }
    *result = mProxyMgr;
    NS_ADDREF(*result);
    return NS_OK;
}

nsresult
nsHttpHandler::GetEventQueueService(nsIEventQueueService **result)
{
    if (!mEventQueueService) {
        nsresult rv;
        mEventQueueService = do_GetService(kEventQueueServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;
    }
    *result = mEventQueueService;
    NS_ADDREF(*result);
    return NS_OK;
}

nsresult
nsHttpHandler::GetStreamConverterService(nsIStreamConverterService **result)
{
    if (!mStreamConvSvc) {
        nsresult rv;
        mStreamConvSvc = do_GetService(kStreamConverterServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;
    }
    *result = mStreamConvSvc;
    NS_ADDREF(*result);
    return NS_OK;
}

nsresult
nsHttpHandler::GetMimeService(nsIMIMEService **result)
{
    if (!mMimeService) {
        nsresult rv;
        mMimeService = do_GetService("@mozilla.org/mime;1", &rv);
        if (NS_FAILED(rv)) return rv;
    }
    *result = mMimeService;
    NS_ADDREF(*result);
    return NS_OK;
}

nsresult 
nsHttpHandler::GetIOService(nsIIOService** result)
{
    NS_ADDREF(*result = mIOService);
    return NS_OK;
}


nsresult
nsHttpHandler::OnModifyRequest(nsIHttpChannel *chan)
{
    nsresult rv;

    LOG(("nsHttpHandler::OnModifyRequest [chan=%x]\n", chan));

    if (!mNetModuleMgr) {
        mNetModuleMgr = do_GetService(kNetModuleMgrCID, &rv);
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsISimpleEnumerator> modules;
    rv = mNetModuleMgr->EnumerateModules(
            NS_NETWORK_MODULE_MANAGER_HTTP_REQUEST_CONTRACTID,
            getter_AddRefs(modules));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISupports> sup;

    // notify each module...
    while (NS_SUCCEEDED(modules->GetNext(getter_AddRefs(sup)))) {
        nsCOMPtr<nsINetModRegEntry> entry = do_QueryInterface(sup, &rv);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsINetNotify> netNotify;
        rv = entry->GetSyncProxy(getter_AddRefs(netNotify));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIHttpNotify> httpNotify = do_QueryInterface(netNotify, &rv);
        if (NS_FAILED(rv)) return rv;

        // fire off the notification, ignore the return code.
        httpNotify->OnModifyRequest(chan);
    }

    // notify interested observers (in our case, Site Control) (Classilla issue 169)
	nsCOMPtr<nsIObserverService> os(do_GetService("@mozilla.org/observer-service;1", &rv));
  	if (os) {
  		// force the channel to become an nsIHttpChannel, or this won't work.
  		// This is going to look like an nsISupports to the other end, however, so
  		// the other end will need to QI the resulting object to what it wants.
  		nsCOMPtr<nsIHttpChannel> hchanptr(do_QueryInterface(chan, &rv));
  		if (NS_SUCCEEDED(rv))
    		rv = os->NotifyObservers(hchanptr, "http-on-modify-request", 0);
    	// we don't care if this fails.
  	}
    
    return NS_OK;
}

nsresult
nsHttpHandler::OnExamineResponse(nsIHttpChannel *chan)
{
    nsresult rv;

    LOG(("nsHttpHandler::OnExamineResponse [chan=%x]\n", chan));

    if (!mNetModuleMgr) {
        mNetModuleMgr = do_GetService(kNetModuleMgrCID, &rv);
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsISimpleEnumerator> modules;
    rv = mNetModuleMgr->EnumerateModules(
            NS_NETWORK_MODULE_MANAGER_HTTP_RESPONSE_CONTRACTID,
            getter_AddRefs(modules));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISupports> sup;
    nsCOMPtr<nsINetModRegEntry> entry;
    nsCOMPtr<nsINetNotify> netNotify;
    nsCOMPtr<nsIHttpNotify> httpNotify;

    // notify each module...
    while (NS_SUCCEEDED(modules->GetNext(getter_AddRefs(sup)))) {
        entry = do_QueryInterface(sup, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = entry->GetSyncProxy(getter_AddRefs(netNotify));
        if (NS_FAILED(rv)) return rv;

        httpNotify = do_QueryInterface(netNotify, &rv);
        if (NS_FAILED(rv)) return rv;

        // fire off the notification, ignore the return code.
        httpNotify->OnExamineResponse(chan);
    }

    // notify interested observers (in our case, the Byblos manager) (Classilla issue 170)
    // If this doesn't work, we get another chance in nsHttpChannel.
	nsCOMPtr<nsIObserverService> os(do_GetService("@mozilla.org/observer-service;1", &rv));
  	if (os) {
  		// force the channel to become an nsIHttpChannel, or this won't work.
  		// This is going to look like an nsISupports to the other end, however, so
  		// the other end will need to QI the resulting object to what it wants.
  		nsCOMPtr<nsIHttpChannel> hchanptr(do_QueryInterface(chan, &rv));
  		if (NS_SUCCEEDED(rv))
    		rv = os->NotifyObservers(hchanptr, "http-on-examine-response", 0);
    	// we don't care if this fails.
  	}

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler <private>
//-----------------------------------------------------------------------------

const nsAFlatCString &
nsHttpHandler::UserAgent()
{
    if (mUserAgentOverride && mUserAgentOverride.Length()) { // modified for Classilla UA support
        LOG(("using general.useragent.override : %s\n", mUserAgentOverride.get()));
        return mUserAgentOverride;
    }

    if (mUserAgentIsDirty) {
        BuildUserAgent();
        mUserAgentIsDirty = PR_FALSE;
    }

    return mUserAgent;
}

void
nsHttpHandler::BuildUserAgent()
{
    LOG(("nsHttpHandler::BuildUserAgent\n"));

    NS_ASSERTION(!mAppName.IsEmpty() &&
                 !mAppVersion.IsEmpty() &&
                 !mPlatform.IsEmpty() &&
                 !mSecurity.IsEmpty() &&
                 !mOscpu.IsEmpty(),
                 "HTTP cannot send practical requests without this much");

#if(0)
    // preallocate to worst-case size, which should always be better
    // than if we didn't preallocate at all.
    mUserAgent.SetCapacity(mAppName.Length() + 
                           mAppVersion.Length() + 
                           mPlatform.Length() + 
                           mSecurity.Length() +
                           mOscpu.Length() +
                           mLanguage.Length() +
                           mMisc.Length() +
                           mProduct.Length() +
                           mProductSub.Length() +
                           mProductComment.Length() +
                           mVendor.Length() +
                           mVendorSub.Length() +
                           mVendorComment.Length() +
                           22);

    // Application portion
    mUserAgent.Assign(mAppName);
    mUserAgent += '/';
    mUserAgent += mAppVersion;
    mUserAgent += ' ';

    // Application comment
    mUserAgent += '(';
    mUserAgent += mPlatform;
    mUserAgent += "; ";
    mUserAgent += mSecurity;
    mUserAgent += "; ";
    mUserAgent += mOscpu;
    if (!mLanguage.IsEmpty()) {
        mUserAgent += "; ";
        mUserAgent += mLanguage;
    }
    if (!mMisc.IsEmpty()) {
        mUserAgent += "; ";
        mUserAgent += mMisc;
    }
    mUserAgent += ')';

    // Product portion
    if (!mProduct.IsEmpty()) {
        mUserAgent += ' ';
        mUserAgent += mProduct;
        if (!mProductSub.IsEmpty()) {
            mUserAgent += '/';
            mUserAgent += mProductSub;
        }
        if (!mProductComment.IsEmpty()) {
            mUserAgent += " (";
            mUserAgent += mProductComment;
            mUserAgent += ')';
        }
    }

    // Vendor portion
    if (!mVendor.IsEmpty()) {
        mUserAgent += ' ';
        mUserAgent += mVendor;
        if (!mVendorSub.IsEmpty()) {
            mUserAgent += '/';
            mUserAgent += mVendorSub;
        }
        if (!mVendorComment.IsEmpty()) {
            mUserAgent += " (";
            mUserAgent += mVendorComment;
            mUserAgent += ')';
        }
    }
#else
	// Classilla issue 171
    mUserAgent.SetCapacity(
                           mProduct.Length() +
                           mProductSub.Length() +
                           160);
                           
    mUserAgent.Assign("NokiaN90-1/3.0545.5.1 Series60/2.8 Profile/MIDP-2.0 Configuration/CLDC-1.1 ");
    // Application comment. Send language and rv: only.
    mUserAgent += '(';
    if (!mLanguage.IsEmpty()) {
        mUserAgent += mLanguage;
    }
    if (!mMisc.IsEmpty()) {
        mUserAgent += "; ";
        mUserAgent += mMisc;
    }
    mUserAgent += ')';
    
    // Product portion
    if (!mProduct.IsEmpty()) {
        mUserAgent += ' ';
        mUserAgent += mProduct;
        if (!mProductSub.IsEmpty()) {
            mUserAgent += '/';
            mUserAgent += mProductSub;
        }
        if (!mProductComment.IsEmpty()) {
            mUserAgent += " (";
            mUserAgent += mProductComment;
            mUserAgent += ')';
        }
    }    
	
#endif
}

void
nsHttpHandler::InitUserAgentComponents()
{

    // Gather Application name and Version.
    mAppName.Adopt(nsCRT::strdup(UA_APPNAME));
    mAppVersion.Adopt(nsCRT::strdup(UA_APPVERSION));

      // Gather platform.
    mPlatform.Adopt(nsCRT::strdup(
#if defined(MOZ_WIDGET_PHOTON)
    "Photon"
#elif defined(XP_OS2)
    "OS/2"
#elif defined(XP_WIN)
    "Windows"
#elif defined(XP_MAC) || defined(XP_MACOSX)
    "Macintosh"
#elif defined(XP_BEOS)
    "BeOS"
#elif defined(NO_X11)
    "?"
#else
    "X11"
#endif
    ));

    // Gather OS/CPU.
#if defined(XP_OS2)
    ULONG os2ver = 0;
    DosQuerySysInfo(QSV_VERSION_MINOR, QSV_VERSION_MINOR,
                    &os2ver, sizeof(os2ver));
    if (os2ver == 11)
        mOscpu.Adopt(nsCRT::strdup("2.11"));
    else if (os2ver == 30)
        mOscpu.Adopt(nsCRT::strdup("Warp 3"));
    else if (os2ver == 40)
        mOscpu.Adopt(nsCRT::strdup("Warp 4"));
    else if (os2ver == 45)
        mOscpu.Adopt(nsCRT::strdup("Warp 4.5"));

#elif defined(XP_PC)
    OSVERSIONINFO info = { sizeof OSVERSIONINFO };
    if (GetVersionEx(&info)) {
        if (info.dwPlatformId == VER_PLATFORM_WIN32_NT) {
            if (info.dwMajorVersion      == 3)
                mOscpu.Adopt(nsCRT::strdup("WinNT3.51"));
            else if (info.dwMajorVersion == 4)
                mOscpu.Adopt(nsCRT::strdup("WinNT4.0"));
            else {
                char *buf = PR_smprintf("Windows NT %ld.%ld",
                                        info.dwMajorVersion,
                                        info.dwMinorVersion);
                if (buf) {
                    mOscpu.Adopt(nsCRT::strdup(buf));
                    PR_smprintf_free(buf);
                }
            }
        } else if (info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS &&
                   info.dwMajorVersion == 4) {
            if (info.dwMinorVersion == 90)
                mOscpu.Adopt(nsCRT::strdup("Win 9x 4.90"));  // Windows Me
            else if (info.dwMinorVersion > 0)
                mOscpu.Adopt(nsCRT::strdup("Win98"));
            else
                mOscpu.Adopt(nsCRT::strdup("Win95"));
        } else {
            char *buf = PR_smprintf("Windows %ld.%ld",
                                    info.dwMajorVersion,
                                    info.dwMinorVersion);
            if (buf) {
                mOscpu.Adopt(nsCRT::strdup(buf));
                PR_smprintf_free(buf);
            }
        }
    }
#elif defined (XP_MACOSX)
    mOscpu.Adopt(nsCRT::strdup("PPC Mac OS X Mach-O"));
#elif defined (XP_MAC)
    long version;
    if (::Gestalt(gestaltSystemVersion, &version) == noErr && version >= 0x00001000)
        mOscpu.Adopt(nsCRT::strdup("PPC Mac OS X"));
    else
        mOscpu.Adopt(nsCRT::strdup("PPC"));
#elif defined (XP_UNIX) || defined (XP_BEOS)
    struct utsname name;
    
    int ret = uname(&name);
    if (ret >= 0) {
        nsCString buf;  
        buf =  (char*)name.sysname;
        buf += ' ';
        buf += (char*)name.machine;
        mOscpu.Assign(buf);
    }
#endif

    mUserAgentIsDirty = PR_TRUE;
}

void
nsHttpHandler::PrefsChanged(nsIPrefBranch *prefs, const char *pref)
{
    nsresult rv = NS_OK;
    PRInt32 val;

    LOG(("nsHttpHandler::PrefsChanged [pref=%s]\n", pref));

#define PREF_CHANGED(p) ((pref == nsnull) || !PL_strcmp(pref, p))

    //
    // UA components
    //

    // Gather vendor values.
    if (PREF_CHANGED(UA_PREF("vendor"))) {
        prefs->GetCharPref(UA_PREF("vendor"),
            getter_Copies(mVendor));
        mUserAgentIsDirty = PR_TRUE;
    }
    if (PREF_CHANGED(UA_PREF("vendorSub"))) {
        prefs->GetCharPref(UA_PREF("vendorSub"),
            getter_Copies(mVendorSub));
        mUserAgentIsDirty = PR_TRUE;
    }
    if (PREF_CHANGED(UA_PREF("vendorComment"))) {
        prefs->GetCharPref(UA_PREF("vendorComment"),
            getter_Copies(mVendorComment));
        mUserAgentIsDirty = PR_TRUE;
    }

    // Gather product values.
    if (PREF_CHANGED(UA_PREF("product"))) {
        prefs->GetCharPref(UA_PREF_PREFIX "product",
            getter_Copies(mProduct));
        mUserAgentIsDirty = PR_TRUE;
    }
    if (PREF_CHANGED(UA_PREF("productSub"))) {
        prefs->GetCharPref(UA_PREF("productSub"),
            getter_Copies(mProductSub));
        mUserAgentIsDirty = PR_TRUE;
    }
    if (PREF_CHANGED(UA_PREF("productComment"))) {
        prefs->GetCharPref(UA_PREF("productComment"),
            getter_Copies(mProductComment));
        mUserAgentIsDirty = PR_TRUE;
    }

    // Gather misc value.
    if (PREF_CHANGED(UA_PREF("misc"))) {
        prefs->GetCharPref(UA_PREF("misc"), getter_Copies(mMisc));
        mUserAgentIsDirty = PR_TRUE;
    }

    // Get Security level supported
    if (PREF_CHANGED(UA_PREF("security"))) {
        prefs->GetCharPref(UA_PREF("security"), getter_Copies(mSecurity));
        if (!mSecurity)
            mSecurity.Adopt(nsCRT::strdup(UA_APPSECURITY_FALLBACK));
        mUserAgentIsDirty = PR_TRUE;
    }

    // Gather locale.
    if (PREF_CHANGED(UA_PREF("locale"))) {
        nsCOMPtr<nsIPrefLocalizedString> pls;
        prefs->GetComplexValue(UA_PREF("locale"),
                                NS_GET_IID(nsIPrefLocalizedString),
                                getter_AddRefs(pls));
        if (pls) {
            nsXPIDLString uval;
            pls->ToString(getter_Copies(uval));
            if (uval)
                mLanguage.Adopt(ToNewUTF8String(nsDependentString(uval)));
        } 
        mUserAgentIsDirty = PR_TRUE;
    }

    // general.useragent.override
    if (PREF_CHANGED(UA_PREF("override"))) {
        prefs->GetCharPref(UA_PREF("override"), getter_Copies(mUserAgentOverride));
        mUserAgentIsDirty = PR_TRUE;
    }

    //
    // HTTP options
    //

    if (PREF_CHANGED(HTTP_PREF("keep-alive.timeout"))) {
        rv = prefs->GetIntPref(HTTP_PREF("keep-alive.timeout"), &val);
        if (NS_SUCCEEDED(rv))
            mIdleTimeout = (PRUint16) CLAMP(val, 1, 0xffff);
    }

    if (PREF_CHANGED(HTTP_PREF("request.max-attempts"))) {
        rv = prefs->GetIntPref(HTTP_PREF("request.max-attempts"), &val);
        if (NS_SUCCEEDED(rv))
            mMaxRequestAttempts = (PRUint16) CLAMP(val, 1, 0xffff);
    }

    if (PREF_CHANGED(HTTP_PREF("request.max-start-delay"))) {
        rv = prefs->GetIntPref(HTTP_PREF("request.max-start-delay"), &val);
        if (NS_SUCCEEDED(rv))
            mMaxRequestDelay = (PRUint16) CLAMP(val, 0, 0xffff);
    }

    if (PREF_CHANGED(HTTP_PREF("max-connections"))) {
        rv = prefs->GetIntPref(HTTP_PREF("max-connections"), &val);
        if (NS_SUCCEEDED(rv))
            mMaxConnections = (PRUint16) CLAMP(val, 1, 0xffff);
    }

    if (PREF_CHANGED(HTTP_PREF("max-connections-per-server"))) {
        rv = prefs->GetIntPref(HTTP_PREF("max-connections-per-server"), &val);
        if (NS_SUCCEEDED(rv))
            mMaxConnectionsPerServer = (PRUint8) CLAMP(val, 1, 0xff);
    }

    if (PREF_CHANGED(HTTP_PREF("max-persistent-connections-per-server"))) {
        rv = prefs->GetIntPref(HTTP_PREF("max-persistent-connections-per-server"), &val);
        if (NS_SUCCEEDED(rv))
            mMaxPersistentConnectionsPerServer = (PRUint8) CLAMP(val, 1, 0xff);
    }

    if (PREF_CHANGED(HTTP_PREF("max-persistent-connections-per-proxy"))) {
        rv = prefs->GetIntPref(HTTP_PREF("max-persistent-connections-per-proxy"), &val);
        if (NS_SUCCEEDED(rv))
            mMaxPersistentConnectionsPerProxy = (PRUint8) CLAMP(val, 1, 0xff);
    }

    if (PREF_CHANGED(HTTP_PREF("sendRefererHeader"))) {
        rv = prefs->GetIntPref(HTTP_PREF("sendRefererHeader"), &val);
        if (NS_SUCCEEDED(rv))
            mReferrerLevel = (PRUint8) CLAMP(val, 0, 0xff);
    }

    if (PREF_CHANGED(HTTP_PREF("redirection-limit"))) {
        rv = prefs->GetIntPref(HTTP_PREF("redirection-limit"), &val);
        if (NS_SUCCEEDED(rv))
            mRedirectionLimit = (PRUint8) CLAMP(val, 0, 0xff);
    }

    if (PREF_CHANGED(HTTP_PREF("version"))) {
        nsXPIDLCString httpVersion;
        prefs->GetCharPref(HTTP_PREF("version"), getter_Copies(httpVersion));
        if (httpVersion) {
            if (!PL_strcmp(httpVersion, "1.1"))
                mHttpVersion = NS_HTTP_VERSION_1_1;
            else if (!PL_strcmp(httpVersion, "0.9"))
                mHttpVersion = NS_HTTP_VERSION_0_9;
            else
                mHttpVersion = NS_HTTP_VERSION_1_0;
        }
    }

    if (PREF_CHANGED(HTTP_PREF("proxy.version"))) {
        nsXPIDLCString httpVersion;
        prefs->GetCharPref(HTTP_PREF("proxy.version"), getter_Copies(httpVersion));
        if (httpVersion) {
            if (!PL_strcmp(httpVersion, "1.1"))
                mProxyHttpVersion = NS_HTTP_VERSION_1_1;
            else
                mProxyHttpVersion = NS_HTTP_VERSION_1_0;
            // it does not make sense to issue a HTTP/0.9 request to a proxy server
        }
    }

    PRBool cVar = PR_FALSE;

    if (PREF_CHANGED(HTTP_PREF("keep-alive"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("keep-alive"), &cVar);
        if (NS_SUCCEEDED(rv)) {
            if (cVar)
                mCapabilities |= NS_HTTP_ALLOW_KEEPALIVE;
            else
                mCapabilities &= ~NS_HTTP_ALLOW_KEEPALIVE;
        }
    }

    if (PREF_CHANGED(HTTP_PREF("proxy.keep-alive"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("proxy.keep-alive"), &cVar);
        if (NS_SUCCEEDED(rv)) {
            if (cVar)
                mProxyCapabilities |= NS_HTTP_ALLOW_KEEPALIVE;
            else
                mProxyCapabilities &= ~NS_HTTP_ALLOW_KEEPALIVE;
        }
    }

    if (PREF_CHANGED(HTTP_PREF("pipelining"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("pipelining"), &cVar);
        if (NS_SUCCEEDED(rv)) {
            if (cVar)
                mCapabilities |=  NS_HTTP_ALLOW_PIPELINING;
            else
                mCapabilities &= ~NS_HTTP_ALLOW_PIPELINING;
        }
    }

    if (PREF_CHANGED(HTTP_PREF("pipelining.maxrequests"))) {
        rv = prefs->GetIntPref(HTTP_PREF("pipelining.maxrequests"), &val);
        if (NS_SUCCEEDED(rv))
            mMaxPipelinedRequests = CLAMP(val, 1, NS_HTTP_MAX_PIPELINED_REQUESTS);
    }

    if (PREF_CHANGED(HTTP_PREF("proxy.pipelining"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("proxy.pipelining"), &cVar);
        if (NS_SUCCEEDED(rv)) {
            if (cVar)
                mProxyCapabilities |=  NS_HTTP_ALLOW_PIPELINING;
            else
                mProxyCapabilities &= ~NS_HTTP_ALLOW_PIPELINING;
        }
    }

    if (PREF_CHANGED(HTTP_PREF("sendSecureXSiteReferrer"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("sendSecureXSiteReferrer"), &cVar);
        if (NS_SUCCEEDED(rv))
            mSendSecureXSiteReferrer = cVar;
    }

    /*
    if (bChangedAll || PL_strcmp(pref, "network.http.connect.timeout") == 0)
        prefs->GetIntPref("network.http.connect.timeout", &mConnectTimeout);

    if (bChangedAll || PL_strcmp(pref, "network.http.request.timeout") == 0)
        prefs->GetIntPref("network.http.request.timeout", &mRequestTimeout);
    */

    if (PREF_CHANGED(HTTP_PREF("accept.default"))) {
        nsXPIDLCString accept;
        rv = prefs->GetCharPref(HTTP_PREF("accept.default"),
                                  getter_Copies(accept));
        if (NS_SUCCEEDED(rv))
            SetAccept(accept);
    }
    
    if (PREF_CHANGED(HTTP_PREF("accept-encoding"))) {
        nsXPIDLCString acceptEncodings;
        rv = prefs->GetCharPref(HTTP_PREF("accept-encoding"),
                                  getter_Copies(acceptEncodings));
        if (NS_SUCCEEDED(rv))
            SetAcceptEncodings(acceptEncodings);
    }

    if (PREF_CHANGED(HTTP_PREF("use-cache"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("use-cache"), &cVar);
        if (NS_SUCCEEDED(rv)) {
            mUseCache = cVar;
            if (!mUseCache) {
                // release our references to the cache
                mCacheSession_ANY = 0;
                mCacheSession_MEM = 0;
            }
        }
    }

    if (PREF_CHANGED(HTTP_PREF("default-socket-type"))) {
        nsXPIDLCString val;
        rv = prefs->GetCharPref(HTTP_PREF("default-socket-type"),
                                getter_Copies(val));
        if (NS_SUCCEEDED(rv)) {
            if (val.IsEmpty())
                mDefaultSocketType.Adopt(0);
            else {
                // verify that this socket type is actually valid
                nsCOMPtr<nsISocketProviderService> sps(
                        do_GetService(kSocketProviderServiceCID, &rv));
                if (NS_SUCCEEDED(rv)) {
                    nsCOMPtr<nsISocketProvider> sp;
                    rv = sps->GetSocketProvider(val, getter_AddRefs(sp));
                    if (NS_SUCCEEDED(rv)) {
                        // OK, this looks like a valid socket provider.
                        mDefaultSocketType.Assign(val);
                    }
                }
            }
        }
    }

    //
    // INTL options
    //

    if (PREF_CHANGED(INTL_ACCEPT_LANGUAGES)) {
        nsCOMPtr<nsIPrefLocalizedString> pls;
        prefs->GetComplexValue(INTL_ACCEPT_LANGUAGES,
                                NS_GET_IID(nsIPrefLocalizedString),
                                getter_AddRefs(pls));
        if (pls) {
            nsXPIDLString uval;
            pls->ToString(getter_Copies(uval));
            if (uval)
                SetAcceptLanguages(NS_ConvertUCS2toUTF8(uval).get());
        } 
    }

    if (PREF_CHANGED(INTL_ACCEPT_CHARSET)) {
        nsCOMPtr<nsIPrefLocalizedString> pls;
        prefs->GetComplexValue(INTL_ACCEPT_CHARSET,
                                NS_GET_IID(nsIPrefLocalizedString),
                                getter_AddRefs(pls));
        if (pls) {
            nsXPIDLString uval;
            pls->ToString(getter_Copies(uval));
            if (uval)
                SetAcceptCharsets(NS_ConvertUCS2toUTF8(uval).get());
        } 
    }

    //
    // IDN options
    //

    if (PREF_CHANGED(NETWORK_ENABLEIDN)) {
        PRBool enableIDN = PR_FALSE;
        prefs->GetBoolPref(NETWORK_ENABLEIDN, &enableIDN);
        // No locking is required here since this method runs in the main
        // UI thread, and so do all the methods in nsHttpChannel.cpp
        // (mIDNConverter is used by nsHttpChannel)
        if (enableIDN && !mIDNConverter) {
            mIDNConverter = do_GetService(NS_IDNSERVICE_CONTRACTID, &rv);
            NS_ASSERTION(NS_SUCCEEDED(rv), "idnSDK not installed");
        }
        else if (!enableIDN && mIDNConverter)
            mIDNConverter = nsnull;
    }

#undef PREF_CHANGED
}

void
nsHttpHandler::GetPrefBranch(nsIPrefBranch **result)
{
    *result = nsnull;
    nsCOMPtr<nsIPrefService> prefService =
        do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefService)
        prefService->GetBranch(nsnull, result);
}

/**
 *  Allocates a C string into that contains a ISO 639 language list
 *  notated with HTTP "q" values for output with a HTTP Accept-Language
 *  header. Previous q values will be stripped because the order of
 *  the langs imply the q value. The q values are calculated by dividing
 *  1.0 amongst the number of languages present.
 *
 *  Ex: passing: "en, ja"
 *      returns: "en,ja;q=0.5"
 *
 *      passing: "en, ja, fr_CA"
 *      returns: "en,ja;q=0.7,fr_CA;q=0.3"
 */
static nsresult
PrepareAcceptLanguages(const char *i_AcceptLanguages, nsACString &o_AcceptLanguages)
{
    if (!i_AcceptLanguages)
        return NS_OK;

    PRUint32 n, size, wrote;
    double q, dec;
    char *p, *p2, *token, *q_Accept, *o_Accept;
    const char *comma;
    PRInt32 available;

    o_Accept = nsCRT::strdup(i_AcceptLanguages);
    if (nsnull == o_Accept)
        return NS_ERROR_OUT_OF_MEMORY;
    for (p = o_Accept, n = size = 0; '\0' != *p; p++) {
        if (*p == ',') n++;
            size++;
    }

    available = size + ++n * 11 + 1;
    q_Accept = new char[available];
    if ((char *) 0 == q_Accept)
        return nsnull;
    *q_Accept = '\0';
    q = 1.0;
    dec = q / (double) n;
    n = 0;
    p2 = q_Accept;
    for (token = nsCRT::strtok(o_Accept, ",", &p);
         token != (char *) 0;
         token = nsCRT::strtok(p, ",", &p))
    {
        while (*token == ' ' || *token == '\x9') token++;
        char* trim;
        trim = PL_strpbrk(token, "; \x9");
        if (trim != (char*)0)  // remove "; q=..." if present
            *trim = '\0';

        if (*token != '\0') {
            comma = n++ != 0 ? "," : ""; // delimiter if not first item
            PRUint32 u = QVAL_TO_UINT(q);
            if (u < 10)
                wrote = PR_snprintf(p2, available, "%s%s;q=0.%u", comma, token, u);
            else
                wrote = PR_snprintf(p2, available, "%s%s", comma, token);
            q -= dec;
            p2 += wrote;
            available -= wrote;
            NS_ASSERTION(available > 0, "allocated string not long enough");
        }
    }
    nsCRT::free(o_Accept);

    o_AcceptLanguages.Assign((const char *) q_Accept);
    delete [] q_Accept;

    return NS_OK;
}

nsresult
nsHttpHandler::SetAcceptLanguages(const char *aAcceptLanguages) 
{
    nsCString buf;
    nsresult rv = PrepareAcceptLanguages(aAcceptLanguages, buf);
    if (NS_SUCCEEDED(rv))
        mAcceptLanguages.Assign(buf);
    return rv;
}

/**
 *  Allocates a C string into that contains a character set/encoding list
 *  notated with HTTP "q" values for output with a HTTP Accept-Charset
 *  header. If the UTF-8 character set is not present, it will be added.
 *  If a wildcard catch-all is not present, it will be added. If more than
 *  one charset is set (as of 2001-02-07, only one is used), they will be
 *  comma delimited and with q values set for each charset in decending order.
 *
 *  Ex: passing: "euc-jp"
 *      returns: "euc-jp,utf-8;q=0.6,*;q=0.6"
 *
 *      passing: "UTF-8"
 *      returns: "UTF-8, *"
 */
static nsresult
PrepareAcceptCharsets(const char *i_AcceptCharset, nsACString &o_AcceptCharset)
{
    PRUint32 n, size, wrote, u;
    PRInt32 available;
    double q, dec;
    char *p, *p2, *token, *q_Accept, *o_Accept;
    const char *acceptable, *comma;
    PRBool add_utf = PR_FALSE;
    PRBool add_asterisk = PR_FALSE;

    if (!i_AcceptCharset)
        acceptable = "";
    else
        acceptable = i_AcceptCharset;
    o_Accept = nsCRT::strdup(acceptable);
    if (nsnull == o_Accept)
        return NS_ERROR_OUT_OF_MEMORY;
    for (p = o_Accept, n = size = 0; '\0' != *p; p++) {
        if (*p == ',') n++;
            size++;
    }

    // only add "utf-8" and "*" to the list if they aren't
    // already specified.

    if (PL_strcasestr(acceptable, "utf-8") == NULL) {
        n++;
        add_utf = PR_TRUE;
    }
    if (PL_strstr(acceptable, "*") == NULL) {
        n++;
        add_asterisk = PR_TRUE;
    }

    available = size + ++n * 11 + 1;
    q_Accept = new char[available];
    if ((char *) 0 == q_Accept)
        return NS_ERROR_OUT_OF_MEMORY;
    *q_Accept = '\0';
    q = 1.0;
    dec = q / (double) n;
    n = 0;
    p2 = q_Accept;
    for (token = nsCRT::strtok(o_Accept, ",", &p);
         token != (char *) 0;
         token = nsCRT::strtok(p, ",", &p)) {
        while (*token == ' ' || *token == '\x9') token++;
        char* trim;
        trim = PL_strpbrk(token, "; \x9");
        if (trim != (char*)0)  // remove "; q=..." if present
            *trim = '\0';

        if (*token != '\0') {
            comma = n++ != 0 ? "," : ""; // delimiter if not first item
            u = QVAL_TO_UINT(q);
            if (u < 10)
                wrote = PR_snprintf(p2, available, "%s%s;q=0.%u", comma, token, u);
            else
                wrote = PR_snprintf(p2, available, "%s%s", comma, token);
            q -= dec;
            p2 += wrote;
            available -= wrote;
            NS_ASSERTION(available > 0, "allocated string not long enough");
        }
    }
    if (add_utf) {
        comma = n++ != 0 ? "," : ""; // delimiter if not first item
        u = QVAL_TO_UINT(q);
        if (u < 10)
            wrote = PR_snprintf(p2, available, "%sutf-8;q=0.%u", comma, u);
        else
            wrote = PR_snprintf(p2, available, "%sutf-8", comma);
        q -= dec;
        p2 += wrote;
        available -= wrote;
        NS_ASSERTION(available > 0, "allocated string not long enough");
    }
    if (add_asterisk) {
        comma = n++ != 0 ? "," : ""; // delimiter if not first item

        // keep q of "*" equal to the lowest q value
        // in the event of a tie between the q of "*" and a non-wildcard
        // the non-wildcard always receives preference.

        q += dec;
        u = QVAL_TO_UINT(q);
        if (u < 10)
            wrote = PR_snprintf(p2, available, "%s*;q=0.%u", comma, u);
        else
            wrote = PR_snprintf(p2, available, "%s*", comma);
        available -= wrote;
        p2 += wrote;
        NS_ASSERTION(available > 0, "allocated string not long enough");
    }
    nsCRT::free(o_Accept);

    // change alloc from C++ new/delete to nsCRT::strdup's way
    o_AcceptCharset.Assign(q_Accept);
#if defined DEBUG_havill
    printf("Accept-Charset: %s\n", q_Accept);
#endif
    delete [] q_Accept;
    return NS_OK;
}

nsresult
nsHttpHandler::SetAcceptCharsets(const char *aAcceptCharsets) 
{
    nsCString buf;
    nsresult rv = PrepareAcceptCharsets(aAcceptCharsets, buf);
    if (NS_SUCCEEDED(rv))
        mAcceptCharsets.Assign(buf);
    return rv;
}

nsresult
nsHttpHandler::SetAccept(const char *aAccept) 
{
    mAccept = aAccept;
    return NS_OK;
}

nsresult
nsHttpHandler::SetAcceptEncodings(const char *aAcceptEncodings) 
{
    mAcceptEncodings = aAcceptEncodings;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS5(nsHttpHandler,
                              nsIHttpProtocolHandler,
                              nsIProxiedProtocolHandler,
                              nsIProtocolHandler,
                              nsIObserver,
                              nsISupportsWeakReference)

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIProtocolHandler
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::GetScheme(nsACString &aScheme)
{
    aScheme = NS_LITERAL_CSTRING("http");
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetDefaultPort(PRInt32 *result)
{
    *result = NS_HTTP_DEFAULT_PORT;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_STD | ALLOWS_PROXY | ALLOWS_PROXY_HTTP;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::NewURI(const nsACString &aSpec,
                      const char *aCharset,
                      nsIURI *aBaseURI,
                      nsIURI **aURI)
{
    LOG(("nsHttpHandler::NewURI\n"));
    return ::NewURI(aSpec, aCharset, aBaseURI, NS_HTTP_DEFAULT_PORT, aURI);
}

NS_IMETHODIMP
nsHttpHandler::NewChannel(nsIURI *uri, nsIChannel **result)
{
    LOG(("nsHttpHandler::NewChannel\n"));

    NS_ENSURE_ARG_POINTER(uri);
    NS_ENSURE_ARG_POINTER(result);

    PRBool isHttp = PR_FALSE, isHttps = PR_FALSE;

    // Verify that we have been given a valid scheme
    nsresult rv = uri->SchemeIs("http", &isHttp);
    if (NS_FAILED(rv)) return rv;
    if (!isHttp) {
        rv = uri->SchemeIs("https", &isHttps);
        if (NS_FAILED(rv)) return rv;
        if (!isHttps) {
            NS_WARNING("Invalid URI scheme");
            return NS_ERROR_UNEXPECTED;
        }
    }
    
    return NewProxiedChannel(uri, nsnull, result);
}

NS_IMETHODIMP 
nsHttpHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIProxiedProtocolHandler
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::NewProxiedChannel(nsIURI *uri,
                                 nsIProxyInfo* proxyInfo,
                                 nsIChannel **result)
{
    nsHttpChannel *httpChannel = nsnull;

    LOG(("nsHttpHandler::NewProxiedChannel [proxyInfo=%p]\n",
        proxyInfo));

    NS_NEWXPCOM(httpChannel, nsHttpChannel);
    if (!httpChannel)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(httpChannel);

    nsresult rv;

    PRBool https;
    rv = uri->SchemeIs("https", &https);
    if (NS_FAILED(rv)) return rv;

    // select proxy caps if using a non-transparent proxy.  SSL tunneling
    // should not use proxy settings.
    PRInt8 caps;
    if (proxyInfo && !nsCRT::strcmp(proxyInfo->Type(), "http") && !https)
        caps = mProxyCapabilities;
    else
        caps = mCapabilities;

    if (https) {
        // HACK: make sure PSM gets initialized on the main thread.
        nsCOMPtr<nsISocketProviderService> spserv =
                do_GetService(kSocketProviderServiceCID);
        if (spserv) {
            nsCOMPtr<nsISocketProvider> provider;
            spserv->GetSocketProvider("ssl", getter_AddRefs(provider));
        }
    }

    rv = httpChannel->Init(uri, caps, proxyInfo);

    if (NS_FAILED(rv)) {
        NS_RELEASE(httpChannel);
        return rv;
    }

    *result = httpChannel;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIHttpProtocolHandler
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::GetUserAgent(nsACString &value)
{
    value = UserAgent();
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetAppName(nsACString &value)
{
    value = mAppName;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetAppVersion(nsACString &value)
{
    value = mAppVersion;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetVendor(nsACString &value)
{
    value = mVendor;
    return NS_OK;
}
NS_IMETHODIMP
nsHttpHandler::SetVendor(const nsACString &value)
{
    mVendor = value;
    mUserAgentIsDirty = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetVendorSub(nsACString &value)
{
    value = mVendorSub;
    return NS_OK;
}
NS_IMETHODIMP
nsHttpHandler::SetVendorSub(const nsACString &value)
{
    mVendorSub = value;
    mUserAgentIsDirty = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetVendorComment(nsACString &value)
{
    value = mVendorComment;
    return NS_OK;
}
NS_IMETHODIMP
nsHttpHandler::SetVendorComment(const nsACString &value)
{
    mVendorComment = value;
    mUserAgentIsDirty = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetProduct(nsACString &value)
{
    value = mProduct;
    return NS_OK;
}
NS_IMETHODIMP
nsHttpHandler::SetProduct(const nsACString &value)
{
    mProduct = value;
    mUserAgentIsDirty = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetProductSub(nsACString &value)
{
    value = mProductSub;
    return NS_OK;
}
NS_IMETHODIMP
nsHttpHandler::SetProductSub(const nsACString &value)
{
    mProductSub = value;
    mUserAgentIsDirty = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetProductComment(nsACString &value)
{
    value = mProductComment;
    return NS_OK;
}
NS_IMETHODIMP
nsHttpHandler::SetProductComment(const nsACString &value)
{
    mProductComment = value;
    mUserAgentIsDirty = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetPlatform(nsACString &value)
{
    value = mPlatform;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetOscpu(nsACString &value)
{
    value = mOscpu;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetLanguage(nsACString &value)
{
    value = mLanguage;
    return NS_OK;
}
NS_IMETHODIMP
nsHttpHandler::SetLanguage(const nsACString &value)
{
    mLanguage = value;
    mUserAgentIsDirty = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetMisc(nsACString &value)
{
    value = mMisc;
    return NS_OK;
}
NS_IMETHODIMP
nsHttpHandler::SetMisc(const nsACString &value)
{
    mMisc = value;
    mUserAgentIsDirty = PR_TRUE;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::Observe(nsISupports *subject,
                       const char *topic,
                       const PRUnichar *data)
{
    LOG(("nsHttpHandler::Observe [topic=\"%s\")]\n", topic));

    if (strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) {
        nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(subject);
        if (prefBranch)
            PrefsChanged(prefBranch, NS_ConvertUCS2toUTF8(data).get());
        
        // XXX should probably shutdown and init the conn mgr.
    }
    else if (strcmp(topic, "profile-change-net-teardown")    == 0 ||
             strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)    == 0) {

        // kill off the "prune dead connections" timer
        StopPruneDeadConnectionsTimer();

        // clear cache of all authentication credentials.
        mAuthCache.ClearAll();

        // ensure connection manager is shutdown
        if (mConnMgr)
            mConnMgr->Shutdown();

        // need to reset the session start time since cache validation may
        // depend on this value.
        mSessionStartTime = NowInSeconds();
    }
    else if (strcmp(topic, "session-logout") == 0) {
        // clear cache of all authentication credentials.
        mAuthCache.ClearAll();

        // need to reset the session start time since cache validation may
        // depend on this value.
        mSessionStartTime = NowInSeconds();
    }
    else if (strcmp(topic, "profile-change-net-restore") == 0) {
        // initialize connection manager
        InitConnectionMgr();

        // restart the "prune dead connections" timer
        StartPruneDeadConnectionsTimer();
    }
    else if (strcmp(topic, "timer-callback") == 0) {
        // prune dead connections
#ifdef DEBUG
        nsCOMPtr<nsITimer> timer = do_QueryInterface(subject);
        NS_ASSERTION(timer == mTimer, "unexpected timer-callback");
#endif
        if (mConnMgr)
            mConnMgr->PruneDeadConnections();
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpsHandler implementation
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS4(nsHttpsHandler,
                              nsIHttpProtocolHandler,
                              nsIProxiedProtocolHandler,
                              nsIProtocolHandler,
                              nsISupportsWeakReference)

nsresult
nsHttpsHandler::Init()
{
    nsCOMPtr<nsIProtocolHandler> httpHandler(
            do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http"));
    NS_ASSERTION(httpHandler.get() != nsnull, "no http handler?");
    return NS_OK;
}

NS_IMETHODIMP
nsHttpsHandler::GetScheme(nsACString &aScheme)
{
    aScheme = NS_LITERAL_CSTRING("https");
    return NS_OK;
}

NS_IMETHODIMP
nsHttpsHandler::GetDefaultPort(PRInt32 *aPort)
{
    *aPort = NS_HTTPS_DEFAULT_PORT;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpsHandler::GetProtocolFlags(PRUint32 *aProtocolFlags)
{
    return gHttpHandler->GetProtocolFlags(aProtocolFlags);
}

NS_IMETHODIMP
nsHttpsHandler::NewURI(const nsACString &aSpec,
                       const char *aOriginCharset,
                       nsIURI *aBaseURI,
                       nsIURI **_retval)
{
    return ::NewURI(aSpec, aOriginCharset, aBaseURI, NS_HTTPS_DEFAULT_PORT, _retval);
}

NS_IMETHODIMP
nsHttpsHandler::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
    return gHttpHandler->NewChannel(aURI, _retval);
}

NS_IMETHODIMP
nsHttpsHandler::AllowPort(PRInt32 aPort, const char *aScheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}
