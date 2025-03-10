/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
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

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#include "actions.h"
#include "client_server.h"
#ifdef GPIO
  #include "configure.h"
#endif
#include "discovered.h"
#include "ext.h"
#include "gpio.h"
#include "main.h"
#include "message.h"
#include "mystring.h"
#include "new_discovery.h"
#include "old_discovery.h"
#ifdef USBOZY
  #include "ozyio.h"
#endif
#include "property.h"
#include "protocols.h"
#include "radio.h"
#ifdef SOAPYSDR
  #include "soapy_discovery.h"
#endif
#ifdef STEMLAB_DISCOVERY
  #include "stemlab_discovery.h"
#endif

static GtkWidget *discovery_dialog;
static DISCOVERED *d;

static GtkWidget *apps_combobox[MAX_DEVICES];

static GtkWidget *host_combo = NULL;
static GtkWidget *host_entry = NULL;
static GtkWidget *host_pwd = NULL;
static int        host_entry_changed = 0;
static int        host_pos = 0;
static int        host_empty = 0;
static gulong     host_combo_signal_id = 0;


GtkWidget *tcpaddr;
char ipaddr_radio[128] = { 0 };
int tcp_enable = 0;

int discover_only_stemlab = 0;

int delayed_discovery(gpointer data);

static char host_addr[128] = "127.0.0.1:50000";

static void host_entry_cb(GtkWidget *widget, gpointer data);

static void print_device(int i) {
  t_print("discovery: found protocol=%d device=%d software_version=%d status=%d address=%s (%02X:%02X:%02X:%02X:%02X:%02X) on %s\n",
          discovered[i].protocol,
          discovered[i].device,
          discovered[i].software_version,
          discovered[i].status,
          inet_ntoa(discovered[i].info.network.address.sin_addr),
          discovered[i].info.network.mac_address[0],
          discovered[i].info.network.mac_address[1],
          discovered[i].info.network.mac_address[2],
          discovered[i].info.network.mac_address[3],
          discovered[i].info.network.mac_address[4],
          discovered[i].info.network.mac_address[5],
          discovered[i].info.network.interface_name);
}

static gboolean close_cb() {
  host_entry_cb(host_entry, NULL);
  return TRUE;
}

static gboolean start_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  radio = (DISCOVERED *)data;
#ifdef STEMLAB_DISCOVERY

  // We need to start the STEMlab app before destroying the dialog, since
  // we otherwise lose the information about which app has been selected.
  if (radio->protocol == STEMLAB_PROTOCOL) {
    const int device_id = radio - discovered;

    if (radio->software_version & BARE_REDPITAYA) {
      // Start via the simple web interface
      (void) alpine_start_app(gtk_combo_box_get_active_id(GTK_COMBO_BOX(apps_combobox[device_id])));
    } else {
      // Start via the STEMlab "bazaar" interface
      (void) stemlab_start_app(gtk_combo_box_get_active_id(GTK_COMBO_BOX(apps_combobox[device_id])));
    }

    //
    // To make this bullet-proof, we do another "discover" now
    // and proceeding this way is the only way to choose between UDP and TCP connection
    // Since we have just started the app, we temporarily deactivate STEMlab detection
    //
    stemlab_cleanup();
    discover_only_stemlab = 1;
    gtk_widget_destroy(discovery_dialog);
    status_text("Wait for STEMlab app\n");
    g_timeout_add(2000, delayed_discovery, NULL);
    return TRUE;
  }

#endif
  //
  // Starting the radio via the GTK queue ensures quick update
  // of the status label
  //
  status_text("Starting Radio ...\n");
  g_timeout_add(10, ext_start_radio, NULL);
  gtk_widget_destroy(discovery_dialog);
  return TRUE;
}

static gboolean protocols_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  configure_protocols(discovery_dialog);
  return TRUE;
}

