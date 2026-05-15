// ============================================================
//  SysMonitor CLIENT  --  Windows
//  Kompýuter maglumatlaryny ýygnaýar we serwere ugradýar
//  Awtozapusk: HKCU\Software\Microsoft\Windows\CurrentVersion\Run
// ============================================================
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601   // Windows 7+

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <pdh.h>
#include <powrprof.h>
#include <wbemidl.h>
#include <comdef.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cstdio>
#include <cstdlib>
#include <cstdlib>
#include <sqlite3.h>

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"iphlpapi.lib")
#pragma comment(lib,"pdh.lib")
#pragma comment(lib,"powrprof.lib")
#pragma comment(lib,"wbemuuid.lib")
#pragma comment(lib,"psapi.lib")

#include "../common/protocol.h"

// ---- Sazlamalar ----
#define SERVER_IP    "192.168.1.100"   // <- serweriň IP-sini ýaz
#define SERVER_PORT  DEFAULT_PORT
#define SEND_INTERVAL_SEC  5           // näçe saniyede bir ugrat

// ---- Global PDH ----
static PDH_HQUERY   g_cpuQuery;
static PDH_HCOUNTER g_cpuTotal;
static PDH_HCOUNTER g_cpuCores[64];
static int          g_coreCount = 0;
static BOOL         g_pdhReady = FALSE;

// ---- PDH başlatmak ----
static void InitPdh(void) {
    if (PdhOpenQuery(NULL, 0, &g_cpuQuery) != ERROR_SUCCESS) return;

    PdhAddCounterA(g_cpuQuery,
        "\\Processor(_Total)\\% Processor Time", 0, &g_cpuTotal);

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    g_coreCount = (int)si.dwNumberOfProcessors;
    if (g_coreCount > 64) g_coreCount = 64;

    char buf[128];
    for (int i = 0; i < g_coreCount; i++) {
        sprintf_s(buf, sizeof(buf),
            "\\Processor(%d)\\%% Processor Time", i);
        PdhAddCounterA(g_cpuQuery, buf, 0, &g_cpuCores[i]);
    }
    PdhCollectQueryData(g_cpuQuery);
    Sleep(500);
    g_pdhReady = TRUE;
}

// ---- CPU % ----
static float GetCpuTotal(void) {
    if (!g_pdhReady) return 0.0f;
    PdhCollectQueryData(g_cpuQuery);
    PDH_FMT_COUNTERVALUE val;
    PdhGetFormattedCounterValue(g_cpuTotal, PDH_FMT_DOUBLE, NULL, &val);
    return (float)val.doubleValue;
}

static void GetCpuPerCore(float *out, int maxCores) {
    if (!g_pdhReady) return;
    PDH_FMT_COUNTERVALUE val;
    for (int i = 0; i < g_coreCount && i < maxCores; i++) {
        PdhGetFormattedCounterValue(g_cpuCores[i], PDH_FMT_DOUBLE, NULL, &val);
        out[i] = (float)val.doubleValue;
    }
}

// ---- CPU ýygylygyny al ----
static float GetCpuFreqMHz(void) {
    HKEY hKey;
    DWORD freq = 0, size = sizeof(DWORD);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey,"~MHz",NULL,NULL,(LPBYTE)&freq,&size);
        RegCloseKey(hKey);
    }
    return (float)freq;
}

// ---- WMI üsti bilen temperaturalary al ----
 id="2r6l0q"
typedef struct
{
    float cpu;
    float mb;
    float hdd;
    float gpu;
}
Temps;

static Temps GetTempsWMI(void)
{
    Temps t = {0,0,0,0};

    HRESULT hr;

    IWbemLocator* pLoc = NULL;
    IWbemServices* pSvc = NULL;
    IEnumWbemClassObject* pEnum = NULL;

    hr = CoInitializeEx(
        0,
        COINIT_MULTITHREADED
    );

    if (FAILED(hr))
        return t;

    CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE,
        NULL
    );

    hr = CoCreateInstance(
        CLSID_WbemLocator,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&pLoc
    );

    if (FAILED(hr) || !pLoc)
        goto done;

    hr = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\WMI"),
        NULL,
        NULL,
        NULL,
        0,
        NULL,
        NULL,
        &pSvc
    );

    if (FAILED(hr) || !pSvc)
        goto done;

    hr = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );

    if (FAILED(hr))
        goto done;

    hr = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM MSAcpi_ThermalZoneTemperature"),
        WBEM_FLAG_FORWARD_ONLY |
        WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnum
    );

    if (SUCCEEDED(hr) && pEnum)
    {
        IWbemClassObject* pObj = NULL;

        ULONG ret = 0;

        hr = pEnum->Next(
            WBEM_INFINITE,
            1,
            &pObj,
            &ret
        );

        if (ret)
        {
            VARIANT vt;

            VariantInit(&vt);

            hr = pObj->Get(
                L"CurrentTemperature",
                0,
                &vt,
                0,
                0
            );

            if (SUCCEEDED(hr) && vt.vt == VT_I4)
            {
                t.cpu =
                    ((float)vt.lVal - 2732.0f) / 10.0f;
            }

            VariantClear(&vt);

            pObj->Release();
        }

        pEnum->Release();
    }

