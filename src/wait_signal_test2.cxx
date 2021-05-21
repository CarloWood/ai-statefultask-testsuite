#include "sys.h"
#include "utils/MultiLoop.h"
#include <exception>
#include <iostream>
#include <bitset>
#include <array>
#include <cassert>
#include <sstream>
#include <fstream>
#include <map>
#include <set>

struct NotAllowed : std::exception
{
};

struct Counter
{
  int m_value;

  Counter() : m_value(1) { }

  bool signal(bool is_idle)
  {
    // It should be impossible to receive a signal while we're not running
    // for a condition that we aren't waiting for. That is, such signal would
    // be ignored and screws up the operation reordering test.
    if (is_idle && m_value != 0)
      throw NotAllowed();
    if (m_value == 2)
      return false;
    ++m_value;
    return m_value == 1;
  }

  bool wait()
  {
    // It is impossible to receive a wait() while we're not running.
    if (m_value == 0)
      throw NotAllowed();
    --m_value;
    return m_value == 1;
  }

  void reset(int v)
  {
    m_value = v;
  }

  bool is_idle() const
  {
    return m_value == 0;
  }

  void print_on(std::ostream& os) const
  {
    os << m_value;
  }

  friend std::ostream& operator<<(std::ostream& os, Counter const& counter)
  {
    counter.print_on(os);
    return os;
  }
};

using counter_index_type = int;

bool is_required(counter_index_type i)
{
  // Index
  // 0  : normal condition 0
  // 1  : normal condition 1
  // 2  : required condition 0
  // 3  : required condition 1
  return i > 1;
}

// Lets encode the state of all four counters into a single int:
// counters[0] + 3 * counters[1] + 9 * counters[2] + 27 * counters[3].
using state_type = int;

// Simulate four counters.
struct Task
{
  static constexpr int OR = 0;
  static constexpr int AND = 1;
  static constexpr int WAIT = 0;
  static constexpr int SIGNAL = 1;

  std::array<Counter, 4> m_counters;

  Task() = default;
  Task(Task const& task) : m_counters{task.m_counters} { }

  void set_state(state_type st)
  {
    int p = 1;
    for (int i = 0; i < 4; ++i)
    {
      m_counters[i].m_value = (st / p) % 3;
      p *= 3;
    }
  }

  state_type state() const
  {
    return m_counters[0].m_value + 3 * m_counters[1].m_value + 9 * m_counters[2].m_value + 27 * m_counters[3].m_value;
  }

  bool wait(int mask)
  {
    bool OR_counter_woke_up = false;
    for (counter_index_type i = 0; i < 4; ++i)
      if ((mask & (1 << i)) && m_counters[i].wait() && !is_required(i))
        OR_counter_woke_up = true;
    return OR_counter_woke_up;
  }

  bool signal(int mask, bool is_idle)
  {
    bool OR_counter_woke_up = false;
    for (counter_index_type i = 0; i < 4; ++i)
      if ((mask & (1 << i)) && m_counters[i].signal(is_idle) && !is_required(i))
        OR_counter_woke_up = true;
    return OR_counter_woke_up;
  }

  void reset()
  {
    for (counter_index_type i = 0; i < 4; ++i)
      m_counters[i].reset(1);
  }

  void reset_OR_counters()
  {
    for (counter_index_type i = 0; i < 4; ++i)
      if (!is_required(i) && m_counters[i].is_idle())
        m_counters[i].reset(1);
  }

  bool is_waiting(bool OR_counter_woke_up)
  {
    bool OR_counter_is_waiting = false;
    bool AND_counter_is_waiting = false;
    for (counter_index_type i = 0; i < 4; ++i)
    {
      if (m_counters[i].m_value == 0)
      {
        if (!is_required(i))
          OR_counter_is_waiting = true;
        else
          AND_counter_is_waiting = true;
      }
    }
    return AND_counter_is_waiting || (OR_counter_is_waiting && !OR_counter_woke_up);
  }

  bool required_is_idle() const
  {
    bool required_is_waiting = false;
    for (counter_index_type i = 2; i < 4; ++i)
    {
      if (m_counters[i].m_value == 0)
        required_is_waiting = true;
    }
    return required_is_waiting;
  }

  bool has_idle_counter() const
  {
    bool counter_is_waiting = false;
    for (counter_index_type i = 0; i < 4; ++i)
    {
      if (m_counters[i].m_value == 0)
        counter_is_waiting = true;
    }
    return counter_is_waiting;
  }

  bool print_on(std::ostream& os)
  {
    bool is_waiting = false;
    for (counter_index_type i = 3; i >= 0; --i)
    {
      int val = m_counters[i].m_value;
      if (i != 3)
        os << ' ';
      os << val;
      if (val == 0)
        is_waiting = true;
    }
    return is_waiting;
  }
};

std::string operation_str(int operation)
{
  int mask = operation & 15;
  std::string result = (operation & 16) ? "signal" : "wait";
  result += '(';
  for (int bit = 8; bit > 0; bit >>= 1)
    result += (mask & bit) ? '1' : '0';
  result += ')';
  return result;
}

