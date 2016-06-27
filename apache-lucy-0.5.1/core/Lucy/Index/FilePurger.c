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

#define C_LUCY_FILEPURGER
#include <ctype.h>
#include "Lucy/Util/ToolSet.h"

#include "Lucy/Index/FilePurger.h"
#include "Clownfish/Boolean.h"
#include "Lucy/Index/IndexManager.h"
#include "Lucy/Index/Segment.h"
#include "Lucy/Index/Snapshot.h"
#include "Lucy/Plan/Schema.h"
#include "Lucy/Store/DirHandle.h"
#include "Lucy/Store/Folder.h"
#include "Lucy/Store/Lock.h"
#include "Lucy/Util/Json.h"

// Place unused files into purgables array and obsolete Snapshots into
// snapshots array.
static void
S_discover_unused(FilePurger *self, Vector **purgables, Vector **snapshots);

// Clean up after a failed background merge session, adding all dead files to
// the list of candidates to be zapped.
static void
S_zap_dead_merge(FilePurger *self, Hash *candidates);

// Return an array of recursively expanded filepath entries.
static Vector*
S_find_all_referenced(Folder *folder, Vector *entries);

FilePurger*
FilePurger_new(Folder *folder, Snapshot *snapshot, IndexManager *manager) {
    FilePurger *self = (FilePurger*)Class_Make_Obj(FILEPURGER);
    return FilePurger_init(self, folder, snapshot, manager);
}

FilePurger*
FilePurger_init(FilePurger *self, Folder *folder, Snapshot *snapshot,
                IndexManager *manager) {
    FilePurgerIVARS *const ivars = FilePurger_IVARS(self);
    ivars->folder       = (Folder*)INCREF(folder);
    ivars->snapshot     = (Snapshot*)INCREF(snapshot);
    ivars->manager      = manager
                         ? (IndexManager*)INCREF(manager)
                         : IxManager_new(NULL, NULL);
    IxManager_Set_Folder(ivars->manager, folder);

    // Don't allow the locks directory to be zapped.
    ivars->disallowed = Hash_new(0);
    Hash_Store_Utf8(ivars->disallowed, "locks", 5, (Obj*)CFISH_TRUE);

    return self;
}

void
FilePurger_Destroy_IMP(FilePurger *self) {
    FilePurgerIVARS *const ivars = FilePurger_IVARS(self);
    DECREF(ivars->folder);
    DECREF(ivars->snapshot);
    DECREF(ivars->manager);
    DECREF(ivars->disallowed);
    SUPER_DESTROY(self, FILEPURGER);
}

void
FilePurger_Purge_IMP(FilePurger *self) {
    FilePurgerIVARS *const ivars = FilePurger_IVARS(self);
    Lock *deletion_lock = IxManager_Make_Deletion_Lock(ivars->manager);

    // Obtain deletion lock, purge files, release deletion lock.
    Lock_Clear_Stale(deletion_lock);
    if (Lock_Obtain(deletion_lock)) {
        Folder *folder   = ivars->folder;
        Hash   *failures = Hash_new(0);
        Vector *purgables;
        Vector *snapshots;

        S_discover_unused(self, &purgables, &snapshots);

        // Attempt to delete entries -- if failure, no big deal, just try
        // again later.  Proceed in reverse lexical order so that directories
        // get deleted after they've been emptied.
        Vec_Sort(purgables);
        for (uint32_t i = Vec_Get_Size(purgables); i--;) {
            String *entry = (String*)Vec_Fetch(purgables, i);
            if (Hash_Fetch(ivars->disallowed, entry)) { continue; }
            if (!Folder_Delete(folder, entry)) {
                if (Folder_Exists(folder, entry)) {
                    Hash_Store(failures, entry, (Obj*)CFISH_TRUE);
                }
            }
        }

        for (uint32_t i = 0, max = Vec_Get_Size(snapshots); i < max; i++) {
            Snapshot *snapshot = (Snapshot*)Vec_Fetch(snapshots, i);
            bool snapshot_has_failures = false;
            if (Hash_Get_Size(failures)) {
                // Only delete snapshot files if all of their entries were
                // successfully deleted.
                Vector *entries = Snapshot_List(snapshot);
                for (uint32_t j = Vec_Get_Size(entries); j--;) {
                    String *entry = (String*)Vec_Fetch(entries, j);
                    if (Hash_Fetch(failures, entry)) {
                        snapshot_has_failures = true;
                        break;
                    }
                }
                DECREF(entries);
            }
            if (!snapshot_has_failures) {
                String *snapfile = Snapshot_Get_Path(snapshot);
                Folder_Delete(folder, snapfile);
            }
        }

        DECREF(failures);
        DECREF(purgables);
        DECREF(snapshots);
        Lock_Release(deletion_lock);
    }
    else {
        WARN("Can't obtain deletion lock, skipping deletion of "
             "obsolete files");
    }

    DECREF(deletion_lock);
}

