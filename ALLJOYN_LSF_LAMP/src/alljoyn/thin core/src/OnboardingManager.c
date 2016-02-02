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

/**
 * Per-module definition of the current module for debug logging.  Must be defined
 * prior to first inclusion of aj_debug.h.
 * The corresponding flag dbgAJOBS is defined in OnboardingService.h and implemented in OnboardingService.c.
 */
#define AJ_MODULE AJOBS
#include <aj_debug.h>

#include <alljoyn.h>
#include <PropertyStore.h>
#include <ServicesCommon.h>
#include <OnboardingService.h>
#include <OnboardingControllerAPI.h>
//#include <aj_wifi_ctrl.h>
#include <OnboardingManager.h>
//#include <aj_nvram.h>

/**
 * Defines soft AP fallback behavior
 * 0 = Don't go back to soft AP if connection fails (default)
 * 1 = Go back into soft AP if connection fails for the set number of retries
 */
#ifndef AJOBS_SOFTAP_FALLBACK
#define AJOBS_SOFTAP_FALLBACK 0
#endif

/**
 * Configuration
 */
static AJOBS_Info g_obInfo = AJOBS_INFO_DEFAULT;
static uint8_t g_obSoftAPFallback = AJOBS_SOFTAP_FALLBACK;
/**
 * State and Error
 */
static uint8_t g_obState = AJOBS_STATE_NOT_CONFIGURED;
static AJOBS_Error g_obLastError = { AJOBS_STATE_LAST_ERROR_VALIDATED, "" };

/**
 * ScanInfos
 */
static AJ_Time g_obLastScan = { 0, 0 };
static AJOBS_ScanInfo g_obScanInfos[AJOBS_MAX_SCAN_INFOS];
static uint8_t g_obScanInfoCount = 0;

/**
 * Onboarding settings
 */
static const AJOBS_Settings* g_obSettings = NULL;

#define AJ_OBS_OBINFO_NV_ID AJ_OBS_NV_ID_BEGIN
#define AJ_OBS_OBLASTERRORCODE_NV_ID (AJ_OBS_OBINFO_NV_ID + 1)
#define AJ_OBS_SOFTAP_FALLBACK_NV_ID (AJ_OBS_OBINFO_NV_ID + 2)

static AJ_Status OnboardingReadSoftAPFallback(uint8_t* fallback)
{
    AJ_Status status = AJ_OK;
    AJ_NV_DATASET* handle;
    if (!fallback) {
        AJ_ErrPrintf(("Failed to read: fallback is NULL\n"));
        return AJ_ERR_NULL;
    }
    handle = AJ_NVRAM_Open(AJ_OBS_SOFTAP_FALLBACK_NV_ID, "r", 0);
    if (handle != NULL) {
        if (AJ_NVRAM_Read(fallback, 1, handle) != 1) {
            AJ_ErrPrintf(("Failed to read fallback: mismatched size read\n"));
            return AJ_ERR_NVRAM_READ;
        } else {
            status = AJ_NVRAM_Close(handle);
        }
        return AJ_OK;
    }
    return AJ_ERR_NULL;
}

static AJ_Status OnboardingWriteSoftAPFallback(uint8_t fallback)
{
    AJ_Status status = AJ_OK;
    AJ_NV_DATASET* handle = AJ_NVRAM_Open(AJ_OBS_SOFTAP_FALLBACK_NV_ID, "w", 1);
    if (handle != NULL) {
        if (AJ_NVRAM_Write(&fallback, 1, handle) != 1) {
            AJ_ErrPrintf(("Failed to write fallback byte: mismatched size write\n"));
            return AJ_ERR_NVRAM_READ;
        } else {
            status = AJ_NVRAM_Close(handle);
        }
        return AJ_OK;
    }
    return AJ_ERR_NULL;
}

static AJ_Status OnboardingReadInfo(AJOBS_Info* info)
{
    AJ_Status status = AJ_OK;
    size_t size = sizeof(AJOBS_Info);
    AJ_NV_DATASET* nvramHandle;
    int sizeRead;

    if (NULL == info) {
        status = AJ_ERR_NULL;
        AJ_ErrPrintf(("Failed to read info: info buffer is NULL\n"));
        goto ErrorExit;
    }

    if (!AJ_NVRAM_Exist(AJ_OBS_OBINFO_NV_ID)) {
        AJ_WarnPrintf(("Failed to read info: info handle in NVRAM (%d) not found\n", AJ_OBS_OBINFO_NV_ID));
        goto ErrorExit;
    }

    memset(info, 0, size);
    nvramHandle = AJ_NVRAM_Open(AJ_OBS_OBINFO_NV_ID, "r", 0);
    if (nvramHandle != NULL) {
        sizeRead = AJ_NVRAM_Read(info, size, nvramHandle);
        status = AJ_NVRAM_Close(nvramHandle);
        if (sizeRead != sizeRead) {
            status = AJ_ERR_NVRAM_READ;
            AJ_ErrPrintf(("Failed to read info: mismatched size read\n"));
        } else {
            AJ_InfoPrintf(("Read Info values: ssid=%s authType=%d pc=%s lastErrorCode=%d\n", info->ssid, info->authType, info->pc));
        }
    }

ErrorExit:
    return status;
}

