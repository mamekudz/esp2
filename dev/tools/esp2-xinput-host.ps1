# ESP][ host gamepad helper — list | wait | rumble
param(
  [Parameter(Mandatory = $true)][ValidateSet("list", "wait", "rumble")][string]$Mode,
  [int]$TimeoutMs = 25000,
  [int]$Index = 0,
  [int]$RumbleMs = 350
)

$ErrorActionPreference = "Continue"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Esp2XiHost {
  [StructLayout(LayoutKind.Sequential)]
  public struct GAMEPAD {
    public ushort wButtons;
    public byte bLeftTrigger;
    public byte bRightTrigger;
    public short sThumbLX;
    public short sThumbLY;
    public short sThumbRX;
    public short sThumbRY;
  }
  [StructLayout(LayoutKind.Sequential)]
  public struct STATE {
    public uint dwPacketNumber;
    public GAMEPAD Gamepad;
  }
  [StructLayout(LayoutKind.Sequential)]
  public struct VIBRATION {
    public ushort wLeftMotorSpeed;
    public ushort wRightMotorSpeed;
  }
  [DllImport("xinput1_4.dll", EntryPoint = "XInputGetState")]
  public static extern int GetState14(int dwUserIndex, out STATE pState);
  [DllImport("xinput1_3.dll", EntryPoint = "XInputGetState")]
  public static extern int GetState13(int dwUserIndex, out STATE pState);
  [DllImport("xinput1_4.dll", EntryPoint = "XInputSetState")]
  public static extern int SetState14(int dwUserIndex, ref VIBRATION pVibration);

  public static int GetState(int i, out STATE s) {
    int r = GetState14(i, out s);
    if (r == 0) return 0;
    try { return GetState13(i, out s); } catch { return r; }
  }
}
"@ | Out-Null

function Get-XiSlots {
  $rows = @()
  for ($i = 0; $i -lt 4; $i++) {
    $st = New-Object Esp2XiHost+STATE
    $r = [Esp2XiHost]::GetState($i, [ref]$st)
    $connected = ($r -eq 0)
    $btn = 0; $lt = 0; $rt = 0
    if ($connected) {
      $btn = [int]$st.Gamepad.wButtons
      $lt = [int]$st.Gamepad.bLeftTrigger
      $rt = [int]$st.Gamepad.bRightTrigger
    }
    $rows += [pscustomobject]@{
      index = $i; connected = $connected; buttons = $btn; lt = $lt; rt = $rt; api = "xinput"
    }
  }
  return $rows
}

function Get-ProductNames {
  $names = @()
  try {
    $names = @(
      Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
        Where-Object {
          $_.FriendlyName -match 'Xbox|8BitDo|8Bitdo|Gamepad|DualShock|DualSense|Wireless Controller|Game Controller|Pro Controller|SN30|Joy-Con|Fire Game|Bluetooth LE XINPUT' -and
          $_.FriendlyName -notmatch 'ACPI|Embedded|Eingebettet|Audio|Headset|Vendor-defined|vom Hersteller|Composite'
        } |
        Select-Object -ExpandProperty FriendlyName -Unique
    )
  } catch {}
  return @($names)
}

function Get-RawControllers {
  $rows = @()
  try {
    $null = [Windows.Gaming.Input.RawGameController, Windows.Gaming.Input, ContentType = WindowsRuntime]
    $list = [Windows.Gaming.Input.RawGameController]::RawGameControllers
    $n = 0
    foreach ($c in $list) {
      $btnCount = [int]$c.ButtonCount
      $buttons = New-Object bool[] $btnCount
      $switches = New-Object Windows.Gaming.Input.GameControllerSwitchPosition[] ([Math]::Max(1, [int]$c.SwitchCount))
      $axes = New-Object double[] ([Math]::Max(1, [int]$c.AxisCount))
      try { [void]$c.GetCurrentReading($buttons, $switches, $axes) } catch {}
      $any = $false
      foreach ($b in $buttons) { if ($b) { $any = $true; break } }
      $rows += [pscustomobject]@{
        index = $n
        name = [string]$c.DisplayName
        buttonCount = $btnCount
        pressed = $any
        api = "raw"
      }
      $n++
    }
  } catch {}
  return @($rows)
}

