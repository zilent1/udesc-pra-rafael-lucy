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

package Clownfish::Build;

my $IS_CPAN_DIST;

BEGIN {
    $IS_CPAN_DIST = -e 'core';

    if (!$IS_CPAN_DIST) {
        unshift @INC,
                '../../compiler/perl/blib/lib',
                '../../compiler/perl/blib/arch',
                '../../compiler/perl/lib'; # blib dir might not exist yet.
    }
}

use base qw(
    Clownfish::CFC::Perl::Build
    Clownfish::CFC::Perl::Build::Charmonic
);

our $VERSION = '0.005001';
$VERSION = eval $VERSION;

use File::Spec::Functions qw( catdir catfile updir rel2abs );
use File::Path qw( rmtree );
use File::Copy qw( move );
use Config;
use Carp;
use Cwd qw( getcwd );

my @BASE_PATH = __PACKAGE__->cf_base_path;

my $COMMON_SOURCE_DIR = catdir( @BASE_PATH, 'common' );
my $CORE_SOURCE_DIR   = catdir( @BASE_PATH, 'core' );
my $CFC_DIR           = catdir( @BASE_PATH, updir(), 'compiler', 'perl' );
my $XS_SOURCE_DIR = 'xs';
my $CFC_BUILD     = catfile( $CFC_DIR, 'Build' );
my $LIB_DIR       = 'lib';
my $CHARMONIZER_C;
if ($IS_CPAN_DIST) {
    $CHARMONIZER_C = 'charmonizer.c';
}
else {
    $CHARMONIZER_C = catfile( $COMMON_SOURCE_DIR, 'charmonizer.c' );
}

sub new {
    my ( $class, %args ) = @_;
    $args{include_dirs}     = [ $CORE_SOURCE_DIR, $XS_SOURCE_DIR ];
    $args{clownfish_params} = {
        autogen_header => _autogen_header(),
        include        => [],                  # Don't use default includes.
        source => [ $CORE_SOURCE_DIR, $XS_SOURCE_DIR ],
    };
    my $self = $class->SUPER::new( recursive_test_files => 1, %args );

    # Fix for MSVC: Although the generated XS should be C89-compliant, it
    # must be compiled in C++ mode like the rest of the code due to a
    # mismatch between the sizes of the C++ bool type and the emulated bool
    # type. (The XS code is compiled with Module::Build's extra compiler
    # flags, not the Clownfish cflags.)
    if ($Config{cc} =~ /^cl\b/) {
        my $extra_cflags = $self->extra_compiler_flags;
        push @$extra_cflags, '/TP';
        $self->extra_compiler_flags(@$extra_cflags);
    }

    if ( $ENV{LUCY_VALGRIND} ) {
        my $optimize = $self->config('optimize') || '';
        $optimize =~ s/\-O\d+/-O1/g;
        $self->config( optimize => $optimize );
    }

    $self->charmonizer_params( charmonizer_c => $CHARMONIZER_C );

    return $self;
}

sub _run_make {
    my ( $self, %params ) = @_;
    my @command           = @{ $params{args} };
    my $dir               = $params{dir};
    my $current_directory = getcwd();
    chdir $dir if $dir;
    unshift @command, 'CC=' . $self->config('cc');
    if ( $self->config('cc') =~ /^cl\b/ ) {
        unshift @command, "-f", "Makefile.MSVC";
    }
    elsif ( $^O =~ /mswin/i ) {
        unshift @command, "-f", "Makefile.MinGW";
    }
    unshift @command, "$Config{make}";
    system(@command) and confess("$Config{make} failed");
    chdir $current_directory if $dir;
}

sub ACTION_cfc {
    my $self = shift;
    return if $IS_CPAN_DIST;
    my $old_dir = getcwd();
    chdir($CFC_DIR);
    if ( !-f 'Build' ) {
        print "\nBuilding Clownfish compiler... \n";
        system("$^X Build.PL");
        system("$^X Build code");
        print "\nFinished building Clownfish compiler.\n\n";
    }
    chdir($old_dir);
}

sub ACTION_copy_clownfish_includes {
    my $self = shift;

    $self->SUPER::ACTION_copy_clownfish_includes;

    $self->cf_copy_include_file( 'XSBind.h' );
}

sub ACTION_clownfish {
    my $self = shift;

    $self->depends_on('cfc');

    $self->SUPER::ACTION_clownfish;
}

sub ACTION_compile_custom_xs {
    my $self = shift;

    $self->depends_on('charmony');

    # Add extra compiler flags from Charmonizer.
    my $charm_cflags = $self->charmony('EXTRA_CFLAGS');
    if ($charm_cflags) {
        my $cf_cflags = $self->clownfish_params('cflags');
        if ($cf_cflags) {
            $cf_cflags .= " $charm_cflags";
        }
        else {
            $cf_cflags = $charm_cflags;
        }
        $self->clownfish_params( cflags => $cf_cflags );
    }

    # Add extra linker flags from Charmonizer.
    my $charm_ldflags = $self->charmony('EXTRA_LDFLAGS');
    if ($charm_ldflags) {
        my $extra_ldflags = $self->extra_linker_flags;
        push @$extra_ldflags, $self->split_like_shell($charm_ldflags);
        $self->extra_linker_flags(@$extra_ldflags);
    }

    $self->SUPER::ACTION_compile_custom_xs;
}

