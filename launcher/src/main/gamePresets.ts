import { randomUUID } from 'node:crypto'
import { join } from 'node:path'
import {
  equipment,
  isHuntMap,
  isHuntStartTime,
  isOpponentCount,
  type GamePreset,
  type GamePresetInput,
  type GamePresetSetup,
} from '../shared/types'
import {
  presetInventory,
  presetName,
  readPresetFile,
  record,
  writePresetFile,
} from './presetStorage'

const presetPath = (root: string): string => join(root, 'config', 'game-presets.local.json')
const slots = equipment.map(([slot]) => slot)

function gameSetup(value: unknown): value is GamePresetSetup {
  if (!record(value)) return false
  const delay = typeof value.respawnDelay === 'string' ? Number(value.respawnDelay) : NaN
  return (
    typeof value.name === 'string' &&
    value.name.length <= 80 &&
    isHuntMap(value.map) &&
    isHuntStartTime(value.time) &&
    typeof value.dayNightCycle === 'boolean' &&
    typeof value.weather === 'string' &&
    !!value.weather &&
    isOpponentCount(value.opponents) &&
    typeof value.customOpponents === 'boolean' &&
    typeof value.enemyRespawns === 'boolean' &&
    typeof value.respawnDelay === 'string' &&
    (!value.enemyRespawns || (Number.isInteger(delay) && delay >= 1 && delay <= 3600)) &&
    typeof value.spawn === 'string' &&
    !!value.spawn &&
    typeof value.x === 'string' &&
    typeof value.z === 'string' &&
    (value.spawn !== 'custom' ||
      (!!value.x.trim() &&
        !!value.z.trim() &&
        Number.isFinite(Number(value.x)) &&
        Number.isFinite(Number(value.z)))) &&
    typeof value.kit === 'string' &&
    !!value.kit &&
    typeof value.playerOutfit === 'string' &&
    !!value.playerOutfit &&
    presetInventory(value.inventory, slots)
  )
}

function gamePresetInput(value: unknown): GamePresetInput {
  if (!record(value) || !gameSetup(value.setup))
    throw new Error('The hunt setup could not be saved as a preset.')
  return { name: presetName(value.name), setup: value.setup }
}

export function loadGamePresets(root: string): GamePreset[] {
  return readPresetFile(presetPath(root)).map((value) => {
    if (!record(value) || typeof value.id !== 'string') throw new Error('Invalid game preset.')
    return { id: value.id, ...gamePresetInput(value) }
  })
}

export function saveGamePreset(root: string, value: unknown): GamePreset {
  const input = gamePresetInput(value)
  const presets = loadGamePresets(root)
  if (presets.some((preset) => preset.name.toLowerCase() === input.name.toLowerCase()))
    throw new Error('A preset of this type already has that name.')
  const preset = { id: randomUUID(), ...input }
  writePresetFile(presetPath(root), [...presets, preset])
  return preset
}
