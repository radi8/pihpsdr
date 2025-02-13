/* Copyright (C)
* 2016 - John Melton, G0ORX/N6LYT
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "band.h"
#include "bandstack.h"
#include "ext.h"
#include "filter.h"
#include "mode.h"
#include "new_menu.h"
#include "noise_menu.h"
#include "radio.h"
#include "vfo.h"

static GtkWidget *dialog = NULL;

static GtkWidget *nr_container;
static GtkWidget *nb_container;
#ifdef EXTNR
  static GtkWidget *nr4_container;
#endif

static void cleanup() {
  if (dialog != NULL) {
    GtkWidget *tmp = dialog;
    dialog = NULL;
    gtk_widget_destroy(tmp);
    sub_menu = NULL;
    active_menu  = NO_MENU;
    radio_save_state();
  }
}

static gboolean close_cb () {
  cleanup();
  return TRUE;
}

void update_noise() {
  int id = active_receiver->id;

  if (radio_is_remote) {
#ifdef CLIENT_SERVER
    send_noise(client_socket, active_receiver);
#endif
    return;
  }

  //
  // Update the mode settings
  //
  if (id == 0) {
    int mode = vfo[id].mode;
    mode_settings[mode].nr = active_receiver->nr;
    mode_settings[mode].nb = active_receiver->nb;
    mode_settings[mode].anf = active_receiver->anf;
    mode_settings[mode].snb = active_receiver->snb;
    mode_settings[mode].nr2_ae = active_receiver->nr2_ae;
    mode_settings[mode].nr_agc = active_receiver->nr_agc;
    mode_settings[mode].nb2_mode = active_receiver->nb2_mode;
    mode_settings[mode].nr2_gain_method = active_receiver->nr2_gain_method;
    mode_settings[mode].nr2_npe_method = active_receiver->nr2_npe_method;
    mode_settings[mode].nr2_trained_threshold = active_receiver->nr2_trained_threshold;
    mode_settings[mode].nr2_trained_t2 = active_receiver->nr2_trained_t2;
    mode_settings[mode].nb_tau = active_receiver->nb_tau;
    mode_settings[mode].nb_advtime = active_receiver->nb_advtime;
    mode_settings[mode].nb_hang = active_receiver->nb_hang;
    mode_settings[mode].nb_thresh = active_receiver->nb_thresh;
#ifdef EXTNR
    mode_settings[mode].nr4_reduction_amount = active_receiver->nr4_reduction_amount;
    mode_settings[mode].nr4_smoothing_factor = active_receiver->nr4_smoothing_factor;
    mode_settings[mode].nr4_whitening_factor = active_receiver->nr4_whitening_factor;
    mode_settings[mode].nr4_noise_rescale = active_receiver->nr4_noise_rescale;
    mode_settings[mode].nr4_post_threshold = active_receiver->nr4_post_threshold;
#endif
    copy_mode_settings(mode);
  }

  rx_set_noise(active_receiver);
  g_idle_add(ext_vfo_update, NULL);
}

static void nb_cb(GtkToggleButton *widget, gpointer data) {
  active_receiver->nb = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
  update_noise();
}

static void nr_cb(GtkToggleButton *widget, gpointer data) {
  active_receiver->nr = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
  update_noise();
}

static void anf_cb(GtkWidget *widget, gpointer data) {
  active_receiver->anf = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  update_noise();
}

static void snb_cb(GtkWidget *widget, gpointer data) {
  active_receiver->snb = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  update_noise();
}

static void ae_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nr2_ae = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  update_noise();
}

static void pos_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nr_agc = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
  update_noise();
}

static void mode_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nb2_mode = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
  update_noise();
}

static void gain_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nr2_gain_method = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
  update_noise();
}

static void npe_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nr2_npe_method = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
  update_noise();
}

static void trained_thr_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nr2_trained_threshold = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  update_noise();
}

static void trained_t2_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nr2_trained_t2 = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  update_noise();
}

static void slew_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nb_tau = 0.001 * gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  update_noise();
}

static void lead_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nb_advtime = 0.001 * gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  update_noise();
}

static void lag_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nb_hang = 0.001 * gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  update_noise();
}

static void thresh_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nb_thresh = 0.165 * gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  update_noise();
}

static void nr_sel_changed(GtkWidget *widget, gpointer data) {
  // show or hide all controls for NR settings
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    gtk_widget_show(nr_container);
  } else {
    gtk_widget_hide(nr_container);
  }
}

static void nb_sel_changed(GtkWidget *widget, gpointer data) {
  // show or hide all controls for NB settings
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    gtk_widget_show(nb_container);
  } else {
    gtk_widget_hide(nb_container);
  }
}

#ifdef EXTNR
static void nr4_sel_changed(GtkWidget *widget, gpointer data) {
  // show or hide all controls for NR4 settings
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    gtk_widget_show(nr4_container);
  } else {
    gtk_widget_hide(nr4_container);
  }
}

static void nr4_reduction_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nr4_reduction_amount = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  update_noise();
}

static void nr4_smoothing_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nr4_smoothing_factor = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  update_noise();
}

static void nr4_whitening_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nr4_whitening_factor = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  update_noise();
}

static void nr4_rescale_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nr4_noise_rescale = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  update_noise();
}

static void nr4_threshold_cb(GtkWidget *widget, gpointer data) {
  active_receiver->nr4_post_threshold = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  update_noise();
}

#endif

void noise_menu(GtkWidget *parent) {
  dialog = gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
  char title[64];
  snprintf(title, 64, "piHPSDR - Noise (RX%d VFO-%s)", active_receiver->id + 1, active_receiver->id == 0 ? "A" : "B");
  GtkWidget *headerbar = gtk_header_bar_new();
  gtk_window_set_titlebar(GTK_WINDOW(dialog), headerbar);
  gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(headerbar), TRUE);
  gtk_header_bar_set_title(GTK_HEADER_BAR(headerbar), title);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (close_cb), NULL);
  g_signal_connect (dialog, "destroy", G_CALLBACK (close_cb), NULL);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid), FALSE);
  gtk_grid_set_column_spacing (GTK_GRID(grid), 5);
  gtk_grid_set_row_spacing (GTK_GRID(grid), 5);
  GtkWidget *close_b = gtk_button_new_with_label("Close");
  gtk_widget_set_name(close_b, "close_button");
  g_signal_connect (close_b, "button-press-event", G_CALLBACK(close_cb), NULL);
  gtk_grid_attach(GTK_GRID(grid), close_b, 0, 0, 1, 1);
  //
  // First row: SNB/ANF/NR method
  //
  GtkWidget *b_snb = gtk_check_button_new_with_label("SNB");
  gtk_widget_set_name(b_snb, "boldlabel");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_snb), active_receiver->snb);
  gtk_widget_show(b_snb);
  gtk_grid_attach(GTK_GRID(grid), b_snb, 0, 1, 1, 1);
  g_signal_connect(b_snb, "toggled", G_CALLBACK(snb_cb), NULL);
  //
  GtkWidget *b_anf = gtk_check_button_new_with_label("ANF");
  gtk_widget_set_name(b_anf, "boldlabel");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_anf), active_receiver->anf);
  gtk_widget_show(b_anf);
  gtk_grid_attach(GTK_GRID(grid), b_anf, 1, 1, 1, 1);
  g_signal_connect(b_anf, "toggled", G_CALLBACK(anf_cb), NULL);
  //
  GtkWidget *nr_title = gtk_label_new("Noise Reduction");
  gtk_widget_set_name(nr_title, "boldlabel");
  gtk_widget_set_halign(nr_title, GTK_ALIGN_END);
  gtk_widget_show(nr_title);
  gtk_grid_attach(GTK_GRID(grid), nr_title, 2, 1, 1, 1);
  GtkWidget *nr_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nr_combo), NULL, "NONE");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nr_combo), NULL, "NR");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nr_combo), NULL, "NR2");
#ifdef EXTNR
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nr_combo), NULL, "NR3");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nr_combo), NULL, "NR4");
#endif
  gtk_combo_box_set_active(GTK_COMBO_BOX(nr_combo), active_receiver->nr);
  my_combo_attach(GTK_GRID(grid), nr_combo, 3, 1, 1, 1);
  g_signal_connect(nr_combo, "changed", G_CALLBACK(nr_cb), NULL);
  //
  // Second row: NB selection
  //
  GtkWidget *nb_title = gtk_label_new("Noise Blanker");
  gtk_widget_set_name(nb_title, "boldlabel");
  gtk_widget_set_halign(nb_title, GTK_ALIGN_END);
  gtk_widget_show(nb_title);
  gtk_grid_attach(GTK_GRID(grid), nb_title, 2, 2, 1, 1);
  GtkWidget *nb_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nb_combo), NULL, "NONE");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nb_combo), NULL, "NB");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(nb_combo), NULL, "NB2");
  gtk_combo_box_set_active(GTK_COMBO_BOX(nb_combo), active_receiver->nb);
  my_combo_attach(GTK_GRID(grid), nb_combo, 3, 2, 1, 1);
  g_signal_connect(nb_combo, "changed", G_CALLBACK(nb_cb), NULL);
  GtkWidget *line = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_size_request(line, -1, 3);
  gtk_grid_attach(GTK_GRID(grid), line, 0, 3, 4, 1);
  //
  // Third row: select settings: NR, NB, NR4 settings
  //
  GtkWidget *nr_sel = gtk_radio_button_new_with_label_from_widget(NULL, "NR Settings");
  gtk_widget_set_name(nr_sel, "boldlabel");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(nr_sel), 1);
  gtk_widget_show(nr_sel);
  gtk_grid_attach(GTK_GRID(grid), nr_sel, 0, 4, 1, 1);
  g_signal_connect(nr_sel, "toggled", G_CALLBACK(nr_sel_changed), NULL);
  //
  GtkWidget *nb_sel = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(nr_sel), "NB Settings");
  gtk_widget_set_name(nb_sel, "boldlabel");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(nb_sel), 0);
  gtk_widget_show(nb_sel);
  gtk_grid_attach(GTK_GRID(grid), nb_sel, 1, 4, 1, 1);
  g_signal_connect(nb_sel, "toggled", G_CALLBACK(nb_sel_changed), NULL);
#ifdef EXTNR
  GtkWidget *nr4_sel = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(nr_sel), "NR4 Settings");
  gtk_widget_set_name(nr4_sel, "boldlabel");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(nr4_sel), 0);
  gtk_widget_show(nr4_sel);
  gtk_grid_attach(GTK_GRID(grid), nr4_sel, 2, 4, 1, 1);
  g_signal_connect(nr4_sel, "toggled", G_CALLBACK(nr4_sel_changed), NULL);
#endif
  //
  // Hiding/Showing ComboBoxes optimized for Touch-Screens does not
  // work. Therefore, we have to group the NR, NB, and NR4 controls
  // in a container, which then can be shown/hidden
  //
  //
  // NR controls
  //
  nr_container = gtk_fixed_new();
  gtk_grid_attach(GTK_GRID(grid), nr_container, 0, 5, 4, 3);
  GtkWidget *nr_grid = gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(nr_grid), TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(nr_grid), TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(nr_grid), 5);
  gtk_grid_set_row_spacing (GTK_GRID(nr_grid), 5);
  //
  GtkWidget *gain_title = gtk_label_new("NR2 Gain Method");
  gtk_widget_set_name(gain_title, "boldlabel");
  gtk_widget_set_halign(gain_title, GTK_ALIGN_END);
  gtk_widget_show(gain_title);
  gtk_grid_attach(GTK_GRID(nr_grid), gain_title, 0, 0, 1, 1);
  //
  GtkWidget *gain_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(gain_combo), NULL, "Linear");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(gain_combo), NULL, "Log");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(gain_combo), NULL, "Gamma");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(gain_combo), NULL, "Trained");
  gtk_combo_box_set_active(GTK_COMBO_BOX(gain_combo), active_receiver->nr2_gain_method);
  my_combo_attach(GTK_GRID(nr_grid), gain_combo, 1, 0, 1, 1);
  g_signal_connect(gain_combo, "changed", G_CALLBACK(gain_cb), NULL);
  //
  GtkWidget *npe_title = gtk_label_new("NR2 NPE Method");
  gtk_widget_set_name(npe_title, "boldlabel");
  gtk_widget_set_halign(npe_title, GTK_ALIGN_END);
  gtk_widget_show(npe_title);
  gtk_grid_attach(GTK_GRID(nr_grid), npe_title, 2, 0, 1, 1);
  GtkWidget *npe_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(npe_combo), NULL, "OSMS");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(npe_combo), NULL, "MMSE");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(npe_combo), NULL, "NSTAT");
  gtk_combo_box_set_active(GTK_COMBO_BOX(npe_combo), active_receiver->nr2_npe_method);
  my_combo_attach(GTK_GRID(nr_grid), npe_combo, 3, 0, 1, 1);
  g_signal_connect(npe_combo, "changed", G_CALLBACK(npe_cb), NULL);
  //
  GtkWidget *pos_title = gtk_label_new("NR/NR2/ANF Position");
  gtk_widget_set_name(pos_title, "boldlabel");
  gtk_widget_set_halign(pos_title, GTK_ALIGN_END);
  gtk_widget_show(pos_title);
  gtk_grid_attach(GTK_GRID(nr_grid), pos_title, 0, 1, 1, 1);
  GtkWidget *pos_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(pos_combo), NULL, "Pre AGC");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(pos_combo), NULL, "Post AGC");
  gtk_combo_box_set_active(GTK_COMBO_BOX(pos_combo), active_receiver->nr_agc);
  my_combo_attach(GTK_GRID(nr_grid), pos_combo, 1, 1, 1, 1);
  g_signal_connect(pos_combo, "changed", G_CALLBACK(pos_cb), NULL);
  //
  GtkWidget *b_ae = gtk_check_button_new_with_label("NR2 Artifact Elimination");
  gtk_widget_set_name(b_ae, "boldlabel");
  gtk_widget_set_halign(b_ae, GTK_ALIGN_END);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b_ae), active_receiver->nr2_ae);
  gtk_widget_show(b_ae);
  gtk_grid_attach(GTK_GRID(nr_grid), b_ae, 2, 1, 2, 1);
  g_signal_connect(b_ae, "toggled", G_CALLBACK(ae_cb), NULL);
  //
  GtkWidget *trained_thr_title = gtk_label_new("NR2 Trained Thresh");
  gtk_widget_set_name(trained_thr_title, "boldlabel");
  gtk_widget_set_halign(trained_thr_title, GTK_ALIGN_END);
  gtk_widget_show(trained_thr_title);
  gtk_grid_attach(GTK_GRID(nr_grid), trained_thr_title, 0, 2, 1, 1);
  GtkWidget *trained_thr_b = gtk_spin_button_new_with_range(-5.0, 5.0, 0.1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(trained_thr_b), active_receiver->nr2_trained_threshold);
  gtk_grid_attach(GTK_GRID(nr_grid), trained_thr_b, 1, 2, 1, 1);
  g_signal_connect(trained_thr_b, "changed", G_CALLBACK(trained_thr_cb), NULL);
  //
  GtkWidget *trained_t2_title = gtk_label_new("NR2 Trained T2");
  gtk_widget_set_name(trained_t2_title, "boldlabel");
  gtk_widget_set_halign(trained_t2_title, GTK_ALIGN_END);
  gtk_widget_show(trained_t2_title);
  gtk_grid_attach(GTK_GRID(nr_grid), trained_t2_title, 2, 2, 1, 1);
  GtkWidget *trained_t2_b = gtk_spin_button_new_with_range(0.02, 0.3, 0.01);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(trained_t2_b), active_receiver->nr2_trained_t2);
  gtk_grid_attach(GTK_GRID(nr_grid), trained_t2_b, 3, 2, 1, 1);
  g_signal_connect(trained_thr_b, "changed", G_CALLBACK(trained_t2_cb), NULL);
  //
  gtk_container_add(GTK_CONTAINER(nr_container), nr_grid);
  //
  // NB controls starting on row 4
  //
  nb_container = gtk_fixed_new();
  gtk_grid_attach(GTK_GRID(grid), nb_container, 0, 5, 4, 3);
  GtkWidget *nb_grid = gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(nb_grid), TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(nb_grid), TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(nb_grid), 5);
  gtk_grid_set_row_spacing (GTK_GRID(nb_grid), 5);
  //
  GtkWidget *mode_title = gtk_label_new("NB2 mode");
  gtk_widget_set_name(mode_title, "boldlabel");
  gtk_widget_set_halign(mode_title, GTK_ALIGN_END);
  gtk_grid_attach(GTK_GRID(nb_grid), mode_title, 0, 0, 1, 1);
  GtkWidget *mode_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(mode_combo), NULL, "Zero");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(mode_combo), NULL, "Sample&Hold");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(mode_combo), NULL, "Mean Hold");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(mode_combo), NULL, "Hold Sample");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(mode_combo), NULL, "Interpolate");
  gtk_combo_box_set_active(GTK_COMBO_BOX(mode_combo), active_receiver->nb2_mode);
  my_combo_attach(GTK_GRID(nb_grid), mode_combo, 1, 0, 1, 1);
  g_signal_connect(mode_combo, "changed", G_CALLBACK(mode_cb), NULL);
  //
  GtkWidget *slew_title = gtk_label_new("NB Slew time (ms)");
  gtk_widget_set_name(slew_title, "boldlabel");
  gtk_widget_set_halign(slew_title, GTK_ALIGN_END);
  gtk_grid_attach(GTK_GRID(nb_grid), slew_title, 0, 1, 1, 1);
  GtkWidget *slew_b = gtk_spin_button_new_with_range(0.0, 0.1, 0.001);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(slew_b), active_receiver->nb_tau * 1000.0);
  gtk_grid_attach(GTK_GRID(nb_grid), slew_b, 1, 1, 1, 1);
  g_signal_connect(slew_b, "changed", G_CALLBACK(slew_cb), NULL);
  //
  GtkWidget *lead_title = gtk_label_new("NB Lead time (ms)");
  gtk_widget_set_name(lead_title, "boldlabel");
  gtk_widget_set_halign(lead_title, GTK_ALIGN_END);
  gtk_grid_attach(GTK_GRID(nb_grid), lead_title, 2, 1, 1, 1);
  GtkWidget *lead_b = gtk_spin_button_new_with_range(0.0, 0.1, 0.001);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(lead_b), active_receiver->nb_advtime * 1000.0);
  gtk_grid_attach(GTK_GRID(nb_grid), lead_b, 3, 1, 1, 1);
  g_signal_connect(lead_b, "changed", G_CALLBACK(lead_cb), NULL);
  //
  GtkWidget *lag_title = gtk_label_new("NB Lag time (ms)");
  gtk_widget_set_name(lag_title, "boldlabel");
  gtk_widget_set_halign(lag_title, GTK_ALIGN_END);
  gtk_grid_attach(GTK_GRID(nb_grid), lag_title, 0, 2, 1, 1);
  GtkWidget *lag_b = gtk_spin_button_new_with_range(0.0, 0.1, 0.001);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(lag_b), active_receiver->nb_hang * 1000.0);
  gtk_grid_attach(GTK_GRID(nb_grid), lag_b, 1, 2, 1, 1);
  g_signal_connect(lag_b, "changed", G_CALLBACK(lag_cb), NULL);
  //
  GtkWidget *thresh_title = gtk_label_new("NB Threshold");
  gtk_widget_set_name(thresh_title, "boldlabel");
  gtk_widget_set_halign(thresh_title, GTK_ALIGN_END);
  gtk_grid_attach(GTK_GRID(nb_grid), thresh_title, 2, 2, 1, 1);
  GtkWidget *thresh_b = gtk_spin_button_new_with_range(15.0, 500.0, 1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(thresh_b), active_receiver->nb_thresh * 6.0606060606); // 1.0/0.165
  gtk_grid_attach(GTK_GRID(nb_grid), thresh_b, 3, 2, 1, 1);
  g_signal_connect(thresh_b, "changed", G_CALLBACK(thresh_cb), NULL);
  gtk_container_add(GTK_CONTAINER(nb_container), nb_grid);
#ifdef EXTNR
  //
  // NR4 controls starting at row 4
  //
  nr4_container = gtk_fixed_new();
  gtk_grid_attach(GTK_GRID(grid), nr4_container, 0, 5, 4, 3);
  GtkWidget *nr4_grid = gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(nr4_grid), TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(nr4_grid), TRUE);
  gtk_grid_set_column_spacing (GTK_GRID(nr4_grid), 5);
  gtk_grid_set_row_spacing (GTK_GRID(nr4_grid), 5);
  //
  GtkWidget *nr4_reduction_title = gtk_label_new("NR4 Reduction (dB)");
  gtk_widget_set_name(nr4_reduction_title, "boldlabel");
  gtk_widget_set_halign(nr4_reduction_title, GTK_ALIGN_END);
  gtk_grid_attach(GTK_GRID(nr4_grid), nr4_reduction_title, 0, 0, 1, 1);
  GtkWidget *nr4_reduction_b = gtk_spin_button_new_with_range(0.0, 20.0, 1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(nr4_reduction_b), active_receiver->nr4_reduction_amount);
  gtk_grid_attach(GTK_GRID(nr4_grid), nr4_reduction_b, 1, 0, 1, 1);
  g_signal_connect(G_OBJECT(nr4_reduction_b), "changed", G_CALLBACK(nr4_reduction_cb), NULL);
  //
  GtkWidget *nr4_smoothing_title = gtk_label_new("NR4 Smoothing (%)");
  gtk_widget_set_name(nr4_smoothing_title, "boldlabel");
  gtk_widget_set_halign(nr4_smoothing_title, GTK_ALIGN_END);
  gtk_grid_attach(GTK_GRID(nr4_grid), nr4_smoothing_title, 2, 0, 1, 1);
  GtkWidget *nr4_smoothing_b = gtk_spin_button_new_with_range(0.0, 100.0, 1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(nr4_smoothing_b), active_receiver->nr4_smoothing_factor);
  gtk_grid_attach(GTK_GRID(nr4_grid), nr4_smoothing_b, 3, 0, 1, 1);
  g_signal_connect(G_OBJECT(nr4_smoothing_b), "changed", G_CALLBACK(nr4_smoothing_cb), NULL);
  //
  GtkWidget *nr4_whitening_title = gtk_label_new("NR4 Whitening (%)");
  gtk_widget_set_name(nr4_whitening_title, "boldlabel");
  gtk_widget_set_halign(nr4_whitening_title, GTK_ALIGN_END);
  gtk_grid_attach(GTK_GRID(nr4_grid), nr4_whitening_title, 0, 1, 1, 1);
  GtkWidget *nr4_whitening_b = gtk_spin_button_new_with_range(0.0, 100.0, 1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(nr4_whitening_b), active_receiver->nr4_whitening_factor);
  gtk_grid_attach(GTK_GRID(nr4_grid), nr4_whitening_b, 1, 1, 1, 1);
  g_signal_connect(G_OBJECT(nr4_whitening_b), "changed", G_CALLBACK(nr4_whitening_cb), NULL);
  //
  GtkWidget *nr4_rescale_title = gtk_label_new("NR4 rescale (dB)");
  gtk_widget_set_name(nr4_rescale_title, "boldlabel");
  gtk_widget_set_halign(nr4_rescale_title, GTK_ALIGN_END);
  gtk_grid_attach(GTK_GRID(nr4_grid), nr4_rescale_title, 2, 1, 1, 1);
  GtkWidget *nr4_rescale_b = gtk_spin_button_new_with_range(0.0, 12.0, 0.1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(nr4_rescale_b), active_receiver->nr4_noise_rescale);
  gtk_grid_attach(GTK_GRID(nr4_grid), nr4_rescale_b, 3, 1, 1, 1);
  g_signal_connect(G_OBJECT(nr4_rescale_b), "changed", G_CALLBACK(nr4_rescale_cb), NULL);
  //
  GtkWidget *nr4_threshold_title = gtk_label_new("NR4 post filter threshold (dB)");
  gtk_widget_set_name(nr4_threshold_title, "boldlabel");
  gtk_widget_set_halign(nr4_threshold_title, GTK_ALIGN_END);
  gtk_grid_attach(GTK_GRID(nr4_grid), nr4_threshold_title, 1, 2, 2, 1);
  GtkWidget *nr4_threshold_b = gtk_spin_button_new_with_range(-10.0, 10.0, 0.1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON(nr4_threshold_b), active_receiver->nr4_post_threshold);
  gtk_grid_attach(GTK_GRID(nr4_grid), nr4_threshold_b, 3, 2, 1, 1);
  g_signal_connect(G_OBJECT(nr4_threshold_b), "changed", G_CALLBACK(nr4_threshold_cb), NULL);
  //
  gtk_container_add(GTK_CONTAINER(nr4_container), nr4_grid);
#endif
  gtk_container_add(GTK_CONTAINER(content), grid);
  sub_menu = dialog;
  gtk_widget_show_all(dialog);
  //
  // The width of the main grid is the largest, since it contains all containers.
  // Determine this width and set the width of all containers to that value.
  // This ensures that the column widths of he main grid and the containers
  // line up so the whole menu looks well aligned.
  //
  int width = gtk_widget_get_allocated_width(grid);
  gtk_widget_set_size_request(nr_grid, width, -1);
  gtk_widget_set_size_request(nb_grid, width, -1);
#ifdef EXTNR
  gtk_widget_set_size_request(nr4_grid, width, -1);
#endif
  gtk_widget_hide(nb_container);
#ifdef EXTNR
  gtk_widget_hide(nr4_container);
#endif
}
