#include "sys.h"
#include "debug.h"
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;
namespace po = boost::program_options;

namespace options {
std::string module_name;
std::string module_desc;
} // namespace options

template<typename IOSTREAM>
void open(IOSTREAM& file, fs::path const& name)
{
  file.open(name);
  if (!file)
    throw std::system_error(errno, std::system_category(), "Failed to open \"" + name.native() + "\"");
}

void write(std::ofstream& file, fs::path const& name, std::string const& line)
{
  file << line << '\n';
  if (!file)
    throw std::system_error(errno, std::system_category(), "Failed to write to \"" + name.native() + "\"");
}

std::string trim_right(std::string const& str)
{
  std::string const pattern = " \f\n\r\t\v";
  return str.substr(0, str.find_last_not_of(pattern) + 1);
}

class Header
{
 private:
  std::string m_doxygen_start;
  std::string m_doxygen_lead;
  std::string m_doxygen_end;
  std::string m_module_name;
  std::string m_module_desc;
  std::string m_old_module_name;
  std::string m_brief;
  std::vector<int> m_copyright_years;
  std::string m_author;
  std::vector<std::string> m_signature;
  std::vector<std::string> m_license;
  std::vector<std::string> m_trailing;

 public:
  Header(std::string const& module_name, std::string const& module_desc) :
    m_doxygen_start("/**"), m_doxygen_lead(" * "), m_doxygen_end(" */"),
    m_module_name(module_name), m_module_desc(module_desc)
  {
    DoutEntering(dc::notice, "Header(\"" << module_name << "\", \"" << module_desc << "\")");
  }

  void add_current_year()
  {
    std::time_t t = std::time(0);
    std::tm* now = std::localtime(&t);
    m_copyright_years.push_back(now->tm_year + 1900);
  }

  void set_module(std::string const& name, std::string const& desc)
  {
    DoutEntering(dc::notice, "set_module(\"" << name << "\", \"" << desc << "\")");
    if (m_module_name.empty())
    {
      m_module_name = name;
      m_module_desc = desc;
    }
  }

  void set_brief(std::string const& desc)
  {
    DoutEntering(dc::notice, "Header::set_brief(\"" << desc << "\")");
    m_brief = desc;
  }

  void decode_copyright_line(std::string const& line);

  void add_copyright_year(int year)
  {
//    DoutEntering(dc::notice, "add_copyright_year(" << year << ")");
    m_copyright_years.push_back(year);
  }

  void set_author(std::string const& author)
  {
    DoutEntering(dc::notice, "set_author(\"" << author << "\")");
    m_author = author;
  }

  void add_signature_line(std::string const& line)
  {
    m_signature.push_back(line);
  }

  void set_part_of(std::string const& text, std::string const& module_name)
  {
    if (m_module_name.empty())
    {
      m_module_name = module_name;
      Dout(dc::notice, "m_module_name set to \"" << m_module_name << "\".");
    }
    else if (m_module_name != module_name)
    {
      Dout(dc::warning, "Found module name \"" << m_module_name << "\" is unequal name in \"" << text << "\".");
      m_old_module_name = module_name;
      Dout(dc::notice, "m_old_module_name set to \"" << module_name << "\".");
    }
  }

  void add_license_line(std::string const& line)
  {
    m_license.push_back(line);
  }

  void add_trailing_line(std::string const& line)
  {
    m_trailing.push_back(line);
  }

  void canonicalize();

  void print_line_to(std::ostream& os, std::string line = std::string()) const;

  void print_on(std::ostream& os) const;
  friend std::ostream& operator<<(std::ostream& os, Header const& header)
  {
    header.print_on(os);
    return os;
  }
};

