<script>
  import { loadHaCreds, haConfigured, fetchEntities, filterByDomains } from '../ha.js';

  // value: current entity_id string; domains: allowed HA domains for this widget type.
  // Home Assistant credentials are managed globally in Settings (the cog), not here.
  let { value = '', domains = [], onchange } = $props();

  let expanded = $state(false);
  let loading = $state(false);
  let error = $state('');
  let entities = $state([]);
  let filter = $state('');

  let filtered = $derived(
    filterByDomains(entities, domains).filter((e) => {
      const q = filter.trim().toLowerCase();
      if (!q) return true;
      return e.entity_id.toLowerCase().includes(q) || e.friendly_name.toLowerCase().includes(q);
    })
  );

  async function load() {
    loading = true;
    error = '';
    try {
      entities = await fetchEntities(loadHaCreds());
    } catch (e) {
      error = e.message;
    } finally {
      loading = false;
    }
  }

  function pick(id) {
    onchange?.(id);
  }
</script>

<div class="row">
  <input
    type="text"
    placeholder="entity_id (e.g. light.living_room)"
    value={value}
    oninput={(e) => onchange?.(e.currentTarget.value)}
  />
  <button type="button" onclick={() => (expanded = !expanded)}>
    {expanded ? 'Hide HA' : 'From HA'}
  </button>
</div>

{#if expanded}
  <div class="panel" style="margin-top:0.5rem">
    {#if !haConfigured()}
      <p class="muted">
        Set your Home Assistant URL and token in Settings (the ⚙ in the top-right) to browse
        entities here.
      </p>
    {:else}
      <div class="row">
        <button type="button" onclick={load} disabled={loading}>
          {loading ? 'Loading…' : 'Load entities'}
        </button>
        <span class="muted">{domains.length ? `domains: ${domains.join(', ')}` : 'all domains'}</span>
      </div>

      {#if error}
        <p class="error-box" style="margin-top:0.5rem">{error}</p>
      {/if}

      {#if entities.length}
        <input
          type="text"
          placeholder="Filter…"
          bind:value={filter}
          style="margin-top:0.5rem"
        />
        <div class="entity-list" style="margin-top:0.5rem">
          {#each filtered as e (e.entity_id)}
            <button type="button" onclick={() => pick(e.entity_id)}>
              <strong>{e.friendly_name}</strong>
              <span class="muted"> — {e.entity_id}</span>
            </button>
          {:else}
            <div class="empty">No matching entities.</div>
          {/each}
        </div>
      {/if}
    {/if}
  </div>
{/if}
