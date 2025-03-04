# minifilter

The `minifilter` system is a lightweight solution for tracking and optionally protecting file deletions on Windows. It consists of a kernel-mode minifilter driver (`driverFlt.sys`) and two user-mode applications: `ctlFlt.exe` for controlling the driver and `watchFlt.exe` for monitoring deletion events.

## Components
- **driverFlt.sys**: A kernel-mode minifilter driver that monitors file deletions for specified paths, storing events in a circular queue (max 10 messages) and optionally blocking deletions for protected files.
- **ctlFlt.exe**: A command-line tool to add, remove, or protect files in the driver’s tracking list.
- **watchFlt.exe**: A console application that polls the driver to retrieve and display deletion events.

## Overview
- **driverFlt.sys**: Intercepts file system operations using the Windows Filter Manager, enqueues deletion events (process name, file path, timestamp), and blocks deletions for protected files.
- **ctlFlt.exe**: Sends IOCTLs to `driverFlt.sys` to manage tracked files, with an option to mark files as protected.
- **watchFlt.exe**: Polls `driverFlt.sys` via IOCTL to fetch deletion messages from the queue, printing them if available.

## Features
- Tracks file deletions with a fixed-size circular queue.
- Non-blocking, polling-based design.
- Optional file protection to prevent deletions using the `-p` command in `ctlFlt.exe`.
- Command-line control via `ctlFlt.exe`.

## Prerequisites
- **Windows OS**: Windows 10 or later (x64).
- **Windows Driver Kit (WDK)**: For building `driverFlt.sys` (e.g., WDK 10.0.22621.0).
- **Visual Studio**: For building all components (e.g., VS 2022 with C++ support).
- **Administrator Privileges**: Required to load the driver and run the apps.

## Building

### driverFlt.sys (Kernel Driver)
1. **Open Project**: Load the driver project in Visual Studio (assumes a `.vcxproj` file).
2. **Configure**: Set to `x64` and `Debug` or `Release`.
3. **Build**: Generate `driverFlt.sys` in `x64\Debug` or `x64\Release`.

### ctlFlt.exe (Control Tool)
1. **Open Project**: Create/load a Console App project in Visual Studio.
2. **Add Source**: Replace `ctlFlt.cpp` with the provided code.
3. **Configure**: Set to `x64` and `Debug` or `Release`.
4. **Build**: Generate `ctlFlt.exe` in `x64\Debug` or `x64\Release`.

### watchFlt.exe (Watcher)
1. **Open Project**: Create/load a Console App project in Visual Studio.
2. **Add Source**: Use the existing `watchFlt.cpp` (unchanged from previous).
3. **Configure**: Set to `x64` and `Debug` or `Release`.
4. **Build**: Generate `watchFlt.exe` in `x64\Debug` or `x64\Release`.

## Installation
1. **Driver Signing**: 
   - For testing: Enable test signing (`bcdedit /set testsigning on`) and sign `driverFlt.sys` with a test certificate, or disable signature enforcement.
   - For production: Use a proper code-signing certificate.
2. **Install Driver**:
   - Copy `driverFlt.sys` to `C:\Drivers`.
   - Register: `sc create driverFlt type= kernel binPath= C:\Drivers\driverFlt.sys`.
   - Start: `net start driverFlt`.

## Usage

### Start the Driver
   - Load: `fltmc load driverFlt`.
   - Verify with `fltmc` (should list "driverFlt").

### Control Tracked Files with `ctlFlt.exe`
- **Add a File**:
    ```
    ctlFlt.exe -a "C:\Test\file.txt"
    ```
    - Adds `C:\Test\file.txt` to the tracking list without protection.
- **Add a Protected File**:
     ```
    ctlFlt.exe -p "C:\Test\file.txt"
    ```
    - Adds `C:\Test\file.txt` as protected (blocks deletion attempts).
- **Remove a File**:
    ```
    ctlFlt.exe -r "C:\Test\file.txt"
    ```
    - Removes `C:\Test\a.txt` from the tracking list.

### Monitor Deletions with `watchFlt.exe`
    watchFlt.exe

- Output: "Connected to FileTracker device. Polling for delete events... (Buffer size: 564 bytes)"
- Polls every 100ms; prints events like:
```
FileLogger: Operation=DELETE, Process=cmd.exe, Path=\Device\HarddiskVolume3\Test\a.txt, DateTime=2025-03-03 14:30:45
```

### Test the Feature
1. **Track a File**: `ctlFlt.exe -a "C:\Test\file.txt"`.
2. **Protect a File**: `ctlFlt.exe -p "C:\Test\protected.txt"`.
3. **Create Files**: 
    ```
    echo. > C:\Test\file.txt
    echo. > C:\Test\protected.txt
    ```
4. **Delete Files**: 
    - `del C:\Test\file.txt` (succeeds, logged by `watchFlt.exe`).
    - `del C:\Test\protected.txt` (fails with "Access is denied").

5. Check `watchFlt.exe` for the event.

### Stop and Unload
- Stop `watchFlt.exe`: Ctrl+C.
- Unload driver:
    ```
    fltmc unload driverFlt
    ```

## Debug Output
- Use **DebugView** (Sysinternals) with "Capture Kernel" enabled:
- `driverFlt: Driver loaded successfully, DELETE_MESSAGE size: 564`
- `driverFlt: Enqueued message, count: 1`
- `driverFlt: Dequeued message, count: 0`

## Limitations
-   Queue Size: 10 messages max; oldest events drop if full.
-   Polling Delay: 100ms; adjust Sleep(100) in watchFlt.cpp if needed.
-   Single Consumer: One watchFlt.exe instance at a time.

## Troubleshooting

- **"Failed to open device"**: Run as Administrator; ensure driver is loaded (`fltmc`).
- **"Failed to get message: 259"**: Normal when queue is empty (`ERROR_NO_MORE_ITEMS`); app retries.
- **"Failed to get message: 170"**: Another `watchFlt.exe` instance is running; ensure only one runs.
- **BSOD**: Shouldn’t occur with this design; if it does, enable Driver Verifier (`verifier /standard /driver driverFlt.sys`) and share bugcheck code.
