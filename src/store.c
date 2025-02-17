/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT,  2016 - Steve Wilson, KA6S
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "band.h"
#include "bandstack.h"
#include "ext.h"
#include "filter.h"
#include "message.h"
#include "mode.h"
#include "property.h"
#include "radio.h"
#include "store.h"
#include "store_menu.h"
#include "vfo.h"

MEM mem[NUM_OF_MEMORYS];  // This makes it a compile time option

void memSaveState() {
  for (int b = 0; b < NUM_OF_MEMORYS; b++) {
    SetPropI1("mem.%d.freqA", b,           mem[b].frequency);
    SetPropI1("mem.%d.freqC", b,           mem[b].ctun_frequency);
    SetPropI1("mem.%d.ctun", b,            mem[b].ctun);
    SetPropI1("mem.%d.mode", b,            mem[b].mode);
    SetPropI1("mem.%d.filter", b,          mem[b].filter);
    SetPropI1("mem.%d.deviation", b,       mem[b].deviation);
    SetPropI1("mem.%d.ctcss_enabled", b,   mem[b].ctcss_enabled);
    SetPropI1("mem.%d.ctcss", b,           mem[b].ctcss);
    SetPropI1("mem.%d.band", b,            mem[b].bd);
  }
}

void memRestoreState() {
  for (int b = 0; b < NUM_OF_MEMORYS; b++) {
    //
    // Set defaults
    //
    mem[b].frequency      = 28010000LL;
    mem[b].ctun_frequency = 28010000LL;
    mem[b].ctun           = 0;
    mem[b].mode           = modeCWU;
    mem[b].filter         = filterF5;
    mem[b].deviation      = 2500;
    mem[b].ctcss_enabled  = 0;
    mem[b].ctcss          = 0;
    mem[b].bd             = band10;
    //
    // Read from props
    //
    GetPropI1("mem.%d.freqA", b,           mem[b].frequency);
    GetPropI1("mem.%d.freqC", b,           mem[b].ctun_frequency);
    GetPropI1("mem.%d.ctun", b,            mem[b].ctun);
    GetPropI1("mem.%d.mode", b,            mem[b].mode);
    GetPropI1("mem.%d.filter", b,          mem[b].filter);
    GetPropI1("mem.%d.deviation", b,       mem[b].deviation);
    GetPropI1("mem.%d.ctcss_enabled", b,   mem[b].ctcss_enabled);
    GetPropI1("mem.%d.ctcss", b,           mem[b].ctcss);
    GetPropI1("mem.%d.band", b,            mem[b].bd);
  }
}

void recall_memory_slot(int index) {
  if (radio_is_remote) {
#ifdef CLIENT_SERVER
    send_recall(client_socket, index);
#endif
    return;
  }
  //
  // Recalling a memory slot is essentially the same as recalling a bandstack entry
  // so we just make use of code in vfo_bandstack_changed()
  //
  int id      = active_receiver->id;
  int b       = mem[index].bd;
  int oldmode = vfo[id].mode;
  const BAND *band = band_get_band(b);
  const BANDSTACK *bandstack = bandstack_get_bandstack(b);
  vfo[id].band           = b;
  vfo[id].bandstack      = bandstack->current_entry;
  vfo[id].frequency      = mem[index].frequency;
  vfo[id].ctun_frequency = mem[index].ctun_frequency;
  vfo[id].ctun           = mem[index].ctun;
  vfo[id].mode           = mem[index].mode;
  vfo[id].filter         = mem[index].filter;
  vfo[id].deviation      = mem[index].deviation;
  vfo[id].lo             = band->frequencyLO + band->errorLO;

  if (can_transmit) {
    transmitter->ctcss_enabled = mem[index].ctcss_enabled;
    transmitter->ctcss         = mem[index].ctcss;
    tx_set_ctcss(transmitter);
  }

  //
  // If mode has changed: apply the settings stored with
  // the mode but keep the filter from the memory slot. This means
  // recalling a memory slot will change
  //
  // - noise reduction settings
  // - equalizer settings
  // - VFO step size
  // - TX compressor settings
  //
  if (oldmode != vfo[id].mode) {
    vfo_apply_mode_settings(active_receiver);
    vfo[id].filter = mem[index].filter;
  }

  vfo_vfos_changed();
}

void store_memory_slot(int index) {
  int id = active_receiver->id;
  //
  // Store current frequency, mode, and filter in slot #index
  //
  mem[index].frequency      = vfo[id].frequency;
  mem[index].ctun_frequency = vfo[id].ctun_frequency;
  mem[index].ctun           = vfo[id].ctun;
  mem[index].mode           = vfo[id].mode;
  mem[index].filter         = vfo[id].filter;
  mem[index].deviation      = vfo[id].deviation;
  mem[index].bd             = vfo[id].band;

  if (can_transmit) {
    mem[index].ctcss_enabled = transmitter->ctcss_enabled;
    mem[index].ctcss = transmitter->ctcss;
  }
  if (radio_is_remote) {
    //
    // Do what has been done above even on the client, to ensure
    // the memory slot label is updated in the memory menu
    // immediately. When the server does the store, it sends back
    // the updated memory slot but this arrives too late to get
    // the menu button updated.
    //
#ifdef CLIENT_SERVER
    send_store(client_socket, index);
#endif
    return;
  }
}
