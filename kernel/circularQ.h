/**
 * @file circular_queue.h
 * @brief A thread-safe circular queue implementation for the Windows Kernel.
 *
 * This module provides a simple circular queue that can be used in kernel-mode
 * drivers. The queue supports variable-sized messages and is protected by a
 * spinlock for thread safety.
 * 
 */

 #pragma once

 #include <ntddk.h>
 
 /**
  * @struct CIRCULAR_QUEUE
  * @brief Represents a circular queue for storing messages.
  */
typedef struct _CIRCULAR_QUEUE {
    KSPIN_LOCK Lock;       // Spinlock for synchronization
    PUCHAR Buffer;         // Pointer to the circular buffer
    ULONG MessageSize;     // Size of each message in bytes
    ULONG MaxMessages;     // Maximum number of messages the queue can hold
    ULONG Head;            // Index of the first message
    ULONG Tail;            // Index of the next free slot
    ULONG Count;           // Number of messages in the queue
} CIRCULAR_QUEUE, * PCIRCULAR_QUEUE;
 
 /**
  * @brief Initializes a circular queue.
  *
  * This function allocates memory for the queue buffer and initializes the queue
  * metadata. The queue can hold up to `MaxMessages` messages, each of size
  * `MessageSize`.
  *
  * @param Queue Pointer to the CIRCULAR_QUEUE structure to initialize.
  * @param MessageSize Size of each message in bytes.
  * @param MaxMessages Maximum number of messages the queue can hold.
  * @return NTSTATUS STATUS_SUCCESS on success, or an error code on failure.
  */
 NTSTATUS InitializeQueue(PCIRCULAR_QUEUE Queue, ULONG MessageSize, ULONG MaxMessages);
 
 /**
  * @brief Cleans up a circular queue.
  *
  * This function frees the memory allocated for the queue buffer. It should be
  * called when the queue is no longer needed to avoid memory leaks.
  *
  * @param Queue Pointer to the CIRCULAR_QUEUE structure to clean up.
  */
 VOID CleanupQueue(PCIRCULAR_QUEUE Queue);
 
 /**
  * @brief Enqueues a message into the circular queue.
  *
  * This function adds a message to the queue. If the queue is full, the oldest
  * message is overwritten by the new message.
  *
  * @param Queue Pointer to the CIRCULAR_QUEUE structure.
  * @param Message Pointer to the message to enqueue.
  */
 VOID Enqueue(PCIRCULAR_QUEUE Queue, PUCHAR Message);
 
 /**
  * @brief Dequeues a message from the circular queue.
  *
  * This function removes a message from the queue and copies it into the provided
  * buffer. If the queue is empty, the function returns FALSE.
  *
  * @param Queue Pointer to the CIRCULAR_QUEUE structure.
  * @param Message Pointer to the buffer where the dequeued message will be stored.
  * @return BOOLEAN TRUE if a message was dequeued successfully, FALSE if the
  *         queue is empty.
  */
 BOOLEAN Dequeue(PCIRCULAR_QUEUE Queue, PUCHAR Message);
 