static AJ_Status OnboardingWriteInfo(AJOBS_Info* info)
{
    AJ_Status status = AJ_OK;
    size_t size = sizeof(AJOBS_Info);
    AJ_NV_DATASET* nvramHandle;
    int sizeWritten;

    if (NULL == info) {
        status = AJ_ERR_NULL;
        AJ_ErrPrintf(("Failed to write info: info buffer is NULL\n"));
        goto ErrorExit;
    }

    nvramHandle = AJ_NVRAM_Open(AJ_OBS_OBINFO_NV_ID, "w", size);
    if (nvramHandle != NULL) {
        sizeWritten = AJ_NVRAM_Write(info, size, nvramHandle);
        status = AJ_NVRAM_Close(nvramHandle);
        if (sizeWritten != size) {
            status = AJ_ERR_NVRAM_WRITE;
            AJ_ErrPrintf(("Failed to write info: mismatched size written\n"));
        } else {
            AJ_InfoPrintf(("Wrote Info values: ssid=%s authType=%d pc=%s\n", info->ssid, info->authType, info->pc));
        }
    }

ErrorExit:
    return status;
}

static AJ_Status OnboardingReadLastErrorCode(int8_t* lastErrorCode)
{
    AJ_Status status = AJ_OK;
    size_t size = sizeof(int8_t);
    AJ_NV_DATASET* nvramHandle;
    int sizeRead;

    if (NULL == lastErrorCode) {
        status = AJ_ERR_NULL;
        AJ_ErrPrintf(("Failed to read last error code: error code buffer is NULL\n"));
        goto ErrorExit;
    }

    if (!AJ_NVRAM_Exist(AJ_OBS_OBLASTERRORCODE_NV_ID)) {
        AJ_WarnPrintf(("Failed to read last error code: last error code handle in NVRAM (%d) not found\n", AJ_OBS_OBLASTERRORCODE_NV_ID));
        goto ErrorExit;
    }

    nvramHandle = AJ_NVRAM_Open(AJ_OBS_OBLASTERRORCODE_NV_ID, "r", 0);
    if (nvramHandle != NULL) {
        sizeRead = AJ_NVRAM_Read(lastErrorCode, size, nvramHandle);
        status = AJ_NVRAM_Close(nvramHandle);
        if (sizeRead != sizeRead) {
            AJ_ErrPrintf(("Failed to read last error code: mismatched size read\n"));
            status = AJ_ERR_NVRAM_READ;
        } else {
            AJ_InfoPrintf(("Read Last Error Code: lastErrorCode=%d\n", *lastErrorCode));
        }
    }

ErrorExit:
    return status;
}

static AJ_Status OnboardingWriteLastErrorCode(int8_t* lastErrorCode)
{
    AJ_Status status = AJ_OK;
    size_t size = sizeof(int8_t);
    AJ_NV_DATASET* nvramHandle;
    int sizeWritten;

    if (NULL == lastErrorCode) {
        status = AJ_ERR_NULL;
        AJ_ErrPrintf(("Failed to write last error code: error code buffer is NULL\n"));
        goto ErrorExit;
    }

    nvramHandle = AJ_NVRAM_Open(AJ_OBS_OBLASTERRORCODE_NV_ID, "w", size);
    if (nvramHandle != NULL) {
        sizeWritten = AJ_NVRAM_Write(lastErrorCode, size, nvramHandle);
        status = AJ_NVRAM_Close(nvramHandle);
        if (sizeWritten != size) {
            status = AJ_ERR_NVRAM_WRITE;
            AJ_ErrPrintf(("Failed to write last error code: mismatched size written\n"));
        } else {
            AJ_InfoPrintf(("Wrote Last Error Code: lastErrorCode=%d\n", *lastErrorCode));
        }
    }

ErrorExit:
    return status;
}

