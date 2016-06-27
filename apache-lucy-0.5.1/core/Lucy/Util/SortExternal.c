/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define C_LUCY_SORTEXTERNAL
#include "Lucy/Util/ToolSet.h"

#include "Lucy/Util/SortExternal.h"
#include "Clownfish/Util/SortUtils.h"

// Refill the main buffer, drawing from the buffers of all runs.
static void
S_refill_buffer(SortExternal *self, SortExternalIVARS *ivars);

// Absorb all the items which are "in-range" from all the Runs into the main
// buffer.
static void
S_absorb_slices(SortExternal *self, SortExternalIVARS *ivars,
                Obj **endpost);

static void
S_merge(SortExternal *self,
        Obj **left_ptr,  size_t left_size,
        Obj **right_ptr, size_t right_size,
        Obj **dest, SortEx_Compare_t compare);

// Return the address for the item in one of the runs' buffers which is the
// highest in sort order, but which we can guarantee is lower in sort order
// than any item which has yet to enter a run buffer.
static Obj**
S_find_endpost(SortExternal *self, SortExternalIVARS *ivars);

// Determine how many buffered items are less than or equal to `endpost`.
static uint32_t
S_find_slice_size(SortExternal *self, SortExternalIVARS *ivars,
                  Obj **endpost);

SortExternal*
SortEx_init(SortExternal *self) {
    SortExternalIVARS *const ivars = SortEx_IVARS(self);

    ivars->mem_thresh   = UINT32_MAX;
    ivars->buffer       = NULL;
    ivars->buf_cap      = 0;
    ivars->buf_max      = 0;
    ivars->buf_tick     = 0;
    ivars->scratch      = NULL;
    ivars->scratch_cap  = 0;
    ivars->runs         = Vec_new(0);
    ivars->slice_sizes  = NULL;
    ivars->slice_starts = NULL;
    ivars->flipped      = false;

    ABSTRACT_CLASS_CHECK(self, SORTEXTERNAL);
    return self;
}

void
SortEx_Destroy_IMP(SortExternal *self) {
    SortExternalIVARS *const ivars = SortEx_IVARS(self);
    FREEMEM(ivars->scratch);
    FREEMEM(ivars->slice_sizes);
    FREEMEM(ivars->slice_starts);
    if (ivars->buffer) {
        SortEx_Clear_Buffer(self);
        FREEMEM(ivars->buffer);
    }
    DECREF(ivars->runs);
    SUPER_DESTROY(self, SORTEXTERNAL);
}

void
SortEx_Clear_Buffer_IMP(SortExternal *self) {
    SortExternalIVARS *const ivars = SortEx_IVARS(self);
    Obj **const buffer = ivars->buffer;
    const uint32_t max = ivars->buf_max;
    for (uint32_t i = ivars->buf_tick; i < max; i++) {
        DECREF(buffer[i]);
    }
    ivars->buf_max    = 0;
    ivars->buf_tick   = 0;
}

void
SortEx_Feed_IMP(SortExternal *self, Obj *item) {
    SortExternalIVARS *const ivars = SortEx_IVARS(self);
    if (ivars->buf_max == ivars->buf_cap) {
        size_t amount = Memory_oversize(ivars->buf_max + 1, sizeof(Obj*));
        SortEx_Grow_Buffer(self, amount);
    }
    ivars->buffer[ivars->buf_max] = item;
    ivars->buf_max++;
}

static CFISH_INLINE Obj*
SI_peek(SortExternal *self, SortExternalIVARS *ivars) {
    if (ivars->buf_tick >= ivars->buf_max) {
        S_refill_buffer(self, ivars);
    }

    if (ivars->buf_max > 0) {
        return ivars->buffer[ivars->buf_tick];
    }
    else {
        return NULL;
    }
}

Obj*
SortEx_Fetch_IMP(SortExternal *self) {
    SortExternalIVARS *const ivars = SortEx_IVARS(self);
    Obj *item = SI_peek(self, ivars);
    ivars->buf_tick++;
    return item;
}

Obj*
SortEx_Peek_IMP(SortExternal *self) {
    SortExternalIVARS *const ivars = SortEx_IVARS(self);
    return SI_peek(self, ivars);
}

void
SortEx_Sort_Buffer_IMP(SortExternal *self) {
    SortExternalIVARS *const ivars = SortEx_IVARS(self);
    if (ivars->buf_tick != 0) {
        THROW(ERR, "Cant Sort_Buffer() after fetching %u32 items", ivars->buf_tick);
    }
    if (ivars->buf_max != 0) {
        Class *klass = SortEx_get_class(self);
        CFISH_Sort_Compare_t compare
            = (CFISH_Sort_Compare_t)METHOD_PTR(klass, LUCY_SortEx_Compare);
        if (ivars->scratch_cap < ivars->buf_cap) {
            ivars->scratch_cap = ivars->buf_cap;
            ivars->scratch
                = (Obj**)REALLOCATE(ivars->scratch,
                                    ivars->scratch_cap * sizeof(Obj*));
        }
        Sort_mergesort(ivars->buffer, ivars->scratch, ivars->buf_max,
                       sizeof(Obj*), compare, self);
    }
}

