#include <fltKernel.h>
#include <dontuse.h>
#include <ntstrsafe.h>
#include "userApi.h"
#include "fileList.h"


extern TRACKED_FILES TrackedFiles;
extern PFLT_FILTER gFilterHandle = NULL;
static PFLT_PORT gClientPort = NULL;
static PFLT_PORT gServerPort = NULL;

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
    else if ((irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_REMOVE_TRACKED_FILE)) {
        status = IoctlRemoveFile(Irp, irpSp);
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

NTSTATUS OnConnect(
    _In_ PFLT_PORT ClientPort,
    _In_opt_ PVOID ServerPortCookie,
    _In_reads_bytes_opt_(SizeOfContext) PVOID ConnectionContext,
    _In_ ULONG SizeOfContext,
    _Outptr_result_maybenull_ PVOID* ConnectionCookie
) {
    UNREFERENCED_PARAMETER(ServerPortCookie);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(SizeOfContext);
    UNREFERENCED_PARAMETER(ConnectionCookie);

    gClientPort = ClientPort;
    DbgPrint("FileTracker: Client connected to port\n");
    return STATUS_SUCCESS;
}

VOID OnDisconnect(
    _In_opt_ PVOID ConnectionCookie
) {
    UNREFERENCED_PARAMETER(ConnectionCookie);

    FltCloseClientPort(gFilterHandle, &gClientPort);
    gClientPort = NULL;
    DbgPrint("FileTracker: Client disconnected from port\n");
}

NTSTATUS PortCreate() 
{
    UNICODE_STRING portName;
    OBJECT_ATTRIBUTES portAttributes;
    // Create communication port
    RtlInitUnicodeString(&portName, PORT_NAME);
    InitializeObjectAttributes(&portAttributes, &portName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
     return FltCreateCommunicationPort(gFilterHandle, &gServerPort, &portAttributes, NULL,
        OnConnect, OnDisconnect, NULL, 1);
}

NTSTATUS SendToUser(PUNICODE_STRING processName, PUNICODE_STRING name, PUNICODE_STRING timeString)
{
    NTSTATUS status = STATUS_SUCCESS;
    // Prepare message for user-mode
    DELETE_MESSAGE msg = { 0 };
    RtlCopyMemory(msg.ProcessName, processName->Buffer, min(processName->Length, sizeof(msg.ProcessName) - sizeof(WCHAR)));
    msg.ProcessName[sizeof(msg.ProcessName) / sizeof(WCHAR) - 1] = L'\0';
    RtlCopyMemory(msg.FilePath, name->Buffer, min(name->Length, sizeof(msg.FilePath) - sizeof(WCHAR)));
    msg.FilePath[sizeof(msg.FilePath) / sizeof(WCHAR) - 1] = L'\0';
    RtlCopyMemory(msg.DateTime, timeString->Buffer, min(timeString->Length, sizeof(msg.DateTime) - sizeof(WCHAR)));
    msg.DateTime[sizeof(msg.DateTime) / sizeof(WCHAR) - 1] = L'\0';

    // Send to user-mode if client is connected
    if (gClientPort) {
        status = FltSendMessage(gFilterHandle, &gClientPort, &msg, sizeof(DELETE_MESSAGE), NULL, 0, 0);
        if (!NT_SUCCESS(status)) {
            DbgPrint("FileTracker: Failed to send message to user-mode: 0x%08x\n", status);
        }
    }

    return status;
}