AJ_Status AJOBS_Start(const AJOBS_Settings* settings)
{
    AJ_Status status = AJ_OK;

    g_obSettings = settings;
    if (g_obSettings == NULL || g_obSettings->AJOBS_SoftAPSSID == NULL || (g_obSettings->AJOBS_SoftAPSSID)[0] == '\0') {
        AJ_ErrPrintf(("AJOBS_Start(): No SoftAP SSID was provided!\n"));
        status = AJ_ERR_INVALID;
    } else if (g_obSettings->AJOBS_SoftAPPassphrase != NULL && strlen(g_obSettings->AJOBS_SoftAPPassphrase) < 8) {
        AJ_ErrPrintf(("AJOBS_Start(): SoftAP Passphrase is too short! Needs to 8 to 63 characters long\n"));
        status = AJ_ERR_INVALID;
    }

    if (status == AJ_OK) {
        status = AJOBS_RegisterObjectList();
    }

    // Check if just started and setup globals from persisted info
    if (status == AJ_OK) {
        status = OnboardingReadInfo(&g_obInfo);
        if (status == AJ_OK) {
            status = OnboardingReadLastErrorCode(&g_obLastError.code);
            g_obLastError.message[0] = '\0';
            if (status == AJ_OK) {
                if (AJOBS_ControllerAPI_IsConfigured()) {
                    switch (g_obLastError.code) {
                    case AJOBS_STATE_LAST_ERROR_VALIDATED:
                    case AJOBS_STATE_LAST_ERROR_UNREACHABLE:
                    case AJOBS_STATE_LAST_ERROR_ERROR_MESSAGE:
                        g_obState = AJOBS_STATE_CONFIGURED_NOT_VALIDATED;
                        break;

                    case AJOBS_STATE_LAST_ERROR_UNSUPPORTED_PROTOCOL:
                    case AJOBS_STATE_LAST_ERROR_UNAUTHORIZED:
                    default:
                        g_obState = AJOBS_STATE_CONFIGURED_ERROR;
                        break;
                    }
                }
            }
        }
    }

    return status;
}

uint8_t AJOBS_IsWiFiConnected()
{
    AJ_WiFiConnectState state = AJ_GetWifiConnectState();

    switch (state) {
    case AJ_WIFI_STATION_OK:
    case AJ_WIFI_CONNECT_OK:
        return TRUE;

    default:
        return FALSE;
    }
}

AJ_Status AJOBS_SetSoftAPFallback(uint8_t fallback)
{
    /*
     * Set the global to the new fallback and write it into NVRAM
     */
    g_obSoftAPFallback = fallback;
    return OnboardingWriteSoftAPFallback(fallback);
}

/**
 * Onboarding state variable.
 */
int8_t AJOBS_GetState()
{
    return g_obState;
}

void AJOBS_SetState(int8_t state)
{
    g_obState = state;
}

/**
 * Onboarding error variable.
 */
const AJOBS_Error* AJOBS_GetError()
{
    return &g_obLastError;
}

/**
 * Onboarding last scan time.
 */
const AJ_Time* AJOBS_GetLastScanTime()
{
    return &g_obLastScan;
}

/**
 * Onboarding scan infos variable.
 */
const AJOBS_ScanInfo* AJOBS_GetScanInfos()
{
    return g_obScanInfos;
}

/**
 * Onboarding scan infos count variable.
 */
uint8_t AJOBS_GetScanInfoCount()
{
    return AJOBS_MAX_SCAN_INFOS;
}

uint8_t AJOBS_IsInfoValid(const char* ssid, const char* pc, int8_t authType)
{
    if ((ssid == NULL) || (strlen(ssid) == 0) || (strlen(ssid) > AJOBS_SSID_MAX_LENGTH)) {
        return FALSE;
    }
    switch (authType) {
    case AJOBS_AUTH_TYPE_OPEN:
        return (pc == NULL) || (strlen(pc) == 0);

    case AJOBS_AUTH_TYPE_WEP:
        return (pc != NULL) && ((strlen(pc) == 5) || (strlen(pc) == 10) || (strlen(pc) == 13) || (strlen(pc) == 26));

    case AJOBS_AUTH_TYPE_WPA_TKIP:
    case AJOBS_AUTH_TYPE_WPA_CCMP:
    case AJOBS_AUTH_TYPE_WPA_AUTO:
    case AJOBS_AUTH_TYPE_WPA2_TKIP:
    case AJOBS_AUTH_TYPE_WPA2_CCMP:
    case AJOBS_AUTH_TYPE_WPA2_AUTO:
        return (pc != NULL) && (strlen(pc) >= 8) && (strlen(pc) <= AJOBS_PASSCODE_MAX_LENGTH);

    case AJOBS_AUTH_TYPE_WPS:
        return (pc != NULL) && (strlen(pc) == 8);

    case AJOBS_AUTH_TYPE_ANY:
        return (pc != NULL) && (strlen(pc) > 0) && (strlen(pc) <= AJOBS_PASSCODE_MAX_LENGTH);

    default:
        return FALSE;
    }
}

