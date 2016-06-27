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

#include "charmony.h"

#define CFC_USE_TEST_MACROS
#include "CFCBase.h"
#include "CFCFileSpec.h"
#include "CFCParcel.h"
#include "CFCSymbol.h"
#include "CFCUtil.h"
#include "CFCVersion.h"
#include "CFCTest.h"

#ifndef true
  #define true 1
  #define false 0
#endif

static void
S_run_tests(CFCTest *test);

static void
S_run_prereq_tests(CFCTest *test);

static void
S_run_parcel_tests(CFCTest *test);

const CFCTestBatch CFCTEST_BATCH_PARCEL = {
    "Clownfish::CFC::Model::Parcel",
    29,
    S_run_tests
};

static void
S_run_tests(CFCTest *test) {
    S_run_prereq_tests(test);
    S_run_parcel_tests(test);
}

static void
S_run_prereq_tests(CFCTest *test) {
    {
        CFCVersion *v77_66_55 = CFCVersion_new("v77.66.55");
        CFCPrereq *prereq = CFCPrereq_new("Flour", v77_66_55);
        const char *name = CFCPrereq_get_name(prereq);
        STR_EQ(test, name, "Flour", "prereq get_name");
        CFCVersion *version = CFCPrereq_get_version(prereq);
        INT_EQ(test, CFCVersion_compare_to(version, v77_66_55), 0,
               "prereq get_version");
        CFCBase_decref((CFCBase*)prereq);
        CFCBase_decref((CFCBase*)v77_66_55);
    }

    {
        CFCVersion *v0 = CFCVersion_new("v0");
        CFCPrereq *prereq = CFCPrereq_new("Sugar", NULL);
        CFCVersion *version = CFCPrereq_get_version(prereq);
        INT_EQ(test, CFCVersion_compare_to(version, v0), 0,
               "prereq with default version");
        CFCBase_decref((CFCBase*)prereq);
        CFCBase_decref((CFCBase*)v0);
    }
}

