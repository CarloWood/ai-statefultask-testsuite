#include <sstream>
#include <iostream>

unsigned long long fibo(int n)
{
  static unsigned long long f[10000];
  if (n < 2)
    return 1;
  if (!f[n])
  {
    if (!f[n - 1])
      f[n - 1] = fibo(n - 1);
    if (!f[n - 2])
      f[n - 2] = fibo(n - 2);
    f[n] = f[n - 1] + f[n - 2];
  }
  return f[n];
}

int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    std::cerr << "Usage: " << argv[0] << " <n> : Calculate the n-th Fibonacci number." << std::endl;
    return 1;
  }
  std::stringstream ss(argv[1]);
  int n;
  ss >> n;

  std::cout << "The " << n << "-th Fibonacci number is " << fibo(n) << std::endl;
}
