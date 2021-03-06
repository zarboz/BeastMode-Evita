/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef WLAN_FEATURE_VOWIFI_11R
/**=========================================================================

  \brief Definitions for SME FT APIs

   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.

   Qualcomm Confidential and Proprietary.

  ========================================================================*/

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <smsDebug.h>
#include <csrInsideApi.h>
#include <csrNeighborRoam.h>

/*--------------------------------------------------------------------------
  Initialize the FT context. 
  ------------------------------------------------------------------------*/
void sme_FTOpen(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus     status = eHAL_STATUS_SUCCESS;

    pMac->ft.ftSmeContext.auth_ft_ies = NULL;                        
    pMac->ft.ftSmeContext.auth_ft_ies_length = 0;                        

    pMac->ft.ftSmeContext.reassoc_ft_ies = NULL;                        
    pMac->ft.ftSmeContext.reassoc_ft_ies_length = 0;       

    status = palTimerAlloc(pMac->hHdd, &pMac->ft.ftSmeContext.preAuthReassocIntvlTimer, 
                            sme_PreauthReassocIntvlTimerCallback, (void *)pMac);

    if (eHAL_STATUS_SUCCESS != status)
    {
        smsLog(pMac, LOGE, FL("Preauth Reassoc interval Timer allocation failed"));
        return;
    }                 

    pMac->ft.ftSmeContext.psavedFTPreAuthRsp = NULL;                        

    pMac->ft.ftSmeContext.FTState = eFT_START_READY;
}

/*--------------------------------------------------------------------------
  Cleanup the SME FT Global context. 
  ------------------------------------------------------------------------*/
void sme_FTClose(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    if (pMac->ft.ftSmeContext.auth_ft_ies != NULL)
    {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
        smsLog( pMac, LOGE, FL(" Freeing %p and setting to NULL\n"), 
            pMac->ft.ftSmeContext.auth_ft_ies);
#endif
        vos_mem_free(pMac->ft.ftSmeContext.auth_ft_ies);
        pMac->ft.ftSmeContext.auth_ft_ies = NULL;
    }
    pMac->ft.ftSmeContext.auth_ft_ies_length = 0;                        

    if (pMac->ft.ftSmeContext.reassoc_ft_ies != NULL)
    {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
        smsLog( pMac, LOGE, FL(" Freeing %p and setting to NULL\n"), 
            pMac->ft.ftSmeContext.reassoc_ft_ies);
#endif
        vos_mem_free(pMac->ft.ftSmeContext.reassoc_ft_ies);
        pMac->ft.ftSmeContext.reassoc_ft_ies = NULL;                        
    }
    pMac->ft.ftSmeContext.reassoc_ft_ies_length = 0;                        

    pMac->ft.ftSmeContext.FTState = eFT_START_READY;
    vos_mem_zero(pMac->ft.ftSmeContext.preAuthbssId, ANI_MAC_ADDR_SIZE);

    if (pMac->ft.ftSmeContext.psavedFTPreAuthRsp != NULL)
    {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
        smsLog( pMac, LOGE, FL("%s: Freeing %p and setting to NULL\n"), 
            pMac->ft.ftSmeContext.psavedFTPreAuthRsp);
#endif
        vos_mem_free(pMac->ft.ftSmeContext.psavedFTPreAuthRsp);
        pMac->ft.ftSmeContext.psavedFTPreAuthRsp = NULL;                        
    }

    palTimerFree(pMac->hHdd, pMac->ft.ftSmeContext.preAuthReassocIntvlTimer);
}


/*--------------------------------------------------------------------------
  Each time the supplicant sends down the FT IEs to the driver.
  This function is called in SME. This fucntion packages and sends
  the FT IEs to PE.
  ------------------------------------------------------------------------*/