void Header::decode_copyright_line(std::string const& line)
{
  DoutEntering(dc::notice, "Header::decode_copyright_line(\"" << line << "\")");

  // Strip off any spaces and/or periods at the end.
  std::string const pattern = " .";
  std::string str = line.substr(0, line.find_last_not_of(pattern) + 1);

  std::regex const copyright_regexp(R"(.*[Cc]opyright\s+[^\d]*((?:(?:19|20)[\d][\d](?:\s*[,-]\s*|\s+))*)(.*))");
  std::smatch sm;
  if (std::regex_match(line, sm, copyright_regexp))
  {
    Dout(dc::notice|continued_cf, "Adding copyright year(s):");
    if (sm.size() > 1)
    {
      std::regex const year_regexp(R"(((?:19|20)[\d][\d])(?:\s*-\s*((?:19|20)[\d][\d]))?)");
      auto years = sm[1].str();
      auto iter = std::sregex_token_iterator(years.begin(), years.end(), year_regexp, {1, 2});
      auto end = std::sregex_token_iterator();
      while (iter != end)
      {
        int range_begin = std::stoi(*iter++);
        std::string range_end_str = *iter++;
        int range_end = range_end_str.empty() ? range_begin : std::stoi(range_end_str);
        for (int year = range_begin; year <= range_end; ++year)
        {
          Dout(dc::continued, ' ' << year);
          add_copyright_year(year);
        }
      }
    }
    Dout(dc::finish, "");
    if (sm.size() > 2)
      set_author(sm[2].str().substr(0, sm[2].str().find_last_not_of(" .") + 1));
  }
}

void Header::canonicalize()
{
  if (m_module_desc.empty())
    m_module_desc = "FIXME";
  if (m_brief.empty())
    m_brief = "FIXME";
  if (m_copyright_years.empty())
    add_current_year();
  else
  {
    std::sort(m_copyright_years.begin(), m_copyright_years.end());
    m_copyright_years.erase(std::unique(m_copyright_years.begin(), m_copyright_years.end() ), m_copyright_years.end());
  }
  if (m_author.empty())
    m_author = "Carlo Wood";
  if (m_signature.empty())
  {
    m_signature.push_back("RSA-1024 0x624ACAD5 1997-01-26                    Sign & Encrypt");
    m_signature.push_back("Fingerprint16 = 32 EC A7 B6 AC DB 65 A6  F6 F6 55 DD 1C DC FF 61");
  }
  if (m_license.empty())
  {
    m_license.push_back("This program is free software: you can redistribute it and/or modify");
    m_license.push_back("it under the terms of the GNU Affero General Public License as published");
    m_license.push_back("by the Free Software Foundation, either version 3 of the License, or");
    m_license.push_back("(at your option) any later version.");
    m_license.push_back("");
    m_license.push_back("This program is distributed in the hope that it will be useful,");
    m_license.push_back("but WITHOUT ANY WARRANTY; without even the implied warranty of");
    m_license.push_back("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the");
    m_license.push_back("GNU Affero General Public License for more details.");
    m_license.push_back("");
    m_license.push_back("You should have received a copy of the GNU Affero General Public License");
    m_license.push_back("along with this program.  If not, see <http://www.gnu.org/licenses/>.");
  }
  if (!m_module_name.empty())
  {
    std::string capitalized_module_name = m_module_name;
    capitalized_module_name[0] = std::toupper(m_module_name[0]);

    std::regex const This_program_regexp(R"(This program)");
    std::regex const this_program_regexp(R"(this program)");
    std::regex Old_module_name_regexp, old_module_name_regexp;
    if (!m_old_module_name.empty())
    {
      old_module_name_regexp = m_old_module_name;
      std::string Old_module_name_regexp_str = "^[Xx]" + m_old_module_name.substr(1);
      Old_module_name_regexp_str[2] = std::toupper(m_old_module_name[0]);
      Old_module_name_regexp_str[3] = std::tolower(m_old_module_name[0]);
      Old_module_name_regexp = Old_module_name_regexp_str;
    }

    for (auto& line : m_license)
    {
      line = std::regex_replace(line, This_program_regexp, capitalized_module_name);
      line = std::regex_replace(line, this_program_regexp, m_module_name);
      if (!m_old_module_name.empty())
      {
        line = std::regex_replace(line, Old_module_name_regexp, capitalized_module_name);
        line = std::regex_replace(line, old_module_name_regexp, m_module_name);
      }
    }
  }
}

void Header::print_line_to(std::ostream& os, std::string line) const
{
  if (line.empty())
    os << trim_right(m_doxygen_lead);
  else
    os << m_doxygen_lead << line;
  os << '\n';
}

