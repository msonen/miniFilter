#include <fltKernel.h>
#include <dontuse.h>
#include <ntstrsafe.h>
#include "fileList.h"
#include "userApi.h"
#include "debug.h"


#pragma comment(lib, "fltmgr.lib")


PFLT_FILTER gFilterHandle = NULL;
TRACKED_FILES TrackedFiles;
PDEVICE_OBJECT gDeviceObject = NULL;


static VOID 
LogDeletion(PUNICODE_STRING name) {
    PEPROCESS process = PsGetCurrentProcess();
    PUNICODE_STRING processName = NULL;
    NTSTATUS status = SeLocateProcessImageName(process, &processName);
    UNICODE_STRING defaultProcessName;
    LARGE_INTEGER systemTime;
    LARGE_INTEGER localTime;
    TIME_FIELDS timeFields;
    WCHAR timeBuffer[64]; // Format datetime string (e.g., "2025-03-02 14:30:45")
    UNICODE_STRING timeString;

    if (!NT_SUCCESS(status) || !processName) {
        RtlInitUnicodeString(&defaultProcessName, L"Unknown Process");
        processName = &defaultProcessName;
    }

    // Get current time

    KeQuerySystemTime(&systemTime);
    ExSystemTimeToLocalTime(&systemTime, &localTime);
    RtlTimeToTimeFields(&localTime, &timeFields);


    timeString.Buffer = timeBuffer;
    timeString.MaximumLength = sizeof(timeBuffer);
    status = RtlUnicodeStringPrintf(&timeString,
        L"%04d-%02d-%02d %02d:%02d:%02d",
        timeFields.Year, timeFields.Month, timeFields.Day,
        timeFields.Hour, timeFields.Minute, timeFields.Second);
    if (NT_SUCCESS(status)) {
        SendToUser(processName, name, &timeString);
        // Log with process name, path, datetime, and operation
        LOG("FileLogger: Operation=DELETE, Process=%wZ, Path=%wZ, DateTime=%wZ\n",
            processName, name, &timeString);

        // Free process name if it was allocated
        if (processName != &defaultProcessName && processName->Buffer) {
            ExFreePool(processName->Buffer);
            ExFreePool(processName);
        }
    }
}

FLT_POSTOP_CALLBACK_STATUS
PostOperationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    NTSTATUS status;
    PFILE_DISPOSITION_INFORMATION dispInfo;

    status = FltGetFileNameInformation(Data, 
        FLT_FILE_NAME_OPENED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo);
    if (!NT_SUCCESS(status)) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    if (!nameInfo->Name.Buffer) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    UNICODE_STRING filePath = nameInfo->Name;
    BOOLEAN fileMatches = GetTrackedFile(&TrackedFiles, &filePath, NULL, NULL);

    if (!fileMatches) {
        DEBUG("FileLogger: %wZ is not being tracked.\n", &nameInfo->Name);
        FltReleaseFileNameInformation(nameInfo);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    if (Data->Iopb->MajorFunction == IRP_MJ_SET_INFORMATION
        && NT_SUCCESS(Data->IoStatus.Status)) {
        dispInfo = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;

        if (dispInfo && dispInfo->DeleteFile) {
            LogDeletion(&nameInfo->Name);
        }
    }

    FltReleaseFileNameInformation(nameInfo);
    return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_PREOP_CALLBACK_STATUS PreOperationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
) {
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    if (Data->Iopb->MajorFunction == IRP_MJ_SET_INFORMATION) {
        PFILE_DISPOSITION_INFORMATION dispInfo = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
        if (dispInfo && dispInfo->DeleteFile) {
            PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
            NTSTATUS status = FltGetFileNameInformation(Data, FLT_FILE_NAME_OPENED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo);
            if (NT_SUCCESS(status) && nameInfo->Name.Buffer) {
                BOOLEAN protected = FALSE;
                if (GetTrackedFile(&TrackedFiles, &nameInfo->Name, NULL, &protected) && protected) {
                    LOG("FileTracker: Blocked deletion of protected file %wZ\n", &nameInfo->Name);
                    FltReleaseFileNameInformation(nameInfo);
                    Data->IoStatus.Status = STATUS_ACCESS_DENIED;
                    return FLT_PREOP_COMPLETE;
                }
                FltReleaseFileNameInformation(nameInfo);
            }
        }
    }

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}


