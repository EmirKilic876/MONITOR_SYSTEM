#pragma once
// ============================================================
//  SysMonitor -- umumy protokol (client <-> server)
//  Ähli maglumatlar bu struct arkaly ugradylýar
// ============================================================

#define PROTOCOL_VERSION   0x01
#define DEFAULT_PORT       54321
#define MAX_HOSTNAME_LEN   64
#define MAX_DRIVES         16
#define MAX_NICS           8
#define MAX_PROCESSES      10   // iň köp ýük döredýän prosesler
#define MAX_PROC_NAME      64
#define MAGIC              0xDEADBEEF

// ---------- Kritiki serhetler (80%) ----------
#define CRIT_CPU_PCT       80.0f
#define CRIT_RAM_PCT       80.0f
#define CRIT_DISK_PCT      80.0f
#define CRIT_TEMP_C        80.0f
#define CRIT_NET_MBPS      80.0f   // umumy ulanylýan zolak geçirijiliginiň 80%

// ---------- Disk sürüji maglumatlary ----------
typedef struct {
    char   letter;          // sürüji harpy (C, D, ...)
    char   fsType[16];      // NTFS, FAT32, ...
    double totalGB;
    double freeGB;
    double usedPct;
} DriveInfo;

// ---------- Tor kartasy maglumatlary ----------
typedef struct {
    char   name[32];        // "Ethernet", "Wi-Fi", ...
    char   ipAddr[40];      // IPv4 ýa-da IPv6
    double sendMbps;        // häzirki iberme tizligi
    double recvMbps;        // häzirki kabul etme tizligi
    double totalBandMbps;   // link tizligi (100/1000 Mbps)
} NicInfo;

// ---------- Proses maglumatlary ----------
typedef struct {
    char   name[MAX_PROC_NAME];
    DWORD  pid;
    float  cpuPct;
    float  ramMB;
} ProcInfo;

// ---------- GPU maglumatlary (NVAPI ýa-da DXGI) ----------
typedef struct {
    char   name[64];
    float  loadPct;
    float  vramUsedMB;
    float  vramTotalMB;
    float  tempC;
} GpuInfo;

// ---------- Esasy paket ----------
#pragma pack(push, 1)
typedef struct {
    UINT32  magic;                      // MAGIC döwülmezlik barlagy
    UINT8   version;                    // PROTOCOL_VERSION
    char    hostname[MAX_HOSTNAME_LEN]; // kompýuteriň ady
    char    osVersion[128];             // Windows wersiýasy
    char    cpuName[64];                // CPU modeli

    // CPU
    float   cpuTotalPct;                // umumy CPU ýüklemesi
    float   cpuPerCorePct[64];          // her özek üçin %
    UINT8   cpuCoreCount;
    float   cpuTempC;                   // °C (WMI arkaly)
    float   cpuFreqMHz;                 // häzirki ýygylyk

    // RAM
    double  ramTotalGB;
    double  ramUsedGB;
    float   ramUsedPct;
    double  pageTotalGB;
    double  pageUsedGB;

    // Diskler
    UINT8     driveCount;
    DriveInfo drives[MAX_DRIVES];

    // Tor
    UINT8   nicCount;
    NicInfo nics[MAX_NICS];

    // GPU
    GpuInfo gpu;
    BOOL    gpuPresent;

    // Ýylylyk datçikleri (RAM, M/board ...)
    float   mbTempC;
    float   hddTempC;

    // Batareýa (laptop)
    BOOL    hasBattery;
    UINT8   batteryPct;
    BOOL    batteryCharging;

    // System uptime
    UINT64  uptimeSeconds;

    // Iň köp ýük döredýän prosesler
    ProcInfo topProcs[MAX_PROCESSES];
    UINT8    procCount;

    // Wagt möhüri (Unix timestamp)
    INT64   timestamp;

    // Kritiki ýagdaý baýdagy (server üçin)
    UINT32  critFlags;   // bit aýratynlyklary -- aşakda

} SysPacket;
#pragma pack(pop)

// critFlags bit aýratynlyklary
#define CRIT_FLAG_CPU        (1 << 0)
#define CRIT_FLAG_RAM        (1 << 1)
#define CRIT_FLAG_DISK       (1 << 2)
#define CRIT_FLAG_TEMP_CPU   (1 << 3)
#define CRIT_FLAG_TEMP_GPU   (1 << 4)
#define CRIT_FLAG_TEMP_MB    (1 << 5)
#define CRIT_FLAG_TEMP_HDD   (1 << 6)
#define CRIT_FLAG_NET        (1 << 7)
#define CRIT_FLAG_GPU_LOAD   (1 << 8)
#define CRIT_FLAG_BATTERY    (1 << 9)
#define CRIT_FLAG_DISK_FULL  (1 << 10)
