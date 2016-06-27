#!/bin/sh
gcc -Iapache-clownfish-0.5.1/runtime/c/autogen/include -Iapache-lucy-0.5.1/c/autogen/include index.c -lcurl -llucy -lcfish -o index
gcc -Iapache-clownfish-0.5.1/runtime/c/autogen/include -Iapache-lucy-0.5.1/c/autogen/include search.c -lcurl -llucy -lcfish -o search

