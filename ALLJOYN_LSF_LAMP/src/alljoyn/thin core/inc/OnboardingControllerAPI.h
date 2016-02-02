/******************************************************************************
 * Copyright AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#ifndef _ONBOARDINGCONTROLLERAPI_H_
#define _ONBOARDINGCONTROLLERAPI_H_

/** @defgroup ControllerAPI Controller API
 *
 *  @{
 */

#include <alljoyn.h>
#include <OnboardingManager.h>

/**
 * Onboarding get state.
 * @return state
 */
int8_t AJOBS_ControllerAPI_GetState();

/**
 * Onboarding set state.
 * @param state
 */
void AJOBS_ControllerAPI_SetState(int8_t state);

/**
 * Onboarding last error variable.
 * @return an error structure filled in
 */
const AJOBS_Error* AJOBS_ControllerAPI_GetError();

/**
 * Onboarding last scan time.
 * @return time of last scan
 */
const AJ_Time* AJOBS_ControllerAPI_GetLastScanTime();

/**
 * Onboarding scan infos variable.
 * @return an array of scanInfo structures filled in by last scan.
 */
const AJOBS_ScanInfo* AJOBS_ControllerAPI_GetScanInfos();

/**
 * Onboarding scan infos count variable.
 * @return number of scanInfos if last saved scan
 */
uint8_t AJOBS_ControllerAPI_GetScanInfoCount();

/**
 * returns whether there is a current configuration
 * @return success
 */
uint8_t AJOBS_ControllerAPI_IsConfigured();

/**
 * perform a check of the current configuration state and accordingly take the relevant action of
 * either establishing SoftAP mode if not configured
 * or attempt to connect using the current connection info.
 * @return status
 */
AJ_Status AJOBS_ControllerAPI_StartSoftAPIfNeededOrConnect();

/**
 * disconnect from current mode (SoftAP or client) and go to idle mode.
 * @param reset
 * @return status
 */
AJ_Status AJOBS_ControllerAPI_GotoIdleWiFi(uint8_t reset);

/**
 * perform a offboarding clearing the current configuration state and values.
 * @return status
 */
AJ_Status AJOBS_ControllerAPI_DoOffboardWiFi();

/**
 * perform a scaninfo.
 * @return status
 */
AJ_Status AJOBS_ControllerAPI_DoScanInfo();

/**
 * return whether the Wi-Fi is in SoftAP mode.
 * @return success
 */
uint8_t AJOBS_ControllerAPI_IsWiFiSoftAP();

/**
 * return whether the Wi-Fi is in client mode.
 * @return success
 */
uint8_t AJOBS_ControllerAPI_IsWiFiClient();

/**
 * return whether the Wi-Fi is connected.
 * @return success
 */
uint8_t AJOBS_ControllerAPI_IsConnected();

/** @} */ // End of group 'ControllerAPI'
 #endif // _ONBOARDINGCONTROLLERAPI_H_
