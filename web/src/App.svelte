<script>
  import { getDevice, getRegistry, getConfig, getIcons, putConfig, deviceHost } from './lib/api.js';
  import { shouldPromptHa, markHaPrompted } from './lib/ha.js';
  import { newEntry, deepClone } from './lib/model.js';
  import Zone from './lib/components/Zone.svelte';
  import WidgetEditor from './lib/components/WidgetEditor.svelte';
  import Settings from './lib/components/Settings.svelte';

  let loading = $state(true);
  let loadError = $state('');
  let device = $state(null);
  let registry = $state([]);
  let icons = $state([]);
  let config = $state(null);

  let tab = $state('header'); // 'header' | 'badges' | 'page'
  let pageIndex = $state(0);

  let saving = $state(false);
  let saveMsg = $state(null); // { ok, text }
  let editor = $state(null); // { registryEntry, entry, isNew, target }

  let showSettings = $state(false);
  let settingsFirstRun = $state(false);

  function openSettings() {
    settingsFirstRun = false;
    showSettings = true;
  }

  function closeSettings() {
    showSettings = false;
    settingsFirstRun = false;
  }

  let systemTypes = $derived(registry.filter((t) => !t.domains || t.domains.length === 0));
  let entityTypes = $derived(registry.filter((t) => t.domains && t.domains.length > 0));
  let byType = $derived(Object.fromEntries(registry.map((t) => [t.type, t])));

  // Widgets array for the currently selected zone (a live reference into the config proxy).
  let currentWidgets = $derived(
    tab === 'header'
      ? config?.header?.widgets
      : tab === 'badges'
        ? config?.badges?.widgets
        : config?.pages?.[pageIndex]?.widgets
  );
  let currentTypes = $derived(tab === 'page' ? entityTypes : systemTypes);

  async function loadAll() {
    loading = true;
    loadError = '';
    try {
      const [d, r, c, ic] = await Promise.all([getDevice(), getRegistry(), getConfig(), getIcons()]);
      device = d;
      registry = r;
      icons = ic;
      // Defensive defaults so the editor never dereferences missing zones.
      c.header ??= { widgets: [] };
      c.badges ??= { widgets: [] };
      c.pages ??= [{ name: 'Home', columns: 4, widgets: [] }];
      config = c;
    } catch (e) {
      loadError = e.message;
    } finally {
      loading = false;
    }
  }

  function openAdd(registryEntry) {
    editor = {
      registryEntry,
      entry: newEntry(registryEntry, config, pageIndex),
      isNew: true,
      target: { tab, pageIndex },
    };
  }

  function openEdit(index) {
    const w = currentWidgets[index];
    editor = {
      registryEntry: byType[w.type],
      entry: deepClone(w),
      isNew: false,
      target: { tab, pageIndex, index },
    };
  }

  function onEditorSave(entry) {
    const arr = currentWidgets;
    if (editor.isNew) arr.push(entry);
    else arr[editor.target.index] = entry;
    editor = null;
    saveMsg = null;
  }

  function deleteWidget(index) {
    currentWidgets.splice(index, 1);
  }

  function moveWidget(index, dir) {
    const arr = currentWidgets;
    const j = index + dir;
    if (j < 0 || j >= arr.length) return;
    [arr[index], arr[j]] = [arr[j], arr[index]];
  }

  async function save() {
    saving = true;
    saveMsg = null;
    try {
      await putConfig(config);
      saveMsg = { ok: true, text: 'Saved and applied.' };
    } catch (e) {
      saveMsg = { ok: false, text: `Rejected: ${e.message}` };
    } finally {
      saving = false;
    }
  }

  loadAll();

  // First visit with no HA credentials: prompt for them once.
  if (shouldPromptHa()) {
    settingsFirstRun = true;
    showSettings = true;
    markHaPrompted();
  }
</script>

<div class="app">
  <div class="topbar">
    <div>
      <h1>PixGate</h1>
      <div class="meta">
        {#if device}{device.name} · v{device.version}{#if device.width} · {device.width}×{device.height}{/if}{/if}
        <span> · {deviceHost}</span>
      </div>
    </div>
    <div class="row">
      {#if saveMsg}
        <span class="status {saveMsg.ok ? 'ok' : 'err'}">{saveMsg.text}</span>
      {/if}
      <button class="primary" onclick={save} disabled={saving || !config}>
        {saving ? 'Saving…' : 'Save & apply'}
      </button>
      <button class="cog" title="Settings" aria-label="Settings" onclick={openSettings}>⚙</button>
    </div>
  </div>

  {#if loading}
    <div class="empty">Loading…</div>
  {:else if loadError}
    <div class="error-box">
      Could not load from the device: {loadError}
      <div style="margin-top:0.5rem"><button onclick={loadAll}>Retry</button></div>
    </div>
  {:else if config}
    <div class="tabs">
      <button class={tab === 'header' ? 'active' : ''} onclick={() => (tab = 'header')}>Header</button>
      <button class={tab === 'badges' ? 'active' : ''} onclick={() => (tab = 'badges')}>Badges</button>
      <button class={tab === 'page' ? 'active' : ''} onclick={() => (tab = 'page')}>Main page</button>
    </div>

    {#if tab === 'page'}
      <label class="row muted" style="margin-bottom:0.75rem">Columns
        <input
          type="number"
          min="1"
          style="max-width:5rem"
          bind:value={config.pages[pageIndex].columns}
        />
      </label>
    {/if}

    <Zone
      widgets={currentWidgets}
      types={currentTypes}
      {byType}
      onAdd={openAdd}
      onEdit={openEdit}
      onDelete={deleteWidget}
      onMove={moveWidget}
    />
  {/if}
</div>

{#if editor}
  <WidgetEditor
    registryEntry={editor.registryEntry}
    entry={editor.entry}
    {icons}
    onsave={onEditorSave}
    oncancel={() => (editor = null)}
  />
{/if}

{#if showSettings}
  <Settings firstRun={settingsFirstRun} onclose={closeSettings} />
{/if}
