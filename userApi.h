
#pragma once
#include <fltKernel.h>
#include <dontuse.h>

/**
 * @def IOCTL_ADD_TRACKED_FILE
 * @brief IOCTL code to add a file to the tracking list.
 *
 * This control code is used by user-mode applications to instruct the driver to start tracking a specified file.
 */
#define IOCTL_ADD_TRACKED_FILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

/**
 * @def IOCTL_REMOVE_TRACKED_FILE
 * @brief IOCTL code to remove a file from the tracking list.
 *
 * This control code is used by user-mode applications to stop tracking a specified file.
 */
#define IOCTL_REMOVE_TRACKED_FILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

/**
 * @def IOCTL_GET_DELETE_MESSAGE
 * @brief IOCTL code to retrieve a deletion message from the queue.
 *
 * This control code is used by user-mode applications to fetch the oldest deletion event from the driver’s circular queue.
 */
#define IOCTL_GET_DELETE_MESSAGE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

/**
 * @def DEVICE_NAME
 * @brief Kernel-mode device name for the driver.
 *
 * The internal name used by the kernel to identify the driver device object.
 */
#define DEVICE_NAME L"\\Device\\FileTracker"

/**
 * @def SYMLINK_NAME
 * @brief User-mode symbolic link name for the driver.
 *
 * The symbolic link name that user-mode applications use to communicate with the driver.
 */
#define SYMLINK_NAME L"\\DosDevices\\FileTracker"

/**
 * @def MAX_MESSAGES
 * @brief Maximum number of messages in the circular queue.
 *
 * Defines the fixed size of the circular queue used to store deletion messages (10 messages).
 */
#define MAX_MESSAGES 10

/**
 * @brief Handles IOCTL requests from user-mode applications.
 *
 * Processes control codes such as adding/removing tracked files or retrieving deletion messages.
 *
 * @param[in] DeviceObject Pointer to the device object associated with the driver.
 * @param[in] Irp Pointer to the I/O Request Packet (IRP) containing the IOCTL request.
 * @return NTSTATUS STATUS_SUCCESS on success, or an appropriate error code (e.g., STATUS_INVALID_DEVICE_REQUEST).
 */
NTSTATUS 
IoctlControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
);

/**
 * @brief Handles device creation requests.
 *
 * Called when a user-mode application opens a handle to the driver’s device.
 *
 * @param[in] DeviceObject Pointer to the device object being created.
 * @param[in] Irp Pointer to the IRP for the create request.
 * @return NTSTATUS STATUS_SUCCESS on success, or an appropriate error code.
 */
NTSTATUS 
IoctlCreateDispatch(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
);

/**
 * @brief Sends a deletion message to the user-mode queue.
 *
 * Constructs and enqueues a deletion message containing process name, file path, and timestamp.
 *
 * @param[in] processName Pointer to a UNICODE_STRING with the process name that performed the deletion.
 * @param[in] name Pointer to a UNICODE_STRING with the file path that was deleted.
 * @param[in] timeString Pointer to a UNICODE_STRING with the timestamp of the deletion.
 * @return NTSTATUS STATUS_SUCCESS if enqueued, STATUS_INSUFFICIENT_RESOURCES if queue is full.
 */
NTSTATUS 
SendToUser(
    PUNICODE_STRING processName, 
    PUNICODE_STRING name, 
    PUNICODE_STRING timeString
);

/**
 * @brief Initializes the IOCTL handling subsystem.
 *
 * Sets up necessary structures (e.g., circular queue) for IOCTL operations.
 *
 * @return NTSTATUS STATUS_SUCCESS on success, or an appropriate error code.
 */
NTSTATUS 
IoctlInit();

/**
 * @brief Clears the IOCTL subsystem state.
 *
 * Resets the circular queue and any associated resources, freeing memory as needed.
 *
 * @return NTSTATUS STATUS_SUCCESS on success, or an appropriate error code.
 */
NTSTATUS
IoctlClear();