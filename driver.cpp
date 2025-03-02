#include <fltKernel.h>
#include <dontuse.h>

#pragma comment(lib, "fltmgr.lib")

#define DRIVER_TAG 'FMon'
#define FILTER_ALTITUDE L"370050"  // Temporary test altitude (range 360000-389999 for file system tests)

typedef struct _DRIVER_CONTEXT {
    UNICODE_STRING MonitorPath;
} DRIVER_CONTEXT, * PDRIVER_CONTEXT;

DRIVER_CONTEXT gDriverContext;
PFLT_FILTER gFilterHandle = NULL;  // Global filter handle for cleanup

FLT_PREOP_CALLBACK_STATUS
PreOperationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Out_ PVOID* CompletionContext
)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    DbgPrint("PreOperationCallback called\n");
    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    NTSTATUS status;

    status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo);
    if (!NT_SUCCESS(status)) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    if ((Data->Iopb->MajorFunction == IRP_MJ_CREATE || Data->Iopb->MajorFunction == IRP_MJ_SET_INFORMATION) &&
        RtlPrefixUnicodeString(&gDriverContext.MonitorPath, &nameInfo->Name, TRUE)) {
        DbgPrint("File Operation - %wZ\n", &nameInfo->Name);
        DbgPrint("Operation Type: %s\n", (Data->Iopb->MajorFunction == IRP_MJ_CREATE) ? "CREATE" : "DELETE");
    }

    FltReleaseFileNameInformation(nameInfo);
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

const FLT_OPERATION_REGISTRATION Callbacks[] = {
    { IRP_MJ_CREATE, 0, PreOperationCallback, NULL },
    { IRP_MJ_SET_INFORMATION, 0, PreOperationCallback, NULL },
    { IRP_MJ_OPERATION_END }
};

NTSTATUS
FilterUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
)
{
    UNREFERENCED_PARAMETER(Flags);
    DbgPrint("FilterUnload called\n");
    if (gFilterHandle) {
        FltUnregisterFilter(gFilterHandle);
        gFilterHandle = NULL;
        DbgPrint("Filter unregistered\n");
    }
    return STATUS_SUCCESS;
}
NTSTATUS
InstanceSetup(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(VolumeDeviceType);
    UNREFERENCED_PARAMETER(VolumeFilesystemType);
    DbgPrint("InstanceSetup called\n");
    return STATUS_SUCCESS;
}


const FLT_REGISTRATION FilterRegistration = {
    sizeof(FLT_REGISTRATION),
    FLT_REGISTRATION_VERSION,
    0,
    NULL,
    Callbacks,
    FilterUnload,
    InstanceSetup,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    UNICODE_STRING altitude;

    DbgPrint("DriverEntry: Starting\n");
    UNREFERENCED_PARAMETER(RegistryPath);

    RtlInitUnicodeString(&gDriverContext.MonitorPath, L"\\??\\C:\\Test");
    DbgPrint("Monitoring path: %wZ\n", &gDriverContext.MonitorPath);

    RtlInitUnicodeString(&altitude, FILTER_ALTITUDE);
    status = FltRegisterFilter(DriverObject, &FilterRegistration, &gFilterHandle);
    if (!NT_SUCCESS(status)) {
        DbgPrint("FltRegisterFilter failed: 0x%08x\n", status);
        return status;
    }
    DbgPrint("Filter registered\n");

    status = FltStartFiltering(gFilterHandle);
    if (!NT_SUCCESS(status)) {
        DbgPrint("FltStartFiltering failed: 0x%08x\n", status);
        FltUnregisterFilter(gFilterHandle);
        gFilterHandle = NULL;
        return status;
    }
    DbgPrint("Filter started\n");

    return STATUS_SUCCESS;
}