sub _valgrind_base_command {
    return
          "PERL_DESTRUCT_LEVEL=2 LUCY_VALGRIND=1 valgrind "
        . "--leak-check=yes "
        . "--show-reachable=yes "
        . "--dsymutil=yes "
        . "--suppressions=../../devel/conf/cfruntime-perl.supp ";
}

# Run the entire test suite under Valgrind.
#
# For this to work, Lucy must be compiled with the LUCY_VALGRIND environment
# variable set to a true value, under a debugging Perl.
#
# A custom suppressions file will probably be needed -- use your judgment.
# To pass in one or more local suppressions files, provide a comma separated
# list like so:
#
#   $ ./Build test_valgrind --suppressions=foo.supp,bar.supp
sub ACTION_test_valgrind {
    my $self = shift;
    # Debian's debugperl uses the Config.pm of the standard system perl
    # so -DDEBUGGING won't be detected.
    die "Must be run under a perl that was compiled with -DDEBUGGING"
        unless $self->config('ccflags') =~ /-D?DEBUGGING\b/
               || $^X =~ /\bdebugperl\b/;
    if ( !$ENV{LUCY_VALGRIND} ) {
        warn "\$ENV{LUCY_VALGRIND} not true -- possible false positives";
    }
    $self->depends_on('code');

    # Unbuffer STDOUT, grab test file names and suppressions files.
    $|++;
    my $t_files = $self->find_test_files;    # not public M::B API, may fail
    my $valgrind_command = $self->_valgrind_base_command;

    if ( my $local_supp = $self->args('suppressions') ) {
        for my $supp ( split( ',', $local_supp ) ) {
            $valgrind_command .= "--suppressions=$supp ";
        }
    }

    # Iterate over test files.
    my @failed;
    for my $t_file (@$t_files) {

        # Run test file under Valgrind.
        print "Testing $t_file...";
        die "Can't find '$t_file'" unless -f $t_file;
        my $command = "$valgrind_command $^X -Mblib $t_file 2>&1";
        my $output = "\n" . ( scalar localtime(time) ) . "\n$command\n";
        $output .= `$command`;

        # Screen-scrape Valgrind output, looking for errors and leaks.
        if (   $?
            or $output =~ /ERROR SUMMARY:\s+[^0\s]/
            or $output =~ /definitely lost:\s+[^0\s]/
            or $output =~ /possibly lost:\s+[^0\s]/
            or $output =~ /still reachable:\s+[^0\s]/ )
        {
            print " failed.\n";
            push @failed, $t_file;
            print "$output\n";
        }
        else {
            print " succeeded.\n";
        }
    }

    # If there are failed tests, print a summary list.
    if (@failed) {
        print "\nFailed "
            . scalar @failed . "/"
            . scalar @$t_files
            . " test files:\n    "
            . join( "\n    ", @failed ) . "\n";
        exit(1);
    }
}

sub _autogen_header {
    return <<"END_AUTOGEN";
***********************************************

!!!! DO NOT EDIT !!!!

This file was auto-generated by Build.PL.

***********************************************

Licensed to the Apache Software Foundation (ASF) under one or more
contributor license agreements.  See the NOTICE file distributed with
this work for additional information regarding copyright ownership.
The ASF licenses this file to You under the Apache License, Version 2.0
(the "License"); you may not use this file except in compliance with
the License.  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
END_AUTOGEN
}

# Run the cleanup targets for independent prerequisite builds.
sub _clean_prereq_builds {
    my $self = shift;
    if ( -e $CFC_BUILD ) {
        my $old_dir = getcwd();
        chdir $CFC_DIR;
        system("$^X Build realclean")
            and die "Clownfish clean failed";
        chdir $old_dir;
    }
}

sub ACTION_clean {
    my $self = shift;
    _clean_prereq_builds($self);
    $self->SUPER::ACTION_clean;
}

sub ACTION_dist {
    my $self = shift;

    # Create POD.
    $self->depends_on('clownfish');
    rmtree("autogen");

    # We build our Perl release tarball from a subdirectory rather than from
    # the top-level $REPOS_ROOT.  Because some assets we need are outside this
    # directory, we need to copy them in.
    my %to_copy = (
        '../../CONTRIBUTING.md' => 'CONTRIBUTING.md',
        '../../LICENSE'         => 'LICENSE',
        '../../NOTICE'          => 'NOTICE',
        '../../README.md'       => 'README.md',
        $CORE_SOURCE_DIR        => 'core',
        $CHARMONIZER_C          => 'charmonizer.c',
    );
    print "Copying files...\n";
    while ( my ( $from, $to ) = each %to_copy ) {
        confess("'$to' already exists") if -e $to;
        system("cp -R $from $to") and confess("cp failed");
    }
    move( "MANIFEST", "MANIFEST.bak" ) or die "move() failed: $!";
    $self->depends_on("manifest");
    $self->SUPER::ACTION_dist;

    # Now that the tarball is packaged up, delete the copied assets.
    rmtree($_) for values %to_copy;
    unlink("META.yml");
    unlink("META.json");
    move( "MANIFEST.bak", "MANIFEST" ) or die "move() failed: $!";
}

1;

