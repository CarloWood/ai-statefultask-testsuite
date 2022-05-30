#include "sys.h"
#include "debug.h"
#include <iomanip>
#include <deque>

// States
//
// C = number of threads that have a read-lock and are blocked on trying to convert that into a write-lock.
// W = number of threads that have a write-lock, or -shortly- are trying if they can get one.
// V = number of threads that are blocked on trying to get a write-lock.
// R = number of threads that have a read-lock, or -shortly- are trying if they can get one.
//
// Notation: CWVR
//
// Stable states (lowercase variables are implied to be larger than zero).
//
// 0000 = unlocked.
// 000r = r threads have a read lock.
// 00vr = v threads are waiting for a write locking, r threads have a read lock.
// 0100 = one thread has the write lock.
// 01v0 = one thread has the write lock, v threads are waiting to get a write lock.
// 100r = r threads have a read lock, 1 thread is waiting to convert their read lock into a write lock (r > 1).
// 10vr = v threads are waiting for a write locking, r threads have a read lock, 1 thread is waiting to convert their read lock into a write lock (r > 1).
//
// That means that the following states are unstable (r > 0, v > 0, w > 1, c > 1):
//
//                0000 : stable
//                000r : stable
// 00v1, 01v0 --> 00v0 --> 01(v-1)0
//                00vr : stable
//                0100 : stable
//                010r --> 
//                01v0 : stable
//                01vr
//                0w00
//                0w0r
//                0wv0
//                0wvr
//                1000
//                100r : stable
//                10v0
//                10vr : stable
//                1100
//                110r
//                11v0
//                11vr
//                1w00
//                1w0r
//                1wv0
//                1wvr
//                cxxx --> throw exception

bool print_literal = false;

using Transition = int;

// The no-op transition.
constexpr int none = 0;
// Initial transitions: these arrive immediately at the right Node if applied to the unlocked state.
constexpr int rdlock = 1;
constexpr int rdunlock = 2;
constexpr int wrlock = 3;
constexpr int wrunlock = 4;
constexpr int wr2rdlock = 5;
// This is also an initial transition, but is not stable when applied to unlocked.
constexpr int rd2wrlock = 6;
// The is to transition away from unstable states to blocking states.
constexpr int failed_rdlock = 7;                // After a rdlock we became blocking, then do this transition before blocking.
constexpr int failed_wrlock = 8;                // After a wrlock we became blocking, then do this transition before blocking.
constexpr int failed_rd2wrlock = 9;             // This is also a no-op.
constexpr int successful_rd2wrlock = 10;        // After a rd2wrlock we did not become blocking, follow up with this transition.
// Irrecoverably dead-lock.
constexpr int must_throw = 11;                  // Two or more threads are trying to convert a read lock into a write lock.

constexpr int first_transition = 1;
constexpr int last_transition = 11;
constexpr int last_non_followup_transition = rd2wrlock;

bool is_blocking(Transition follow_up)
{
  return follow_up == failed_rdlock || follow_up == failed_wrlock || follow_up == failed_rd2wrlock;
}

struct State
{
  int c;        // Waiting for read-to-write.
  int w;        // Obtained write.
  int v;        // Obtained write, or waiting for write (possibly read-to-write).
  int r;        // Obtained read.

  // Construct an unlocked read-write lock.
  State() : c(0), w(0), v(0), r(0) { }
  State(int c_, int w_, int v_, int r_) : c(c_), w(w_), v(v_), r(r_) { }

  // These states should never occur, not even shortly.
  bool is_illegal() const
  {
    return c > r || c > v ||            // A thread that waits for read-to-write (c) also keeps its read count (r) and adds a waiting-for-write count (v).
           w > v;                       // A thread that has a write count (w) also keeps it waiting-for-write count (v).
  }

  // States that are not stable (and not illegal) can occur, but only for a very short time.
  bool is_stable() const
  {
    return !(                           // Not any of...
        is_illegal() ||                 // The below expressions do not take illegal states into account - but lets be clear that those are not stable.
        w > 1 ||                        // More than one thread has the write-lock.
        c > 1 ||                        // More than one thread is trying to convert its read-lock into a write-lock.
        (w == 1 && r > 0) ||            // A thread has the write lock, while one or more threads have a read-lock.
        (v > 0 && (w == 0 && r == c))); // Threads are waiting for a write lock while nobody has a read- or write-lock.
  }

  // Return required follow-up transition depending on whether or not the last transition succeeded.
  Transition follow_up(State const& previous) const
  {
    if (c >= 2 && c > previous.c)                               // Attempting to convert read to write lock while there is already another thread trying to do that.
      return must_throw;
    if (r > previous.r && previous.v > 0)                       // Attempting to get a read-lock while there are threads waiting on or having a write lock.
      return (w < previous.w) ? none : failed_rdlock;           // Except when this was a wr2rdlock.
    if (v > previous.v && (previous.r > c || previous.w > 0))   // Attempting to get a write-lock while there are read-locks or already another write-lock.
      return (c > previous.c) ? failed_rd2wrlock : failed_wrlock;
    return (c > previous.c) ? successful_rd2wrlock : none;
  }

  bool operator==(State const& s) const
  {
    return c == s.c && w == s.w && v == s.v && r == s.r;
  }

