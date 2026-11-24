import { useCallback, useEffect, useRef, useState, type FormEvent, type ReactNode } from 'react'
import type {
  Operation,
  Overview,
  Preferences,
  SessionAction,
  SessionStatus,
} from '../../shared/types'
import { huntMapName } from '../../shared/types'
import wordmark from '../../../../flash/predate_wordmark.png'
import { HuntPicker } from './HuntPicker'
import { LauncherMusic } from './LauncherMusic'
import { OperationSetup, readOperationDraft } from './OperationSetup'

type Page = 'Continue' | 'New Hunt' | 'Settings' | 'Backups'
type SettingsValues = Omit<Preferences, 'schema' | 'selected_world'>

const menu = ['New Hunt', 'Settings', 'Backups'] as const
const busyPhases = new Set([
  'starting',
  'loading',
  'launching',
  'connecting',
  'saving',
  'stopping',
  'pausing',
])
const phaseLabels: Record<string, string> = {
  starting: 'Preparing Hunt…',
  loading: 'Loading Hunt…',
  launching: 'Loading Hunt…',
  connecting: 'Loading Hunt…',
  playing: 'In Hunt',
  pausing: 'Pausing…',
  paused: 'Paused',
  reconnecting: 'Hunt Interrupted',
  saving: 'Saving…',
  stopping: 'Saving and Quitting…',
  suspended: 'Saved',
}

const actionErrors: Record<SessionAction['action'], string> = {
  continue: 'Couldn’t open the hunt. Try Continue again.',
  new: 'Couldn’t start the hunt. Check your setup and try again.',
  quit: 'Couldn’t finish saving. Try Save and Quit again.',
  backup: 'Couldn’t create a backup. Try again.',
  restore: 'Couldn’t restore the backup. Try another backup.',
  delete: 'Couldn’t delete the save. Try again.',
}

const huntListError = 'Couldn’t load your hunts. Try again.'
const internalDetails =
  /dayz|\b(?:steam|battleye|servers?|connect\w*|reconnect\w*|disconnect\w*|remote|rcon|network|localhost|backend|bridge|supervisor|ipc|native|script|process|dependency)\b/i
const playerValidation = [
  /^(?:Starting inventory|Unknown starting (?:inventory item|kit)|Invalid starting inventory|Too many levels|Cargo overlaps|Two items occupy)/i,
  /^(?:Invalid (?:container|position|quantity|placement|cargo position|(?:operation|hunt) name|new (?:operation|hunt) settings)|Use (?:an operation|a hunt) name|Set (?:a valid|whether|a respawn))/i,
  /^(?:Choose (?:a supported hunt map|a spawn|one, three|1, 3)|Unknown (?:Chernarus|Livonia|Sakhal|Nasdara) spawn|Both (?:Chernarus|Livonia|Sakhal|Nasdara) coordinates|The selected spawn|Livonia mission input|The server configuration does not select|DayZ opened |That operation already exists)/i,
  /^(?:Unsupported launcher settings|Backup retention|Compass|This operation.s mod|Mod content changed|Mod changed while|Additional mod is selected|Use Save and Quit|Select a starting kit|Select a backup)/i,
  /^(?:Save and quit the current hunt before deleting saves|That hunt no longer exists|The original hunt is protected|Select a save)/i,
  /\b(?:does not fit in|is incompatible with)\b/i,
]

