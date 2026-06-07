<script>
  import { loadHaCreds, saveHaCreds, clearHaCreds } from '../ha.js';

  // onclose(): dismiss the dialog. firstRun: tweak copy for the initial prompt vs. the cog menu.
  let { onclose, firstRun = false } = $props();

  const creds = loadHaCreds();
  let url = $state(creds.url);
  let token = $state(creds.token);

  function onKeydown(e) {
    if (e.key === 'Escape') onclose?.();
  }

  function save() {
    saveHaCreds(url.trim(), token.trim());
    onclose?.();
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

    <div class="footer">
      <button type="button" onclick={clear}>Clear</button>
      <span class="spacer"></span>
      <button type="button" onclick={() => onclose?.()}>{firstRun ? 'Skip' : 'Cancel'}</button>
      <button type="button" class="primary" onclick={save}>Save</button>
    </div>
  </div>
</div>
