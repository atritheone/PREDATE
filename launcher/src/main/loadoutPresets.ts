import { randomUUID } from 'node:crypto'
import { existsSync, readFileSync, renameSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import {
  equipment,
  weaponSlots,
  type Inventory,
  type InventoryItem,
  type LoadoutPreset,
  type LoadoutPresetInput,
  type PresetKind,
} from '../shared/types'

const presetPath = (root: string): string => join(root, 'config', 'loadout-presets.local.json')
const equipmentSlots = equipment.map(([slot]) => slot).filter((slot) => !weaponSlots.includes(slot))

function record(value: unknown): value is Record<string, unknown> {
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

function presetInventory(value: unknown, kind: PresetKind): value is Inventory {
  const slots: readonly string[] = kind === 'weapon' ? weaponSlots : equipmentSlots
  return (
    record(value) &&
    Object.entries(value).every(([slot, item]) => slots.includes(slot) && inventoryItem(item))
  )
}

function presetInput(value: unknown): LoadoutPresetInput {
  if (!record(value) || (value.kind !== 'weapon' && value.kind !== 'equipment'))
    throw new Error('Choose a preset type.')
  const name = typeof value.name === 'string' ? value.name.trim() : ''
  if (!name || name.length > 80) throw new Error('Use a preset name of 1–80 characters.')
  if (!presetInventory(value.inventory, value.kind))
    throw new Error('The inventory could not be saved as a preset.')
  return { name, kind: value.kind, inventory: value.inventory }
}

export function loadPresets(root: string): LoadoutPreset[] {
  const path = presetPath(root)
  if (!existsSync(path)) return []
  const data: unknown = JSON.parse(readFileSync(path, 'utf8'))
  if (!record(data) || data.schema !== 1 || !Array.isArray(data.presets))
    throw new Error('Saved presets could not be read.')
  return data.presets.map((value: unknown) => {
    if (!record(value) || typeof value.id !== 'string') throw new Error('Invalid saved preset.')
    return {
      id: value.id,
      ...presetInput(value),
      ...(value.isDefault === true && { isDefault: true }),
    }
  })
}

export function savePreset(root: string, value: unknown): LoadoutPreset {
  const input = presetInput(value)
  const presets = loadPresets(root)
  if (
    presets.some(
      (preset) =>
        preset.kind === input.kind && preset.name.toLowerCase() === input.name.toLowerCase(),
    )
  )
    throw new Error('A preset of this type already has that name.')

  const preset = { id: randomUUID(), ...input }
  writePresets(root, [...presets, preset])
  return preset
}

export function setDefaultPreset(root: string, kind: unknown, id: unknown): LoadoutPreset[] {
  if (kind !== 'weapon' && kind !== 'equipment') throw new Error('Choose a preset type.')
  const presets = loadPresets(root)
  if (id !== null && !presets.some((preset) => preset.kind === kind && preset.id === id))
    throw new Error('Choose a saved preset.')

  const updated = presets.map((preset) =>
    preset.kind === kind ? { ...preset, isDefault: preset.id === id ? true : undefined } : preset,
  )
  writePresets(root, updated)
  return updated
}

function writePresets(root: string, presets: LoadoutPreset[]): void {
  const path = presetPath(root)
  const temporary = path + '.' + randomUUID() + '.tmp'
  try {
    writeFileSync(temporary, JSON.stringify({ schema: 1, presets }, null, 2), 'utf8')
    renameSync(temporary, path)
  } finally {
    rmSync(temporary, { force: true })
  }
}
