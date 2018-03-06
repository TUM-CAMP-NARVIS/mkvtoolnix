/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions and helper functions for AC-3 data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#pragma once

#include "common/common_pch.h"

#include "common/byte_buffer.h"

#define AC3_SYNC_WORD           0x0b77

#define AC3_CHANNEL                  0
#define AC3_MONO                     1
#define AC3_STEREO                   2
#define AC3_3F                       3
#define AC3_2F1R                     4
#define AC3_3F1R                     5
#define AC3_2F2R                     6
#define AC3_3F2R                     7
#define AC3_CHANNEL1                 8
#define AC3_CHANNEL2                 9
#define AC3_DOLBY                   10
#define AC3_CHANNEL_MASK            15

#define AC3_LFE                     16

#define EAC3_FRAME_TYPE_INDEPENDENT  0
#define EAC3_FRAME_TYPE_DEPENDENT    1
#define EAC3_FRAME_TYPE_AC3_CONVERT  2
#define EAC3_FRAME_TYPE_RESERVED     3

class codec_c;

namespace mtx {

namespace bits {
class reader_c;
}

namespace ac3 {

class frame_c {
public:
  unsigned int m_sample_rate{}, m_bit_rate{}, m_channels{}, m_flags{}, m_bytes{}, m_bs_id{}, m_samples{}, m_frame_type{}, m_sub_stream_id{};
  unsigned int m_dialog_normalization{}, m_dialog_normalization_bit_position{};
  boost::optional<unsigned int> m_dialog_normalization2, m_dialog_normalization2_bit_position;
  uint64_t m_stream_position{}, m_garbage_size{};
  bool m_valid{};
  memory_cptr m_data;
  std::vector<frame_c> m_dependent_frames;

public:
  frame_c();
  void init();
  bool is_eac3() const;
  codec_c get_codec() const;
  void add_dependent_frame(frame_c const &frame, unsigned char const *buffer, std::size_t buffer_size);
  bool decode_header(unsigned char const *buffer, std::size_t buffer_size);
  bool decode_header_type_eac3(mtx::bits::reader_c &bc);
  bool decode_header_type_ac3(mtx::bits::reader_c &bc);

  std::string to_string(bool verbose) const;

  int find_in(memory_cptr const &buffer);
  int find_in(unsigned char const *buffer, std::size_t buffer_size);
};

class parser_c {
protected:
  std::deque<frame_c> m_frames;
  mtx::bytes::buffer_c m_buffer;
  uint64_t m_parsed_stream_position, m_total_stream_position;
  frame_c m_current_frame;
  std::size_t m_garbage_size;

public:
  parser_c();
  void add_bytes(memory_cptr const &mem);
  void add_bytes(unsigned char *const buffer, std::size_t size);
  void flush();
  std::size_t frame_available() const;
  frame_c get_frame();
  uint64_t get_parsed_stream_position() const;
  uint64_t get_total_stream_position() const;

  int find_consecutive_frames(unsigned char const *buffer, std::size_t buffer_size, std::size_t num_required_headers);

  void parse(bool end_of_stream);
};

bool verify_checksum1(unsigned char const *buf, std::size_t size);
void remove_dialog_normalization(unsigned char *buf, std::size_t size);

}}                              // namespace mtx::ac3
