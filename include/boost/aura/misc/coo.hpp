/*
 * Copyright (C) 2011-2015 Biomedizinische NMR Forschungs GmbH
 *         Author: Sebastian Schaetz <sschaet@gwdg.de>
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *      (See accompanying file LICENSE_1_0.txt or copy at
 *            http://www.boost.org/LICENSE_1_0.txt)
 */

#ifndef AURA_MISC_COO_HPP
#define AURA_MISC_COO_HPP

#include <tuple>
#include <complex>
#include <vector>
#include <iostream>
#include <fstream>
#include <boost/aura/bounds.hpp>

namespace boost
{
namespace aura 
{

struct coo_index
{
	inline coo_index() : size(1), stride(1), start(0), end(0) {}
	unsigned long size;
	long stride;
	unsigned long start;
	unsigned long end;
};


inline void fill_stride_end(std::vector<coo_index>& result)
{
	result[0].end = result[0].stride * result[0].size;
	for (std::size_t i=1; i<result.size(); i++) {
		result[i].stride = result[i-1].end;
		result[i].end = result[i].stride * result[i].size;
	}
}

inline std::array<char, 4096> coo_build_header(const std::vector<coo_index>& data)
{
	std::array<char, 4096> header;
        header.fill(0);
	int p = 0;
        p += snprintf(&header[0], 4096, 
			"Type: float\nDimensions: %lu\n", data.size());

        for (std::size_t n = 0; n < data.size(); n++) {
                p += snprintf(&header[0] + p, 4096 - p, 
			"[%ld\t%ld\t%ld\t%ld]\n", data[n].start, data[n].end, 
			data[n].size, data[n].stride);
	}
	return header;
}

inline bounds coo_parse_header(const std::array<char, 4096>& header)
{
	bounds b;
	unsigned int dims = 0;
	char name[10];
	int pos = 0;
	int c = 0;

	sscanf(&header[pos], "Type: %9s\n%n", name, &c);
	pos += c;
	sscanf(&header[pos], "Dimensions: %d\n%n", &dims, &c);
	pos += c;
	for(unsigned int i = 0; i < dims; i++)
	{
		unsigned long size = 0;
		long stride = 0;
		unsigned long start = 0;
		unsigned long end = 0;
		sscanf(&header[pos], "[%ld %ld %ld %ld]\n%n",
			&start, &end, &size, &stride, &c);
		if (i>0) {
			b.push_back(size);
		}
		pos += c;
		assert(pos <= 4096);
	}

	// squeeze higher dimensions
	for (unsigned int i=b.size()-1; i>=0; i--) {
		if (b[i] == 1) {
			(void)b.pop_back();	
		} else {
			break;	
		}
	}
	return b;
}

template <typename T>
struct coo_cardinality 
{
    typedef T type;
    static const int cardinality = 1;
};

template <typename T>
struct coo_cardinality<std::complex<T> > {
    typedef T type;
    static const int cardinality = 2;
};

template <typename InputIt, typename Bounds>
void coo_write(const InputIt first, const Bounds& b, const char* filename)
{
	// build a coo index object
	std::vector<coo_index> ci(16, coo_index());
	ci[0].size = coo_cardinality<typename InputIt::value_type>::cardinality;
	for (std::size_t s=0; s<b.size(); s++) {
		ci[s+1].size = b[s];
	}
	fill_stride_end(ci);

	// create the header
	auto header = coo_build_header(ci);

	// open the file
	std::ofstream stream(filename, std::ios::out|std::ios::binary);
	// tell the stream to throw on problems, like a non-existing file.
	stream.exceptions ( std::ifstream::failbit | std::ifstream::badbit );
	
	// write the header
	stream.write(&header[0], header.size());

	// write the data
	stream.write((char*)std::addressof(*first), 
			sizeof(
				typename std::iterator_traits<InputIt>
				::value_type
				) * 
			product(b));
}

// coo_write for standard aura device_arrays
template <typename T>
void coo_write(const boost::aura::device_array<T> &gpuData, const char* filename, feed &f)
{
    // get data dimension
    aura::bounds b = gpuData.get_bounds();

    // create temporary cpu array
    std::vector<T> cpuData(b);

    // copy data to cpu
    boost::aura::copy(gpuData, cpuData, f);
    boost::aura::wait_for(f);

    // call standard coo_write
    coo_write(cpuData.begin(), b, filename);
}

// more generic coo_write, accepting device ranges
template <typename DeviceRangeType>
void coo_write(const DeviceRangeType &gpuData, const char* filename, feed &f)
{
    // get data dimension
    aura::bounds b = gpuData.get_bounds();

    // create temporary cpu array
    std::vector<typename DeviceRangeType::value_type> cpuData(b);

    // copy data to cpu
    boost::aura::copy(gpuData, cpuData, f);
    boost::aura::wait_for(f);

    // call standard coo_write
    coo_write(cpuData.begin(), b, filename);
}


template <typename T>
std::tuple<std::vector<T>, bounds> coo_read(const char* filename)
{
	// open file
	std::ifstream stream(filename, std::ios::in|std::ios::binary);
	// tell the stream to throw on problems, like a non-existing file.
	stream.exceptions ( std::ifstream::failbit | std::ifstream::badbit );

	// create output
	std::tuple<std::vector<T>, bounds> r;

	// read header
	std::array<char, 4096> header;
	stream.read(&header[0], header.size());

	// parse header
	std::get<1>(r) = coo_parse_header(header);
	// resize output
	std::get<0>(r).resize(product(std::get<1>(r)));
	// read data
	stream.read((char*)(std::get<0>(r).data()), 
			std::get<0>(r).size()*sizeof(T));
    return r;
}

// coo read into device array
template <typename T>
std::tuple<boost::aura::device_array<T>, bounds> coo_read(const char* filename, aura::device &d, feed &f)
{
    // load coo to host memory
    boost::aura::bounds b;
    std::vector<T> data;
    std::tie(data, b) = coo_read<T>(filename);

    // create output
    std::tuple<aura::device_array<T>, bounds> r;
    std::get<0>(r) = aura::device_array<T>(b,d);
    std::get<1>(r) = b;

    //copy data to device
    aura::copy(data, std::get<0>(r),f);
    wait_for(f);

    return(r);
}


} // namespace aura
} // boost

#endif // AURA_MISC_COO_HPP

