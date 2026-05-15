// ============================================================
//  SysMonitor SERVER  --  Windows
//  * Müşderileri dinleýär (TCP)
//  * SQLite maglumat bazasyna ýazýar
//  * Kritiki kompýuterleri öňe çykarýar
//  * Awtozapusk: HKCU\...\Run
// ============================================================
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cstdio>
#include <cstdlib>

#pragma comment(lib,"ws2_32.lib")

// SQLite amalgamation -- sqlite3.c we sqlite3.h bir papkada bolmaly
#include "sqlite3.h"
#include "../common/protocol.h"

// ---- Sazlamalar ----
#define LISTEN_PORT      DEFAULT_PORT
#define DB_FILE          "monitor.db"
#define LOG_FILE         "server.log"
#define BACKLOG          32
#define MAX_CLIENTS      256

// ---- Global maglumat bazasy ----
static sqlite3 *g_db   = NULL;
static CRITICAL_SECTION g_dbLock;
static FILE *g_logFp   = NULL;

// ---- Log ----
static void LogMsg(const char *fmt, ...) {
    char buf[1024];
    va_list va; va_start(va,fmt); vsnprintf(buf,sizeof(buf),fmt,va); va_end(va);

    time_t t = time(NULL); char ts[32];
    strftime(ts,sizeof(ts),"%Y-%m-%d %H:%M:%S",localtime(&t));

    if (g_logFp) { fprintf(g_logFp,"[%s] %s\n",ts,buf); fflush(g_logFp); }
    printf("[%s] %s\n",ts,buf);
}

// ---- Maglumat bazasyny taýýarla ----
static BOOL InitDb(void) {
    if (sqlite3_open(DB_FILE, &g_db) != SQLITE_OK) {
        LogMsg("DB açyp bolmady: %s", sqlite3_errmsg(g_db));
        return FALSE;
    }
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", 0,0,0);
    sqlite3_exec(g_db, "PRAGMA synchronous=NORMAL;", 0,0,0);

    const char *sql =
    // ---- esasy ýazgy tablisasy ----
    "CREATE TABLE IF NOT EXISTS snapshots ("
    "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  ts          INTEGER NOT NULL,"           // Unix timestamp
    "  hostname    TEXT    NOT NULL,"
    "  os          TEXT,"
    "  cpu_name    TEXT,"
    "  cpu_pct     REAL,"
    "  cpu_temp    REAL,"
    "  cpu_freq    REAL,"
    "  ram_total   REAL,"
    "  ram_used    REAL,"
    "  ram_pct     REAL,"
    "  page_total  REAL,"
    "  page_used   REAL,"
    "  gpu_name    TEXT,"
    "  gpu_load    REAL,"
    "  gpu_vram_u  REAL,"
    "  gpu_vram_t  REAL,"
    "  gpu_temp    REAL,"
    "  mb_temp     REAL,"
    "  hdd_temp    REAL,"
    "  battery_pct INTEGER,"
    "  uptime_sec  INTEGER,"
    "  crit_flags  INTEGER,"
    "  has_battery INTEGER"
    ");"

    // ---- disk tablisasy ----
    "CREATE TABLE IF NOT EXISTS snap_disks ("
    "  snap_id   INTEGER,"
    "  letter    TEXT,"
    "  fs_type   TEXT,"
    "  total_gb  REAL,"
    "  free_gb   REAL,"
    "  used_pct  REAL"
    ");"

    // ---- tor tablisasy ----
    "CREATE TABLE IF NOT EXISTS snap_nics ("
    "  snap_id    INTEGER,"
    "  name       TEXT,"
    "  ip_addr    TEXT,"
    "  send_mbps  REAL,"
    "  recv_mbps  REAL,"
    "  band_mbps  REAL"
    ");"

    // ---- proses tablisasy ----
    "CREATE TABLE IF NOT EXISTS snap_procs ("
    "  snap_id  INTEGER,"
    "  pid      INTEGER,"
    "  name     TEXT,"
    "  cpu_pct  REAL,"
    "  ram_mb   REAL"
    ");"

    // ---- her kompýuteri yzarlama ----
    "CREATE TABLE IF NOT EXISTS clients ("
    "  hostname    TEXT PRIMARY KEY,"
    "  last_seen   INTEGER,"
    "  last_ip     TEXT,"
    "  crit_flags  INTEGER"
    ");";

    char *err = NULL;
    if (sqlite3_exec(g_db, sql, 0, 0, &err) != SQLITE_OK) {
        LogMsg("Tablisa döretmek başartmady: %s", err);
        sqlite3_free(err);
        return FALSE;
    }
    LogMsg("Maglumat bazasy taýýar: %s", DB_FILE);
    return TRUE;
}

