#pragma once
#include <fltKernel.h>
#include <dontuse.h>

// Structure to hold each tracked filename
typedef struct _TRACKED_FILE_ENTRY {
    LIST_ENTRY ListEntry;    // Must be first member for LIST_ENTRY
    UNICODE_STRING FileName; // Stores the filename
} TRACKED_FILE_ENTRY, * PTRACKED_FILE_ENTRY;

// Global structure to manage the list
typedef struct _TRACKED_FILES {
    LIST_ENTRY FileListHead;
    KSPIN_LOCK Lock;        // For synchronization
} TRACKED_FILES, * PTRACKED_FILES;


NTSTATUS InitializeTrackedFiles(PTRACKED_FILES TrackedFilesList);
NTSTATUS AddTrackedFile(PTRACKED_FILES TrackedFilesList, PCWSTR FileName);
VOID CleanupTrackedFiles(PTRACKED_FILES TrackedFilesList);
BOOLEAN GetTrackedFile(_In_ PTRACKED_FILES TrackedFilesList,
    _In_ PUNICODE_STRING FilePath,
    _Out_opt_ PUNICODE_STRING FoundFilePath  // Optional: returns the matched path
);