AJ_Status AJOBS_UpdateInfo(const char* ssid, const char* pc, int8_t authType) {
    AJ_Status status = AJ_OK;
    size_t ssidLen = 0;
    size_t pcLen = 0;
    uint8_t modified = FALSE;

    if ((g_obInfo.ssid != ssid) && (strcmp(g_obInfo.ssid, ssid) != 0)) {
        ssidLen = min(strlen(ssid), AJOBS_SSID_MAX_LENGTH);
        strncpy(g_obInfo.ssid, ssid, AJOBS_SSID_MAX_LENGTH);
        g_obInfo.ssid[ssidLen] = '\0';
        modified |= TRUE;
    }

    if ((g_obInfo.pc != pc) && (strcmp(g_obInfo.pc, pc) != 0)) {
        pcLen = min(strlen(pc), AJOBS_PASSCODE_MAX_LENGTH);
        strncpy(g_obInfo.pc, pc, AJOBS_PASSCODE_MAX_LENGTH);
        g_obInfo.pc[pcLen] = '\0';
        modified |= TRUE;
    }

    if (g_obInfo.authType != authType) {
        g_obInfo.authType = authType;
        modified |= TRUE;
    }

    if (modified == TRUE) {
        if (g_obInfo.ssid[0] == '\0') {
            g_obState = AJOBS_STATE_NOT_CONFIGURED; // Change state to NOT_CONFIGURED
        } else {
            g_obState = AJOBS_STATE_CONFIGURED_NOT_VALIDATED; // Change state to CONFIGURED_NOT_VALIDATED
        }
        status = OnboardingWriteInfo(&g_obInfo);
    }

    return status;
}

#ifndef AJ_CASE
#define AJ_CASE(_status) case _status: return # _status
#endif

static const char* AJ_WiFiConnectStateText(AJ_WiFiConnectState state)
{
#ifdef NDEBUG
    /* Expectation is that thin client status codes will NOT go beyond 255 */
    static char code[4];

#ifdef _WIN32
    _snprintf(code, sizeof(code), "%03u", state);
#else
    snprintf(code, sizeof(code), "%03u", state);
#endif

    return code;
#else
    switch (state) {
        AJ_CASE(AJ_WIFI_IDLE);
        AJ_CASE(AJ_WIFI_CONNECTING);
        AJ_CASE(AJ_WIFI_CONNECT_OK);
        AJ_CASE(AJ_WIFI_SOFT_AP_INIT);
        AJ_CASE(AJ_WIFI_SOFT_AP_UP);
        AJ_CASE(AJ_WIFI_STATION_OK);
        AJ_CASE(AJ_WIFI_CONNECT_FAILED);
        AJ_CASE(AJ_WIFI_AUTH_FAILED);
        AJ_CASE(AJ_WIFI_DISCONNECTING);

    default:
        return "<unknown>";
    }
#endif
}

uint8_t AJOBS_ControllerAPI_IsWiFiSoftAP()
{
    AJ_WiFiConnectState state = AJ_GetWifiConnectState();
    switch (state) {
    case AJ_WIFI_SOFT_AP_INIT:
    case AJ_WIFI_SOFT_AP_UP:
    case AJ_WIFI_STATION_OK:
        return TRUE;

    case AJ_WIFI_IDLE:
    case AJ_WIFI_CONNECTING:
    case AJ_WIFI_CONNECT_OK:
    case AJ_WIFI_CONNECT_FAILED:
    case AJ_WIFI_AUTH_FAILED:
    case AJ_WIFI_DISCONNECTING:
    default:
        return FALSE;
    }
}

uint8_t AJOBS_ControllerAPI_IsWiFiClient()
{
    AJ_WiFiConnectState state = AJ_GetWifiConnectState();
    switch (state) {
    case AJ_WIFI_CONNECTING:
    case AJ_WIFI_CONNECT_OK:
    case AJ_WIFI_DISCONNECTING:
    case AJ_WIFI_CONNECT_FAILED:
    case AJ_WIFI_AUTH_FAILED:
        return TRUE;

    case AJ_WIFI_IDLE:
    case AJ_WIFI_SOFT_AP_INIT:
    case AJ_WIFI_SOFT_AP_UP:
    case AJ_WIFI_STATION_OK:
    default:
        return FALSE;
    }
}

static int8_t GetAuthType(AJ_WiFiSecurityType secType, AJ_WiFiCipherType cipherType)
{
    switch (secType) {
    case AJ_WIFI_SECURITY_NONE:
        return AJOBS_AUTH_TYPE_OPEN;

    case AJ_WIFI_SECURITY_WEP:
        return AJOBS_AUTH_TYPE_WEP;

    case AJ_WIFI_SECURITY_WPA:
        switch (cipherType) {
        case AJ_WIFI_CIPHER_TKIP:
            return AJOBS_AUTH_TYPE_WPA_TKIP;

        case AJ_WIFI_CIPHER_CCMP:
            return AJOBS_AUTH_TYPE_WPA_CCMP;

        case AJ_WIFI_CIPHER_NONE:
            return AJOBS_AUTH_TYPE_WPA_AUTO;

        default:
            break;
        }
        break;

    case AJ_WIFI_SECURITY_WPA2:
        switch (cipherType) {
        case AJ_WIFI_CIPHER_TKIP:
            return AJOBS_AUTH_TYPE_WPA2_TKIP;

        case AJ_WIFI_CIPHER_CCMP:
            return AJOBS_AUTH_TYPE_WPA2_CCMP;

        case AJ_WIFI_CIPHER_NONE:
            return AJOBS_AUTH_TYPE_WPA2_AUTO;

        default:
            break;
        }
        break;
    }
    return AJOBS_AUTH_TYPE_ANY;
}

