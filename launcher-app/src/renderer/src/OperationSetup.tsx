import { useState, type ReactNode } from 'react'
import {
  OPPONENT_COUNTS,
  type OpponentCount,
  type Overview,
  type SessionAction,
} from '../../shared/types'
import { InventoryEditor } from './InventoryEditor'
import { SpawnEditor, validSpawn } from './SpawnEditor'
import {
  completeLegacyInventory,
  hasDefaultComponents,
  inventoryEntries,
  starterInventory,
  type Inventory,
} from './operationInventory'
import './operation-setup.css'

const setupKey = 'predate.operation-draft.v1'
const timePresets = [
  ['Dawn', '06:00'],
  ['Day', '12:00'],
  ['Dusk', '18:00'],
  ['Night', '00:00'],
] as const
const defaultTime = '12:00'

export interface OperationDraft {
  name: string
  time: string
  opponents: OpponentCount
  enemyRespawns: boolean
  respawnDelay: string
  spawn: string
  x: string
  z: string
  kit: string
  inventory: Inventory | null
}

const defaults: OperationDraft = {
  name: '',
  time: defaultTime,
  opponents: 30,
  enemyRespawns: true,
  respawnDelay: '120',
  spawn: 'regional',
  x: '',
  z: '',
  kit: 'm4a1',
  inventory: null,
}

export function readOperationDraft(): OperationDraft {
  try {
    const saved = localStorage.getItem(setupKey)
    if (!saved) return { ...defaults }
    const { componentsReady, ...draft } = JSON.parse(saved) as Partial<OperationDraft> & {
      componentsReady?: boolean
      footprints?: boolean
    }
    delete draft.footprints
    if (!componentsReady && draft.inventory)
      draft.inventory = completeLegacyInventory(draft.inventory)
    return { ...defaults, ...draft }
  } catch {
    return { ...defaults }
  }
}

