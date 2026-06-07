<script>
  import { loadHaCreds, saveHaCreds, clearHaCreds } from '../ha.js';

  // onclose(): dismiss. firstRun: tweak copy for the initial prompt vs. the cog menu.
  // config: live dashboard config (device display settings live here). onApplyDisplay(): persist
  // config to the device; resolves on success, throws on failure.
  // uiTheme/onUiTheme: the browser-only editor theme (independent of the device theme).
  let { onclose, firstRun = false, config = null, onApplyDisplay, uiTheme = 'light', onUiTheme } =
    $props();

  const creds = loadHaCreds();
  let url = $state(creds.url);
  let token = $state(creds.token);

  let saving = $state(false);
  let error = $state('');

  function onKeydown(e) {
    if (e.key === 'Escape') onclose?.();
  }

  async function save() {
    saving = true;
    error = '';
    saveHaCreds(url.trim(), token.trim());
    // Display settings (theme/orientation) live in the device config, so persist them too.
    try {
      if (config) await onApplyDisplay?.();
      onclose?.();
    } catch (e) {
      error = `Could not save display settings to the device: ${e.message}`;
    } finally {
      saving = false;
    }
  }

  function clear() {
    clearHaCreds();
    url = '';
    token = '';
  }
</script>

<svelte:window onkeydown={onKeydown} />

<div class="modal-backdrop">
  <div class="modal">
    <h2>{firstRun ? 'Welcome to PixGate' : 'Settings'}</h2>

    <h3>Editor</h3>
    <p class="muted">
      Light or dark for this configuration website. Saved in this browser only — independent of the
      device's theme. Defaults to your system setting.
    </p>
    <label class="field">Theme
      <select value={uiTheme} onchange={(e) => onUiTheme?.(e.currentTarget.value)}>
        <option value="light">Light</option>
        <option value="dark">Dark</option>
      </select>
    </label>

    {#if config}
      <h3>Display</h3>
      <p class="muted">Appearance of the on-device dashboard.</p>
      <div class="grid2">
        <label class="field">Theme
          <select bind:value={config.display.theme}>
            <option value="light">Light</option>
            <option value="dark">Dark</option>
          </select>
        </label>
        <label class="field">Orientation
          <select bind:value={config.display.orientation}>
            <option value={0}>0° (landscape)</option>
            <option value={90}>90°</option>
            <option value={180}>180°</option>
            <option value={270}>270°</option>
          </select>
        </label>
      </div>
      <p class="muted" style="font-size:0.8rem">
        Note: RGB-panel boards may only support 0°/180° rotation. Saving sends the full
        configuration to the device.
      </p>
    {/if}

    <h3>Home Assistant</h3>
    <p class="muted">
      Optional: connect to Home Assistant so you can browse and pick entities when adding widgets.
      These credentials are stored only in this browser and are never sent to the device. You can
      skip this and type entity IDs manually.
    </p>

    <label class="field">Home Assistant URL
      <input type="text" placeholder="http://homeassistant.local:8123" bind:value={url} />
    </label>
    <label class="field">Long-lived access token
      <input type="password" placeholder="Long-lived access token" bind:value={token} />
    </label>

    {#if error}
      <p class="error-box">{error}</p>
    {/if}

    <div class="footer">
      <button type="button" onclick={clear}>Clear HA</button>
      <span class="spacer"></span>
      <button type="button" onclick={() => onclose?.()} disabled={saving}>
        {firstRun ? 'Skip' : 'Cancel'}
      </button>
      <button type="button" class="primary" onclick={save} disabled={saving}>
        {saving ? 'Saving…' : 'Save'}
      </button>
    </div>
  </div>
</div>
