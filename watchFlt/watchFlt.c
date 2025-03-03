#include <windows.h>
#include <fltUser.h>
#include <stdio.h>

#define PORT_NAME L"\\FileTrackerPort"

typedef struct _DELETE_MESSAGE {
    ULONG MessageId;
    WCHAR ProcessName[260];
    WCHAR FilePath[260];
    WCHAR DateTime[20];
} DELETE_MESSAGE, * PDELETE_MESSAGE;

int wmain() {
    HANDLE hPort;
    HRESULT hr = FilterConnectCommunicationPort(PORT_NAME, 0, NULL, 0, NULL, &hPort);
    if (FAILED(hr)) {
        wprintf(L"Failed to connect to port: 0x%08x\n", hr);
        return 1;
    }

    wprintf(L"Connected to FileTracker port. Watching for delete events...\n");

    DELETE_MESSAGE msg;
    while (TRUE) {
        hr = FilterGetMessage(hPort, (PFILTER_MESSAGE_HEADER)&msg, sizeof(msg), NULL);
        if (FAILED(hr)) {
            wprintf(L"Failed to get message: 0x%08x\n", hr);
            break;
        }

        wprintf(L"FileLogger: Operation=DELETE, Process=%s, Path=%s, DateTime=%s\n",
            msg.ProcessName, msg.FilePath, msg.DateTime);
    }

    CloseHandle(hPort);
    return 0;
}