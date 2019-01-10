/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions used in all programs, helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <algorithm>
#include <cstring>
#ifdef SYS_WINDOWS
# include <windows.h>
#endif

#include "common/command_line.h"
#include "common/hacks.h"
#include "common/json.h"
#include "common/mm_io_x.h"
#include "common/mm_file_io.h"
#include "common/mm_proxy_io.h"
#include "common/mm_text_io.h"
#include "common/mm_write_buffer_io.h"
#include "common/strings/editing.h"
#include "common/strings/utf8.h"
#include "common/translation.h"
#include "common/version.h"

namespace mtx { namespace cli {

bool g_gui_mode = false;

static void
read_args_from_json_file(std::vector<std::string> &args,
                         std::string const &filename) {
  std::string buffer;

  try {
    auto io = std::make_shared<mm_text_io_c>(std::make_shared<mm_file_io_c>(filename));
    io->read(buffer, io->get_size());

  } catch (mtx::mm_io::exception &ex) {
    mxerror(fmt::format(Y("The file '{0}' could not be opened for reading: {1}.\n"), filename, ex));
  }

  try {
    auto doc       = mtx::json::parse(buffer);
    auto skip_next = false;

    if (!doc.is_array())
      throw std::domain_error{Y("JSON option files must contain a JSON array consisting solely of JSON strings")};

    for (auto const &val : doc) {
      if (!val.is_string())
        throw std::domain_error{Y("JSON option files must contain a JSON array consisting solely of JSON strings")};

      if (skip_next) {
        skip_next = false;
        continue;
      }

      auto string = val.get<std::string>();
      if (string == "--command-line-charset")
        skip_next = true;

      else
        args.push_back(string);
    }

  } catch (std::exception const &ex) {
    mxerror(fmt::format("The JSON option file '{0}' contains an error: {1}.\n", filename, ex.what()));
  }
}

/** \brief Expand the command line parameters

   Takes each command line paramter, converts it to UTF-8, and reads more
   commands from command files if the argument starts with '@'. Puts all
   arguments into a new array.
   On Windows it uses the \c GetCommandLineW() function. That way it can
   also handle multi-byte input like Japanese file names.

   \param argc The number of arguments. This is the same argument that
     \c main normally receives.
   \param argv The arguments themselves. This is the same argument that
     \c main normally receives.
   \return An array of strings converted to UTF-8 containing all the
     command line arguments and any arguments read from option files.
*/
#if !defined(SYS_WINDOWS)
std::vector<std::string>
args_in_utf8(int argc,
             char **argv) {
  int i;
  std::vector<std::string> args;

  charset_converter_cptr cc_command_line = g_cc_stdio;

  for (i = 1; i < argc; i++)
    if (argv[i][0] == '@')
      read_args_from_json_file(args, &argv[i][1]);
    else {
      if (!strcmp(argv[i], "--command-line-charset")) {
        if ((i + 1) == argc)
          mxerror(Y("'--command-line-charset' is missing its argument.\n"));
        cc_command_line = charset_converter_c::init(!argv[i + 1] ? "" : argv[i + 1]);
        i++;
      } else
        args.push_back(cc_command_line->utf8(argv[i]));
    }

  return args;
}

#else  // !defined(SYS_WINDOWS)

std::vector<std::string>
args_in_utf8(int,
             char **) {
  std::vector<std::string> args;
  std::string utf8;

  int num_args     = 0;
  LPWSTR *arg_list = CommandLineToArgvW(GetCommandLineW(), &num_args);

  if (!arg_list)
    return args;

  int i;
  for (i = 1; i < num_args; i++) {
    auto arg = to_utf8(std::wstring{arg_list[i]});

    if (arg[0] == '@')
      read_args_from_json_file(args, arg.substr(1));

    else
      args.push_back(arg);
  }

  LocalFree(arg_list);

  return args;
}
#endif // !defined(SYS_WINDOWS)

std::string g_usage_text, g_version_info;

/** Handle command line arguments common to all programs

   Iterates over the list of command line arguments and handles the ones
   that are common to all programs. These include --output-charset,
   --redirect-output, --help, --version and --verbose along with their
   short counterparts.

   \param args A vector of strings containing the command line arguments.
     The ones that have been handled are removed from the vector.
   \param redirect_output_short The name of the short option that is
     recognized for --redirect-output. If left empty then no short
     version is accepted.
   \returns \c true if the locale has changed and the function should be
     called again and \c false otherwise.
*/
bool
handle_common_args(std::vector<std::string> &args,
                   const std::string &redirect_output_short) {
  size_t i = 0;

  while (args.size() > i) {
    if (args[i] == "--debug") {
      if ((i + 1) == args.size())
        mxerror("Missing argument for '--debug'.\n");

      debugging_c::request(args[i + 1]);
      args.erase(args.begin() + i, args.begin() + i + 2);

    } else if (args[i] == "--engage") {
      if ((i + 1) == args.size())
        mxerror(Y("'--engage' lacks its argument.\n"));

      mtx::hacks::engage(args[i + 1]);
      args.erase(args.begin() + i, args.begin() + i + 2);

    } else if (args[i] == "--gui-mode") {
      g_gui_mode = true;
      args.erase(args.begin() + i, args.begin() + i + 1);

    } else if (args[i] == "--flush-on-close") {
      mm_file_io_c::enable_flushing_on_close(true);
      args.erase(args.begin() + i, args.begin() + i + 1);

    } else
      ++i;
  }

  // First see if there's an output charset given.
  i = 0;
  while (args.size() > i) {
    if (args[i] == "--output-charset") {
      if ((i + 1) == args.size())
        mxerror(Y("Missing argument for '--output-charset'.\n"));
      set_cc_stdio(args[i + 1]);
      args.erase(args.begin() + i, args.begin() + i + 2);
    } else
      ++i;
  }

  // Now let's see if the user wants the output redirected.
  i = 0;
  while (args.size() > i) {
    if ((args[i] == "--redirect-output") || (args[i] == "-r") ||
        ((redirect_output_short != "") &&
         (args[i] == redirect_output_short))) {
      if ((i + 1) == args.size())
        mxerror(fmt::format(Y("'{0}' is missing the file name.\n"), args[i]));
      try {
        if (!stdio_redirected()) {
          mm_io_cptr file = mm_write_buffer_io_c::open(args[i + 1], 128 * 1024);
          file->write_bom(g_stdio_charset);
          redirect_stdio(file);
        }
        args.erase(args.begin() + i, args.begin() + i + 2);
      } catch(mtx::mm_io::exception &) {
        mxerror(fmt::format(Y("Could not open the file '{0}' for directing the output.\n"), args[i + 1]));
      }
    } else
      ++i;
  }

  // Check for the translations to use (if any).
  i = 0;
  while (args.size() > i) {
    if (args[i] == "--ui-language") {
      if ((i + 1) == args.size())
        mxerror(Y("Missing argument for '--ui-language'.\n"));

      if (args[i + 1] == "list") {
        mxinfo(Y("Available translations:\n"));
        auto translation = translation_c::ms_available_translations.begin(), end = translation_c::ms_available_translations.end();
        while (translation != end) {
          mxinfo(fmt::format("  {0} ({1})\n", translation->get_locale(), translation->m_english_name));
          ++translation;
        }
        mxexit();
      }

      if (-1 == translation_c::look_up_translation(args[i + 1]))
        mxerror(fmt::format(Y("There is no translation available for '{0}'.\n"), args[i + 1]));

      init_locales(args[i + 1]);

      args.erase(args.begin() + i, args.begin() + i + 2);

      return true;
    } else
      ++i;
  }

  // Last find the --help, --version, --check-for-updates arguments.
  i = 0;
  while (args.size() > i) {
    if ((args[i] == "-V") || (args[i] == "--version")) {
      mxinfo(fmt::format("{0}\n", g_version_info));
      mxexit();

    } else if ((args[i] == "-v") || (args[i] == "--verbose")) {
      ++verbose;
      args.erase(args.begin() + i, args.begin() + i + 1);

    } else if ((args[i] == "-q") || (args[i] == "--quiet")) {
      verbose         = 0;
      g_suppress_info = true;
      args.erase(args.begin() + i, args.begin() + i + 1);

    } else if ((args[i] == "-h") || (args[i] == "-?") || (args[i] == "--help"))
      display_usage();

    else
      ++i;
  }

  return false;
}

void
display_usage(int exit_code) {
  mxinfo(fmt::format("{0}\n", g_usage_text));
  mxexit(exit_code);
}

}}