void Header::print_on(std::ostream& os) const
{
  if (!m_doxygen_start.empty())
    os << m_doxygen_start << '\n';
  if (!m_module_name.empty())
  {
    os << m_doxygen_lead << m_module_name << " -- " << m_module_desc << '\n';
    print_line_to(os);
  }
  os << m_doxygen_lead << "@file\n";
  os << m_doxygen_lead << "@brief " << m_brief << '\n';
  print_line_to(os);
  os << m_doxygen_lead;
  char const* separator = "@Copyright (C) ";
  int first_year = 0, last_year = 0;
  int final_year = m_copyright_years.back();
  for (int year : m_copyright_years)
  {
    if (year == last_year + 1)
    {
      last_year = year;
      if (year != final_year)
        continue;
    }
    if (first_year || year == final_year)
    {
      if (!first_year)
        first_year = year;
      os << separator << first_year;
      separator = (last_year > first_year + 1) ? " - " : ", ";
      if (last_year > first_year)
        os << separator << last_year;
      separator = ", ";
      if (year == final_year && year > std::max(first_year, last_year))
        os << separator << year;
    }
    first_year = last_year = year;
  }
  os << "  " << m_author << ".\n";
  print_line_to(os);
  for (auto& line : m_signature)
    print_line_to(os, line);
  if (!m_module_name.empty())
  {
    print_line_to(os);
    os << m_doxygen_lead << "This file is part of " << m_module_name << ".\n";
  }
  print_line_to(os);
  for (auto line : m_license)
    print_line_to(os, line);
  if (!m_trailing.empty())
  {
    print_line_to(os);
    for (auto& line : m_trailing)
      print_line_to(os, line);
  }
  if (!m_doxygen_end.empty())
    os << m_doxygen_end << '\n';
}

bool is_C_comment_start(std::string left_trimmed_line)
{
//  DoutEntering(dc::notice|continued_cf, "is_C_comment_start(\"" << left_trimmed_line << "\") = ");
  bool result = left_trimmed_line.size() > 1 && left_trimmed_line[0] == '/' && left_trimmed_line[1] == '*';
//  Dout(dc::finish, result);
  return result;
}

bool is_C_comment_end(std::string const& right_trimmed_line)
{
//  DoutEntering(dc::notice|continued_cf, "is_C_comment_end(\"" << right_trimmed_line << "\") = ");
  int sz = right_trimmed_line.size();
  bool result = sz > 1 && right_trimmed_line[sz - 2] == '*' && right_trimmed_line[sz - 1] == '/';
//  Dout(dc::finish, result);
  return result;
}

bool is_Cpp_comment(std::string const& left_trimmed_line)
{
//  DoutEntering(dc::notice|continued_cf, "is_Cpp_comment(\"" << left_trimmed_line << "\" = ");
  bool result = left_trimmed_line.size() > 1 && left_trimmed_line[0] == '/' && left_trimmed_line[1] == '/';
//  Dout(dc::finish, result);
  return result;
}

std::vector<std::string> resplit(std::string const& s, std::string rgx_str = "\\s+")
{
  std::vector<std::string> elems;

  std::regex rgx(rgx_str);

  std::sregex_token_iterator iter(s.begin(), s.end(), rgx, -1);
  std::sregex_token_iterator end;

  while (iter != end)
  {
    elems.push_back(*iter);
    ++iter;
  }

  return elems;
}

std::string extract(std::string lead_reg_expr, std::string line)
{
//  DoutEntering(dc::notice, "extract(\"" << lead_reg_expr << "\", \"" << line << "\")");
  std::regex reg_expr(lead_reg_expr + R"((.*))");
  std::smatch sm;
  if (std::regex_match(line, sm, reg_expr))
  {
#if 0
    Dout(dc::notice, "sm.size() = " << sm.size());
    for (int i = 0; i < sm.size(); ++i)
      Dout(dc::notice, "\"" << sm[i].str() << "\"");
#endif
    ASSERT(sm.size() >= 2);
    return trim_right(sm[1].str());
  }
  return "";
}

