import type { PredateAPI } from '../../shared/types'
declare global {
  interface Window {
    predate: PredateAPI
  }
}