// ---- Paketi DB-e ýaz ----
static void SavePacket(const SysPacket *p, const char *clientIp) {
    EnterCriticalSection(&g_dbLock);

    sqlite3_exec(g_db,"BEGIN;",0,0,0);

    // 1) Esasy ýazgy
    sqlite3_stmt *stmt;
    const char *ins =
    "INSERT INTO snapshots"
    "(ts,hostname,os,cpu_name,cpu_pct,cpu_temp,cpu_freq,"
    " ram_total,ram_used,ram_pct,page_total,page_used,"
    " gpu_name,gpu_load,gpu_vram_u,gpu_vram_t,gpu_temp,"
    " mb_temp,hdd_temp,battery_pct,uptime_sec,crit_flags,has_battery)"
    "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

    sqlite3_prepare_v2(g_db,ins,-1,&stmt,NULL);
    sqlite3_bind_int64(stmt, 1, p->timestamp);
    sqlite3_bind_text (stmt, 2, p->hostname, -1, SQLITE_STATIC);
    sqlite3_bind_text (stmt, 3, p->osVersion,-1, SQLITE_STATIC);
    sqlite3_bind_text (stmt, 4, p->cpuName,  -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt,5, p->cpuTotalPct);
    sqlite3_bind_double(stmt,6, p->cpuTempC);
    sqlite3_bind_double(stmt,7, p->cpuFreqMHz);
    sqlite3_bind_double(stmt,8, p->ramTotalGB);
    sqlite3_bind_double(stmt,9, p->ramUsedGB);
    sqlite3_bind_double(stmt,10,p->ramUsedPct);
    sqlite3_bind_double(stmt,11,p->pageTotalGB);
    sqlite3_bind_double(stmt,12,p->pageUsedGB);
    sqlite3_bind_text  (stmt,13,p->gpuPresent?p->gpu.name:"",-1,SQLITE_STATIC);
    sqlite3_bind_double(stmt,14,p->gpu.loadPct);
    sqlite3_bind_double(stmt,15,p->gpu.vramUsedMB);
    sqlite3_bind_double(stmt,16,p->gpu.vramTotalMB);
    sqlite3_bind_double(stmt,17,p->gpu.tempC);
    sqlite3_bind_double(stmt,18,p->mbTempC);
    sqlite3_bind_double(stmt,19,p->hddTempC);
    sqlite3_bind_int   (stmt,20,p->batteryPct);
    sqlite3_bind_int64 (stmt,21,(sqlite3_int64)p->uptimeSeconds);
    sqlite3_bind_int   (stmt,22,(int)p->critFlags);
    sqlite3_bind_int   (stmt,23,p->hasBattery?1:0);
    sqlite3_step(stmt);
    sqlite3_int64 snapId = sqlite3_last_insert_rowid(g_db);
    sqlite3_finalize(stmt);

    // 2) Diskler
    const char *dins="INSERT INTO snap_disks"
        "(snap_id,letter,fs_type,total_gb,free_gb,used_pct)"
        "VALUES(?,?,?,?,?,?)";
    for (int i = 0; i < p->driveCount; i++) {
        char ltr[2]={p->drives[i].letter,0};
        sqlite3_prepare_v2(g_db,dins,-1,&stmt,NULL);
        sqlite3_bind_int64 (stmt,1,snapId);
        sqlite3_bind_text  (stmt,2,ltr,-1,SQLITE_TRANSIENT);
        sqlite3_bind_text  (stmt,3,p->drives[i].fsType,-1,SQLITE_STATIC);
        sqlite3_bind_double(stmt,4,p->drives[i].totalGB);
        sqlite3_bind_double(stmt,5,p->drives[i].freeGB);
        sqlite3_bind_double(stmt,6,p->drives[i].usedPct);
        sqlite3_step(stmt); sqlite3_finalize(stmt);
    }

    // 3) Tor
    const char *nins="INSERT INTO snap_nics"
        "(snap_id,name,ip_addr,send_mbps,recv_mbps,band_mbps)"
        "VALUES(?,?,?,?,?,?)";
    for (int i = 0; i < p->nicCount; i++) {
        sqlite3_prepare_v2(g_db,nins,-1,&stmt,NULL);
        sqlite3_bind_int64 (stmt,1,snapId);
        sqlite3_bind_text  (stmt,2,p->nics[i].name,-1,SQLITE_STATIC);
        sqlite3_bind_text  (stmt,3,p->nics[i].ipAddr,-1,SQLITE_STATIC);
        sqlite3_bind_double(stmt,4,p->nics[i].sendMbps);
        sqlite3_bind_double(stmt,5,p->nics[i].recvMbps);
        sqlite3_bind_double(stmt,6,p->nics[i].totalBandMbps);
        sqlite3_step(stmt); sqlite3_finalize(stmt);
    }

    // 4) Prosesler
    const char *pins="INSERT INTO snap_procs"
        "(snap_id,pid,name,cpu_pct,ram_mb) VALUES(?,?,?,?,?)";
    for (int i = 0; i < p->procCount; i++) {
        sqlite3_prepare_v2(g_db,pins,-1,&stmt,NULL);
        sqlite3_bind_int64(stmt,1,snapId);
        sqlite3_bind_int  (stmt,2,(int)p->topProcs[i].pid);
        sqlite3_bind_text (stmt,3,p->topProcs[i].name,-1,SQLITE_STATIC);
        sqlite3_bind_double(stmt,4,p->topProcs[i].cpuPct);
        sqlite3_bind_double(stmt,5,p->topProcs[i].ramMB);
        sqlite3_step(stmt); sqlite3_finalize(stmt);
    }

    // 5) clients tablisasyny güncelle
    const char *cups=
    "INSERT INTO clients(hostname,last_seen,last_ip,crit_flags)"
    "VALUES(?,?,?,?)"
    "ON CONFLICT(hostname) DO UPDATE SET"
    "  last_seen=excluded.last_seen,"
    "  last_ip=excluded.last_ip,"
    "  crit_flags=excluded.crit_flags";
    sqlite3_prepare_v2(g_db,cups,-1,&stmt,NULL);
    sqlite3_bind_text(stmt,1,p->hostname,-1,SQLITE_STATIC);
    sqlite3_bind_int64(stmt,2,p->timestamp);
    sqlite3_bind_text(stmt,3,clientIp,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,4,(int)p->critFlags);
    sqlite3_step(stmt); sqlite3_finalize(stmt);

    sqlite3_exec(g_db,"COMMIT;",0,0,0);
    LeaveCriticalSection(&g_dbLock);
}

