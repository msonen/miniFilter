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
            // Convert user-mode wide string to UNICODE_STRING
            UNICODE_STRING userFilePath;
            RtlInitUnicodeString(&userFilePath, (PCWSTR)inputBuffer);
            if (userFilePath.Length > 0 && userFilePath.Length <= inputBufferLength) {
                status = AddTrackedFile(&TrackedFiles, userFilePath.Buffer);
            }
            else {
                status = STATUS_INVALID_PARAMETER;
            }
        }
        else {
            status = STATUS_INVALID_PARAMETER;
        }
    }
    else {
        status = STATUS_INVALID_DEVICE_REQUEST;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}