function errorMessage(error: unknown, fallback: string): string {
  const message = (error instanceof Error ? error.message : String(error))
    .replace(/^Error invoking remote method '[^']+': Error: /, '')
    .trim()
  if (message === 'Steam is not running. Open Steam, then try again.') return message
  if (/^(?:Sakhal|Nasdara) (?:requires|needs its|map files differ)/.test(message)) return message
  if (/^DayZ is already open|^This world's server was started separately/i.test(message))
    return 'A game is already open. Close it before starting this hunt.'
  if (/^The server (had already )?stopped unexpectedly/i.test(message))
    return 'The hunt ended unexpectedly. Select Continue to recover your last backup.'
  if (/^The paused operation could not reconnect/i.test(message))
    return 'The hunt couldn’t resume. Use Save and Quit to keep your progress.'
  if (/^DayZ has not acknowledged Save and Quit/i.test(message)) return actionErrors.quit
  if (/missing mod folder/i.test(message))
    return 'This hunt needs a mod that is missing. Restore its mod folder and try again.'
  if (/^Mod folder must contain/i.test(message)) return 'Choose a valid mod folder.'
  if (/^Additional mod is selected more than once/i.test(message))
    return 'That mod is already added.'
  if (
    /^Mod content changed|^This operation.s mod (?:preset|order or folders) changed/i.test(message)
  )
    return 'The mods for this hunt have changed. Restore the previous mods or start a new hunt.'
  if (/^This snapshot requires a different game/i.test(message))
    return 'This backup was made with a different version of the game.'
  if (/^An older PREDATE session/i.test(message))
    return 'Restart your computer, then reopen PREDATE and try again.'

  // Keep player-facing validation details while leaving startup and runtime diagnostics in the logs.
  if (
    internalDetails.test(message) ||
    /[\r\n]|[a-z]:[\\/]|(?:\.exe|\.log|\.pbo|\.py)\b/i.test(message)
  )
    return fallback
  if (!playerValidation.some((pattern) => pattern.test(message))) return fallback
  return message
    .replace(/\boperation(s?)\b/gi, (word, plural: string) => {
      const noun = word.startsWith('O') ? 'Hunt' : 'hunt'
      return noun + plural
    })
    .replace(/\ban hunt\b/gi, 'a hunt')
}

function Settings({
  preferences,
  save,
  showError,
}: {
  preferences: Preferences
  save: (values: SettingsValues) => Promise<void>
  showError: (message: string) => void
}): ReactNode {
  const [limit, setLimit] = useState(String(preferences.backup_limit))
  const [mods, setMods] = useState(preferences.extra_mods)
  const [scale, setScale] = useState(String(preferences.receiver_scale))
  const [contrast, setContrast] = useState(preferences.receiver_high_contrast)
  const [saving, setSaving] = useState(false)
  const [saved, setSaved] = useState(false)

  const addMod = async (): Promise<void> => {
    try {
      const folder = await window.predate.chooseMod()
      if (folder) {
        setMods((current) => [...new Set([...current, folder])])
        setSaved(false)
      }
    } catch (error) {
      showError(errorMessage(error, 'Couldn’t open the mod picker. Try again.'))
    }
  }

  const submit = async (event: FormEvent): Promise<void> => {
    event.preventDefault()
    setSaving(true)
    setSaved(false)
    try {
      await save({
        reconnect_grace: preferences.reconnect_grace,
        backup_limit: Number(limit),
        offline: false,
        extra_mods: mods,
        receiver_scale: Number(scale),
        receiver_high_contrast: contrast,
      })
      setSaved(true)
    } catch (error) {
      showError(errorMessage(error, 'Couldn’t save your settings. Try again.'))
    } finally {
      setSaving(false)
    }
  }

  return (
    <form onSubmit={submit} onChange={() => setSaved(false)}>
      <fieldset disabled={saving}>
        <label className="setting">
          Compass Size
          <input
            aria-label="Compass Size in Percent"
            type="number"
            min="80"
            max="150"
            step="10"
            required
            value={scale}
            onChange={(event) => setScale(event.target.value)}
          />
          <span className="unit">%</span>
        </label>
        <label className="setting">
          Compass High Contrast
          <input
            aria-label="Compass High Contrast"
            type="checkbox"
            checked={contrast}
            onChange={(event) => setContrast(event.target.checked)}
          />
        </label>
        <label className="setting">
          Backups to Keep
          <input
            type="number"
            min="1"
            max="200"
            required
            value={limit}
            onChange={(event) => setLimit(event.target.value)}
          />
        </label>
        <div className="mods">
          <div className="split">
            <div>
              <h2>Extra Mods</h2>
              <p className="hint">For new hunts.</p>
            </div>
            <button type="button" onClick={() => void addMod()}>
              Add Folder
            </button>
          </div>
          {mods.map((mod) => {
            const name = mod.split(/[\\/]/).pop()
            return (
              <div className="mod-row split" key={mod}>
                <span title={name}>{name}</span>
                <button
                  type="button"
                  className="quiet"
                  aria-label={'Remove ' + name}
                  onClick={() => {
                    setMods((current) => current.filter((value) => value !== mod))
                    setSaved(false)
                  }}
                >
                  Remove
                </button>
              </div>
            )
          })}
        </div>
        <div className="form-actions">
          <button className="primary">{saving ? 'Saving…' : 'Save Settings'}</button>
          <span className="hint" role="status">
            {saved && 'Settings Saved'}
          </span>
        </div>
      </fieldset>
    </form>
  )
}

function DeleteSaveDialog({
  name,
  close,
  confirm,
}: {
  name: string
  close: () => void
  confirm: () => void
}): ReactNode {
  const dialog = useRef<HTMLDialogElement>(null)
  useEffect(() => {
    dialog.current?.showModal()
  }, [])

  return (
    <dialog
      ref={dialog}
      className="gear-picker preset-dialog delete-dialog"
      aria-labelledby="delete-save-title"
      aria-describedby="delete-save-description"
      onCancel={close}
    >
      <div className="stack">
        <h2 id="delete-save-title">Delete Save?</h2>
        <p id="delete-save-description">
          Permanently delete <strong>{name}</strong>? Existing backups will be kept.
        </p>
        <div className="setup-actions">
          <button onClick={close} autoFocus>
            Cancel
          </button>
          <button className="danger" onClick={confirm}>
            Delete Save
          </button>
        </div>
      </div>
    </dialog>
  )
}

export function App(): ReactNode {
  const [page, setPage] = useState<Page>('Continue')
  const heading = useRef<HTMLHeadingElement>(null)
  const [overview, setOverview] = useState<Overview | null>(null)
  const [status, setStatus] = useState<SessionStatus>({ phase: 'idle', message: '' })
  const [selected, setSelected] = useState('')
  const [pending, setPending] = useState<SessionAction['action'] | null>(null)
  const [quitting, setQuitting] = useState(false)
  const [error, setError] = useState('')
  const [notice, setNotice] = useState('')
  const [draft, setDraft] = useState(readOperationDraft)
  const [backupFile, setBackupFile] = useState('')
  const [restoreName, setRestoreName] = useState('Restored Hunt')
  const [deleteTarget, setDeleteTarget] = useState<Operation | null>(null)

  const refresh = useCallback(async (selectPreferred = false): Promise<void> => {
    const result = await window.predate.overview()
    setOverview(result)
    setStatus(result.status)
    setSelected((current) => {
      const preferred = selectPreferred || !current ? result.preferences.selected_world : current
      return (
        result.operations.find((item) => item.id === preferred)?.id ??
        result.operations.find((item) => item.id === result.preferences.selected_world)?.id ??
        result.operations[0]?.id ??
        ''
      )
    })
  }, [])

  useEffect(() => {
    if (!window.predate) {
      setError('PREDATE couldn’t open. Close the launcher and try again.')
      return
    }
    void refresh().catch((error) => setError(errorMessage(error, huntListError)))
    const unsubscribeStatus = window.predate.onStatus(setStatus)
    const unsubscribeContinueError = window.predate.onContinueError((message) => {
      setError(errorMessage(message, actionErrors.continue))
    })
    return () => {
      unsubscribeStatus()
      unsubscribeContinueError()
    }
  }, [refresh])

  useEffect(() => {
    // Game-exit saves can update the backup history independently of the launcher.
    if (window.predate && ['suspended', 'error'].includes(status.phase))
      void refresh().catch((error) => setError(errorMessage(error, huntListError)))
  }, [status.phase, refresh])

  useEffect(() => {
    heading.current?.focus()
    void window.predate?.setSetupOpen?.(page === 'New Hunt').catch(() => {})
  }, [page])

  const openPage = (next: Page): void => {
    setPage(next)
    setNotice('')
  }

  const perform = async (action: SessionAction): Promise<void> => {
    const stop = action.action === 'quit'
    if (stop) setQuitting(true)
    else setPending(action.action)
    setError('')
    setNotice('')
    try {
      await window.predate.perform(action)
      const newSelection = action.action === 'new' || action.action === 'restore'
      await refresh(newSelection)
      if (newSelection) {
        setDraft((current) => ({ ...current, name: '' }))
        setBackupFile('')
        setPage('Continue')
      }
      if (action.action === 'backup') setNotice('Backup created')
      if (action.action === 'delete') setNotice('Save deleted')
    } catch (error) {
      setError(errorMessage(error, actionErrors[action.action]))
    } finally {
      if (stop) setQuitting(false)
      else setPending(null)
    }
  }

  const savePreferences = async (values: SettingsValues): Promise<void> => {
    setError('')
    await window.predate.savePreferences(values)
    await refresh()
  }

  const busy = pending !== null || quitting || busyPhases.has(status.phase)
  const active = !['idle', 'suspended'].includes(status.phase)
  const operation = overview?.operations.find((item) => item.id === selected)
  const operationRunning = status.phase === 'playing' && status.profile === selected
  const inHunt =
    (active && status.phase !== 'error') || quitting || pending === 'continue' || pending === 'new'
  const canStop = inHunt || status.phase === 'error'
  const selectedBackup = overview?.backups.find((item) => item.file === backupFile)
  const phase = phaseLabels[status.phase]
  const sessionNote = quitting
    ? 'Saving and Quitting…'
    : (active ? phase : undefined) || (pending ? 'Working…' : notice || phase)
  const failure =
    status.phase === 'error'
      ? errorMessage(
          status.error || status.message,
          'The hunt was interrupted. Select Continue to try again.',
        )
      : ''
  const onHome = page === 'Continue'
  const selectedMap = operation?.map
  useEffect(() => {
    if (page !== 'New Hunt' && !(page === 'Continue' && (selectedMap === 'sakhal' || selectedMap === 'nasdara'))) return
    let mounted = true
    const updateMaps = (refresh = false): void => {
      void window.predate.maps(refresh).then((maps) => {
        if (mounted) setOverview((value) => value ? { ...value, maps } : value)
      }).catch(() => {})
    }
    updateMaps(true)
    const interval = setInterval(() => updateMaps(true), 2000)
    return () => { mounted = false; clearInterval(interval) }
  }, [page, selectedMap])

  return (
    <div className="app-shell">
      <div className="titlebar" aria-hidden="true" />
      <div className="screen">
        {!onHome && (
          <header className="page-header split">
            <h1 ref={heading} tabIndex={-1}>
              {page}
            </h1>
            <button className="quiet" onClick={() => openPage('Continue')}>
              ← Back
            </button>
          </header>
        )}
        {(error || failure) && (
          <div className="error-banner split" role="alert">
            <span>{error || failure}</span>
            {overview && error && error !== failure && (
              <button className="quiet" aria-label="Dismiss Error" onClick={() => setError('')}>
                ×
              </button>
            )}
          </div>
        )}
        <main
          key={page}
          className={onHome ? 'home-view' : page === 'New Hunt' ? 'setup-view' : undefined}
        >
          {onHome && (
            <div className="home">
              <h1 className="wordmark" ref={heading} tabIndex={-1}>
                <img src={wordmark} alt="PREDATE" width="1965" height="400" />
              </h1>
              {overview ? (
                <>
                  <HuntPicker
                    hunts={overview.operations}
                    selected={operation}
                    disabled={busy}
                    deleteDisabled={busy || inHunt}
                    select={setSelected}
                    requestDelete={setDeleteTarget}
                  />
                  <button
                    className="primary play"
                    disabled={busy || !operation || operationRunning}
                    onClick={() => void perform({ action: 'continue', profile: selected })}
                  >
                    <span className="play-arrow" aria-hidden="true" />
                    {['paused', 'reconnecting'].includes(status.phase) &&
                    status.profile === selected
                      ? 'Resume'
                      : 'Continue'}
                  </button>
                  <nav aria-label="Launcher Menu">
                    {menu.map((item) => (
                      <button key={item} onClick={() => openPage(item)}>
                        {item}
                      </button>
                    ))}
                  </nav>
                </>
              ) : !error ? (
                <p className="home-message" role="status">
                  Loading…
                </p>
              ) : (
                window.predate && (
                  <button
                    className="retry"
                    onClick={() => {
                      setError('')
                      void refresh().catch((error) => setError(errorMessage(error, huntListError)))
                    }}
                  >
                    Retry
                  </button>
                )
              )}
            </div>
          )}
          {overview && page === 'New Hunt' && (
            <OperationSetup
              overview={overview}
              draft={draft}
              update={setDraft}
              busy={busy}
              active={active && status.phase !== 'error'}
              create={perform}
              showError={setError}
            />
          )}
          {overview && page === 'Settings' && (
            <Settings
              preferences={overview.preferences}
              save={savePreferences}
              showError={setError}
            />
          )}
          {overview && page === 'Backups' && (
            <div className="stack">
              {overview.backups.length ? (
                <label className="field">
                  Snapshot
                  <select
                    value={selectedBackup?.file ?? ''}
                    onChange={(event) => setBackupFile(event.target.value)}
                  >
                    <option value="">Select a Snapshot</option>
                    {overview.backups.map((item) => (
                      <option key={item.file} value={item.file}>
                        {new Date(item.created).toLocaleString(undefined, {
                          dateStyle: 'medium',
                          timeStyle: 'short',
                        })}{' '}
                        · {item.name} · {huntMapName(item.map)}
                      </option>
                    ))}
                  </select>
                </label>
              ) : (
                <p className="hint">No snapshots yet.</p>
              )}
              {selectedBackup && (
                <form
                  className="stack"
                  onSubmit={(event) => {
                    event.preventDefault()
                    if (!busy && restoreName.trim())
                      void perform({
                        action: 'restore',
                        backup: selectedBackup.file,
                        name: restoreName,
                      })
                  }}
                >
                  <label className="field">
                    Restored Hunt Name
                    <input
                      id="restore-name"
                      maxLength={80}
                      required
                      value={restoreName}
                      onChange={(event) => setRestoreName(event.target.value)}
                    />
                  </label>
                  <button className="primary" disabled={busy || !restoreName.trim()}>
                    Restore Copy
                  </button>
                </form>
              )}
              {operation && (
                <button
                  className="snapshot-button"
                  title={operation.name}
                  disabled={busy || active}
                  onClick={() => void perform({ action: 'backup', profile: selected })}
                >
                  Back Up {operation.name}
                </button>
              )}
            </div>
          )}
        </main>
        <footer className="session-footer split">
          <span className="session-status" role="status">
            {sessionNote}
          </span>
          {canStop && (
            <button
              className="save-and-quit"
              disabled={quitting || status.phase === 'saving' || status.phase === 'stopping'}
              onClick={() => void perform({ action: 'quit' })}
            >
              Save and Quit
            </button>
          )}
        </footer>
      </div>
      {deleteTarget && (
        <DeleteSaveDialog
          name={`${deleteTarget.name} · ${huntMapName(deleteTarget.map)}`}
          close={() => setDeleteTarget(null)}
          confirm={() => {
            setDeleteTarget(null)
            void perform({ action: 'delete', profile: deleteTarget.id })
          }}
        />
      )}
      <LauncherMusic inHunt={inHunt} />
    </div>
  )
}
