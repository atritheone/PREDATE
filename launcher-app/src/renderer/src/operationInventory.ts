import catalog from '../../../resources/operation-catalog.json'
import presets from '../../../../source/Predate/data/loadouts.json'
import type { InventoryEntry } from '../../shared/types'

export interface Gear {
  id: string
  name: string
  kind: string
  width: number
  height: number
  cargoWidth: number
  cargoHeight: number
  capacity: number
  slots: string[]
  accepts: string[]
  magazines: string[]
}

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

const playerSpareMagazines = 6
const requiredParts: Record<string, string> = {
  batteryd: 'Battery9V',
  carbattery: 'CarBattery',
  gascanister: 'SmallGasCanister',
  gasmaskfilter: 'GasMask_Filter',
  sparkplug: 'SparkPlug',
}

// UniversalLight uses the rail capability of the mounted handguard on these weapons.
const railHandguards: Record<string, string> = {
  weaponhandguardm4: 'M4_RISHndgrd',
  weaponhandguardak: 'AK_RailHndgrd',
  weaponhandguardmp5: 'MP5_RailHndgrd',
}

function lightRailSlot(item: InventoryItem): string | undefined {
  if (!Object.values(item.attachments).some((part) => part.id === 'UniversalLight')) return
  const slot = gearById.get(item.id)?.accepts.find((key) => railHandguards[key.toLowerCase()])
  return (
    slot &&
    (Object.keys(item.attachments).find((key) => key.toLowerCase() === slot.toLowerCase()) ?? slot)
  )
}

function isRailHandguard(slot: string, item?: string): boolean {
  const base = railHandguards[slot.toLowerCase()]
  return !!base && !!item && (item === base || item.startsWith(base + '_'))
}

export function requiresLightRail(item: InventoryItem, slot: string): boolean {
  return lightRailSlot(item)?.toLowerCase() === slot.toLowerCase()
}

export function attachmentChoices(item: InventoryItem, slot: string): Gear[] {
  const choices = compatibleGear(slot, gearById.get(item.id))
  return requiresLightRail(item, slot)
    ? choices.filter((option) => isRailHandguard(slot, option.id))
    : choices
}

function ensureLightRail(item: InventoryItem): InventoryItem {
  const slot = lightRailSlot(item)
  if (!slot || isRailHandguard(slot, item.attachments[slot]?.id)) return item
  return {
    ...item,
    attachments: { ...item.attachments, [slot]: newItem(railHandguards[slot.toLowerCase()]) },
  }
}

export function updateAttachment(
  item: InventoryItem,
  slot: string,
  part?: InventoryItem,
): InventoryItem {
  const attachments = { ...item.attachments }
  if (part) attachments[slot] = part
  else delete attachments[slot]
  return ensureLightRail({ ...item, attachments })
}

export function completeLegacyInventory(inventory: Inventory): Inventory {
  function complete(item: InventoryItem): InventoryItem {
    const attachments = newItem(item.id).attachments
    for (const [slot, part] of Object.entries(item.attachments)) {
      // A saved part can use a different case for the same DayZ slot.
      const defaultSlot = Object.keys(attachments).find(
        (key) => key.toLowerCase() === slot.toLowerCase(),
      )
      if (defaultSlot) delete attachments[defaultSlot]
      attachments[slot] = complete(part)
    }
    return ensureLightRail({
      ...item,
      attachments,
      cargo: item.cargo.map((placement) => ({ ...placement, item: complete(placement.item) })),
    })
  }
  return Object.fromEntries(Object.entries(inventory).map(([slot, item]) => [slot, complete(item)]))
}

export function inventoryEntries(inventory: Inventory): InventoryEntry[] {
  const entries: InventoryEntry[] = []

  function add(
    item: InventoryItem,
    parent: number,
    placement: InventoryEntry['Placement'],
    slot = '',
    cargo?: Placement,
  ): void {
    const index = entries.length
    entries.push({
      ClassName: item.id,
      Parent: parent,
      Placement: placement,
      Slot: slot,
      Row: cargo?.row ?? -1,
      Column: cargo?.column ?? -1,
      Flipped: cargo?.rotated ?? false,
      Amount: item.amount ?? -1,
    })
    const rail = lightRailSlot(item)
    if (rail && item.attachments[rail]) add(item.attachments[rail], index, 'attachment', rail)
    for (const [attachment, part] of Object.entries(item.attachments)) {
      if (attachment !== '@magazine' && attachment !== rail)
        add(part, index, 'attachment', attachment)
    }
    const magazine = item.attachments['@magazine']
    if (magazine) add(magazine, index, 'magazine')
    for (const position of item.cargo) add(position.item, index, 'cargo', '', position)
  }

  for (const [slot, item] of Object.entries(inventory)) {
    if (slot === 'Hands') add(item, -1, 'hands')
    else add(item, -1, 'attachment', slot)
  }
  return entries
}

