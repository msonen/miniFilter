#include <windows.h>
#include <stdio.h>

#define IOCTL_ADD_TRACKED_FILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Function to convert Win32 path to NT path
BOOL ConvertWin32ToNtPath(const wchar_t* win32Path, wchar_t* ntPath, size_t ntPathSize) {
    wchar_t fullPath[MAX_PATH];
    wchar_t driveLetter[3] = { win32Path[0], L':', L'\0' };
    wchar_t devicePath[MAX_PATH];

    // Get the full Win32 path (resolves relative paths)
    if (!GetFullPathNameW(win32Path, MAX_PATH, fullPath, NULL)) {
        wprintf(L"Failed to get full path: %d\n", GetLastError());
        return FALSE;
    }

    // Query the NT device path for the drive letter (e.g., "C:" -> "\Device\HarddiskVolume3")
    if (QueryDosDeviceW(driveLetter, devicePath, MAX_PATH) == 0) {
        wprintf(L"Failed to query device path: %d\n", GetLastError());
        return FALSE;
    }

    // Construct the NT path by replacing the drive letter with the device path
    size_t deviceLen = wcslen(devicePath);
    size_t pathLen = wcslen(fullPath) - 2; // Subtract "C:"
    if (deviceLen + pathLen + 1 > ntPathSize) { // +1 for null terminator
        wprintf(L"NT path buffer too small\n");
        return FALSE;
    }

    wcscpy_s(ntPath, ntPathSize, devicePath);
    wcscat_s(ntPath, ntPathSize, fullPath + 2); // Append the path after "C:"

    return TRUE;
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2) {
        wprintf(L"Usage: %s <file_path>\n", argv[0]);
        wprintf(L"Example: %s \"C:\\Test\\file.txt\"\n", argv[0]);
        return 1;
    }

    // Buffer for NT path
    wchar_t ntPath[MAX_PATH];
    if (!ConvertWin32ToNtPath(argv[1], ntPath, MAX_PATH)) {
        wprintf(L"Failed to convert path: %s\n", argv[1]);
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
    BOOL success = DeviceIoControl(hDevice,
        IOCTL_ADD_TRACKED_FILE,
        ntPath,                      // Send NT path
        (wcslen(ntPath) + 1) * sizeof(WCHAR),
        NULL,
        0,
        &bytesReturned,
        NULL);

    if (success) {
        wprintf(L"Successfully added file: %s (NT: %s)\n", argv[1], ntPath);
    }
    else {
        wprintf(L"Failed to add file: %d\n", GetLastError());
    }

    CloseHandle(hDevice);
    return success ? 0 : 1;
}