export function OperationSetup({
  overview,
  draft,
  update,
  busy,
  active,
  create,
  showError,
}: {
  overview: Overview
  draft: OperationDraft
  update: (draft: OperationDraft) => void
  busy: boolean
  active: boolean
  create: (action: Extract<SessionAction, { action: 'new' }>) => Promise<void>
  showError: (message: string) => void
}): ReactNode {
  const [saved, setSaved] = useState(false)
  const change = (patch: Partial<OperationDraft>): void => {
    update({ ...draft, ...patch })
    setSaved(false)
  }
  const kit = overview.loadouts.find((item) => item.id === draft.kit) ?? overview.loadouts[0]
  const loadout = draft.inventory ?? starterInventory(kit?.id ?? '')
  const opponentOptions = overview.operationOptions?.opponents ?? OPPONENT_COUNTS.slice(0, 3)
  const opponents = opponentOptions.includes(draft.opponents) ? draft.opponents : opponentOptions[0]
  const randomRegion = overview.operationOptions?.randomRegion === true
  const spawn = draft.spawn === 'regional' && !randomRegion ? 'original' : draft.spawn
  const spawnValid = spawn !== 'custom' || validSpawn(draft.x, draft.z)
  const delay = Number(draft.respawnDelay)
  const valid =
    !!draft.name.trim() &&
    !!kit &&
    spawnValid &&
    /^(?:[01]\d|2[0-3]):[0-5]\d$/.test(draft.time) &&
    (!draft.enemyRespawns || (Number.isInteger(delay) && delay >= 1 && delay <= 3600))
  const saveDraft = (): void => {
    try {
      localStorage.setItem(
        setupKey,
        JSON.stringify({ ...draft, opponents, spawn, componentsReady: true }),
      )
      setSaved(true)
    } catch {
      showError('Could not save this setup on this PC.')
    }
  }
  const submit = (): void => {
    if (!valid || busy) return
    const [startHour, startMinute] = draft.time.split(':').map(Number)
    const entries = inventoryEntries(loadout)
    void create({
      action: 'new',
      name: draft.name,
      loadout: kit.id,
      opponents,
      spawn: spawn === 'custom' ? { x: Number(draft.x), z: Number(draft.z) } : { preset: spawn },
      setup: {
        startHour,
        startMinute,
        enemyRespawns: draft.enemyRespawns,
        respawnDelay: draft.enemyRespawns ? delay : 120,
        ...(draft.inventory !== null || hasDefaultComponents(entries)
          ? { inventory: entries }
          : {}),
      },
    })
  }

  return (
    <form
      className="operation-setup new-operation"
      onSubmit={(event) => {
        event.preventDefault()
        submit()
      }}
    >
      <div className="setup-body">
        <div className="setup-sections">
          <section className="game-setup stack" aria-labelledby="game-heading">
            <h2 className="setup-heading" id="game-heading">
              Game
            </h2>
            <label className="field">
              Operation name
              <input
                id="operation-name"
                maxLength={80}
                value={draft.name}
                onChange={(event) => change({ name: event.target.value })}
              />
            </label>
            <div className="game-fields">
              <label className="field">
                Start time
                <input
                  type="time"
                  id="start-time"
                  value={draft.time}
                  onChange={(event) => change({ time: event.target.value })}
                />
              </label>
              <label className="field">
                Enemy count
                <select
                  id="opponent-count"
                  value={opponents}
                  onChange={(event) => {
                    const value = Number(event.target.value) as OpponentCount
                    if (opponentOptions.includes(value)) change({ opponents: value })
                  }}
                >
                  {opponentOptions.map((count) => (
                    <option key={count} value={count}>
                      {count} {count === 1 ? 'operator' : 'operators'}
                    </option>
                  ))}
                </select>
              </label>
            </div>
            <div className="time-presets" aria-label="Time presets">
              {timePresets.map(([label, value]) => (
                <button
                  type="button"
                  key={value}
                  aria-pressed={draft.time === value}
                  onClick={() => change({ time: value })}
                >
                  {label}
                </button>
              ))}
            </div>
            <label className="game-toggle split">
              <span>
                <strong>Enemy respawns</strong>
                <small>Replace defeated operators after a delay.</small>
              </span>
              <input
                type="checkbox"
                checked={draft.enemyRespawns}
                onChange={(event) => change({ enemyRespawns: event.target.checked })}
              />
            </label>
            {draft.enemyRespawns && (
              <label className="field respawn-delay">
                Respawn delay (seconds)
                <input
                  type="number"
                  min="1"
                  max="3600"
                  value={draft.respawnDelay}
                  onChange={(event) => change({ respawnDelay: event.target.value })}
                />
              </label>
            )}
          </section>
          <section className="spawn-setup" aria-labelledby="spawn-heading">
            <h2 className="setup-heading" id="spawn-heading">
              Spawn
            </h2>
            <SpawnEditor
              presets={overview.spawnPoints}
              preset={spawn}
              randomRegion={randomRegion}
              x={draft.x}
              z={draft.z}
              change={(spawn, x, z) => change({ spawn, x, z })}
            />
          </section>
          <section className="loadout-setup" aria-labelledby="loadout-heading">
            <h2 className="setup-heading" id="loadout-heading">
              Loadout
            </h2>
            <label className="field kit-select">
              Starting kit
              <select
                id="starting-kit"
                value={draft.inventory === null ? kit?.id : 'custom'}
                onChange={(event) => {
                  const id = event.target.value
                  if (id === 'empty') change({ inventory: {} })
                  else change({ kit: id, inventory: null })
                }}
              >
                {overview.loadouts.map((item) => (
                  <option key={item.id} value={item.id}>
                    {item.name}
                  </option>
                ))}
                {draft.inventory !== null && <option value="custom">Custom</option>}
                <option value="empty">Empty inventory</option>
              </select>
            </label>
            <InventoryEditor inventory={loadout} update={(inventory) => change({ inventory })} />
          </section>
        </div>
      </div>
      <div className="setup-footer split">
        <p className="hint" role="status">
          {saved
            ? 'Setup saved on this PC.'
            : !spawnValid
              ? 'Enter valid spawn coordinates.'
              : active && !busy
                ? 'The current operation will save and close.'
                : ''}
        </p>
        <div className="setup-actions">
          <button type="button" onClick={saveDraft}>
            Save Draft
          </button>
          <button className="primary" type="submit" disabled={!valid || busy}>
            Create and Play
          </button>
        </div>
      </div>
    </form>
  )
}
