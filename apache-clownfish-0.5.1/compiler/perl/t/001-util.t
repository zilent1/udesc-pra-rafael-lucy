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

use Test::More tests => 15;
use File::stat qw( stat );
use Clownfish::CFC::Util qw(
    slurp_text
    current
    verify_args
    a_isa_b
    write_if_changed
);

my $foo_txt = 'foo.txt';
unlink $foo_txt;
open( my $fh, '>', $foo_txt ) or die "Can't open '$foo_txt': $!";
print $fh "foo";
close $fh or die "Can't close '$foo_txt': $!";
is( slurp_text($foo_txt), "foo", "slurp_text" );

ok( current( $foo_txt, $foo_txt ), "current" );
ok( !current( $foo_txt, 't' ), "not current" );
ok( !current( $foo_txt, "nonexistent_file" ),
    "not current when dest file mising"
);

my $ten_seconds_ago = time() - 10;
utime( $ten_seconds_ago, $ten_seconds_ago, $foo_txt )
    or die "utime failed";
write_if_changed( $foo_txt, "foo" );
is( stat($foo_txt)->mtime, $ten_seconds_ago,
    "write_if_changed does nothing if contents not changed" );
write_if_changed( $foo_txt, "foofoo" );
ok( stat($foo_txt)->mtime != $ten_seconds_ago,
    "write_if_changed writes if contents changed"
);

unlink $foo_txt;

my %defaults = ( foo => undef );
sub test_verify_args { return verify_args( \%defaults, @_ ) }

ok( test_verify_args( foo => 'foofoo' ), "args verified" );
ok( !test_verify_args('foo'), "odd args fail verification" );
like( $@, qr/odd/, 'set $@ on odd arg failure' );
ok( !test_verify_args( bar => 'nope' ), "bad param doesn't verify" );
like( $@, qr/param/, 'set $@ on invalid param failure' );

my $foo = bless {}, 'Foo';
ok( a_isa_b( $foo, 'Foo' ), "a_isa_b true" );
ok( !a_isa_b( $foo,  'Bar' ), "a_isa_b false" );
ok( !a_isa_b( 'Foo', 'Foo' ), "a_isa_b not blessed" );
ok( !a_isa_b( undef, 'Foo' ), "a_isa_b undef" );