const FLT_OPERATION_REGISTRATION Callbacks[] = {
    //{ IRP_MJ_CREATE, 0, NULL, PostOperationCallback },
    { IRP_MJ_SET_INFORMATION, 0, PreOperationCallback, PostOperationCallback },
    //{ IRP_MJ_CLEANUP, 0, NULL, PostOperationCallback },
    { IRP_MJ_OPERATION_END }
};

NTSTATUS
FilterUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
)
{
    UNREFERENCED_PARAMETER(Flags);
    DEBUG("FilterUnload called\n");
    CleanupTrackedFiles(&TrackedFiles);
    if (gFilterHandle) {
        FltUnregisterFilter(gFilterHandle);
        gFilterHandle = NULL;
        LOG("Filter unregistered\n");
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
    DEBUG("InstanceSetup called - FsType: %d\n", VolumeFilesystemType);
    if (VolumeFilesystemType == FLT_FSTYPE_NTFS) {
        return STATUS_SUCCESS;
    }
    return STATUS_FLT_DO_NOT_ATTACH;
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


// Driver unload routine
VOID DriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNICODE_STRING symlinkName;
    LOG("driverFlt: Driver unload routine.");
    IoctlClear();
    RtlInitUnicodeString(&symlinkName, SYMLINK_NAME);

    IoDeleteSymbolicLink(&symlinkName);
    DEBUG("driverFlt: Symbolic link removed.");
    if (gDeviceObject) {
        IoDeleteDevice(gDeviceObject);
    }

    CleanupTrackedFiles(&TrackedFiles);
    LOG("driverFlt: Driver unloaded.");
}


NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    UNICODE_STRING deviceName;
    UNICODE_STRING symlinkName;

    DEBUG("DriverEntry: Starting\n");
    UNREFERENCED_PARAMETER(RegistryPath);

    status = InitializeTrackedFiles(&TrackedFiles);
    if (!NT_SUCCESS(status)) {
        DEBUG("InitializeTrackedFiles failed, 0x%08x\n", status);
        return status;
    }
    
    // Create device object
    RtlInitUnicodeString(&deviceName, DEVICE_NAME);
    status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 
            0, FALSE, &gDeviceObject);
    if (!NT_SUCCESS(status)) {
        LOG("driverFlt: Failed to create device, 0x%08x\n", status);
        CleanupTrackedFiles(&TrackedFiles);
        return status;
    }

    // Create symbolic link
    RtlInitUnicodeString(&symlinkName, SYMLINK_NAME);
    status = IoCreateSymbolicLink(&symlinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        LOG("driverFlt: Failed to create symlink, 0x%08x\n", status);
        IoDeleteDevice(gDeviceObject);
        CleanupTrackedFiles(&TrackedFiles);
        return status;
    }

    // Set up dispatch routines
    DriverObject->MajorFunction[IRP_MJ_CREATE] = IoctlCreateDispatch;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IoctlControl;
    DriverObject->DriverUnload = DriverUnload;

    status = FltRegisterFilter(DriverObject, &FilterRegistration, &gFilterHandle);
    if (!NT_SUCCESS(status)) {
        DEBUG("driverFlt: FltRegisterFilter failed, 0x%08x\n", status);
        return status;
    }
    DEBUG("Filter registered\n");

    status = FltStartFiltering(gFilterHandle);
    if (!NT_SUCCESS(status)) {
        DEBUG("FltStartFiltering failed: 0x%08x\n", status);
        FltUnregisterFilter(gFilterHandle);
        gFilterHandle = NULL;
        return status;
    }
    LOG("Filter started\n");
    
    status = IoctlInit();
    if (!NT_SUCCESS(status)) {
        DbgPrint("driverFlt: Failed to create comm port: 0x%08x\n", status);
        IoDeleteSymbolicLink(&symlinkName);
        IoDeleteDevice(gDeviceObject);
        CleanupTrackedFiles(&TrackedFiles);
        return status;
    }
    
    return STATUS_SUCCESS;
}