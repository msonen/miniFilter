# minifilter

The `minifilter` system is a lightweight solution for tracking file deletions on Windows. It consists of a kernel-mode minifilter driver (`driverFlt.sys`) and two user-mode applications: `ctlFlt.exe` for controlling the driver and `watchFlt.exe` for monitoring deletion events.

## Components
- **driverFlt.sys**: A kernel-mode minifilter driver that monitors file deletions for specified paths, storing events in a circular queue (max 10 messages).
- **ctlFlt.exe**: A command-line tool to add or remove files from the driver’s tracking list.
- **watchFlt.exe**: A console application that polls the driver to retrieve and display deletion events.

## Overview
- **driverFlt.sys**: Intercepts file system operations using the Windows Filter Manager, enqueues deletion events with process name, file path, and timestamp.
- **ctlFlt.exe**: Sends IOCTLs to `driverFlt.sys` to manage the list of tracked files.
- **watchFlt.exe**: Polls `driverFlt.sys` via IOCTL to fetch deletion messages from the queue, printing them if available.

## Features
- Simple file deletion tracking with a fixed-size circular queue.
- Non-blocking design; `watchFlt.exe` polls without waiting.
- Command-line control via `ctlFlt.exe` for adding/removing tracked files.

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
2. **Add Source**: Use the `ctlFlt.cpp` code (see below if not provided).
3. **Configure**: Set to `x64` and `Debug` or `Release`.
4. **Build**: Generate `ctlFlt.exe` in `x64\Debug` or `x64\Release`.

### watchFlt.exe (Watcher)
1. **Open Project**: Create/load a Console App project in Visual Studio.
2. **Add Source**: Replace `watchFlt.cpp` with the provided code.
3. **Configure**: Set to `x64` and `Debug` or `Release`.
4. **Build**: Generate `watchFlt.exe` in `x64\Debug` or `x64\Release`.

## Installation
1. **Driver Signing**: 
   - For testing: Enable test signing (`bcdedit /set testsigning on`) and sign `driverFlt.sys` with a test certificate, or disable signature enforcement.
   - For production: Use a proper code-signing certificate.
2. **Install Driver**:
   - Copy `driverFlt.sys`, `driverFlt.cat`, and `driverFlt.inf` to `C:\Drivers`.
   - Register: `pnputil /add-driver C:\Drivers\driverFlt.inf /install`.

## Usage
### Start the Driver
   - Load: `fltmc load driverFlt`.
   - Verify with `fltmc` (should list "driverFlt").

### Control Tracked Files with `ctlFlt.exe`
- **Add a File**:
    ```
    ctlFlt.exe -a "C:\Test\file.txt"
    ```
    - Adds `C:\Test\a.txt` to the driver’s tracking list.
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

### Test a Deletion
1. Add a file: `ctlFlt.exe -a "C:\Test\file.txt"`.
2. Create the file: `echo. > C:\Test\file.txt`.
3. Delete it: `del C:\Test\file.txt`.
4. Check `watchFlt.exe` for the event.

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
