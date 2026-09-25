# Diagnostic session model

Interactive diagnostics gather technical data automatically. The user
preferably answers only:

    OK
    NOT_OK

## Session record (language-neutral JSON)

Path (planned on SD): `/diagnostics/<sessionId>.json`

```json
{
  "schemaVersion": 1,
  "sessionId": "boot-00000001",
  "firmwareVersion": "0.0.0-bringup",
  "hardwareId": "esp32s3-mac-...",
  "startedMs": 12345,
  "tests": [
    {
      "type": "imu_orientation",
      "steps": [
        {
          "id": "tilt_left",
          "raw": { "ax": 0, "ay": 0, "az": 0 },
          "interpreted": { "axis": "x", "sign": -1 },
          "user": "OK"
        }
      ],
      "result": "pass"
    }
  ]
}
```

## Rules

- Field names and enums stay English/neutral for agents.
- UI labels use i18x (`diagnostic.ok`, `diagnostic.not_ok`).
- No RTC required; use monotonic ms + session counter.
