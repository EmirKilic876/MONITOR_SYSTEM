# SysMonitor -- Kompýuter Gözegçilik Sistemasy

## Arhitektura

```
[Müşderi kompýuterler]  ──TCP──►  [Serwer]  ──►  [SQLite DB]
   client.exe                      server.exe       monitor.db
   (her 5 sek)                    (konsol paneli)
```

## Ýygnalýan maglumatlar

| Kategoriýa | Maglumat |
|---|---|
| **CPU** | Umumy %, her özek %, temperatura, ýygylyk (MHz), model ady |
| **RAM** | Jemi, ulanylýan (GB / %), sahypa faýly |
| **Diskler** | Harp, görnüş (NTFS...), jemi/boş GB, dolulyk % |
| **Tor** | Her NIC üçin iberiş/kabul (Mbps), IP, zolak geçirijiligi |
| **GPU** | Model, ýüklemesi %, VRAM (ulanylan/jemi), temperatura |
| **Temperatura** | CPU, GPU, Enäniň platasynda (MB), HDD |
| **Batareýa** | Göterim, zarýadlanýarmy |
| **Prosesler** | Iň köp RAM ulanýan 10 proses (at, PID, RAM MB) |
| **Uptime** | Ulgam işlän wagty (sekunt) |
| **OS** | Windows ady we build belgisi |

## Kritiki ýagdaý (> 80%)

- CPU ýüklemesi > 80 %
- RAM ýüklemesi > 80 %
- Disk dolulyk > 80 %
- CPU/GPU/MB/HDD temperatura > 80 °C
- Tor zolak geçirijiligi > 80 %
- GPU ýüklemesi > 80 %
- Batareýa < 10 % (zarýad ýok bolanda)

Kritiki kompýuterler serwer panelinde **iň ýokarda** görkezilýär.

---

## Gurnamak we Derlemek

### Talaplar
- **Visual Studio 2019/2022** (C++ Desktop Development)
- **CMake 3.16+**
- **SQLite amalgamation** (`sqlite3.c` + `sqlite3.h`) -- [sqlite.org/download.html](https://www.sqlite.org/download.html)

### Ädimleri

```batch
REM 1. SQLite amalgamation-y yukle we server\ papkasyna goy
REM    server\sqlite3.c
REM    server\sqlite3.h

REM 2. client\client.c ichindaki SERVER_IP-ni oz serwer IP-ne duzelt:
REM    #define SERVER_IP  "192.168.1.XXX"

REM 3. Derlemek
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

REM 4. Fayllary al:
REM    build\Release\SysMonitorClient.exe  --> her musteride islet
REM    build\Release\SysMonitorServer.exe  --> serwerde islet
```

---

## Ulanmak

### Serwer
```batch
SysMonitorServer.exe
```
- `monitor.db` -- SQLite bazasy (SQL bilen sorgulap bilersiň)
- `server.log` -- ähli wakalar

### Müşderi
```batch
SysMonitorClient.exe
```
- Başlanandan soňra awtozapusk üçin Registrä ýazylýar
- Her 5 sekunddan maglumatlary serwere ugradýar

---

## Maglumat bazasyna sorgulamak

```sql
-- Soňky 1 sagat içinde kritiki bolan kompýuterler
SELECT hostname, cpu_pct, ram_pct, crit_flags, datetime(ts,'unixepoch')
FROM snapshots
WHERE ts > strftime('%s','now','-1 hour')
  AND crit_flags > 0
ORDER BY crit_flags DESC;

-- Iň köp CPU ulanýan kompýuter
SELECT hostname, AVG(cpu_pct) as avg_cpu
FROM snapshots
GROUP BY hostname
ORDER BY avg_cpu DESC
LIMIT 10;

-- Haýsy diskler dolulyga golaý?
SELECT s.hostname, d.letter, d.used_pct, d.free_gb
FROM snap_disks d
JOIN snapshots s ON s.id = d.snap_id
WHERE d.used_pct > 70
ORDER BY d.used_pct DESC;
```

---

## Faýl gurluşy

```..
monitor_system/
├── common/
│   └── protocol.h       # umumy protokol struct-lary
├── client/
│   └── client.c         # müşderi programmasy
├── server/
│   ├── server.c         # serwer programmasy
│   ├── sqlite3.c        # <- özüň goş (amalgamation)
│   └── sqlite3.h        # <- özüň goş (amalgamation)
└── CMakeLists.txt       # cmake
```
