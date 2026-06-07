# PixGate firmware

Prebuilt firmware images produced from the per-device configs in [`../devices/`](../devices).

Run [`../build.sh`](../build.sh) from the repo root to (re)generate them:

```bash
./build.sh                 # build every devices/*.yaml
./build.sh wt32-sc01-plus  # build a single device
```

Each device gets its own subfolder here containing:

- `firmware.factory.bin` — full image for first-time flashing (e.g. via ESP Web Tools).
- `firmware.ota.bin` — image for over-the-air updates.
- `firmware.bin` — the raw application image.
