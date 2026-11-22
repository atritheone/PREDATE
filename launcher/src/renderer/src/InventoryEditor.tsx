import {
  createContext,
  useContext,
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from 'react'
import type { WeaponModulePreview } from '../../shared/types'
import {
  attachmentChoices,
  canPlace,
  compatibleGear,
  dimensions,
  equipment,
  gear,
  gearById,
  newItem,
  placeInCargo,
  requiresLightRail,
  slotLabel,
  updateAttachment,
  workshopGearIds,
  type Gear,
  type Inventory,
  type InventoryItem,
  type Placement,
} from './operationInventory'

interface PickerRequest {
  title: string
  options: Gear[]
  attachmentSlot?: string
  choose: (item: Gear) => void
  fits?: (item: Gear) => boolean
}

type OpenPicker = (request: PickerRequest) => void

const searchAliases: Record<string, string> = {
  NVGoggles: 'nvg nvgs nightvision',
  NVGHeadstrap: 'nvg nvgs night vision mount',
}

const WeaponModuleContext = createContext<WeaponModulePreview | undefined>(undefined)

function itemVariant(id: string): string {
  return id.replace(/_dayz\b/i, '_Logo')
}

function itemLabel(
  item: Pick<Gear, 'id' | 'name'>,
  slot?: string,
  weaponModule?: WeaponModulePreview,
): WeaponModulePreview {
  if (
    item.id === 'PredateWeaponModule' &&
    slot?.toLowerCase() === 'weaponflashlight' &&
    weaponModule
  )
    return weaponModule
  return { name: item.name, className: itemVariant(item.id) }
}

function ItemPicker({
  request,
  showPicker,
}: {
  request: PickerRequest
  showPicker: (value: PickerRequest | null) => void
}): ReactNode {
  const dialog = useRef<HTMLDialogElement>(null)
  const [query, setQuery] = useState('')
  const [kind, setKind] = useState('')
  const weaponModule = useContext(WeaponModuleContext)
  const matches = useMemo(() => {
    const words = query.toLowerCase().trim().split(/\s+/)
    return request.options.filter((item) => {
      const label = itemLabel(item, request.attachmentSlot, weaponModule)
      const text = [label.name, label.className, searchAliases[item.id]].join(' ').toLowerCase()
      return (!kind || item.kind === kind) && words.every((word) => text.includes(word))
    })
  }, [request.options, request.attachmentSlot, query, kind, weaponModule])
  useEffect(() => {
    dialog.current?.showModal()
    dialog.current?.querySelector('input')?.focus()
  }, [])

  return (
    <dialog
      className="gear-picker"
      ref={dialog}
      aria-labelledby="picker-title"
      onCancel={() => showPicker(null)}
    >
      <header className="split">
        <h2 id="picker-title">{request.title}</h2>
        <button
          type="button"
          className="quiet"
          aria-label="Close Item Picker"
          onClick={() => showPicker(null)}
        >
          ×
        </button>
      </header>
      <div className="picker-filters">
        <input
          aria-label="Search Items"
          placeholder="Search items or class names"
          value={query}
          onChange={(event) => setQuery(event.target.value)}
          autoFocus
        />
        {request.options.length > 100 && (
          <select
            aria-label="Item Category"
            value={kind}
            onChange={(event) => setKind(event.target.value)}
          >
            <option value="">All Items</option>
            <option value="item">Gear & Supplies</option>
            <option value="weapon">Weapons</option>
            <option value="magazine">Ammo & Magazines</option>
          </select>
        )}
      </div>
      <div className="picker-items">
        {matches.map((item) => {
          const fits = request.fits?.(item) ?? true
          const label = itemLabel(item, request.attachmentSlot, weaponModule)
          return (
            <button
              key={item.id}
              type="button"
              className="picker-item"
              disabled={!fits}
              onClick={() => {
                request.choose(item)
                showPicker(null)
              }}
            >
              <span>
                <strong>{label.name}</strong>
                <small>{label.className}</small>
              </span>
              <span>{fits ? item.width + '×' + item.height : 'No Space'}</span>
            </button>
          )
        })}
        {!matches.length && <p className="hint">No matching items.</p>}
      </div>
    </dialog>
  )
}

function Cargo({
  host,
  item,
  update,
  pick,
}: {
  host: Gear
  item: InventoryItem
  update: (item: InventoryItem) => void
  pick: OpenPicker
}): ReactNode {
  const [selected, setSelected] = useState<number | null>(null)
  const current = selected === null ? undefined : item.cargo[selected]
  const others = item.cargo.filter((_, index) => index !== selected)
  const change = (next: Placement): void =>
    update({
      ...item,
      cargo: item.cargo.map((placement, index) => (index === selected ? next : placement)),
    })
  const move = (row: number, column: number): void => {
    if (!current) return
    const next = { ...current, row, column }
    if (canPlace(host, others, next)) change(next)
  }

  return (
    <section className="cargo">
      <div className="split inventory-subhead">
        <h3>
          Cargo {host.cargoWidth}×{host.cargoHeight}
        </h3>
        <button
          type="button"
          onClick={() =>
            pick({
              title: 'Add to ' + host.name,
              options: gear,
              fits: (option) => !!placeInCargo(host, item.cargo, newItem(option.id)),
              choose: (option) => {
                const placement = placeInCargo(host, item.cargo, newItem(option.id))
                if (placement) {
                  setSelected(item.cargo.length)
                  update({ ...item, cargo: [...item.cargo, placement] })
                }
              },
            })
          }
        >
          + Add Item
        </button>
      </div>
      <div
        className="cargo-grid"
        style={{ '--columns': host.cargoWidth, '--rows': host.cargoHeight } as React.CSSProperties}
        aria-label={host.name + ' Cargo Grid'}
      >
        {item.cargo.map((placement, index) => {
          const [width, height] = dimensions(placement)
          const info = gearById.get(placement.item.id)
          return (
            <button
              key={index}
              className="cargo-tile"
              type="button"
              aria-pressed={selected === index}
              aria-label={
                (info?.name ?? placement.item.id) +
                ', row ' +
                (placement.row + 1) +
                ', column ' +
                (placement.column + 1)
              }
              title={info?.name ?? placement.item.id}
              style={{
                gridColumn: placement.column + 1 + ' / span ' + width,
                gridRow: placement.row + 1 + ' / span ' + height,
              }}
              onClick={() => setSelected(index === selected ? null : index)}
            >
              {info?.name ?? placement.item.id}
            </button>
          )
        })}
      </div>
      {current && (
        <div className="cargo-selection">
          <div className="cargo-controls">
            <button
              type="button"
              disabled={!canPlace(host, others, { ...current, rotated: !current.rotated })}
              onClick={() => change({ ...current, rotated: !current.rotated })}
            >
              Rotate
            </button>
            {[
              ['←', 'Move Left', 0, -1],
              ['↑', 'Move Up', -1, 0],
              ['↓', 'Move Down', 1, 0],
              ['→', 'Move Right', 0, 1],
            ].map(([arrow, label, row, column]) => (
              <button
                key={label}
                type="button"
                aria-label={String(label)}
                disabled={
                  !canPlace(host, others, {
                    ...current,
                    row: current.row + Number(row),
                    column: current.column + Number(column),
                  })
                }
                onClick={() => move(current.row + Number(row), current.column + Number(column))}
              >
                {arrow}
              </button>
            ))}
            <button
              type="button"
              className="quiet"
              onClick={() => {
                update({ ...item, cargo: others })
                setSelected(null)
              }}
            >
              Remove
            </button>
          </div>
          <ItemDetails
            key={selected}
            item={current.item}
            update={(replacement) => change({ ...current, item: replacement })}
            pick={pick}
          />
        </div>
      )}
    </section>
  )
}

function ItemDetails({
  item,
  update,
  pick,
  attachmentSlot,
}: {
  item: InventoryItem
  update: (item: InventoryItem) => void
  pick: OpenPicker
  attachmentSlot?: string
}): ReactNode {
  const weaponModule = useContext(WeaponModuleContext)
  const info = gearById.get(item.id)
  if (!info) return null
  const label = itemLabel(info, attachmentSlot, weaponModule)
  const slots = [...info.accepts, ...(info.magazines.length ? ['@magazine'] : [])]
  const attach = (slot: string, part?: InventoryItem): void =>
    update(updateAttachment(item, slot, part))

  return (
    <div className="item-details">
      <div className="item-identity">
        <strong>{label.name}</strong>
        <small>{label.className}</small>
      </div>
      {info.capacity > 0 && (
        <label className="quantity-field">
          {info.kind === 'magazine' ? 'Rounds' : 'Quantity'}
          <input
            type="number"
            min={info.kind === 'magazine' ? 0 : 1}
            max={info.capacity}
            value={item.amount ?? ''}
            placeholder="Default"
            onChange={(event) => {
              const number = event.target.valueAsNumber
              const amount = Number.isFinite(number)
                ? Math.min(
                    info.capacity,
                    Math.max(info.kind === 'magazine' ? 0 : 1, Math.floor(number)),
                  )
                : undefined
              update({ ...item, amount })
            }}
          />
          <span className="hint">/ {info.capacity}</span>
        </label>
      )}
      {slots.length > 0 && (
        <section className="attachment-list">
          <h3>Attachments</h3>
          {slots.map((slot) => {
            const installed = item.attachments[slot]
            const options = attachmentChoices(item, slot)
            if (!options.length && !installed) return null
            const choose = (): void =>
              pick({
                title: slotLabel(slot),
                options,
                attachmentSlot: slot,
                choose: (part) => attach(slot, newItem(part.id)),
              })
            return installed ? (
              <details key={slot}>
                <summary>
                  <span>{slotLabel(slot)}</span>
                  <strong>
                    {
                      itemLabel(
                        {
                          id: installed.id,
                          name: gearById.get(installed.id)?.name ?? installed.id,
                        },
                        slot,
                        weaponModule,
                      ).name
                    }
                  </strong>
                </summary>
                <div className="attachment-body">
                  <div className="inventory-actions">
                    <button type="button" onClick={choose}>
                      Change
                    </button>
                    <button
                      type="button"
                      className="quiet"
                      disabled={requiresLightRail(item, slot)}
                      title={
                        requiresLightRail(item, slot) ? 'Remove the weapon light first' : undefined
                      }
                      onClick={() => attach(slot)}
                    >
                      Remove
                    </button>
                  </div>
                  <ItemDetails
                    key={installed.id}
                    item={installed}
                    attachmentSlot={slot}
                    update={(part) => attach(slot, part)}
                    pick={pick}
                  />
                </div>
              </details>
            ) : (
              <button key={slot} className="attachment-empty split" type="button" onClick={choose}>
                <span>{slotLabel(slot)}</span>
                <span>+ Add</span>
              </button>
            )
          })}
        </section>
      )}
      {info.cargoWidth > 0 && info.cargoHeight > 0 && (
        <Cargo host={info} item={item} update={update} pick={pick} />
      )}
    </div>
  )
}

export function InventoryEditor({
  inventory,
  update,
  includeWorkshopGear,
  weaponModule,
}: {
  inventory: Inventory
  update: (value: Inventory) => void
  includeWorkshopGear: boolean
  weaponModule?: WeaponModulePreview
}): ReactNode {
  const [slot, setSlot] = useState<string>('Hands')
  const [picker, setPicker] = useState<PickerRequest | null>(null)
  const entry = inventory[slot]
  const label = equipment.find(([id]) => id === slot)?.[1] ?? slot
  const pick: OpenPicker = (request) =>
    setPicker(
      includeWorkshopGear
        ? request
        : { ...request, options: request.options.filter((item) => !workshopGearIds.has(item.id)) },
    )
  const replace = (next?: InventoryItem): void => {
    const slots = { ...inventory }
    if (next) slots[slot] = next
    else delete slots[slot]
    update(slots)
  }
  const choose = (): void =>
    pick({
      title: label,
      options: compatibleGear(slot),
      choose: (item) => replace(newItem(item.id)),
    })
  return (
    <WeaponModuleContext.Provider value={weaponModule}>
      <div className="inventory-workspace">
        <div className="equipment-list" aria-label="Equipment Slots">
          {equipment.map(([id, name]) => {
            const item = inventory[id]
            return (
              <button
                key={id}
                type="button"
                className="equipment-slot"
                aria-pressed={slot === id}
                onClick={() => setSlot(id)}
              >
                <span>{name}</span>
                <strong title={item ? gearById.get(item.id)?.name : undefined}>
                  {item ? (gearById.get(item.id)?.name ?? item.id) : 'Empty'}
                </strong>
              </button>
            )
          })}
        </div>
        <div className="inventory-inspector">
          <header className="split inventory-subhead">
            <h2>{label}</h2>
            <div className="inventory-actions">
              <button type="button" onClick={choose}>
                {entry ? 'Change' : '+ Equip'}
              </button>
              {entry && (
                <button type="button" className="quiet" onClick={() => replace()}>
                  Clear
                </button>
              )}
            </div>
          </header>
          {entry ? (
            <ItemDetails key={slot + entry.id} item={entry} update={replace} pick={pick} />
          ) : (
            <p className="hint">Empty Slot</p>
          )}
        </div>
        {picker && <ItemPicker key={picker.title} request={picker} showPicker={setPicker} />}
      </div>
    </WeaponModuleContext.Provider>
  )
}
