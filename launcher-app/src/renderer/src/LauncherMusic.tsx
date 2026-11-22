import { useEffect, useRef, useState, type ReactNode } from 'react'
import shadows from './assets/shadows.flac'

const muteKey = 'predate.launcher.musicMuted'
const replayDelayMs = 3000

function readMuted(): boolean {
  try {
    return localStorage.getItem(muteKey) === 'true'
  } catch {
    return false
  }
}

export function LauncherMusic({ inHunt }: { inHunt: boolean }): ReactNode {
  const audioRef = useRef<HTMLAudioElement>(null)
  const restartAt = useRef<number | null>(null)
  const [muted, setMuted] = useState(readMuted)

  useEffect(() => {
    const audio = audioRef.current
    if (!audio) return

    audio.volume = 1
    let replayTimer: number | undefined
    const syncPlayback = (): void => {
      window.clearTimeout(replayTimer)
      const now = performance.now()
      if (audio.ended) restartAt.current ??= now + replayDelayMs

      if (muted || inHunt) {
        audio.pause()
        return
      }

      const remaining = (restartAt.current ?? now) - now
      if (remaining > 0) {
        replayTimer = window.setTimeout(syncPlayback, Math.ceil(remaining))
        return
      }

      restartAt.current = null
      if (audio.ended) audio.currentTime = 0
      void audio.play().catch(() => {})
    }

    syncPlayback()
    audio.addEventListener('ended', syncPlayback)
    return () => {
      window.clearTimeout(replayTimer)
      audio.removeEventListener('ended', syncPlayback)
    }
  }, [inHunt, muted])

  const toggleMute = (): void => {
    const next = !muted
    setMuted(next)
    try {
      localStorage.setItem(muteKey, String(next))
    } catch {
      // The button still works for this session when local storage is unavailable.
    }
  }

  return (
    <>
      <audio ref={audioRef} src={shadows} preload="none" muted={muted} hidden />
      {!inHunt && (
        <button
          type="button"
          className="music-toggle"
          aria-label={muted ? 'Unmute Music' : 'Mute Music'}
          onClick={toggleMute}
        >
          {muted ? 'Unmute' : 'Mute'}
        </button>
      )}
    </>
  )
}
