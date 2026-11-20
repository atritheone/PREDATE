import { useEffect, useRef, useState, type ReactNode } from 'react'
import type { PresetKind } from '../../shared/types'

export function SavePresetDialog({
  save,
  close,
}: {
  save: (kind: PresetKind, name: string) => Promise<void>
  close: () => void
}): ReactNode {
  const dialog = useRef<HTMLDialogElement>(null)
  const [kind, setKind] = useState<PresetKind>('weapon')
  const [name, setName] = useState('')
  const [saving, setSaving] = useState(false)
  const [error, setError] = useState('')

  useEffect(() => {
    dialog.current?.showModal()
  }, [])

  return (
    <dialog
      className="gear-picker preset-dialog"
      ref={dialog}
      aria-labelledby="preset-title"
      onCancel={(event) => {
        event.preventDefault()
        if (!saving) close()
      }}
    >
      <form
        className="stack"
        onSubmit={async (event) => {
          event.preventDefault()
          if (!name.trim() || saving) return
          setError('')
          setSaving(true)
          try {
            await save(kind, name.trim())
            close()
          } catch (failure) {
            const message = failure instanceof Error ? failure.message : ''
            setError(
              message.includes('already has that name')
                ? 'A preset of this type already has that name.'
                : 'Could not save the preset.',
            )
            setSaving(false)
          }
        }}
      >
        <h2 id="preset-title">Save Preset</h2>
        <label className="field">
          Preset Type
          <select
            value={kind}
            onChange={(event) => setKind(event.target.value as PresetKind)}
            disabled={saving}
          >
            <option value="weapon">Weapons</option>
            <option value="equipment">Equipment</option>
          </select>
        </label>
        <label className="field">
          Preset Name
          <input
            value={name}
            onChange={(event) => setName(event.target.value)}
            maxLength={80}
            autoFocus
            disabled={saving}
          />
        </label>
        {error && (
          <p className="hint" role="alert">
            {error}
          </p>
        )}
        <div className="setup-actions">
          <button type="button" className="quiet" disabled={saving} onClick={close}>
            Cancel
          </button>
          <button type="submit" className="primary" disabled={!name.trim() || saving}>
            Save Preset
          </button>
        </div>
      </form>
    </dialog>
  )
}