// ---- Kritiki baýdaklary çözme ----
static void PrintCritFlags(UINT32 f, char *buf, int sz) {
    buf[0] = 0;
    if (f & CRIT_FLAG_CPU)       strncat(buf,"CPU-ýüklemesi ",sz-strlen(buf)-1);
    if (f & CRIT_FLAG_RAM)       strncat(buf,"RAM ",sz-strlen(buf)-1);
    if (f & CRIT_FLAG_DISK)      strncat(buf,"Disk ",sz-strlen(buf)-1);
    if (f & CRIT_FLAG_TEMP_CPU)  strncat(buf,"CPU-Temp ",sz-strlen(buf)-1);
    if (f & CRIT_FLAG_TEMP_GPU)  strncat(buf,"GPU-Temp ",sz-strlen(buf)-1);
    if (f & CRIT_FLAG_TEMP_MB)   strncat(buf,"MB-Temp ",sz-strlen(buf)-1);
    if (f & CRIT_FLAG_TEMP_HDD)  strncat(buf,"HDD-Temp ",sz-strlen(buf)-1);
    if (f & CRIT_FLAG_NET)       strncat(buf,"Tor-doldy ",sz-strlen(buf)-1);
    if (f & CRIT_FLAG_GPU_LOAD)  strncat(buf,"GPU-ýük ",sz-strlen(buf)-1);
    if (f & CRIT_FLAG_BATTERY)   strncat(buf,"Batareýa-az ",sz-strlen(buf)-1);
    if (!f) strncat(buf,"(ýok)",sz-strlen(buf)-1);
}

// ---- Konsol sanawyny çykar ----
static void PrintDashboard(void) {
    EnterCriticalSection(&g_dbLock);

    // Kritiki kompýuterler öňe çykýar
    const char *sql =
    "SELECT hostname, last_ip, last_seen, crit_flags "
    "FROM clients "
    "ORDER BY crit_flags DESC, last_seen DESC";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db,sql,-1,&stmt,NULL)!=SQLITE_OK) {
        LeaveCriticalSection(&g_dbLock);
        return;
    }

    printf("\n");
    printf("============================================================\n");
    printf("  SYSMONITOR SERWER PANELI  --  %s\n", __TIME__);
    printf("  [!] Kritiki (>80%%)   [ ] Normal\n");
    printf("============================================================\n");
    printf("%-20s %-16s %-10s %-30s\n",
        "Kompýuter", "IP", "Wagt(s)", "Kritiki ýagdaýlar");
    printf("------------------------------------------------------------\n");

    while (sqlite3_step(stmt)==SQLITE_ROW) {
        const char *host = (const char*)sqlite3_column_text(stmt,0);
        const char *ip   = (const char*)sqlite3_column_text(stmt,1);
        time_t lastSeen  = (time_t)sqlite3_column_int64(stmt,2);
        UINT32 flags     = (UINT32)sqlite3_column_int(stmt,3);

        time_t now = time(NULL);
        long ago   = (long)(now - lastSeen);

        char flagStr[256];
        PrintCritFlags(flags, flagStr, sizeof(flagStr));

        printf("%s %-20s %-16s %-10ld %s\n",
            flags ? "[!]" : "[ ]",
            host ? host : "?",
            ip   ? ip   : "?",
            ago,
            flagStr);
    }
    sqlite3_finalize(stmt);
    printf("============================================================\n\n");
    LeaveCriticalSection(&g_dbLock);
}

