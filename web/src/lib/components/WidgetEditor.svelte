<script>
  import { untrack } from 'svelte';
  import Field from './Field.svelte';
  import { deepClone } from '../model.js';

  // registryEntry: the type's registry info ({type, domains, schema}); entry: the widget being
  // added/edited; onPage: whether this widget lives on the page grid (and therefore has a cell);
  // icons: available icon names; onsave(entry)/oncancel() callbacks. The component is mounted
  // fresh each time the editor opens, so snapshotting the props once is intentional.
  let { registryEntry, entry, onPage = false, icons = [], onsave, oncancel } = $props();

  // Work on a clone so Cancel discards cleanly. Grid placement is a property of the page grid,
  // not of the widget type — a zone widget that happens to bind an entity (clock, badge) has no
  // cell, so only seed/show one for page widgets.
  let draft = $state(
    untrack(() => {
      const d = deepClone(entry);
      if (onPage && !d.cell) d.cell = { col: 0, row: 0, col_span: 1, row_span: 1 };
      return d;
    })
  );
  let error = $state('');

  function onKeydown(e) {
    if (e.key === 'Escape') oncancel?.();
  }

  function setCfg(key, value) {
    draft.cfg[key] = value;
  }

  function missingRequired() {
    return (registryEntry.schema || [])
      .filter((f) => f.required)
      .filter((f) => {
        const v = draft.cfg[f.key];
        return v === undefined || v === '' || v === null;
      })
      .map((f) => f.label || f.key);
  }

  function save() {
    const missing = missingRequired();
    if (missing.length) {
      error = `Required: ${missing.join(', ')}`;
      return;
    }
    onsave?.(deepClone(draft));
  }
</script>

<svelte:window onkeydown={onKeydown} />

<div class="modal-backdrop">
  <div class="modal">
    <h2>{entry.id ? 'Edit' : 'Add'} {registryEntry.type} widget</h2>

    {#each registryEntry.schema || [] as field (field.key)}
      <Field
        {field}
        value={draft.cfg[field.key]}
        domains={registryEntry.domains}
        {icons}
        onchange={setCfg}
      />
    {/each}

    {#if onPage && draft.cell}
      <fieldset style="border:1px solid var(--border);border-radius:8px;padding:0.75rem">
        <legend class="muted">Grid placement</legend>
        <div class="grid2">
          <label class="field">Column
            <input type="number" min="0" bind:value={draft.cell.col} />
          </label>
          <label class="field">Row
            <input type="number" min="0" bind:value={draft.cell.row} />
          </label>
          <label class="field">Column span
            <input type="number" min="1" bind:value={draft.cell.col_span} />
          </label>
          <label class="field">Row span
            <input type="number" min="1" bind:value={draft.cell.row_span} />
          </label>
        </div>
      </fieldset>
    {/if}

    {#if error}<p class="status err">{error}</p>{/if}

    <div class="footer">
      <button type="button" onclick={() => oncancel?.()}>Cancel</button>
      <button type="button" class="primary" onclick={save}>Done</button>
    </div>
  </div>
</div>
