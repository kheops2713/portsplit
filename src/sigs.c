/*
 * sigs.c - this file is part of Portsplit.
 *
 * Portsplit is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Portsplit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Portsplit.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "sigs.h"
#include "main.h"

void set_child_ignored_signals (void)
{
  struct sigaction action;

  action.sa_handler = SIG_IGN;
  sigemptyset (&action.sa_mask);
  sigaddset (&action.sa_mask, SIGHUP);
  sigaddset (&action.sa_mask, SIGUSR1);
  sigaddset (&action.sa_mask, SIGUSR2);
  action.sa_flags = 0;
  action.sa_sigaction = SIG_IGN;

  sigaction (SIGHUP, &action, NULL);
  sigaction (SIGUSR1, &action, NULL);
  sigaction (SIGUSR2, &action, NULL);
}


void set_caugth_signals (void)
{
  struct sigaction action;
  sigset_t sigset;

  action.sa_handler = &signal_catcher;
  sigemptyset (&action.sa_mask);
  action.sa_flags = 0;

  sigaction (SIGTERM, &action, NULL);
  sigaction (SIGINT, &action, NULL);
  sigaction (SIGHUP, &action, NULL);
  sigaction (SIGCHLD, &action, NULL);
}

void signal_catcher (int sig)
{
  write (sigfd[1], &sig, sizeof(sig));
}
