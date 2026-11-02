#pragma once

// Fallback for Windows SDKs older than 10.0.20348, which don't ship
// <audioclientactivationparams.h>. Definitions match the Microsoft header
// (process-loopback capture, available at runtime on Windows 10 2004+).

#if __has_include(<audioclientactivationparams.h>)
#include <audioclientactivationparams.h>
#else
#include <windows.h>

typedef enum AUDIOCLIENT_ACTIVATION_TYPE
{
    AUDIOCLIENT_ACTIVATION_TYPE_DEFAULT = 0,
    AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK = 1
} AUDIOCLIENT_ACTIVATION_TYPE;

typedef enum PROCESS_LOOPBACK_MODE
{
    PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE = 0,
    PROCESS_LOOPBACK_MODE_EXCLUDE_TARGET_PROCESS_TREE = 1
} PROCESS_LOOPBACK_MODE;

typedef struct AUDIOCLIENT_PROCESS_LOOPBACK_PARAMS
{
    DWORD TargetProcessId;
    PROCESS_LOOPBACK_MODE ProcessLoopbackMode;
} AUDIOCLIENT_PROCESS_LOOPBACK_PARAMS;

typedef struct AUDIOCLIENT_ACTIVATION_PARAMS
{
    AUDIOCLIENT_ACTIVATION_TYPE ActivationType;
    union
    {
        AUDIOCLIENT_PROCESS_LOOPBACK_PARAMS ProcessLoopbackParams;
    };
} AUDIOCLIENT_ACTIVATION_PARAMS;

#define VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK L"VAD\\Process_Loopback"
#endif
