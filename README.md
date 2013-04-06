BgpCompare v1.0
===============

Author: Tibor Djurica Potpara <tibor.djurica@ojdip.net>

BgpCompare is an utility that takes two files containing IP(v6) subn-
ets in [Address]/[Subnet mask] notation and outputs a binary set ope-
ration performed on these two files. Use it to compare BGP route tab-
les for missing routes.

BgpCompare is a free software published under the terms of GNU Lesser
General Public Licence version 3. See licence.txt for more info.

Installation instructions
=========================

Compile with 
   
    g++ -std=c++0x -I . BgpCompare.cpp -lboost_regex -o bgpcompare

or

    g++ -std=c++0x -I . BgpCompare.cpp -DUSE_STD_REGEX -o bgpcompare

if your standard C++ library includes <regex>

BgpCompare requires a C++11 compliant compiler (auto, nullptr, lambdas,
strict enum types)

2. Invoke bgpcompare with -h option for command line options
