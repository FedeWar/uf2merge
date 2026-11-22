/*
 *
 */
// Project
// C++
#include <cstdint>
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <cstring>
#include <cassert>

const uint32_t HEADER_SZ = 0x20;
const uint32_t FLASH_BEGIN = 0x10000000;

enum flags_t
{
	NOT_MAIN_FLASH	= 0x0001,
	FILE_CONTAINER	= 0x1000,
	FAMILY_ID		= 0x2000,
	MD5_CHECKSUM	= 0x4000,
	EXTENSION_TAGS	= 0x8000
};

struct block
{
	flags_t flags;
	uint32_t address;
	union
	{
		uint32_t family_id;
		uint32_t file_size;
	};
	std::vector<uint8_t> data; // Max 476
};

typedef std::vector<block> uf2_t;

void parse_header(block& b, const std::array<uint8_t, HEADER_SZ> header_buf)
{
	uint32_t magic1 = *reinterpret_cast<const uint32_t*>(&(header_buf[0]));
	assert(magic1 == 0x0a324655);

	uint32_t magic2 = *reinterpret_cast<const uint32_t*>(&(header_buf[4]));
	assert(magic2 == 0x9e5d5157);

	b.flags = (flags_t)(*reinterpret_cast<const uint32_t*>(&(header_buf[8])));
	assert(b.flags == FAMILY_ID);

	b.address = *reinterpret_cast<const uint32_t*>(&(header_buf[12]));
	assert(b.address >= FLASH_BEGIN && b.address < 0x20000000);
	std::cout << "Target Address 0x" << b.address << '\n';

	uint32_t payload_size = *reinterpret_cast<const uint32_t*>(&(header_buf[16]));
	assert(payload_size > 0 && payload_size < 476);
	std::cout << "Payload Size 0x" << payload_size << " bytes\n";
	b.data.resize(payload_size);

	uint32_t block_num = *reinterpret_cast<const uint32_t*>(&(header_buf[20]));
	std::cout << "Block Number " << std::dec << block_num << '\n';

	uint32_t total_blocks = *reinterpret_cast<const uint32_t*>(&(header_buf[24]));
	std::cout << "Total Blocks " << total_blocks << std::hex << '\n';

	b.family_id = *reinterpret_cast<const uint32_t*>(&(header_buf[28]));
	assert(b.family_id == 0xe48bff56);
}

std::vector<block> parse_uf2(std::string filename)
{
	std::fstream fstrm(filename, std::ios::binary | std::ios::in);
	if(!fstrm.is_open())
	{
		std::cerr << "Could not open \"" << filename << "\" file. Ignoring it.\n";
		return {};
	}

	std::vector<block> uf2;

	while(!fstrm.eof())
	{
		block b;

		std::array<uint8_t, HEADER_SZ> header_buf;
		fstrm.read((char*) header_buf.data(), HEADER_SZ);
//		std::cout << "Gcount " << fstrm.gcount() << '\n';
		if(fstrm.eof())
			break;

		parse_header(b, header_buf);

		// Read payload
		fstrm.read((char*) b.data.data(), b.data.size());

		// Print payload
//		std::cout << "Payload\n";
//		for(size_t i = 0; i < b.data.size(); i++)
//		{
//			if(i > 0 && i % 0x10 == 0)
//				std::cout << '\n';
//			if(b.data[i] < 0x10)
//				std::cout << '0';
//			std::cout << (uint32_t) b.data[i] << ' ';
//		}
//		std::cout << std::endl;

		// Discard padding
		fstrm.ignore(512 - 32 - 4 - b.data.size());

		// Read final magic number
		uint32_t magic3;
		fstrm.read((char*) &magic3, 4);
		assert(magic3 == 0x0ab16f30);

		uf2.push_back(b);
	}

	fstrm.close();

	return uf2;
}

bool is_empty_block(block b)
{
	for(auto c : b.data)
	{
		if(c != 0)
			return false;
	}
	return true;
}

uf2_t merge(const std::vector<uf2_t> files)
{
	uf2_t m ;

	for(auto&& it : files)
	{
		for(auto&& b : it)
		{
			// The block under evaluation is empty, skip
			if(is_empty_block(b))
			{
				continue;
			}

			// Check if block is already present
			uint32_t add = b.address;
			bool push = true;
			for(auto&& i : m)
			{
				if(i.address == add)
				{
					if(is_empty_block(i))
					{
						i = b;
						push = false;
					}
					else
					{
						// Both blocks are not empty
						assert(false);
					}
					break;
				}
			}

			if(push)
			{
				m.push_back(b);
			}
		}
	}

	return m;
}

void write_uf2(const std::string filename, uf2_t f)
{
	std::fstream fstrm(filename, std::ios::binary | std::ios::out);

	uint32_t magic1 = 0x0a324655;
	uint32_t magic2 = 0x9e5d5157;
	uint32_t magic3 = 0x0ab16f30;
	uint32_t total_blocks = f.size();
	uint32_t block_number = 0;

	for(auto&& b : f)
	{
		fstrm.write(reinterpret_cast<char*>(&magic1), 4);
		fstrm.write(reinterpret_cast<char*>(&magic2), 4);

		uint32_t flags = b.flags;
		fstrm.write(reinterpret_cast<char*>(&flags), 4);

		fstrm.write(reinterpret_cast<char*>(&b.address), 4);

		uint32_t payload_size = b.data.size();
		fstrm.write(reinterpret_cast<char*>(&payload_size), 4);

		fstrm.write(reinterpret_cast<char*>(&block_number), 4);
		fstrm.write(reinterpret_cast<char*>(&total_blocks), 4);
		fstrm.write(reinterpret_cast<char*>(&b.family_id), 4);
		fstrm.write(reinterpret_cast<char*>(b.data.data()), b.data.size());

		// Write all 0s as padding
		size_t padding = 512 - 32 - 4 - b.data.size();
		for(size_t i = 0; i < padding; i++)
			fstrm.put(0);

		fstrm.write(reinterpret_cast<char*>(&magic3), 4);

		block_number++;
	}

	fstrm.close();
}

int main(int argc, char* argv[])
{
	std::cout << std::hex;
	std::vector<uf2_t> input_files;
	std::string output_file;

	for(int i = 1; i < argc; i++)
	{
		if(strcmp(argv[i], "-i") == 0)
		{
			input_files.push_back(parse_uf2(argv[++i]));
		}
		else if(strcmp(argv[i], "-o") == 0)
		{
			output_file = argv[++i];
		}
		else
		{
			std::cerr << "Program argument \"" << argv[i] << "\" not recognised.";
			return -1;
		}
	}

	if(input_files.size() == 0)
	{
		std::cerr << "No input file provided." << std::endl;
		return -1;
	}

	if(output_file.length() == 0)
	{
		std::cerr << "No output file provided." << std::endl;
		return -1;
	}

	auto merged = merge(input_files);
	write_uf2(output_file, merged);

	return 0;
}
