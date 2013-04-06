/*
BgpCompare.cpp - utility that performs binary set operations on collections 
of IP(v6) address ranges (such as routing tables) 

Coded by Tibor Djurica Potpara <tibor.djurica@ojdip.net>, 2012

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include<iostream>
#include<fstream>
#include<string>
#include<algorithm>
#include<functional>

#ifdef USE_STD_REGEX
	#include<regex>
	namespace regex_namespace = std;
#else
	#include<boost/regex.hpp>
	namespace regex_namespace = boost;
#endif

#include<IPAddress.h>

template<typename T>
struct IPNode
{
	T ip;
	short prefix;

	IPNode(T ip_, short prefix_) : 
		ip(ip_), prefix(prefix_) {};
};

enum IPMarkerType 
{ 
	ipm_invalid,
	ipm_a_open, 
	ipm_a_close,
	ipm_b_open, 
	ipm_b_close
};

template<typename T>
struct IPMarker
{
	T ip;
	IPMarkerType type;

	IPMarker() :
		ip(T()), type(ipm_invalid) {};

	IPMarker(T ip_, IPMarkerType type_) :
		ip(ip_), type(type_) {};

	bool operator < (const IPMarker& a) const 
	{
		return ip < a.ip;
	}
};

// OutputAdapter base class. Provides a callback function for `IPNode`s

template<typename T>
class OutputAdapter
{
public:
	virtual void operator ()(const IPNode<T>& node) const = 0;
	virtual bool empty() const { return false; };
};

template<typename T>
class EmptyOutputAdapter : public OutputAdapter<T>
{
public:
	virtual void operator ()(const IPNode<T>& node) const {};
	virtual bool empty() const { return true; };
};

template<typename T>
class SimpleAdapter : public OutputAdapter<T>
{
	virtual void operator ()(const IPNode<T>& node) const 
	{
		std::cout << node.ip.to_string() << "/" << node.prefix << std::endl;
	}
};

template<typename T>
class DiffAdapter : public OutputAdapter<T>
{
private:
	std::string prefix_;
public:
	DiffAdapter(const std::string& prefix) : prefix_(prefix) {};

	virtual void operator ()(const IPNode<T>& node) const 
	{
		std::cout << prefix_ << node.ip.to_string() << "/" << node.prefix << std::endl;
	}
};

// ComparisonKernel base class. Provides a means to perform different operations on sets.

class ComparisonKernel
{
public:
	virtual bool operator ()(const int A, const int B) const = 0;
	virtual bool symetric() const = 0;
};

class UnionKernel : public ComparisonKernel
{
	virtual bool operator ()(const int A, const int B) const 
	{
		return A > 0 || B > 0;
	}
	virtual bool symetric() const { return true; }
};

class IntersectionKernel : public ComparisonKernel
{
	virtual bool operator ()(const int A, const int B) const 
	{
		return A > 0 && B > 0;
	}
	virtual bool symetric() const { return true; }
};

class DifferenceKernel : public ComparisonKernel
{
	virtual bool operator ()(const int A, const int B) const 
	{
		return A == 0 && B > 0;
	}
	virtual bool symetric() const { return false; }
};

template<typename T>
void read_regexp(const std::string& in_file, const regex_namespace::regex& regexp,
				 const OutputAdapter<T>& callback)
{
	// Reads a file line-by-line and calls a callback for every successfuly matched
	// and parsed IP address.

	regex_namespace::smatch what;
	std::ifstream file_to_read(in_file);

	if (!file_to_read)
		throw std::runtime_error("Cannot read file!");

	while (!file_to_read.eof())
	{
		std::string hay;
		getline(file_to_read, hay);

		if (regex_namespace::regex_match(hay, what, regexp))
		{		
			if (what.size() >= 2)
			{
				IPNode<T> new_node(T(what[1].str()), atoi(what[2].str().c_str()));
				callback(new_node);
			}
		}
	} 
}

template<typename T>
void add(T start, T stop, const OutputAdapter<T>& callback)
{
	// Collapses an IP range into a collection of subnets with
	// lowest prefix lengths.

	while (start < stop)
	{
		for (int i=0; i <= T::bit_length; i++)
		{
			if (start.network_zeros(i) == start && 
				start.network_ones(i) <= stop)
			{
				callback(IPNode<T>(start,i));

				// We have reached the top of the IP address space
				if (start.network_ones(i) == start.network_ones()) return;

				start = start.network_ones(i).next();
				break;
			}
		}

	}
}

// Description of the algorithm:
// http://stackoverflow.com/questions/11891109/algorithm-to-produce-a-difference-of-two-collections-of-intervals

template<typename T>
void traverse( 
	typename std::vector<IPMarker<T>>::iterator start, 
	typename std::vector<IPMarker<T>>::iterator stop, 
	const ComparisonKernel& kernel,
	const OutputAdapter<T>& callback_a,
	const OutputAdapter<T>& callback_b = EmptyOutputAdapter<T>())
{ 			
	// Traverses the marker vector, applies appropriate comparison kernel and
	// invokes appropriate callbacks.

	struct batch_data
	{
		int count;
		bool inside;
		T start;
		const OutputAdapter<T>& callback;

		batch_data(const OutputAdapter<T>& callback_) 
			: count(0), inside(false), callback(callback_) {};
	} A(callback_a), B(callback_b);

	for(auto iter = start; iter != stop; ++iter)
	{
		if (iter->type == ipm_a_open) A.count ++; 
		if (iter->type == ipm_a_close)  A.count --; 
		if (iter->type == ipm_b_open)  B.count ++; 
		if (iter->type == ipm_b_close)  B.count --;

		bool open = iter->type == ipm_a_open || iter->type == ipm_b_open;

		// We would like to skip duplicate block starts (while 
		// still counting them above, of course)

		if (iter + 1 != stop)
		{
			if ((iter + 1)->ip == iter->ip)
				continue;
		}

		auto track = [&](batch_data& A_, batch_data& B_) { 
			if (kernel(A_.count, B_.count))
			{
				if (!A_.inside)
				{
					A_.inside = true;
					if (open) A_.start = iter->ip;
					else      A_.start = iter->ip.next();
				}
			}
			else
			{
				if (A_.inside)
				{
					A_.inside = false;
					if (open) add<T>(A_.start, iter->ip.previous(), A_.callback);
					else      add<T>(A_.start, iter->ip,            A_.callback);
				}
			} 
		};

		track(A,B);

		// If a comparison kernel is commutative, we don't have to track both.

		if (!B.callback.empty())
			track(B,A);
	}
}

template<typename T>
class InsertAdapter : public OutputAdapter<T>
{
private:
	std::vector<IPMarker<T>>& vector_;
	IPMarkerType start_;
	IPMarkerType stop_;
public:
	InsertAdapter(std::vector<IPMarker<T>>& vector, IPMarkerType start, 
		IPMarkerType stop) : vector_(vector), start_(start), stop_(stop) {};

	virtual void operator ()(const IPNode<T>& node) const 
	{
		vector_.push_back(IPMarker<T>(node.ip.network_zeros(node.prefix), start_));  
		vector_.push_back(IPMarker<T>(node.ip.network_ones(node.prefix), stop_  ));
	}
};

template<typename T>
void process(const std::string & file1, 
			 const std::string & file2, 
			 const std::string & regex,
			 const ComparisonKernel& kernel
			 )
{
	std::vector<IPMarker<T>> markers;

	// We read IPs from both files by matching a regexp. If a regex matches
	// but IP is invalid, it will throw an exception.

	read_regexp<T>(file1, regex_namespace::regex(regex.c_str()), 
		InsertAdapter<T>(markers, ipm_a_open, ipm_a_close));
	read_regexp<T>(file2, regex_namespace::regex(regex.c_str()), 
		InsertAdapter<T>(markers, ipm_b_open, ipm_b_close));

	std::sort(markers.begin(), markers.end());

	// Deep magic begins here
	if (kernel.symetric())
		traverse<T>(markers.begin(), markers.end(), kernel, SimpleAdapter<T>());
	else
		traverse<T>(markers.begin(), markers.end(), kernel, DiffAdapter<T>("+"), DiffAdapter<T>("-"));
}

namespace default_regex
{
	// TODO: Tweak to work out-of-the box for most routing platforms
	const std::string IPv6 = "[^0-9a-fA-F\\:]*([0-9a-fA-F\\:\\.]+)/([0-9]+).*";
	const std::string IPv4 = "[^0-9]*([0-9\\.]+)/([0-9]+).*";
}

void print_syntax()
{
	std::cout << 
		"Usage: " <<std::endl <<
		"    bgpcompare [diff|union|intersect] [ipv6|ipv4] fileA fileB [regex]" << std::endl <<
		std::endl <<
		"Input:" << std::endl <<
		"The program will read IP addresses from files specified by fileA and " << std::endl <<
		"fileB, one per line. IP addresses are matched using a regular expres-" << std::endl <<
		"sion with two captures,  one for the address and the other for prefix" << std::endl <<
		"length. If a regular expression does  not match, the line is ignored." << std::endl <<
		"Full line must  be matched. By default, these two regular expressions" << std::endl <<
		"are used: " << std::endl <<
		"    IPv4: " << default_regex::IPv4 << std::endl <<
		"    IPv6: " << default_regex::IPv6 << std::endl <<
		"Custom regular expression can be provided in lieu of a default one by" << std::endl <<
		"filling a fourth command line parameter." << std::endl << 
		std::endl <<
		"Output:" << std::endl <<
		"The program will perform an operation on sets of IP subnets A and B. " << std::endl <<
		" diff:      The program will calculate both the difference of A and B" << std::endl <<
		"            and the difference of B and A. The results will be prefi-" << std::endl <<
		"            xed by '+' and '-' respectively (similar to diff syntax)." << std::endl <<
		" union:     The program will output the union of A and B (subnets ei-" << std::endl <<
		"            ther in A or in B)."	<< std::endl <<
		" intersect: The program will output the intersection of A and B (sub-" << std::endl <<
		"            nets both in A and in B)." << std::endl;
}

int main(int argc, char *argv[])
{
	const char* invalid_options = "Invalid command line parameters (use -h switch for help)";

	try {
		std::string regex;

		switch (argc)
		{
		case 0:
		case 1:
			print_syntax();
			return 0;
		case 2:
			{
				std::string param(argv[1]);
				if (param == "-h" || param == "/h" || param == "/?")
				{
					print_syntax();
					return 0;
				}
				else
					throw std::runtime_error(invalid_options);
			}
		case 6:
			regex = argv[5];
		case 5:		
			{
				std::string kernel_type = argv[1];
				std::string address_family = argv[2];

				std::unique_ptr<ComparisonKernel> kernel;

				if (kernel_type == "diff")  kernel.reset(new DifferenceKernel()); else
					if (kernel_type == "union") kernel.reset(new UnionKernel()); else
						if (kernel_type == "intersect") kernel.reset(new IntersectionKernel()); else
							throw std::runtime_error(invalid_options);

				if (address_family == "-6" || address_family == "/6" ||  address_family == "ipv6")
				{
					if (argc == 5) regex = default_regex::IPv6;
					process<IPAddress::IPv6>(argv[3], argv[4], regex, *kernel.get());
				}
				else
					if (address_family == "-4" || address_family == "/4" ||  address_family == "ipv4")
					{
						if (argc == 5) regex = default_regex::IPv4;
						process<IPAddress::IPv4>(argv[3], argv[4], regex, *kernel.get());
					}
					else
						throw std::runtime_error(invalid_options);
				break;
			}
		default: 
			throw std::runtime_error(invalid_options);
		}
	}
	catch(std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}

	return 0;
}
