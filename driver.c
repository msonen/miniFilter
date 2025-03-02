#include <fltKernel.h>
#include <dontuse.h>
#include <ntstrsafe.h>
#include "fileList.h"


#pragma comment(lib, "fltmgr.lib")

//#define DEBUG_MODE
#ifdef DEBUG_MODE
#define DEBUG(...)    DbgPrint(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

#define LOG(...)    DbgPrint(__VA_ARGS__)


static PFLT_FILTER gFilterHandle = NULL;
static TRACKED_FILES TrackedFiles;

static VOID LogDeletion(PUNICODE_STRING name) {
    PEPROCESS process = PsGetCurrentProcess();
    PUNICODE_STRING processName = NULL;
    NTSTATUS status = SeLocateProcessImageName(process, &processName);
    UNICODE_STRING defaultProcessName;
    if (!NT_SUCCESS(status) || !processName) {
        RtlInitUnicodeString(&defaultProcessName, L"Unknown Process");
        processName = &defaultProcessName;
    }

    // Get current time
    LARGE_INTEGER systemTime;
    LARGE_INTEGER localTime;
    TIME_FIELDS timeFields;
    KeQuerySystemTime(&systemTime);
    ExSystemTimeToLocalTime(&systemTime, &localTime);
    RtlTimeToTimeFields(&localTime, &timeFields);

    // Format datetime string (e.g., "2025-03-02 14:30:45")
    WCHAR timeBuffer[64];
    UNICODE_STRING timeString;
    timeString.Buffer = timeBuffer;
    timeString.MaximumLength = sizeof(timeBuffer);
    status = RtlUnicodeStringPrintf(&timeString,
        L"%04d-%02d-%02d %02d:%02d:%02d",
        timeFields.Year, timeFields.Month, timeFields.Day,
        timeFields.Hour, timeFields.Minute, timeFields.Second);
    if (!NT_SUCCESS(status)) {
        RtlInitUnicodeString(&timeString, L"Unknown Time");
    }

    // Log with process name, path, datetime, and operation
    LOG("FileLogger: Operation=DELETE, Process=%wZ, Path=%wZ, DateTime=%wZ\n",
        processName, name, &timeString);

    // Free process name if it was allocated
    if (processName != &defaultProcessName && processName->Buffer) {
        ExFreePool(processName->Buffer);
        ExFreePool(processName);
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

    status = FltGetFileNameInformation(Data, FLT_FILE_NAME_OPENED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo);
    if (!NT_SUCCESS(status)) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    if (!nameInfo->Name.Buffer) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    UNICODE_STRING filePath = nameInfo->Name;
    BOOLEAN fileMatches = GetTrackedFile(&TrackedFiles, &filePath, NULL);

    if (!fileMatches) {
        DEBUG("FileLogger: %wZ is not being tracked.\n", &nameInfo->Name);
        FltReleaseFileNameInformation(nameInfo);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    if (Data->Iopb->MajorFunction == IRP_MJ_SET_INFORMATION && NT_SUCCESS(Data->IoStatus.Status)) {
        dispInfo = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;

        if (dispInfo && dispInfo->DeleteFile) {
            LogDeletion(&nameInfo->Name);
        }
    }

    FltReleaseFileNameInformation(nameInfo);
    return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_PREOP_CALLBACK_STATUS
preOperationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext
)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    DEBUG("PostOperationCallback - MajorFunction: 0x%x, Status: 0x%08x\n",
        Data->Iopb->MajorFunction, Data->IoStatus.Status);

    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    NTSTATUS status;

    // Safely get the file name
    status = FltGetFileNameInformation(Data, FLT_FILE_NAME_OPENED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo);
    if (!NT_SUCCESS(status)) {
        DEBUG("FltGetFileNameInformation failed: 0x%08x\n", status);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    if (!nameInfo->Name.Buffer || wcsstr(nameInfo->Name.Buffer, L"\\Test\\") == NULL) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    DEBUG("File: %wZ\n", &nameInfo->Name);

    FltReleaseFileNameInformation(nameInfo);
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}



const FLT_OPERATION_REGISTRATION Callbacks[] = {
    //{ IRP_MJ_CREATE, 0, NULL, PostOperationCallback },
    { IRP_MJ_SET_INFORMATION, 0, NULL, PostOperationCallback },
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

#define BUILTIN_TRACK_LIST      2
static PCWSTR _filelist_builtin[BUILTIN_TRACK_LIST] =
{
    L"\\Device\\HarddiskVolume3\\Test\\cmd.txt", L"\\Device\\HarddiskVolume3\\Test\\a.txt"
};

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;

    DEBUG("DriverEntry: Starting\n");
    UNREFERENCED_PARAMETER(RegistryPath);

    status = InitializeTrackedFiles(&TrackedFiles);
    if (!NT_SUCCESS(status)) {
        DEBUG("InitializeTrackedFiles failed: 0x%08x\n", status);
        return status;
    }
    
    for (size_t i = 0; i < BUILTIN_TRACK_LIST; ++i) {
        status = AddTrackedFile(&TrackedFiles, _filelist_builtin[i]);

        if (!NT_SUCCESS(status)) {
            DEBUG("AddTrackedFile failed: 0x%08x\n", status);
            return status;
        }
    }

    status = FltRegisterFilter(DriverObject, &FilterRegistration, &gFilterHandle);
    if (!NT_SUCCESS(status)) {
        DEBUG("FltRegisterFilter failed: 0x%08x\n", status);
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

    return STATUS_SUCCESS;
}