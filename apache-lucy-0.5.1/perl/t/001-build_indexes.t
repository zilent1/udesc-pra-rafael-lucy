# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

use strict;
use warnings;
use lib 'buildlib';

use Test::More tests => 4;
use File::Spec::Functions qw( catfile );
use File::Find qw( find );
use Lucy::Test::TestUtils qw(
    working_dir
    create_working_dir
    remove_working_dir
    create_uscon_index
    persistent_test_index_loc
);

remove_working_dir();
ok( !-e working_dir(), "Working dir doesn't exist" );
create_working_dir();
ok( -e working_dir(), "Working dir successfully created" );

create_uscon_index();

my $path = persistent_test_index_loc();
ok( -d $path, "created index directory" );
my $num_cfmeta = 0;
find(
    {   no_chdir => 1,
        wanted   => sub { $num_cfmeta++ if $File::Find::name =~ /cfmeta/ },
    },
    $path
);
cmp_ok( $num_cfmeta, '>', 0, "at least one .cf file exists" );

