// Light/dark theme for the configuration website itself. Browser-only and independent of the
// device's `display.theme` (which themes the on-device dashboard). Defaults to the OS preference
// until the user makes an explicit choice, which is then remembered in this browser.

const KEY = 'pixgate.uiTheme';

export function osTheme() {
  return window.matchMedia?.('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
}

// The user's explicit choice, or null if they've never picked one.
export function loadUiTheme() {
  const v = localStorage.getItem(KEY);
  return v === 'light' || v === 'dark' ? v : null;
}

export function saveUiTheme(theme) {
  localStorage.setItem(KEY, theme);
}

// Effective theme: explicit choice if set, otherwise follow the OS.
export function resolveUiTheme() {
  return loadUiTheme() ?? osTheme();
}