bool process(std::vector<std::string>& lines)
{
  std::vector<std::string> const input_lines(std::move(lines));
  lines.clear();

  int il = 0;
  int const el = input_lines.size();
  enum State { searching, module, file, brief, copyright, signature, part_of, license, trailing };
  State state = searching;
  Header header(options::module_name, options::module_desc);
  std::regex const module_regexp(R"((\S+)\s+--\s+(.*))");
  std::regex const file_regexp(R"([@\\]file(?:\s+|$)(.*))");
  std::regex const brief_regexp(R"([@\\]brief\s+(.*))");
  std::regex const copyright_regexp(R"([Cc]opyright .*Carlo Wood)");
  std::regex const part_of_regexp(R"(This file is part of\s+([A-Za-z0-9_-]*))");
  std::regex const license_start_regexp(R"(free software)");
  std::regex const license_end_regexp(R"(www\.gnu\.org)");
  for (;;)
  {
    // Skip empty lines.
    while (il < el && input_lines[il].empty())
    {
      Dout(dc::notice, "Skipping empty line " << il);
      ++il;
    }
    bool first_line = true;
    bool saw_file_or_brief_or_copyright = false;
    bool saw_copyright = false;
    bool saw_license = false;
    bool saw_module = false;
    bool trailing_empty_line = false;
    bool is_C_comment = is_C_comment_start(boost::trim_left_copy(input_lines[il]));
    std::string lead_reg_expr = R"(^\s*/)";       // Zero or more leading spaces followed by a '/'.
    lead_reg_expr += is_C_comment ? '*' : '/';    // Followed by a '*' or another '/'.
    lead_reg_expr += R"([-+*/!=#%^]* ?(.*))";     // Followed by zero or more non-space characters, followed by at most one space.
    while (is_C_comment || is_Cpp_comment(boost::trim_left_copy(input_lines[il])))
    {
      std::string text = extract(lead_reg_expr, input_lines[il]);
      bool last_line = is_C_comment && is_C_comment_end(trim_right(input_lines[il]));
      if (is_C_comment && last_line)
        text = extract(R"(^[\s]*(.*[^\s])[\s]*\*/)", text);       // The appended reg expr in extract will match nothing and is discarded.
      text = trim_right(text);
//      std::cout << "state = " << state << "; TEXT: \"" << text << "\"\n";
      std::smatch sm;
      if (state == file)
      {
        state = searching;
        if (std::regex_match(text, sm, brief_regexp))
          state = brief;
        else
          Dout(dc::warning, "@file is not immediately followed by @brief.");
      }
      if (state == searching)
      {
        if (first_line && std::regex_match(text, sm, module_regexp))
          state = module;
        else if (std::regex_match(text, sm, file_regexp))
          state = file;
        else if (std::regex_search(text, sm, copyright_regexp))
          state = copyright;
        else if (!saw_license && std::regex_search(text, sm, part_of_regexp))
          state = part_of;
        else if (!saw_license && std::regex_search(text, sm, license_start_regexp))
          state = license;
        else if (saw_copyright && !saw_license && !text.empty())
          state = signature;
        else if (saw_license && !text.empty())
          state = trailing;
      }
      switch (state)
      {
        case searching:
        {
          // We found nothing. Discard this line.
          Dout(dc::warning(!text.empty()), "Throwing away: \"" << text << "\"");
          break;
        }
        case module:
        {
          header.set_module(sm[1].str(), sm[2].str());
          state = searching;
          saw_module = true;
          break;
        }
        case file:
        {
          Dout(dc::warning(!sm[1].str().empty()), "@file with trailing characters!");
          saw_file_or_brief_or_copyright = true;
          break;
        }
        case brief:
        {
          header.set_brief(sm[1].str());
          state = searching;
          saw_file_or_brief_or_copyright = true;
          break;
        }
        case copyright:
        {
          header.decode_copyright_line(text);
          state = searching;
          saw_file_or_brief_or_copyright = saw_copyright = true;
          break;
        }
        case signature:
        {
          header.add_signature_line(text);
          state = searching;
          break;
        }
        case part_of:
        {
          header.set_part_of(text, sm[1].str());
          state = searching;
          break;
        }
        case license:
        {
          header.add_license_line(text);
          saw_license = true;
          if (std::regex_search(text, sm, license_end_regexp))
            state = searching;
          break;
        }
        case trailing:
        {
          if (text.empty())
            trailing_empty_line = true;
          else
          {
            if (trailing_empty_line)
              header.add_trailing_line("");
            header.add_trailing_line(text);
            trailing_empty_line = false;
          }
          break;
        }
        first_line = false;
      }
      ++il;
      if (last_line)
        break;
      if (is_C_comment)
        lead_reg_expr = R"(^\s*\*+ ?(.*))";       // Zero or more leading spaces followed by one or more '*'s, followed by at most one space.
    }
    if (!saw_file_or_brief_or_copyright && saw_module)
    {
      saw_file_or_brief_or_copyright = true;      // Only do this once.
      continue;
    }
    break;
  }
  // Skip empty lines.
  while (il < el && input_lines[il].empty())
  {
    Dout(dc::notice, "Skipping empty line " << il);
    ++il;
  }

  header.canonicalize();
  std::stringstream ss;
  ss << header << '\n';

  std::string str;
  while (std::getline(ss, str))
    lines.push_back(str);

  while (il < el)
    lines.push_back(input_lines[il++]);

  return input_lines != lines;
}

