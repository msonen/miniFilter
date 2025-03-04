#pragma once
#include <fltKernel.h>
#include <dontuse.h>

/**
 * @struct _TRACKED_FILE_ENTRY
 * @brief Structure to hold each tracked filename in the linked list.
 *
 * This structure represents a single entry in the list of tracked files,
 * containing the filename and a link to the next/previous entries.
 */
typedef struct _TRACKED_FILE_ENTRY {
    LIST_ENTRY ListEntry;    ///< Must be the first member for LIST_ENTRY compatibility with the linked list.
    UNICODE_STRING FileName; ///< Stores the tracked filename as a UNICODE_STRING.
} TRACKED_FILE_ENTRY, *PTRACKED_FILE_ENTRY;

/**
 * @struct _TRACKED_FILES
 * @brief Global structure to manage the list of tracked files.
 *
 * This structure holds the head of the linked list of tracked files and a spinlock
 * for thread-safe operations.
 */
typedef struct _TRACKED_FILES {
    LIST_ENTRY FileListHead; ///< Head of the doubly-linked list of tracked file entries.
    KSPIN_LOCK Lock;         ///< Spinlock for synchronizing access to the list.
} TRACKED_FILES, *PTRACKED_FILES;

/**
 * @brief Initializes the tracked files list.
 *
 * Sets up the linked list head and initializes the spinlock for thread-safe operations.
 *
 * @param[in,out] TrackedFilesList Pointer to the TRACKED_FILES structure to initialize.
 * @return NTSTATUS STATUS_SUCCESS on success, or an appropriate error code.
 */
NTSTATUS InitializeTrackedFiles(PTRACKED_FILES TrackedFilesList);

/**
 * @brief Adds a file to the tracked files list.
 *
 * Allocates a new entry, copies the provided filename, and inserts it into the list.
 *
 * @param[in,out] TrackedFilesList Pointer to the TRACKED_FILES structure managing the list.
 * @param[in] FileName Pointer to a null-terminated wide-character string of the filename to track.
 * @return NTSTATUS STATUS_SUCCESS on success, STATUS_INSUFFICIENT_RESOURCES if allocation fails.
 */
NTSTATUS AddTrackedFile(PTRACKED_FILES TrackedFilesList, PCWSTR FileName);

/**
 * @brief Removes a file from the tracked files list.
 *
 * Searches for the specified file and removes it from the list if found, freeing its memory.
 *
 * @param[in,out] TrackedFilesList Pointer to the TRACKED_FILES structure managing the list.
 * @param[in] FilePath Pointer to a null-terminated wide-character string of the filename to remove.
 * @return NTSTATUS STATUS_SUCCESS if removed, STATUS_NOT_FOUND if not in the list.
 */
NTSTATUS RemoveTrackedFile(PTRACKED_FILES TrackedFilesList, PCWSTR FilePath);

/**
 * @brief Cleans up the tracked files list.
 *
 * Frees all entries in the list and resets the structure to an empty state.
 *
 * @param[in,out] TrackedFilesList Pointer to the TRACKED_FILES structure to clean up.
 */
VOID CleanupTrackedFiles(PTRACKED_FILES TrackedFilesList);

/**
 * @brief Checks if a file is in the tracked files list.
 *
 * Searches the list for a matching filename (case-insensitive).
 *
 * @param[in] TrackedFilesList Pointer to the TRACKED_FILES structure managing the list.
 * @param[in] FilePath Pointer to a UNICODE_STRING containing the filename to search for.
 * @param[out] FoundFilePath Optional pointer to a UNICODE_STRING to receive the matched filename (if found).
 * @return BOOLEAN TRUE if the file is found, FALSE otherwise.
 */
BOOLEAN GetTrackedFile(
    _In_ PTRACKED_FILES TrackedFilesList,
    _In_ PUNICODE_STRING FilePath,
    _Out_opt_ PUNICODE_STRING FoundFilePath
);