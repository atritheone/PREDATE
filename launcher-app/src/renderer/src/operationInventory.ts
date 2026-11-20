import catalog from '../../../resources/operation-catalog.json'
import modernLoadouts from '../../../resources/modern-loadouts.json'
import presets from '../../../../source/Predate/data/loadouts.json'
import type { Inventory, InventoryEntry, InventoryItem, Placement } from '../../shared/types'

export type { Inventory, InventoryItem, Placement } from '../../shared/types'
export { equipment } from '../../shared/types'

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
  if (
    !Object.values(item.attachments).some((part) =>
      ['UniversalLight', 'PredateWeaponModule'].includes(part.id),
    )
  )
    return
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

export const workshopGearIds = new Set(modernLoadouts.Catalog.map((item) => item.id))
export const gear: Gear[] = [...catalog, ...modernLoadouts.Catalog]
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
  weaponflashlight: 'Weapon Light',
  weaponwrap: 'Weapon Wrap',
  weaponbayonet: 'Bayonet',
  weaponbayonetak: 'Bayonet',
  backpack_1: 'Utility Slot',
}

export function slotLabel(slot: string): string {
  return (
    slotLabels[slot.toLowerCase()] ??
    slot
      .replace(/^weapon/i, '')
      .replace(/(M4|AK)$/i, '')
      .replace(/([a-z])([A-Z])/g, '$1 $2')
      .replaceAll('_', ' ')
      .replace(/^\w/, (initial) => initial.toUpperCase())
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

interface ClothingPreset {
  Id: string
  Name: string
  Body: string
  Legs: string
  Feet: string
  Vest?: string
  Pouches?: string
  Gloves?: string
  Headgear?: string
  Eyewear?: string
  NightVision?: string
  WorkshopNightVision?: string
  Legacy?: boolean
}

const allClothingPresets: ClothingPreset[] = presets.Outfits
export const clothingPresets = allClothingPresets.filter((outfit) => !outfit.Legacy)
export const defaultPlayerOutfit = presets.DefaultOutfit

function clothingPreset(id: string): ClothingPreset {
  return allClothingPresets.find((option) => option.Id === id) ?? clothingPresets[0]
}

function wornClothing(outfit: ClothingPreset): Inventory {
  return { Body: newItem(outfit.Body), Legs: newItem(outfit.Legs), Feet: newItem(outfit.Feet) }
}

function outfitMount(outfit: ClothingPreset): ['Headgear' | 'Eyewear', string] {
  return outfit.Headgear
    ? ['Headgear', outfit.Headgear]
    : ['Eyewear', outfit.Eyewear ?? presets.NightVisionMount]
}

export function upgradeDraftOutfitMount(inventory: Inventory, outfitId: string): Inventory {
  const outfit = clothingPreset(outfitId)
  const strap = inventory.Eyewear
  const goggles = strap?.attachments.NVG
  if (!outfit.Headgear || inventory.Headgear || strap?.id !== presets.NightVisionMount || !goggles)
    return inventory

  const helmet = newItem(outfit.Headgear)
  helmet.attachments.NVG = goggles
  const updated: Inventory = { ...inventory, Headgear: helmet }
  delete updated.Eyewear
  return updated
}

function issuedPlayerGear(outfit: ClothingPreset, kitId: string): Inventory {
  const modern = modernLoadouts.Kits.some((kit) => kit.Id === kitId)
  const native = [...presets.Kits, ...presets.OperatorKits].find((kit) => kit.Id === kitId)
  const workshop = modern || (native && 'WorkshopItems' in native)
  const kitGoggles = modern
    ? modernLoadouts.Equipment.NightVisionClass
    : native && 'NightVisionClass' in native
      ? native.NightVisionClass
      : undefined
  const standardGoggles = typeof kitGoggles === 'string' ? kitGoggles : 'NVGoggles'
  const [mountSlot, mountClass] = outfitMount(outfit)
  const mount = newItem(mountClass)
  const goggles =
    workshop && outfit.WorkshopNightVision
      ? outfit.WorkshopNightVision
      : (outfit.NightVision ?? standardGoggles)
  mount.attachments.NVG = newItem(goggles)
  const vest = newItem(outfit.Vest ?? 'PlateCarrierVest')
  vest.attachments.VestPouch = newItem(outfit.Pouches ?? 'PlateCarrierPouches')
  return {
    [mountSlot]: mount,
    Vest: vest,
    Gloves: newItem(outfit.Gloves ?? 'TacticalGloves_Black'),
  }
}

function transferCarriedItems(
  previous: InventoryItem,
  garment: InventoryItem,
  displaced: InventoryItem[],
): void {
  for (const [slot, part] of Object.entries(previous.attachments)) {
    const replacement = garment.attachments[slot]
    if (replacement) transferCarriedItems(part, replacement, displaced)
    else if (attachmentChoices(garment, slot).some((choice) => choice.id === part.id))
      garment.attachments[slot] = part
    else displaced.push(part)
  }
  const host = gearById.get(garment.id)
  for (const placement of previous.cargo) {
    if (host && canPlace(host, garment.cargo, placement)) garment.cargo.push(placement)
    else displaced.push(placement.item)
  }
}

export function changeOutfit(
  inventory: Inventory,
  outfitId: string,
  previousOutfitId: string,
  kitId: string,
): Inventory | null {
  const outfit = clothingPreset(outfitId)
  const previousOutfit = clothingPreset(previousOutfitId)
  const worn = wornClothing(outfit)
  if (outfit.Headgear || outfit.Eyewear || previousOutfit.Headgear || previousOutfit.Eyewear)
    Object.assign(worn, issuedPlayerGear(outfit, kitId))
  const updated = { ...inventory }
  const displaced: InventoryItem[] = []
  const [mountSlot] = outfitMount(outfit)
  const [oldMountSlot, oldMountClass] = outfitMount(previousOutfit)
  const otherSlot = mountSlot === 'Headgear' ? 'Eyewear' : 'Headgear'
  const otherMount = inventory[otherSlot]
  if (
    worn[mountSlot] &&
    otherMount &&
    ((otherSlot === oldMountSlot && otherMount.id === oldMountClass) ||
      (mountSlot === 'Headgear' && gearById.get(otherMount.id)?.accepts.includes('NVG')))
  ) {
    transferCarriedItems(otherMount, worn[mountSlot], displaced)
    delete updated[otherSlot]
  }
  for (const [slot, garment] of Object.entries(worn)) {
    const previous = inventory[slot]
    if (previous) transferCarriedItems(previous, garment, displaced)
    updated[slot] = garment
  }
  for (const item of displaced) {
    const fits = ['Body', 'Legs', 'Feet', 'Back', 'Vest'].some((slot) => {
      const container = updated[slot]
      const host = container && gearById.get(container.id)
      const placement = host && placeInCargo(host, container.cargo, item)
      if (!placement) return false
      updated[slot] = { ...container, cargo: [...container.cargo, placement] }
      return true
    })
    if (fits) continue
    const vest = updated.Vest
    const pouch = vest?.attachments.VestPouch
    const host = pouch && gearById.get(pouch.id)
    const placement = host && placeInCargo(host, pouch.cargo, item)
    if (!placement) return null
    updated.Vest = {
      ...vest,
      attachments: {
        ...vest.attachments,
        VestPouch: { ...pouch, cargo: [...pouch.cargo, placement] },
      },
    }
  }
  return updated
}

export function starterInventory(id: string, outfitId: string): Inventory {
  const modern = modernLoadouts.Kits.find((kit) => kit.Id === id)
  const preset = modern ?? [...presets.Kits, ...presets.OperatorKits].find((kit) => kit.Id === id)
  if (!preset) return {}
  const outfit = clothingPreset(outfitId)
  const root: Inventory = {
    ...wornClothing(outfit),
    ...issuedPlayerGear(outfit, id),
    Hands: newItem(preset.FirearmClass),
  }
  const weapon = gearById.get(preset.FirearmClass)
  const parts = [...preset.Attachments, preset.SuppressorClass]
  if (modern) parts.push(modernLoadouts.Equipment.ModuleClass)
  for (const part of parts) {
    const slot = weapon?.accepts.find((option) =>
      compatibleGear(option, weapon).some((item) => item.id === part),
    )
    if (slot) root.Hands.attachments[slot] = newItem(part)
  }
  const pouches = root.Vest.attachments.VestPouch
  root.Hands.attachments['@magazine'] = newItem(preset.MagazineClass)
  const supplies = [
    ...Array<string>(4).fill('BandageDressing'),
    'Morphine',
    'Epinephrine',
    'Compass',
    'ChernarusMap',
  ]
  const containers = [pouches, root.Legs, root.Body]
  const spareLimit = modern ? modernLoadouts.Equipment.SpareMagazines : playerSpareMagazines
  for (let count = spareLimit; count >= 0; count--) {
    for (const container of containers) container.cargo = []
    const items = [...Array<string>(count).fill(preset.MagazineClass), ...supplies]
    const fits = items.every((id) => {
      const item = newItem(id)
      if (id === preset.MagazineClass) item.amount = gearById.get(id)?.capacity
      return containers.some((container) => {
        const host = gearById.get(container.id)
        const placement = host && placeInCargo(host, container.cargo, item)
        if (!placement) return false
        container.cargo.push(placement)
        return true
      })
    })
    if (fits) break
  }
  return root
}
