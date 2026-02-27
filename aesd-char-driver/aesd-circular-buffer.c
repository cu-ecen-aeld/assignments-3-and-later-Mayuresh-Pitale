/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @editor  Mayuresh Pitale 02/27/26
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    uint8_t index;
    int i;
    // Safety check
    if (buffer == NULL || entry_offset_byte_rtn == NULL) {
        return NULL;
    }
    // If the buffer is empty, return NULL
    if ((buffer->in_offs == buffer->out_offs) && !(buffer->full)) {
        return NULL;
    }

    index = buffer->out_offs; // Start at the oldest entry in the buffer
    // Loop through the buffer entries, checking if char_offset falls within the current entry
    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        if (char_offset < buffer->entry[index].size) {
            *entry_offset_byte_rtn = char_offset;
            return &(buffer->entry[index]);
        }
        // Move to the next entry and adjust char_offset accordingly
        char_offset -= buffer->entry[index].size;
        index = (index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        // If we've looped back to the write pointer and the buffer isn't full, char_offset is out of range
        if ((index == buffer->in_offs) && !(buffer->full)) {
            break;
        }
    }
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    // Safety check
    if (buffer == NULL || add_entry == NULL) {
        return;
    }

    buffer->entry[buffer->in_offs] = *add_entry;// Copy the new entry into the buffer at the current write offset

    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;// Move the write offset to the next position, wrapping around if necessary

    // If the buffer is full, move the read offset to the next position to make room for the new entry
    if (buffer->full) {
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    // If the write offset has caught up to the read offset, the buffer is now full
    if (buffer->in_offs == buffer->out_offs) {
        buffer->full = true;
    }
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