void sme_SetFTIEs( tHalHandle hHal, tANI_U8 sessionId, tANI_U8 *ft_ies, 
        tANI_U16 ft_ies_length )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status = eHAL_STATUS_FAILURE;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if (!( HAL_STATUS_SUCCESS( status ))) return;

    if (ft_ies == NULL) 
    {
        smsLog( pMac, LOGE, FL(" ft ies is NULL\n"));
        sme_ReleaseGlobalLock( &pMac->sme );
        return; 
    }

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    smsLog( pMac, LOGE, "FT IEs Req is received in state %d\n", 
        pMac->ft.ftSmeContext.FTState);
#endif

    // Global Station FT State
    switch(pMac->ft.ftSmeContext.FTState)
    {
        case eFT_START_READY:
        case eFT_AUTH_REQ_READY:
            if ((pMac->ft.ftSmeContext.auth_ft_ies) && 
                    (pMac->ft.ftSmeContext.auth_ft_ies_length))
            {
                // Free the one we received last from the supplicant
                vos_mem_free(pMac->ft.ftSmeContext.auth_ft_ies);
                pMac->ft.ftSmeContext.auth_ft_ies_length = 0; 
            }

            // Save the FT IEs
            pMac->ft.ftSmeContext.auth_ft_ies = vos_mem_malloc(ft_ies_length);
            if(pMac->ft.ftSmeContext.auth_ft_ies == NULL)
            {
               smsLog( pMac, LOGE, FL("Memory allocation failed for "
                                      "auth_ft_ies\n"));
               sme_ReleaseGlobalLock( &pMac->sme );
               return;
            }
            pMac->ft.ftSmeContext.auth_ft_ies_length = ft_ies_length;
            vos_mem_copy((tANI_U8 *)pMac->ft.ftSmeContext.auth_ft_ies, ft_ies,
                ft_ies_length);
                
            pMac->ft.ftSmeContext.FTState = eFT_AUTH_REQ_READY;

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
            smsLog( pMac, LOGE, "ft_ies_length=%d\n", ft_ies_length);
            /*
            smsLog( pMac, LOGE, "%d: New Auth ft_ies_length=%02x%02x%02x\n", 
                current->pid, pMac->ft.ftSmeContext.auth_ft_ies[0],
                pMac->ft.ftSmeContext.auth_ft_ies[1],
                pMac->ft.ftSmeContext.auth_ft_ies[2]);
                */
#endif
            break;

        case eFT_AUTH_COMPLETE:
            // We will need to re-start preauth. If we received FT IEs in
            // eFT_PRE_AUTH_DONE state, it implies there was a rekey in 
            // our pre-auth state. Hence this implies we need Pre-auth again.

            // OK now inform SME we have no pre-auth list.
            // Delete the pre-auth node locally. Set your self back to restart pre-auth
            // TBD
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
            smsLog( pMac, LOGE, 
                "Pre-auth done and now receiving---> AUTH REQ <---- in state %d\n", 
                pMac->ft.ftSmeContext.FTState);
            smsLog( pMac, LOGE, "Unhandled reception of FT IES in state %d\n", 
                pMac->ft.ftSmeContext.FTState);
#endif
            break;

        case eFT_REASSOC_REQ_WAIT:
            // We are done with pre-auth, hence now waiting for
            // reassoc req. This is the new FT Roaming in place

            // At this juncture we are ready to start sending Re-Assoc Req.
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
            smsLog( pMac, LOGE, "New Reassoc Req=%p in state %d\n", 
                ft_ies, pMac->ft.ftSmeContext.FTState);
#endif
            if ((pMac->ft.ftSmeContext.reassoc_ft_ies) && 
                (pMac->ft.ftSmeContext.reassoc_ft_ies_length))
            {
                // Free the one we received last from the supplicant
                vos_mem_free(pMac->ft.ftSmeContext.reassoc_ft_ies);
                pMac->ft.ftSmeContext.reassoc_ft_ies_length = 0; 
            }

            // Save the FT IEs
            pMac->ft.ftSmeContext.reassoc_ft_ies = vos_mem_malloc(ft_ies_length);
            if(pMac->ft.ftSmeContext.reassoc_ft_ies == NULL)
            {
               smsLog( pMac, LOGE, FL("Memory allocation failed for "
                                      "reassoc_ft_ies\n"));
               sme_ReleaseGlobalLock( &pMac->sme );
               return;
            }
            pMac->ft.ftSmeContext.reassoc_ft_ies_length = ft_ies_length;
            vos_mem_copy((tANI_U8 *)pMac->ft.ftSmeContext.reassoc_ft_ies, ft_ies,
                ft_ies_length);
                
            pMac->ft.ftSmeContext.FTState = eFT_SET_KEY_WAIT;
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
            smsLog( pMac, LOGE, "ft_ies_length=%d state=%d\n", ft_ies_length,
                pMac->ft.ftSmeContext.FTState);
            /*
            smsLog( pMac, LOGE, "%d: New Auth ft_ies_length=%02x%02x%02x\n", 
                current->pid, pMac->ft.ftSmeContext.reassoc_ft_ies[0],
                pMac->ft.ftSmeContext.reassoc_ft_ies[1],
                pMac->ft.ftSmeContext.reassoc_ft_ies[2]);
                */
#endif
            
            break;

        default:
            smsLog( pMac, LOGE, FL(" Unhandled state=%d\n"),
                pMac->ft.ftSmeContext.FTState);
            break;
    }
    sme_ReleaseGlobalLock( &pMac->sme );
}
            
