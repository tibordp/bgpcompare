/*
IPAddress.h - classes for working with IP(v6) addresses (IP arithmetics,
			  parsing, textual representation)
			  
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

#pragma once

#include<string>
#include<sstream>
#include<vector>
#include<cstdint>
#include<stdexcept>

namespace IPAddress {

	class IPv4
	{
	private:
		// This parser considers IPv4 addresses with leading zeros in parts valid.
		// Leading zeros do not signify octal notation

		void parse(std::string::const_iterator begin, 
			std::string::const_iterator end)  {
				value = 0;
				int octet = 0, count = 0;

				std::string invalid_format = "Invalid IPv4 format (" + std::string(begin, end) + ")";

				if (*begin == '.') 
					throw std::runtime_error(invalid_format);

				for (auto iter = begin; 
					iter != end; ++iter)
				{
					if (*iter <= '9' && *iter >= '0') 
						octet = octet * 10 + (*iter - '0');
					else
						if (*iter == '.' && octet < 256)
						{
							// There shouldn't be a ".." anywhere in the address,
							// nor should an address end with "."
							if (iter + 1 == end)
								throw std::runtime_error(invalid_format);
							else
								if (*(iter + 1) == '.')
									throw std::runtime_error(invalid_format);

							value = (value << 8) | octet;
							octet = 0;
							count ++;					
						}
						else
							// If there are unknown characrers or if number is >= 256
							throw std::runtime_error(invalid_format);
				}

				// Till this point three octets should've been read.
				if (count != 3 || octet >= 256)
					throw std::runtime_error(invalid_format);

				value = (value << 8) | octet;
		}

	public:
		static const int bit_length = 32;
		uint32_t value;

		IPv4 network_zeros(const short prefix = 0) const {
			// Due to undefined behavior otherwise
			if (prefix == 0) return IPv4(0);
			return IPv4(value & (0xffffffff << (32 - prefix)));
		};
		IPv4 network_ones (const short prefix = 0) const {
			// Due to undefined behavior otherwise
			if (prefix == 0) return IPv4(0xffffffff);
			return IPv4(value | (~ (0xffffffff << (32 - prefix))));
		};

		std::string to_string() const {
			std::stringstream buf;
			buf << std::dec << ((value >> 24) & 0xff) << "."
				<< std::dec << ((value >> 16) & 0xff)  << "."
				<< std::dec << ((value >> 8)  & 0xff) << "."
				<< std::dec << ((value >> 0)  & 0xff);
			return buf.str();
		}

		IPv4() : value(0) {};

		IPv4(uint32_t value_) : value(value_) {};

		IPv4(std::string::const_iterator begin, 
			std::string::const_iterator end) {
				parse(begin, end);
		}

		IPv4(const std::string& text) {
			parse(text.cbegin(),text.cend());
		};

		static IPv4 subnet_mask(const short prefix)
		{
			if (prefix < 0 || prefix > 32) 
				throw std::runtime_error("Invalid prefix size");
			return IPv4(0xffffff << (32-prefix));
		}

		IPv4 next(const short prefix = bit_length)
		{
			if (prefix < 0 || prefix > bit_length) 
				throw std::runtime_error("Invalid prefix size");
			return IPv4(value + (1 << (32-prefix)));
		}

		IPv4 previous(const short prefix = bit_length)
		{
			if (prefix < 0 || prefix > bit_length) 
				throw std::runtime_error("Invalid prefix size");
			return IPv4(value - (1 << (32-prefix)));
		}

		bool operator < (const IPv4& a) const
		{
			return value < a.value;
		}

		bool operator > (const IPv4& a) const
		{
			return value > a.value;
		}

		bool operator <= (const IPv4& a) const
		{
			return value <= a.value;
		}

		bool operator >= (const IPv4& a) const
		{
			return value >= a.value;
		}

		bool operator == (const IPv4& a) const
		{
			return value == a.value;
		}

		bool operator != (const IPv4& a) const
		{
			return value != a.value;
		}
	};

	class IPv6 
	{
	private:
		typedef std::vector<uint16_t> segment_vector;

		segment_vector segmentize() const
		{
			segment_vector segments(8);

			for (int i=0; i < 4; i ++ )
			{
				segments[i] =   (network >> (48 - i * 16))  & 0xffff;
				segments[i+4] = (host >> (48 - i * 16))  & 0xffff;
			}

			return segments;
		}

		std::string collapse(segment_vector::const_iterator begin, segment_vector::const_iterator end) const
		{
			std::stringstream buf;

			segment_vector::const_iterator collapse(end), 
				collapse_start(end), collapse_end(end);

			for (auto iter = begin; iter != end; ++iter)
			{
				if (*iter == 0)
				{
					if (collapse == end)
						collapse = iter;

				} else
				{
					if (collapse != end)
					{
						// Strictly greater than, because first occurence of multiple 0's
						// must be collapsed as per RFC 5952
						if (iter - collapse > collapse_end - collapse_start)
						{
							collapse_start = collapse;
							collapse_end   = iter;
						}
						collapse = end;					
					}
				}
			}

			// In the case of "::" at the end.
			if (collapse != end) 
			{
				if ( end - collapse > collapse_end - collapse_start )
				{
					collapse_start = collapse;
					collapse_end   = end;
				}
				collapse = end;		
			}

			// If longest consectutive chain is 1 segment long, we do not collapse
			if (collapse_end - collapse_start == 1)
				collapse_start = end;

			for (auto iter = begin; iter != end; ++iter)
			{
				if (iter == collapse_start)
					buf << "::";
				if (iter >= collapse_start && iter < collapse_end)
					continue;
				buf << std::hex << *iter;
				if (iter + 1 != end && iter + 1 != collapse_start)
					buf << ":";
			}

			return buf.str();
		}

		uint16_t parse_segment(std::string::const_iterator begin, 
			std::string::const_iterator end)
		{
			std::stringstream buf(std::string(begin,end));
			uint16_t result;
			buf >> std::hex >> result;
			return result;
		}

		// This parser has been validated with test cases that are provided in
		// http://download.dartware.com/thirdparty/test-ipv6-regex.pl.

		void parse(std::string::const_iterator begin, 
			std::string::const_iterator end) {
				network = 0; host = 0;

				std::string invalid_format = "Invalid IPv6 format (" + std::string(begin, end) + ")";

				segment_vector segments_first, segments_second;
				segment_vector* segments = &segments_first;

				auto iter = begin;

				// If address begins with ":", it should be valid only in case that
				// "::" is at the beginning.
				if (*begin == ':')
				{
					if (*(begin+1) != ':')
						throw std::runtime_error(invalid_format);
					segments = &segments_second;
					iter += 2;
				}

				auto segment_start = iter,
					segment_end = iter;
				bool IPv4_embedded = false;

				for (; iter != end; ++iter)
				{
					if ((*iter >= 'a' && *iter <= 'f') ||
						(*iter >= 'A' && *iter <= 'F') ||
						(*iter >= '0' && *iter <= '9'))
					{
						continue;
					}
					else if (*iter == ':')
					{
						segment_end = iter;

						// No more than 4 characters per segment
						if (segment_end - segment_start > 4) 
							throw std::runtime_error(invalid_format);				

						// Indicates a "::". Should only be valid if there hasn't
						// been one till this point.
						if (segment_end - segment_start == 0) 
						{
							if (segments == &segments_second)
								// Veè kot en :: na naslov
								throw std::runtime_error(invalid_format);
							else
								segments = &segments_second;
						} else
							segments->push_back(parse_segment(segment_start,segment_end));

						segment_start = iter + 1;

						// If there is a "." in the segment, we assume that the rest of an address
						// is IPv4.
					} else if (*iter == '.') 
					{
						IPv4_embedded = true;
						break;
					}
					// Invalid characters in input.
					else throw std::runtime_error(invalid_format);

				}

				if (*(end-1) == ':' && !(segments == &segments_second 
					&& segments_second.size() == 0))
					throw std::runtime_error(invalid_format);

				if (IPv4_embedded)
				{
					IPv4 embedded_part(segment_start, end);
					segments->push_back(embedded_part.value >> 16);
					segments->push_back(embedded_part.value & 0xffff);
				}
				else
					// If an address ends with "::", we should keep the second vector
					// empty, even though adding a single 0x0000 would result in the same
					// address. Doing so saves us one step on address verification.
					if  (*(end-1) != ':')
						segments->push_back(parse_segment(segment_start, end));

				// If "::" is not present in the address.
				if (segments == &segments_first)
				{
					if (segments_first.size() != 8)
						throw std::runtime_error(invalid_format);
				}
				else
				{
					if (segments_first.size() + segments_second.size() >= 8)
						throw std::runtime_error(invalid_format);
				}

				// We iterate through the first segment vector and insert segments into
				// appropriate adress parts.
				for (segment_vector::size_type i=0; i < segments_first.size(); i++)
				{
					if (i < 4)
						network |= (static_cast<uint64_t>
						(segments_first[i]) << ((3 - i) * 16));
					else
						host    |= (static_cast<uint64_t>
						(segments_first[i]) << ((7 - i) * 16));
				}

				// We iterate through the first segment vector (backwards so as not to care
				// how long a "::" is) and insert segments into appropriate adress parts.
				for (segment_vector::size_type i=0; i < segments_second.size(); i++)
				{
					if (i < 4)
						host |= (static_cast<uint64_t>
						(segments_second[segments_second.size()-i-1]) << (i * 16));
					else
						network   |= (static_cast<uint64_t>
						(segments_second[segments_second.size()-i-1]) << ((i-4) * 16));
				}
		}

	public:
		static const int bit_length = 128;
		uint64_t network;
		uint64_t host;

		IPv6 network_zeros(const short prefix = 0) const {
			if (prefix == 0) 
				return IPv6(0,0);
			if (prefix > 64) 
				return IPv6(network, host & (0xffffffffffffffff << (128 - prefix )));
			else 
				return IPv6(network & (0xffffffffffffffff << (64 - prefix )), 0);
		};

		IPv6 network_ones (const short prefix = 0) const {		
			if (prefix == 0) 
				return IPv6(0xffffffffffffffff,0xffffffffffffffff);
			if (prefix > 64) 
				return IPv6(network, host | (~ (0xffffffffffffffff << (128 - prefix )))); 
			else 
				return IPv6(network | (~(0xffffffffffffffff << (64 - prefix ))), 0xffffffffffffffff);	
		}; 

		// eg. "2001:db8::1020:ff"
		std::string to_string() const {
			segment_vector segments = segmentize();
			return collapse(segments.begin(), segments.end());
		}

		// eg. "2001:db8::16.32.0.255"
		std::string to_string_v4_mapped() const {
			segment_vector segments = segmentize();
			std::string v6part = collapse(segments.begin(), segments.end() - 2);

			// If there is a "::" at the end, we don't add another
			return v6part + (*(v6part.end() - 1) == ':' ? "" : ":") +
				IPv4((segments[6] << 16) | segments[7]).to_string();
		}

		// eg. "2001:0db8:0000:0000:0000:0000:1020:00ff"
		std::string to_string_full() const {
			std::stringstream buf;

			// A local lambda to print a single address part, ommiting the trailing ":"
			auto print_part = [&buf] (uint64_t part) { for (int i = 15; i >= 0; i--)
			{
				buf << std::hex << ((part >> i*4) & 0xf);
				if (!(i % 4) && (i != 0))
					buf << ":";
			}};

			print_part(network);
			buf << ":";
			print_part(host);

			return buf.str();
		};

		IPv6() : network(0), host(0) {};
		IPv6(uint64_t network_, uint64_t host_) : 
		network(network_), host(host_) {};

		IPv6(std::string::const_iterator begin, 
			std::string::const_iterator end) {
				parse(begin, end);
		}

		static IPv6 prefix_6to4(const IPv4& a)
		{
			return IPv6(0x2002000000000000 | 
				(static_cast<uint64_t>(a.value) << 16), 0);
		}

		IPv6 next(const short prefix = bit_length)
		{
			if (prefix < 0 || prefix > bit_length) 
				throw std::runtime_error("Invalid prefix size");
			if (prefix <= 64) 
				return IPv6(network + 
				( static_cast<uint64_t>(0x1) << (64 - prefix)), host);
			else
			{
				uint64_t newhost = host + (static_cast<uint64_t>(0x1) << (128 - prefix));
				return IPv6(network + (newhost < host ? 1 : 0), newhost);
			}
		}

		IPv6 previous(const short prefix = bit_length)
		{
			if (prefix < 0 || prefix > bit_length) 
				throw std::runtime_error("Invalid prefix size");
			if (prefix <= 64) 
				return IPv6(network - 
				( static_cast<uint64_t>(0x1) << (64 - prefix)), host);
			else
			{
				uint64_t newhost = host - (static_cast<uint64_t>(0x1) << (128 - prefix));
				return IPv6(network - (newhost > host ? 1 : 0), newhost);
			}
		}

		IPv6(const std::string& text) {
			parse(text.cbegin(),text.cend());
		};

		bool operator < (const IPv6& a) const
		{
			return (network < a.network) || 
				(network == a.network && host < a.host);
		};

		bool operator > (const IPv6& a) const
		{
			return (network > a.network) || 
				(network == a.network && host > a.host);
		};

		bool operator <= (const IPv6& a) const
		{
			return ! operator>(a);
		};

		bool operator >= (const IPv6& a) const
		{
			return ! operator<(a);
		};

		bool operator == (const IPv6& a) const
		{
			return (network == a.network) && (host == a.host);
		};

		bool operator != (const IPv6& a) const
		{
			return ! operator==(a);
		};
	};
}