import { contextBridge, ipcRenderer } from 'electron'
import type { PredateAPI, SessionStatus } from '../shared/types'

const api: PredateAPI = {
  overview: () => ipcRenderer.invoke('predate:overview'),
  perform: (action) => ipcRenderer.invoke('predate:perform', action),
  savePreferences: (values) => ipcRenderer.invoke('predate:preferences', values),
  chooseMod: () => ipcRenderer.invoke('predate:choose-mod'),
  setSetupOpen: (open) => ipcRenderer.invoke('predate:setup-size', open),
  onStatus: (callback) => {
    const listener = (_event: Electron.IpcRendererEvent, status: SessionStatus): void =>
      callback(status)
    ipcRenderer.on('predate:status', listener)
    return () => ipcRenderer.removeListener('predate:status', listener)
  },
}

contextBridge.exposeInMainWorld('predate', api)
