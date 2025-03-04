#pragma once
#include <fltKernel.h>
#include <dontuse.h>


// Define the IOCTL code
#define IOCTL_ADD_TRACKED_FILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_REMOVE_TRACKED_FILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_DELETE_MESSAGE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
// Device name and symbolic link
#define DEVICE_NAME L"\\Device\\FileTracker"
#define SYMLINK_NAME L"\\DosDevices\\FileTracker"



NTSTATUS 
IoctlControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
);

NTSTATUS 
IoctlCreateDispatch(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
);

NTSTATUS 
SendToUser(PUNICODE_STRING processName, 
    PUNICODE_STRING name, PUNICODE_STRING timeString);

NTSTATUS 
IoctlInit();

NTSTATUS
IoctlClear();