#include <windows.h>
#include <stdio.h>

#define DEVICE_NAME L"\\\\.\\FileTracker"
#define IOCTL_ADD_TRACKED_FILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_REMOVE_TRACKED_FILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

static BOOL ConvertWin32ToNtPath(const wchar_t* win32Path, wchar_t* ntPath, size_t ntPathSize) {
    wchar_t fullPath[MAX_PATH];
    wchar_t driveLetter[3] = { win32Path[0], L':', L'\0' };
    wchar_t devicePath[MAX_PATH];

    if (!GetFullPathNameW(win32Path, MAX_PATH, fullPath, NULL)) {
        wprintf(L"Failed to get full path: %d\n", GetLastError());
        return FALSE;
    }

    if (QueryDosDeviceW(driveLetter, devicePath, MAX_PATH) == 0) {
        wprintf(L"Failed to query device path: %d\n", GetLastError());
        return FALSE;
    }

    size_t deviceLen = wcslen(devicePath);
    size_t pathLen = wcslen(fullPath) - 2;
    if (deviceLen + pathLen + 1 > ntPathSize) {
        wprintf(L"NT path buffer too small\n");
        return FALSE;
    }

    wcscpy_s(ntPath, ntPathSize, devicePath);
    wcscat_s(ntPath, ntPathSize, fullPath + 2);
    return TRUE;
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 3) {
        wprintf(L"Usage: %s [-a|-r|-p] <file_path>\n", argv[0]);
        wprintf(L"  -a: Add file to tracking\n");
        wprintf(L"  -r: Remove file from tracking\n");
        wprintf(L"  -p: Add file with protection (prevents deletion)\n");
        return 1;
    }

    HANDLE hDevice = CreateFileW(DEVICE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) {
        wprintf(L"Failed to open device: %d\n", GetLastError());
        return 1;
    }

    BOOL protect = FALSE;
    DWORD ioCode;
    if (wcscmp(argv[1], L"-a") == 0) {
        ioCode = IOCTL_ADD_TRACKED_FILE;
    }
    else if (wcscmp(argv[1], L"-r") == 0) {
        ioCode = IOCTL_REMOVE_TRACKED_FILE;
    }
    else if (wcscmp(argv[1], L"-p") == 0) {
        ioCode = IOCTL_ADD_TRACKED_FILE;
        protect = TRUE;
    }
    else {
        wprintf(L"Invalid command: %s\n", argv[1]);
        CloseHandle(hDevice);
        return 1;
    }

    WCHAR filePath[1024];

    if (!ConvertWin32ToNtPath(argv[2], filePath, MAX_PATH)) {
        wprintf(L"Failed to convert path: %s\n", argv[2]);
        return 1;
    }

    if (protect) {
        wcscat_s(filePath, 1024, L":p"); // Append ":p" to indicate protection
    }

    DWORD bytesReturned;
    BOOL success = DeviceIoControl(hDevice, ioCode, filePath, (wcslen(filePath) + 1) * sizeof(WCHAR), NULL, 0, &bytesReturned, NULL);
    if (success) {
        if (ioCode == IOCTL_REMOVE_TRACKED_FILE) {
            wprintf(L"Removed %s successfully\n", argv[2]);
        }
        else {
            wprintf(L"Added %s successfully%s\n", argv[2], protect ? L" (protected)" : L"");
        }
    }
    else {
        wprintf(L"Failed to %s %s: %d\n", (ioCode == IOCTL_REMOVE_TRACKED_FILE) ? L"remove" : L"add", argv[2], GetLastError());
    }

    CloseHandle(hDevice);
    return success ? 0 : 1;
}