#include <ntddk.h>
#include "circularQ.h"
 
// Initialize the circular queue
NTSTATUS InitializeQueue(PCIRCULAR_QUEUE Queue, ULONG MessageSize, ULONG MaxMessages) {
    // Validate input parameters
    if (MessageSize == 0 || MaxMessages == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    // Allocate memory for the buffer
    Queue->Buffer = (PUCHAR)ExAllocatePool2(POOL_FLAG_NON_PAGED, MessageSize * MaxMessages, 'cQ');
    if (!Queue->Buffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Initialize queue metadata
    KeInitializeSpinLock(&Queue->Lock);
    Queue->MessageSize = MessageSize;
    Queue->MaxMessages = MaxMessages;
    Queue->Head = 0;
    Queue->Tail = 0;
    Queue->Count = 0;

    return STATUS_SUCCESS;
}

// Clean up the circular queue
VOID CleanupQueue(PCIRCULAR_QUEUE Queue) {
    if (Queue->Buffer) {
        ExFreePoolWithTag(Queue->Buffer, 'cQ');
        Queue->Buffer = NULL;
    }
}

// Enqueue a message
VOID Enqueue(PCIRCULAR_QUEUE Queue, PUCHAR Message) {
    KIRQL oldIrql;
    KeAcquireSpinLock(&Queue->Lock, &oldIrql);

    // Copy the message into the buffer
    ULONG offset = Queue->Tail * Queue->MessageSize;
    RtlCopyMemory(Queue->Buffer + offset, Message, Queue->MessageSize);

    Queue->Tail = (Queue->Tail + 1) % Queue->MaxMessages;

    if (Queue->Count == Queue->MaxMessages) {
        // Queue is full, overwrite the oldest message
        Queue->Head = (Queue->Head + 1) % Queue->MaxMessages;
    }
    else {
        Queue->Count++;
    }

    KeReleaseSpinLock(&Queue->Lock, oldIrql);

}

// Dequeue a message
BOOLEAN Dequeue(PCIRCULAR_QUEUE Queue, PUCHAR Message) {
    KIRQL oldIrql;
    BOOLEAN result = FALSE;

    // Acquire the spinlock
    KeAcquireSpinLock(&Queue->Lock, &oldIrql);

    if (Queue->Count > 0) {
        // Copy the message from the buffer
        ULONG offset = Queue->Head * Queue->MessageSize;
        RtlCopyMemory(Message, Queue->Buffer + offset, Queue->MessageSize);

        // Update queue metadata
        Queue->Head = (Queue->Head + 1) % Queue->MaxMessages;
        Queue->Count--;
        result = TRUE;
    }

    // Release the spinlock
    KeReleaseSpinLock(&Queue->Lock, oldIrql);

    return result;
}