#ifdef GPIO
#ifdef GPIO_CONFIGURE_LINES
static gboolean gpio_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  configure_gpio(discovery_dialog);
  return TRUE;
}

#endif
#endif

static void gpio_changed_cb(GtkWidget *widget, gpointer data) {
  controller = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
#ifndef GPIO

  if (controller != G2_V2) {
    controller = NO_CONTROLLER;
    gtk_combo_box_set_active(GTK_COMBO_BOX(widget), controller);
  }

#endif
  gpio_set_defaults(controller);
  gpioSaveState();
}

static gboolean discover_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(discovery_dialog);
  g_timeout_add(100, delayed_discovery, NULL);
  return TRUE;
}

static gboolean exit_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(discovery_dialog);
  _exit(0);
  return TRUE;
}

void save_ipaddr() {
  clearProperties();

  if (strlen(ipaddr_radio) > 0) {
    SetPropS0("radio_ip_addr", ipaddr_radio);
  }

  SetPropI0("radio_tcp_enable", tcp_enable);
  saveProperties("ipaddr.props");
}

static gboolean radio_ip_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  const char *cp;
  cp = gtk_entry_get_text(GTK_ENTRY(tcpaddr));

  if (cp && (strlen(cp) > 0)) {
    STRLCPY(ipaddr_radio, cp, sizeof(ipaddr_radio));
  } else {
    ipaddr_radio[0]=0;
  }

  save_ipaddr();
  return FALSE;
}

static void tcp_en_cb(GtkWidget *widget, gpointer data) {
  tcp_enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  save_ipaddr();
}

//------------------------------------------------------+
// Supporting functions for the server selection screen |
//------------------------------------------------------+

static void save_hostlist() {
  g_signal_handler_block(G_OBJECT(host_combo), host_combo_signal_id);
  clearProperties();
  int count = 0;
  const gchar *text;
  for (int i = 0; ; i++) {
    gtk_combo_box_set_active(GTK_COMBO_BOX(host_combo), i);

    if (gtk_combo_box_get_active(GTK_COMBO_BOX(host_combo)) != i) {
      break;
    }

    text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(host_combo));
    if  (strlen(text) > 0) {
      SetPropS1("host[%d]", count++, text);
    }
  }

  SetPropI0("num_hosts", count);
  SetPropS0("current_host", host_addr);
  SetPropS0("property_version", "3.00");
  saveProperties("remote.props");
  gtk_combo_box_set_active(GTK_COMBO_BOX(host_combo), host_pos);
  g_signal_handler_unblock(G_OBJECT(host_combo), host_combo_signal_id);
}

static void host_entry_cb(GtkWidget *widget, gpointer data) {
  //
  // This is called when the ENTER key is hit in the text entry,
  // but also at other occasions to update the combo-box and save
  // its contents.
  //
  if (host_entry_changed) {
    //
    // If  this flag has been set, something has been entered into the
    // text entry field. host_addr has been updated but not the combo
    // box itself. If the text entry field is empty, usually text==NULL
    // rather than strlen(text)==0.
    //
    g_signal_handler_block(G_OBJECT(host_combo), host_combo_signal_id);
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(widget));

    if (!host_empty) {
      // Remove old combobox entry unless it was  empty
      gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(host_combo), host_pos);
    }

    if (text && (strlen(text) > 0)) {
      // Add new entry at the beginning, unless the new text is empty
      gtk_combo_box_text_prepend(GTK_COMBO_BOX_TEXT(host_combo), NULL, text);
      snprintf(host_addr, sizeof(host_addr), "%s", text);
    }

    gtk_combo_box_set_active(GTK_COMBO_BOX(host_combo), 0);
    host_pos = 0;
    text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(host_combo));
    host_empty = !(text && (strlen(text) > 0));
    host_entry_changed = 0;
    g_signal_handler_unblock(G_OBJECT(host_combo), host_combo_signal_id);
  }

  //
  // The combo box has changed, so dump contents to props file
  //
  save_hostlist();
}

