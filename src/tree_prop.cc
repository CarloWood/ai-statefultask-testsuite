#include <iostream>
#include <array>
#include <algorithm>
#include <cassert>

// Return the parent tree index of i.
int p(int i)
{
  assert(0 < i && i < 5);
  return (i + 1) / 2 - 1;
}

// Return the sibbling tree index of i.
int s(int i)
{
  assert(i == 1 || i == 3 || i == 4);
  return (i == 1) ? 2 : 7 - i;
}

int interval_to_parent_index(int in)
{
  assert(in == 1);
  return 3;
}

bool is_sane(std::array<int, 5> const& cache, int const* t)
{
  if (t[4] != 3)
    std::cout << "t[4] isn't 3!" << std::endl;
  if (t[2] != 4)
    std::cout << "t[2] isn't 4!" << std::endl;
  if (!(t[3] == 1    || t[3] == 2))
    std::cout << "t[3] isn't 1 or 2!" << std::endl;
  if (!(t[1] == t[3] || t[1] == t[4]))
    std::cout << "t[1] isn't t[3] (" << t[3] << ") or t[4] (" << t[4] << ")!" << std::endl;
  if (!(t[0] == t[1] || t[0] == t[2]))
    std::cout << "t[0] isn't t[1] (" << t[1] << ") or t[2] (" << t[2] << ")!" << std::endl;
  if (std::min(cache[   1], cache[   2]) != cache[t[3]])
    std::cout << "t[3] (" << t[3] << ") points to the wrong value (" << cache[t[3]] << ")! Should be " << std::min(cache[   1], cache[   2]) << "." << std::endl;
  if (std::min(cache[t[3]], cache[t[4]]) != cache[t[1]])
    std::cout << "t[1] (" << t[1] << ") points to the wrong value (" << cache[t[1]] << ")! Should be " << std::min(cache[t[3]], cache[t[4]]) << "." << std::endl;
  if (std::min(cache[t[1]], cache[t[2]]) != cache[t[0]])
    std::cout << "t[0] (" << t[0] << ") points to the wrong value (" << cache[t[0]] << ")! Should be " << std::min(cache[t[1]], cache[t[2]]) << "." << std::endl;

  return t[4] == 3 && t[2] == 4 &&
         (t[3] == 1    || t[3] == 2)    && std::min(cache[   1], cache[   2]) == cache[t[3]] &&
         (t[1] == t[3] || t[1] == t[4]) && std::min(cache[t[3]], cache[t[4]]) == cache[t[1]] &&
         (t[0] == t[1] || t[0] == t[2]) && std::min(cache[t[1]], cache[t[2]]) == cache[t[0]];
}

int main()
{
  // Situation:
  //
  // old_v
  //   |
  //   v
  // new_v  cache[2]  cache[3] cache[4]
  //   \      /       /       /
  //    \    /       /       /
  //     t[3]     t[4]      /
  //       \      /        /
  //        \    /        /
  //         t[1]     t[2]
  //           \      /
  //            \    /
  //             t[0]
  //
  // old_v is changed into new_v.

  // Every possibility is tested by looping over all permutations of the following values:
  std::array<int, 5> cache;
  for (int old_v = 100; old_v <= 500; old_v += 100)
  {
    for (int new_v = 100; new_v <= 500; new_v += 100)
    {
      if (new_v == old_v)
        continue;
      for (cache[2] = 100; cache[2] <= 500; cache[2] += 100)
      {
        for (cache[3] = 100; cache[3] <= 500; cache[3] += 100)
        {
          for (cache[4] = 100; cache[4] <= 500; cache[4] += 100)
          {
            int t[5];

            t[4] = 3;
            t[2] = 4;

            cache[1] = old_v;

            // Let t3 point to the smallest value of cache[1] and cache[2].
            for (int t3 = 1; t3 <= 2; ++t3)
            {
              if (cache[t3] > std::min(cache[1], cache[2]))
                continue;

              // Let t1 point to the smallest value of cache[t3] and cache[t[4]].
              assert(t[4] > t3);
              for (int t1 = t3; t1 <= t[4]; t1 += (t[4] - t3))
              {
                if (cache[t1] > std::min(cache[t3], cache[t[4]]))
                  continue;

                // Let t0 point to the smallest value of cache[t1] and cache[t[2]].
                assert(t[2] > t1);
                for (int t0 = t1; t0 <= t[2]; t0 += (t[2] - t1))
                {
                  if (cache[t0] > std::min(cache[t1], cache[t[2]]))
                    continue;

                  t[0] = t0;
                  t[1] = t1;
                  t[3] = t3;

                  assert(new_v != old_v);
                  assert(is_sane(cache, t));

#if 0
                  std::cout << "\nnew_v = " << new_v;
                  for (int c = 1; c < 5; ++c)
                    std::cout << "; cache[" << c << "] = " << cache[c];
                  for (int c = 0; c < 5; ++c)
                    std::cout << "; t[" << c << "] = " << t[c];
                  std::cout << std::endl;
#endif

                  // Prepare variables.
                  int interval = 1;                                             // interval is the interval that changed.
                  // Change to new value;
                  cache[interval] = new_v;                                      // Here it does.

                  if (new_v < old_v)                                            // The value can either become less or become greater.
                  {
                    // Execute the algorithm for cache value becoming less.
                    int nv = cache[interval];                                   // Let 'nv' be the new value.
                    int i = interval_to_parent_index(interval);                 // Let 'i' be the index of the parent node in the tree above 'interval'.
                    while (nv <= cache[t[i]])                                   // t[i] is the content of that node. cache[t[i]] is the value that is represents.
                    {
                      t[i] = interval;                                          // Update that tree node.
                      if (i == 0)                                               // If this was the top-most node in the tree then we're done.
                        break;
                      i = p(i);                                                 // Set 'i' to be the index of the parent node in the tree above 'i'.
                    }
                  }
                  else
                  {
                    // Execute the algorithm for cache value becoming greater.
                    int nv = cache[interval];                                   // Let 'nv' be the new value.
                    int i = interval_to_parent_index(interval);                 // Let 'i' be the index of the parent node in the tree above 'interval'.

                    int in = interval;                                          // Let 'in' be the interval whose value is changed with respect to t[i].
                    int si = 3 - in;                                    // Let 'si' be the interval of the current sibling of in.
                    for(;;)
                    {
                      int sv = cache[si];
                      bool stays_smaller = nv <= sv;
                      if (!stays_smaller)
                      {
                        if (t[i] == si)
                          break;
                        nv = sv;
                        in = si;
                      }
                      t[i] = in;                                                // Update the tree.
                      if (i == 0)                                               // If this was the top-most node in the tree then we're done.
                        break;
                      si = t[s(i)];                                     // Update the sibling interval.
                      i = p(i);                                                 // Set 'i' to be the index of the parent node in the tree above 'i'.
                    }
                  }

                  // Test the result.
                  if (!is_sane(cache, t))
                  {
                    std::cout << "Failure." << std::endl;
                    return 0;
                  }

                  // Continue test.
                  cache[1] = old_v;
                }
              }
            }
          }
        }
      }
    }
  }
  std::cout << "Success!" << std::endl;
}
