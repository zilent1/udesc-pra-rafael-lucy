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

use Test::More tests => 54;
use Clownfish::CFC::Model::Class;
use Clownfish::CFC::Parser;

my $parser = Clownfish::CFC::Parser->new;
$parser->parse('parcel Neato;');

my $thing = Clownfish::CFC::Model::Variable->new(
    type => $parser->parse('Thing*'),
    name => 'thing',
);
my $widget = Clownfish::CFC::Model::Variable->new(
    type => $parser->parse('Widget*'),
    name => 'widget',
);
my $tread_water = Clownfish::CFC::Model::Function->new(
    return_type => $parser->parse('void'),
    name        => 'tread_water',
    param_list  => $parser->parse('()'),
);
my %foo_create_args = (
    parcel     => 'Neato',
    class_name => 'Foo',
);

my $foo = Clownfish::CFC::Model::Class->create(%foo_create_args);
$foo->add_function($tread_water);
$foo->add_member_var($thing);
$foo->add_inert_var($widget);
my $should_be_foo = Clownfish::CFC::Model::Class->fetch_singleton(
    parcel     => 'Neato',
    class_name => 'Foo',
);
is( $$foo, $$should_be_foo, "fetch_singleton" );

SKIP: {
    skip( 'Exceptions leak', 3 )
        if $ENV{LUCY_VALGRIND};

    eval { Clownfish::CFC::Model::Class->create(%foo_create_args) };
    like( $@, qr/two classes with name/i,
          "Can't call create for the same class more than once" );

    eval {
        Clownfish::CFC::Model::Class->create(
            parcel     => 'Neato',
            class_name => 'Other::Foo',
        );
    };
    like( $@, qr/class name conflict/i,
          "Can't create classes wth the same final component" );
    eval {
        Clownfish::CFC::Model::Class->create(
            parcel     => 'Neato',
            class_name => 'Bar',
            nickname   => 'Foo',
        );
    };
    like( $@, qr/class nickname conflict/i,
          "Can't create classes wth the same nickname" );
}

my $foo_jr = Clownfish::CFC::Model::Class->create(
    parcel            => 'Neato',
    class_name        => 'Foo::FooJr',
    parent_class_name => 'Foo',
);

is( $foo_jr->get_struct_sym,  'FooJr',       "struct_sym" );
is( $foo_jr->full_struct_sym, 'neato_FooJr', "full_struct_sym" );
is( $foo_jr->get_nickname, "FooJr",
    "derive class_nickname from class_name" );

my $file_spec = Clownfish::CFC::Model::FileSpec->new(
    source_dir  => '.',
    path_part   => 'Foo/FooJr',
);
my $final_foo = Clownfish::CFC::Model::Class->create(
    parcel            => 'Neato',
    class_name        => 'Foo::FooJr::FinalFoo',
    parent_class_name => 'Foo::FooJr',
    file_spec         => $file_spec,
    final             => 1,
);
ok( $final_foo->final, "final" );
is( $final_foo->include_h, 'Foo/FooJr.h', "inlude_h uses path_part" );
is( $final_foo->get_parent_class_name, 'Foo::FooJr',
    "get_parent_class_name" );

$parser->parse("parcel Neato;");
$parser->set_class_name("Foo");
my $do_stuff = $parser->parse('void Do_Stuff(Foo *self);')
    or die "parsing failure";
$foo->add_method($do_stuff);

$parser->set_class_name("InertFoo");
my $inert_do_stuff = $parser->parse('void Do_Stuff(InertFoo *self);')
    or die "parsing failure";
$parser->set_class_name("");

my %inert_args = (
    parcel     => 'Neato',
    class_name => 'InertFoo',
    inert      => 1,
);
eval {
    my $class = Clownfish::CFC::Model::Class->create(%inert_args);
    $class->add_method($inert_do_stuff);
};
like(
    $@,
    qr/inert class/i,
    "Error out on conflict between inert attribute and object method"
);

$foo->add_child($foo_jr);
$foo_jr->add_child($final_foo);
$foo->grow_tree;
eval { $foo->grow_tree };
like( $@, qr/grow_tree/, "call grow_tree only once." );
eval { $foo_jr->add_method($do_stuff) };
like( $@, qr/grow_tree/, "Forbid add_method after grow_tree." );

is( ${ $foo_jr->get_parent },    $$foo,    "grow_tree, one level" );
is( ${ $final_foo->get_parent }, $$foo_jr, "grow_tree, two levels" );
is( ${ $foo->fresh_method("Do_Stuff") }, $$do_stuff, 'fresh_method' );
is( ${ $foo_jr->method("Do_Stuff") },    $$do_stuff, "inherited method" );
ok( !$foo_jr->fresh_method("Do_Stuff"),    'inherited method not "fresh"' );
ok( $final_foo->method("Do_Stuff")->final, "Finalize inherited method" );
ok( !$foo_jr->method("Do_Stuff")->final, "Don't finalize method in parent" );
is_deeply( $foo->inert_vars,        [$widget],      "inert vars" );
is_deeply( $foo->functions,         [$tread_water], "inert funcs" );
is_deeply( $foo->methods,           [$do_stuff],    "methods" );
is_deeply( $foo->fresh_methods,     [$do_stuff],    "fresh_methods" );
is_deeply( $foo->fresh_member_vars, [$thing],       "fresh_member_vars" );
is_deeply( $foo_jr->member_vars,    [$thing],       "inherit member vars" );
is_deeply( $foo_jr->functions,         [], "don't inherit inert funcs" );
is_deeply( $foo_jr->fresh_member_vars, [], "fresh_member_vars" );
is_deeply( $foo_jr->inert_vars,        [], "don't inherit inert vars" );
is_deeply( $final_foo->fresh_methods,  [], "fresh_methods" );