done:

    if (pSvc)
        pSvc->Release();

    if (pLoc)
        pLoc->Release();

    CoUninitialize();

    return t;
}
```


// ---- CPU adyny al ----
static void GetCpuName(char *buf, int sz) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type, size = sz;
        RegQueryValueExA(hKey,"ProcessorNameString",NULL,&type,
            (LPBYTE)buf,&size);
        RegCloseKey(hKey);
    }
}

// ---- OS wersiýasyny al ----
 id="m9v2c7"
static void GetOsVersion(char* buf, int sz)
{
    if (!buf || sz <= 0)
        return;

    buf[0] = '\0';

    HKEY hKey = NULL;

    char productName[128] = "Unknown Windows";
    char build[32] = "Unknown";

    DWORD size;
    LONG status;

    status = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        0,
        KEY_READ,
        &hKey
    );

    if (status != ERROR_SUCCESS)
    {
        strncpy_s(
            buf,
            sz,
            "Unknown Windows",
            _TRUNCATE
        );

        return;
    }

    size = sizeof(productName);

    RegQueryValueExA(
        hKey,
        "ProductName",
        NULL,
        NULL,
        (LPBYTE)productName,
        &size
    );

    size = sizeof(build);

    RegQueryValueExA(
        hKey,
        "CurrentBuild",
        NULL,
        NULL,
        (LPBYTE)build,
        &size
    );

    RegCloseKey(hKey);

    snprintf(
        buf,
        sz,
        "%s (Build %s)",
        productName,
        build
    );
}
```


// ---- RAM ----
static void FillRam(SysPacket *p) {
    MEMORYSTATUSEX ms = { sizeof(ms) };
    GlobalMemoryStatusEx(&ms);
    p->ramTotalGB  = ms.ullTotalPhys  / (1024.0*1024.0*1024.0);
    p->ramUsedGB   = (ms.ullTotalPhys - ms.ullAvailPhys)
                     / (1024.0*1024.0*1024.0);
    p->ramUsedPct  = (float)(ms.ullTotalPhys - ms.ullAvailPhys)
                     * 100.0f / (float)ms.ullTotalPhys;
    p->pageTotalGB = ms.ullTotalPageFile / (1024.0*1024.0*1024.0);
    p->pageUsedGB  = (ms.ullTotalPageFile - ms.ullAvailPageFile)
                     / (1024.0*1024.0*1024.0);
}

// ---- Diskler ----
static void FillDrives(SysPacket *p) {
    char drives[256];
    DWORD len = GetLogicalDriveStringsA(sizeof(drives), drives);
    int idx = 0;
    for (char *d = drives; d < drives+len && idx < MAX_DRIVES; d += strlen(d)+1) {
        UINT type = GetDriveTypeA(d);
        if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE &&
            type != DRIVE_CDROM) continue;

        ULARGE_INTEGER free, total, totalFree;
        if (!GetDiskFreeSpaceExA(d, &free, &total, &totalFree)) continue;

        DriveInfo *di = &p->drives[idx];
        di->letter    = d[0];
        di->totalGB   = total.QuadPart / (1024.0*1024.0*1024.0);
        di->freeGB    = totalFree.QuadPart / (1024.0*1024.0*1024.0);
        di->usedPct   = total.QuadPart > 0
                        ? (double)(total.QuadPart - totalFree.QuadPart)
                          / total.QuadPart * 100.0
                        : 0.0;
        char fsName[32]="";
        GetVolumeInformationA(d,NULL,0,NULL,NULL,NULL,fsName,sizeof(fsName));
        strncpy_s(di->fsType,sizeof(di->fsType),fsName,_TRUNCATE);
        idx++;
    }
    p->driveCount = (UINT8)idx;
}

// ---- Tor kartasy tizligi ----
 id="w8n3qp"
typedef struct
{
    UINT64 in;
    UINT64 out;
    DWORD  ifIdx;
}
NetSnap;

static NetSnap g_prevNet[MAX_NICS];

static ULONGLONG g_prevNetTime = 0;

static int g_nicCount = 0;

