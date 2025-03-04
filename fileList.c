#include <fltKernel.h>
#include <dontuse.h>
#include "fileList.h"


// Initialization function
NTSTATUS InitializeTrackedFiles(PTRACKED_FILES TrackedFilesList)
{
    InitializeListHead(&TrackedFilesList->FileListHead);
    KeInitializeSpinLock(&TrackedFilesList->Lock);
    return STATUS_SUCCESS;
}

NTSTATUS 
AddTrackedFile(PTRACKED_FILES TrackedFilesList, PCWSTR FileName, BOOLEAN Protected) {
    KIRQL oldIrql;
    PTRACKED_FILE_ENTRY entry = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(TRACKED_FILE_ENTRY), 'kFtL');
    if (!entry) return STATUS_INSUFFICIENT_RESOURCES;

    SIZE_T pathLength = wcslen(FileName) * sizeof(WCHAR);
    entry->FileName.Buffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, pathLength + sizeof(WCHAR), 'kFtL');
    if (!entry->FileName.Buffer) {
        ExFreePool(entry);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    entry->FileName.Length = (USHORT)pathLength;
    entry->FileName.MaximumLength = (USHORT)(pathLength + sizeof(WCHAR));
    RtlCopyMemory(entry->FileName.Buffer, FileName, pathLength);
    entry->FileName.Buffer[pathLength / sizeof(WCHAR)] = L'\0';
    entry->Protected = Protected;

    KeAcquireSpinLock(&TrackedFilesList->Lock, &oldIrql);
    InsertTailList(&TrackedFilesList->FileListHead, &entry->ListEntry);
    KeReleaseSpinLock(&TrackedFilesList->Lock, oldIrql);
    return STATUS_SUCCESS;
}

NTSTATUS RemoveTrackedFile(PTRACKED_FILES TrackedFilesList, PCWSTR FilePath) {
    NTSTATUS status = STATUS_NOT_FOUND;
    KIRQL oldIrql;
    UNICODE_STRING fileToRemove;
    RtlInitUnicodeString(&fileToRemove, FilePath);

    KeAcquireSpinLock(&TrackedFilesList->Lock, &oldIrql);
    PLIST_ENTRY entry = TrackedFilesList->FileListHead.Flink;
    while (entry != &TrackedFilesList->FileListHead) {
        PTRACKED_FILE_ENTRY fileEntry = CONTAINING_RECORD(entry, TRACKED_FILE_ENTRY, ListEntry);
        if (RtlEqualUnicodeString(&fileToRemove, &fileEntry->FileName, TRUE)) {
            RemoveEntryList(&fileEntry->ListEntry);
            if (fileEntry->FileName.Buffer) ExFreePool(fileEntry->FileName.Buffer);
            ExFreePool(fileEntry);
            status = STATUS_SUCCESS;
            break;
        }
        entry = entry->Flink;
    }
    KeReleaseSpinLock(&TrackedFilesList->Lock, oldIrql);
    return status;
}

// Cleanup function (updated to use simpler ExFreePool)
VOID CleanupTrackedFiles(PTRACKED_FILES TrackedFilesList)
{
    KIRQL oldIrql;
    KeAcquireSpinLock(&TrackedFilesList->Lock, &oldIrql);

    while (!IsListEmpty(&TrackedFilesList->FileListHead)) {
        PLIST_ENTRY entry = RemoveHeadList(&TrackedFilesList->FileListHead);
        PTRACKED_FILE_ENTRY fileEntry = CONTAINING_RECORD(entry, TRACKED_FILE_ENTRY, ListEntry);

        if (fileEntry->FileName.Buffer) {
            ExFreePool(fileEntry->FileName.Buffer);
        }
        ExFreePool(fileEntry);
    }

    KeReleaseSpinLock(&TrackedFilesList->Lock, oldIrql);
}

BOOLEAN 
GetTrackedFile(PTRACKED_FILES TrackedFilesList, PUNICODE_STRING FilePath, 
    PUNICODE_STRING FoundFilePath, PBOOLEAN Protected) {
    KIRQL oldIrql;
    BOOLEAN fileExists = FALSE;

    KeAcquireSpinLock(&TrackedFilesList->Lock, &oldIrql);
    PLIST_ENTRY entry = TrackedFilesList->FileListHead.Flink;
    while (entry != &TrackedFilesList->FileListHead) {
        PTRACKED_FILE_ENTRY fileEntry = CONTAINING_RECORD(entry, TRACKED_FILE_ENTRY, ListEntry);
        if (RtlEqualUnicodeString(FilePath, &fileEntry->FileName, TRUE)) {
            fileExists = TRUE;
            if (FoundFilePath) *FoundFilePath = fileEntry->FileName;
            if (Protected) *Protected = fileEntry->Protected;
            break;
        }
        entry = entry->Flink;
    }
    KeReleaseSpinLock(&TrackedFilesList->Lock, oldIrql);
    return fileExists;
}