static AJ_WiFiSecurityType GetSecType(int8_t authType)
{
    AJ_WiFiSecurityType secType = AJ_WIFI_SECURITY_NONE;

    switch (authType) {
    case AJOBS_AUTH_TYPE_OPEN:
        secType = AJ_WIFI_SECURITY_NONE;
        break;

    case AJOBS_AUTH_TYPE_WEP:
        secType = AJ_WIFI_SECURITY_WEP;
        break;

    case AJOBS_AUTH_TYPE_WPA_TKIP:
    case AJOBS_AUTH_TYPE_WPA_CCMP:
        secType = AJ_WIFI_SECURITY_WPA;
        break;

    case AJOBS_AUTH_TYPE_WPA2_TKIP:
    case AJOBS_AUTH_TYPE_WPA2_CCMP:
    case AJOBS_AUTH_TYPE_WPS:
        secType = AJ_WIFI_SECURITY_WPA2;
        break;

    default:
        break;
    }

    return secType;
}

static AJ_WiFiCipherType GetCipherType(int8_t authType)
{
    AJ_WiFiCipherType cipherType = AJ_WIFI_CIPHER_NONE;

    switch (authType) {
    case AJOBS_AUTH_TYPE_OPEN:
        cipherType = AJ_WIFI_CIPHER_NONE;
        break;

    case AJOBS_AUTH_TYPE_WEP:
        cipherType = AJ_WIFI_CIPHER_WEP;
        break;

    case AJOBS_AUTH_TYPE_WPA_TKIP:
    case AJOBS_AUTH_TYPE_WPA2_TKIP:
        cipherType = AJ_WIFI_CIPHER_TKIP;
        break;

    case AJOBS_AUTH_TYPE_WPS:
    case AJOBS_AUTH_TYPE_WPA_CCMP:
    case AJOBS_AUTH_TYPE_WPA2_CCMP:
        cipherType = AJ_WIFI_CIPHER_CCMP;
        break;

    default:
        break;
    }

    return cipherType;
}

static void GotScanInfo(const char* ssid, const uint8_t bssid[6], uint8_t rssi, AJOBS_AuthType authType)
{
    if (g_obScanInfoCount < AJOBS_MAX_SCAN_INFOS) {
        strncpy(g_obScanInfos[g_obScanInfoCount].ssid, ssid, AJOBS_SSID_MAX_LENGTH);
        g_obScanInfos[g_obScanInfoCount].authType = authType;
        ++g_obScanInfoCount;
    }
}

static void WiFiScanResult(void* context, const char* ssid, const uint8_t bssid[6], uint8_t rssi, AJ_WiFiSecurityType secType, AJ_WiFiCipherType cipherType)
{
    static const char* const sec[] = { "OPEN", "WEP", "WPA", "WPA2" };
    static const char* const typ[] = { "", ":TKIP", ":CCMP", ":WEP" };
    int8_t authType = GetAuthType(secType, cipherType);
    AJ_InfoPrintf(("WiFiScanResult found ssid=%s rssi=%d security=%s%s\n", ssid, rssi, sec[secType], typ[cipherType]));
    GotScanInfo(ssid, bssid, rssi, authType);
}

AJ_Status AJOBS_ControllerAPI_DoScanInfo()
{
    AJ_Status status = AJ_OK;

    memset(g_obScanInfos, 0, sizeof(g_obScanInfos));
    g_obScanInfoCount = 0;
    // Scan neighbouring networks and save info -> Call OBM_WiFiScanResult().
    status = AJ_WiFiScan(NULL, WiFiScanResult, AJOBS_MAX_SCAN_INFOS);
    if (status == AJ_OK) {
        AJ_InitTimer(&g_obLastScan);
    }

    return status;
}

AJ_Status AJOBS_ControllerAPI_GotoIdleWiFi(uint8_t reset)
{
    AJ_Status status = AJ_OK;
    AJ_WiFiConnectState wifiConnectState = AJ_GetWifiConnectState();

    if (wifiConnectState != AJ_WIFI_IDLE) {
        status = AJ_DisconnectWiFi();
        if (reset) {
            status = AJ_ResetWiFi();
        }
        wifiConnectState = AJ_GetWifiConnectState();
    }
    AJ_Sleep(1000);

    while ((wifiConnectState != AJ_WIFI_IDLE) && (wifiConnectState != AJ_WIFI_CONNECT_FAILED) && (wifiConnectState != AJ_WIFI_DISCONNECTING)) {
        wifiConnectState = AJ_GetWifiConnectState();
        AJ_InfoPrintf(("WIFI_CONNECT_STATE: %s\n", AJ_WiFiConnectStateText(wifiConnectState)));
        AJ_Sleep(500);
    }

    return status;
}

