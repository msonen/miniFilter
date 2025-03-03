#include <windows.h>
#include <stdio.h>

#define IOCTL_ADD_TRACKED_FILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2) {
        wprintf(L"Usage: %s <file_path>\n", argv[0]);
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
        argv[1],                     // Input buffer (file path)
        (wcslen(argv[1]) + 1) * sizeof(WCHAR), // Include null terminator
        NULL,                        // No output buffer
        0,
        &bytesReturned,
        NULL);

    if (success) {
        wprintf(L"Successfully added file: %s\n", argv[1]);
    }
    else {
        wprintf(L"Failed to add file: %d\n", GetLastError());
    }

    CloseHandle(hDevice);
    return success ? 0 : 1;
}