eHalStatus sme_FTSendUpdateKeyInd(tHalHandle hHal, tCsrRoamSetKey * pFTKeyInfo)
{
    tSirFTUpdateKeyInfo *pMsg;
    tANI_U16 msgLen;
    eHalStatus status = eHAL_STATUS_FAILURE;
    tAniEdType tmpEdType;
    tAniKeyDirection tmpDirection;
    //tANI_U8 *pBuf;
    tANI_U8 *p = NULL;
    tAniEdType edType;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    int i = 0;

    smsLog(pMac, LOGE, FL("keyLength %d\n"), pFTKeyInfo->keyLength);

      for(i=0; i<pFTKeyInfo->keyLength; i++)
          smsLog(pMac, LOGE, FL("%02x"), pFTKeyInfo->Key[i]); 

    msgLen  = sizeof( tANI_U16) + sizeof( tANI_U16 ) + 
       sizeof( pMsg->keyMaterial.length ) + sizeof( pMsg->keyMaterial.edType ) + 
       sizeof( pMsg->keyMaterial.numKeys ) + sizeof( pMsg->keyMaterial.key );
                     
    status = palAllocateMemory(pMac->hHdd, (void **)&pMsg, msgLen);
    if ( !HAL_STATUS_SUCCESS(status) )
    {
       return eHAL_STATUS_FAILURE;
    }

    palZeroMemory(pMac->hHdd, pMsg, msgLen);
    pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_FT_UPDATE_KEY);
    pMsg->length = pal_cpu_to_be16(msgLen);

    p = (tANI_U8 *)&pMsg->keyMaterial;

    // Set the pMsg->keyMaterial.length field (this length is defined as all data that follows the edType field
    // in the tSirKeyMaterial keyMaterial; field).
    //
    // !!NOTE:  This keyMaterial.length contains the length of a MAX size key, though the keyLength can be 
    // shorter than this max size.  Is LIM interpreting this ok ?
    p = pal_set_U16( p, pal_cpu_to_be16((tANI_U16)( sizeof( pMsg->keyMaterial.numKeys ) + 
                                                    ( pMsg->keyMaterial.numKeys * sizeof( pMsg->keyMaterial.key ) ) )) );

    // set pMsg->keyMaterial.edType
    edType = csrTranslateEncryptTypeToEdType( pFTKeyInfo->encType );
    tmpEdType = pal_cpu_to_be32(edType);
    palCopyMemory( pMac->hHdd, p, (tANI_U8 *)&tmpEdType, sizeof(tAniEdType) );
    p += sizeof( pMsg->keyMaterial.edType );

    // set the pMsg->keyMaterial.numKeys field
    *p = pMsg->keyMaterial.numKeys;
    p += sizeof( pMsg->keyMaterial.numKeys );   

    // set pSirKey->keyId = keyId;
    *p = pMsg->keyMaterial.key[ 0 ].keyId;
    p += sizeof( pMsg->keyMaterial.key[ 0 ].keyId );

    // set pSirKey->unicast = (tANI_U8)fUnicast;
    *p = (tANI_U8)eANI_BOOLEAN_TRUE;
    p += sizeof( pMsg->keyMaterial.key[ 0 ].unicast );

    // set pSirKey->keyDirection = aniKeyDirection;
    tmpDirection = pal_cpu_to_be32(pFTKeyInfo->keyDirection);
    palCopyMemory( pMac->hHdd, p, (tANI_U8 *)&tmpDirection, sizeof(tAniKeyDirection) );
    p += sizeof(tAniKeyDirection);
    //    pSirKey->keyRsc = ;;
    palCopyMemory( pMac->hHdd, p, pFTKeyInfo->keyRsc, CSR_MAX_RSC_LEN );
    p += sizeof( pMsg->keyMaterial.key[ 0 ].keyRsc );

    // set pSirKey->paeRole
    *p = pFTKeyInfo->paeRole;   // 0 is Supplicant
    p++;

    // set pSirKey->keyLength = keyLength;
    p = pal_set_U16( p, pal_cpu_to_be16(pFTKeyInfo->keyLength) );

    if ( pFTKeyInfo->keyLength && pFTKeyInfo->Key ) 
    {   
        palCopyMemory( pMac->hHdd, p, pFTKeyInfo->Key, pFTKeyInfo->keyLength ); 
        if(pFTKeyInfo->keyLength == 16)
        {
            smsLog(pMac, LOGE, "  SME Set keyIdx (%d) encType(%d) key = %02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X\n",
            pFTKeyInfo->keyId, edType, pFTKeyInfo->Key[0], pFTKeyInfo->Key[1], pFTKeyInfo->Key[2], pFTKeyInfo->Key[3], pFTKeyInfo->Key[4],
            pFTKeyInfo->Key[5], pFTKeyInfo->Key[6], pFTKeyInfo->Key[7], pFTKeyInfo->Key[8],
            pFTKeyInfo->Key[9], pFTKeyInfo->Key[10], pFTKeyInfo->Key[11], pFTKeyInfo->Key[12], pFTKeyInfo->Key[13], pFTKeyInfo->Key[14], pFTKeyInfo->Key[15]);
        }
    }

    status = palSendMBMessage(pMac->hHdd, pMsg);

    return( status );
}

