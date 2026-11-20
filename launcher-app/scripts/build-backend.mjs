import { spawnSync } from 'node:child_process'
import { copyFileSync, existsSync, mkdirSync, writeFileSync } from 'node:fs'
import { dirname, join, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const app = resolve(dirname(fileURLToPath(import.meta.url)), '..')
const root = resolve(app, '..')
const python = join(root, 'build/launcher-python/Scripts/python.exe')
function run(executable, args) {
  const result = spawnSync(executable, args, { cwd: root, stdio: 'inherit', windowsHide: true })
  if (result.error) throw result.error
  if (result.status !== 0) process.exit(result.status ?? 1)
}
mkdirSync(join(app, 'resources'), { recursive: true })
for (const extension of ['png', 'ico']) {
  copyFileSync(
    join(root, `flash/predate_logo.${extension}`),
    join(app, `resources/icon.${extension}`),
  )
}
if (!existsSync(python)) run('py.exe', ['-3', '-m', 'venv', join(root, 'build/launcher-python')])
run(python, ['-m', 'pip', 'install', '-r', join(app, 'backend-requirements.txt')])
run(python, [
  '-m',
  'PyInstaller',
  '--noconfirm',
  '--onefile',
  '--console',
  '--add-data',
  `${join(root, 'source/Predate/data/loadouts.json')};predate-data`,
  '--add-data',
  `${join(app, 'resources/operation-catalog.json')};predate-data`,
  '--add-data',
  `${join(app, 'resources/modern-loadouts.json')};predate-data`,
  '--name',
  'PREDATEBackend',
  '--icon',
  join(app, 'resources/icon.ico'),
  '--version-file',
  join(app, 'resources/backend-version.txt'),
  '--distpath',
  join(app, 'backend'),
  '--workpath',
  join(root, 'build/launcher-backend'),
  '--specpath',
  join(root, 'build/launcher-backend'),
  join(root, 'scripts', 'backend_entry.py'),
])
writeFileSync(join(app, 'backend/project.json'), JSON.stringify({ root }))
