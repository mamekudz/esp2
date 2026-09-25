# IMU calibration model (no tilt-to-joystick yet)

Future graphical workflow. Do **not** implement mapping in this milestone.

## Steps

1. neutral  
2. tilt left  
3. tilt right  
4. tilt forward  
5. tilt backward  
6. rotate CW  
7. rotate CCW  

For each step:

- show requested movement (i18x)
- visualize detected motion
- capture raw QMI8658 values (byte-wise reads — known-good)
- user presses OK / NOT_OK
- store into diagnostic session JSON

## Inference targets (automatic later)

- swap XY
- invert X
- invert Y
- physical enclosure orientation

Users must not interpret raw numbers.

## Hardware note

QMI8658 multi-byte ADDR_AI burst reads were incorrect on this board; keep
per-register byte reads until proven otherwise.
