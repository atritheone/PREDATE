import { useEffect, useState, type FormEvent } from 'react'
import type { ProfileStat, ProfileStats } from '../../shared/types'

const wholeNumber = new Intl.NumberFormat()
const decimal = new Intl.NumberFormat(undefined, { maximumFractionDigits: 1 })
const percent = new Intl.NumberFormat(undefined, { style: 'percent', maximumFractionDigits: 1 })

function duration(seconds: number): string {
  const hours = Math.floor(seconds / 3600)
  const minutes = Math.floor((seconds % 3600) / 60)
  const remaining = `${minutes}m ${Math.floor(seconds % 60)}s`
  return hours ? `${wholeNumber.format(hours)}h ${remaining}` : remaining
}

function formatStat({ value, format }: ProfileStat): string {
  if (value === null) return '—'
  switch (format) {
    case 'text':
      return value
    case 'duration':
      return duration(value)
    case 'distance':
      return `${wholeNumber.format(value)} m`
    case 'percent':
      return percent.format(value)
    case 'decimal':
      return decimal.format(value)
    case 'count':
      return wholeNumber.format(value)
  }
}

export function ProfilePage({
  callsign,
  save,
}: {
  callsign: string
  save: (callsign: string) => Promise<string>
}) {
  const [name, setName] = useState(callsign)
  const [saving, setSaving] = useState(false)
  const [saveError, setSaveError] = useState(false)
  const [stats, setStats] = useState<ProfileStats>()
  const [statsError, setStatsError] = useState(false)

  useEffect(() => {
    let mounted = true
    void window.predate.profileStats().then(
      (result) => {
        if (mounted) setStats(result)
      },
      () => {
        if (mounted) setStatsError(true)
      },
    )
    return () => {
      mounted = false
    }
  }, [])

  const submit = async (event: FormEvent): Promise<void> => {
    event.preventDefault()
    if (saving || name.trim() === callsign) return
    setSaving(true)
    setSaveError(false)
    try {
      setName(await save(name.trim()))
    } catch {
      setSaveError(true)
    } finally {
      setSaving(false)
    }
  }

  return (
    <div className="stack profile-page">
      <form className="profile-callsign-form" onSubmit={submit}>
        <label className="field">
          Callsign
          <input
            autoComplete="off"
            value={name}
            maxLength={32}
            disabled={saving}
            onChange={(event) => {
              setName(event.target.value)
              setSaveError(false)
            }}
          />
        </label>
        <button className="primary" disabled={saving || name.trim() === callsign}>
          {saving ? 'Saving…' : 'Save'}
        </button>
        {saveError && <p role="alert">Couldn’t save your callsign. Try again.</p>}
      </form>
      <section aria-labelledby="profile-stats-heading">
        <h2 id="profile-stats-heading">Statistics</h2>
        {statsError && <p role="alert">Couldn’t load your stats. Reopen Profile to retry.</p>}
        {!stats && !statsError && <p role="status">Loading stats…</p>}
        {stats?.map(({ title, rows }) => (
          <section className="profile-stat-group" key={title}>
            <h3>{title}</h3>
            <dl className="score-stats profile-stats">
              {rows.map((stat) => (
                <div key={stat.label}>
                  <dt>{stat.label}</dt>
                  <dd>{formatStat(stat)}</dd>
                </div>
              ))}
            </dl>
          </section>
        ))}
      </section>
    </div>
  )
}
