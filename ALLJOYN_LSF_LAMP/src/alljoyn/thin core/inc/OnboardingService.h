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

#ifndef _ONBOARDINGSERVICE_H_
#define _ONBOARDINGSERVICE_H_

/** @defgroup OnboardingService Onboarding Service Framework
 *
 *  @{
 */

#include <alljoyn.h>
#include <ServicesCommon.h>

/**
 * Turn on per-module debug printing by setting this variable to non-zero value
 * (usually in debugger).
 */
#ifndef NDEBUG
extern uint8_t dbgAJOBS;
#endif

/**
 * Published Onboarding BusObjects.
 */

extern AJ_Object AJOBS_ObjectList[];                  /**< onboarding objects */

/**
 * Register the Onboarding Service Bus Objects list
 * @return aj_status
 */
AJ_Status AJOBS_RegisterObjectList();

#define AJOBS_SSID_MAX_LENGTH  32                     /**< ssid max length */

/*
 * Onboarding Service Message Handlers
 */

/**
 * Handler for property getters associated with org.alljoyn.Onboarding.
 * @param replyMsg
 * @param propId
 * @param context
 * @return aj_status
 */
AJ_Status AJOBS_PropGetHandler(AJ_Message* replyMsg, uint32_t propId, void* context);

/**
 * Handler for property setters associated with org.alljoyn.Onboarding.
 * @param replyMsg
 * @param propId
 * @param context
 * @return aj_status
 */
AJ_Status AJOBS_PropSetHandler(AJ_Message* replyMsg, uint32_t propId, void* context);

/**
 * Handler for ConfigureWiFi request in org.alljoyn.Onboarding.
 * @param msg
 * @return aj_status
 */
AJ_Status AJOBS_ConfigureWiFiHandler(AJ_Message* msg);

/**
 * Handler for ConnectWiFi request in org.alljoyn.Onboarding.
 * @param msg
 * @return aj_status
 */
AJ_Status AJOBS_ConnectWiFiHandler(AJ_Message* msg);

/**
 * Handler for OffboardWiFi request in org.alljoyn.Onboarding.
 * @param msg
 * @return aj_status
 */
AJ_Status AJOBS_OffboardWiFiHandler(AJ_Message* msg);

/**
 * Handler for GetScanInfo request in org.alljoyn.Onboarding.
 * @param msg
 * @return aj_status
 */
AJ_Status AJOBS_GetScanInfoHandler(AJ_Message* msg);

/*
   //will be used in future versions
   AJ_Status AJOBS_SendConnectionResult(AJ_BusAttachment* bus);
 */

/**
 * Called when Routing Node is connected.
 * @param busAttachment
 * @return aj_status
 */
AJ_Status AJOBS_ConnectedHandler(AJ_BusAttachment* busAttachment);

/**
 * Called just before disconnecting from the Router Node.
 * @param busAttachment
 * @return aj_status
 */
AJ_Status AJOBS_DisconnectHandler(AJ_BusAttachment* busAttachment);

/**
 * Called when a new incoming message requires processing.
 * @param busAttachment
 * @param msg
 * @param msgStatus
 * @return service_Status
 */
AJSVC_ServiceStatus AJOBS_MessageProcessor(AJ_BusAttachment* busAttachment, AJ_Message* msg, AJ_Status* msgStatus);

/** @} */ //End of group 'OnboardingService'
 #endif // _ONBOARDINGSERVICE_H_
