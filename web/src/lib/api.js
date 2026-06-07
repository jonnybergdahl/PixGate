// Device JSON API client (DESIGN.md §11).
//
// In production the GUI is loaded by the device's own shell page, so the device is the page
// origin and all calls are same-origin (empty base). For local development the SPA runs under
// Vite on localhost, so it needs to be told which device to talk to — via `?host=<ip>` in the
// URL or the VITE_DEVICE_HOST env var. A resolved host is remembered in localStorage.

const HOST_KEY = 'pixgate.deviceHost';

function resolveBase() {
  const fromQuery = new URLSearchParams(location.search).get('host');
  if (fromQuery) {
    localStorage.setItem(HOST_KEY, fromQuery);
    return normalize(fromQuery);
  }
  const fromEnv = import.meta.env.VITE_DEVICE_HOST;
  if (fromEnv) return normalize(fromEnv);
  const stored = localStorage.getItem(HOST_KEY);
  if (stored && import.meta.env.DEV) return normalize(stored);
  // Production: served by the device, same origin.
  return '';
}

function normalize(host) {
  if (/^https?:\/\//i.test(host)) return host.replace(/\/$/, '');
  return `http://${host.replace(/\/$/, '')}`;
}

const base = resolveBase();

export const deviceHost = base || location.origin;

async function getJson(path) {
  const res = await fetch(`${base}${path}`, { headers: { Accept: 'application/json' } });
  if (!res.ok) throw new Error(`${path} → HTTP ${res.status}`);
  return res.json();
}

export const getDevice = () => getJson('/api/device');
export const getRegistry = () => getJson('/api/registry');
export const getConfig = () => getJson('/api/config');
export const getIcons = () => getJson('/api/icons').catch(() => []);

export async function putConfig(config) {
  // POST, not PUT: ESPHome's ESP-IDF web server only registers GET/POST/OPTIONS handlers,
  // so a PUT is rejected with 405 before reaching the device's handler.
  const res = await fetch(`${base}/api/config`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(config),
  });
  if (!res.ok) {
    let detail = `HTTP ${res.status}`;
    try {
      const body = await res.json();
      if (body && body.error) detail = body.error;
    } catch (_) { /* ignore non-JSON error bodies */ }
    throw new Error(detail);
  }
  return true;
}
