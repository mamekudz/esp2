/**
 * Shared CDC line framing for ESP][ development serial.
 *
 * Protocol roles on one CDC (mutually exclusive sessions):
 *   MEDIA_PROTOCOL  — #ESP2UPLOAD + ESPU binary frames
 *   INPUT_PROTOCOL  — #ESP2INPUT … text lines (LIVE mode)
 *   DIAGNOSTIC_OUTPUT — [TAG] … / other device logs (host must ignore)
 *
 * Never treat an incomplete line as an ACK. Retain residual bytes across waits.
 * Pause the line reader while host→device binary MEDIA frames are on the wire.
 */

/**
 * @param {import('serialport').SerialPort} port
 */
export function createLineReader(port) {
  let buf = "";
  /** @type {string[]} */
  const queue = [];
  let paused = false;

  const onData = (d) => {
    if (paused) {
      return;
    }
    buf += d.toString("utf8");
    for (;;) {
      const nl = buf.search(/\r?\n/);
      if (nl < 0) {
        break;
      }
      const line = buf.slice(0, nl);
      buf = buf.slice(nl).replace(/^\r?\n/, "");
      if (line.length > 0) {
        queue.push(line);
      }
    }
    if (buf.length > 65536) {
      buf = buf.slice(-4096);
    }
  };
  port.on("data", onData);

  return {
    /** @returns {string | undefined} */
    shift() {
      return queue.shift();
    },
    clear() {
      queue.length = 0;
      buf = "";
    },
    /**
     * Detach CDC consumer so binary MEDIA frames are not UTF-8 line-parsed.
     * Call resume() before waiting for text ACKs again.
     */
    pause() {
      if (!paused) {
        port.off("data", onData);
        paused = true;
        buf = "";
      }
    },
    resume() {
      if (paused) {
        port.on("data", onData);
        paused = false;
      }
    },
    /**
     * @param {(line: string) => boolean} pred
     * @param {number} timeoutMs
     */
    async waitLine(pred, timeoutMs) {
      const deadline = Date.now() + timeoutMs;
      for (;;) {
        while (queue.length > 0) {
          const line = queue.shift();
          if (line.startsWith("#NAK")) {
            throw new Error(line);
          }
          if (pred(line)) {
            return line;
          }
        }
        if (Date.now() > deadline) {
          throw new Error("ack timeout");
        }
        await new Promise((r) => setTimeout(r, 10));
      }
    },
    /**
     * @param {string} prefix
     * @param {number} timeoutMs
     */
    async waitAck(prefix, timeoutMs) {
      return this.waitLine(
        (line) =>
          line.startsWith(prefix) ||
          line.startsWith("#ESP2UPLOAD DONE") ||
          (prefix.startsWith("#ACK") &&
            line.startsWith("#ACK") &&
            line.includes(prefix.slice(5))),
        timeoutMs,
      );
    },
    dispose() {
      port.off("data", onData);
      paused = true;
      queue.length = 0;
      buf = "";
    },
  };
}

/** Text command prefixes reserved for host→device control. */
export const DevCommand = {
  UploadEnter: "#ESP2UPLOAD",
  UsbMsc: "#ESP2USBMSC",
  InputLive: "#ESP2INPUT LIVE",
  InputIdle: "#ESP2INPUT IDLE",
  InputKey: "#ESP2INPUT KEY",
  InputPad: "#ESP2INPUT PAD",
  DiskMount: "#ESP2DISK MOUNT",
  DiskBoot: "#ESP2DISK BOOT",
  AppleReset: "#ESP2APPLE RESET",
};
