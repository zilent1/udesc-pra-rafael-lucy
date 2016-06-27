# Sample subclass of QueryParser.

Implement a custom search query language using a subclass of
[](cfish:lucy.QueryParser).

## The language

At first, our query language will support only simple term queries and phrases
delimited by double quotes.  For simplicity's sake, it will not support
parenthetical groupings, boolean operators, or prepended plus/minus.  The
results for all subqueries will be unioned together -- i.e. joined using an OR
-- which is usually the best approach for small-to-medium-sized document
collections.

Later, we'll add support for trailing wildcards.

## Single-field parser

Our initial parser implentation will generate queries against a single fixed
field, "content", and it will analyze text using a fixed choice of English
EasyAnalyzer.  We won't subclass Lucy::Search::QueryParser just yet.

~~~ perl
package FlatQueryParser;
use Lucy::Search::TermQuery;
use Lucy::Search::PhraseQuery;
use Lucy::Search::ORQuery;
use Carp;

sub new { 
    my $analyzer = Lucy::Analysis::EasyAnalyzer->new(
        language => 'en',
    );
    return bless { 
        field    => 'content',
        analyzer => $analyzer,
    }, __PACKAGE__;
}
~~~

Some private helper subs for creating TermQuery and PhraseQuery objects will
help keep the size of our main parse() subroutine down:

~~~ perl
sub _make_term_query {
    my ( $self, $term ) = @_;
    return Lucy::Search::TermQuery->new(
        field => $self->{field},
        term  => $term,
    );
}

sub _make_phrase_query {
    my ( $self, $terms ) = @_;
    return Lucy::Search::PhraseQuery->new(
        field => $self->{field},
        terms => $terms,
    );
}
~~~

Our private \_tokenize() method treats double-quote delimited material as a
single token and splits on whitespace everywhere else.

~~~ perl
sub _tokenize {
    my ( $self, $query_string ) = @_;
    my @tokens;
    while ( length $query_string ) {
        if ( $query_string =~ s/^\s+// ) {
            next;    # skip whitespace
        }
        elsif ( $query_string =~ s/^("[^"]*(?:"|$))// ) {
            push @tokens, $1;    # double-quoted phrase
        }
        else {
            $query_string =~ s/(\S+)//;
            push @tokens, $1;    # single word
        }
    }
    return \@tokens;
}
~~~

The main parsing routine creates an array of tokens by calling \_tokenize(),
runs the tokens through through the EasyAnalyzer, creates TermQuery or
PhraseQuery objects according to how many tokens emerge from the
EasyAnalyzer's split() method, and adds each of the sub-queries to the primary
ORQuery.

~~~ perl
sub parse {
    my ( $self, $query_string ) = @_;
    my $tokens   = $self->_tokenize($query_string);
    my $analyzer = $self->{analyzer};
    my $or_query = Lucy::Search::ORQuery->new;

    for my $token (@$tokens) {
        if ( $token =~ s/^"// ) {
            $token =~ s/"$//;
            my $terms = $analyzer->split($token);
            my $query = $self->_make_phrase_query($terms);
            $or_query->add_child($phrase_query);
        }
        else {
            my $terms = $analyzer->split($token);
            if ( @$terms == 1 ) {
                my $query = $self->_make_term_query( $terms->[0] );
                $or_query->add_child($query);
            }
            elsif ( @$terms > 1 ) {
                my $query = $self->_make_phrase_query($terms);
                $or_query->add_child($query);
            }
        }
    }

    return $or_query;
}
~~~

## Multi-field parser

Most often, the end user will want their search query to match not only a
single 'content' field, but also 'title' and so on.  To make that happen, we
have to turn queries such as this...

    foo AND NOT bar

... into the logical equivalent of this:

    (title:foo OR content:foo) AND NOT (title:bar OR content:bar)

Rather than continue with our own from-scratch parser class and write the
routines to accomplish that expansion, we're now going to subclass Lucy::Search::QueryParser
and take advantage of some of its existing methods.

Our first parser implementation had the "content" field name and the choice of
English EasyAnalyzer hard-coded for simplicity, but we don't need to do that
once we subclass Lucy::Search::QueryParser.  QueryParser's constructor --
which we will inherit, allowing us to eliminate our own constructor --
requires a Schema which conveys field
and Analyzer information, so we can just defer to that.

~~~ perl
package FlatQueryParser;
use base qw( Lucy::Search::QueryParser );
use Lucy::Search::TermQuery;
use Lucy::Search::PhraseQuery;
use Lucy::Search::ORQuery;
use PrefixQuery;
use Carp;

# Inherit new()
~~~

We're also going to jettison our \_make_term_query() and \_make_phrase_query()
helper subs and chop our parse() subroutine way down.  Our revised parse()
routine will generate Lucy::Search::LeafQuery objects instead of TermQueries
and PhraseQueries:

~~~ perl
sub parse {
    my ( $self, $query_string ) = @_;
    my $tokens = $self->_tokenize($query_string);
    my $or_query = Lucy::Search::ORQuery->new;
    for my $token (@$tokens) {
        my $leaf_query = Lucy::Search::LeafQuery->new( text => $token );
        $or_query->add_child($leaf_query);
    }
    return $self->expand($or_query);
}
~~~

The magic happens in QueryParser's expand() method, which walks the ORQuery
object we supply to it looking for LeafQuery objects, and calls expand_leaf()
for each one it finds.  expand_leaf() performs field-specific analysis,
decides whether each query should be a TermQuery or a PhraseQuery, and if
multiple fields are required, creates an ORQuery which mults out e.g.  `foo`
into `(title:foo OR content:foo)`.

## Extending the query language

To add support for trailing wildcards to our query language, we need to
override expand_leaf() to accommodate PrefixQuery, while deferring to the
parent class implementation on TermQuery and PhraseQuery.

~~~ perl
sub expand_leaf {
    my ( $self, $leaf_query ) = @_;
    my $text = $leaf_query->get_text;
    if ( $text =~ /\*$/ ) {
        my $or_query = Lucy::Search::ORQuery->new;
        for my $field ( @{ $self->get_fields } ) {
            my $prefix_query = PrefixQuery->new(
                field        => $field,
                query_string => $text,
            );
            $or_query->add_child($prefix_query);
        }
        return $or_query;
    }
    else {
        return $self->SUPER::expand_leaf($leaf_query);
    }
}
~~~

Ordinarily, those asterisks would have been stripped when running tokens
through the EasyAnalyzer -- query strings containing "foo\*" would produce
TermQueries for the term "foo".  Our override intercepts tokens with trailing
asterisks and processes them as PrefixQueries before `SUPER::expand_leaf` can
discard them, so that a search for "foo\*" can match "food", "foosball", and so
on.

## Usage

Insert our custom parser into the search.cgi sample app to get a feel for how
it behaves:

~~~ perl
my $parser = FlatQueryParser->new( schema => $searcher->get_schema );
my $query  = $parser->parse( decode( 'UTF-8', $cgi->param('q') || '' ) );
my $hits   = $searcher->hits(
    query      => $query,
    offset     => $offset,
    num_wanted => $page_size,
);
...
~~~

