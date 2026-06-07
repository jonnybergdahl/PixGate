// Browser-side Home Assistant entity discovery (DESIGN.md §11, "the entity-picker problem").
//
// The device does not know HA's entity list, so the *browser* fetches it directly from HA using
// a user-supplied base URL + long-lived access token. These credentials live only in the
// browser (localStorage) and never touch the device. HA must allow this origin via its
// `http: cors_allowed_origins:` config, otherwise the request is blocked by CORS — in which
// case the user falls back to manual entity_id entry.

const URL_KEY = 'pixgate.haUrl';
const TOKEN_KEY = 'pixgate.haToken';

export function loadHaCreds() {
  return {
    url: localStorage.getItem(URL_KEY) || '',
    token: localStorage.getItem(TOKEN_KEY) || '',
  };
}

export function saveHaCreds(url, token) {
  localStorage.setItem(URL_KEY, url.replace(/\/$/, ''));
  localStorage.setItem(TOKEN_KEY, token);
}

export function clearHaCreds() {
  localStorage.removeItem(URL_KEY);
  localStorage.removeItem(TOKEN_KEY);
}

// Fetch all entity states from HA. Returns [{ entity_id, friendly_name, domain, state }].
export async function fetchEntities({ url, token }) {
  if (!url || !token) throw new Error('Home Assistant URL and token are required.');
  const base = url.replace(/\/$/, '');
  let res;
  try {
    res = await fetch(`${base}/api/states`, {
      headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' },
    });
  } catch (e) {
    throw new Error(
      'Could not reach Home Assistant. Check the URL and that HA allows this origin ' +
        '(http: cors_allowed_origins). You can still enter an entity_id manually.'
    );
  }
  if (res.status === 401) throw new Error('Home Assistant rejected the token (401).');
  if (!res.ok) throw new Error(`Home Assistant returned HTTP ${res.status}.`);
  const states = await res.json();
  return states.map((s) => ({
    entity_id: s.entity_id,
    domain: s.entity_id.split('.')[0],
    friendly_name: (s.attributes && s.attributes.friendly_name) || s.entity_id,
    state: s.state,
  }));
}

// Keep entities whose domain is in `domains` (empty/undefined → keep all).
export function filterByDomains(entities, domains) {
  if (!domains || domains.length === 0) return entities;
  const set = new Set(domains);
  return entities.filter((e) => set.has(e.domain));
}
