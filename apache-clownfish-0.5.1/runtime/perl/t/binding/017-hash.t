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
use Clownfish qw( to_clownfish );

my $hash = Clownfish::Hash->new( capacity => 10 );
$hash->store( "foo", Clownfish::String->new("bar") );
$hash->store( "baz", Clownfish::String->new("banana") );

my $iter = Clownfish::HashIterator->new($hash);
isa_ok( $iter, 'Clownfish::HashIterator' );
for (my $i = 0; $i < 2; $i++) {
    ok( $iter->next, "iter next $i" );
    my $key = $iter->get_key;
    is( $iter->get_value, $hash->fetch($key), "iter get $i" );
}
ok( !$iter->next, 'iter next final');

ok( !defined( $hash->fetch("blah") ),
    "fetch for a non-existent key returns undef" );

$hash->clear();
$hash->store( "nada", undef );
ok( !defined($hash->fetch("nada")), "store/fetch undef value" );
is( $hash->get_size, 1, "size after storing undef value" );

my %hash_with_utf8_keys = ( "\x{263a}" => "foo" );
my $round_tripped = to_clownfish( \%hash_with_utf8_keys )->to_perl;
is_deeply( $round_tripped, \%hash_with_utf8_keys,
    "Round trip conversion of hash with UTF-8 keys" );