  friend std::ostream& operator<<(std::ostream& os, State const& s)
  {
    if (print_literal)
      return os << s.c << s.w << s.v << s.r;

    if (s.c == 0)
      os << '0';
    else if (s.c == 1)
      os << '1';
    else
      os << 'c';
    if (s.w == 0)
      os << '0';
    else if (s.w == 1)
      os << '1';
    else
      os << 'w';
    if (s.v == 0)
      os << '0';
    else if (s.v == s.c)
      os << s.v;
    else
      os << 'v';
    if (s.r == 0)
      os << '0';
    else if (s.r == s.c)
      os << s.r;
    else
      os << 'r';
    return os;
  }
};

bool apply(Transition t, State& s)
{
  ASSERT(!s.is_illegal());
  switch (t)
  {
    case rdlock:
      s.r++;
      break;
    case rdunlock:
      if (s.r == 0)             // Applying rdunlock on a mutex that has no read-locks is UB.
        return false;
      s.r--;
      break;
    case wrlock:
      s.w++;
      s.v++;
      break;
    case wrunlock:
      if (s.w == 0)             // Applying wrunlock on a mutex that has no write-locks in UB.
        return false;
      s.w--;
      s.v--;
      break;
    case wr2rdlock:
      if (s.w == 0)             // Applying wr2rdlock on a mutex that no write-locks is UB.
        return false;
      s.w--;
      s.v--;
      s.r++;
      break;
    case rd2wrlock:
      if (s.r == 0)             // Applying rd2wrlock on a mutex that has no read-locks is UB.
        return false;
      s.c++;
      s.v++;
      break;
    case failed_rdlock:
      s.r--;
      break;
    case failed_wrlock:
      s.w--;
      break;
    case failed_rd2wrlock:
      break;
    case successful_rd2wrlock:
      s.w++;
      s.c--;
      s.r--;
      break;
    case must_throw:
      return false;
  }
  if (s.is_illegal())
    DoutFatal(dc::core, s << " is illegal!");
  return true;
}

char const* transition_name(Transition t)
{
  switch (t)
  {
    case none:
      return "none";
    case rdlock:
      return "rdlock";
    case rdunlock:
      return "rdunlock";
    case wrlock:
      return "wrlock";
    case wrunlock:
      return "wrunlock";
    case wr2rdlock:
      return "wr2rdlock";
    case rd2wrlock:
      return "rd2wrlock";
    case failed_rdlock:
      return "failed_rdlock";
    case failed_wrlock:
      return "failed_wrlock";
    case successful_rd2wrlock:
      return "successful_rd2wrlock";
    case failed_rd2wrlock:
      return "failed_rd2wrlock";
    case must_throw:
      return "must_throw";
  }
  AI_NEVER_REACHED
}

struct Node
{
  State const state;

  Node(State const& state_) : state(state_) { }

  std::array<Node*, last_transition + 1> transitions;
};

std::deque<Node> stable_nodes;
std::deque<Node> unstable_nodes;

Node* find_node(State const& s)
{
  ASSERT(!s.is_illegal());
  for (Node& node : stable_nodes)
    if (node.state == s)
      return &node;
  for (Node& node : unstable_nodes)
    if (node.state == s)
      return &node;
  if (s.is_stable())
  {
    stable_nodes.emplace_back(s);
    return &stable_nodes.back();
  }
  unstable_nodes.emplace_back(s);
  return &unstable_nodes.back();
}

int main()
{
  Debug(debug::init());

  // Construct stable and unstable node lists.
  Dout(dc::notice, "Generating nodes...");
  for (int c = 0; c <= 2; ++c)
    for (int w = 0; w <= 2; ++w)
      for (int v = 0; v <= c + 1; ++v)
        for (int r = 0; r <= c + 1; ++r)
        {
          State s(c, w, v, r);
          if (s.is_illegal())
            continue;
          std::cout << s;
          if (s.is_stable())
          {
            std::cout <<" : stable";
            stable_nodes.emplace_back(s);
          }
          else
            unstable_nodes.emplace_back(s);
          std::cout << '\n';
        }

  print_literal = true;

  // Apply transitions to each stable state.
  Dout(dc::notice, "Applying initial transitions to stable states...");
  for (auto& node : stable_nodes)
  {
    for (int transition = first_transition; transition <= last_non_followup_transition; ++transition)
    {
      State s = node.state;
      if (!apply(transition, s))
        continue;

      // Applying transition to node.state gives s.
      node.transitions[transition] = find_node(s);

      std::cout << node.state << " -" << std::setfill('-') << std::setw(9) << transition_name(transition) << "-> " << s;
      Transition follow_up = s.follow_up(node.state);
      if (follow_up)
      {
        if (follow_up == must_throw)
        {
          std::cout << " (must throw)\n";
          continue;
        }
        else
        {
          bool success = apply(follow_up, s);
          ASSERT(success);
          std::cout << " -" << std::setfill('-') << std::setw(9) << transition_name(follow_up) << "-> " << s;

          // Applying follow_up gives s.
          node.transitions[transition]->transitions[follow_up] = find_node(s);
        }
      }
      if (is_blocking(follow_up))
        std::cout << " (blocking)";
      if (!s.is_stable())
        std::cout << " UNSTABLE";
      std::cout << std::endl;
    }
  }
}
