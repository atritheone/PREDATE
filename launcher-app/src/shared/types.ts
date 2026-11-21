export interface Preferences {
  schema: number
  selected_world: string
  reconnect_grace: number
  offline: boolean
  extra_mods: string[]
  backup_limit: number
  receiver_scale: number
  receiver_high_contrast: boolean
}

export interface Operation {
  id: string
  name: string
  created: string | null
  extra_mods: string[]
  kind: string
}

export interface Backup {
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
  spawnPoints: { id: string; name: string; x: number; z: number }[]
  operationOptions?: {
    opponents: OpponentCount[]
    randomRegion: boolean
    weaponModule?: WeaponModulePreview
  }
  operations: Operation[]
  preferences: Preferences
  backups: Backup[]
  status: SessionStatus
}

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
  return typeof value === 'number' && Number.isInteger(value) && value >= 1 && value <= MAX_OPPONENT_COUNT
}

export type SessionAction =
  | { action: 'continue'; profile: string }
  | { action: 'quit' }
  | {
      action: 'new'
      name: string
      loadout?: string
      opponents?: OpponentCount
      spawn?: SpawnSelection
      setup?: NewOperationSettings
    }
  | { action: 'backup'; profile: string }
  | { action: 'restore'; backup: string; name: string }

export interface PredateAPI {
  overview(): Promise<Overview>
  perform(action: SessionAction): Promise<void>
  savePreferences(values: Omit<Preferences, 'schema' | 'selected_world'>): Promise<Preferences>
  chooseMod(): Promise<string | null>
  loadoutPresets(): Promise<LoadoutPreset[]>
  saveLoadoutPreset(preset: LoadoutPresetInput): Promise<LoadoutPreset>
  setSetupOpen(open: boolean): Promise<void>
  onStatus(callback: (status: SessionStatus) => void): () => void
}
