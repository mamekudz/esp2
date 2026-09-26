// ESP][ mechanical skeleton — NON-STYLED layout / collision study only.
// NOT the final enclosure. No fillets, vents, keycaps, logos, or drive art.
//
// Units: millimetres. Coordinate system: see dimensions/coordinate-system.json
//   X = left→right, Y = front→rear, Z = bottom→top
//
// Open in OpenSCAD (or any SCAD→STEP toolchain). Values marked PROVISIONAL
// must not be treated as frozen product dimensions.

$fn = 24;

/* ---- OFFICIAL Waveshare display params (board.json / display-geometry.json) ---- */
oled_physical_width  = 26.74;
oled_physical_height = 41.62;
oled_physical_r      = 3.7;

oled_active_width  = 22.34;
oled_active_height = 36.06;
oled_upper_radius  = 2.5;
oled_active_offset_x = 2.2;
oled_active_offset_y = 2.28; // from glass top

/* PROVISIONAL_CAD_CANDIDATE visible window */
monitor_visible_width  = 22.34;
monitor_visible_height = 33.56; // 36.06 - 2.50
bezel_overlap_top = 2.5;

/* Module envelope (OFFICIAL outline — not STEP AABB) */
module_w = 29.12;
module_h = 44.0;
module_t = 9.6;

/* ---- PROVISIONAL miniature enclosure volumes (layout only) ---- */
computer_w = 120;
computer_d = 90;
computer_h = 35;

monitor_w = 55;
monitor_d = 40;
monitor_h = 70;

drive_w = 40;
drive_d = 55;
drive_h = 18;

keyboard_zone_h = 8;

module rounded_rect_2d(w, h, r) {
  offset(r = r) offset(delta = -r) square([w, h], center = true);
}

module display_glass_physical() {
  color([0.7, 0.85, 1.0, 0.35])
    linear_extrude(height = 0.8)
      rounded_rect_2d(oled_physical_width, oled_physical_height, oled_physical_r);
}

module display_active_area() {
  // Upper-only rounding approximated with full rounded rect for visualization.
  color([0.2, 0.9, 0.3, 0.45])
    translate([0, -(oled_physical_height - oled_active_height) / 2 + oled_active_offset_y
                 - (oled_physical_height / 2 - oled_active_height / 2), 0.9])
      linear_extrude(height = 0.4)
        rounded_rect_2d(oled_active_width, oled_active_height, oled_upper_radius);
}

module display_visible_window() {
  color([1, 0.2, 0.2, 0.55])
    translate([0,
      -(oled_physical_height / 2)
        + oled_active_offset_y
        + bezel_overlap_top
        + monitor_visible_height / 2,
      1.4])
      linear_extrude(height = 0.3)
        square([monitor_visible_width, monitor_visible_height], center = true);
}

module waveshare_module_envelope() {
  // Axis-aligned stand-in for official outline (STEP imported separately).
  color([0.3, 0.3, 0.35, 0.5])
    cube([module_w, module_t, module_h], center = true);
}

module board_insertion_envelope() {
  // Clearance volume for +Z insertion from below — PROVISIONAL padding.
  pad = 1.5;
  color([1, 0.6, 0, 0.2])
    translate([0, 0, -module_h / 2 - 8])
      cube([module_w + 2 * pad, module_t + 2 * pad, module_h + 16], center = true);
}

module usb_path_keepout() {
  // PENDING_COMPONENT_SELECTION — placeholder only.
  color([0.2, 0.5, 1, 0.25])
    translate([0, module_t / 2 + 6, -module_h / 2 + 4])
      cube([12, 18, 10], center = true);
}

module approx_computer_volume() {
  color([0.93, 0.86, 0.7, 0.25])
    translate([0, computer_d / 2, computer_h / 2])
      cube([computer_w, computer_d, computer_h], center = true);
}

module approx_monitor_volume() {
  color([0.93, 0.86, 0.7, 0.3])
    translate([0, -monitor_d / 2 + 8, computer_h + monitor_h / 2])
      cube([monitor_w, monitor_d, monitor_h], center = true);
}

module approx_drive_volumes() {
  color([0.93, 0.86, 0.7, 0.28]) {
    translate([-drive_w / 2 - 2, computer_d / 2 + 5, computer_h + drive_h / 2])
      cube([drive_w, drive_d, drive_h], center = true);
    translate([drive_w / 2 + 2, computer_d / 2 + 5, computer_h + drive_h / 2])
      cube([drive_w, drive_d, drive_h], center = true);
  }
}

module bottom_plane() {
  color([0.55, 0.55, 0.55, 0.4])
    translate([0, computer_d / 2, -0.5])
      cube([computer_w + 4, computer_d + 4, 1], center = true);
}

module keyboard_zone() {
  color([0.45, 0.28, 0.15, 0.35])
    translate([0, 25, keyboard_zone_h / 2 + 1])
      cube([computer_w - 10, 50, keyboard_zone_h], center = true);
}

module rear_io_zone() {
  color([0.4, 0.4, 0.45, 0.35])
    translate([0, computer_d - 4, 12])
      cube([computer_w - 20, 8, 20], center = true);
}

/* Board + display stack placed in monitor (portrait) */
module board_stack_in_monitor() {
  translate([0, -monitor_d / 2 + 8 + module_t / 2 + 2, computer_h + monitor_h / 2]) {
    rotate([90, 0, 0]) {
      waveshare_module_envelope();
      board_insertion_envelope();
      translate([0, -module_t / 2 - 0.5, 0]) {
        rotate([-90, 0, 0]) {
          // Glass facing −Y (toward user) after outer rotate — visualized flat here.
        }
      }
      usb_path_keepout();
    }
    // Display planes facing user (−Y)
    translate([0, -module_t / 2 - 1, 0])
      rotate([90, 0, 0]) {
        display_glass_physical();
        display_active_area();
        display_visible_window();
      }
  }
}

bottom_plane();
approx_computer_volume();
approx_monitor_volume();
approx_drive_volumes();
keyboard_zone();
rear_io_zone();
board_stack_in_monitor();
