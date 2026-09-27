// ESP][ keyboard + badge + POWER reference geometry
// Units: millimetres. Plasticity-friendly primitives (no decorative mesh).
//
// Coordinate system (local keyboard):
//   +X left→right, +Y front→rear (space at Y≈0), +Z bottom→top
// Insertion into beige upper: from below along +Z
//
// ABSOLUTE_SCALE = PROVISIONAL (unit_mm). Relative layout from
//   dimensions/keyboard-layout.json / keylayout_example.png
//
// NOT final enclosure. POWER is separate. No Apple trademark artwork.

$fn = 32;

/* ---- PROVISIONAL scale (matches keyboard-layout.json) ---- */
unit_mm = 5.5;
key_gap_u = 0.08;
key_top_h = 1.2;
key_chamfer = 0.25;
carrier_t = 1.6;
web_t = 0.8;
web_z = 0.3; // web top below key-top underside
legend_inlay_d = 0.35;
legend_t = 0.4;

/* Envelope margins */
env_margin_x = 4.0;
env_margin_y = 4.0;

function u(v) = v * unit_mm;

module key_top(w_u, h_u) {
  w = u(w_u);
  h = u(h_u);
  // Simple chamfered top via Minkowski-lite: outer then inset top
  hull() {
    translate([key_chamfer, key_chamfer, 0])
      cube([w - 2 * key_chamfer, h - 2 * key_chamfer, key_top_h]);
    translate([0, 0, 0])
      cube([w, h, key_top_h * 0.35]);
  }
}

module legend_recess(w_u, h_u) {
  w = u(w_u) - 1.2;
  h = u(h_u) - 1.2;
  if (w > 0.8 && h > 0.8)
    translate([0.6, 0.6, key_top_h - legend_inlay_d])
      cube([w, h, legend_inlay_d + 0.05]);
}

module key_with_recess(w_u, h_u) {
  difference() {
    key_top(w_u, h_u);
    legend_recess(w_u, h_u);
  }
}

/* ---- Key positions (xU, yU, wU, hU) — keep in sync with keyboard-layout.json ---- */
keys = [
  // number row
  [0.00, 4.48, 1.00, 1.00], [1.08, 4.48, 1.00, 1.00], [2.16, 4.48, 1.00, 1.00],
  [3.24, 4.48, 1.00, 1.00], [4.32, 4.48, 1.00, 1.00], [5.40, 4.48, 1.00, 1.00],
  [6.48, 4.48, 1.00, 1.00], [7.56, 4.48, 1.00, 1.00], [8.64, 4.48, 1.00, 1.00],
  [9.72, 4.48, 1.00, 1.00], [10.80, 4.48, 1.00, 1.00], [11.88, 4.48, 1.00, 1.00],
  [12.96, 4.48, 1.35, 1.00],
  // qwerty
  [0.35, 3.36, 1.00, 1.00], [1.43, 3.36, 1.00, 1.00], [2.51, 3.36, 1.00, 1.00],
  [3.59, 3.36, 1.00, 1.00], [4.67, 3.36, 1.00, 1.00], [5.75, 3.36, 1.00, 1.00],
  [6.83, 3.36, 1.00, 1.00], [7.91, 3.36, 1.00, 1.00], [8.99, 3.36, 1.00, 1.00],
  [10.07, 3.36, 1.00, 1.00], [11.15, 3.36, 1.00, 1.00], [12.23, 3.36, 1.25, 1.00],
  [13.56, 3.36, 1.60, 1.00],
  // asdf
  [0.55, 2.24, 1.25, 1.00], [1.88, 2.24, 1.00, 1.00], [2.96, 2.24, 1.00, 1.00],
  [4.04, 2.24, 1.00, 1.00], [5.12, 2.24, 1.00, 1.00], [6.20, 2.24, 1.00, 1.00],
  [7.28, 2.24, 1.00, 1.00], [8.36, 2.24, 1.00, 1.00], [9.44, 2.24, 1.00, 1.00],
  [10.52, 2.24, 1.00, 1.00], [11.60, 2.24, 1.00, 1.00], [12.68, 2.24, 1.00, 1.00],
  [13.76, 2.24, 1.00, 1.00],
  // zxcv
  [0.85, 1.12, 1.55, 1.00], [2.48, 1.12, 1.00, 1.00], [3.56, 1.12, 1.00, 1.00],
  [4.64, 1.12, 1.00, 1.00], [5.72, 1.12, 1.00, 1.00], [6.80, 1.12, 1.00, 1.00],
  [7.88, 1.12, 1.00, 1.00], [8.96, 1.12, 1.00, 1.00], [10.04, 1.12, 1.00, 1.00],
  [11.12, 1.12, 1.00, 1.00], [12.20, 1.12, 1.00, 1.00], [13.28, 1.12, 1.35, 1.00],
  // space
  [2.20, 0.00, 9.50, 1.00]
];

