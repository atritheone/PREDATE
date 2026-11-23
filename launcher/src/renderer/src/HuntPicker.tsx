import { useRef, type KeyboardEvent, type ReactNode } from 'react'
import type { Operation } from '../../shared/types'

export function HuntPicker({
  hunts,
  selected,
  disabled,
  deleteDisabled,
  select,
  requestDelete,
}: {
  hunts: Operation[]
  selected?: Operation
  disabled: boolean
  deleteDisabled: boolean
  select: (id: string) => void
  requestDelete: (hunt: Operation) => void
}): ReactNode {
  const trigger = useRef<HTMLButtonElement>(null)
  const options = useRef<HTMLUListElement>(null)

  const close = (): void => {
    if (!options.current?.matches(':popover-open')) return
    options.current.hidePopover()
    trigger.current?.focus()
  }

  const deleteButton = (hunt: Operation): ReactNode =>
    hunt.id !== 'main' && (
      <button
        type="button"
        className="hunt-delete"
        aria-label={`Delete save: ${hunt.name}`}
        title="Delete Save"
        disabled={deleteDisabled}
        onClick={() => {
          close()
          requestDelete(hunt)
        }}
      >
        ×
      </button>
    )

  const moveFocus = (event: KeyboardEvent<HTMLUListElement>): void => {
    if (event.key !== 'ArrowDown' && event.key !== 'ArrowUp') return
    event.preventDefault()
    const rows = [...event.currentTarget.querySelectorAll<HTMLButtonElement>('.hunt-option')]
    const current = rows.findIndex((row) => row.parentElement?.contains(event.target as Node))
    const direction = event.key === 'ArrowDown' ? 1 : -1
    rows[current + direction]?.focus()
  }

  return (
    <div className="field hunt-picker">
      <span id="hunt-label">Hunt</span>
      <div className="hunt-field">
        <button
          type="button"
          className="hunt-trigger"
          id="operation"
          aria-labelledby="hunt-label hunt-value"
          popoverTarget="hunt-options"
          ref={trigger}
          disabled={disabled || !selected}
          onClick={(event) => {
            event.preventDefault()
            if (options.current?.matches(':popover-open')) {
              close()
            } else {
              options.current?.showPopover({ source: event.currentTarget })
              options.current
                ?.querySelector<HTMLButtonElement>('[aria-current="true"]')
                ?.focus()
            }
          }}
          onKeyDown={(event) => {
            if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
              event.preventDefault()
              event.currentTarget.click()
            }
          }}
        >
          <span id="hunt-value">{selected?.name ?? 'No Saved Hunts'}</span>
        </button>
        {selected && deleteButton(selected)}
      </div>
      <ul
        className="hunt-options"
        id="hunt-options"
        aria-labelledby="hunt-label"
        popover="auto"
        ref={options}
        onKeyDown={moveFocus}
      >
        {hunts.map((hunt) => (
          <li key={hunt.id}>
            <button
              type="button"
              className="hunt-option"
              aria-current={hunt.id === selected?.id ? 'true' : undefined}
              title={hunt.name}
              disabled={disabled}
              onClick={() => {
                close()
                select(hunt.id)
              }}
            >
              {hunt.name}
            </button>
            {deleteButton(hunt)}
          </li>
        ))}
      </ul>
    </div>
  )
}
