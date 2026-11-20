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

export const OPPONENT_COUNTS = [1, 3, 6, 15, 30] as const
export type OpponentCount = (typeof OPPONENT_COUNTS)[number]

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
  setSetupOpen(open: boolean): Promise<void>
  onStatus(callback: (status: SessionStatus) => void): () => void
}