if ($Mode -eq "list") {
  $best = Get-XiSlots
  Start-Sleep -Milliseconds 150
  $second = Get-XiSlots
  for ($i = 0; $i -lt 4; $i++) {
    if ($second[$i].connected) { $best[$i] = $second[$i] }
  }
  $names = Get-ProductNames
  $ni = 0
  foreach ($s in $best) {
    $product = "XInput#$($s.index)"
    if ($s.connected -and $ni -lt $names.Count) {
      $product = [string]$names[$ni]
      $ni++
    }
    $conn = if ($s.connected) { 1 } else { 0 }
    Write-Output ("xi|{0}|{1}|{2}|{3}|{4}|{5}" -f $s.index, $product, $(if ($s.connected) {6} else {0}), $(if ($s.connected) {16} else {0}), $conn, $s.buttons)
  }
  foreach ($name in $names) {
    Write-Output ("pnp|{0}" -f $name)
  }
  foreach ($r in (Get-RawControllers)) {
    $p = if ($r.pressed) { 1 } else { 0 }
    Write-Output ("raw|{0}|{1}|{2}|{3}" -f $r.index, $r.name, $r.buttonCount, $p)
  }
  exit 0
}

if ($Mode -eq "rumble") {
  try {
    $v = New-Object Esp2XiHost+VIBRATION
    $v.wLeftMotorSpeed = 25000
    $v.wRightMotorSpeed = 25000
    [void][Esp2XiHost]::SetState14($Index, [ref]$v)
    Start-Sleep -Milliseconds ([Math]::Max(80, [Math]::Min(2000, $RumbleMs)))
    $v.wLeftMotorSpeed = 0
    $v.wRightMotorSpeed = 0
    [void][Esp2XiHost]::SetState14($Index, [ref]$v)
    Write-Output ("rumble {0} ok" -f $Index)
  } catch {
    Write-Output ("rumble {0} fail" -f $Index)
  }
  exit 0
}

if ($Mode -eq "wait") {
  $deadline = [Environment]::TickCount64 + [Math]::Max(1000, $TimeoutMs)
  $prevPressed = @(0, 0, 0, 0)
  $prevConnected = @($false, $false, $false, $false)
  $rawPrev = @{}
  $slots = Get-XiSlots
  for ($i = 0; $i -lt 4; $i++) {
    $prevConnected[$i] = [bool]$slots[$i].connected
    $pressed = ($slots[$i].buttons -ne 0) -or ($slots[$i].lt -gt 30) -or ($slots[$i].rt -gt 30)
    $prevPressed[$i] = if ($pressed) { 1 } else { 0 }
  }
  foreach ($r in (Get-RawControllers)) {
    $rawPrev[$r.index] = if ($r.pressed) { 1 } else { 0 }
  }

  while ([Environment]::TickCount64 -lt $deadline) {
    $slots = Get-XiSlots
    for ($i = 0; $i -lt 4; $i++) {
      $s = $slots[$i]
      if (-not $s.connected) {
        $prevConnected[$i] = $false
        $prevPressed[$i] = 0
        continue
      }
      $pressedNow = ($s.buttons -ne 0) -or ($s.lt -gt 30) -or ($s.rt -gt 30)
      $woke = (-not $prevConnected[$i]) -and $pressedNow
      $edge = ($prevPressed[$i] -eq 0) -and $pressedNow
      if ($edge -or $woke) {
        Write-Output ("press {0} {1} xinput" -f $i, $s.buttons)
        exit 0
      }
      $prevConnected[$i] = $true
      $prevPressed[$i] = if ($pressedNow) { 1 } else { 0 }
    }

    foreach ($r in (Get-RawControllers)) {
      $was = 0
      if ($rawPrev.ContainsKey($r.index)) { $was = [int]$rawPrev[$r.index] }
      $now = if ($r.pressed) { 1 } else { 0 }
      if ($now -eq 1 -and $was -eq 0) {
        # Prefer matching XInput slot if one is connected; else use raw index capped 0..3
        $map = $r.index
        if ($map -gt 3) { $map = 0 }
        $xi = Get-XiSlots
        for ($i = 0; $i -lt 4; $i++) {
          if ($xi[$i].connected) { $map = $i; break }
        }
        $safeName = ($r.name -replace '[|\\r\\n]', ' ')
        Write-Output ("press {0} 1 raw {1}" -f $map, $safeName)
        exit 0
      }
      $rawPrev[$r.index] = $now
    }
    Start-Sleep -Milliseconds 40
  }
  Write-Output "timeout"
  exit 0
}
