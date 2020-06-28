def create_iso3166_country_list_file
  cpp_file_name = "src/common/iso3166_country_list.cpp"
  iso3166_1     = JSON.parse(IO.readlines("/usr/share/iso-codes/json/iso_3166-1.json").join(''))
  rows          = iso3166_1["3166-1"].
    map do |entry|
    [ entry["alpha_2"].upcase.to_cpp_string,
      entry["alpha_3"].upcase.to_cpp_string,
      sprintf('%03s', entry["numeric"].gsub(%r{^0+}, '')),
      entry["name"].to_u8_cpp_string,
      (entry["official_name"] || '').to_u8_cpp_string,
    ]
  end
  header = <<EOT
/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   ISO 3166 countries

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

// ------------------------------------------------------------------------
// NOTE: this file is auto-generated by the "dev:iso3166_list" rake target.
// ------------------------------------------------------------------------

#include "common/common_pch.h"

#include "common/iso3166.h"

namespace mtx::iso3166 {

std::vector<country_t> const g_countries{
EOT

  footer = <<EOT
};

} // namespace mtx::iso3166
EOT

  content = header + format_table(rows.sort, :column_suffix => ',', :row_prefix => "  { ", :row_suffix => "  },").join("\n") + "\n" + footer

  runq("write", cpp_file_name) { IO.write("#{$source_dir}/#{cpp_file_name}", content); 0 }
end
