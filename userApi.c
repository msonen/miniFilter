#include <fltKernel.h>
#include <dontuse.h>
#include <ntstrsafe.h>
#include "userApi.h"
#include "fileList.h"
#include "circularQ.h"
#include "debug.h"


#pragma pack(push, 1) // Ensure tight packing
typedef struct _DELETE_MESSAGE {
    ULONG MessageId;
    WCHAR ProcessName[260];
    WCHAR FilePath[260];
    WCHAR DateTime[20];
} DELETE_MESSAGE, * PDELETE_MESSAGE;
#pragma pack(pop)


extern TRACKED_FILES TrackedFiles;
extern PDEVICE_OBJECT gDeviceObject;
static CIRCULAR_QUEUE MessageQueue;

static NTSTATUS 
IoctlAddFile(_In_ PIRP Irp, PIO_STACK_LOCATION irpSp)
{
    PVOID inputBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG inputBufferLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    NTSTATUS status = STATUS_SUCCESS;

    if (inputBuffer && inputBufferLength > sizeof(WCHAR)) {
        UNICODE_STRING userFilePath;
        BOOLEAN protected = FALSE;
        // Check for protection flag (e.g., ends with ":p")
        PWCHAR buffer = (PWCHAR)inputBuffer;
        if (wcslen(buffer) > 2 && wcscmp(buffer + wcslen(buffer) - 2, L":p") == 0) {
            protected = TRUE;
            buffer[wcslen(buffer) - 2] = L'\0'; // Remove ":p" for filename
        }
        RtlInitUnicodeString(&userFilePath, buffer);
        if (userFilePath.Length > 0 && userFilePath.Length <= inputBufferLength) {
            if(!GetTrackedFile(&TrackedFiles, &userFilePath, NULL, NULL)){
                status = AddTrackedFile(&TrackedFiles, userFilePath.Buffer, protected);
                if (NT_SUCCESS(status)) {
                    LOG("driverFlt: Successfully added file %wZ, Protected: %d\n", &userFilePath, protected);
                } else {
                    LOG("driverFlt: Failed to add file %wZ, status: 0x%08x\n", &userFilePath, status);
                }
            }
            else {
                status = STATUS_ALREADY_REGISTERED;
            }
        } else {
            status = STATUS_INVALID_PARAMETER;
        }
    } else {
        status = STATUS_INVALID_PARAMETER;
    }
    Irp->IoStatus.Information = 0;
    return status;
}

static NTSTATUS 
IoctlRemoveFile(_In_ PIRP Irp, PIO_STACK_LOCATION irpSp)
{
    PVOID inputBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG inputBufferLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    NTSTATUS status = STATUS_SUCCESS;

    if (inputBuffer && inputBufferLength > 0 && inputBufferLength <= UNICODE_STRING_MAX_BYTES) {
        UNICODE_STRING userFilePath;
        RtlInitUnicodeString(&userFilePath, (PCWSTR)inputBuffer);
        if (userFilePath.Length > 0 && userFilePath.Length <= inputBufferLength) {
            status = RemoveTrackedFile(&TrackedFiles, userFilePath.Buffer);
            if (NT_SUCCESS(status)) {
                DEBUG("driverFlt: Successfully removed file %wZ\n", &userFilePath);
            }
            else {
                DEBUG("driverFlt: Failed to remove file %wZ, status: 0x%08x\n", &userFilePath, status);
            }
        }
        else {
            status = STATUS_INVALID_PARAMETER;
        }
    }
    else {
        status = STATUS_INVALID_PARAMETER;
    }

    return status;
}


static NTSTATUS 
IoctlGetDelMsg(_In_ PIRP Irp, PIO_STACK_LOCATION irpSp)
{
    PVOID outputBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    NTSTATUS status = STATUS_SUCCESS;

    if (outputBuffer && outputBufferLength >= sizeof(DELETE_MESSAGE)) {
        DELETE_MESSAGE msg;
        if (Dequeue(&MessageQueue, (PUCHAR)&msg)) {
            RtlCopyMemory(outputBuffer, &msg, sizeof(DELETE_MESSAGE));
            Irp->IoStatus.Information = sizeof(DELETE_MESSAGE);
        }
        else {
            status = STATUS_NO_MORE_ENTRIES;
            Irp->IoStatus.Information = 0;
        }
    }
    else {
        status = STATUS_BUFFER_TOO_SMALL;
        DEBUG("driverFlt: Buffer too small for IOCTL_GET_DELETE_MESSAGE, provided: %lu, required: %lu\n",
            outputBufferLength, sizeof(DELETE_MESSAGE));
        Irp->IoStatus.Information = 0;
    }

    return status;
}

// IOCTL handler
NTSTATUS 
IoctlControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);

    UNREFERENCED_PARAMETER(DeviceObject);

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode)
    {
    case IOCTL_ADD_TRACKED_FILE:
        status = IoctlAddFile(Irp, irpSp);
        break;
    case IOCTL_REMOVE_TRACKED_FILE:
        status = IoctlRemoveFile(Irp, irpSp);
        break;
    case IOCTL_GET_DELETE_MESSAGE:
        status = IoctlGetDelMsg(Irp, irpSp);
        break;
    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        DEBUG("driverFlt: Unknown IOCTL code\n");
        break;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

// Handle IRP_MJ_CREATE
NTSTATUS 
IoctlCreateDispatch(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    DEBUG("driverFlt: IRP_MJ_CREATE received\n");

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS 
IoctlInit() 
{
    // Initialize delete event
    return InitializeQueue(&MessageQueue, sizeof(DELETE_MESSAGE), MAX_MESSAGES);
}

static NTSTATUS 
SafeCopyUnicodeString(PWCHAR dest, size_t destSize, PUNICODE_STRING src) {
    if (!dest || !src || !src->Buffer) {
        return STATUS_INVALID_PARAMETER;
    }

    // Ensure the destination buffer is large enough to hold at least one character and a null terminator
    if (destSize < sizeof(WCHAR)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    // Copy the string, ensuring null-termination
    size_t bytesToCopy = min(src->Length, destSize - sizeof(WCHAR));
    RtlCopyMemory(dest, src->Buffer, bytesToCopy);
    dest[bytesToCopy / sizeof(WCHAR)] = L'\0'; // Null-terminate the string

    return STATUS_SUCCESS;
}

NTSTATUS 
SendToUser(PUNICODE_STRING processName, PUNICODE_STRING name, PUNICODE_STRING timeString) {
    NTSTATUS status = STATUS_SUCCESS;
    DELETE_MESSAGE message = { 0 }; // Initialize the message structure to zero

    // Validate input parameters
    if (!processName || !name || !timeString) {
        return STATUS_INVALID_PARAMETER;
    }

    // Copy process name
    status = SafeCopyUnicodeString(message.ProcessName, sizeof(message.ProcessName), processName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to copy process name: 0x%X\n", status);
        return status;
    }

    // Copy file path
    status = SafeCopyUnicodeString(message.FilePath, sizeof(message.FilePath), name);
    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to copy file path: 0x%X\n", status);
        return status;
    }

    // Copy timestamp
    status = SafeCopyUnicodeString(message.DateTime, sizeof(message.DateTime), timeString);
    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to copy timestamp: 0x%X\n", status);
        return status;
    }

    // Enqueue the message
    Enqueue(&MessageQueue, (PUCHAR)&message);

    return STATUS_SUCCESS;
}

NTSTATUS
IoctlClear() {

    CleanupQueue(&MessageQueue);
    return STATUS_SUCCESS;
}