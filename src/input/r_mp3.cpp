/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   MP3 reader module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/codec.h"
#include "common/error.h"
#include "common/hacks.h"
#include "common/id3.h"
#include "common/id_info.h"
#include "common/mm_io_x.h"
#include "input/r_mp3.h"
#include "merge/input_x.h"
#include "output/p_mp3.h"

#define CHUNK_SIZE 16384

int
mp3_reader_c::probe_file(mm_io_c &in,
                         uint64_t,
                         int64_t probe_range,
                         int num_headers,
                         bool require_zero_offset) {
  try {
    mtx::id3::skip_v2_tag(in);

    auto offset = find_valid_headers(in, probe_range, num_headers);
    return (require_zero_offset && (0 == offset)) || (!require_zero_offset && (0 <= offset));

  } catch (...) {
    return 0;
  }
}

mp3_reader_c::mp3_reader_c(const track_info_c &ti,
                           const mm_io_cptr &in)
  : generic_reader_c(ti, in)
  , m_chunk(memory_c::alloc(CHUNK_SIZE))
{
}

void
mp3_reader_c::read_headers() {
  try {
    auto tag_size_start = mtx::id3::skip_v2_tag(*m_in);
    auto tag_size_end   = mtx::id3::tag_present_at_end(*m_in);

    if (0 > tag_size_start)
      tag_size_start = 0;
    if (0 < tag_size_end)
      m_size -= tag_size_end;

    auto init_read_len = std::min(m_size - tag_size_start, static_cast<uint64_t>(CHUNK_SIZE));

    int pos = find_valid_headers(*m_in, init_read_len, 5);
    if (0 > pos)
      throw mtx::input::header_parsing_x();

    m_in->setFilePointer(tag_size_start + pos, seek_beginning);
    m_in->read(m_chunk->get_buffer(), 4);

    decode_mp3_header(m_chunk->get_buffer(), &m_mp3header);

    m_in->setFilePointer(tag_size_start + pos, seek_beginning);

    show_demuxer_info();

    if ((0 < pos) && verbose)
      mxwarn_fn(m_ti.m_fname, boost::format(Y("Skipping %1% bytes at the beginning (no valid MP3 header found).\n")) % pos);

    m_ti.m_id = 0;        // ID for this track.

  } catch (mtx::mm_io::exception &) {
    throw mtx::input::open_x();
  }
}

mp3_reader_c::~mp3_reader_c() {
}

void
mp3_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('a', 0) || (NPTZR() != 0))
    return;

  add_packetizer(new mp3_packetizer_c(this, m_ti, m_mp3header.sampling_frequency, m_mp3header.channels, false));
  show_packetizer_info(0, PTZR0);
}

file_status_e
mp3_reader_c::read(generic_packetizer_c *,
                   bool) {
  int nread = m_in->read(m_chunk->get_buffer(), CHUNK_SIZE);
  if (0 >= nread)
    return flush_packetizers();

  PTZR0->process(new packet_t(new memory_c(m_chunk->get_buffer(), nread, false)));

  return FILE_STATUS_MOREDATA;
}

void
mp3_reader_c::identify() {
  auto info = mtx::id::info_c{};
  info.add(mtx::id::audio_channels,           m_mp3header.channels);
  info.add(mtx::id::audio_sampling_frequency, m_mp3header.sampling_frequency);

  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_AUDIO, m_mp3header.get_codec().get_name(), info.get());
}

int
mp3_reader_c::find_valid_headers(mm_io_c &io,
                                 int64_t probe_range,
                                 int num_headers) {
  try {
    io.setFilePointer(0, seek_beginning);
    mtx::id3::skip_v2_tag(io);

    memory_cptr buf = memory_c::alloc(probe_range);
    int nread       = io.read(buf->get_buffer(), probe_range);

    // auto header = mp3_header_t{};
    // auto idx    = find_mp3_header(buf->get_buffer(), std::min(nread, 32));

    // if ((0 == idx) && decode_mp3_header(&buf->get_buffer()[idx], &header) && header.is_tag) {
    //   probe_range += header.framesize;
    //   buf->resize(probe_range);

    //   io.setFilePointer(0, seek_beginning);
    //   nread = io.read(buf->get_buffer(), probe_range);
    // }

    io.setFilePointer(0, seek_beginning);

    return find_consecutive_mp3_headers(buf->get_buffer(), nread, num_headers);
    // auto result = find_consecutive_mp3_headers(buf->get_buffer(), nread, num_headers);
    // return -1 == result ? -1 : result + idx;

  } catch (...) {
    return -1;
  }
}
