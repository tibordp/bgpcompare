BgpCompare v1.0
===============

Author: Tibor Djurica Potpara <tibor.djurica@ojdip.net>

BgpCompare is an utility that takes two files containing IP(v6) subnets
in [Address]/[Subnet mask] notation and outputs a binary set operation
performed on these two files. Use it to compare BGP route tables 
for missing routes.

BgpCompare is free software published under the terms of GNU Lesser
General Public Licence version 3. See `LICENCE` for more info.

Installation instructions
=========================

Compile with:
   
    g++ -std=c++0x -I . BgpCompare.cpp -lboost_regex -o bgpcompare

or:

    g++ -std=c++0x -I . BgpCompare.cpp -DUSE_STD_REGEX -o bgpcompare

if your standard C++ library includes `<regex>`

BgpCompare requires a C++11 compliant compiler (`auto`, `nullptr`, `lambda`s,
strict `enum` types)

After building, invoke `bgpcompare` with the `-h` flag for command line options.