eHalStatus sme_FTUpdateKey( tHalHandle hHal, tCsrRoamSetKey * pFTKeyInfo )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status = eHAL_STATUS_FAILURE;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if (!( HAL_STATUS_SUCCESS( status )))
    {
       return eHAL_STATUS_FAILURE;
    }

    if (pFTKeyInfo == NULL) 
    {
        smsLog( pMac, LOGE, "%s: pFTKeyInfo is NULL\n", __FUNCTION__);
        sme_ReleaseGlobalLock( &pMac->sme );
        return eHAL_STATUS_FAILURE; 
    }

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    smsLog( pMac, LOGE, "sme_FTUpdateKey is received in state %d\n", 
        pMac->ft.ftSmeContext.FTState);
#endif

    // Global Station FT State
    switch(pMac->ft.ftSmeContext.FTState)
    {
    case eFT_SET_KEY_WAIT:
       status = sme_FTSendUpdateKeyInd( hHal, pFTKeyInfo );
       pMac->ft.ftSmeContext.FTState = eFT_START_READY;
       break;
          
    default:
       smsLog( pMac, LOGE, "%s: Unhandled state=%d\n", __FUNCTION__,
               pMac->ft.ftSmeContext.FTState);
       status = eHAL_STATUS_FAILURE;
       break;
    }
    sme_ReleaseGlobalLock( &pMac->sme );

    return status;
}
/*--------------------------------------------------------------------------
 *
 * HDD Interface to SME. SME now sends the Auth 2 and RIC IEs up to the supplicant.
 * The supplicant will then proceed to send down the
 * Reassoc Req.
 *
 *------------------------------------------------------------------------*/
