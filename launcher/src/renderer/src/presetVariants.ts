import type { Inventory, InventoryItem, LoadoutPreset, Placement } from '../../shared/types'
import { gearById, isNightVisionOptic, isWeaponOptic, mountedOptic } from './operationInventory'

function goggles(id: string): boolean {
  return gearById.get(id)?.slots.some((slot) => slot.toLowerCase() === 'nvg') ?? false
}

function nightGear(id: string): boolean {
  return (
    goggles(id) || isNightVisionOptic(id) || /^(NVGHeadstrap|PredateOperatorHeadstrap_)/.test(id)
  )
}

function visionCounts(inventory: Inventory): { goggles: number; optics: number } {
  let goggleCount = 0
  let optics = 0
  function visit(item: InventoryItem): void {
    if (goggles(item.id)) goggleCount++
    if (isNightVisionOptic(item.id)) optics++
    Object.values(item.attachments).forEach(visit)
    item.cargo.forEach(({ item }) => visit(item))
  }
  Object.values(inventory).forEach(visit)
  return { goggles: goggleCount, optics }
}

function position({ row, column, rotated }: Placement): string {
  return `${row}:${column}:${rotated}`
}

function cargoWithoutVision(cargo: Placement[], other: Placement[]): Inventory {
  return Object.fromEntries(
    cargo
      .filter(
        (entry) =>
          !nightGear(entry.item.id) &&
          !(
            isWeaponOptic(entry.item.id) &&
            other.some(
              (candidate) =>
                position(candidate) === position(entry) && isNightVisionOptic(candidate.item.id),
            )
          ),
      )
      .map((entry) => [position(entry), entry.item]),
  )
}

function sameGear(
  left: Inventory,
  right: Inventory,
  leftScope?: string,
  rightScope?: string,
): boolean {
  const slots = (inventory: Inventory, scope?: string): string[] =>
    Object.keys(inventory).filter((slot) => slot !== scope && !nightGear(inventory[slot].id))
  const leftSlots = slots(left, leftScope)
  const rightSlots = new Set(slots(right, rightScope))
  return (
    leftSlots.length === rightSlots.size &&
    leftSlots.every((slot) => {
      if (!rightSlots.has(slot)) return false
      const a = left[slot]
      const b = right[slot]
      if (a.id !== b.id || a.amount !== b.amount) return false

      const aNightScope = Object.values(a.attachments).some((item) => isNightVisionOptic(item.id))
      const bNightScope = Object.values(b.attachments).some((item) => isNightVisionOptic(item.id))
      return (
        sameGear(
          a.attachments,
          b.attachments,
          bNightScope && !aNightScope ? mountedOptic(a)?.[0] : undefined,
          aNightScope && !bNightScope ? mountedOptic(b)?.[0] : undefined,
        ) && sameGear(cargoWithoutVision(a.cargo, b.cargo), cargoWithoutVision(b.cargo, a.cargo))
      )
    })
  )
}

function nameVariant(name: string): { base: string; night: boolean } {
  const words: string[] = name.toLowerCase().match(/[\p{L}\p{N}]+/gu) ?? []
  return {
    base: words.filter((word) => word !== 'night').join(' '),
    night: words.includes('night'),
  }
}

function counterpart(preset: LoadoutPreset, presets: LoadoutPreset[]) {
  const others = presets.filter((item) => item.id !== preset.id && item.kind === preset.kind)
  const name = nameVariant(preset.name)
  const named = others.filter((item) => {
    const option = nameVariant(item.name)
    return !!name.base && option.base === name.base && option.night !== name.night
  })
  if (named.length) return named.length === 1 ? { other: named[0], night: name.night } : undefined

  const vision = visionCounts(preset.inventory)
  const matches = others.flatMap((other) => {
    const compared = visionCounts(other.inventory)
    const gogglesDiff = vision.goggles - compared.goggles
    const opticsDiff = vision.optics - compared.optics
    if (
      (!gogglesDiff && !opticsDiff) ||
      gogglesDiff * opticsDiff < 0 ||
      !sameGear(preset.inventory, other.inventory)
    )
      return []
    return [{ other, night: gogglesDiff > 0 || opticsDiff > 0 }]
  })
  return matches.length === 1 ? matches[0] : undefined
}

export function presetForLight(
  preset: LoadoutPreset,
  presets: LoadoutPreset[],
  night: boolean,
): LoadoutPreset {
  const pair = counterpart(preset, presets)
  return pair && pair.night !== night && counterpart(pair.other, presets)?.other.id === preset.id
    ? pair.other
    : preset
}
