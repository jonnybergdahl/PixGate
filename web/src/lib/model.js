// Helpers for manipulating the dashboard config document the device exchanges over /api/config.
// Document shape (DESIGN.md §9):
//   { schema_version, header:{widgets:[]}, badges:{widgets:[]},
//     pages:[{name,columns,widgets:[{id,type,cell,cfg}]}] }
// System widgets (header/badges) are stored as { type, cfg }; entity widgets (page grid) add an
// `id` and a `cell`. A widget type is a "system" type when its registry `domains` list is empty.

export function isSystemType(registryEntry) {
  return !registryEntry.domains || registryEntry.domains.length === 0;
}

// Build a cfg object seeded with each schema field's default.
export function defaultCfg(schema) {
  const cfg = {};
  for (const f of schema || []) {
    if (f.default !== undefined && f.default !== '') {
      cfg[f.key] = f.type === 'bool' ? f.default === 'true' : f.default;
    } else if (f.type === 'bool') {
      cfg[f.key] = false;
    }
  }
  return cfg;
}

// Next free entity-widget id ("w1", "w2", ...) across all pages.
export function nextWidgetId(config) {
  let max = 0;
  for (const page of config.pages || []) {
    for (const w of page.widgets || []) {
      const m = /^w(\d+)$/.exec(w.id || '');
      if (m) max = Math.max(max, parseInt(m[1], 10));
    }
  }
  return `w${max + 1}`;
}

// Row-major cell for the Nth widget on a page of `columns` columns.
export function autoCell(index, columns) {
  const cols = Math.max(1, columns || 4);
  return { col: index % cols, row: Math.floor(index / cols), col_span: 1, row_span: 1 };
}

// A fresh widget entry for the given registry type, ready to edit.
export function newEntry(registryEntry, config, pageIndex) {
  const cfg = defaultCfg(registryEntry.schema);
  if (isSystemType(registryEntry)) {
    return { type: registryEntry.type, cfg };
  }
  const page = config.pages[pageIndex];
  const index = (page.widgets || []).length;
  return {
    id: nextWidgetId(config),
    type: registryEntry.type,
    cell: autoCell(index, page.columns),
    cfg,
  };
}

export function deepClone(obj) {
  return JSON.parse(JSON.stringify(obj));
}

// Short human summary of a widget entry for the list row.
export function widgetSummary(entry, registryEntry) {
  const bits = [];
  if (entry.cfg && entry.cfg.entity_id) bits.push(entry.cfg.entity_id);
  if (entry.cfg && entry.cfg.label) bits.push(`“${entry.cfg.label}”`);
  if (!bits.length && registryEntry) bits.push(registryEntry.type);
  return bits.join(' · ');
}
