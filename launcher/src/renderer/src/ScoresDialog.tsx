import { useEffect, useRef, useState } from 'react'
import type { ScoreEncounter, ScoreHistory } from '../../shared/types'
import { formatAccuracy } from './accuracy'

const duration = (seconds: number): string => {
  const minutes = Math.floor(seconds / 60)
  return `${minutes}m ${Math.floor(seconds % 60)}s`
}

const scoreText = (points: number): string => String(points).padStart(9, '0')
const accuracyText = (hits: number, shots: number): string =>
  shots ? `${Math.round((1000 * hits) / shots) / 10}%` : '—'
const encounterPoints = (entry: ScoreEncounter): number =>
  entry.KillPoints +
  entry.PursuitPoints +
  entry.EngagementPoints +
  entry.CounterPoints +
  entry.PrecisionPoints +
  entry.RangePoints

export function ScoresDialog({ profile, onClose }: { profile: string; onClose: () => void }) {
  const dialog = useRef<HTMLDialogElement>(null)
  const [history, setHistory] = useState<ScoreHistory>()
  const [selected, setSelected] = useState<number>()
  const [error, setError] = useState(false)
  useEffect(() => {
    dialog.current?.showModal()
    let mounted = true
    window.predate.scores(profile).then(
      (result) => {
        if (mounted) setHistory(result)
      },
      () => {
        if (mounted) setError(true)
      },
    )
    return () => {
      mounted = false
    }
  }, [profile])

  const lives = [...(history?.lives ?? [])].sort((a, b) => b.Life - a.Life)
  const life = lives.find((entry) => entry.Life === selected) ?? lives[0]
  const encounters = life?.Encounters ?? []
  const pursuits = encounters.filter((entry) => entry.Tracked && entry.Committed)
  const converted = pursuits.filter((entry) => entry.KillPoints > 0).length
  const legacyShots = life?.Version === 1 ? life.Shots : (life?.LegacyShots ?? 0)
  const legacyHits = life?.Version === 1 ? life.Hits : (life?.LegacyHits ?? 0)
  const hits = (life?.Hits ?? 0) - legacyHits
  const misses = life?.Misses ?? 0
  const pending = life?.PendingRounds ?? 0
  const unverified = life?.UnverifiedRounds ?? 0
  const verifiedRounds = hits + misses
  const accuracy =
    pending || unverified || !verifiedRounds
      ? null
      : (hits + (life?.ExtraOperatorHits ?? 0)) / verifiedRounds
  return (
    <dialog
      ref={dialog}
      className="scores-dialog"
      aria-labelledby="scores-heading"
      onClose={onClose}
    >
      <header className="split">
        <h2 id="scores-heading">{history?.name ?? 'Scores'}</h2>
        <button className="quiet" onClick={() => dialog.current?.close()}>
          Close
        </button>
      </header>
      {error && <p role="alert">Scores are unavailable. Reopen the score window to retry.</p>}
      {!error && !history && <p role="status">Loading scores…</p>}
      {history && !life && <p>No recorded deployments yet.</p>}
      {life && (
        <>
          <label className="score-life-picker">
            Deployment
            <select value={life.Life} onChange={(event) => setSelected(Number(event.target.value))}>
              {lives.map((entry) => (
                <option value={entry.Life} key={entry.Life}>
                  {entry.Life} · {scoreText(entry.Score)} · {entry.EndReason || 'In progress'}
                </option>
              ))}
            </select>
          </label>
          <div className="score-digits">{scoreText(life.Score)}</div>
          <dl className="score-stats">
            <div>
              <dt>Kills</dt>
              <dd>{life.Kills}</dd>
            </div>
            {life.Version >= 2 && (
              <div>
                <dt>Accuracy</dt>
                <dd>{formatAccuracy(accuracy)}</dd>
                <dd>
                  {hits} rounds hit · {misses} missed
                </dd>
                {pending > 0 && <dd>{pending} pending</dd>}
                {unverified > 0 && <dd>{unverified} unverified</dd>}
              </div>
            )}
            {(legacyShots > 0 || life.Version === 1) && (
              <div>
                <dt>Earlier Accuracy Estimate</dt>
                <dd>
                  {accuracyText(legacyHits, legacyShots)} ({legacyHits}/{legacyShots})
                </dd>
              </div>
            )}
            <div>
              <dt>Survived</dt>
              <dd>{duration(life.SurvivedSeconds)}</dd>
            </div>
            <div>
              <dt>Pursuits converted</dt>
              <dd>
                {converted}/{pursuits.length}
              </dd>
            </div>
            <div>
              <dt>Survival points</dt>
              <dd>{life.SurvivalScore}</dd>
            </div>
            <div>
              <dt>Outcome</dt>
              <dd>
                {life.EndReason || 'In progress'}
                {life.Killer ? ` · ${life.Killer}` : ''}
              </dd>
            </div>
          </dl>
          {life.Partial && <p>Recording started after this deployment began.</p>}
          {!life.EndReason && <p>Stats through the last save checkpoint.</p>}
          {encounters
            .filter((entry) => entry.KillPoints > 0 || (entry.Tracked && entry.Committed))
            .map((entry) => (
              <details key={`${entry.OperatorId}-${entry.Deployment}`} className="score-encounter">
                <summary>
                  {entry.Callsign || entry.OperatorId} ·{' '}
                  {entry.KillPoints
                    ? `${encounterPoints(entry).toLocaleString()} pts`
                    : entry.Outcome.replace(/^Life ended$/, 'Deployment ended') || 'In pursuit'}
                </summary>
                <dl className="score-stats">
                  {entry.Tracked && (
                    <>
                      <div>
                        <dt>Ground covered</dt>
                        <dd>{Math.round(entry.GroundCovered)} m</dd>
                      </div>
                      <div>
                        <dt>Useful closure</dt>
                        <dd>{Math.round(Math.max(0, entry.UsefulClosure))} m</dd>
                      </div>
                      <div>
                        <dt>Active pursuit</dt>
                        <dd>{duration(entry.LockSeconds)}</dd>
                      </div>
                      {entry.ReachedContact && (
                        <div>
                          <dt>Time to contact</dt>
                          <dd>{duration(entry.TimeToContact)}</dd>
                        </div>
                      )}
                      {entry.RoundsToFirstHit > 0 && (
                        <div>
                          <dt>Rounds since lock to first hit (est.)</dt>
                          <dd>{entry.RoundsToFirstHit}</dd>
                        </div>
                      )}
                      <div>
                        <dt>Final Approach</dt>
                        <dd>{duration(entry.FinalApproachSeconds)}</dd>
                      </div>
                      <div>
                        <dt>Pulses received</dt>
                        <dd>{entry.Pulses}</dd>
                      </div>
                    </>
                  )}
                  <div>
                    <dt>Return damage</dt>
                    <dd>{Math.round(entry.ReturnDamage)}</dd>
                  </div>
                  {entry.KillPoints > 0 && (
                    <>
                      <div>
                        <dt>{entry.WoundKill ? 'Wound kill' : 'Direct kill'}</dt>
                        <dd>+{entry.KillPoints}</dd>
                      </div>
                      <div>
                        <dt>Pursuit</dt>
                        <dd>+{entry.PursuitPoints}</dd>
                      </div>
                      <div>
                        <dt>Engagement</dt>
                        <dd>+{entry.EngagementPoints}</dd>
                      </div>
                      <div>
                        <dt>Counter-hunt</dt>
                        <dd>+{entry.CounterPoints}</dd>
                      </div>
                      <div>
                        <dt>Precision</dt>
                        <dd>+{entry.PrecisionPoints}</dd>
                      </div>
                      {entry.CreditedRange > 0 && (
                        <div>
                          <dt>Credited hit range</dt>
                          <dd>
                            {Math.round(entry.CreditedRange)} m · +{entry.RangePoints}
                          </dd>
                        </div>
                      )}
                    </>
                  )}
                </dl>
              </details>
            ))}
        </>
      )}
    </dialog>
  )
}
