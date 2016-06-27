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

use Test::More tests => 87;

BEGIN { use_ok('Clownfish::CFC::Parser') }

my $parser = Clownfish::CFC::Parser->new;
isa_ok( $parser, "Clownfish::CFC::Parser" );

isa_ok(
    $parser->parse("parcel Fish;"),
    "Clownfish::CFC::Model::Parcel",
    "parcel_definition"
);

# Set and leave parcel.
my $registered = Clownfish::CFC::Model::Parcel->new(
    name     => 'Crustacean',
    nickname => 'Crust',
);
$registered->register;
my $parcel = $parser->parse('parcel Crustacean;')
    or die "failed to process parcel_definition";
is( $$parcel, $$registered, "Fetch registered parcel" );
is( ${ $parser->get_parcel },
    $$parcel, "parcel_definition sets internal \$parcel var" );

for (qw( foo _foo foo_yoo FOO Foo fOO f00 foo_foo_foo )) {
    my $var = $parser->parse("int32_t $_;");
    is( $var->get_name, $_, "identifier/declarator: $_" );
}

for (qw( void float uint32_t int64_t uint8_t bool )) {
    my $var = $parser->parse("int32_t $_;");
    ok( !defined($var), "reserved word not parsed as identifier: $_" );
}

isa_ok( $parser->parse("bool"),
    "Clownfish::CFC::Model::Type", "C99 integer specifier bool" );

for (qw( ByteBuf Obj ANDMatcher )) {
    my $class = $parser->parse("class $_ {}");
    my $type  = $parser->parse("$_*");
    $type->resolve;
    is( $type->get_specifier, "crust_$_", "object_type_specifier $_" );
}

ok( $parser->parse("const char")->const, "type_qualifier const" );

ok( $parser->parse("$_ inert int32_t foo;")->$_, "exposure_specifier $_" )
    for qw( public );

ok( $parser->parse("int32_t foo;")->private,
    "exposure_specifier '' implicitly private" );

isa_ok( $parser->parse($_), "Clownfish::CFC::Model::Type", "type $_" )
    for ( 'const char *', 'Obj*', 'i32_t', 'char[]', 'long[1]', 'i64_t[30]' );

is( $parser->parse("(int32_t foo = $_)")->get_initial_values->[0],
    $_, "hex_constant: $_" )
    for (qw( 0x1 0x0a 0xFFFFFFFF -0xFC ));

is( $parser->parse("(int32_t foo = $_)")->get_initial_values->[0],
    $_, "integer_constant: $_" )
    for (qw( 1 -9999  0 10000 ));

is( $parser->parse("(double foo = $_)")->get_initial_values->[0],
    $_, "float_constant: $_" )
    for (qw( 1.0 -9999.999  0.1 0.0 ));

is( $parser->parse("(String *foo = $_)")->get_initial_values->[0],
    $_, "string_literal: $_" )
    for ( q|"blah"|, q|"blah blah"|, q|"\\"blah\\" \\"blah\\""| );

my @composites = ( 'int[]', "i32_t **", "Foo **", "Foo ***", "const void *" );
for my $composite (@composites) {
    my $parsed = $parser->parse($composite);
    ok( $parsed && $parsed->is_composite, "composite_type: $composite" );
}

my @object_types = ( 'Obj *', "incremented Foo*", "decremented String *" );
for my $object_type (@object_types) {
    my $parsed = $parser->parse($object_type);
    ok( $parsed && $parsed->is_object, "object_type: $object_type" );
}

my %param_lists = (
    '(int foo)'                 => 1,
    '(Obj *foo, Foo **foo_ptr)' => 2,
    '()'                        => 0,
);
while ( my ( $param_list, $num_params ) = each %param_lists ) {
    my $parsed = $parser->parse($param_list);
    isa_ok(
        $parsed,
        "Clownfish::CFC::Model::ParamList",
        "param_list: $param_list"
    );
}
ok( $parser->parse("(int foo, ...)")->variadic, "variadic param list" );
my $param_list = $parser->parse(q|(int foo = 0xFF, char *bar ="blah")|);
is_deeply(
    $param_list->get_initial_values,
    [ '0xFF', '"blah"' ],
    "initial values"
);

$parser->set_class_name('Stuff::Obj');
ok( $parser->parse($_), "declaration statement: $_" )
    for (
    'public Foo* Spew_Foo(Obj *self, uint32_t *how_many);',
    'public inert Hash *hash;',
    );

for (qw( Foo FooJr FooIII Foo4th )) {
    my $class = $parser->parse("class $_ {}");
    my $type  = $parser->parse("$_*");
    $type->resolve;
    is( $type->get_specifier, "crust_$_", "object_type_specifier: $_" )
}
Clownfish::CFC::Model::Class->_clear_registry();

SKIP: {
    skip( "Can't recover from bad specifier under flex/lemon parser", 6 );
    ok( !$parser->parse("$_*"), "illegal object_type_specifier: $_" )
        for (qw( foo fooBar Foo_Bar FOOBAR 1Foo 1FOO ));
}

is( $parser->parse("class $_ { }")->get_name, $_, "class_name: $_" )
    for (qw( Foo Foo::FooJr Foo::FooJr::FooIII Foo::FooJr::FooIII::Foo4th ));

SKIP: {
    skip( "Can't recover from bad class name under flex/lemon parser", 6 );
    ok( !$parser->parse("class $_ { }"), "illegal class_name: $_" )
        for (qw( foo fooBar Foo_Bar FOOBAR 1Foo 1FOO ));
}

is( $parser->parse(qq|class Foodie$_ nickname $_ { }|)->get_nickname,
    $_, "nickname: $_" )
    for (qw( Food FF ));

SKIP: {
    skip( "Can't recover from bad nickname under flex/lemon parser", 3 );
    is( !$parser->parse(qq|class Foodie$_ nickname $_ { }|),
        "Illegal nickname: $_" )
        for (qw( foo fOO 1Foo ));
}

