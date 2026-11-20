import { useEffect, useRef, useState, type PointerEvent, type ReactNode } from 'react'
import terrain from './assets/chernarus.png'
import map from '../../../resources/chernarus-places.json'
import type { Overview } from '../../shared/types'

type Point = { x: number; z: number }

const maxZoom = 8
const lower = 10.01
const upper = 15349.99

const constrain = (value: number, min: number, max: number): number =>
  Math.min(max, Math.max(min, value))

function coordinate(value: number): string {
  return constrain(value, lower, upper).toFixed(2)
}

function MapView({
  point,
  select,
}: {
  point: Point | undefined
  select: (point: Point) => void
}): ReactNode {
  const frame = useRef<HTMLDivElement>(null)
  const layer = useRef<HTMLDivElement>(null)
  const [view, setView] = useState({ zoom: 1, left: 0, top: 0 })
  const pan = useRef<{ x: number; y: number; left: number; top: number; moved: boolean } | null>(
    null,
  )
  const selectedX = point?.x
  const selectedZ = point?.z
  useEffect(() => {
    if (selectedX === undefined || selectedZ === undefined) return
    setView((current) => {
      const x = selectedX / map.size
      const y = 1 - selectedZ / map.size
      const span = 1 / current.zoom
      if (
        x >= current.left &&
        x <= current.left + span &&
        y >= current.top &&
        y <= current.top + span
      )
        return current
      return {
        ...current,
        left: constrain(x - span / 2, 0, 1 - span),
        top: constrain(y - span / 2, 0, 1 - span),
      }
    })
  }, [selectedX, selectedZ])

  const setZoom = (zoom: number): void => {
    const focusX = point ? point.x / map.size : view.left + 0.5 / view.zoom
    const focusY = point ? 1 - point.z / map.size : view.top + 0.5 / view.zoom
    setView({
      zoom,
      left: constrain(focusX - 0.5 / zoom, 0, 1 - 1 / zoom),
      top: constrain(focusY - 0.5 / zoom, 0, 1 - 1 / zoom),
    })
  }

  const move = (event: PointerEvent<HTMLDivElement>): void => {
    const start = pan.current
    if (!start || !frame.current) return
    const dx = event.clientX - start.x
    const dy = event.clientY - start.y
    if (Math.hypot(dx, dy) < 4 && !start.moved) return
    start.moved = true
    const width = frame.current.clientWidth * view.zoom
    const max = 1 - 1 / view.zoom
    setView({
      ...view,
      left: constrain(start.left - dx / width, 0, max),
      top: constrain(start.top - dy / width, 0, max),
    })
  }

  const end = (event: PointerEvent<HTMLDivElement>): void => {
    const start = pan.current
    pan.current = null
    if (!start || start.moved || !layer.current) return
    const bounds = layer.current.getBoundingClientRect()
    select({
      x: ((event.clientX - bounds.left) * map.size) / bounds.width,
      z: ((bounds.bottom - event.clientY) * map.size) / bounds.height,
    })
  }

  return (
    <div className="map-panel">
      <div className="map-toolbar split">
        <strong>Chernarus</strong>
        <div className="map-zoom">
          <button
            type="button"
            aria-label="Zoom Out"
            disabled={view.zoom === 1}
            onClick={() => setZoom(view.zoom / 2)}
          >
            −
          </button>
          <button type="button" aria-label="Reset Map Zoom" onClick={() => setZoom(1)}>
            {view.zoom}×
          </button>
          <button
            type="button"
            aria-label="Zoom In"
            disabled={view.zoom === maxZoom}
            onClick={() => setZoom(view.zoom * 2)}
          >
            +
          </button>
        </div>
      </div>
      <div className="map-stage">
        <div
          ref={frame}
          className="map-viewport"
          aria-label="Chernarus spawn map. Select a point, or enter coordinates below."
          role="img"
          onPointerDown={(event) => {
            if (event.button !== 0) return
            event.currentTarget.setPointerCapture(event.pointerId)
            pan.current = {
              x: event.clientX,
              y: event.clientY,
              left: view.left,
              top: view.top,
              moved: false,
            }
          }}
          onPointerMove={move}
          onPointerUp={end}
          onPointerCancel={() => {
            pan.current = null
          }}
        >
          <div
            ref={layer}
            className="map-layer"
            style={{
              width: view.zoom * 100 + '%',
              left: -view.left * view.zoom * 100 + '%',
              top: -view.top * view.zoom * 100 + '%',
            }}
          >
            <img src={terrain} alt="" draggable={false} />
            <div className="map-grid" />
            {map.places
              .filter((place) => view.zoom >= 2 || place.major)
              .map((place) => (
                <span
                  className="map-label"
                  key={place.name + place.x + place.z}
                  style={{
                    left: (place.x / map.size) * 100 + '%',
                    top: (1 - place.z / map.size) * 100 + '%',
                  }}
                >
                  {place.name}
                </span>
              ))}
            {point && (
              <span
                className="spawn-pin"
                style={{
                  left: (point.x / map.size) * 100 + '%',
                  top: (1 - point.z / map.size) * 100 + '%',
                }}
              />
            )}
          </div>
          <span className="map-north">▲ N</span>
          <span className="map-scale">
            {Math.round(map.size / (view.zoom * 5)).toLocaleString()} m
          </span>
        </div>
      </div>
    </div>
  )
}

export function SpawnEditor({
  presets,
  preset,
  randomRegion,
  x,
  z,
  change,
}: {
  presets: Overview['spawnPoints']
  preset: string
  randomRegion: boolean
  x: string
  z: string
  change: (preset: string, x: string, z: string) => void
}): ReactNode {
  const selected = presets.find((item) => item.id === preset)
  const point =
    selected ??
    (preset === 'custom' && validSpawn(x, z) ? { x: Number(x), z: Number(z) } : undefined)
  const east = selected ? String(selected.x) : x
  const north = selected ? String(selected.z) : z
  return (
    <section className="spawn-workspace" aria-label="Spawn Setup">
      <MapView
        point={point}
        select={(next) => change('custom', coordinate(next.x), coordinate(next.z))}
      />
      <div className="spawn-controls stack">
        <label className="field">
          Spawn Location
          <select
            id="spawn-location"
            value={preset}
            onChange={(event) => {
              const next = presets.find((item) => item.id === event.target.value)
              if (event.target.value === 'regional') change('regional', '', '')
              else change(event.target.value, next ? String(next.x) : x, next ? String(next.z) : z)
            }}
          >
            <optgroup label="Classic Spawns">
              {presets.map((item) => (
                <option value={item.id} key={item.id}>
                  {item.name}
                </option>
              ))}
            </optgroup>
            {randomRegion && <option value="regional">Random Region Across Chernarus</option>}
            <option value="custom">Custom Coordinates</option>
          </select>
        </label>
        <div className="coordinate-fields">
          <label className="field">
            East (X)
            <input
              type="number"
              id="spawn-x"
              step="any"
              min={10}
              max={15350}
              value={east}
              onChange={(event) => change('custom', event.target.value, north)}
            />
          </label>
          <label className="field">
            North (Z)
            <input
              type="number"
              id="spawn-z"
              step="any"
              min={10}
              max={15350}
              value={north}
              onChange={(event) => change('custom', east, event.target.value)}
            />
          </label>
        </div>
        <p className="hint">
          Click to place the pin. Zoom in and drag to pan, or enter exact coordinates in metres.
        </p>
      </div>
    </section>
  )
}

export function validSpawn(x: string, z: string): boolean {
  return [x, z].every((value) => Number(value) > 10 && Number(value) < 15350)
}
