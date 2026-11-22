import { useEffect, useRef, useState, type ReactNode } from 'react'

export function SavePresetDialog({
  title,
  save,
  close,
}: {
  title: string
  save: (name: string) => Promise<void>
  close: () => void
}): ReactNode {
  const dialog = useRef<HTMLDialogElement>(null)
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
            await save(name.trim())
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
        <h2 id="preset-title">{title}</h2>
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