static AJ_Status DoConnectWifi()
{
    AJ_Status status = AJ_OK;
    AJ_WiFiSecurityType secType = AJ_WIFI_SECURITY_NONE;
    AJ_WiFiCipherType cipherType = AJ_WIFI_CIPHER_NONE;
    int8_t fallback = AJOBS_AUTH_TYPE_OPEN;
    int8_t fallbackUntil = AJOBS_AUTH_TYPE_OPEN;
    uint8_t retries = 0;
    AJ_WiFiConnectState wifiConnectState;
    int8_t prevLastErrorCode = g_obLastError.code;
    char* password = g_obInfo.pc;
    size_t hexLen = strlen(password);
    uint8_t raw[(AJOBS_PASSCODE_MAX_LENGTH / 2) + 1] = { 0 };
    int8_t prevObState = g_obState;

    /* Only decode the password from HEX if authentication type is NOT WEP */
    if (AJOBS_AUTH_TYPE_WEP != g_obInfo.authType) {
        AJ_HexToRaw(password, hexLen, raw, (AJOBS_PASSCODE_MAX_LENGTH / 2) + 1);
        raw[hexLen / 2] = 0;
    }

    AJ_InfoPrintf(("Attempting to connect to %s with passcode=%s and auth=%d\n", g_obInfo.ssid, g_obInfo.pc, g_obInfo.authType));

    switch (g_obInfo.authType) {
    case AJOBS_AUTH_TYPE_ANY:
        if (strlen(g_obInfo.pc) == 0) {
            break;
        }
        fallback = AJOBS_AUTH_TYPE_WPA2_CCMP;
        fallbackUntil = AJOBS_AUTH_TYPE_OPEN;
        break;

    case AJOBS_AUTH_TYPE_WPA_AUTO:
        if (strlen(g_obInfo.pc) == 0) {
            break;
        }
        fallback = AJOBS_AUTH_TYPE_WPA_CCMP;
        fallbackUntil = AJOBS_AUTH_TYPE_WPA_TKIP;
        break;

    case AJOBS_AUTH_TYPE_WPA2_AUTO:
        if (strlen(g_obInfo.pc) == 0) {
            break;
        }
        fallback = AJOBS_AUTH_TYPE_WPA2_CCMP;
        fallbackUntil = AJOBS_AUTH_TYPE_WPA2_TKIP;
        break;

    default:
        fallback = g_obInfo.authType;
    }

    secType = GetSecType(fallback);
    cipherType = GetCipherType(fallback);
    AJ_InfoPrintf(("Trying to connect with auth=%d (secType=%d, cipherType=%d)\n", fallback, secType, cipherType));

    while (1) {
        // Setup the password
        if ((AJ_WIFI_SECURITY_WEP == secType) && ((hexLen / 2) & 1)) { // Ensure that the HEX encoded password is passed for WEP when raw password is not HEX
            password = g_obInfo.pc;
        } else {
            password = (char*)raw;
        }

        if (prevObState == AJOBS_STATE_CONFIGURED_NOT_VALIDATED) {
            g_obLastError.code = AJOBS_STATE_LAST_ERROR_VALIDATED;
            g_obLastError.message[0] = '\0';
            g_obState = AJOBS_STATE_CONFIGURED_VALIDATING;
        }

        status = AJ_ConnectWiFi(g_obInfo.ssid, secType, cipherType, password);
        AJ_InfoPrintf(("AJ_ConnectWifi returned %s\n", AJ_StatusText(status)));

        wifiConnectState = AJ_GetWifiConnectState();

        // Set last error and state
        if ((status == AJ_OK) /* (wifiConnectState == AJ_WIFI_CONNECT_OK)*/) {
            g_obLastError.code = AJOBS_STATE_LAST_ERROR_VALIDATED;
            g_obLastError.message[0] = '\0';
            g_obState = AJOBS_STATE_CONFIGURED_VALIDATED;
            break; // Leave retry loop
        } else if ((status == AJ_ERR_CONNECT) /* (wifiConnectState == AJ_WIFI_CONNECT_FAILED)*/) {
            g_obLastError.code = AJOBS_STATE_LAST_ERROR_UNREACHABLE;
            strncpy(g_obLastError.message, "Network unreachable!", AJOBS_ERROR_MESSAGE_LEN);
            g_obState = AJOBS_STATE_CONFIGURED_RETRY;
        } else if ((status == AJ_ERR_SECURITY) /* (wifiConnectState == AJ_WIFI_AUTH_FAILED)*/) {
            g_obLastError.code = AJOBS_STATE_LAST_ERROR_UNAUTHORIZED;
            strncpy(g_obLastError.message, "Authorization failed!", AJOBS_ERROR_MESSAGE_LEN);
            g_obState = AJOBS_STATE_CONFIGURED_ERROR;
        } else if (status == AJ_ERR_DRIVER) {
            g_obLastError.code = AJOBS_STATE_LAST_ERROR_ERROR_MESSAGE;
            strncpy(g_obLastError.message, "Driver error!", AJOBS_ERROR_MESSAGE_LEN);
            g_obState = AJOBS_STATE_CONFIGURED_ERROR;
        } else if (status == AJ_ERR_DHCP) {
            g_obLastError.code = AJOBS_STATE_LAST_ERROR_ERROR_MESSAGE;
            strncpy(g_obLastError.message, "Failed to establish IP!", AJOBS_ERROR_MESSAGE_LEN);
            g_obState = AJOBS_STATE_CONFIGURED_RETRY;
        } else { /* if (status == AJ_ERR_UNKNOWN) */
            g_obLastError.code = AJOBS_STATE_LAST_ERROR_ERROR_MESSAGE;
            strncpy(g_obLastError.message, "Failed to connect! Unexpected error", AJOBS_ERROR_MESSAGE_LEN);
            g_obState = AJOBS_STATE_CONFIGURED_ERROR;
        }
        // Treat any errors on first validation as terminal and do NOT switch to RETRY mode
        if (prevObState == AJOBS_STATE_CONFIGURED_NOT_VALIDATED) {
            g_obState = AJOBS_STATE_CONFIGURED_ERROR;
        }

        AJ_WarnPrintf(("Warning - DoConnectWifi wifiConnectState = %s\n", AJ_WiFiConnectStateText(wifiConnectState)));
        AJ_WarnPrintf(("Last error set to \"%s\" (code=%d)\n", g_obLastError.message, g_obLastError.code));

        // Check if retry limit reached
        if (retries++ >= g_obSettings->AJOBS_MAX_RETRIES) {
            // Check whether in a automatic authentication type fallback loop
            if (g_obInfo.authType < 0) {
                // Check whether there is another fallback to try
                if (fallback > fallbackUntil) {
                    fallback--; // Try next authentication protocol
                    secType = GetSecType(fallback);
                    cipherType = GetCipherType(fallback);
                    retries = 0;
                    AJ_InfoPrintf(("Trying to connect with fallback auth=%d (secType=%d, cipherType=%d)\n", fallback, secType, cipherType));
                    continue;
                }
                g_obLastError.code = AJOBS_STATE_LAST_ERROR_UNSUPPORTED_PROTOCOL;
                strncpy(g_obLastError.message, "Unsupported protocol", AJOBS_ERROR_MESSAGE_LEN);
                g_obState = AJOBS_STATE_CONFIGURED_ERROR;
                AJ_WarnPrintf(("Warning - all fallbacks were exhausted\n"));
                AJ_WarnPrintf(("Last error set to \"%s\" (code=%d)\n", g_obLastError.message, g_obLastError.code));
            }
            break; // Leave retry loop
        }
        AJ_InfoPrintf(("Retry number %d out of %d\n", retries, g_obSettings->AJOBS_MAX_RETRIES));
    }

    // If succeeded through a fallback loop set the successful authType
    if ((g_obState == AJOBS_STATE_CONFIGURED_VALIDATED) && (g_obInfo.authType != fallback)) {
        g_obInfo.authType = fallback;
        OnboardingWriteInfo(&g_obInfo);
    }

    // If error code changed persist it
    if (g_obLastError.code != prevLastErrorCode) {
        OnboardingWriteLastErrorCode(&g_obLastError.code);
    }

    return status;
}