static void FillNics(SysPacket* p)
{
    if (!p)
        return;

    ULONG bufLen = 0;

    if (GetAdaptersInfo(NULL, &bufLen)
        != ERROR_BUFFER_OVERFLOW)
    {
        return;
    }

    IP_ADAPTER_INFO* aBuf =
        (IP_ADAPTER_INFO*)malloc(bufLen);

    if (!aBuf)
        return;

    if (GetAdaptersInfo(aBuf, &bufLen)
        != NO_ERROR)
    {
        free(aBuf);
        return;
    }

    ULONG tLen = 0;

    GetIfTable(NULL, &tLen, FALSE);

    MIB_IFTABLE* tbl =
        (MIB_IFTABLE*)malloc(tLen);

    if (!tbl)
    {
        free(aBuf);
        return;
    }

    if (GetIfTable(tbl, &tLen, FALSE)
        != NO_ERROR)
    {
        free(aBuf);
        free(tbl);
        return;
    }

    ULONGLONG now = GetTickCount64();

    double elapsed =
        (g_prevNetTime > 0)
        ? (double)(now - g_prevNetTime) / 1000.0
        : 1.0;

    int idx = 0;

    for (
        IP_ADAPTER_INFO* a = aBuf;
        a && idx < MAX_NICS;
        a = a->Next
    )
    {
        NicInfo* ni = &p->nics[idx];

        ZeroMemory(ni, sizeof(NicInfo));

        strncpy_s(
            ni->name,
            sizeof(ni->name),
            a->Description,
            _TRUNCATE
        );

        strncpy_s(
            ni->ipAddr,
            sizeof(ni->ipAddr),
            a->IpAddressList.IpAddress.String,
            _TRUNCATE
        );

        for (DWORD i = 0; i < tbl->dwNumEntries; i++)
        {
            MIB_IFROW* row = &tbl->table[i];

            if (row->dwIndex == a->Index)
            {
                ni->totalBandMbps =
                    row->dwSpeed /
                    (1024.0 * 1024.0);

                if (g_prevNetTime > 0)
                {
                    UINT64 dIn =
                        (UINT64)row->dwInOctets -
                        g_prevNet[idx].in;

                    UINT64 dOut =
                        (UINT64)row->dwOutOctets -
                        g_prevNet[idx].out;

                    ni->recvMbps =
                        (dIn * 8.0)
                        / (elapsed * 1024.0 * 1024.0);

                    ni->sendMbps =
                        (dOut * 8.0)
                        / (elapsed * 1024.0 * 1024.0);
                }

                g_prevNet[idx].in =
                    (UINT64)row->dwInOctets;

                g_prevNet[idx].out =
                    (UINT64)row->dwOutOctets;

                g_prevNet[idx].ifIdx =
                    row->dwIndex;

                break;
            }
        }

        idx++;
    }

    p->nicCount = (UINT8)idx;

    g_prevNetTime = now;

    free(aBuf);
    free(tbl);
}
```

// ---- Batareýa ----
static void FillBattery(SysPacket *p) {
    SYSTEM_POWER_STATUS ps;
    if (GetSystemPowerStatus(&ps)) {
        p->hasBattery     = (ps.BatteryFlag != 128); // 128 = ýok
        p->batteryPct     = (ps.BatteryLifePercent == 255)
                            ? 0 : ps.BatteryLifePercent;
        p->batteryCharging= (ps.BatteryFlag & 8) != 0;
    }
}

// ---- Iň köp ýük prosesler ----
static void FillTopProcs(SysPacket *p) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if (snap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
    ProcInfo all[512]; int cnt = 0;

    if (Process32First(snap, &pe)) {
        do {
            if (cnt >= 512) break;
            HANDLE hProc = OpenProcess(
                PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,
                FALSE, pe.th32ProcessID);
            if (!hProc) continue;

            PROCESS_MEMORY_COUNTERS_EX pmc = {sizeof(pmc)};
            GetProcessMemoryInfo(hProc,(PROCESS_MEMORY_COUNTERS*)&pmc,
                sizeof(pmc));

            all[cnt].pid   = pe.th32ProcessID;
            all[cnt].ramMB = (float)(pmc.PrivateUsage/(1024.0*1024.0));
            all[cnt].cpuPct= 0.0f; // PDH ile daha doğru ölçüm lazım
            strncpy_s(all[cnt].name,MAX_PROC_NAME,pe.szExeFile,_TRUNCATE);
            CloseHandle(hProc);
            cnt++;
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);

    // RAM boýunça tertiple (uly -> kiçi)
    for (int i = 0; i < cnt-1; i++)
        for (int j = i+1; j < cnt; j++)
            if (all[j].ramMB > all[i].ramMB) {
                ProcInfo tmp = all[i]; all[i]=all[j]; all[j]=tmp;
            }

    int take = cnt < MAX_PROCESSES ? cnt : MAX_PROCESSES;
    for (int i = 0; i < take; i++) p->topProcs[i] = all[i];
    p->procCount = (UINT8)take;
}

// ---- Kritiki baýdaklar ----
static UINT32 CalcCritFlags(const SysPacket *p) {
    UINT32 f = 0;
    if (p->cpuTotalPct  >= CRIT_CPU_PCT)  f |= CRIT_FLAG_CPU;
    if (p->ramUsedPct   >= CRIT_RAM_PCT)  f |= CRIT_FLAG_RAM;
    if (p->cpuTempC > 0 && p->cpuTempC   >= CRIT_TEMP_C) f |= CRIT_FLAG_TEMP_CPU;
    if (p->gpu.tempC > 0&& p->gpu.tempC  >= CRIT_TEMP_C) f |= CRIT_FLAG_TEMP_GPU;
    if (p->mbTempC > 0  && p->mbTempC    >= CRIT_TEMP_C) f |= CRIT_FLAG_TEMP_MB;
    if (p->hddTempC> 0  && p->hddTempC   >= CRIT_TEMP_C) f |= CRIT_FLAG_TEMP_HDD;
    if (p->gpu.loadPct  >= CRIT_CPU_PCT)  f |= CRIT_FLAG_GPU_LOAD;
    if (p->hasBattery && p->batteryPct < 10 && !p->batteryCharging)
        f |= CRIT_FLAG_BATTERY;
    for (int i = 0; i < p->driveCount; i++)
        if (p->drives[i].usedPct >= CRIT_DISK_PCT) f |= CRIT_FLAG_DISK;
    for (int i = 0; i < p->nicCount; i++) {
        double bw = p->nics[i].totalBandMbps;
        if (bw > 0 && (p->nics[i].sendMbps+p->nics[i].recvMbps)/bw*100.0>=CRIT_NET_MBPS)
            f |= CRIT_FLAG_NET;
    }
    return f;
}

// ---- Paketi doldur ----
static void BuildPacket(SysPacket *p) {
    memset(p, 0, sizeof(*p));
    p->magic   = MAGIC;
    p->version = PROTOCOL_VERSION;
    p->timestamp = (INT64)time(NULL);

    gethostname(p->hostname, MAX_HOSTNAME_LEN);
    GetCpuName(p->cpuName, sizeof(p->cpuName));
    GetOsVersion(p->osVersion, sizeof(p->osVersion));

    p->cpuTotalPct = GetCpuTotal();
    p->cpuCoreCount= (UINT8)g_coreCount;
    GetCpuPerCore(p->cpuPerCorePct, 64);
    p->cpuFreqMHz  = GetCpuFreqMHz();

    Temps temps    = GetTempsWMI();
    p->cpuTempC    = temps.cpu;
    p->mbTempC     = temps.mb;
    p->hddTempC    = temps.hdd;

    // Uptime
    ULONGLONG ms = GetTickCount64();
    p->uptimeSeconds = ms / 1000ULL;

    FillRam(p);
    FillDrives(p);
    FillNics(p);
    FillBattery(p);
    FillTopProcs(p);

    p->critFlags = CalcCritFlags(p);
}

// ---- Awtozapusk ---- (HKCU\...\Run)
static void RegisterAutostart(void) {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);

    HKEY hKey;
    RegOpenKeyExA(HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hKey);
    RegSetValueExA(hKey,"SysMonitorClient",0,REG_SZ,
        (LPBYTE)path,(DWORD)(strlen(path)+1));
    RegCloseKey(hKey);
}

// ---- Esasy funksiýa ----
int WINAPI WinMain(HINSTANCE hInst,HINSTANCE hPrev,
                   LPSTR lpCmd,int nShow) {
    RegisterAutostart();
    InitPdh();

    WSADATA wsd;
    WSAStartup(MAKEWORD(2,2),&wsd);

    while (1) {
        SOCKET sock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        if (sock == INVALID_SOCKET) { Sleep(SEND_INTERVAL_SEC*1000); continue; }

        struct sockaddr_in srv;
        srv.sin_family      = AF_INET;
        srv.sin_port        = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP, &srv.sin_addr);

        if (connect(sock,(SOCKADDR*)&srv,sizeof(srv)) == 0) {
            SysPacket pkt;
            BuildPacket(&pkt);
            send(sock,(char*)&pkt,sizeof(pkt),0);
        }
        closesocket(sock);
        Sleep(SEND_INTERVAL_SEC * 1000);
    }

    WSACleanup();
    return 0;
}
