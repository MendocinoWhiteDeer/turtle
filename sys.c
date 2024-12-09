#include "turtle.h"

pid_t frk()
{
  pid_t pid = fork();
  if (pid == -1) panic("frk() failed");
  return pid;
}
