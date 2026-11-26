import { useEffect, useState, type ReactNode } from 'react'
import {
  DEFAULT_OPPONENT_COUNT,
  MAX_OPPONENT_COUNT,
  OPPONENT_COUNTS,
  isOpponentCount,
  isHuntMap,
  weaponSlots,
  type HuntMap,
  type LoadoutPreset,
  type OpponentCount,
  type Overview,
  type PresetKind,
  type SessionAction,
} from '../../shared/types'
import { InventoryEditor } from './InventoryEditor'
import { SavePresetDialog } from './SavePresetDialog'
import { SpawnEditor, validSpawn } from './SpawnEditor'
import { terrainAssets } from './terrainAssets'
import {
  changeOutfit,
  clothingPresets,
  completeLegacyInventory,
  defaultPlayerOutfit,
  inventoryEntries,
  removeEpinephrine,
  starterInventory,
  stowSidearm,
  switchPresetOptic,
  upgradeDraftOutfitMount,
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
const defaultRespawnDelay = '7'

function isNightStart(time: string, map: HuntMap = 'chernarusplus'): boolean {
  if (!/^(?:[01]\d|2[0-3]):[0-5]\d$/.test(time)) return false
  const [hour, minute] = time.split(':').map(Number)
  const start = hour * 60 + minute
  const { sunrise, sunset } = terrainAssets[map]
  return start < sunrise || start >= sunset
}

function clockLabel(minutes: number): string {
  return `${String(Math.floor(minutes / 60)).padStart(2, '0')}:${String(minutes % 60).padStart(2, '0')}`
}

const presetSaveTitles = {
  weapon: 'Save Weapon Preset',
  equipment: 'Save Equipment Preset',
} as const
const rifleGroups = [
  ['Assault rifle', 'Assault Rifles'],
  ['Battle rifle', 'Battle Rifles'],
  ['Sniper rifle', 'Sniper Rifles'],
] as const

export interface OperationDraft {
  name: string
  map: HuntMap
  time: string
  dayNightCycle: boolean
  opponents: OpponentCount
  enemyRespawns: boolean
  respawnDelay: string
  spawn: string
  x: string
  z: string
  kit: string
  playerOutfit: string
  inventory: Inventory | null
  weaponPresetId?: string
  equipmentPresetId?: string
}

const defaults: OperationDraft = {
  name: '',
  map: 'chernarusplus',
  time: defaultTime,
  dayNightCycle: true,
  opponents: DEFAULT_OPPONENT_COUNT,
  enemyRespawns: true,
  respawnDelay: defaultRespawnDelay,
  spawn: 'original',
  x: '',
  z: '',
  kit: 'hk417',
  playerOutfit: defaultPlayerOutfit,
  inventory: null,
}

export function readOperationDraft(): OperationDraft {
  try {
    const saved = localStorage.getItem(setupKey)
    if (!saved) return { ...defaults }
    const { componentsReady, respawnRules, ...draft } = JSON.parse(saved) as Partial<OperationDraft> & {
      componentsReady?: boolean
      footprints?: boolean
      operatorOutfit?: string
      respawnRules?: number
    }
    delete draft.footprints
    delete draft.operatorOutfit
    if (respawnRules === undefined && draft.respawnDelay === '120') draft.respawnDelay = defaultRespawnDelay
    if (!isHuntMap(draft.map)) delete draft.map
    if (typeof draft.dayNightCycle !== 'boolean') delete draft.dayNightCycle
    if (!isOpponentCount(draft.opponents)) delete draft.opponents
    if (draft.inventory) {
      draft.inventory = removeEpinephrine(draft.inventory)
      if (!componentsReady) draft.inventory = completeLegacyInventory(draft.inventory)
    }
    if (draft.playerOutfit === undefined && draft.inventory) {
      const worn = draft.inventory
      draft.playerOutfit = clothingPresets.find(
        (outfit) =>
          worn.Body?.id === outfit.Body &&
          (worn.Legs?.id === outfit.Legs || worn.Legs?.id === (outfit.PresetLegs ?? outfit.Legs)) &&
          worn.Feet?.id === outfit.Feet &&
          (!outfit.Vest || worn.Vest?.id === outfit.Vest),
      )?.Id
      if (draft.playerOutfit === undefined) delete draft.playerOutfit
    }
    if (draft.inventory && draft.playerOutfit && !draft.equipmentPresetId)
      draft.inventory = upgradeDraftOutfitMount(draft.inventory, draft.playerOutfit)
    if (
      draft.inventory &&
      draft.time &&
      isNightStart(draft.time, draft.map) &&
      !draft.weaponPresetId &&
      draft.dayNightCycle === undefined
    ) {
      const kit = draft.kit ?? defaults.kit
      const outfit = draft.playerOutfit ?? defaults.playerOutfit
      draft.inventory = switchPresetOptic(
        draft.inventory,
        starterInventory(kit, outfit),
        starterInventory(kit, outfit, true),
      )
    }
    return { ...defaults, ...draft }
  } catch {
    return { ...defaults }
  }
}

function inventoryPart(inventory: Inventory, kind: PresetKind): Inventory {
  return Object.fromEntries(
    Object.entries(inventory).filter(
      ([slot]) => weaponSlots.includes(slot) === (kind === 'weapon'),
    ),
  )
}

function samePart(left: Inventory, right: Inventory, kind: PresetKind): boolean {
  const first = inventoryPart(left, kind)
  const second = inventoryPart(right, kind)
  return (
    Object.keys(first).length === Object.keys(second).length &&
    Object.entries(first).every(
      ([slot, item]) => JSON.stringify(item) === JSON.stringify(second[slot]),
    )
  )
}

function presetValue(preset: LoadoutPreset): string {
  return 'saved:' + preset.id
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
  const [saveStatus, setSaveStatus] = useState('')
  const [presets, setPresets] = useState<LoadoutPreset[] | null>(null)
  const [savingPreset, setSavingPreset] = useState<PresetKind | null>(null)
  const [exactOpponents, setExactOpponents] = useState<string | null>(null)
  useEffect(() => {
    let mounted = true
    void window.predate.loadoutPresets().then(
      (saved) => {
        if (mounted) setPresets(saved)
      },
      () => {
        if (mounted) showError('Could not read the saved loadout presets.')
      },
    )
    return () => {
      mounted = false
    }
  }, [showError])

  const change = (patch: Partial<OperationDraft>): void => {
    update({ ...draft, ...patch })
    setSaveStatus('')
  }
  const kit = overview.loadouts.find((item) => item.id === draft.kit) ?? overview.loadouts[0]
  const playerOutfit = clothingPresets.some(({ Id }) => Id === draft.playerOutfit)
    ? draft.playerOutfit
    : defaults.playerOutfit
  const terrain = overview.maps.find((item) => item.id === draft.map) ?? overview.maps[0]
  const lighting = terrainAssets[terrain.id]
  const times = terrain.id === 'sakhal' || terrain.id === 'nasdara'
    ? [['Dawn', clockLabel(lighting.sunrise)], ['Day', '12:00'], ['Dusk', clockLabel(lighting.sunset)], ['Night', '00:00']]
    : timePresets
  const night = isNightStart(draft.time, draft.map)
  const standardLoadout = starterInventory(kit?.id ?? '', playerOutfit, night)
  const loadout = draft.inventory ?? standardLoadout
  const changeTime = (time: string): void => {
    const nextNight = isNightStart(time, terrain.id)
    if (draft.inventory && !draft.weaponPresetId && nextNight !== night) {
      const nextLoadout = starterInventory(kit?.id ?? '', playerOutfit, nextNight)
      change({ time, inventory: switchPresetOptic(draft.inventory, standardLoadout, nextLoadout) })
    } else {
      change({ time })
    }
  }
  const changeMap = (map: HuntMap): void => {
    const nextNight = isNightStart(draft.time, map)
    const inventory =
      draft.inventory && !draft.weaponPresetId && nextNight !== night
        ? switchPresetOptic(
            draft.inventory,
            standardLoadout,
            starterInventory(kit?.id ?? '', playerOutfit, nextNight),
          )
        : draft.inventory
    change({ map, spawn: 'original', x: '', z: '', inventory })
  }
  const weaponPresets = presets?.filter(({ kind }) => kind === 'weapon') ?? []
  const equipmentPresets = presets?.filter(({ kind }) => kind === 'equipment') ?? []
  const savedWeapon = weaponPresets.find(({ id }) => id === draft.weaponPresetId)
  const savedEquipment = equipmentPresets.find(({ id }) => id === draft.equipmentPresetId)
  const weaponEdit = !samePart(loadout, standardLoadout, 'weapon')
  const equipmentEdit = !samePart(loadout, standardLoadout, 'equipment')
  const applyPreset = (preset: LoadoutPreset): void => {
    const inventory = {
      ...inventoryPart(loadout, preset.kind === 'weapon' ? 'equipment' : 'weapon'),
      ...removeEpinephrine(preset.inventory),
    }
    change(
      preset.kind === 'weapon'
        ? { inventory, weaponPresetId: preset.id }
        : { inventory, equipmentPresetId: preset.id },
    )
  }
  const savePreset = async (kind: PresetKind, name: string): Promise<void> => {
    const preset = await window.predate.saveLoadoutPreset({
      kind,
      name,
      inventory: inventoryPart(loadout, kind),
    })
    setPresets((existing) => [...(existing ?? []), preset])
    applyPreset(preset)
    setSaveStatus(kind === 'weapon' ? 'Weapon preset saved.' : 'Equipment preset saved.')
  }
  const selectWeapon = (value: string): void => {
    const preset = weaponPresets.find((option) => presetValue(option) === value)
    if (preset) {
      applyPreset(preset)
    } else if (value === 'empty') {
      change({ inventory: {}, weaponPresetId: undefined, equipmentPresetId: undefined })
    } else if (overview.loadouts.some(({ id }) => id === value)) {
      const inventory =
        savedEquipment || equipmentEdit
          ? {
              ...inventoryPart(loadout, 'equipment'),
              ...inventoryPart(starterInventory(value, playerOutfit, night), 'weapon'),
            }
          : null
      change({ kit: value, inventory, weaponPresetId: undefined })
    }
  }
  const selectPlayerOutfit = (outfitId: string): void => {
    const preset = equipmentPresets.find((option) => presetValue(option) === outfitId)
    if (preset) {
      applyPreset(preset)
      return
    }
    if (!clothingPresets.some(({ Id }) => Id === outfitId)) return
    const inventory =
      draft.inventory && changeOutfit(draft.inventory, outfitId, playerOutfit, kit?.id ?? '')
    if (draft.inventory && !inventory) {
      showError('There is no room for your carried gear in this outfit.')
      return
    }
    change({ playerOutfit: outfitId, inventory, equipmentPresetId: undefined })
  }
  const opponentOptions: readonly number[] = overview.operationOptions?.opponents ?? OPPONENT_COUNTS
  const customOpponents = exactOpponents !== null || !opponentOptions.includes(draft.opponents)
  const opponents = exactOpponents === null ? draft.opponents : Number(exactOpponents)
  const randomRegion = overview.operationOptions?.randomRegion === true
  const spawn = draft.spawn === 'regional' && !randomRegion ? 'original' : draft.spawn
  const spawnValid = spawn !== 'custom' || validSpawn(draft.x, draft.z, terrain.size)
  const delay = Number(draft.respawnDelay)
  const valid =
    !!draft.name.trim() &&
    terrain.available &&
    !!kit &&
    spawnValid &&
    isOpponentCount(opponents) &&
    /^(?:[01]\d|2[0-3]):[0-5]\d$/.test(draft.time) &&
    (!draft.enemyRespawns || (Number.isInteger(delay) && delay >= 1 && delay <= 3600))
  const saveDraft = (): void => {
    if (!isOpponentCount(opponents)) {
      showError('Enter a whole number from 1 to 100 opponents.')
      return
    }
    try {
      localStorage.setItem(
        setupKey,
        JSON.stringify({
          ...draft,
          opponents,
          spawn,
          playerOutfit,
          respawnRules: 1,
          componentsReady: true,
        }),
      )
      setSaveStatus('Setup saved on this PC.')
    } catch {
      showError('Could not save this setup on this PC.')
    }
  }
  const submit = (): void => {
    if (!valid || busy) return
    const packed = stowSidearm(loadout)
    if (!packed) {
      showError('Make room for the sidearm in a holster or worn container.')
      return
    }
    const [startHour, startMinute] = draft.time.split(':').map(Number)
    void create({
      action: 'new',
      map: terrain.id,
      name: draft.name,
      loadout: kit.id,
      opponents,
      spawn: spawn === 'custom' ? { x: Number(draft.x), z: Number(draft.z) } : { preset: spawn },
      setup: {
        startHour,
        startMinute,
        dayNightCycle: draft.dayNightCycle,
        enemyRespawns: draft.enemyRespawns,
        respawnDelay: draft.enemyRespawns ? delay : Number(defaultRespawnDelay),
        playerOutfit,
        inventory: inventoryEntries(packed),
      },
    })
  }

  return (
    <>
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
                Hunt Name
                <input
                  id="operation-name"
                  maxLength={80}
                  value={draft.name}
                  onChange={(event) => change({ name: event.target.value })}
                />
              </label>
              <label className="field">
                Map
                <select
                  id="hunt-map"
                  value={terrain.id}
                  onChange={(event) => {
                    const selected = overview.maps.find((item) => item.id === event.target.value && item.available)
                    if (selected) changeMap(selected.id)
                  }}
                >
                  {overview.maps.map((item) => (
                    <option key={item.id} value={item.id} disabled={!item.available}>
                      {item.name}
                    </option>
                  ))}
                </select>
              </label>
              <div className="time-controls">
                <label className="field">
                  Start Time
                  <input
                    type="time"
                    id="start-time"
                    value={draft.time}
                    onChange={(event) => changeTime(event.target.value)}
                  />
                </label>
                <label className="day-night-cycle">
                  <input
                    type="checkbox"
                    id="day-night-cycle"
                    checked={draft.dayNightCycle}
                    onChange={(event) => change({ dayNightCycle: event.target.checked })}
                  />
                  <span>Day Night Cycle</span>
                </label>
                <div className="time-presets" aria-label="Time Presets">
                  {times.map(([label, value]) => (
                    <button
                      type="button"
                      key={value}
                      aria-pressed={draft.time === value}
                      onClick={() => changeTime(value)}
                    >
                      {label}
                    </button>
                  ))}
                </div>
              </div>
              <div className="game-fields">
                <label className="field">
                  Enemy Count
                  <select
                    id="opponent-count"
                    value={customOpponents ? 'custom' : opponents}
                    onChange={(event) => {
                      if (event.target.value === 'custom') {
                        if (exactOpponents === null) setExactOpponents(String(draft.opponents))
                        return
                      }
                      setExactOpponents(null)
                      change({ opponents: Number(event.target.value) })
                    }}
                  >
                    {opponentOptions.map((count) => (
                      <option key={count} value={count}>
                        {count} {count === 1 ? 'Operator' : 'Operators'}
                      </option>
                    ))}
                    <option value="custom">Custom</option>
                  </select>
                </label>
                {customOpponents && (
                  <label className="field">
                    Exact Operator Count
                    <input
                      id="exact-opponent-count"
                      type="number"
                      min={1}
                      max={MAX_OPPONENT_COUNT}
                      step={1}
                      required
                      value={exactOpponents ?? draft.opponents}
                      onChange={(event) => {
                        const value = event.target.value
                        setExactOpponents(value)
                        if (isOpponentCount(Number(value))) change({ opponents: Number(value) })
                      }}
                    />
                  </label>
                )}
              </div>
              <label className="game-toggle split">
                <span>
                  <strong>Enemy Respawns</strong>
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
                  Respawn Delay (Seconds)
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
                terrain={terrain}
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
              <div className="kit-select">
                <div className="field">
                  <label htmlFor="starting-kit">Weapon Presets</label>
                  <button
                    type="button"
                    aria-label={presetSaveTitles.weapon}
                    disabled={!presets || busy}
                    onClick={() => setSavingPreset('weapon')}
                  >
                    Save
                  </button>
                  <select
                    id="starting-kit"
                    value={savedWeapon ? presetValue(savedWeapon) : weaponEdit ? 'custom' : kit?.id}
                    onChange={(event) => selectWeapon(event.target.value)}
                  >
                    {rifleGroups.map(([category, label]) => {
                      const kits = overview.loadouts.filter((item) => item.description === category)
                      return kits.length ? (
                        <optgroup key={category} label={label}>
                          {kits.map((item) => (
                            <option key={item.id} value={item.id}>
                              {item.name}
                            </option>
                          ))}
                        </optgroup>
                      ) : null
                    })}
                    {overview.loadouts
                      .filter(
                        (item) => !rifleGroups.some(([category]) => category === item.description),
                      )
                      .map((item) => (
                        <option key={item.id} value={item.id}>
                          {item.name}
                        </option>
                      ))}
                    {weaponPresets.length > 0 && (
                      <optgroup label="Saved Weapon Presets">
                        {weaponPresets.map((preset) => (
                          <option key={preset.id} value={presetValue(preset)}>
                            {preset.name}
                          </option>
                        ))}
                      </optgroup>
                    )}
                    {weaponEdit && !savedWeapon && (
                      <option value="custom" disabled>
                        Custom
                      </option>
                    )}
                    <option value="empty">Empty Inventory</option>
                  </select>
                </div>
                <div className="field">
                  <label htmlFor="player-outfit">Equipment Presets</label>
                  <button
                    type="button"
                    aria-label={presetSaveTitles.equipment}
                    disabled={!presets || busy}
                    onClick={() => setSavingPreset('equipment')}
                  >
                    Save
                  </button>
                  <select
                    id="player-outfit"
                    value={
                      savedEquipment
                        ? presetValue(savedEquipment)
                        : equipmentEdit
                          ? 'custom'
                          : playerOutfit
                    }
                    onChange={(event) => selectPlayerOutfit(event.target.value)}
                  >
                    {clothingPresets.map(({ Id, Name }) => (
                      <option key={Id} value={Id}>
                        {Name}
                      </option>
                    ))}
                    {equipmentPresets.length > 0 && (
                      <optgroup label="Saved Equipment Presets">
                        {equipmentPresets.map((preset) => (
                          <option key={preset.id} value={presetValue(preset)}>
                            {preset.name}
                          </option>
                        ))}
                      </optgroup>
                    )}
                    {equipmentEdit && !savedEquipment && (
                      <option value="custom" disabled>
                        Custom
                      </option>
                    )}
                  </select>
                </div>
              </div>
              <InventoryEditor
                inventory={loadout}
                includeWorkshopGear={kit?.workshop === true}
                weaponModule={overview.operationOptions?.weaponModule}
                update={(inventory) =>
                  change({
                    inventory,
                    weaponPresetId: samePart(inventory, loadout, 'weapon')
                      ? draft.weaponPresetId
                      : undefined,
                    equipmentPresetId: samePart(inventory, loadout, 'equipment')
                      ? draft.equipmentPresetId
                      : undefined,
                  })
                }
              />
            </section>
          </div>
        </div>
        <div className="setup-footer split">
          <p className="hint" role="status">
            {saveStatus ||
              (!spawnValid
                ? 'Enter valid spawn coordinates.'
                : active && !busy
                  ? 'The current hunt will save and close.'
                  : '')}
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
      {savingPreset && (
        <SavePresetDialog
          title={presetSaveTitles[savingPreset]}
          save={(name) => savePreset(savingPreset, name)}
          close={() => setSavingPreset(null)}
        />
      )}
    </>
  )
}
