import { randomUUID } from 'node:crypto'
import { join } from 'node:path'
import {
  equipment,
  weaponSlots,
  type LoadoutPreset,
  type LoadoutPresetInput,
} from '../shared/types'
import {
  presetInventory,
  presetName,
  readPresetFile,
  record,
  writePresetFile,
} from './presetStorage'

const presetPath = (root: string): string => join(root, 'config', 'loadout-presets.local.json')
const equipmentSlots = equipment.map(([slot]) => slot).filter((slot) => !weaponSlots.includes(slot))

function presetInput(value: unknown): LoadoutPresetInput {
  if (!record(value) || (value.kind !== 'weapon' && value.kind !== 'equipment'))
    throw new Error('Choose a preset type.')
  const name = presetName(value.name)
  if (!presetInventory(value.inventory, value.kind === 'weapon' ? weaponSlots : equipmentSlots))
    throw new Error('The inventory could not be saved as a preset.')
  return { name, kind: value.kind, inventory: value.inventory }
}

export function loadPresets(root: string): LoadoutPreset[] {
  return readPresetFile(presetPath(root)).map((value) => {
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
  writePresetFile(presetPath(root), [...presets, preset])
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
  writePresetFile(presetPath(root), updated)
  return updated
}
