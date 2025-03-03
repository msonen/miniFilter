#pragma once
#include <fltKernel.h>
#include <dontuse.h>


// Define the IOCTL code
#define IOCTL_ADD_TRACKED_FILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_REMOVE_TRACKED_FILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
// Device name and symbolic link
#define DEVICE_NAME L"\\Device\\FileTracker"
#define SYMLINK_NAME L"\\DosDevices\\FileTracker"
#define PORT_NAME L"\\FileTrackerPort"

// Message structure for communication port
typedef struct _DELETE_MESSAGE {
    ULONG MessageId;  // Unique identifier for the message
    WCHAR ProcessName[260];
    WCHAR FilePath[260];
    WCHAR DateTime[20];
} DELETE_MESSAGE, * PDELETE_MESSAGE;


NTSTATUS IoctlControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
);

NTSTATUS IoctlCreateDispatch(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
);

NTSTATUS OnConnect(
    _In_ PFLT_PORT ClientPort,
    _In_opt_ PVOID ServerPortCookie,
    _In_reads_bytes_opt_(SizeOfContext) PVOID ConnectionContext,
    _In_ ULONG SizeOfContext,
    _Outptr_result_maybenull_ PVOID* ConnectionCookie
);

VOID OnDisconnect(
    _In_opt_ PVOID ConnectionCookie
);

NTSTATUS 
PortCreate();

NTSTATUS 
SendToUser(PUNICODE_STRING processName, 
    PUNICODE_STRING name, PUNICODE_STRING timeString);