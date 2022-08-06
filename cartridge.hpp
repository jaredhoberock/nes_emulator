#pragma once

#include <cstdint>
#include <fmt/format.h>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <vector>


class nrom
{
  private:
    int num_prg_banks_;

  public:
    inline nrom(int num_prg_banks)
      : num_prg_banks_{num_prg_banks}
    {}

    // this returns optional<uint16_t> because some games try to read to
    // addresses which have no physical location. this condition is called an "open bus"
    inline std::optional<std::uint16_t> map(std::uint16_t address) const
    {
      std::optional<std::uint16_t> result;

      if(address >= 0x8000)
      {
        // map incoming addresses beginning at 0x8000 to the first bye of PRG memory
        address -= 0x8000;

        if(num_prg_banks_ == 1)
        {
          // this mask does mirroring for when we have a single bank
          address &= 0x3FFF;
        }

        result = address;
      }

      return result;
    }

    inline std::uint16_t map_graphics(std::uint16_t address) const
    {
      if(address >= 0x2000)
      {
        throw std::runtime_error(fmt::format("nrom::map_graphics: Bad address: {:04X}", address));
      }

      return address;
    }
};


class cartridge
{
  public:
    enum nametable_mirroring_kind
    {
      // XXX there are others
      horizontal, vertical
    };

    // see https://www.nesdev.org/wiki/INES#iNES_file_format
    struct ines_file_header
    {
      char name[4];
      std::uint8_t num_prg_rom_chunks;
      std::uint8_t num_chr_rom_chunks;
      std::uint8_t flags_6;
      std::uint8_t flags_7;
      std::uint8_t flags_8;
      std::uint8_t flags_9;
      std::uint8_t flags_10;
      char unused[5];

      inline ines_file_header(std::istream &is)
      {
        is.read(reinterpret_cast<char*>(this), sizeof(ines_file_header));
      }

      inline int mapper_id() const
      {
        return (flags_7 & 0xF0) | (flags_6 >> 4);
      }

      inline bool trainer_present() const
      {
        return flags_6 & 0x04;
      }

      inline nametable_mirroring_kind mirroring() const
      {
        return (flags_6 & 0x01) ? vertical : horizontal;
      }
    };

    // XXX ideally, a cartridge is constructed from two arrays (prg & chr data) and a mapper
    //     so we should create a function whose job is to take a filename and return the tuple (mapper, prg_data, chr_data)

    inline cartridge(int num_prg_banks, int num_chr_banks, nametable_mirroring_kind mirroring, bool trainer_present_in_stream, std::istream& is)
      : num_prg_banks_{num_prg_banks},
        num_chr_banks_{num_chr_banks},
        nametable_mirroring_{mirroring},
        prg_memory_(num_prg_banks_ * 16384),
        chr_memory_(num_chr_banks_ * 8192),
        mapper_{num_prg_banks_}
    {
      // if a 512B trainer is present, ignore it
      if(trainer_present_in_stream)
      {
        is.seekg(512, std::ios_base::cur);
      }

      // read in PRG data
      is.read(reinterpret_cast<char*>(prg_memory_.data()), prg_memory_.size());

      // read in CHR data
      is.read(reinterpret_cast<char*>(chr_memory_.data()), chr_memory_.size());
    }

    inline cartridge(ines_file_header header, std::istream& is)
      : cartridge{header.num_prg_rom_chunks, header.num_chr_rom_chunks, header.mirroring(), header.trainer_present(), is}
    {
      if(header.mapper_id() != 0)
      {
        throw std::runtime_error(fmt::format("cartridge: ROM requires unsupported mapper {}", header.mapper_id()));
      }

      if(header.flags_6 & 0b00001000)
      {
        throw std::runtime_error("ROM requires four-screen VRAM");
      }
    }

    inline cartridge(std::istream&& is)
      : cartridge{ines_file_header{is}, is}
    {}

    inline cartridge(const std::string& filename)
      : cartridge{std::ifstream{filename.c_str(), std::ios::binary}}
    {}

    inline nametable_mirroring_kind nametable_mirroring() const
    {
      return nametable_mirroring_;
    }

    inline std::uint8_t read(std::uint16_t address) const
    {
      auto mapped_address = mapper_.map(address);

      // detect an open bus and return 0 in this case
      // see https://www.nesdev.org/wiki/Open_bus_behavior
      // XXX what we do here does not model the actual NES behavior
      // per that webpage, on an actual NES, reading an open bus repeats the
      // last value that was read on the bus before this read
      // instead, we'll just return 0
      // to model the NES behavior, we'd need some extra state in the bus class
      // to remember the previous value returned from bus::read and return that
      // in an open bus case
      return mapped_address ? prg_memory_[*mapped_address] : 0;
    }

    inline void write(std::uint16_t address, std::uint8_t value)
    {
      // XXX this hack allows us to override the reset vector while debugging
      if(address == 0xFFFC or address == 0xFFFD)
      {
        auto mapped_address = mapper_.map(address);

        if(not mapped_address)
        {
          throw std::runtime_error(fmt::format("cartridge::write: Bad address: {:04X}", address));
        }

        prg_memory_[*mapped_address] = value;
      }
      else
      {
        throw std::runtime_error(fmt::format("cartridge::write: Bad address: {:04X}", address));
      }
    }

    inline std::uint8_t graphics_read(std::uint16_t address) const
    {
      std::uint16_t mapped_address = mapper_.map_graphics(address);
      return chr_memory_[mapped_address];
    }

  private:
    // XXX these values are possibly only relevant to the mapper
    int num_prg_banks_;
    int num_chr_banks_;
    nametable_mirroring_kind nametable_mirroring_;

    std::vector<std::uint8_t> prg_memory_;
    std::vector<std::uint8_t> chr_memory_;

    nrom mapper_;
};