is_deeply( $foo->tree_to_ladder, [ $foo, $foo_jr, $final_foo ],
    'tree_to_ladder' );

ok( $parser->parse("$_ class Iam$_ { }")->$_, "class_modifier: $_" )
    for (qw( final inert ));

is( $parser->parse("class Fu::$_ inherits $_ { }")->get_parent_class_name,
    $_, "class_inheritance: $_" )
    for ( 'Fooble', 'Foo::FooJr::FooIII' );

is( $parser->parse("class MissingInherits { }")->get_parent_class_name,
    "Clownfish::Obj", "class inherits from Clownfish::Obj by default" );

my $class_content
    = 'public class Foo::Foodie nickname Foodie inherits Foo { int num; }';
my $class = $parser->parse($class_content);
isa_ok( $class, "Clownfish::CFC::Model::Class", "class_declaration FooJr" );
ok( ( scalar grep { $_->get_name eq 'num' } @{ $class->fresh_member_vars } ),
    "parsed member var" );

$class_content = q|
    /**
     * Bow wow.
     *
     * Wow wow wow.
     */
    public class Animal::Dog inherits Animal {
        public inert Dog* init(Dog *self, String *name, String *fave_food);
        inert uint32_t count();
        inert uint64_t num_dogs;
        public inert Dog* top_dog;

        String  *name;
        bool     likes_to_go_fetch;
        ChewToy *squishy;
        Owner   *mom;

        void               Destroy(Dog *self);
        public String*     Bark(Dog *self);
        public void        Eat(Dog *self);
        public void        Bite(Dog *self, Enemy *enemy);
        public Thing      *Fetch(Dog *self, Thing *thing);
        public final void  Bury(Dog *self, Bone *bone);
        public abstract incremented nullable Thing*
        Scratch(Dog *self);

        int32_t[1]  flexible_array_at_end_of_struct;
    }
|;

$class = $parser->parse($class_content);
isa_ok( $class, "Clownfish::CFC::Model::Class", "class_declaration Dog" );
ok( ( scalar grep { $_->get_name eq 'num_dogs' } @{ $class->inert_vars } ),
    "parsed inert var" );
ok( ( scalar grep { $_->get_name eq 'top_dog' } @{ $class->inert_vars } ),
    "parsed public inert var" );
ok( ( scalar grep { $_->get_name eq 'mom' } @{ $class->fresh_member_vars } ),
    "parsed member var" );
ok( ( scalar grep { $_->get_name eq 'squishy' } @{ $class->fresh_member_vars } ),
    "parsed member var" );
ok( ( scalar grep { $_->get_name eq 'init' } @{ $class->functions } ),
    "parsed function" );
ok( ( scalar grep { $_->get_name eq 'Destroy' } @{ $class->fresh_methods } ),
    "parsed parcel method" );
ok( ( scalar grep { $_->get_name eq 'Bury' } @{ $class->fresh_methods } ),
    "parsed public method" );
ok( ( scalar grep { $_->get_name eq 'Scratch' } @{ $class->fresh_methods } ),
    "parsed public abstract nullable method" );

for my $method ( @{ $class->fresh_methods } ) {
    if ( $method->get_name eq 'Scratch' ) {
        ok( $method->get_return_type->nullable,
            "public abstract incremented nullable flagged as nullable" );
    }
}
is( ( scalar grep { $_->public } @{ $class->fresh_methods } ),
    6, "pass acl to Method constructor" );

$class_content = qq|
    inert class Rigor::Mortis nickname Mort {
        inert void lie_still();
    }|;
$class = $parser->parse($class_content);
isa_ok( $class, "Clownfish::CFC::Model::Class", "inert class_declaration" );
ok( $class->inert, "inert modifier parsed and passed to constructor" );

$class_content = qq|
    final class Ultimo {
        /** Throws an error.
         */
        void Say_Never(Ultimo *self);
    }|;
$class = $parser->parse($class_content);
ok( $class->final, "final class_declaration" );

Clownfish::CFC::Model::Class->_clear_registry();

{
    eval {
        my $class = Clownfish::CFC::Model::Class->create(%foo_create_args);
        my $inert = Clownfish::CFC::Model::Class->create(%inert_args);
        $class->add_child($inert);
    };
    like( $@, qr/inert class/i, "inert class can't inherit" );

    Clownfish::CFC::Model::Class->_clear_registry();
}

{
    eval {
        my $class = Clownfish::CFC::Model::Class->create(%foo_create_args);
        my $inert = Clownfish::CFC::Model::Class->create(%inert_args);
        $inert->add_child($class);
    };
    like( $@, qr/inert class/i, "can't inherit from inert class" );

    Clownfish::CFC::Model::Class->_clear_registry();
}

