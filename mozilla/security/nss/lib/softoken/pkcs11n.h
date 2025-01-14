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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *      Dr Stephen Henson <stephen.henson@gemplus.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifndef _PKCS11N_H_
#define _PKCS11N_H_

#ifdef DEBUG
static const char CKT_CVS_ID[] = "@(#) $RCSfile: pkcs11n.h,v $ $Revision: 1.5.2.1 $ $Date: 2002/11/26 22:02:06 $ $Name:  $";
#endif /* DEBUG */

/*
 * pkcs11n.h
 *
 * This file contains the NSS-specific type definitions for Cryptoki
 * (PKCS#11).
 */

/*
 * NSSCK_VENDOR_NSS
 *
 * Cryptoki reserves the high half of all the number spaces for
 * vendor-defined use.  I'd like to keep all of our Netscape-
 * specific values together, but not in the oh-so-obvious
 * 0x80000001, 0x80000002, etc. area.  So I've picked an offset,
 * and constructed values for the beginnings of our spaces.
 *
 * Note that some "historical" Netscape values don't fall within
 * this range.
 */
#define NSSCK_VENDOR_NSS 0x4E534350 /* NSCP */

/*
 * Netscape-defined object classes
 * 
 */
#define CKO_NSS (CKO_VENDOR_DEFINED|NSSCK_VENDOR_NSS)

#define CKO_NSS_CRL                (CKO_NSS + 1)
#define CKO_NSS_SMIME              (CKO_NSS + 2)
#define CKO_NSS_TRUST              (CKO_NSS + 3)
#define CKO_NSS_BUILTIN_ROOT_LIST  (CKO_NSS + 4)

/*
 * Netscape-defined key types
 *
 */
#define CKK_NSS (CKK_VENDOR_DEFINED|NSSCK_VENDOR_NSS)

#define CKK_NSS_PKCS8              (CKK_NSS + 1)
/*
 * Netscape-defined certificate types
 *
 */
#define CKC_NSS (CKC_VENDOR_DEFINED|NSSCK_VENDOR_NSS)

/*
 * Netscape-defined object attributes
 *
 */
#define CKA_NSS (CKA_VENDOR_DEFINED|NSSCK_VENDOR_NSS)

#define CKA_NSS_URL                (CKA_NSS +  1)
#define CKA_NSS_EMAIL              (CKA_NSS +  2)
#define CKA_NSS_SMIME_INFO         (CKA_NSS +  3)
#define CKA_NSS_SMIME_TIMESTAMP    (CKA_NSS +  4)
#define CKA_NSS_PKCS8_SALT         (CKA_NSS +  5)
#define CKA_NSS_PASSWORD_CHECK     (CKA_NSS +  6)
#define CKA_NSS_EXPIRES            (CKA_NSS +  7)
#define CKA_NSS_KRL                (CKA_NSS +  8)

#define CKA_NSS_PQG_COUNTER        (CKA_NSS +  20)
#define CKA_NSS_PQG_SEED           (CKA_NSS +  21)
#define CKA_NSS_PQG_H              (CKA_NSS +  22)
#define CKA_NSS_PQG_SEED_BITS      (CKA_NSS +  23)

/*
 * Trust attributes:
 *
 * If trust goes standard, these probably will too.  So I'll
 * put them all in one place.
 */

#define CKA_TRUST (CKA_NSS + 0x2000)

/* "Usage" key information */
#define CKA_TRUST_DIGITAL_SIGNATURE     (CKA_TRUST +  1)
#define CKA_TRUST_NON_REPUDIATION       (CKA_TRUST +  2)
#define CKA_TRUST_KEY_ENCIPHERMENT      (CKA_TRUST +  3)
#define CKA_TRUST_DATA_ENCIPHERMENT     (CKA_TRUST +  4)
#define CKA_TRUST_KEY_AGREEMENT         (CKA_TRUST +  5)
#define CKA_TRUST_KEY_CERT_SIGN         (CKA_TRUST +  6)
#define CKA_TRUST_CRL_SIGN              (CKA_TRUST +  7)

/* "Purpose" trust information */
#define CKA_TRUST_SERVER_AUTH           (CKA_TRUST +  8)
#define CKA_TRUST_CLIENT_AUTH           (CKA_TRUST +  9)
#define CKA_TRUST_CODE_SIGNING          (CKA_TRUST + 10)
#define CKA_TRUST_EMAIL_PROTECTION      (CKA_TRUST + 11)
#define CKA_TRUST_IPSEC_END_SYSTEM      (CKA_TRUST + 12)
#define CKA_TRUST_IPSEC_TUNNEL          (CKA_TRUST + 13)
#define CKA_TRUST_IPSEC_USER            (CKA_TRUST + 14)
#define CKA_TRUST_TIME_STAMPING         (CKA_TRUST + 15)
#define CKA_TRUST_STEP_UP_APPROVED      (CKA_TRUST + 16)
/* we'll add this functionality later -- ck */
#define CKA_CERT_SHA1_HASH	        (CKA_TRUST + 100)
#define CKA_CERT_MD5_HASH		(CKA_TRUST + 101)