module brown_key_tops() {
  for (k = keys)
    translate([u(k[0]), u(k[1]), carrier_t])
      key_with_recess(k[2], k[3]);
}

module hidden_webs() {
  // Horizontal row webs + vertical links — below viewing plane through gaps
  color([0.35, 0.2, 0.1])
    translate([0, 0, web_z]) {
      // row spines
      for (y = [0.0, 1.12, 2.24, 3.36, 4.48])
        translate([u(0.2), u(y) + u(0.35), 0])
          cube([u(14.8), web_t, web_t]);
      // vertical connectors
      for (x = [1, 4, 7, 10, 13])
        translate([u(x), u(0.2), 0])
          cube([web_t, u(5.2), web_t]);
    }
}

module carrier_plate() {
  w = u(15.16) + 2 * env_margin_x;
  d = u(5.48) + 2 * env_margin_y;
  translate([-env_margin_x, -env_margin_y, 0])
    cube([w, d, carrier_t]);
}

module brown_keyboard_one_piece() {
  color([0.42, 0.27, 0.16]) {
    carrier_plate();
    hidden_webs();
    brown_key_tops();
  }
}

/* White legend inserts — placeholder plates (editable text in Plasticity) */
module white_legend_placeholders() {
  color([0.95, 0.95, 0.92])
    for (k = keys)
      if (k[2] < 8) // skip space
        translate([u(k[0]) + 0.7, u(k[1]) + 0.7, carrier_t + key_top_h - legend_inlay_d])
          cube([u(k[2]) - 1.4, u(k[3]) - 1.4, legend_t]);
}

/* POWER — separate, not on brown carrier */
module power_indicator() {
  fw = unit_mm;
  fd = unit_mm;
  bh = 3.0;
  recess = 0.4;
  // Position: left of space bar (PROVISIONAL)
  translate([u(2.2) - fw - 2.5, u(0.0), carrier_t + key_top_h - recess]) {
    color([0.95, 0.95, 0.9, 0.85])
      cube([fw, fd, bh]);
    // black POWER legend placeholder (thin raised bar stand-in)
    color([0.05, 0.05, 0.05])
      translate([0.8, 1.8, bh])
        cube([fw - 1.6, 0.9, 0.15]);
  }
}

/* 0.2 mm nozzle legend test coupon */
module legend_test_coupon() {
  plate_w = 40;
  plate_d = 22;
  translate([0, -40, 0]) {
    color([0.95, 0.95, 0.92])
      cube([plate_w, plate_d, 1.0]);
    // Candidate A strokes 0.25 mm — bar proxies for difficult glyphs
    color([0.2, 0.2, 0.2]) {
      for (i = [0:6])
        translate([3 + i * 5, 4, 1.0])
          cube([0.25, 8, 0.25]);
      // Candidate B strokes 0.35 mm
      for (i = [0:6])
        translate([3 + i * 5, 14, 1.0])
          cube([0.35, 6, 0.25]);
    }
  }
}

/* ESP][ badge carrier — no Apple artwork */
module esp2_badge_carrier() {
  bw = 22;
  bh = 8;
  bt = 1.2;
  recess = 0.25;
  translate([30, -40, 0]) {
    color([0.95, 0.95, 0.95])
      difference() {
        hull() {
          translate([1.5, 1.5, 0]) cylinder(h = bt, r = 1.5);
          translate([bw - 1.5, 1.5, 0]) cylinder(h = bt, r = 1.5);
          translate([1.5, bh - 1.5, 0]) cylinder(h = bt, r = 1.5);
          translate([bw - 1.5, bh - 1.5, 0]) cylinder(h = bt, r = 1.5);
        }
        translate([1.2, 1.2, bt - recess])
          cube([bw - 2.4, bh - 2.4, recess + 0.05]);
      }
  }
}

/* Beige enclosure interface reference plane (not full shell) */
module beige_interface_plane() {
  w = u(15.16) + 2 * env_margin_x + 6;
  d = u(5.48) + 2 * env_margin_y + 6;
  color([0.93, 0.86, 0.7, 0.35])
    translate([-env_margin_x - 3, -env_margin_y - 3, -1.5])
      cube([w, d, 1.2]);
}

/* ---- Assembly preview ---- */
module keyboard_reference_assembly() {
  beige_interface_plane();
  brown_keyboard_one_piece();
  white_legend_placeholders();
  power_indicator();
  legend_test_coupon();
  esp2_badge_carrier();
}

keyboard_reference_assembly();