int main()
{
  Task counters;

  std::array<std::array<int, 32>, 81> graph;
  int const not_allowed_magic = 81;
  for (state_type s = 0; s < 81; ++s)
    for (int operation = 0; operation < 32; ++operation)
      graph[s][operation] = not_allowed_magic;

  for (state_type s = 0; s < 81; ++s)
  {
    counters.set_state(s);

    std::cout << "\nInitial state: ";
    std::ostringstream node1;
    bool initial_state_is_waiting = counters.print_on(node1);
    std::cout << node1.str() << (initial_state_is_waiting ? " (waiting)\n" : "\n");

    std::string name1 = node1.str();
    std::string label;

    int operation = 16;  // signal
    for (int mask = 1; mask <= 15; ++mask)
    {
      std::ostringstream node2;
      std::ostringstream ss;
      ss << "signal(" << std::bitset<4>(mask) << ")";
      label = ss.str();
      std::cout << label << " : ";
      Task counters2{counters};
      try
      {
        bool OR_counter_woke_up = counters2.signal(mask, counters2.has_idle_counter());
        bool woken_up = initial_state_is_waiting && !counters2.required_is_idle() && (OR_counter_woke_up || !counters2.has_idle_counter());
        if (woken_up)
          counters2.reset();
        else if (OR_counter_woke_up)
          counters2.reset_OR_counters();
        graph[s][operation + mask] = counters2.state();
        counters2.print_on(node2);
        std::string name2 = node2.str();
        std::cout << name2 << " --> " << std::boolalpha << woken_up << ' ';
        if (counters2.has_idle_counter() && !woken_up)
          std::cout << "(waiting)";
        else
          std::cout << "(running)";
        std::cout << std::endl;
      }
      catch (NotAllowed const&)
      {
        graph[s][operation + mask] = not_allowed_magic;
        std::cout << "disallowed\n";
      }
    }

    operation = 0;      // wait
    for (int mask = 1; mask <= 15; ++mask)
    {
      std::ostringstream node2;
      std::ostringstream ss;
      ss << "wait(" << std::bitset<4>(mask) << ")";
      label = ss.str();
      std::cout << label << " : ";
      Task counters2{counters};
      try
      {
        bool OR_counter_woke_up = counters2.wait(mask);
        bool woken_up = !counters2.required_is_idle() && OR_counter_woke_up;
        if (woken_up)
          counters2.reset();
        else if (OR_counter_woke_up)
          counters2.reset_OR_counters();
        graph[s][operation + mask] = counters2.state();
        counters2.print_on(node2);
        std::string name2 = node2.str();
        std::cout << name2;
        if (counters2.has_idle_counter())
          std::cout << " (waiting)";
        else
          std::cout << " (running)";
        std::cout << std::endl;
      }
      catch (NotAllowed const&)
      {
        graph[s][operation + mask] = not_allowed_magic;
        std::cout << "disallowed\n";
      }
    }
  }

  Task task;
  using result_map_type = std::map<std::multiset<int>, std::pair<std::string, bool>>;
  result_map_type result_map;
  for (int number_of_operations = 1; number_of_operations <= 4; ++number_of_operations)
  {
    for (MultiLoop ml(number_of_operations); !ml.finished(); ml.next_loop())
    {
      while (ml() < 32)
      {
        if ((ml() & 15) == 0)
        {
          ml.breaks(0);
          break;
        }
        if (ml.inner_loop())    // Most inner loop.
        {
          bool not_allowed = false;
          task.set_state(40);
          std::ostringstream ss;
          task.print_on(ss);
          bool running = true;
          std::multiset<int> operations;
          for (int op = 0; op < number_of_operations; ++op)
          {
            int operation = ml[op];
            operations.insert(operation);
            ss << " --" << operation_str(operation) << "-> ";
            int next_state = graph[task.state()][operation];
            if (next_state == not_allowed_magic || ((operation & 16) == 0 && !running))
            {
              ss << " not allowed";
              not_allowed = true;
              break;
            }
            task.set_state(next_state);
            task.print_on(ss);
            running = !task.has_idle_counter();
            if ((operation & 16) == 0 && running)
            {
              ss << " --(running)-> ";
              task.set_state(40);
              task.print_on(ss);
            }
          }
          if (!not_allowed)
          {
            if (running)
              ss << " (running)";
            else
              ss << " (waiting)";
          }
          if (not_allowed)
          {
            ml.breaks(0);
            break;
          }
          //std::cout << ss.str() << std::endl;
          auto res = result_map.insert(result_map_type::value_type{operations, {ss.str(), running}});
          if (!res.second && res.first->second.second != running)
          {
            std::cout << "Difference between:\n\t" << ss.str() << "\nand\t" << res.first->second.first << std::endl;
          }
        }
        ml.start_next_loop_at(1);
      }
    }
  }
}
