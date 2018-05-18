#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include "gnuplot-iostream/gnuplot-iostream.h"
#include "debug.h"

struct Plot
{
  Gnuplot gp;
  std::string m_title;
  std::string m_xlabel;
  std::string m_ylabel;
  std::string m_header;
  std::mutex m_mutex;
  std::map<std::string, std::vector<boost::tuple<double, double, double>>> m_map;
  std::vector<std::string> m_cmds;
  double m_x_min;
  double m_x_max;
  double m_y_min;
  double m_y_max;

  Plot(std::string title, std::string xlabel, std::string ylabel) :
    m_title(title), m_xlabel(xlabel), m_ylabel(ylabel), m_x_min(0.0), m_x_max(0.0), m_y_min(0.0), m_y_max(0.0) { }
  void set_xrange(double x_min, double x_max) { m_x_min = x_min; m_x_max = x_max; }
  void set_yrange(double y_min, double y_max) { m_y_min = y_min; m_y_max = y_max; }
  void set_header(std::string header) { m_header = header; }
  bool has_data() const { return !m_map.empty(); }
  void add(std::string const& str) { m_cmds.push_back(str); }
  size_t points(std::string key) const { auto iter = m_map.find(key); return iter == m_map.end() ? 0 : iter->second.size(); }

  void add_data_point(double x, double y, double dy, std::string const& description)
  {
    std::unique_lock<std::mutex> lk(m_mutex);
    m_map[description].emplace_back(x, y, dy); 
  }

  void show(std::string with = "")
  {
    Dout(dc::notice|flush_cf|continued_cf, "Generating graph... ");
    gp << "set title '" << m_title << "' font \"helvetica,12\"\n";
    gp << "set xlabel '" << m_xlabel << "'\n";
    gp << "set ylabel '" << m_ylabel << "'\n";
    if (m_x_max > 0.0)
      gp << "set xrange [" << m_x_min << ":" << m_x_max << "]\n";
    else
      gp << "set xrange [" << m_x_min << ":]\n";
    if (m_y_max > 0.0)
      gp << "set yrange [" << m_y_min << ":" << m_y_max << "]\n";
    else
      gp << "set yrange [" << m_y_min << ":]\n";
    for (auto&& s : m_cmds)
      gp << s << '\n';
    char const* separator = "plot ";
    for (auto&& e : m_map)
    {
      gp << separator << "'-'";
      if (!m_header.empty())
        gp << ' ' << m_header;
      if (!with.empty())
        gp << " with " << with;
      gp << " title '" << e.first << "'";
      separator = ", ";
    }
    gp << '\n';
    for (auto&& e : m_map)
      gp.send1d(e.second);
    Dout(dc::finish|flush_cf, "done");
  }
};
