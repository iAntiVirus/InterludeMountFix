// BUILD_TAG: IMF_ATTACH_SEAT_FINAL_R1
//
// Lineage II Interlude / Win32 x86 / Release / /MT
//
// Goal
// ----
// Choose the rider attachment bone per mount NPC directly from MountSeats.ini.
//
// Example:
//   [Mount.41001]
//   Enabled=1
//   SeatBone=Dummy01
//
//   [Mount.41005]
//   Enabled=1
//   SeatBone=Dummy05
//
// Duplicate SeatBone values are allowed.
// Any positive NPC ID listed in [MountList] is supported.
//
// Verified path in the supplied Engine.dll:
//   APawn::RiderEnter()
//      -> creates the mount actor
//      -> stores mount NPC ID at mountActor+0x69C
//      -> constructs FName("Bone15")
//      -> mountActor->AttachToBone(riderActor, Bone15, 0)
//
// This DLL hooks only:
//   AActor::AttachToBone(AActor*, FName, int)
//
// For a configured mount:
//   original Bone15 -> INI SeatBone
//
// No F8.
// No GetBonePosition hook.
// No duplicate-bone rejection.
// No legacy CRT float parser.
//
// X/Y/Z are loaded and logged for INI compatibility, but this build does not
// apply an additional local XYZ offset. The actual seat location is selected
// by SeatBone.
//
// Runtime files:
//   MountSeats.ini
//   InterludeMountFix.log
//

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#pragma comment(linker, "/EXPORT:L2UI_Init=_L2UI_Init@0")

// -----------------------------------------------------------------------------
// Engine-compatible types
// -----------------------------------------------------------------------------

typedef uint32_t FNameLite;

typedef void(__thiscall* L2FNameCtorFn)(
    FNameLite* self,
    const wchar_t* name,
    int findType);

typedef int(__thiscall* AttachToBoneFn)(
    void* self,
    void* actorToAttach,
    FNameLite bone,
    int flag);

// -----------------------------------------------------------------------------
// Verified supplied Engine.dll
// -----------------------------------------------------------------------------

static const DWORD kEngineTimestamp = 0x46DBE989u;
static const DWORD kEngineImageSize = 0x01B0D000u;

// ?AttachToBone@AActor@@QAEHPAV1@VFName@@H@Z
static const char* kAttachToBoneExport =
"?AttachToBone@AActor@@QAEHPAV1@VFName@@H@Z";

// Export thunk resolves to Engine base + 0x22E3E0.
static const DWORD kAttachToBoneImplRva = 0x0022E3E0u;

// Verified first 5 whole bytes:
//   83 EC 0C    sub esp, 0Ch
//   56          push esi
//   57          push edi
static const BYTE kAttachToBoneSig[5] =
{
    0x83, 0xEC, 0x0C, 0x56, 0x57
};

static const char* kL2FNameCtorExport =
"??0L2FName@@QAE@PBGW4EFindName@@@Z";

// RiderEnter stores the normalized mount NPC ID here before AttachToBone().
static const DWORD kRiderNpcIdOffset = 0x0000069Cu;

// Original RiderEnter attachment bone in this Engine.dll.
static const wchar_t* kOriginalRiderBoneName = L"Bone15";

// -----------------------------------------------------------------------------
// Limits / structures
// -----------------------------------------------------------------------------

static const int kMaxMounts = 128;

struct MountPreset
{
    BOOL enabled;
    int npcId;

    char section[64];
    char codeName[128];
    char meshName[128];

    char boneNameA[128];
    wchar_t boneNameW[128];
    FNameLite boneIndex;

    // Loaded only for INI compatibility / logging in this build.
    float X;
    float Y;
    float Z;

    volatile LONG hits;
};

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------

static HMODULE gSelfModule = NULL;

static char gDllDir[MAX_PATH] = { 0 };
static char gIniPath[MAX_PATH] = { 0 };
static char gLogPath[MAX_PATH] = { 0 };

static HANDLE gLog = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION gLogCs;
static volatile LONG gLogCsReady = 0;

static volatile LONG gStarted = 0;
static volatile LONG gInstalled = 0;

static MountPreset gMounts[kMaxMounts];
static int gMountCount = 0;

static BOOL gLogPresetHits = TRUE;
static int gMaxInitialHitLogs = 20;
static int gPeriodicHitLogEvery = 500;

static FNameLite gOriginalRiderBone = 0;

