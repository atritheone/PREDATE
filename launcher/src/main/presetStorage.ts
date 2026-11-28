import { randomUUID } from 'node:crypto'
import { existsSync, readFileSync, renameSync, rmSync, writeFileSync } from 'node:fs'
import type { Inventory, InventoryItem } from '../shared/types'

export function record(value: unknown): value is Record<string, unknown> {
  return !!value && typeof value === 'object' && !Array.isArray(value)
}

function inventoryItem(value: unknown, depth = 1): value is InventoryItem {
  if (
    !record(value) ||
    depth > 12 ||
    typeof value.id !== 'string' ||
    !value.id ||
    (value.amount !== undefined && !Number.isFinite(value.amount)) ||
    !record(value.attachments) ||
    !Array.isArray(value.cargo)
  )
    return false
  return (
    Object.values(value.attachments).every((child) => inventoryItem(child, depth + 1)) &&
    value.cargo.every(
      (item: unknown) =>
        record(item) &&
        Number.isInteger(item.row) &&
        Number.isInteger(item.column) &&
        typeof item.rotated === 'boolean' &&
        inventoryItem(item.item, depth + 1),
    )
  )
}

export function presetInventory(value: unknown, slots: readonly string[]): value is Inventory {
  return (
    record(value) &&
    Object.entries(value).every(([slot, item]) => slots.includes(slot) && inventoryItem(item))
  )
}

export function presetName(value: unknown): string {
  const name = typeof value === 'string' ? value.trim() : ''
  if (!name || name.length > 80) throw new Error('Use a preset name of 1–80 characters.')
  return name
}

export function readPresetFile(path: string): unknown[] {
  if (!existsSync(path)) return []
  const data: unknown = JSON.parse(readFileSync(path, 'utf8'))
  if (!record(data) || data.schema !== 1 || !Array.isArray(data.presets))
    throw new Error('Saved presets could not be read.')
  return data.presets
}

export function writePresetFile(path: string, presets: unknown[]): void {
  const temporary = path + '.' + randomUUID() + '.tmp'
  try {
    writeFileSync(temporary, JSON.stringify({ schema: 1, presets }, null, 2), 'utf8')
    renameSync(temporary, path)
  } finally {
    rmSync(temporary, { force: true })
  }
}
