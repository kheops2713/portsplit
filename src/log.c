/*
 * log.c - this file is part of Portsplit.
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

#include <time.h>
#include "log.h"

char *now ()
{
  static char str[64];
  time_t epochsec = time(NULL);
  struct tm *proute;

  proute = localtime(&epochsec);

  strftime (str, 64, DT_FMT, proute);

  return str;
}

void now_r (char *str)
{
  time_t epochsec = time(NULL);
  struct tm *proute;

  proute = localtime(&epochsec);

  strftime (str, 64, DT_FMT, proute);
}