/* Netscape trust stuff */
/* XXX fgmr new ones here-- step-up, etc. */

/* HISTORICAL: define used to pass in the database key for DSA private keys */
#define CKA_NSS_DB                 0xD5A0DB00L
#define CKA_NSS_TRUST              0x80000001L

/*
 * Netscape-defined crypto mechanisms
 *
 */
#define CKM_NSS (CKM_VENDOR_DEFINED|NSSCK_VENDOR_NSS)
/*
 * HISTORICAL:
 * Do not attempt to use these. They are only used by NETSCAPE's internal
 * PKCS #11 interface. Most of these are place holders for other mechanism
 * and will change in the future.
 */
#define CKM_NSS_PBE_SHA1_DES_CBC           0x80000002L
#define CKM_NSS_PBE_SHA1_TRIPLE_DES_CBC    0x80000003L
#define CKM_NSS_PBE_SHA1_40_BIT_RC2_CBC    0x80000004L
#define CKM_NSS_PBE_SHA1_128_BIT_RC2_CBC   0x80000005L
#define CKM_NSS_PBE_SHA1_40_BIT_RC4        0x80000006L
#define CKM_NSS_PBE_SHA1_128_BIT_RC4       0x80000007L
#define CKM_NSS_PBE_SHA1_FAULTY_3DES_CBC   0x80000008L
#define CKM_NSS_PBE_SHA1_HMAC_KEY_GEN      0x80000009L
#define CKM_NSS_PBE_MD5_HMAC_KEY_GEN       0x8000000aL
#define CKM_NSS_PBE_MD2_HMAC_KEY_GEN       0x8000000bL

#define CKM_TLS_PRF_GENERAL                     0x80000373L


/*
 * Netscape-defined return values
 *
 */
#define CKR_NSS (CKM_VENDOR_DEFINED|NSSCK_VENDOR_NSS)

/*
 * Trust info
 *
 * This isn't part of the Cryptoki standard (yet), so I'm putting
 * all the definitions here.  Some of this would move to nssckt.h
 * if trust info were made part of the standard.  In view of this
 * possibility, I'm putting my (Netscape) values in the netscape
 * vendor space, like everything else.
 */

typedef CK_ULONG          CK_TRUST;

/* The following trust types are defined: */
#define CKT_VENDOR_DEFINED     0x80000000

#define CKT_NSS (CKT_VENDOR_DEFINED|NSSCK_VENDOR_NSS)

/* If trust goes standard, these'll probably drop out of vendor space. */
#define CKT_NSS_TRUSTED            (CKT_NSS + 1)
#define CKT_NSS_TRUSTED_DELEGATOR  (CKT_NSS + 2)
#define CKT_NSS_UNTRUSTED          (CKT_NSS + 3)
#define CKT_NSS_MUST_VERIFY        (CKT_NSS + 4)
#define CKT_NSS_TRUST_UNKNOWN      (CKT_NSS + 5) /* default */
#define CKT_NSS_MUST_VERIFY_TRUST  (CKT_NSS + 3)
#define CKT_NSS_NOT_TRUSTED        (CKT_NSS + 10)

/* 
 * These may well remain Netscape-specific; I'm only using them
 * to cache resolution data.
 */
#define CKT_NSS_VALID              (CKT_NSS + 10)
#define CKT_NSS_VALID_DELEGATOR    (CKT_NSS + 11)


/*
 * These are not really PKCS #11 values specifically. They are the 'loadable'
 * module spec NSS uses. The are available for others to use as well, but not
 * part of the formal PKCS #11 spec.
 *
 * The function 'FIND' returns an array of PKCS #11 initialization strings
 * The function 'ADD' takes a PKCS #11 initialization string and stores it.
 * The function 'DEL' takes a 'name= library=' value and deletes the associated
 *  string.
 * The function 'RELEASE' frees the array returned by 'FIND'
 */
#define SECMOD_MODULE_DB_FUNCTION_FIND  0
#define SECMOD_MODULE_DB_FUNCTION_ADD   1
#define SECMOD_MODULE_DB_FUNCTION_DEL   2
#define SECMOD_MODULE_DB_FUNCTION_RELEASE 3 
typedef char ** (PR_CALLBACK *SECMODModuleDBFunc)(unsigned long function,
                                        char *parameters, void *moduleSpec);

#endif /* _PKCS11N_H_ */
