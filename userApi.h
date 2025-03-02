#pragma once
// Define the IOCTL code
#define IOCTL_ADD_TRACKED_FILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Device name and symbolic link
#define DEVICE_NAME L"\\Device\\FileTracker"
#define SYMLINK_NAME L"\\DosDevices\\FileTracker"

NTSTATUS IoctlAddFile(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
);