static AttachToBoneFn gOriginalAttachToBone = NULL;
static void* gAttachTrampoline = NULL;

// -----------------------------------------------------------------------------
// Paths / logging
// -----------------------------------------------------------------------------

static void BuildPaths()
{
    char full[MAX_PATH] = { 0 };

    if (!gSelfModule ||
        !GetModuleFileNameA(
            gSelfModule,
            full,
            MAX_PATH))
    {
        lstrcpyA(gDllDir, ".");
    }
    else
    {
        lstrcpynA(
            gDllDir,
            full,
            MAX_PATH);

        char* slash1 =
            strrchr(
                gDllDir,
                '\\');

        char* slash2 =
            strrchr(
                gDllDir,
                '/');

        char* slash = slash1;

        if (slash2 &&
            (!slash || slash2 > slash))
        {
            slash = slash2;
        }

        if (slash)
            *slash = '\0';
        else
            lstrcpyA(gDllDir, ".");
    }

    _snprintf_s(
        gIniPath,
        sizeof(gIniPath),
        _TRUNCATE,
        "%s\\MountSeats.ini",
        gDllDir);

    _snprintf_s(
        gLogPath,
        sizeof(gLogPath),
        _TRUNCATE,
        "%s\\InterludeMountFix.log",
        gDllDir);
}

static void InitLogLock()
{
    if (InterlockedCompareExchange(
        &gLogCsReady,
        1,
        0) == 0)
    {
        InitializeCriticalSection(
            &gLogCs);
    }
}

static void InitLog()
{
    InitLogLock();

    EnterCriticalSection(
        &gLogCs);

    if (gLog == INVALID_HANDLE_VALUE)
    {
        gLog =
            CreateFileA(
                gLogPath[0]
                ? gLogPath
                : "InterludeMountFix.log",
                GENERIC_WRITE,
                FILE_SHARE_READ |
                FILE_SHARE_WRITE,
                NULL,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL);
    }

    LeaveCriticalSection(
        &gLogCs);
}

static void Log(
    const char* fmt,
    ...)
{
    InitLog();

    if (gLog == INVALID_HANDLE_VALUE)
        return;

    EnterCriticalSection(
        &gLogCs);

    char body[2048] = { 0 };

    va_list ap;
    va_start(ap, fmt);

    _vsnprintf_s(
        body,
        sizeof(body),
        _TRUNCATE,
        fmt,
        ap);

    va_end(ap);

    SYSTEMTIME st;
    GetLocalTime(&st);

    char line[2304] = { 0 };

    int len =
        _snprintf_s(
            line,
            sizeof(line),
            _TRUNCATE,
            "[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s\r\n",
            st.wYear,
            st.wMonth,
            st.wDay,
            st.wHour,
            st.wMinute,
            st.wSecond,
            st.wMilliseconds,
            body);

    if (len > 0)
    {
        DWORD written = 0;

        WriteFile(
            gLog,
            line,
            (DWORD)len,
            &written,
            NULL);

        FlushFileBuffers(gLog);
    }

    LeaveCriticalSection(
        &gLogCs);
}

// -----------------------------------------------------------------------------
// INI helpers
// -----------------------------------------------------------------------------

static void ReadIniString(
    const char* section,
    const char* key,
    const char* fallback,
    char* out,
    DWORD outSize)
{
    GetPrivateProfileStringA(
        section,
        key,
        fallback,
        out,
        outSize,
        gIniPath);
}

static float ReadIniFloat(
    const char* section,
    const char* key,
    float fallback)
{
    char def[64] = { 0 };
    char value[64] = { 0 };

    _snprintf_s(
        def,
        sizeof(def),
        _TRUNCATE,
        "%.6f",
        fallback);

    GetPrivateProfileStringA(
        section,
        key,
        def,
        value,
        sizeof(value),
        gIniPath);

    float parsed = fallback;

    if (sscanf_s(
        value,
        "%f",
        &parsed) != 1)
    {
        return fallback;
    }

    return parsed;
}

static bool ToWide(
    const char* src,
    wchar_t* dst,
    int dstCount)
{
    if (!src ||
        !src[0] ||
        !dst ||
        dstCount <= 1)
    {
        return false;
    }

    int n =
        MultiByteToWideChar(
            CP_ACP,
            0,
            src,
            -1,
            dst,
            dstCount);

    return n > 0;
}