void
SortEx_Flip_IMP(SortExternal *self) {
    SortEx_Flush(self);
    SortEx_IVARS(self)->flipped = true;
}

void
SortEx_Add_Run_IMP(SortExternal *self, SortExternal *run) {
    SortExternalIVARS *const ivars = SortEx_IVARS(self);
    Vec_Push(ivars->runs, (Obj*)run);
    uint32_t num_runs = Vec_Get_Size(ivars->runs);
    ivars->slice_sizes
        = (uint32_t*)REALLOCATE(ivars->slice_sizes,
                                num_runs * sizeof(uint32_t));
    ivars->slice_starts
        = (Obj***)REALLOCATE(ivars->slice_starts, num_runs * sizeof(Obj**));
}

void
SortEx_Shrink_IMP(SortExternal *self) {
    SortExternalIVARS *const ivars = SortEx_IVARS(self);
    if (ivars->buf_max - ivars->buf_tick > 0) {
        size_t buf_count = SortEx_Buffer_Count(self);
        size_t size        = buf_count * sizeof(Obj*);
        if (ivars->buf_tick > 0) {
            Obj **start = ivars->buffer + ivars->buf_tick;
            memmove(ivars->buffer, start, size);
        }
        ivars->buffer   = (Obj**)REALLOCATE(ivars->buffer, size);
        ivars->buf_tick = 0;
        ivars->buf_max  = buf_count;
        ivars->buf_cap  = buf_count;
    }
    else {
        FREEMEM(ivars->buffer);
        ivars->buffer   = NULL;
        ivars->buf_tick = 0;
        ivars->buf_max  = 0;
        ivars->buf_cap  = 0;
    }
    ivars->scratch_cap = 0;
    FREEMEM(ivars->scratch);
    ivars->scratch = NULL;

    for (uint32_t i = 0, max = Vec_Get_Size(ivars->runs); i < max; i++) {
        SortExternal *run = (SortExternal*)Vec_Fetch(ivars->runs, i);
        SortEx_Shrink(run);
    }
}

static void
S_refill_buffer(SortExternal *self, SortExternalIVARS *ivars) {
    // Reset buffer vars.
    SortEx_Clear_Buffer(self);

    // Make sure all runs have at least one item in the buffer.
    uint32_t i = 0;
    while (i < Vec_Get_Size(ivars->runs)) {
        SortExternal *const run = (SortExternal*)Vec_Fetch(ivars->runs, i);
        if (SortEx_Buffer_Count(run) > 0 || SortEx_Refill(run) > 0) {
            i++; // Run has some elements, so keep.
        }
        else {
            Vec_Excise(ivars->runs, i, 1);
        }
    }

    // Absorb as many elems as possible from all runs into main buffer.
    if (Vec_Get_Size(ivars->runs)) {
        Obj **endpost = S_find_endpost(self, ivars);
        S_absorb_slices(self, ivars, endpost);
    }
}

static Obj**
S_find_endpost(SortExternal *self, SortExternalIVARS *ivars) {
    Obj **endpost = NULL;

    for (uint32_t i = 0, max = Vec_Get_Size(ivars->runs); i < max; i++) {
        // Get a run and retrieve the last item in its buffer.
        SortExternal *const run = (SortExternal*)Vec_Fetch(ivars->runs, i);
        SortExternalIVARS *const run_ivars = SortEx_IVARS(run);
        const uint32_t tick = run_ivars->buf_max - 1;
        if (tick >= run_ivars->buf_cap || run_ivars->buf_max < 1) {
            THROW(ERR, "Invalid SortExternal buffer access: %u32 %u32 %u32", tick,
                  run_ivars->buf_max, run_ivars->buf_cap);
        }
        else {
            // Cache item with the highest sort value currently held in memory
            // by the run.
            Obj **candidate = run_ivars->buffer + tick;

            // If it's the first run, item is automatically the new endpost.
            if (i == 0) {
                endpost = candidate;
            }
            // If it's less than the current endpost, it's the new endpost.
            else if (SortEx_Compare(self, candidate, endpost) < 0) {
                endpost = candidate;
            }
        }
    }

    return endpost;
}