// ---- Her gelýän müşderi üçin Thread ----
typedef struct { SOCKET sock; char ip[40]; } ClientArg;

static DWORD WINAPI ClientThread(LPVOID arg) {
    ClientArg *ca = (ClientArg*)arg;
    SOCKET sock   = ca->sock;
    char   ip[40]; strcpy_s(ip,sizeof(ip),ca->ip);
    free(ca);

    // Paketi kabul et
    SysPacket pkt;
    int total = 0;
    char *buf = (char*)&pkt;
    while (total < (int)sizeof(pkt)) {
        int r = recv(sock, buf+total, sizeof(pkt)-total, 0);
        if (r <= 0) break;
        total += r;
    }
    closesocket(sock);

    if (total < (int)sizeof(pkt)) {
        LogMsg("[WARN] %s: Eksik paket (%d/%d)", ip, total, (int)sizeof(pkt));
        return 0;
    }
    if (pkt.magic != MAGIC) {
        LogMsg("[WARN] %s: Nädogry magic 0x%X", ip, pkt.magic);
        return 0;
    }
    if (pkt.version != PROTOCOL_VERSION) {
        LogMsg("[WARN] %s: Wersiýa tapawudy %d", ip, pkt.version);
    }

    SavePacket(&pkt, ip);

    char flagStr[256];
    PrintCritFlags(pkt.critFlags, flagStr, sizeof(flagStr));
    LogMsg("[DATA] %-20s | CPU:%.1f%% RAM:%.1f%% | Kritiki: %s",
        pkt.hostname, pkt.cpuTotalPct, pkt.ramUsedPct, flagStr);

    // Eger kritiki bolsa konsolda sanaw täzele
    if (pkt.critFlags) PrintDashboard();

    return 0;
}

// ---- Awtozapusk ----
static void RegisterAutostart(void) {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    HKEY hKey;
    RegOpenKeyExA(HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hKey);
    RegSetValueExA(hKey,"SysMonitorServer",0,REG_SZ,
        (LPBYTE)path,(DWORD)(strlen(path)+1));
    RegCloseKey(hKey);
}

// ---- Wagt bilen sanaw täzele (her 30s) ----
static DWORD WINAPI DashThread(LPVOID unused) {
    while (1) { Sleep(30000); PrintDashboard(); }
    return 0;
}

// ---- main ----
int main(void) {
    SetConsoleOutputCP(65001); // UTF-8 konsol

    g_logFp = fopen(LOG_FILE,"a");
    InitializeCriticalSection(&g_dbLock);
    RegisterAutostart();

    if (!InitDb()) return 1;

    WSADATA wsd;
    WSAStartup(MAKEWORD(2,2),&wsd);

    SOCKET srv = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    int opt = 1;
    setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,(char*)&opt,sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(LISTEN_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(srv,(SOCKADDR*)&addr,sizeof(addr));
    listen(srv,BACKLOG);

    LogMsg("Serwer başlady. Port: %d", LISTEN_PORT);
    PrintDashboard();

    CreateThread(NULL,0,DashThread,NULL,0,NULL);

    while (1) {
        struct sockaddr_in caddr;
        int clen = sizeof(caddr);
        SOCKET csock = accept(srv,(SOCKADDR*)&caddr,&clen);
        if (csock == INVALID_SOCKET) continue;

        ClientArg *ca = (ClientArg*)malloc(sizeof(ClientArg));
        ca->sock = csock;
        inet_ntop(AF_INET,&caddr.sin_addr,ca->ip,sizeof(ca->ip));

        HANDLE th = CreateThread(NULL,0,ClientThread,ca,0,NULL);
        if (th) CloseHandle(th);
    }

    DeleteCriticalSection(&g_dbLock);
    if (g_logFp) fclose(g_logFp);
    sqlite3_close(g_db);
    WSACleanup();
    return 0;
}