AJ_Status AJOBS_ControllerAPI_StartSoftAPIfNeededOrConnect()
{
    AJ_Status status = AJ_OK;

    // Check if there is Wi-Fi connectivity and return if established already
    if (AJOBS_IsWiFiConnected()) {
        AJ_InfoPrintf(("Already connected with ConnectionState=%u, no need to establish connection!\n", (uint8_t)AJ_GetWifiConnectState()));
        return status;
    }
    while (1) {
        status = AJOBS_ControllerAPI_GotoIdleWiFi(g_obSettings->AJOBS_RESET_WIFI_ON_IDLE); // Go into IDLE mode, reset Wi-Fi
        if (status != AJ_OK) {
            break;
        }
        status = AJOBS_ControllerAPI_DoScanInfo(); // Perform scan of available Wi-Fi networks
        if (status != AJ_OK) {
            break;
        }
        // Check if require to switch into SoftAP mode.
        if ((g_obState == AJOBS_STATE_NOT_CONFIGURED) || ((g_obState == AJOBS_STATE_CONFIGURED_ERROR || g_obState == AJOBS_STATE_CONFIGURED_RETRY) && g_obSoftAPFallback)) {
            AJ_InfoPrintf(("Establishing SoftAP with ssid=%s%s auth=%s\n", g_obSettings->AJOBS_SoftAPSSID, (g_obSettings->AJOBS_SoftAPIsHidden ? " (hidden)" : ""), g_obSettings->AJOBS_SoftAPPassphrase == NULL ? "OPEN" : g_obSettings->AJOBS_SoftAPPassphrase));
            if (g_obState == AJOBS_STATE_CONFIGURED_RETRY) {
                AJ_InfoPrintf(("Retry timer activated\n"));
                status = AJ_EnableSoftAP(g_obSettings->AJOBS_SoftAPSSID, g_obSettings->AJOBS_SoftAPIsHidden, g_obSettings->AJOBS_SoftAPPassphrase, g_obSettings->AJOBS_WAIT_BETWEEN_RETRIES);
            } else {
                status = AJ_EnableSoftAP(g_obSettings->AJOBS_SoftAPSSID, g_obSettings->AJOBS_SoftAPIsHidden, g_obSettings->AJOBS_SoftAPPassphrase, g_obSettings->AJOBS_WAIT_FOR_SOFTAP_CONNECTION);
            }
            if (status != AJ_OK) {
                AJ_WarnPrintf(("Failed to establish connection in SoftAP status=%s\n", AJ_StatusText(status)));
                if (AJ_ERR_TIMEOUT == status) {
                    // Check for timer elapsed for retry
                    if (g_obState == AJOBS_STATE_CONFIGURED_RETRY) {
                        AJ_InfoPrintf(("Retry timer elapsed at %ums\n", g_obSettings->AJOBS_WAIT_BETWEEN_RETRIES));
                        g_obState = AJOBS_STATE_CONFIGURED_VALIDATED;
                        continue; // Loop back and connect in client mode
                    }
                }
            }
        } else { // Otherwise connect to given configuration and according to error code returned map to relevant onboarding state and set value for LastError and ConnectionResult.
            status = DoConnectWifi();
            if (status != AJ_OK) {
                AJ_WarnPrintf(("Failed to establish connection with current configuration status=%s\n", AJ_StatusText(status)));
            }
            if (g_obState == AJOBS_STATE_CONFIGURED_ERROR || g_obState == AJOBS_STATE_CONFIGURED_RETRY) {
                continue; // Loop back and establish SoftAP mode
            }
        }
        break; // Either connected to (as client) or connected from (as SoftAP) a station
    }

    return status;
}