static void
S_absorb_slices(SortExternal *self, SortExternalIVARS *ivars,
                Obj **endpost) {
    uint32_t    num_runs     = Vec_Get_Size(ivars->runs);
    Obj      ***slice_starts = ivars->slice_starts;
    uint32_t   *slice_sizes  = ivars->slice_sizes;
    Class      *klass        = SortEx_get_class(self);
    SortEx_Compare_t compare = METHOD_PTR(klass, LUCY_SortEx_Compare);

    if (ivars->buf_max != 0) { THROW(ERR, "Can't refill unless empty"); }

    // Find non-empty slices.
    uint32_t num_slices = 0;
    uint32_t total_size = 0;
    for (uint32_t i = 0; i < num_runs; i++) {
        SortExternal *const run = (SortExternal*)Vec_Fetch(ivars->runs, i);
        SortExternalIVARS *const run_ivars = SortEx_IVARS(run);
        uint32_t slice_size = S_find_slice_size(run, run_ivars, endpost);

        if (slice_size) {
            // Track slice start and size.
            slice_starts[num_slices] = run_ivars->buffer + run_ivars->buf_tick;
            slice_sizes[num_slices]  = slice_size;
            run_ivars->buf_tick += slice_size;

            num_slices++;
            total_size += slice_size;
        }
    }

    if (num_slices == 0) { return; }

    if (ivars->buf_cap < total_size) {
        size_t cap = Memory_oversize(total_size, sizeof(Obj*));
        SortEx_Grow_Buffer(self, cap);
    }
    ivars->buf_max = total_size;

    if (num_slices == 1) {
        // Copy single slice content from run buffer to main buffer.
        memcpy(ivars->buffer, slice_starts[0], total_size * sizeof(Obj*));
        return;
    }

    // There are more than two slices to merge.
    if (ivars->scratch_cap < total_size) {
        ivars->scratch_cap = total_size;
        ivars->scratch = (Obj**)REALLOCATE(
                            ivars->scratch, ivars->scratch_cap * sizeof(Obj*));
    }

    // Divide-and-conquer k-way merge.
    while (num_slices > 1) {
        uint32_t i = 0;
        uint32_t j = 0;
        Obj **dest = ivars->scratch;

        while (i < num_slices) {
            if (num_slices - i >= 2) {
                // Merge two consecutive slices.
                const uint32_t merged_size = slice_sizes[i] + slice_sizes[i + 1];
                S_merge(self,
                        slice_starts[i], slice_sizes[i],
                        slice_starts[i + 1], slice_sizes[i + 1],
                        dest, compare);
                slice_sizes[j]  = merged_size;
                slice_starts[j] = dest;
                dest += merged_size;
                i += 2;
                j += 1;
            }
            else if (num_slices - i >= 1) {
                // Move last slice.
                memcpy(dest, slice_starts[i], slice_sizes[i] * sizeof(Obj*));
                slice_sizes[j]  = slice_sizes[i];
                slice_starts[j] = dest;
                i += 1;
                j += 1;
            }
        }
        num_slices = j;

        // Swap scratch and buffer.
        Obj      **tmp_buf = ivars->buffer;
        uint32_t   tmp_cap = ivars->buf_cap;
        ivars->buffer      = ivars->scratch;
        ivars->buf_cap     = ivars->scratch_cap;
        ivars->scratch     = tmp_buf;
        ivars->scratch_cap = tmp_cap;
    }
}

// Assumes left_size > 0 and right_size > 0.
static void
S_merge(SortExternal *self,
        Obj **left_ptr,  size_t left_size,
        Obj **right_ptr, size_t right_size,
        Obj **dest, SortEx_Compare_t compare) {

    Obj **left_limit  = left_ptr  + left_size;
    Obj **right_limit = right_ptr + right_size;

    while (1) {
        if (compare(self, left_ptr, right_ptr) <= 0) {
            *dest++ = *left_ptr++;
            if (left_ptr >= left_limit) {
                right_size = right_limit - right_ptr;
                memcpy(dest, right_ptr, right_size * sizeof(Obj*));
                break;
            }
        }
        else {
            *dest++ = *right_ptr++;
            if (right_ptr >= right_limit) {
                left_size = left_limit - left_ptr;
                memcpy(dest, left_ptr, left_size * sizeof(Obj*));
                break;
            }
        }
    }
}

void
SortEx_Grow_Buffer_IMP(SortExternal *self, uint32_t cap) {
    SortExternalIVARS *const ivars = SortEx_IVARS(self);
    if (cap > ivars->buf_cap) {
        ivars->buffer = (Obj**)REALLOCATE(ivars->buffer, cap * sizeof(Obj*));
        ivars->buf_cap = cap;
    }
}

static uint32_t
S_find_slice_size(SortExternal *self, SortExternalIVARS *ivars,
                  Obj **endpost) {
    int32_t          lo      = ivars->buf_tick - 1;
    int32_t          hi      = ivars->buf_max;
    Obj            **buffer  = ivars->buffer;
    SortEx_Compare_t compare
        = METHOD_PTR(SortEx_get_class(self), LUCY_SortEx_Compare);

    // Binary search.
    while (hi - lo > 1) {
        const int32_t mid   = lo + ((hi - lo) / 2);
        const int32_t delta = compare(self, buffer + mid, endpost);
        if (delta > 0) { hi = mid; }
        else           { lo = mid; }
    }

    // If lo is still -1, we didn't find anything.
    return lo == -1
           ? 0
           : (lo - ivars->buf_tick) + 1;
}

void
SortEx_Set_Mem_Thresh_IMP(SortExternal *self, uint32_t mem_thresh) {
    SortEx_IVARS(self)->mem_thresh = mem_thresh;
}

uint32_t
SortEx_Buffer_Count_IMP(SortExternal *self) {
    SortExternalIVARS *const ivars = SortEx_IVARS(self);
    return ivars->buf_max - ivars->buf_tick;
}


