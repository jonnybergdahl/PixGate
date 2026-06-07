<script>
  import EntityPicker from './EntityPicker.svelte';

  // field: a ConfigField from /api/registry schema; value: current value; domains: widget's HA
  // domains (used for the ENTITY field's picker); onchange(newValue) writes back.
  let { field, value, domains = [], icons = [], onchange } = $props();

  function set(v) {
    onchange?.(field.key, v);
  }
</script>

<div class="field">
  <!-- svelte-ignore a11y_label_has_associated_control -->
  <!-- The associated control is rendered conditionally below by field type. -->
  <label>
    {field.label || field.key}
    {#if field.required}<span class="req">*</span>{/if}
  </label>

  {#if field.type === 'entity'}
    <EntityPicker {value} {domains} onchange={(v) => set(v)} />
  {:else if field.type === 'bool'}
    <input type="checkbox" checked={!!value} onchange={(e) => set(e.currentTarget.checked)} />
  {:else if field.type === 'int'}
    <input
      type="number"
      value={value ?? ''}
      oninput={(e) => set(e.currentTarget.value === '' ? '' : Number(e.currentTarget.value))}
    />
  {:else if field.type === 'enum'}
    <select value={value ?? ''} onchange={(e) => set(e.currentTarget.value)}>
      <option value="" disabled>Choose…</option>
      {#each field.options || [] as opt}
        <option value={opt}>{opt}</option>
      {/each}
    </select>
  {:else if field.type === 'color'}
    <div class="row">
      <input
        type="color"
        value={value || '#000000'}
        oninput={(e) => set(e.currentTarget.value)}
        style="max-width:3rem"
      />
      <input type="text" value={value ?? ''} oninput={(e) => set(e.currentTarget.value)} />
    </div>
  {:else if field.type === 'icon'}
    <input
      type="text"
      list="pixgate-icons"
      placeholder="mdi:lightbulb"
      value={value ?? ''}
      oninput={(e) => set(e.currentTarget.value)}
    />
    {#if icons.length}
      <datalist id="pixgate-icons">
        {#each icons as ic}<option value={ic}></option>{/each}
      </datalist>
    {/if}
  {:else}
    <input type="text" value={value ?? ''} oninput={(e) => set(e.currentTarget.value)} />
  {/if}
</div>