static gboolean connect_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
  char myhost[256];
  int  myport;
  const char *mypwd;

  host_entry_cb(host_entry, NULL);

  *myhost = 0;
  myport = 0;
  gchar **splitstr = g_strsplit(host_addr, ":", 2);
  if (splitstr[0] && splitstr[1]) {
     snprintf(myhost, sizeof(myhost), "%s", splitstr[0]);
     myport = atoi(splitstr[1]);
  }
  g_strfreev(splitstr);
  mypwd = gtk_entry_get_text(GTK_ENTRY(host_pwd));


  t_print("connect_cb: host=%s port=%d pw=%s\n", myhost, myport, mypwd);

  if (*myhost == 0 || myport == 0) {
    g_idle_add(fatal_error, "NOTICE: invalid host:port string.");
    return TRUE;
  }

  switch (radio_connect_remote(myhost, myport, mypwd)) {
  case 0:
    gtk_widget_destroy(discovery_dialog);
    break;
  case -1:
    g_idle_add(fatal_error, "NOTICE: remote connection failed.");
    break;
  case -2:
    g_idle_add(fatal_error, "NOTICE: remote connection timeout.");
    break;
  case -3:
    g_idle_add(fatal_error, "NOTICE: host not found.");
    break;
  case -4:
    g_idle_add(fatal_error, "NOTICE: invalid  password.");
    break;
  default:
    g_idle_add(fatal_error, "NOTICE: unknown error in connect.");
    break;
  }

  return TRUE;
}

static void host_combo_cb(GtkWidget *widget, gpointer data) {
  int val = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

  if (val >= 0) {
    //
    // An existing entry has been selected
    //
    host_pos = val;
    const gchar *c = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget));
    host_empty = (strlen(c) < 1);
    if (!host_empty) {
      snprintf(host_addr, sizeof(host_addr), "%s", c);
    }
  } else {
    //
    // Something has been typed into the entry  field: update host_addr
    //
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(host_entry));
    if (text) {
      snprintf(host_addr, sizeof(host_addr), "%s", text);
    } else {
      *host_addr = 0;
    }
    host_entry_changed = 1;
  }
}

static void password_visibility_cb(GtkToggleButton *button, gpointer user_data) {
    GtkEntry *entry = GTK_ENTRY(user_data);
    gboolean visible = !gtk_entry_get_visibility(entry);
    gtk_entry_set_visibility(entry, visible);
    if (visible) {
      gtk_button_set_label(GTK_BUTTON(button), "Hide");
    } else {
      gtk_button_set_label(GTK_BUTTON(button), "Show");
    }
}

//----------------------------------------------------+
// Build the discovery window                         |
//----------------------------------------------------+