void option_dependency(po::variables_map const& vm, char const* for_what, char const* required_option)
{
  if (vm.count(for_what) && !vm[for_what].defaulted())
    if (vm.count(required_option) == 0 || vm[required_option].defaulted())
      throw std::logic_error(std::string("Option '") + for_what + "' requires option '" + required_option + "'.");
}

int main(int argc, char* argv[])
{
  Debug(NAMESPACE_DEBUG::init());

  po::options_description options_description("Program options");
  po::positional_options_description positional_options_description;

  options_description.add_options()
    ("help,h",                                                     "print usage message")
    ("module-name,n",   po::value<std::string>(),                  "name of the git submodule")
    ("module-desc,d",   po::value<std::string>(),                  "description of the git submodule")
    ("input-file",      po::value<std::vector<std::string>>(),     "")
  ;
  positional_options_description.add("input-file", -1);

  po::variables_map variables_map;
  po::store(po::command_line_parser(argc, argv)
        .options(options_description)
        .positional(positional_options_description)
        .allow_unregistered()
        .run(),
      variables_map);

  if (variables_map.count("help"))
  {
    std::cout << options_description << "\n";
    return 0;
  }

  option_dependency(variables_map, "module-name", "module-desc");
  option_dependency(variables_map, "module-desc", "module-name");

  std::vector<std::string> non_options;
  bool wrong_number_of_non_options = variables_map.count("input-file") == 0;
  if (!wrong_number_of_non_options)
  {
    non_options = variables_map["input-file"].as<std::vector<std::string>>();
    wrong_number_of_non_options = non_options.size() != 1;
  }
  if (wrong_number_of_non_options)
  {
    std::cerr << "Usage: " << argv[0] << " [options] <input file>\n";
    return 1;
  }
  if (variables_map.count("module-name"))
  {
    options::module_name = variables_map["module-name"].as<std::string>();
    options::module_desc = variables_map["module-desc"].as<std::string>();
  }

  fs::path const input_file_name = non_options[0];

  try
  {
    // Read input file.
    std::vector<std::string> lines;
    {
      std::ifstream file;
      open(file, input_file_name);

      std::string str;
      while (std::getline(file, str))
        lines.push_back(str);
    }
    Dout(dc::notice, "Read " << input_file_name << ": " << lines.size() << " lines.");

    if (process(lines))
    {
      // Write output file.
      fs::path output_file_name = input_file_name;
      output_file_name += ".new";

      {
        std::ofstream ofile;
        open(ofile, output_file_name);

        for (auto& line : lines)
          write(ofile, output_file_name, line);
      }

      Dout(dc::notice, "Renaming " << output_file_name << " to " << input_file_name);
      fs::rename(output_file_name, input_file_name);
    }
  }
  catch (std::system_error const& error)
  {
    std::cerr << error.what() << std::endl;
    return 1;
  }
}