void sme_GetFTPreAuthResponse( tHalHandle hHal, tANI_U8 *ft_ies, 
                               tANI_U32 ft_ies_ip_len, tANI_U16 *ft_ies_length )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status = eHAL_STATUS_FAILURE;

    *ft_ies_length = 0;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if (!( HAL_STATUS_SUCCESS( status ))) 
       return;

    /* All or nothing - proceed only if both BSSID and FT IE fit */
    if((ANI_MAC_ADDR_SIZE + 
       pMac->ft.ftSmeContext.psavedFTPreAuthRsp->ft_ies_length) > ft_ies_ip_len) 
    {
       sme_ReleaseGlobalLock( &pMac->sme );
       return;
    }

    // hdd needs to pack the bssid also along with the 
    // auth response to supplicant
    vos_mem_copy(ft_ies, pMac->ft.ftSmeContext.preAuthbssId, ANI_MAC_ADDR_SIZE);
    
    // Copy the auth resp FTIEs
    vos_mem_copy(&(ft_ies[ANI_MAC_ADDR_SIZE]), 
                 pMac->ft.ftSmeContext.psavedFTPreAuthRsp->ft_ies, 
                 pMac->ft.ftSmeContext.psavedFTPreAuthRsp->ft_ies_length);

    *ft_ies_length = ANI_MAC_ADDR_SIZE + 
       pMac->ft.ftSmeContext.psavedFTPreAuthRsp->ft_ies_length;

    pMac->ft.ftSmeContext.FTState = eFT_REASSOC_REQ_WAIT;

#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
    smsLog( pMac, LOGE, FL(" Filled auth resp = %d\n"), *ft_ies_length);
#endif
    sme_ReleaseGlobalLock( &pMac->sme );
    return;
}

/*--------------------------------------------------------------------------
 *
 * SME now sends the RIC IEs up to the supplicant.
 * The supplicant will then proceed to send down the
 * Reassoc Req.
 *
 *------------------------------------------------------------------------*/
void sme_GetRICIEs( tHalHandle hHal, tANI_U8 *ric_ies, tANI_U32 ric_ies_ip_len,
                    tANI_U32 *ric_ies_length )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status = eHAL_STATUS_FAILURE;

    *ric_ies_length = 0;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if (!( HAL_STATUS_SUCCESS( status ))) 
       return;

    /* All or nothing */
    if (pMac->ft.ftSmeContext.psavedFTPreAuthRsp->ric_ies_length > 
        ric_ies_ip_len)
    {
       sme_ReleaseGlobalLock( &pMac->sme );
       return;
    }

    vos_mem_copy(ric_ies, pMac->ft.ftSmeContext.psavedFTPreAuthRsp->ric_ies, 
                 pMac->ft.ftSmeContext.psavedFTPreAuthRsp->ric_ies_length);

    *ric_ies_length = pMac->ft.ftSmeContext.psavedFTPreAuthRsp->ric_ies_length;

#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
    smsLog( pMac, LOGE, FL(" Filled ric ies = %d\n"), *ric_ies_length);
#endif

    sme_ReleaseGlobalLock( &pMac->sme );
    return;
}

/*--------------------------------------------------------------------------
 *
 * Timer callback for the timer that is started between the preauth completion and 
 * reassoc request to the PE. In this interval, it is expected that the pre-auth response 
 * and RIC IEs are passed up to the WPA supplicant and received back the necessary FTIEs 
 * required to be sent in the reassoc request
 *
 *------------------------------------------------------------------------*/
void sme_PreauthReassocIntvlTimerCallback(void *context)
{
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
    tpAniSirGlobal pMac = (tpAniSirGlobal )context;
    csrNeighborRoamRequestHandoff(pMac);
#endif
    return;
}
/* End of File */
#endif /* WLAN_FEATURE_VOWIFI_11R */