AJ_Status AJOBS_ClearInfo()
{
    AJ_Status status;

    // Clear last error
    g_obLastError.message[0] = '\0';
    if (g_obLastError.code != AJOBS_STATE_LAST_ERROR_VALIDATED) {
        g_obLastError.code = AJOBS_STATE_LAST_ERROR_VALIDATED;
        status = OnboardingWriteLastErrorCode(&g_obLastError.code);
        if (status != AJ_OK) {
            goto ErrorExit;
        }
    }

    // Clear info
    status = AJOBS_UpdateInfo("", "", AJOBS_AUTH_TYPE_OPEN);
    if (status != AJ_OK) {
        goto ErrorExit;
    }

    // Clear state
    g_obState = AJOBS_STATE_NOT_CONFIGURED;

    AJ_InfoPrintf(("ClearInfo status: %s\n", AJ_StatusText(status)));
    return status;

ErrorExit:

    AJ_ErrPrintf(("ClearInfo status: %s\n", AJ_StatusText(status)));
    return status;
}

AJ_Status AJOBS_ControllerAPI_DoOffboardWiFi()
{
    return (AJOBS_ClearInfo());
}

uint8_t AJOBS_ControllerAPI_IsConfigured()
{
    return g_obInfo.ssid[0] != '\0';
}

uint8_t AJOBS_ControllerAPI_IsConnected()
{
    return AJOBS_IsWiFiConnected();
}

AJ_Status AJOBS_EstablishWiFi()
{
    AJ_Status status;

    status = AJOBS_ControllerAPI_StartSoftAPIfNeededOrConnect();
    if (status != AJ_OK) {
        goto ErrorExit;
    }

    AJ_InfoPrintf(("EstablishWiFi status: %s\n", AJ_StatusText(status)));
    return status;

ErrorExit:

    AJ_ErrPrintf(("EstablishWiFi status: %s\n", AJ_StatusText(status)));
    return status;
}

AJ_Status AJOBS_SwitchToRetry()
{
    AJ_Status status = AJ_OK;

    g_obState = AJOBS_STATE_CONFIGURED_RETRY;
    status = AJOBS_DisconnectWiFi();
    if (status != AJ_OK) {
        goto ErrorExit;
    }

    AJ_InfoPrintf(("SwitchToRetry status: %s\n", AJ_StatusText(status)));
    return status;

ErrorExit:
    AJ_ErrPrintf(("SwitchToRetry status: %s\n", AJ_StatusText(status)));
    return status;
}

AJ_Status AJOBS_DisconnectWiFi()
{
    AJ_Status status = AJ_OK;

    status = AJOBS_ControllerAPI_GotoIdleWiFi(g_obSettings->AJOBS_RESET_WIFI_ON_IDLE);

    return status;
}
