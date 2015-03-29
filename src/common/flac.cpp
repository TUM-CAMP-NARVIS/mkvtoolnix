/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   FLAC helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "config.h"

#if defined(HAVE_FLAC_FORMAT_H)

#include "common/common_pch.h"

#include <FLAC/stream_decoder.h>
#include <stdarg.h>

#include "common/bit_cursor.h"
#include "common/flac.h"
#include "common/mm_io_x.h"

namespace mtx { namespace flac {

static FLAC__StreamDecoderReadStatus
flac_read_cb(const FLAC__StreamDecoder *,
             FLAC__byte buffer[],
             size_t *bytes,
             void *client_data) {
  return reinterpret_cast<decoder_c *>(client_data)->flac_read_cb(buffer, bytes);
}

static FLAC__StreamDecoderWriteStatus
flac_write_cb(const FLAC__StreamDecoder *,
              const FLAC__Frame *frame,
              const FLAC__int32 * const data[],
              void *client_data) {
  return reinterpret_cast<decoder_c *>(client_data)->flac_write_cb(frame, data);
}

static void
flac_metadata_cb(const FLAC__StreamDecoder *,
                 const FLAC__StreamMetadata *metadata,
                 void *client_data) {
  reinterpret_cast<decoder_c *>(client_data)->flac_metadata_cb(metadata);
}

static void
flac_error_cb(const FLAC__StreamDecoder *,
              FLAC__StreamDecoderErrorStatus status,
              void *client_data) {
  reinterpret_cast<decoder_c *>(client_data)->flac_error_cb(status);
}

static FLAC__StreamDecoderSeekStatus
flac_seek_cb(const FLAC__StreamDecoder *,
             FLAC__uint64 absolute_byte_offset,
             void *client_data) {
  return reinterpret_cast<decoder_c *>(client_data)->flac_seek_cb(absolute_byte_offset);
}

static FLAC__StreamDecoderTellStatus
flac_tell_cb(const FLAC__StreamDecoder *,
             FLAC__uint64 *absolute_byte_offset,
             void *client_data) {
  return reinterpret_cast<decoder_c *>(client_data)->flac_tell_cb(*absolute_byte_offset);
}

static FLAC__StreamDecoderLengthStatus
flac_length_cb(const FLAC__StreamDecoder *,
               FLAC__uint64 *stream_length,
               void *client_data) {
  return reinterpret_cast<decoder_c *>(client_data)->flac_length_cb(*stream_length);
}

static FLAC__bool
flac_eof_cb(const FLAC__StreamDecoder *,
            void *client_data) {
  return reinterpret_cast<decoder_c *>(client_data)->flac_eof_cb();
}

// ----------------------------------------------------------------------

decoder_c::decoder_c() {
}

decoder_c::~decoder_c() {
}

void
decoder_c::flac_metadata_cb(const FLAC__StreamMetadata *) {
}

void
decoder_c::flac_error_cb(FLAC__StreamDecoderErrorStatus) {
}

FLAC__StreamDecoderSeekStatus
decoder_c::flac_seek_cb(uint64_t) {
  return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__StreamDecoderTellStatus
decoder_c::flac_tell_cb(uint64_t &) {
  return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
}

FLAC__StreamDecoderLengthStatus
decoder_c::flac_length_cb(uint64_t &) {
  return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
}

FLAC__bool
decoder_c::flac_eof_cb() {
  return false;
}

FLAC__StreamDecoderReadStatus
decoder_c::flac_read_cb(FLAC__byte [],
                        size_t *) {
  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderWriteStatus
decoder_c::flac_write_cb(FLAC__Frame const *,
                         FLAC__int32 const * const[]) {
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void
decoder_c::init_flac_decoder() {
  m_flac_decoder.reset(FLAC__stream_decoder_new());
  if (!m_flac_decoder)
    mxerror(Y("flac_decoder: FLAC__stream_decoder_new() failed.\n"));

  if (!FLAC__stream_decoder_set_metadata_respond_all(m_flac_decoder.get()))
    mxerror(Y("flac_decoder: Could not set metadata_respond_all.\n"));

  if (FLAC__stream_decoder_init_stream(m_flac_decoder.get(), mtx::flac::flac_read_cb, mtx::flac::flac_seek_cb, mtx::flac::flac_tell_cb, mtx::flac::flac_length_cb, mtx::flac::flac_eof_cb,
                                       mtx::flac::flac_write_cb, mtx::flac::flac_metadata_cb, mtx::flac::flac_error_cb, this) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
    mxerror(Y("flac_decoder: Could not initialize the FLAC decoder.\n"));
}

// ----------------------------------------------------------------------

static bool
skip_utf8(bit_reader_c &bits,
          int size) {

  uint32_t value = bits.get_bits(8);

  int num;
  if (!(value & 0x80))          /* 0xxxxxxx */
    num = 0;
  else if ((value & 0xC0) && !(value & 0x20)) /* 110xxxxx */
    num = 1;
  else if ((value & 0xE0) && !(value & 0x10)) /* 1110xxxx */
    num = 2;
  else if ((value & 0xF0) && !(value & 0x08)) /* 11110xxx */
    num = 3;
  else if ((value & 0xF8) && !(value & 0x04)) /* 111110xx */
    num = 4;
  else if ((value & 0xFC) && !(value & 0x02)) /* 1111110x */
    num = 5;
  else if ((size == 64) && (value & 0xFE) && !(value & 0x01)) /* 11111110 */
    num = 6;
  else
    return false;

  bits.skip_bits(num * 8);

  return true;
}

// See http://flac.sourceforge.net/format.html#frame_header
static int
get_num_samples_internal(unsigned char const *mem,
                         int size,
                         FLAC__StreamMetadata_StreamInfo const &stream_info) {
  bit_reader_c bits(mem, size);

  // Sync word: 11 1111 1111 1110
  if (bits.peek_bits(14) != 0x3ffe)
    return -1;

  bits.skip_bits(14);

  // Reserved
  bits.skip_bits(2);

  // Block size
  uint32_t value       = bits.get_bits(4);
  int free_sample_size = 0;
  int samples          = 0;

  if (0 == value)
    samples = stream_info.min_blocksize;
  else if (1 == value)
    samples = 192;
  else if ((2 <= value) && (5 >= value))
    samples = 576 << (value - 2);
  else if (8 <= value)
    samples = 256 << (value - 8);
  else
    free_sample_size = value;

  // Sample rate
  bits.skip_bits(4);

  // Channel assignment
  bits.skip_bits(4);

  // Sample size (3 bits) and zero bit padding (1 bit)
  bits.skip_bits(4);

  if (stream_info.min_blocksize != stream_info.max_blocksize) {
    if (!skip_utf8(bits, 64))
      return -1;

  } else if (!skip_utf8(bits, 32))
      return -1;

  if ((6 == free_sample_size) || (7 == free_sample_size)) {
    samples = bits.get_bits(8);
    if (7 == free_sample_size) {
      samples <<= 8;
      samples  |= bits.get_bits(8);
    }
    samples++;
  }

  // CRC
  bits.skip_bits(8);

  return samples;
}

int
get_num_samples(unsigned char const *mem,
                int size,
                FLAC__StreamMetadata_StreamInfo const &stream_info) {
  try {
    return get_num_samples_internal(mem, size, stream_info);
  } catch(...) {
    return -1;
  }
}

#define FPFX "flac_decode_headers: "

struct header_extractor_t {
  unsigned char const *mem;
  unsigned int size;
  unsigned int nread;

  FLAC__StreamMetadata_StreamInfo stream_info;
  bool stream_info_found;
};

static FLAC__StreamDecoderReadStatus
read_cb(FLAC__StreamDecoder const *,
        FLAC__byte buffer[],
        size_t *bytes,
        void *client_data) {
  auto fhe = (header_extractor_t *)client_data;

  if (fhe->nread == fhe->size)
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

  size_t num_bytes = *bytes > (fhe->size - fhe->nread) ? (fhe->size - fhe->nread) : *bytes;
  memcpy(buffer, &fhe->mem[fhe->nread], num_bytes);
  fhe->nread += num_bytes;
  *bytes      = num_bytes;

  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static void
metadata_cb(FLAC__StreamDecoder const *,
            FLAC__StreamMetadata const *metadata,
            void *client_data) {
  auto fhe = (header_extractor_t *)client_data;
  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
    memcpy(&fhe->stream_info, &metadata->data.stream_info, sizeof(FLAC__StreamMetadata_StreamInfo));
    fhe->stream_info_found = true;
  }
}

static FLAC__StreamDecoderWriteStatus
write_cb(FLAC__StreamDecoder const *,
         FLAC__Frame const *,
         FLAC__int32 const * const [],
         void *) {
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void
error_cb(FLAC__StreamDecoder const *,
         FLAC__StreamDecoderErrorStatus,
         void *) {
}

int
decode_headers(unsigned char const *mem,
               int size,
               int num_elements,
               ...) {
  header_extractor_t fhe;
  FLAC__StreamDecoder *decoder;
  int result, i;
  va_list ap;

  if (!mem || (0 >= size))
    return -1;

  memset(&fhe, 0, sizeof(header_extractor_t));
  fhe.mem  = mem;
  fhe.size = size;

  decoder  = FLAC__stream_decoder_new();

  if (!decoder)
    mxerror(FPFX "FLAC__stream_decoder_new() failed.\n");
  if (!FLAC__stream_decoder_set_metadata_respond_all(decoder))
    mxerror(FPFX "Could not set metadata_respond_all.\n");
  if (FLAC__stream_decoder_init_stream(decoder, read_cb, nullptr, nullptr, nullptr, nullptr, write_cb, metadata_cb, error_cb, &fhe) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
    mxerror(FPFX "Could not initialize the FLAC decoder.\n");

  FLAC__stream_decoder_process_until_end_of_stream(decoder);

  result = 0;
  if (fhe.stream_info_found)
    result |= FLAC_HEADER_STREAM_INFO;

  va_start(ap, num_elements);
  for (i = 0; i < num_elements; ++i) {
    int type;

    type = va_arg(ap, int);
    switch (type) {
      case FLAC_HEADER_STREAM_INFO: {
        FLAC__StreamMetadata_StreamInfo *stream_info;

        stream_info = va_arg(ap, FLAC__StreamMetadata_StreamInfo *);
        if (result & FLAC_HEADER_STREAM_INFO)
          memcpy(stream_info, &fhe.stream_info, sizeof(FLAC__StreamMetadata_StreamInfo));

        break;
      }

      default:
        type = va_arg(ap, int);
    }
  }

  FLAC__stream_decoder_delete(decoder);

  return result;
}

}}                              // namespace mtx::flac

#endif
