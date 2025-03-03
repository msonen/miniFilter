#include <fltKernel.h>
#include <dontuse.h>
#include <ntstrsafe.h>
#include "userApi.h"
#include "fileList.h"

#define MAX_MESSAGES 10 // Circular queue size

#pragma pack(push, 1) // Ensure tight packing
typedef struct _DELETE_MESSAGE {
    ULONG MessageId;
    WCHAR ProcessName[260];
    WCHAR FilePath[260];
    WCHAR DateTime[20];
} DELETE_MESSAGE, * PDELETE_MESSAGE;
#pragma pack(pop)


// Circular queue for messages
typedef struct _MESSAGE_QUEUE {
    DELETE_MESSAGE Messages[MAX_MESSAGES];
    ULONG Head; // Index of oldest message
    ULONG Tail; // Index where next message goes
    ULONG Count; // Number of messages in queue
    KSPIN_LOCK Lock;
} MESSAGE_QUEUE, * PMESSAGE_QUEUE;

extern TRACKED_FILES TrackedFiles;
extern PDEVICE_OBJECT gDeviceObject;
static MESSAGE_QUEUE MessageQueue;


VOID InitializeMessageQueue(PMESSAGE_QUEUE Queue) {
    RtlZeroMemory(Queue->Messages, sizeof(Queue->Messages));
    Queue->Head = 0;
    Queue->Tail = 0;
    Queue->Count = 0;
    KeInitializeSpinLock(&Queue->Lock);
}

NTSTATUS EnqueueMessage(PMESSAGE_QUEUE Queue, PDELETE_MESSAGE Message) {
    KIRQL oldIrql;
    NTSTATUS status;
    KeAcquireSpinLock(&Queue->Lock, &oldIrql);

    if (Queue->Count < MAX_MESSAGES) {
        RtlCopyMemory(&Queue->Messages[Queue->Tail], Message, sizeof(DELETE_MESSAGE));
        Queue->Tail = (Queue->Tail + 1) % MAX_MESSAGES;
        Queue->Count++;
        status = STATUS_SUCCESS;
        DbgPrint("FileTracker: Enqueued message, count: %lu\n", Queue->Count);
    }
    else {
        status = STATUS_INSUFFICIENT_RESOURCES;
        DbgPrint("FileTracker: Queue full, message dropped\n");
    }

    KeReleaseSpinLock(&Queue->Lock, oldIrql);
    return status;
}

NTSTATUS DequeueMessage(PMESSAGE_QUEUE Queue, PDELETE_MESSAGE Message) {
    KIRQL oldIrql;
    NTSTATUS status;

    KeAcquireSpinLock(&Queue->Lock, &oldIrql);

    if (Queue->Count > 0) {
        RtlCopyMemory(Message, &Queue->Messages[Queue->Head], sizeof(DELETE_MESSAGE));
        Queue->Head = (Queue->Head + 1) % MAX_MESSAGES;
        Queue->Count--;
        status = STATUS_SUCCESS;
        DbgPrint("FileTracker: Dequeued message, count: %lu\n", Queue->Count);
    }
    else {
        status = STATUS_NO_MORE_ENTRIES;
        DbgPrint("FileTracker: Queue empty\n");
    }

    KeReleaseSpinLock(&Queue->Lock, oldIrql);
    return status;
}

static NTSTATUS IoctlAddFile(_In_ PIRP Irp, PIO_STACK_LOCATION irpSp)
{
    PVOID inputBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG inputBufferLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    NTSTATUS status = STATUS_SUCCESS;

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

    return status;
}

static NTSTATUS IoctlRemoveFile(_In_ PIRP Irp, PIO_STACK_LOCATION irpSp)
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
                DbgPrint("FileTracker: Successfully removed file %wZ\n", &userFilePath);
            }
            else {
                DbgPrint("FileTracker: Failed to remove file %wZ, status: 0x%08x\n", &userFilePath, status);
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


static NTSTATUS IoctlGetDelMsg(_In_ PIRP Irp, PIO_STACK_LOCATION irpSp)
{
    PVOID outputBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    NTSTATUS status = STATUS_SUCCESS;

    if (outputBuffer && outputBufferLength >= sizeof(DELETE_MESSAGE)) {
        DELETE_MESSAGE msg;
        status = DequeueMessage(&MessageQueue, &msg);
        if (NT_SUCCESS(status)) {
            RtlCopyMemory(outputBuffer, &msg, sizeof(DELETE_MESSAGE));
            Irp->IoStatus.Information = sizeof(DELETE_MESSAGE);
        }
        else {
            Irp->IoStatus.Information = 0;
        }
    }
    else {
        status = STATUS_BUFFER_TOO_SMALL;
        DbgPrint("FileTracker: Buffer too small for IOCTL_GET_DELETE_MESSAGE, provided: %lu, required: %lu\n",
            outputBufferLength, sizeof(DELETE_MESSAGE));
        Irp->IoStatus.Information = 0;
    }

    return status;
}

// IOCTL handler
NTSTATUS IoctlControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);

    UNREFERENCED_PARAMETER(DeviceObject);

    if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_ADD_TRACKED_FILE) {
        status = IoctlAddFile(Irp, irpSp);
    }
    else if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_REMOVE_TRACKED_FILE) {
        status = IoctlRemoveFile(Irp, irpSp);
    }
    else if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_GET_DELETE_MESSAGE) {
        status = IoctlGetDelMsg(Irp, irpSp);
    }
    else {
        status = STATUS_INVALID_DEVICE_REQUEST;
        DbgPrint("FileTracker: Unknown IOCTL code\n");
    }

    Irp->IoStatus.Status = status;
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

NTSTATUS IoctlInit() 
{
    // Initialize delete event
    InitializeMessageQueue(&MessageQueue);

    return STATUS_SUCCESS;
}



NTSTATUS SendToUser(PUNICODE_STRING processName, PUNICODE_STRING name, PUNICODE_STRING timeString)
{
    NTSTATUS status = STATUS_SUCCESS;
    // Prepare message for user-mode
    DELETE_MESSAGE message;
    PDELETE_MESSAGE msg = &message;
    RtlCopyMemory(msg->ProcessName, processName->Buffer, min(processName->Length, sizeof(msg->ProcessName) - sizeof(WCHAR)));
    msg->ProcessName[sizeof(msg->ProcessName) / sizeof(WCHAR) - 1] = L'\0';
    RtlCopyMemory(msg->FilePath, name->Buffer, min(name->Length, sizeof(msg->FilePath) - sizeof(WCHAR)));
    msg->FilePath[sizeof(msg->FilePath) / sizeof(WCHAR) - 1] = L'\0';
    RtlCopyMemory(msg->DateTime, timeString->Buffer, min(timeString->Length, sizeof(msg->DateTime) - sizeof(WCHAR)));
    msg->DateTime[sizeof(msg->DateTime) / sizeof(WCHAR) - 1] = L'\0';

    EnqueueMessage(&MessageQueue, msg);

    return status;
}

NTSTATUS
IoctlClear() {

    return STATUS_SUCCESS;
}