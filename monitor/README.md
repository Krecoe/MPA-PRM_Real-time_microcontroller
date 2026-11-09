# BLE monitoring station (Web Bluetooth)

A bedside-style monitor: it connects to the board over Bluetooth Low Energy, shows the current
state and a timeline of states, and raises an alarm when fast breathing or "not attached" lasts too
long, when data stops arriving, or when the connection is lost.

Files: `index.html` (markup), `app.js` (all logic), `style.css`. No build step, no dependencies.

## Why it cannot live on the board

Web Bluetooth only exists in a **secure context** - `https://` or `http://localhost`. The page the
board serves over its own Wi-Fi hotspot is plain `http://` on a LAN IP, so the browser hides
`navigator.bluetooth` there. This monitor therefore has to be published on GitHub Pages from a
**public** repository. (The board's own page is a separate deliverable and stays on the board.)

## Publish on GitHub Pages

Works from this repository if it is public, otherwise create a small public repo and copy this
folder to its root:

```bash
# option A - this repository is public: Pages serves /monitor
# Settings -> Pages -> Source: Deploy from a branch -> branch main, folder /monitor

# option B - separate public repository (recommended, keeps the project repo private)
cd monitor
git init -b main
git add index.html app.js style.css README.md
git commit -m "BLE monitoring station"
gh repo create mpa-prm-monitor --public --source=. --push
# then: Settings -> Pages -> Source: Deploy from a branch -> main, root
```

Pages gives you `https://<user>.github.io/<repo>/` in a minute or two. That URL is what goes in the
project README - the README must link the monitoring page.

Open that URL over **https** (or run the folder on `http://localhost` for development):

```bash
python -m http.server 8000   # then open http://localhost:8000
```

## Browsers

Chrome or Edge on Windows, macOS or Android. Firefox and Safari do not support Web Bluetooth, and
Chrome on iPhone does not either. The machine running the page needs Bluetooth LE - this does not
have to be the computer powering or flashing the board, since BLE is wireless and USB carries no
data anyway.

## Using it

1. **Connect board** - Chrome's device picker opens (Web Bluetooth must be started from a click).
   Pick `ESP32-BREATH`. If it is not listed, check nRF Connect first and see the advertisement note
   in [docs/BLE_PROTOCOL.md](../docs/BLE_PROTOCOL.md).
2. The big card shows the current state and how long it has held; bars underneath show class
   probabilities if the firmware sends them.
3. The timeline covers the last 5 minutes.
4. Alarms: full-screen flashing overlay plus a repeating tone. **Silence** stops the tone.

## Alarm limits

| Alarm | Default | Rationale |
|-------|---------|-----------|
| Fast breathing too long | 30 s | a genuinely fast respiratory rate that holds for half a minute is the finding worth flagging |
| Not attached too long | 60 s | the sensor has clearly fallen off or was never placed |
| No data while connected | 5 s | needs a >=1 s heartbeat notify from the firmware, see the protocol |
| Connection lost | immediate | a monitor that has gone quiet must never fail silently |

All four are editable in the UI and can be disabled individually. **Before the demo, drop the two
duration limits to a few seconds** so you can trigger them on cue instead of holding your breath for
the teacher.

## Testing without hardware

```
index.html?simulate=scenario&fast=4&notAttached=5   mixed scenario, timeline + both duration alarms
index.html?simulate=fast&fast=3&drop=6              fast-breathing alarm, then connection-lost at 6 s
index.html?simulate=not_attached&notAttached=4      the other duration alarm on its own
```

`simulate` = `scenario` or one state name, `fast` / `notAttached` / `stale` = limits in seconds.

Verified in headless Chrome on 2026-11-09: page loads with no console errors, both duration alarms
and the connection-lost alarm fire, timeline segments and per-class totals populate. Not yet verified
against real firmware - see the checklist below.

## Checklist against the real board

- [ ] Firmware sends the service UUID in the advertisement (device appears in Chrome's picker)
- [ ] Notifications arrive and the state card changes colour
- [ ] No false "No data" alarm while the state stays constant for a minute
- [ ] Pull the board's power (or walk out of range) -> connection-lost alarm, then automatic reconnect
- [ ] Works from the published https URL, not only localhost
