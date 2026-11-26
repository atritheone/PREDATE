export interface Preferences {
  schema: number
  selected_world: string
  reconnect_grace: number
  extra_mods: string[]
  backup_limit: number
  receiver_scale: number
  receiver_high_contrast: boolean
}

const huntMaps = {
  chernarusplus: 'Chernarus',
  enoch: 'Livonia',
  sakhal: 'Sakhal',
  nasdara: 'Nasdara',
} as const
export type HuntMap = keyof typeof huntMaps
export function isHuntMap(map: unknown): map is HuntMap {
  return typeof map === 'string' && Object.hasOwn(huntMaps, map)
}
export type SpawnPoint = { id: string; name: string; x: number; z: number }
export type HuntTerrain = {
  id: HuntMap
  name: string
  size: number
  available: boolean
  spawnPoints: SpawnPoint[]
}

export function huntMapName(map?: HuntMap): string {
  return huntMaps[map ?? 'chernarusplus']
}

export interface Operation {
  map?: HuntMap
  id: string
  name: string
  created: string | null
  extra_mods: string[]
  kind: string
}

export interface Backup {
  map?: HuntMap
  file: string
  name: string
  created: string
  bytes: number
}

export interface SessionStatus {
  phase: string
  message: string
  profile?: string | null
  error?: string
  grace_remaining?: number | null
}

export interface WeaponModulePreview {
  className: string
  name: string
}

export interface Overview {
  loadouts: { id: string; name: string; description: string; workshop?: boolean }[]
  spawnPoints: SpawnPoint[]
  maps: HuntTerrain[]
  operationOptions?: {
    opponents: OpponentCount[]
    randomRegion: boolean
    weaponModule?: WeaponModulePreview
  }
  operations: Operation[]
  preferences: Preferences
  callsign: string
  backups: Backup[]
  status: SessionStatus
}

export type ProfileStat = {
  label: string
} & (
  | {
      value: number | null
      format: 'count' | 'decimal' | 'duration' | 'distance' | 'percent' | 'accuracy'
    }
  | { value: string | null; format: 'text' }
)

export type ProfileStats = { title: string; rows: ProfileStat[] }[]

export type SpawnSelection = { preset: string } | { x: number; z: number }
export interface InventoryItem {
  id: string
  amount?: number
  attachments: Record<string, InventoryItem>
  cargo: Placement[]
}

export interface Placement {
  row: number
  column: number
  rotated: boolean
  item: InventoryItem
}

export type Inventory = Record<string, InventoryItem>
export type PresetKind = 'weapon' | 'equipment'
export interface LoadoutPresetInput {
  kind: PresetKind
  name: string
  inventory: Inventory
}
export interface LoadoutPreset extends LoadoutPresetInput {
  id: string
}
export const weaponSlots: readonly string[] = ['Hands', 'Sidearm', 'Shoulder', 'Melee']
export const equipment = [
  ['Hands', 'Hands'],
  ['Sidearm', 'Sidearm'],
  ['Shoulder', 'Shoulder'],
  ['Melee', 'Second Shoulder'],
  ['Headgear', 'Head'],
  ['Mask', 'Face'],
  ['Eyewear', 'Eyes'],
  ['Body', 'Jacket'],
  ['Vest', 'Vest'],
  ['Back', 'Backpack'],
  ['Legs', 'Pants'],
  ['Feet', 'Boots'],
  ['Gloves', 'Gloves'],
  ['Hips', 'Belt'],
  ['Armband', 'Armband'],
] as const

export interface InventoryEntry {
  ClassName: string
  Parent: number
  Placement: 'hands' | 'attachment' | 'magazine' | 'cargo'
  Slot: string
  Row: number
  Column: number
  Flipped: boolean
  Amount: number
}

export interface NewOperationSettings {
  startHour: number
  startMinute: number
  dayNightCycle: boolean
  enemyRespawns: boolean
  respawnDelay: number
  playerOutfit?: string
  operatorOutfit?: string
  inventory?: InventoryEntry[]
}

export const OPPONENT_COUNTS = [1, 3, 6, 15, 30, 50, 100] as const
export const MAX_OPPONENT_COUNT = 100
export const DEFAULT_OPPONENT_COUNT = MAX_OPPONENT_COUNT
export type OpponentCount = number

export function isOpponentCount(value: unknown): value is OpponentCount {
  return (
    typeof value === 'number' &&
    Number.isInteger(value) &&
    value >= 1 &&
    value <= MAX_OPPONENT_COUNT
  )
}

export type SessionAction =
  | { action: 'continue'; profile: string }
  | { action: 'quit' }
  | { action: 'delete'; profile: string }
  | {
      action: 'new'
      map?: HuntMap
      name: string
      loadout?: string
      opponents?: OpponentCount
      spawn?: SpawnSelection
      setup?: NewOperationSettings
    }
  | { action: 'backup'; profile: string }
  | { action: 'restore'; backup: string; name: string }

export interface ScoreEncounter {
  Callsign: string
  OperatorId: string
  Deployment: number
  Tracked: boolean
  Committed: boolean
  WoundKill: boolean
  Outcome: string
  LockSeconds: number
  TimeToContact: number
  ReachedContact: boolean
  RoundsToFirstHit: number
  FinalApproachSeconds: number
  GroundCovered: number
  UsefulClosure: number
  ReturnDamage: number
  Pulses: number
  PursuitPoints: number
  EngagementPoints: number
  CounterPoints: number
  PrecisionPoints: number
  RangePoints: number
  CreditedRange: number
  KillPoints: number
}

export interface ScoreLife {
  Life: number
  Version: number
  SurvivedSeconds: number
  EndReason: string
  Killer: string
  Partial: boolean
  Shots: number
  Hits: number
  ExtraOperatorHits?: number
  Misses?: number
  PendingRounds?: number
  UnverifiedRounds?: number
  LegacyShots?: number
  LegacyHits?: number
  Kills: number
  SurvivalScore: number
  Score: number
  Encounters: ScoreEncounter[]
}

export interface ScoreHistory {
  name: string
  lives: ScoreLife[]
}

export interface PredateAPI {
  scores(profile: string): Promise<ScoreHistory>
  profileStats(): Promise<ProfileStats>
  saveCallsign(callsign: string): Promise<string>
  overview(): Promise<Overview>
  perform(action: SessionAction): Promise<void>
  savePreferences(values: Omit<Preferences, 'schema' | 'selected_world'>): Promise<Preferences>
  chooseMod(): Promise<string | null>
  loadoutPresets(): Promise<LoadoutPreset[]>
  saveLoadoutPreset(preset: LoadoutPresetInput): Promise<LoadoutPreset>
  setSetupOpen(open: boolean): Promise<void>
  onStatus(callback: (status: SessionStatus) => void): () => void
  onContinueError(callback: (message: string) => void): () => void
}
