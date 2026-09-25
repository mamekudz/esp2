# Module boundaries

## AppleIIMachine (conceptual)

Platform-neutral façade. Exact names may evolve; contracts must not expose
ESP32 types.

```
class AppleIIMachine {
  void reset();                         // Apple II only; disks stay mounted
  void runCycles(uint32_t cycles);
  void keyDown(NormalizedKey key);
  void keyUp(NormalizedKey key);
  void setJoystick(JoystickState state);
  void setPaddle(PaddleState state);
  MediaResult mountDisk(DriveId id, DiskImage* image);
  MediaResult unmountDisk(DriveId id);
  VideoFrameState getVideoState() const;
  size_t consumeAudioEvents(SpeakerEvent* out, size_t max);
};
```

## Video pipeline stages

1. `VideoMemoryView` — Apple II text / LoRes / HiRes buffers + soft switches
2. `VideoDecoder` — logical pixels / bit stream (phase-aware for HGR)
3. `ColorRenderer` — `CompositeColor` | mono white/green/amber
4. `DisplayEffect` — Sharp | Monitor | CrtTv × Off/Low/Medium/High
5. `Viewport` — LandscapeStandalone | PortraitApple2Case | ControlScreen
6. `DisplaySink` — physical CO5300 transfer (firmware only)

Monochrome modes **must** render from decoder mono info, never by
desaturating composite RGB.

## Input pipeline

```
raw HID / IMU / touch / paddles
        |
   transport adapters
        |
   normalized state
        |
   +----+----------------+
   |                     |
 emulator hotkeys     Apple II keyboard / joystick
```

Hotkeys are intercepted **before** Apple II software sees keys.

## Media pipeline

```
Apple II  ->  Disk II card  ->  VirtualDrive[1|2]  ->  DiskImage  ->  StorageBackend
```

StorageBackend for V1: microSD (Arduino `SD` wrapped). Disk II must not call
`SD.open` directly.

## Reset semantics

| Action | Effect |
| --- | --- |
| Apple II reset | Emulated machine reset; mounted images remain |
| ESP32 restart | Full firmware reboot |

## BlueShift boundary

ESP][ must treat BlueShift as a BLE HID peripheral. No BlueShift-specific
opcodes, pairing secrets, or Classic-BT bridging code belong in this repo.
