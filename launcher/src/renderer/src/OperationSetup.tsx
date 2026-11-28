import { useEffect, useState, type Dispatch, type ReactNode, type SetStateAction } from 'react'
import {
  DEFAULT_OPPONENT_COUNT,
  MAX_OPPONENT_COUNT,
  OPPONENT_COUNTS,
  isOpponentCount,
  isHuntMap,
  isHuntStartTime,
  weaponSlots,
  type GamePreset,
  type GamePresetSetup,
  type HuntMap,
  type HuntTerrain,
  type LoadoutPreset,
  type Overview,
  type PresetKind,
  type SessionAction,
} from '../../shared/types'
import { InventoryEditor } from './InventoryEditor'
import { presetForLight } from './presetVariants'
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
const defaultWeather = 'dynamic'
const defaultRespawnDelay = '7'

function isNightStart(time: string, map: HuntMap = 'chernarusplus'): boolean {
  if (!isHuntStartTime(time)) return false
  const [hour, minute] = time.split(':').map(Number)
  const start = hour * 60 + minute
  const { sunrise, sunset } = terrainAssets[map]
  return start < sunrise || start >= sunset
}

function clockLabel(minutes: number): string {
  return `${String(Math.floor(minutes / 60)).padStart(2, '0')}:${String(minutes % 60).padStart(2, '0')}`
}

function weatherForTerrain(terrain: HuntTerrain, weather: string): string {
  return terrain.weatherOptions.some(({ id }) => id === weather) ? weather : defaultWeather
}

const presetSaveTitles = {
  weapon: 'Save Weapon Preset',
  equipment: 'Save Equipment Preset',
  game: 'Save Game Preset',
} as const
const rifleGroups = [
  ['Assault rifle', 'Assault Rifles'],
  ['Battle rifle', 'Battle Rifles'],
  ['Sniper rifle', 'Sniper Rifles'],
] as const

export interface OperationDraft extends Omit<GamePresetSetup, 'customOpponents' | 'inventory'> {
  inventory: Inventory | null
  // An omitted ID follows the saved default. Null keeps the draft's own inventory.
  weaponPresetId?: string | null
  equipmentPresetId?: string | null
}

