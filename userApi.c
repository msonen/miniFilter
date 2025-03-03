#include <fltKernel.h>
#include <dontuse.h>
#include "userApi.h"
#include "fileList.h"


extern TRACKED_FILES TrackedFiles;
// IOCTL handler
NTSTATUS IoctlAddFile(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    PVOID inputBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG inputBufferLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;

    UNREFERENCED_PARAMETER(DeviceObject);

    if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_ADD_TRACKED_FILE) {
        if (inputBuffer && inputBufferLength > 0 && inputBufferLength <= UNICODE_STRING_MAX_BYTES) {
            UNICODE_STRING userFilePath;
            RtlInitUnicodeString(&userFilePath, (PCWSTR)inputBuffer);
            if (userFilePath.Length > 0 && userFilePath.Length <= inputBufferLength) {
                status = AddTrackedFile(&TrackedFiles, userFilePath.Buffer);
                if (NT_SUCCESS(status)) {
                    DbgPrint("FileTracker: Successfully added file %wZ\n", &userFilePath);
                }
                else {
                    DbgPrint("FileTracker: Failed to add file %wZ, status: 0x%08x\n", &userFilePath, status);
                }
            }
            else {
                status = STATUS_INVALID_PARAMETER;
                DbgPrint("FileTracker: Invalid parameter in IOCTL\n");
            }
        }
        else {
            status = STATUS_INVALID_PARAMETER;
            DbgPrint("FileTracker: No input buffer or invalid length\n");
        }
    }
    else {
        status = STATUS_INVALID_DEVICE_REQUEST;
        DbgPrint("FileTracker: Unknown IOCTL code\n");
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

// Handle IRP_MJ_CREATE
NTSTATUS IoctlCreateDispatch(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    DbgPrint("FileTracker: IRP_MJ_CREATE received\n");

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}
