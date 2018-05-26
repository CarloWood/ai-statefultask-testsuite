#include "sys.h"
#include "cwds/gnuplot_tools.h"

int main()
{
  eda::FrequencyCounter<int> fc;

  while (1)
  {
    fc.print();
    int input;
    std::cin >> input;
    fc.add(input);
  }
}