static void discovery() {
  //
  // On the discovery screen, make the combo-boxes "touchscreen-friendly"
  //
  optimize_for_touchscreen = 1;
  protocolsRestoreState();
  selected_device = 0;
  devices = 0;
  loadProperties("ipaddr.props");
  GetPropS0("radio_ip_addr", ipaddr_radio);
  GetPropI0("radio_tcp_enable", tcp_enable);

#ifdef USBOZY

  if (enable_usbozy && !discover_only_stemlab) {
    //
    // first: look on USB for an Ozy
    //
    status_text("Looking for USB based OZY devices");

    if (ozy_discover() != 0) {
      discovered[devices].protocol = ORIGINAL_PROTOCOL;
      discovered[devices].device = DEVICE_OZY;
      discovered[devices].software_version = 10;              // we can't know yet so this isn't a real response
      STRLCPY(discovered[devices].name, "Ozy on USB", sizeof(discovered[devices].name));
      discovered[devices].frequency_min = 0.0;
      discovered[devices].frequency_max = 61440000.0;

      for (int i = 0; i < 6; i++) {
        discovered[devices].info.network.mac_address[i] = 0;
      }

      discovered[devices].status = STATE_AVAILABLE;
      discovered[devices].info.network.address_length = 0;
      discovered[devices].info.network.interface_length = 0;
      STRLCPY(discovered[devices].info.network.interface_name, "USB",
              sizeof(discovered[devices].info.network.interface_name));
      discovered[devices].use_tcp = 0;
      discovered[devices].use_routing = 0;
      discovered[devices].supported_receivers = 2;
      t_print("discovery: found USB OZY device min=%0.3f MHz max=%0.3f MHz\n",
              discovered[devices].frequency_min * 1E-6,
              discovered[devices].frequency_max * 1E-6);
      devices++;
    }
  }

#endif
#ifdef SATURN
#include "saturnmain.h"

  if (enable_saturn_xdma && !discover_only_stemlab) {
    status_text("Looking for /dev/xdma* based saturn devices");
    saturn_discovery();
  }

#endif
#ifdef STEMLAB_DISCOVERY

  if (enable_stemlab && !discover_only_stemlab) {
    status_text("Looking for STEMlab WEB apps");
    stemlab_discovery();
  }

#endif

  if (enable_protocol_1 || discover_only_stemlab) {
    if (discover_only_stemlab) {
      status_text("Stemlab ... Looking for SDR apps");
    } else {
      status_text("Protocol 1 ... Discovering Devices");
    }

    old_discovery();
  }

  if (enable_protocol_2 && !discover_only_stemlab) {
    status_text("Protocol 2 ... Discovering Devices");
    new_discovery();
  }

#ifdef SOAPYSDR

  if (enable_soapy_protocol && !discover_only_stemlab) {
    status_text("SoapySDR ... Discovering Devices");
    soapy_discovery();
  }

#endif
  status_text("Discovery completed.");
  // subsequent discoveries check all protocols enabled.
  discover_only_stemlab = 0;
  t_print("discovery: found %d devices\n", devices);

  for (int i = 0; i < devices; i++) { print_device(i); }

  gdk_window_set_cursor(gtk_widget_get_window(top_window), gdk_cursor_new(GDK_ARROW));
  discovery_dialog = gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(discovery_dialog), GTK_WINDOW(top_window));
  GtkWidget *headerbar = gtk_header_bar_new();
  gtk_window_set_titlebar(GTK_WINDOW(discovery_dialog), headerbar);
  gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(headerbar), TRUE);
  gtk_header_bar_set_title(GTK_HEADER_BAR(headerbar), "piHPSDR - Discovery");
  g_signal_connect(discovery_dialog, "delete_event", G_CALLBACK(close_cb), NULL);
  g_signal_connect(discovery_dialog, "destroy", G_CALLBACK(close_cb), NULL);
  GtkWidget *content;
  content = gtk_dialog_get_content_area(GTK_DIALOG(discovery_dialog));
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
  gtk_grid_set_row_spacing (GTK_GRID(grid), 10);
  int row = 0;

  if (devices == 0) {
    GtkWidget *label = gtk_label_new("No local devices found!");
    gtk_widget_set_name(label, "med_txt");
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 3, 1);
    row++;
  } else {
    char version[16];
    char text[512];
    char macStr[18];

    for (row = 0; row < devices; row++) {
      d = &discovered[row];
      t_print("Device Protocol=%d name=%s\n", d->protocol, d->name);
      snprintf(version, sizeof(version), "v%d.%d",
               d->software_version / 10,
               d->software_version % 10);
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               d->info.network.mac_address[0],
               d->info.network.mac_address[1],
               d->info.network.mac_address[2],
               d->info.network.mac_address[3],
               d->info.network.mac_address[4],
               d->info.network.mac_address[5]);

      switch (d->protocol) {
      case ORIGINAL_PROTOCOL:
      case NEW_PROTOCOL:
        if (d->device == DEVICE_OZY) {
          snprintf(text, sizeof(text), "%s (%s via USB)", d->name,
                   d->protocol == ORIGINAL_PROTOCOL ? "Protocol 1" : "Protocol 2");
        } else if (d->device == NEW_DEVICE_SATURN && strcmp(d->info.network.interface_name, "XDMA") == 0) {
          snprintf(text, sizeof(text), "%s (%s v%d) fpga:%x (%s) on /dev/xdma*", d->name,
                   d->protocol == ORIGINAL_PROTOCOL ? "Protocol 1" : "Protocol 2", d->software_version,
                   d->fpga_version, macStr);
        } else {
          snprintf(text, sizeof(text), "%s (%s %s) %s (%s) on %s: ",
                   d->name,
                   d->protocol == ORIGINAL_PROTOCOL ? "Protocol 1" : "Protocol 2",
                   version,
                   inet_ntoa(d->info.network.address.sin_addr),
                   macStr,
                   d->info.network.interface_name);
        }

        break;

      case SOAPYSDR_PROTOCOL:
#ifdef SOAPYSDR
        snprintf(text, sizeof(text), "%s (Protocol SOAPY_SDR %s) on %s", d->name, d->info.soapy.version, d->info.soapy.address);
#endif
        break;

      case STEMLAB_PROTOCOL:
        snprintf(text, sizeof(text), "Choose SDR App from %s: ",
                 inet_ntoa(d->info.network.address.sin_addr));
      }

      GtkWidget *label = gtk_label_new(text);
      gtk_widget_set_name(label, "boldlabel");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_show(label);
      gtk_grid_attach(GTK_GRID(grid), label, 0, row, 3, 1);
      GtkWidget *start_button = gtk_button_new();
      gtk_widget_set_name(start_button, "big_txt");
      gtk_widget_show(start_button);
      gtk_grid_attach(GTK_GRID(grid), start_button, 3, row, 1, 1);
      g_signal_connect(start_button, "button-press-event", G_CALLBACK(start_cb), (gpointer)d);

      // if not available then cannot start it
      switch (d->status) {
      case STATE_AVAILABLE:
        gtk_button_set_label(GTK_BUTTON(start_button), "Start");
        break;

      case STATE_SENDING:
        gtk_button_set_label(GTK_BUTTON(start_button), "In Use");
        gtk_widget_set_sensitive(start_button, FALSE);
        break;

      case STATE_INCOMPATIBLE:
        gtk_button_set_label(GTK_BUTTON(start_button), "Incompatible");
        gtk_widget_set_sensitive(start_button, FALSE);
        break;
      }

      if (d->device != SOAPYSDR_USB_DEVICE) {
        int can_connect = 0;

        //
        // We can connect if
        //  a) either the computer or the radio have a self-assigned IP 169.254.xxx.yyy address
        //  b) we have a "routed" (TCP or UDP) connection to the radio
        //  c) radio and network address are in the same subnet
        //
        if (!strncmp(inet_ntoa(d->info.network.address.sin_addr), "169.254.", 8)) { can_connect = 1; }

        if (!strncmp(inet_ntoa(d->info.network.interface_address.sin_addr), "169.254.", 8)) { can_connect = 1; }

        if (d->use_routing) { can_connect = 1; }

        if ((d->info.network.interface_address.sin_addr.s_addr & d->info.network.interface_netmask.sin_addr.s_addr) ==
            (d->info.network.address.sin_addr.s_addr & d->info.network.interface_netmask.sin_addr.s_addr)) { can_connect = 1; }

        if (!can_connect) {
          gtk_button_set_label(GTK_BUTTON(start_button), "Subnet!");
          gtk_widget_set_sensitive(start_button, FALSE);
        }
      }

      if (d->protocol == STEMLAB_PROTOCOL) {
        if (d->software_version == 0) {
          gtk_button_set_label(GTK_BUTTON(start_button), "No SDR app found!");
          gtk_widget_set_sensitive(start_button, FALSE);
        } else {
          apps_combobox[row] = gtk_combo_box_text_new();

          // We want the default selection priority for the STEMlab app to be
          // RP-Trx > HAMlab-Trx > Pavel-Trx > Pavel-Rx, so we add in decreasing order and
          // always set the newly added entry to be active.
          if ((d->software_version & STEMLAB_PAVEL_RX) != 0) {
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(apps_combobox[row]),
                                      "sdr_receiver_hpsdr", "Pavel-Rx");
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(apps_combobox[row]),
                                        "sdr_receiver_hpsdr");
          }

          if ((d->software_version & STEMLAB_PAVEL_TRX) != 0) {
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(apps_combobox[row]),
                                      "sdr_transceiver_hpsdr", "Pavel-Trx");
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(apps_combobox[row]),
                                        "sdr_transceiver_hpsdr");
          }

          if ((d->software_version & HAMLAB_RP_TRX) != 0) {
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(apps_combobox[row]),
                                      "hamlab_sdr_transceiver_hpsdr", "HAMlab-Trx");
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(apps_combobox[row]),
                                        "hamlab_sdr_transceiver_hpsdr");
          }

          if ((d->software_version & STEMLAB_RP_TRX) != 0) {
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(apps_combobox[row]),
                                      "stemlab_sdr_transceiver_hpsdr", "STEMlab-Trx");
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(apps_combobox[row]),
                                        "stemlab_sdr_transceiver_hpsdr");
          }

          my_combo_attach(GTK_GRID(grid), apps_combobox[row], 4, row, 1, 1);
          gtk_widget_show(apps_combobox[row]);
        }
      }
    }
  }

  //----------------------------------------------------+
  // Construct the Server selection and start interface |
  //----------------------------------------------------+

  loadProperties("remote.props");
  GetPropS0("current_host", host_addr);

  t_print("current host: %s\n", host_addr);

  // Create a "Server" button
  GtkWidget *start_server_button = gtk_button_new_with_label("Use Server");
  g_signal_connect(start_server_button, "clicked", G_CALLBACK(connect_cb), grid);
  gtk_grid_attach(GTK_GRID(grid), start_server_button, 0, row, 1, 1);


  //
  // Create combo-box for servers
  // Populate with hosts from props files, put the "current" host first
  //

  host_combo = gtk_combo_box_text_new_with_entry();
  gtk_grid_attach(GTK_GRID(grid), host_combo, 1, row, 1, 1);

  int num_hosts=0;
  char str[128];
  GetPropI0("num_hosts", num_hosts);
  for (int i = 0; i < num_hosts; i++) {
    *str = 0;
    GetPropS1("host[%d]", i, str);
    t_print("HOST ENTRY #%d = %s\n", i, str);
    if (strcmp(str, host_addr) && *str && strlen(str) > 0) {  // Avoid duplicate
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(host_combo), NULL, str);
    }
  }

   *str = 0;
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(host_combo), NULL, str);
  gtk_combo_box_text_prepend(GTK_COMBO_BOX_TEXT(host_combo), NULL, host_addr);
  host_pos = 0;
  gtk_combo_box_set_active(GTK_COMBO_BOX(host_combo), host_pos);
  host_combo_signal_id = g_signal_connect(host_combo, "changed", G_CALLBACK(host_combo_cb), NULL);

  host_entry = gtk_bin_get_child(GTK_BIN(host_combo));
  g_signal_connect(host_entry, "activate", G_CALLBACK(host_entry_cb), NULL);

  // Create the password entry box
  host_pwd = gtk_entry_new();
  gtk_entry_set_visibility(GTK_ENTRY(host_pwd), FALSE);
  gtk_entry_set_placeholder_text(GTK_ENTRY(host_pwd), "Server Password");
  gtk_grid_attach(GTK_GRID(grid), host_pwd, 2, row, 1, 1);

  // Create the password visibility toggle button
  GtkWidget *toggle_button = gtk_toggle_button_new_with_label("Show");
  g_signal_connect(toggle_button, "toggled", G_CALLBACK(password_visibility_cb), host_pwd);
  gtk_grid_attach(GTK_GRID(grid), toggle_button, 3, row, 1, 1);

  row++;
  controller = NO_CONTROLLER;
  gpioRestoreState();
  gpio_set_defaults(controller);
  GtkWidget *gpio = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(gpio), NULL, "No Controller");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(gpio), NULL, "Controller1");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(gpio), NULL, "Controller2 V1");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(gpio), NULL, "Controller2 V2");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(gpio), NULL, "G2 Front Panel");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(gpio), NULL, "G2 Mk2 Panel");
  my_combo_attach(GTK_GRID(grid), gpio, 0, row, 1, 1);
  gtk_combo_box_set_active(GTK_COMBO_BOX(gpio), controller);
  g_signal_connect(gpio, "changed", G_CALLBACK(gpio_changed_cb), NULL);
  GtkWidget *discover_b = gtk_button_new_with_label("Discover");
  g_signal_connect (discover_b, "button-press-event", G_CALLBACK(discover_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid), discover_b, 1, row, 1, 1);
  GtkWidget *protocols_b = gtk_button_new_with_label("Protocols");
  g_signal_connect (protocols_b, "button-press-event", G_CALLBACK(protocols_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid), protocols_b, 2, row, 1, 1);
