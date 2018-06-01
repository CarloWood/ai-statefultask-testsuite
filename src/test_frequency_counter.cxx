#include "sys.h"
#include "cwds/gnuplot_tools.h"

int main()
{
  eda::FrequencyCounter<int, 4> fc;

  while (1)
  {
    fc.print_on(std::cout);
    int input;
    std::cin >> input;
    fc.add(input);
  }
}