export const equipment = [
  ['Hands', 'Hands'],
  ['Shoulder', 'Shoulder'],
  ['Melee', 'Second shoulder'],
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

export const gear: Gear[] = catalog
export const gearById = new Map(gear.map((item) => [item.id, item]))
const gearByConfigName = new Map(gear.map((item) => [item.id.toLowerCase(), item]))
const slotOptions = new Map<string, Gear[]>()
for (const item of gear) {
  for (const slot of item.slots) {
    const key = slot.toLowerCase()
    const items = slotOptions.get(key) ?? []
    if (!items.length) slotOptions.set(key, items)
    items.push(item)
  }
}

const slotLabels: Record<string, string> = {
  '@magazine': 'Magazine',
  batteryd: 'Battery',
  nvg: 'Night Vision',
  vestholster: 'Holster',
  vestpouch: 'Pouches',
  weaponoptics: 'Optic',
  weaponopticsak: 'Optic',
  weaponflashlight: 'Weapon light',
  weaponwrap: 'Weapon wrap',
  weaponbayonet: 'Bayonet',
  weaponbayonetak: 'Bayonet',
  backpack_1: 'Utility slot',
}

export function slotLabel(slot: string): string {
  return (
    slotLabels[slot.toLowerCase()] ??
    slot
      .replace(/^weapon/i, '')
      .replace(/(M4|AK)$/i, '')
      .replace(/([a-z])([A-Z])/g, '$1 $2')
      .replaceAll('_', ' ')
  )
}

export function compatibleGear(slot: string, host?: Gear): Gear[] {
  if (slot === 'Hands') return gear
  if (slot === '@magazine') {
    return (
      host?.magazines.flatMap((id) => {
        const item = gearById.get(id) ?? gearByConfigName.get(id.toLowerCase())
        return item ? [item] : []
      }) ?? []
    )
  }
  return slotOptions.get(slot.toLowerCase()) ?? []
}

export function newItem(id: string): InventoryItem {
  const attachments: InventoryItem['attachments'] = {}
  for (const slot of gearById.get(id)?.accepts ?? []) {
    const key = slot.toLowerCase()
    const part =
      key === 'hook' && (id === 'FishingRod' || id === 'ImprovisedFishingRod')
        ? 'Hook'
        : requiredParts[key]
    if (part && compatibleGear(slot).some((candidate) => candidate.id === part))
      attachments[slot] = newItem(part)
  }
  return { id, attachments, cargo: [] }
}

export function dimensions(placement: Placement): [number, number] {
  const item = gearById.get(placement.item.id)
  if (!item) return [1, 1]
  return placement.rotated ? [item.height, item.width] : [item.width, item.height]
}

export function canPlace(host: Gear, cargo: Placement[], placement: Placement): boolean {
  const [width, height] = dimensions(placement)
  const { row, column } = placement
  return (
    column >= 0 &&
    row >= 0 &&
    column + width <= host.cargoWidth &&
    row + height <= host.cargoHeight &&
    cargo.every((other) => {
      const [otherWidth, otherHeight] = dimensions(other)
      return (
        column + width <= other.column ||
        other.column + otherWidth <= column ||
        row + height <= other.row ||
        other.row + otherHeight <= row
      )
    })
  )
}

export function placeInCargo(
  host: Gear,
  cargo: Placement[],
  item: InventoryItem,
): Placement | null {
  for (const rotated of [false, true]) {
    for (let row = 0; row < host.cargoHeight; row++) {
      for (let column = 0; column < host.cargoWidth; column++) {
        const placement = { row, column, rotated, item }
        if (canPlace(host, cargo, placement)) return placement
      }
    }
  }
  return null
}

export function starterInventory(id: string): Inventory {
  const preset = [...presets.Kits, ...presets.OperatorKits].find((kit) => kit.Id === id)
  if (!preset) return {}
  const root: Inventory = {}
  for (const [slot, item] of Object.entries({
    Headgear: 'Mich2001Helmet',
    Body: 'TTsKOJacket_Camo',
    Vest: 'PlateCarrierVest',
    Back: 'AssaultBag_Green',
    Legs: 'TTSKOPants',
    Feet: 'MilitaryBoots_Black',
    Gloves: 'TacticalGloves_Black',
    Hands: preset.FirearmClass,
  })) {
    root[slot] = newItem(item)
  }
  const weapon = gearById.get(preset.FirearmClass)
  for (const part of [...preset.Attachments, preset.SuppressorClass]) {
    const slot = weapon?.accepts.find((option) =>
      compatibleGear(option, weapon).some((item) => item.id === part),
    )
    if (slot) root.Hands.attachments[slot] = newItem(part)
  }
  root.Headgear.attachments.NVG = newItem('NVGoggles')
  root.Hands.attachments['@magazine'] = newItem(preset.MagazineClass)
  const spares = [
    ...Array<string>(playerSpareMagazines).fill(preset.MagazineClass),
    ...Array<string>(4).fill('BandageDressing'),
    'Morphine',
    'Epinephrine',
    'Compass',
    'ChernarusMap',
  ]
  for (const id of spares) {
    const item = newItem(id)
    if (id === preset.MagazineClass) item.amount = gearById.get(id)?.capacity
    for (const slot of ['Body', 'Legs', 'Back']) {
      const container = root[slot]
      const host = gearById.get(container.id)
      const placement = host && placeInCargo(host, container.cargo, item)
      if (placement) {
        container.cargo.push(placement)
        break
      }
    }
  }
  return root
}
