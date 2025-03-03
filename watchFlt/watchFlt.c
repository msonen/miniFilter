#include <windows.h>
#include <stdio.h>

#define DEVICE_NAME L"\\\\.\\FileTracker"
#define IOCTL_GET_DELETE_MESSAGE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#pragma pack(push, 1)
typedef struct _DELETE_MESSAGE {
    ULONG MessageId;
    WCHAR ProcessName[260];
    WCHAR FilePath[260];
    WCHAR DateTime[20];
} DELETE_MESSAGE, * PDELETE_MESSAGE;
#pragma pack(pop)

int wmain() {
    HANDLE hDevice = CreateFileW(DEVICE_NAME,
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

    wprintf(L"Connected to FileTracker device. Polling for delete events... (Buffer size: %zu bytes)\n", sizeof(DELETE_MESSAGE));

    while (TRUE) {
        DELETE_MESSAGE msg = { 0 };
        DWORD bytesReturned;
        BOOL success = DeviceIoControl(hDevice,
            IOCTL_GET_DELETE_MESSAGE,
            NULL, 0,
            &msg, sizeof(DELETE_MESSAGE),
            &bytesReturned,
            NULL);

        if (success && bytesReturned == sizeof(DELETE_MESSAGE)) {
            wprintf(L"FileLogger: Operation=DELETE, Process=%s, Path=%s, DateTime=%s\n",
                msg.ProcessName, msg.FilePath, msg.DateTime);
        }
        else {
            DWORD error = GetLastError();
            if (error == ERROR_NO_MORE_ITEMS) {
                // No messages, wait and retry
                Sleep(100); // Simple polling delay
            }
            else {
                wprintf(L"Failed to get message: %d\n", error);
                break;
            }
        }
    }

    CloseHandle(hDevice);
    return 0;
}