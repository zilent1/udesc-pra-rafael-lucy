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

use Test::More tests => 14;

BEGIN { use_ok('Clownfish::CFC::Model::ParamList') }

ok( Clownfish::CFC::Model::ParamList->new, 'new' );

my $parser = Clownfish::CFC::Parser->new;
$parser->parse('parcel Neato;')
    or die "failed to process parcel_definition";

isa_ok(
    $parser->parse($_),
    "Clownfish::CFC::Model::Variable",
    "param_variable: $_"
) for ( 'uint32_t baz', 'String *stuff', 'float **ptr', );

my $obj_class = $parser->parse("class Obj {}");

my $param_list = $parser->parse("(Obj *self, int num)");
$param_list->resolve_types;
isa_ok( $param_list, "Clownfish::CFC::Model::ParamList" );
ok( !$param_list->variadic, "not variadic" );
is( $param_list->to_c, 'neato_Obj* self, int num', "to_c" );
is( $param_list->name_list, 'self, num', "name_list" );

$param_list = $parser->parse("(Obj *self=NULL, int num, ...)");
$param_list->resolve_types;
ok( $param_list->variadic, "variadic" );
is_deeply(
    $param_list->get_initial_values,
    [ "NULL", undef ],
    "initial_values"
);
is( $param_list->to_c, 'neato_Obj* self, int num, ...', "to_c" );
is( $param_list->num_vars, 2, "num_vars" );
isa_ok(
    $param_list->get_variables->[0],
    "Clownfish::CFC::Model::Variable",
    "get_variables..."
);

