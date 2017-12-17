/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   IO callback class definitions

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modifications by Nils Maier <maierman@web.de>
*/

#pragma once

#include "common/common_pch.h"

#include "common/mm_io.h"

class mm_write_buffer_io_c: public mm_proxy_io_c {
protected:
  memory_cptr m_af_buffer;
  unsigned char *m_buffer;
  size_t m_fill;
  const size_t m_size;
  debugging_option_c m_debug_seek, m_debug_write;

public:
  mm_write_buffer_io_c(mm_io_cptr const &out, size_t buffer_size);
  virtual ~mm_write_buffer_io_c();

  virtual uint64 getFilePointer();
  virtual void setFilePointer(int64 offset, seek_mode mode = seek_beginning);
  virtual void flush();
  virtual void close();
  virtual void discard_buffer();

  static mm_io_cptr open(const std::string &file_name, size_t buffer_size);

protected:
  virtual uint32 _read(void *buffer, size_t size);
  virtual size_t _write(const void *buffer, size_t size);
  virtual void flush_buffer();
};
