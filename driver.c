#include <fltKernel.h>
#include <dontuse.h>

#pragma comment(lib, "fltmgr.lib")


PFLT_FILTER gFilterHandle = NULL;

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

    //DbgPrint("PostOperationCallback - MajorFunction: 0x%x, Status: 0x%08x\n",
        //Data->Iopb->MajorFunction, Data->IoStatus.Status);

    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    NTSTATUS status;
    PFILE_DISPOSITION_INFORMATION dispInfo;

    // Safely get the file name
    status = FltGetFileNameInformation(Data, FLT_FILE_NAME_OPENED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo);
    if (!NT_SUCCESS(status)) {
        //DbgPrint("FltGetFileNameInformation failed: 0x%08x\n", status);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }
    
    if (!nameInfo->Name.Buffer || wcsstr(nameInfo->Name.Buffer, L"\\Test\\") == NULL) 
    {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    //DbgPrint("File: %wZ\n", &nameInfo->Name);

    if (Data->Iopb->MajorFunction == IRP_MJ_SET_INFORMATION && NT_SUCCESS(Data->IoStatus.Status)) {
        dispInfo = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;

        if (dispInfo && dispInfo->DeleteFile) {
            DbgPrint("FileLogger: File deleted - %wZ\n", &nameInfo->Name);
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

    //DbgPrint("PostOperationCallback - MajorFunction: 0x%x, Status: 0x%08x\n",
        //Data->Iopb->MajorFunction, Data->IoStatus.Status);

    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    NTSTATUS status;

    // Safely get the file name
    status = FltGetFileNameInformation(Data, FLT_FILE_NAME_OPENED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo);
    if (!NT_SUCCESS(status)) {
        //DbgPrint("FltGetFileNameInformation failed: 0x%08x\n", status);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    if (!nameInfo->Name.Buffer || wcsstr(nameInfo->Name.Buffer, L"\\Test\\") == NULL) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    DbgPrint("File: %wZ\n", &nameInfo->Name);


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
    DbgPrint("InstanceSetup called - FsType: %d\n", VolumeFilesystemType);
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


NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;

    DbgPrint("DriverEntry: Starting\n");
    UNREFERENCED_PARAMETER(RegistryPath);

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