static void
S_run_parcel_tests(CFCTest *test) {
    {
        CFCParcel *parcel = CFCParcel_new("Foo", NULL, NULL, NULL);
        OK(test, parcel != NULL, "new");
        OK(test, !CFCParcel_included(parcel), "not included");
        CFCBase_decref((CFCBase*)parcel);
    }

    {
        CFCFileSpec *file_spec = CFCFileSpec_new(".", "Parcel", true);
        CFCParcel *parcel = CFCParcel_new("Foo", NULL, NULL, file_spec);
        OK(test, CFCParcel_included(parcel), "included");
        CFCBase_decref((CFCBase*)parcel);
        CFCBase_decref((CFCBase*)file_spec);
    }

    {
        const char *json =
            "        {\n"
            "            \"name\": \"Crustacean\",\n"
            "            \"nickname\": \"Crust\",\n"
            "            \"version\": \"v0.1.0\"\n"
            "        }\n";
        CFCParcel *parcel = CFCParcel_new_from_json(json, NULL);
        OK(test, parcel != NULL, "new_from_json");
        CFCBase_decref((CFCBase*)parcel);
    }

    {
        const char *path = "t" CHY_DIR_SEP "cfbase" CHY_DIR_SEP "Animal.cfp";
        CFCParcel *parcel = CFCParcel_new_from_file(path, NULL);
        OK(test, parcel != NULL, "new_from_file");
        CFCBase_decref((CFCBase*)parcel);
    }

    {
        CFCParcel *parcel = CFCParcel_new("Crustacean", "Crust", NULL, NULL);
        CFCParcel_register(parcel);
        STR_EQ(test, CFCVersion_get_vstring(CFCParcel_get_version(parcel)),
               "v0", "get_version");

        CFCBase_decref((CFCBase*)parcel);
        CFCParcel_reap_singletons();
    }

    {
        const char *json =
            "        {\n"
            "            \"name\": \"Crustacean\",\n"
            "            \"version\": \"v0.1.0\",\n"
            "            \"prerequisites\": {\n"
            "                \"Clownfish\": null,\n"
            "                \"Arthropod\": \"v30.104.5\"\n"
            "            }\n"
            "        }\n";
        CFCParcel *parcel = CFCParcel_new_from_json(json, NULL);

        CFCPrereq **prereqs = CFCParcel_get_prereqs(parcel);
        OK(test, prereqs != NULL, "prereqs");

        CFCPrereq *cfish = prereqs[0];
        OK(test, cfish != NULL, "prereqs[0]");
        const char *cfish_name = CFCPrereq_get_name(cfish);
        STR_EQ(test, cfish_name, "Clownfish", "prereqs[0] name");
        CFCVersion *v0            = CFCVersion_new("v0");
        CFCVersion *cfish_version = CFCPrereq_get_version(cfish);
        INT_EQ(test, CFCVersion_compare_to(cfish_version, v0), 0,
               "prereqs[0] version");

        CFCPrereq *apod = prereqs[1];
        OK(test, apod != NULL, "prereqs[1]");
        const char *apod_name = CFCPrereq_get_name(apod);
        STR_EQ(test, apod_name, "Arthropod", "prereqs[1] name");
        CFCVersion *v30_104_5    = CFCVersion_new("v30.104.5");
        CFCVersion *apod_version = CFCPrereq_get_version(apod);
        INT_EQ(test, CFCVersion_compare_to(apod_version, v30_104_5), 0,
               "prereqs[1] version");

        OK(test, prereqs[2] == NULL, "prereqs[2]");

        CFCBase_decref((CFCBase*)v30_104_5);
        CFCBase_decref((CFCBase*)v0);
        CFCBase_decref((CFCBase*)parcel);
    }

    {
        CFCFileSpec *foo_file_spec = CFCFileSpec_new(".", "Foo", true);
        CFCParcel *foo = CFCParcel_new("Foo", NULL, NULL, foo_file_spec);
        CFCParcel_register(foo);

        CFCVersion *cfish_version = CFCVersion_new("v0.8.7");
        CFCFileSpec *cfish_file_spec
            = CFCFileSpec_new(".", "Clownfish", true);
        CFCParcel *cfish
            = CFCParcel_new("Clownfish", NULL, cfish_version, cfish_file_spec);
        CFCParcel_register(cfish);

        const char *crust_json =
            "        {\n"
            "            \"name\": \"Crustacean\",\n"
            "            \"version\": \"v0.1.0\",\n"
            "            \"prerequisites\": {\n"
            "                \"Clownfish\": \"v0.8.5\",\n"
            "            }\n"
            "        }\n";
        CFCParcel *crust = CFCParcel_new_from_json(crust_json, NULL);
        CFCParcel_register(crust);

        CFCParcel_check_prereqs(crust);
        INT_EQ(test, CFCParcel_required(foo), false, "parcel not required");
        INT_EQ(test, CFCParcel_required(cfish), true, "prereq required");
        INT_EQ(test, CFCParcel_required(crust), true, "self required");

        CFCParcel **prereq_parcels = CFCParcel_prereq_parcels(crust);
        OK(test, prereq_parcels[0] != NULL, "prereq_parcels[0]");
        const char *name = CFCParcel_get_name(prereq_parcels[0]);
        STR_EQ(test, name, "Clownfish", "prereq_parcels[0] name");
        OK(test, prereq_parcels[1] == NULL, "prereq_parcels[0]");

        OK(test, CFCParcel_has_prereq(crust, cfish), "has_prereq");
        OK(test, CFCParcel_has_prereq(crust, crust), "has_prereq self");
        OK(test, !CFCParcel_has_prereq(crust, foo), "has_prereq false");

        CFCParcel_add_struct_sym(cfish, "Swim");
        CFCParcel_add_struct_sym(crust, "Pinch");
        CFCParcel_add_struct_sym(foo, "Bar");
        CFCParcel *found;
        found = CFCParcel_lookup_struct_sym(crust, "Swim");
        OK(test, found == cfish, "lookup_struct_sym prereq");
        found = CFCParcel_lookup_struct_sym(crust, "Pinch");
        OK(test, found == crust, "lookup_struct_sym self");
        found = CFCParcel_lookup_struct_sym(crust, "Bar");
        OK(test, found == NULL, "lookup_struct_sym other");

        FREEMEM(prereq_parcels);
        CFCBase_decref((CFCBase*)crust);
        CFCBase_decref((CFCBase*)cfish_version);
        CFCBase_decref((CFCBase*)cfish_file_spec);
        CFCBase_decref((CFCBase*)cfish);
        CFCBase_decref((CFCBase*)foo_file_spec);
        CFCBase_decref((CFCBase*)foo);
        CFCParcel_reap_singletons();
    }
}