//
  row++;
  GtkWidget *tcp_b = gtk_label_new("Radio IP Addr ");
  gtk_widget_set_name(tcp_b, "boldlabel");
  gtk_widget_set_halign (tcp_b, GTK_ALIGN_END);
  gtk_grid_attach(GTK_GRID(grid), tcp_b, 0, row, 1, 1);
  tcpaddr = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(tcpaddr), 20);
  gtk_grid_attach(GTK_GRID(grid), tcpaddr, 1, row, 1, 1);
  gtk_entry_set_text(GTK_ENTRY(tcpaddr), ipaddr_radio);
  g_signal_connect (tcpaddr, "changed", G_CALLBACK(radio_ip_cb), NULL);
  GtkWidget *tcp_en = gtk_check_button_new_with_label("Enable TCP");
  gtk_widget_set_name(tcp_en, "boldlabel");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tcp_en), tcp_enable);
  gtk_widget_set_halign (tcp_en, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), tcp_en, 2, row, 1, 1);
  g_signal_connect(tcp_en, "toggled", G_CALLBACK(tcp_en_cb), NULL);
  GtkWidget *exit_b = gtk_button_new_with_label("Close");
  gtk_widget_set_name(exit_b, "close_button");
  g_signal_connect (exit_b, "button-press-event", G_CALLBACK(exit_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid), exit_b, 3, row, 1, 1);
  gtk_container_add (GTK_CONTAINER (content), grid);
  gtk_widget_show_all(discovery_dialog);
  t_print("showing device dialog\n");
  //
  // Autostart and RedPitaya radios:
  //
  // Autostart means that if there only one device detected, start the
  // radio without the need to click the "Start" button.
  //
  // If this is the first round of a RedPitaya (STEMlab) discovery,
  // there may be more than one SDR app on the RedPitya available
  // from which one can choose one.
  // With autostart, there is no choice in this case, the SDR app
  // with highest priority is automatically chosen (and started),
  // and then the discovery process is re-initiated for RedPitya
  // devices only.
  //
  t_print("%s: devices=%d autostart=%d\n", __FUNCTION__, devices, autostart);

  if (devices == 1 && autostart) {
    d = &discovered[0];

    if (d->status == STATE_AVAILABLE) {
      if (start_cb(NULL, NULL, (gpointer)d)) { return; }
    }
  }
}

//
// Execute discovery() through g_timeout_add()
//
int delayed_discovery(gpointer data) {
  discovery();
  return G_SOURCE_REMOVE;
}
