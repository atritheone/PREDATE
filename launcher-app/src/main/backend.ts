import { app } from 'electron'
import { spawn, type ChildProcessWithoutNullStreams } from 'node:child_process'
import { createHash } from 'node:crypto'
import { createInterface } from 'node:readline'
import { appendFileSync, copyFileSync, existsSync, mkdirSync, readFileSync } from 'node:fs'
import { dirname, join, resolve } from 'node:path'
import type { Overview, Preferences, SessionAction, SessionStatus } from '../shared/types'

export function findProject(): string {
  const candidates = [
    process.env.PREDATE_ROOT,
    process.env.PORTABLE_EXECUTABLE_DIR,
    process.cwd(),
    resolve(__dirname, '../../..'),
    resolve(__dirname, '../../../..'),
  ]
  const hint = join(process.resourcesPath, 'backend/project.json')
  if (existsSync(hint)) candidates.push(JSON.parse(readFileSync(hint, 'utf8')).root)
  for (const candidate of candidates) {
    if (
      candidate &&
      existsSync(join(candidate, 'config', 'release-lock.json')) &&
      existsSync(join(candidate, 'mods'))
    )
      return resolve(candidate)
  }
  throw new Error(
    'PREDATE project was not found. Run the launcher from the PREDATE project folder.',
  )
}

export class Backend {
  private child: ChildProcessWithoutNullStreams
  private sequence = 0
  private pending = new Map<
    number,
    { resolve: (value: unknown) => void; reject: (error: Error) => void }
  >()
  private closed = false

  constructor(readonly root: string) {
    let executable = 'py.exe'
    let args = ['-3', join(root, 'scripts', 'backend_entry.py'), 'bridge']
    if (app.isPackaged) {
      const bundled = join(process.resourcesPath, 'backend/PREDATEBackend.exe')
      const payload = readFileSync(bundled)
      const hash = createHash('sha256').update(payload).digest('hex')
      // The supervisor must survive closing a portable Electron app and its temp folder.
      executable = join(root, 'session-runtime/backend', hash.slice(0, 16), 'PREDATEBackend.exe')
      mkdirSync(dirname(executable), { recursive: true })
      if (!existsSync(executable)) copyFileSync(bundled, executable)
      else if (createHash('sha256').update(readFileSync(executable)).digest('hex') !== hash)
        throw new Error('Cached session backend failed verification')
      args = ['bridge']
    }
    mkdirSync(join(root, 'session-runtime'), { recursive: true })
    this.child = spawn(executable, args, {
      cwd: root,
      env: { ...process.env, PREDATE_ROOT: root, PREDATE_LAUNCHER_PID: String(process.pid) },
      windowsHide: true,
      stdio: 'pipe',
    })
    this.child.stderr.on('data', (data: Buffer) =>
      appendFileSync(join(root, 'session-runtime/launcher-backend.log'), data),
    )
    createInterface({ input: this.child.stdout }).on('line', (line: string) => {
      try {
        const response = JSON.parse(line) as {
          id: number
          ok: boolean
          result?: unknown
          error?: string
        }
        const waiting = this.pending.get(response.id)
        if (!waiting) return
        this.pending.delete(response.id)
        if (response.ok) waiting.resolve(response.result)
        else waiting.reject(new Error(response.error || 'PREDATE backend failed'))
      } catch {
        /* Non-protocol diagnostics belong on stderr. */
      }
    })
    const failed = (error: Error): void => {
      this.closed = true
      for (const pending of this.pending.values()) pending.reject(error)
      this.pending.clear()
    }
    this.child.on('error', failed)
    this.child.on('exit', () => failed(new Error('The PREDATE session bridge closed')))
  }

  request<T>(method: string, params: unknown = {}): Promise<T> {
    if (this.closed)
      return Promise.reject(new Error('The PREDATE backend is closed. Reopen the launcher.'))
    return new Promise<T>((resolve, reject) => {
      const id = ++this.sequence
      this.pending.set(id, { resolve: (value) => resolve(value as T), reject })
      this.child.stdin.write(JSON.stringify({ id, method, params }) + '\n', (error) => {
        if (error) {
          this.pending.delete(id)
          reject(error)
        }
      })
    })
  }

  initialize(): Promise<Overview> {
    return this.request('initialize')
  }
  overview(): Promise<Overview> {
    return this.request('overview')
  }
  status(): Promise<SessionStatus> {
    return this.request('status')
  }
  preferences(values: unknown): Promise<Preferences> {
    return this.request('preferences', values)
  }

  async perform(action: SessionAction, diagnostics = false): Promise<void> {
    const { requestId } = await this.request<{ requestId: string }>(
      'action',
      diagnostics && (action.action === 'new' || action.action === 'continue')
        ? { ...action, diagnostics: true }
        : action,
    )
    const deadline = Date.now() + 300_000
    while (Date.now() < deadline) {
      const result = await this.request<{ ok: boolean; error?: string } | null>('result', {
        requestId,
      })
      if (result) {
        if (!result.ok) throw new Error(result.error || 'The session action failed')
        return
      }
      await new Promise((resolve) => setTimeout(resolve, 150))
    }
    throw new Error(
      'The session is still busy. Its current status remains visible in the launcher.',
    )
  }

  close(): void {
    this.child.stdin.end()
  }
}