const defaults: OperationDraft = {
  name: '',
  map: 'chernarusplus',
  time: defaultTime,
  dayNightCycle: true,
  weather: defaultWeather,
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
    const { componentsReady, respawnRules, presetDefaultRules, ...draft } = JSON.parse(saved) as Partial<OperationDraft> & {
      componentsReady?: boolean
      footprints?: boolean
      operatorOutfit?: string
      respawnRules?: number
      presetDefaultRules?: number
    }
    delete draft.footprints
    delete draft.operatorOutfit
    if (respawnRules === undefined && draft.respawnDelay === '120') draft.respawnDelay = defaultRespawnDelay
    if (!isHuntMap(draft.map)) delete draft.map
    if (typeof draft.dayNightCycle !== 'boolean') delete draft.dayNightCycle
    if (typeof draft.weather !== 'string') delete draft.weather
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
    if (!presetDefaultRules) {
      const kit = draft.kit ?? defaults.kit
      const outfit = draft.playerOutfit ?? defaults.playerOutfit
      const stock = starterInventory(kit, outfit, isNightStart(draft.time ?? defaultTime, draft.map))
      if (draft.weaponPresetId === undefined &&
          (kit !== defaults.kit || (draft.inventory && !samePart(draft.inventory, stock, 'weapon'))))
        draft.weaponPresetId = null
      if (draft.equipmentPresetId === undefined &&
          (outfit !== defaults.playerOutfit || (draft.inventory && !samePart(draft.inventory, stock, 'equipment'))))
        draft.equipmentPresetId = null
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

function applyPreset(inventory: Inventory, preset?: LoadoutPreset): Inventory {
  return preset ? {
    ...inventoryPart(inventory, preset.kind === 'weapon' ? 'equipment' : 'weapon'),
    ...removeEpinephrine(preset.inventory),
  } : inventory
}

function presetSelection(presets: LoadoutPreset[], id: string | null | undefined, night: boolean) {
  const preset = presets.find((item) => id === undefined ? item.isDefault : item.id === id)
  return preset && presetForLight(preset, presets, night)
}

const presetFields = { weapon: 'weaponPresetId', equipment: 'equipmentPresetId' } as const

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
  update: Dispatch<SetStateAction<OperationDraft>>
  busy: boolean
  active: boolean
  create: (action: Extract<SessionAction, { action: 'new' }>) => Promise<boolean>
  showError: (message: string) => void
}): ReactNode {
  const [saveStatus, setSaveStatus] = useState('')
  const [presets, setPresets] = useState<LoadoutPreset[] | null>(null)
  const [savingPreset, setSavingPreset] = useState<PresetKind | 'game' | null>(null)
  const [gamePresets, setGamePresets] = useState<GamePreset[] | null>(null)
  const [gamePresetId, setGamePresetId] = useState('')
  const [changingDefault, setChangingDefault] = useState(false)
  const [exactOpponents, setExactOpponents] = useState<string | null>(null)
  useEffect(() => {
    let mounted = true
    void window.predate.loadoutPresets().then(
      (saved) => {
        if (mounted) setPresets(saved)
      },
      () => {
        if (mounted) {
          showError('Could not read the saved loadout presets.')
          setPresets([])
        }
      },
    )
    void window.predate.gamePresets().then(
      (saved) => {
        if (mounted) setGamePresets(saved)
      },
      () => {
        if (mounted) showError('Could not read the saved game presets.')
      },
    )
    return () => {
      mounted = false
    }
  }, [showError])

  const change = (patch: Partial<OperationDraft>): void => {
    update((current) => ({ ...current, ...patch }))
    setSaveStatus('')
  }
  const kit = overview.loadouts.find((item) => item.id === draft.kit) ?? overview.loadouts[0]
  const playerOutfit = clothingPresets.some(({ Id }) => Id === draft.playerOutfit)
    ? draft.playerOutfit
    : defaults.playerOutfit
  const terrain = overview.maps.find((item) => item.id === draft.map) ?? overview.maps[0]
  const weather = weatherForTerrain(terrain, draft.weather)
  const lighting = terrainAssets[terrain.id]
  const times = terrain.id === 'sakhal' || terrain.id === 'nasdara'
    ? [['Dawn', clockLabel(lighting.sunrise)], ['Day', '12:00'], ['Dusk', clockLabel(lighting.sunset)], ['Night', '00:00']]
    : timePresets
  const night = isNightStart(draft.time, draft.map)
  const standardLoadout = starterInventory(kit?.id ?? '', playerOutfit, night)
  const weaponPresets = presets?.filter(({ kind }) => kind === 'weapon') ?? []
  const equipmentPresets = presets?.filter(({ kind }) => kind === 'equipment') ?? []
  const weaponDefault = presetSelection(weaponPresets, undefined, night)
  const equipmentDefault = presetSelection(equipmentPresets, undefined, night)
  const savedWeapon = draft.weaponPresetId === undefined
    ? weaponDefault : presetSelection(weaponPresets, draft.weaponPresetId, night)
  const savedEquipment = draft.equipmentPresetId === undefined
    ? equipmentDefault : presetSelection(equipmentPresets, draft.equipmentPresetId, night)
  const loadout = applyPreset(applyPreset(draft.inventory ?? standardLoadout, savedWeapon), savedEquipment)
  const changeTime = (time: string): void => {
    const nextNight = isNightStart(time, terrain.id)
    if (draft.inventory && !savedWeapon && nextNight !== night) {
      const nextLoadout = starterInventory(kit?.id ?? '', playerOutfit, nextNight)
      change({ time, inventory: switchPresetOptic(draft.inventory, standardLoadout, nextLoadout) })
    } else {
      change({ time })
    }
  }
  const changeMap = (nextTerrain: HuntTerrain): void => {
    const map = nextTerrain.id
    const nextNight = isNightStart(draft.time, map)
    const inventory =
      draft.inventory && !savedWeapon && nextNight !== night
        ? switchPresetOptic(
            draft.inventory,
            standardLoadout,
            starterInventory(kit?.id ?? '', playerOutfit, nextNight),
          )
        : draft.inventory
    change({
      map,
      weather: weatherForTerrain(nextTerrain, weather),
      spawn: 'original',
      x: '',
      z: '',
      inventory,
    })
  }
  const weaponEdit = !samePart(loadout, standardLoadout, 'weapon')
  const equipmentEdit = !samePart(loadout, standardLoadout, 'equipment')
  const savePreset = async (kind: PresetKind, name: string): Promise<void> => {
    const preset = await window.predate.saveLoadoutPreset({
      kind,
      name,
      inventory: inventoryPart(loadout, kind),
    })
    setPresets((existing) => [...(existing ?? []), preset])
    change({ [presetFields[kind]]: preset.id })
    setSaveStatus(kind === 'weapon' ? 'Weapon preset saved.' : 'Equipment preset saved.')
  }
  const defaultButton = (preset: LoadoutPreset, currentDefault?: LoadoutPreset): ReactNode => {
    const active = currentDefault?.id === preset.id
    const label = `${active ? 'Clear' : 'Set'} Default ${preset.kind === 'weapon' ? 'Weapon' : 'Equipment'} Preset`
    return (
      <button
        className="preset-default"
        type="button"
        title={label}
        aria-label={label}
        aria-pressed={active}
        disabled={busy || changingDefault}
        onClick={async () => {
          setChangingDefault(true)
          try {
            setPresets(await window.predate.setDefaultLoadoutPreset(preset.kind, active ? null : preset.id))
            change({ [presetFields[preset.kind]]: active ? preset.id : undefined })
            setSaveStatus(active ? 'Default preset cleared.' : 'Default preset saved.')
          } catch {
            showError('Could not update the default preset.')
          } finally {
            setChangingDefault(false)
          }
        }}
      >
        {active ? '★' : '☆'}
      </button>
    )
  }
  const selectWeapon = (value: string): void => {
    const preset = weaponPresets.find((option) => presetValue(option) === value)
    if (preset) {
      change({ weaponPresetId: preset.id })
    } else if (value === 'empty') {
      change({ inventory: {}, weaponPresetId: null, equipmentPresetId: null })
    } else if (overview.loadouts.some(({ id }) => id === value)) {
      const inventory =
        savedEquipment || equipmentEdit
          ? {
              ...inventoryPart(loadout, 'equipment'),
              ...inventoryPart(starterInventory(value, playerOutfit, night), 'weapon'),
            }
          : null
      change({ kit: value, inventory, weaponPresetId: null })
    }
  }
  const selectPlayerOutfit = (outfitId: string): void => {
    const preset = equipmentPresets.find((option) => presetValue(option) === outfitId)
    if (preset) {
      change({ equipmentPresetId: preset.id })
      return
    }
    if (!clothingPresets.some(({ Id }) => Id === outfitId)) return
    const preserveLoadout = draft.inventory || savedWeapon || savedEquipment
    const inventory = preserveLoadout ? changeOutfit(loadout, outfitId, playerOutfit, kit?.id ?? '') : null
    if (preserveLoadout && !inventory) {
      showError('There is no room for your carried gear in this outfit.')
      return
    }
    change({ playerOutfit: outfitId, inventory, equipmentPresetId: null })
  }
  const opponentOptions: readonly number[] = overview.operationOptions?.opponents ?? OPPONENT_COUNTS
  const customOpponents = exactOpponents !== null || !opponentOptions.includes(draft.opponents)
  const opponents = exactOpponents === null ? draft.opponents : Number(exactOpponents)
  const randomRegion = overview.operationOptions?.randomRegion === true
  const spawn = draft.spawn === 'regional' && !randomRegion ? 'original' : draft.spawn
  const spawnValid = spawn !== 'custom' || validSpawn(draft.x, draft.z, terrain.size)
  const delay = Number(draft.respawnDelay)
  const validSetup =
    !!presets &&
    !!kit &&
    spawnValid &&
    isOpponentCount(opponents) &&
    isHuntStartTime(draft.time) &&
    (!draft.enemyRespawns || (Number.isInteger(delay) && delay >= 1 && delay <= 3600))
  const valid = validSetup && !!draft.name.trim() && terrain.available
  const selectedGamePreset = gamePresets?.find(({ id }) => id === gamePresetId)

  const saveGamePreset = async (name: string): Promise<void> => {
    const preset = await window.predate.saveGamePreset({
      name,
      setup: {
        name: draft.name,
        map: terrain.id,
        time: draft.time,
        dayNightCycle: draft.dayNightCycle,
        weather,
        opponents,
        customOpponents,
        enemyRespawns: draft.enemyRespawns,
        respawnDelay: draft.respawnDelay,
        spawn,
        x: draft.x,
        z: draft.z,
        kit: kit.id,
        playerOutfit,
        inventory: loadout,
      },
    })
    setGamePresets((existing) => [...(existing ?? []), preset])
    setGamePresetId(preset.id)
    setSaveStatus('Game preset saved.')
  }

  const loadGamePreset = (): void => {
    if (!selectedGamePreset) return
    const { customOpponents, ...setup } = selectedGamePreset.setup
    setExactOpponents(customOpponents ? String(setup.opponents) : null)
    update({ ...setup, weaponPresetId: null, equipmentPresetId: null })
    setSaveStatus('Game preset loaded.')
  }

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
          weather,
          playerOutfit,
          respawnRules: 1,
          presetDefaultRules: 1,
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
        weather,
        enemyRespawns: draft.enemyRespawns,
        respawnDelay: draft.enemyRespawns ? delay : Number(defaultRespawnDelay),
        playerOutfit,
        inventory: inventoryEntries(packed),
      },
    }).then((started) => {
      if (started && (weaponDefault || equipmentDefault)) {
        update((current) => ({
          ...current,
          ...(weaponDefault && { weaponPresetId: undefined }),
          ...(equipmentDefault && { equipmentPresetId: undefined }),
        }))
      }
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
                    if (selected) changeMap(selected)
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
                <label className="game-checkbox day-night-cycle">
                  <input
                    type="checkbox"
                    id="day-night-cycle"
                    checked={draft.dayNightCycle}
                    onChange={(event) => change({ dayNightCycle: event.target.checked })}
                  />
                  <span>Day Night Cycle</span>
                </label>
                <div className="preset-buttons time-presets" role="group" aria-label="Time Presets">
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
              <div className="weather-controls">
                <span className="field" id="weather-heading">Weather</span>
                <div
                  className="preset-buttons weather-presets"
                  id="operation-weather"
                  role="group"
                  aria-labelledby="weather-heading"
                >
                  {terrain.weatherOptions.map(({ id, label }) => (
                    <button
                      type="button"
                      key={id}
                      value={id}
                      aria-pressed={weather === id}
                      onClick={() => change({ weather: id })}
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
                <label className="game-checkbox">
                  <input
                    type="checkbox"
                    id="enemy-redeployments"
                    checked={draft.enemyRespawns}
                    onChange={(event) => change({ enemyRespawns: event.target.checked })}
                  />
                  <span>Enemy Redeployments</span>
                </label>
                {customOpponents && (
                  <label className="field exact-opponent-field">
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
              {draft.enemyRespawns && (
                <label className="field redeploy-delay">
                  Redeploy Delay (Seconds)
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
                  <div className="preset-actions">
                    {savedWeapon && defaultButton(savedWeapon, weaponDefault)}
                    <button
                      type="button"
                      aria-label={presetSaveTitles.weapon}
                      disabled={!presets || busy || changingDefault}
                      onClick={() => setSavingPreset('weapon')}
                    >
                      Save
                    </button>
                  </div>
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
                  <div className="preset-actions">
                    {savedEquipment && defaultButton(savedEquipment, equipmentDefault)}
                    <button
                      type="button"
                      aria-label={presetSaveTitles.equipment}
                      disabled={!presets || busy || changingDefault}
                      onClick={() => setSavingPreset('equipment')}
                    >
                      Save
                    </button>
                  </div>
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
                      : null,
                    equipmentPresetId: samePart(inventory, loadout, 'equipment')
                      ? draft.equipmentPresetId
                      : null,
                  })
                }
              />
            </section>
          </div>
        </div>
        <div className="setup-footer">
          <div className="game-preset-controls" role="group" aria-label="Game Presets">
            <select
              aria-label="Game Presets"
              value={gamePresetId}
              disabled={busy || !gamePresets?.length}
              onChange={(event) => setGamePresetId(event.target.value)}
            >
              <option value="">Game Presets</option>
              {gamePresets?.map((preset) => (
                <option key={preset.id} value={preset.id}>
                  {preset.name}
                </option>
              ))}
            </select>
            <button type="button" onClick={loadGamePreset} disabled={busy || !selectedGamePreset}>
              Load
            </button>
            <button
              type="button"
              aria-label="Save Game Preset"
              disabled={busy || !gamePresets || !validSetup}
              onClick={() => setSavingPreset('game')}
            >
              Save Preset
            </button>
          </div>
          <button type="button" onClick={saveDraft}>
            Save Draft
          </button>
          <p className="hint" role="status">
            {saveStatus ||
              (!spawnValid
                ? 'Enter valid spawn coordinates.'
                : active && !busy
                  ? 'The current hunt will save and close.'
                  : '')}
          </p>
          <button className="primary" type="submit" disabled={!valid || busy}>
            Create and Play
          </button>
        </div>
      </form>
      {savingPreset && (
        <SavePresetDialog
          title={presetSaveTitles[savingPreset]}
          save={savingPreset === 'game' ? saveGamePreset : (name) => savePreset(savingPreset, name)}
          close={() => setSavingPreset(null)}
        />
      )}
    </>
  )
}
