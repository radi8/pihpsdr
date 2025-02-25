/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT, 2016 - Steve Wilson, KA6S
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*
*/

#ifndef _STORE_H_
#define _STORE_H_

#include <gtk/gtk.h>
#include "bandstack.h"

/* --------------------------------------------------------------------------*/
/**
* @brief Band definition
*/
struct _MEM_STORE {
  int sat_mode;
  int split;        // this will only be used with SAT/RSAT
  int duplex;       // this will only be used with SAT/RSAT
  long long frequency;
  long long ctun_frequency;
  int ctun;
  int mode;
  int filter;
  int deviation;
  int bd;
  //
  // The above data is for "normal" (non-SAT-mode) memory slots.
  // The data comes from / goes to the VFO of the active receiver (only)
  //
  // For SAT/RSAT modes, we need a second set of data, and the VFO-A data
  // always goes to the "normal" set, while the VFO-B data goes to the
  // "alternate" set.
  //
  // This does *not* apply to CTCSS data, since this is global
  //
  long long alt_frequency;
  long long alt_ctun_frequency;
  int  alt_ctun;
  int  alt_mode;
  int  alt_filter;
  int  alt_deviation;
  int  alt_bd;

  int ctcss_enabled;
  int ctcss;
};

typedef struct _MEM_STORE MEM;

extern MEM mem[];
void memRestoreState(void);
void memSaveState(void);
void recall_memory_slot(int index);
void store_memory_slot(int index);

#endif
