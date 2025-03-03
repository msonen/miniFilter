#include <windows.h>
#include <stdio.h>
#include <string.h>

#define IOCTL_ADD_TRACKED_FILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_REMOVE_TRACKED_FILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

BOOL ConvertWin32ToNtPath(const wchar_t* win32Path, wchar_t* ntPath, size_t ntPathSize) {
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

void PrintUsage(const wchar_t* progName) {
    wprintf(L"Usage: %s [-a|-r] <file_path>\n", progName);
    wprintf(L"  -a: Add a file to track\n");
    wprintf(L"  -r: Remove a file from tracking\n");
    wprintf(L"Example: %s -a \"C:\\Test\\file.txt\"\n", progName);
    wprintf(L"         %s -r \"C:\\Test\\file.txt\"\n", progName);
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    BOOL problemen = FALSE;
    BOOL isAdd = FALSE, isRemove = FALSE;
    const wchar_t* filePathArg = NULL;

    // Custom argument parsing
    if (argc == 3) {
        if (_wcsicmp(argv[1], L"-a") == 0) {
            isAdd = TRUE;
            filePathArg = argv[2];
        }
        else if (_wcsicmp(argv[1], L"-r") == 0) {
            isRemove = TRUE;
            filePathArg = argv[2];
        }
        else {
            problemen = TRUE;
        }
    }
    else {
        problemen = TRUE;
    }

    if (problemen || (!isAdd && !isRemove)) {
        if (problemen) {
            wprintf(L"Invalid arguments\n");
        }
        else {
            wprintf(L"Must specify -a or -r\n");
        }
        PrintUsage(argv[0]);
        return 1;
    }

    // Convert Win32 path to NT path
    wchar_t ntPath[MAX_PATH];
    if (!ConvertWin32ToNtPath(filePathArg, ntPath, MAX_PATH)) {
        wprintf(L"Failed to convert path: %s\n", filePathArg);
        return 1;
    }

    HANDLE hDevice = CreateFileW(L"\\\\.\\FileTracker",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {
        wprintf(L"Failed to open device: %d\n", GetLastError());
        return 1;
    }

    DWORD bytesReturned;
    DWORD ioctlCode = isAdd ? IOCTL_ADD_TRACKED_FILE : IOCTL_REMOVE_TRACKED_FILE;
    BOOL success = DeviceIoControl(hDevice,
        ioctlCode,
        ntPath,
        (wcslen(ntPath) + 1) * sizeof(WCHAR),
        NULL,
        0,
        &bytesReturned,
        NULL);

    if (success) {
        if (isAdd) {
            wprintf(L"Successfully added file: %s (NT: %s)\n", filePathArg, ntPath);
        }
        else {
            wprintf(L"Successfully removed file: %s (NT: %s)\n", filePathArg, ntPath);
        }
    }
    else {
        wprintf(L"Failed to %s file: %d\n", isAdd ? L"add" : L"remove", GetLastError());
    }

    CloseHandle(hDevice);
    return success ? 0 : 1;
}