// -----------------------------------------------------------------------------
// PE / export helpers
// -----------------------------------------------------------------------------

static bool ReadEngineInfo(
    HMODULE engine,
    DWORD& timestamp,
    DWORD& imageSize)
{
    if (!engine)
        return false;

    __try
    {
        BYTE* base =
            (BYTE*)engine;

        IMAGE_DOS_HEADER* dos =
            (IMAGE_DOS_HEADER*)base;

        if (dos->e_magic !=
            IMAGE_DOS_SIGNATURE)
        {
            return false;
        }

        IMAGE_NT_HEADERS32* nt =
            (IMAGE_NT_HEADERS32*)
            (base + dos->e_lfanew);

        if (nt->Signature !=
            IMAGE_NT_SIGNATURE)
        {
            return false;
        }

        if (nt->FileHeader.Machine !=
            IMAGE_FILE_MACHINE_I386)
        {
            return false;
        }

        timestamp =
            nt->FileHeader.TimeDateStamp;

        imageSize =
            nt->OptionalHeader.SizeOfImage;

        return true;
    }
    __except (
        EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void* ResolveExportJump(
    HMODULE engine,
    const char* name)
{
    FARPROC p =
        GetProcAddress(
            engine,
            name);

    if (!p)
        return NULL;

    BYTE* stub =
        (BYTE*)p;

    __try
    {
        if (stub[0] == 0xE9)
        {
            int32_t rel =
                *(int32_t*)
                (stub + 1);

            return
                stub +
                5 +
                rel;
        }

        return p;
    }
    __except (
        EXCEPTION_EXECUTE_HANDLER)
    {
        return NULL;
    }
}

static bool MakeFName(
    HMODULE engine,
    const wchar_t* name,
    FNameLite* outIndex)
{
    if (!engine ||
        !name ||
        !outIndex)
    {
        return false;
    }

    FARPROC ctor =
        GetProcAddress(
            engine,
            kL2FNameCtorExport);

    if (!ctor)
        return false;

    FNameLite value = 0;

    __try
    {
        L2FNameCtorFn makeName =
            (L2FNameCtorFn)ctor;

        // Same Interlude behavior used by the previous working source.
        makeName(
            &value,
            name,
            1);
    }
    __except (
        EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    *outIndex = value;
    return true;
}

// -----------------------------------------------------------------------------
// Mount configuration
// -----------------------------------------------------------------------------

static MountPreset* FindMountPreset(
    int npcId)
{
    if (npcId <= 0)
        return NULL;

    for (int i = 0;
        i < gMountCount;
        ++i)
    {
        MountPreset& p =
            gMounts[i];

        if (p.enabled &&
            p.npcId == npcId)
        {
            return &p;
        }
    }

    return NULL;
}

static bool LoadMountConfig(
    HMODULE engine)
{
    ZeroMemory(
        gMounts,
        sizeof(gMounts));

    gMountCount = 0;

    gLogPresetHits =
        GetPrivateProfileIntA(
            "General",
            "LogPresetHits",
            1,
            gIniPath)
        ? TRUE
        : FALSE;

    gMaxInitialHitLogs =
        GetPrivateProfileIntA(
            "General",
            "MaxInitialHitLogs",
            20,
            gIniPath);

    if (gMaxInitialHitLogs < 0)
        gMaxInitialHitLogs = 0;

    gPeriodicHitLogEvery =
        GetPrivateProfileIntA(
            "General",
            "PeriodicHitLogEvery",
            500,
            gIniPath);

    if (gPeriodicHitLogEvery < 0)
        gPeriodicHitLogEvery = 0;

    int declared =
        GetPrivateProfileIntA(
            "General",
            "MountCount",
            0,
            gIniPath);

    if (declared < 0)
        declared = 0;

    if (declared > kMaxMounts)
        declared = kMaxMounts;

    Log(
        "INI = %s",
        gIniPath);

    Log(
        "Declared MountCount = %d",
        declared);

    for (int i = 0;
        i < declared;
        ++i)
    {
        char key[32] = { 0 };
        char idText[32] = { 0 };

        _snprintf_s(
            key,
            sizeof(key),
            _TRUNCATE,
            "Mount%d",
            i);

        ReadIniString(
            "MountList",
            key,
            "",
            idText,
            sizeof(idText));

        if (!idText[0])
        {
            Log(
                "WARNING: [MountList] %s is empty.",
                key);

            continue;
        }

        int npcId = 0;

        if (sscanf_s(
            idText,
            "%d",
            &npcId) != 1)
        {
            npcId = 0;
        }

        if (npcId <= 0)
        {
            Log(
                "WARNING: [MountList] %s has invalid id '%s'.",
                key,
                idText);

            continue;
        }

        MountPreset temp;
        ZeroMemory(
            &temp,
            sizeof(temp));

        temp.npcId =
            npcId;

        _snprintf_s(
            temp.section,
            sizeof(temp.section),
            _TRUNCATE,
            "Mount.%d",
            npcId);

        temp.enabled =
            GetPrivateProfileIntA(
                temp.section,
                "Enabled",
                0,
                gIniPath)
            ? TRUE
            : FALSE;

        ReadIniString(
            temp.section,
            "CodeName",
            "",
            temp.codeName,
            sizeof(temp.codeName));

        ReadIniString(
            temp.section,
            "Mesh",
            "",
            temp.meshName,
            sizeof(temp.meshName));

        ReadIniString(
            temp.section,
            "SeatBone",
            "",
            temp.boneNameA,
            sizeof(temp.boneNameA));

        temp.X =
            ReadIniFloat(
                temp.section,
                "X",
                0.0f);

        temp.Y =
            ReadIniFloat(
                temp.section,
                "Y",
                0.0f);

        temp.Z =
            ReadIniFloat(
                temp.section,
                "Z",
                0.0f);

        if (!temp.enabled)
        {
            Log(
                "SKIP %s | NPC=%d | %s | disabled",
                temp.section,
                temp.npcId,
                temp.codeName);

            continue;
        }

        if (!temp.boneNameA[0])
        {
            Log(
                "WARNING: %s enabled but SeatBone= is empty.",
                temp.section);

            continue;
        }

        if (!ToWide(
            temp.boneNameA,
            temp.boneNameW,
            (int)
            (sizeof(temp.boneNameW) /
                sizeof(temp.boneNameW[0]))))
        {
            Log(
                "WARNING: %s invalid SeatBone '%s'.",
                temp.section,
                temp.boneNameA);

            continue;
        }

        if (!MakeFName(
            engine,
            temp.boneNameW,
            &temp.boneIndex))
        {
            Log(
                "WARNING: %s could not resolve FName '%s'.",
                temp.section,
                temp.boneNameA);

            continue;
        }

        if (gMountCount >=
            kMaxMounts)
        {
            break;
        }

        gMounts[gMountCount] =
            temp;

        Log(
            "ACTIVE %s | NPC=%d | Name=%s | Mesh=%s | "
            "SeatBone=%s | FName=0x%08X | "
            "XYZ=(%.3f, %.3f, %.3f) [XYZ reserved]",
            temp.section,
            temp.npcId,
            temp.codeName,
            temp.meshName,
            temp.boneNameA,
            temp.boneIndex,
            temp.X,
            temp.Y,
            temp.Z);

        ++gMountCount;
    }

    Log(
        "Active mount presets = %d",
        gMountCount);

    return true;
}

// -----------------------------------------------------------------------------
// Rider mount ID
// -----------------------------------------------------------------------------

static int GetMountNpcIdFromActor(
    void* mountActor,
    int* rawOut)
{
    if (rawOut)
        *rawOut = 0;

    if (!mountActor)
        return 0;

    __try
    {
        int raw =
            *(int*)
            ((BYTE*)mountActor +
                kRiderNpcIdOffset);

        if (rawOut)
            *rawOut = raw;

        int npcId = raw;

        // Defensive support if a +1000000 form is ever seen.
        if (npcId >= 1000000 &&
            npcId < 2000000)
        {
            npcId -= 1000000;
        }

        if (npcId <= 0)
            return 0;

        return npcId;
    }
    __except (
        EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

// -----------------------------------------------------------------------------
// AttachToBone hook
// -----------------------------------------------------------------------------

static int __fastcall HookAttachToBone(
    void* self,
    void* /*edx*/,
    void* actorToAttach,
    FNameLite bone,
    int flag)
{
    if (!gOriginalAttachToBone)
        return 0;

    FNameLite selectedBone =
        bone;

    MountPreset* preset =
        NULL;

    int rawNpcId = 0;

    int npcId =
        GetMountNpcIdFromActor(
            self,
            &rawNpcId);

    // RiderEnter in this supplied Engine.dll uses Bone15.
    // Only replace that original rider-seat attachment request.
    if (npcId > 0 &&
        bone == gOriginalRiderBone)
    {
        preset =
            FindMountPreset(
                npcId);

        if (preset)
        {
            selectedBone =
                preset->boneIndex;
        }
    }

    int result =
        gOriginalAttachToBone(
            self,
            actorToAttach,
            selectedBone,
            flag);

    if (preset)
    {
        LONG hit =
            InterlockedIncrement(
                &preset->hits);

        BOOL shouldLog =
            FALSE;

        if (gLogPresetHits &&
            hit <=
            gMaxInitialHitLogs)
        {
            shouldLog =
                TRUE;
        }

        if (gLogPresetHits &&
            gPeriodicHitLogEvery > 0 &&
            (hit %
                gPeriodicHitLogEvery) == 0)
        {
            shouldLog =
                TRUE;
        }

        if (shouldLog)
        {
            Log(
                "SEAT ATTACH #%ld | NPC=%d | rawNpc=%d | "
                "Name=%s | Mesh=%s | "
                "OriginalBone=Bone15(0x%08X) -> "
                "SeatBone=%s(0x%08X) | "
                "mountActor=%p | riderActor=%p | flag=%d | result=%d",
                hit,
                preset->npcId,
                rawNpcId,
                preset->codeName,
                preset->meshName,
                gOriginalRiderBone,
                preset->boneNameA,
                preset->boneIndex,
                self,
                actorToAttach,
                flag,
                result);
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
// Trampoline / patch
// -----------------------------------------------------------------------------

static void* BuildTrampoline(
    void* target)
{
    BYTE* t =
        (BYTE*)VirtualAlloc(
            NULL,
            32,
            MEM_COMMIT |
            MEM_RESERVE,
            PAGE_EXECUTE_READWRITE);

    if (!t)
        return NULL;

    // Exactly 5 complete verified bytes.
    memcpy(
        t,
        target,
        5);

    t[5] =
        0xE9;

    intptr_t rel =
        ((BYTE*)target + 5) -
        (t + 10);

    *(int32_t*)
        &t[6] =
        (int32_t)rel;

    FlushInstructionCache(
        GetCurrentProcess(),
        t,
        10);

    return t;
}

static bool InstallAttachHook(
    HMODULE engine)
{
    FARPROC exportStub =
        GetProcAddress(
            engine,
            kAttachToBoneExport);

    if (!exportStub)
    {
        Log(
            "ERROR: AActor::AttachToBone(FName) export not found.");

        return false;
    }

    BYTE* target =
        (BYTE*)ResolveExportJump(
            engine,
            kAttachToBoneExport);

    if (!target)
    {
        Log(
            "ERROR: could not resolve AttachToBone implementation.");

        return false;
    }

    BYTE* expected =
        (BYTE*)engine +
        kAttachToBoneImplRva;

    Log(
        "Engine base = %p",
        engine);

    Log(
        "AttachToBone export = %p",
        exportStub);

    Log(
        "AttachToBone implementation = %p",
        target);

    Log(
        "Expected implementation = %p",
        expected);

    if (target != expected)
    {
        Log(
            "ERROR: AttachToBone target mismatch | expected=%p actual=%p",
            expected,
            target);

        return false;
    }

    __try
    {
        for (int i = 0;
            i < 5;
            ++i)
        {
            if (target[i] !=
                kAttachToBoneSig[i])
            {
                Log(
                    "ERROR: AttachToBone signature mismatch at byte %d | "
                    "expected=%02X actual=%02X",
                    i,
                    kAttachToBoneSig[i],
                    target[i]);

                return false;
            }
        }
    }
    __except (
        EXCEPTION_EXECUTE_HANDLER)
    {
        Log(
            "ERROR: exception reading AttachToBone signature.");

        return false;
    }

    gAttachTrampoline =
        BuildTrampoline(
            target);

    if (!gAttachTrampoline)
    {
        Log(
            "ERROR: AttachToBone trampoline allocation failed.");

        return false;
    }

    gOriginalAttachToBone =
        (AttachToBoneFn)
        gAttachTrampoline;

    BYTE patch[5] = { 0 };

    patch[0] =
        0xE9;

    intptr_t rel =
        (BYTE*)&HookAttachToBone -
        (target + 5);

    *(int32_t*)
        &patch[1] =
        (int32_t)rel;

    DWORD oldProtect = 0;

    if (!VirtualProtect(
        target,
        5,
        PAGE_EXECUTE_READWRITE,
        &oldProtect))
    {
        Log(
            "ERROR: VirtualProtect failed: %lu",
            GetLastError());

        return false;
    }

    memcpy(
        target,
        patch,
        sizeof(patch));

    FlushInstructionCache(
        GetCurrentProcess(),
        target,
        sizeof(patch));

    DWORD dummy = 0;

    VirtualProtect(
        target,
        5,
        oldProtect,
        &dummy);

    InterlockedExchange(
        &gInstalled,
        1);

    Log(
        "SUCCESS: AActor::AttachToBone(FName) hook installed.");

    return true;
}

// -----------------------------------------------------------------------------
// Worker
// -----------------------------------------------------------------------------

static DWORD WINAPI WorkerThread(
    LPVOID)
{
    BuildPaths();
    InitLog();

    Log(
        "============================================================");

    Log(
        "InterludeMountFix - INI rider SeatBone selector");

    Log(
        "Build tag = IMF_ATTACH_SEAT_FINAL_R1");

    Log(
        "Hook = AActor::AttachToBone(FName)");

    Log(
        "Mode = NPC ID -> INI SeatBone");

    Log(
        "============================================================");

    HMODULE engine =
        NULL;

    for (int i = 0;
        i < 150;
        ++i)
    {
        engine =
            GetModuleHandleA(
                "Engine.dll");

        if (engine)
            break;

        Sleep(100);
    }

    if (!engine)
    {
        Log(
            "ERROR: Engine.dll not found.");

        return 0;
    }

    Sleep(1000);

    DWORD timestamp = 0;
    DWORD imageSize = 0;

    if (!ReadEngineInfo(
        engine,
        timestamp,
        imageSize))
    {
        Log(
            "ERROR: Engine PE validation failed.");

        return 0;
    }

    Log(
        "Engine timestamp = 0x%08X",
        timestamp);

    Log(
        "Engine image size = 0x%08X",
        imageSize);

    if (timestamp !=
        kEngineTimestamp)
    {
        Log(
            "ERROR: Engine build mismatch | expected=0x%08X actual=0x%08X",
            kEngineTimestamp,
            timestamp);

        return 0;
    }

    if (imageSize !=
        kEngineImageSize)
    {
        Log(
            "WARNING: Engine image size differs | expected=0x%08X actual=0x%08X",
            kEngineImageSize,
            imageSize);
    }

    if (!MakeFName(
        engine,
        kOriginalRiderBoneName,
        &gOriginalRiderBone))
    {
        Log(
            "ERROR: could not resolve original rider FName Bone15.");

        return 0;
    }

    Log(
        "Original RiderEnter bone = Bone15 | FName=0x%08X",
        gOriginalRiderBone);

    if (!LoadMountConfig(
        engine))
    {
        Log(
            "ERROR: MountSeats.ini load failed.");

        return 0;
    }

    if (!InstallAttachHook(
        engine))
    {
        Log(
            "ERROR: AttachToBone hook installation failed.");

        return 0;
    }

    Log(
        "READY.");

    Log(
        "Configured mount RiderEnter attachments will replace Bone15 "
        "with SeatBone from MountSeats.ini.");

    Log(
        "Duplicate SeatBone values are allowed.");

    Log(
        "Any positive NPC ID listed in [MountList] is supported.");

    Log(
        "============================================================");

    return 0;
}

// -----------------------------------------------------------------------------
// Start / export / DllMain
// -----------------------------------------------------------------------------

static void Start()
{
    if (InterlockedCompareExchange(
        &gStarted,
        1,
        0) != 0)
    {
        return;
    }

    HANDLE h =
        CreateThread(
            NULL,
            0,
            WorkerThread,
            NULL,
            0,
            NULL);

    if (h)
    {
        CloseHandle(h);
    }
    else
    {
        InterlockedExchange(
            &gStarted,
            0);
    }
}

extern "C"
__declspec(dllexport)
void __stdcall L2UI_Init()
{
    Start();
}

BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD reason,
    LPVOID)
{
    if (reason ==
        DLL_PROCESS_ATTACH)
    {
        gSelfModule =
            hModule;

        DisableThreadLibraryCalls(
            hModule);

        Start();
    }

    return TRUE;
}
