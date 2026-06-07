<script>
  import { widgetSummary } from '../model.js';

  // widgets: the zone's widget entries; types: registry entries addable here; byType: type→entry
  // lookup for summaries; callbacks mutate via the parent so the single config doc stays canonical.
  let { widgets = [], types = [], byType = {}, onAdd, onEdit, onDelete, onMove } = $props();

  let choosing = $state(false);

  function add(t) {
    choosing = false;
    onAdd?.(t);
  }
</script>

<div class="panel">
  {#if widgets.length === 0}
    <div class="empty">No widgets yet.</div>
  {/if}

  {#each widgets as w, i (w.id || `${w.type}-${i}`)}
    <div class="card">
      <div>
        <div class="title">{w.type}</div>
        <div class="sub">{widgetSummary(w, byType[w.type])}</div>
      </div>
      <div class="actions">
        <button type="button" title="Move up" disabled={i === 0} onclick={() => onMove?.(i, -1)}>↑</button>
        <button type="button" title="Move down" disabled={i === widgets.length - 1} onclick={() => onMove?.(i, 1)}>↓</button>
        <button type="button" onclick={() => onEdit?.(i)}>Edit</button>
        <button type="button" class="danger" onclick={() => onDelete?.(i)}>Delete</button>
      </div>
    </div>
  {/each}

  <div style="margin-top:0.5rem">
    {#if choosing}
      <div class="row" style="flex-wrap:wrap">
        {#each types as t (t.type)}
          <button type="button" onclick={() => add(t)}>+ {t.type}</button>
        {:else}
          <span class="muted">No widget types available.</span>
        {/each}
        <button type="button" onclick={() => (choosing = false)}>Cancel</button>
      </div>
    {:else}
      <button type="button" class="primary" onclick={() => (choosing = true)}>Add widget</button>
    {/if}
  </div>
</div>
