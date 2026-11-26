import { contextBridge, ipcRenderer } from 'electron'
import type { PredateAPI, SessionStatus } from '../shared/types'

const api: PredateAPI = {
  scores: (profile) => ipcRenderer.invoke('predate:scores', profile),
  profileStats: () => ipcRenderer.invoke('predate:profile-stats'),
  saveCallsign: (callsign) => ipcRenderer.invoke('predate:save-callsign', callsign),
  overview: () => ipcRenderer.invoke('predate:overview'),
  perform: (action) => ipcRenderer.invoke('predate:perform', action),
  savePreferences: (values) => ipcRenderer.invoke('predate:preferences', values),
  chooseMod: () => ipcRenderer.invoke('predate:choose-mod'),
  loadoutPresets: () => ipcRenderer.invoke('predate:loadout-presets'),
  saveLoadoutPreset: (preset) => ipcRenderer.invoke('predate:save-loadout-preset', preset),
  setSetupOpen: (open) => ipcRenderer.invoke('predate:setup-size', open),
  onStatus: (callback) => {
    const listener = (_event: Electron.IpcRendererEvent, status: SessionStatus): void =>
      callback(status)
    ipcRenderer.on('predate:status', listener)
    return () => ipcRenderer.removeListener('predate:status', listener)
  },
  onContinueError: (callback) => {
    const listener = (_event: Electron.IpcRendererEvent, message: string): void => callback(message)
    ipcRenderer.on('predate:continue-error', listener)
    return () => ipcRenderer.removeListener('predate:continue-error', listener)
  },
}

contextBridge.exposeInMainWorld('predate', api)
