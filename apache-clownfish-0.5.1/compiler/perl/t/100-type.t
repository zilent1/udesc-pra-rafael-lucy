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

use Test::More tests => 10;
use Clownfish::CFC::Model::Parcel;

my $neato_parcel = Clownfish::CFC::Model::Parcel->new( name => 'Neato' );
$neato_parcel->register;

my $type = Clownfish::CFC::Model::Type->new(
    parcel    => 'Neato',
    specifier => 'mytype_t'
);
is( ${ $type->get_parcel },
    $$neato_parcel, "constructor changes parcel name to Parcel singleton" );

is( $type->to_c, "mytype_t", "to_c()" );
ok( !$type->const, "const() is off by default" );
is( $type->get_specifier, "mytype_t", "get_specifier()" );

ok( !$type->is_object,    "is_object() false by default" );
ok( !$type->is_integer,   "is_integer() false by default" );
ok( !$type->is_floating,  "is_floating() false by default" );
ok( !$type->is_void,      "is_void() false by default" );
ok( !$type->is_composite, "is_composite() false by default" );
ok( !$type->cfish_string, "cfish_string() false by default" );