static void
S_zap_dead_merge(FilePurger *self, Hash *candidates) {
    FilePurgerIVARS *const ivars = FilePurger_IVARS(self);
    IndexManager *manager    = ivars->manager;
    Lock         *merge_lock = IxManager_Make_Merge_Lock(manager);

    Lock_Clear_Stale(merge_lock);
    if (!Lock_Is_Locked(merge_lock)) {
        Hash *merge_data = IxManager_Read_Merge_Data(manager);
        Obj  *cutoff = merge_data
                       ? Hash_Fetch_Utf8(merge_data, "cutoff", 6)
                       : NULL;

        if (cutoff) {
            String *cutoff_seg = Seg_num_to_name(Json_obj_to_i64(cutoff));
            if (Folder_Exists(ivars->folder, cutoff_seg)) {
                String *merge_json = SSTR_WRAP_C("merge.json");
                DirHandle *dh = Folder_Open_Dir(ivars->folder, cutoff_seg);

                if (!dh) {
                    THROW(ERR, "Can't open segment dir '%o'", cutoff_seg);
                }

                Hash_Store(candidates, cutoff_seg, (Obj*)CFISH_TRUE);
                Hash_Store(candidates, merge_json, (Obj*)CFISH_TRUE);
                while (DH_Next(dh)) {
                    // TODO: recursively delete subdirs within seg dir.
                    String *entry = DH_Get_Entry(dh);
                    String *filepath = Str_newf("%o/%o", cutoff_seg, entry);
                    Hash_Store(candidates, filepath, (Obj*)CFISH_TRUE);
                    DECREF(filepath);
                    DECREF(entry);
                }
                DECREF(dh);
            }
            DECREF(cutoff_seg);
        }

        DECREF(merge_data);
    }

    DECREF(merge_lock);
    return;
}

static void
S_discover_unused(FilePurger *self, Vector **purgables_ptr,
                  Vector **snapshots_ptr) {
    FilePurgerIVARS *const ivars = FilePurger_IVARS(self);
    Folder      *folder       = ivars->folder;
    DirHandle   *dh           = Folder_Open_Dir(folder, NULL);
    if (!dh) { RETHROW(INCREF(Err_get_error())); }
    Vector      *spared       = Vec_new(1);
    Vector      *snapshots    = Vec_new(1);
    String      *snapfile     = NULL;

    // Start off with the list of files in the current snapshot.
    if (ivars->snapshot) {
        Vector *entries    = Snapshot_List(ivars->snapshot);
        Vector *referenced = S_find_all_referenced(folder, entries);
        Vec_Push_All(spared, referenced);
        DECREF(entries);
        DECREF(referenced);
        snapfile = Snapshot_Get_Path(ivars->snapshot);
        if (snapfile) { Vec_Push(spared, INCREF(snapfile)); }
    }

    Hash *candidates = Hash_new(64);
    while (DH_Next(dh)) {
        String *entry = DH_Get_Entry(dh);
        if (Str_Starts_With_Utf8(entry, "snapshot_", 9)
            && Str_Ends_With_Utf8(entry, ".json", 5)
            && (!snapfile || !Str_Equals(entry, (Obj*)snapfile))
        ) {
            Snapshot *snapshot
                = Snapshot_Read_File(Snapshot_new(), folder, entry);
            Lock *lock
                = IxManager_Make_Snapshot_Read_Lock(ivars->manager, entry);
            Vector *snap_list  = Snapshot_List(snapshot);
            Vector *referenced = S_find_all_referenced(folder, snap_list);

            // DON'T obtain the lock -- only see whether another
            // entity holds a lock on the snapshot file.
            if (lock) {
                Lock_Clear_Stale(lock);
            }
            if (lock && Lock_Is_Locked(lock)) {
                // The snapshot file is locked, which means someone's using
                // that version of the index -- protect all of its entries.
                uint32_t new_size = Vec_Get_Size(spared)
                                    + Vec_Get_Size(referenced)
                                    + 1;
                Vec_Grow(spared, new_size);
                Vec_Push(spared, (Obj*)Str_Clone(entry));
                Vec_Push_All(spared, referenced);
            }
            else {
                // No one's using this snapshot, so all of its entries are
                // candidates for deletion.
                for (uint32_t i = 0, max = Vec_Get_Size(referenced); i < max; i++) {
                    String *file = (String*)Vec_Fetch(referenced, i);
                    Hash_Store(candidates, file, (Obj*)CFISH_TRUE);
                }
                Vec_Push(snapshots, INCREF(snapshot));
            }

            DECREF(referenced);
            DECREF(snap_list);
            DECREF(snapshot);
            DECREF(lock);
        }
        DECREF(entry);
    }
    DECREF(dh);

    // Clean up after a dead segment consolidation.
    S_zap_dead_merge(self, candidates);

    // Eliminate any current files from the list of files to be purged.
    for (uint32_t i = 0, max = Vec_Get_Size(spared); i < max; i++) {
        String *filename = (String*)Vec_Fetch(spared, i);
        DECREF(Hash_Delete(candidates, filename));
    }

    // Pass back purgables and Snapshots.
    *purgables_ptr = Hash_Keys(candidates);
    *snapshots_ptr = snapshots;

    DECREF(candidates);
    DECREF(spared);
}

static Vector*
S_find_all_referenced(Folder *folder, Vector *entries) {
    Hash *uniqued = Hash_new(Vec_Get_Size(entries));
    for (uint32_t i = 0, max = Vec_Get_Size(entries); i < max; i++) {
        String *entry = (String*)Vec_Fetch(entries, i);
        Hash_Store(uniqued, entry, (Obj*)CFISH_TRUE);
        if (Folder_Is_Directory(folder, entry)) {
            Vector *contents = Folder_List_R(folder, entry);
            for (uint32_t j = Vec_Get_Size(contents); j--;) {
                String *sub_entry = (String*)Vec_Fetch(contents, j);
                Hash_Store(uniqued, sub_entry, (Obj*)CFISH_TRUE);
            }
            DECREF(contents);
        }
    }
    Vector *referenced = Hash_Keys(uniqued);
    DECREF(uniqued);
    return referenced;
}


