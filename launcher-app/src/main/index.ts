import { app, BrowserWindow, dialog, ipcMain, nativeImage, screen, type Rectangle } from 'electron'
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs'
import { basename, join } from 'node:path'
import { pathToFileURL } from 'node:url'
import { Backend, findProject } from './backend'
import { OPPONENT_COUNTS, type OpponentCount, type SessionAction } from '../shared/types'

let window: BrowserWindow | null = null
let backend: Backend
let polling = false
let timer: NodeJS.Timeout | undefined
let rendererURL = ''
const windowSize = { width: 596, height: 486, minWidth: 480, minHeight: 440 }
const setupSize = { width: 1420, height: 820, minWidth: 960, minHeight: 680 }

function validateAction(value: unknown): SessionAction {
  if (!value || typeof value !== 'object') throw new Error('Invalid session action')
  const action = value as Record<string, unknown>
  if (!['continue', 'quit', 'new', 'restore', 'backup'].includes(String(action.action)))
    throw new Error('Unknown session action')
  if (
    action.profile !== undefined &&
    (typeof action.profile !== 'string' || !/^[a-z][a-z0-9-]{0,31}$/.test(action.profile))
  )
    throw new Error('Invalid hunt')
  if (
    action.name !== undefined &&
    (typeof action.name !== 'string' || !action.name.trim() || action.name.length > 80)
  )
    throw new Error('Use a hunt name of 1–80 characters')
  if (
    action.backup !== undefined &&
    (typeof action.backup !== 'string' || basename(action.backup) !== action.backup)
  )
    throw new Error('Invalid snapshot')
  if (
    action.loadout !== undefined &&
    (action.action !== 'new' ||
      typeof action.loadout !== 'string' ||
      !/^[a-z0-9-]{1,32}$/.test(action.loadout))
  )
    throw new Error('Select a starting kit for a new hunt')
  if (
    action.opponents !== undefined &&
    (action.action !== 'new' || !OPPONENT_COUNTS.includes(action.opponents as OpponentCount))
  )
    throw new Error('Choose 1, 3, 6, 15, or 30 opponents for a new hunt')
  if (action.spawn !== undefined) {
    if (action.action !== 'new' || !action.spawn || typeof action.spawn !== 'object')
      throw new Error('Set a spawn when creating a new hunt')
    const spawn = action.spawn as Record<string, unknown>
    const preset = Object.keys(spawn).length === 1 && typeof spawn.preset === 'string'
    const coordinates =
      Object.keys(spawn).length === 2 &&
      [spawn.x, spawn.z].every(
        (point) =>
          typeof point === 'number' && Number.isFinite(point) && point > 10 && point < 15350,
      )
    if (!preset && !coordinates) throw new Error('Enter a preset or valid Chernarus coordinates')
  }
  if (
    action.setup !== undefined &&
    (action.action !== 'new' ||
      !action.setup ||
      typeof action.setup !== 'object' ||
      Array.isArray(action.setup))
  )
    throw new Error('Game settings apply only to a new hunt')
  return action as SessionAction
}

function trusted(event: Electron.IpcMainInvokeEvent): void {
  if (
    !window ||
    event.sender !== window.webContents ||
    event.senderFrame !== window.webContents.mainFrame ||
    event.senderFrame.url !== rendererURL
  )
    throw new Error('Untrusted launcher frame')
}

async function createWindow(): Promise<void> {
  const diagnose = process.argv.includes('--diagnose')
  backend = new Backend(findProject())
  // Importing creates a verified copy; the original saved world remains untouched.
  await backend.initialize()
  const iconPath = join(app.getAppPath(), 'resources/icon.png')
  window = new BrowserWindow({
    title: 'PREDATE',
    ...windowSize,
    backgroundColor: '#080d1a',
    titleBarStyle: 'hidden',
    titleBarOverlay: { color: '#080d1a', symbolColor: '#a2bad5', height: 32 },
    show: false,
    autoHideMenuBar: true,
    icon: existsSync(iconPath) ? nativeImage.createFromBuffer(readFileSync(iconPath)) : undefined,
    webPreferences: {
      preload: join(__dirname, '../preload/index.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
      backgroundThrottling: !diagnose,
    },
  })
  window.webContents.setWindowOpenHandler(() => ({ action: 'deny' }))
  window.webContents.on('will-navigate', (event) => event.preventDefault())
  window.once('ready-to-show', () => {
    if (!diagnose) window?.show()
  })
  window.on('closed', () => {
    window = null
  })
  ipcMain.handle('predate:overview', (event) => {
    trusted(event)
    return backend.overview()
  })
  ipcMain.handle('predate:perform', (event, value: unknown) => {
    trusted(event)
    return backend.perform(validateAction(value))
  })
  ipcMain.handle('predate:preferences', (event, values: unknown) => {
    trusted(event)
    return backend.preferences(values)
  })
  ipcMain.handle('predate:choose-mod', async (event) => {
    trusted(event)
    const result = await dialog.showOpenDialog(window!, {
      title: 'Choose a mod folder',
      properties: ['openDirectory'],
    })
    return result.canceled ? null : result.filePaths[0]
  })
  let setupBounds: { original: Rectangle; expanded: Rectangle } | null = null
  ipcMain.handle('predate:setup-size', (event, open: unknown) => {
    trusted(event)
    if (diagnose || typeof open !== 'boolean' || !window) return
    if (!open) {
      window.setMinimumSize(windowSize.minWidth, windowSize.minHeight)
      if (setupBounds && !window.isMaximized() && !window.isFullScreen()) {
        const current = window.getBounds()
        const { original, expanded } = setupBounds
        if (current.width === expanded.width && current.height === expanded.height)
          window.setBounds({
            ...original,
            x: original.x + current.x - expanded.x,
            y: original.y + current.y - expanded.y,
          })
      }
      setupBounds = null
      return
    }
    if (setupBounds) return
    const original = window.getBounds()
    const area = screen.getDisplayMatching(original).workArea
    window.setMinimumSize(
      Math.min(area.width, setupSize.minWidth),
      Math.min(area.height, setupSize.minHeight),
    )
    if (window.isMaximized() || window.isFullScreen()) return
    const width = Math.min(area.width, Math.max(original.width, setupSize.width))
    const height = Math.min(area.height, Math.max(original.height, setupSize.height))
    if (width === original.width && height === original.height) return
    const x = Math.max(
      area.x,
      Math.min(original.x - Math.floor((width - original.width) / 2), area.x + area.width - width),
    )
    const y = Math.max(
      area.y,
      Math.min(
        original.y - Math.floor((height - original.height) / 2),
        area.y + area.height - height,
      ),
    )
    window.setBounds({ x, y, width, height })
    setupBounds = { original, expanded: window.getBounds() }
  })
  rendererURL = pathToFileURL(join(__dirname, '../renderer/index.html')).href
  if (!app.isPackaged && process.env.ELECTRON_RENDERER_URL) {
    const developmentURL = new URL(process.env.ELECTRON_RENDERER_URL)
    if (
      !['localhost', '127.0.0.1'].includes(developmentURL.hostname) ||
      developmentURL.protocol !== 'http:'
    )
      throw new Error('Development renderer must use localhost')
    rendererURL = developmentURL.href
    await window.loadURL(rendererURL)
  } else await window.loadFile(join(__dirname, '../renderer/index.html'))
  if (diagnose) {
    await window.webContents.executeJavaScript(`(async () => {
      const deadline = Date.now() + 5000;
      while (!document.querySelector('nav button, [role="alert"]') && Date.now() < deadline)
        await new Promise(resolve => setTimeout(resolve, 50));
    })()`)
    const pageChecks: Array<{
      width: number
      height: number
      page: string
      heading: string
      horizontalOverflow: boolean
      error: string | null
      kitOptions?: string[]
      defaultKit?: string
      clothingOptions?: string[]
      defaultPlayerOutfit?: string
      playerEquipmentLabel?: string
      aiEquipmentLabel?: string
      defaultOperatorOutfit?: string
      defaultOperatorOutfitLabel?: string
      operatorOutfitByCount?: boolean
      opponentOptions?: string[]
      defaultOpponents?: string
      footprintControlAbsent?: boolean
      createVisible?: boolean
      spawnOptions?: string[]
      defaultSpawn?: string
      defaultSpawnLabel?: string
      classicSpawnGroup?: string
      customSpawnFields?: boolean
      startTime?: boolean
      spawnMap?: boolean
      equipmentSlots?: number
      attachments?: number
      itemPicker?: boolean
      setupSections?: string[]
      setupFits?: boolean
      displayControls?: boolean
    }> = []
    const launcherPages = ['Continue', 'Settings', 'Backups']
    for (const [width, height, pages] of [
      [windowSize.width, windowSize.height, launcherPages],
      [windowSize.minWidth, windowSize.minHeight, launcherPages],
      [setupSize.width, setupSize.height, ['New Hunt']],
      [setupSize.minWidth, setupSize.minHeight, ['New Hunt']],
    ] as const) {
      window.setSize(width, height)
      for (const page of pages) {
        const check = (await window.webContents.executeJavaScript(`(async () => {
          const page = ${JSON.stringify(page)};
          if (page !== 'Continue')
            Array.from(document.querySelectorAll('nav button')).find(button => button.textContent === page)?.click();
          await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
          const main = document.querySelector('main');
          const opponentSelect = document.getElementById('opponent-count');
          const createButton = document.querySelector('.new-operation button.primary');
          const startTime = !!document.getElementById('start-time');
          const features = {};
          if (page === 'New Hunt') {
            const paint = () => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
            const sections = Array.from(document.querySelectorAll('.setup-sections > section > h2'));
            const setupBody = document.querySelector('.setup-body');
            features.setupSections = sections.map(heading => heading.textContent);
            features.setupFits = !!setupBody &&
              setupBody.scrollHeight <= setupBody.clientHeight + 1 &&
              getComputedStyle(setupBody).overflowY === 'hidden' &&
              sections.every(heading => {
                const rect = heading.getBoundingClientRect();
                const bounds = setupBody.getBoundingClientRect();
                return rect.top >= bounds.top && rect.bottom <= bounds.bottom;
              });
            const spawnSelect = document.getElementById('spawn-location');
            features.spawnOptions = Array.from(spawnSelect?.options ?? []).map(option => option.value);
            features.defaultSpawn = spawnSelect?.value;
            features.defaultSpawnLabel = spawnSelect?.selectedOptions[0]?.textContent.trim();
            features.classicSpawnGroup = spawnSelect?.querySelector('optgroup')?.label;
            features.spawnMap = !!document.querySelector('.map-viewport img');
            features.customSpawnFields = !!document.getElementById('spawn-x') && !!document.getElementById('spawn-z');
            const kitSelect = document.getElementById('starting-kit');
            features.kitOptions = Array.from(kitSelect?.options ?? []).map(option => option.value);
            features.defaultKit = kitSelect?.value;
            const playerOutfit = document.getElementById('player-outfit');
            const operatorOutfit = document.getElementById('operator-outfit');
            features.clothingOptions = Array.from(playerOutfit?.options ?? []).map(option => option.value);
            features.defaultPlayerOutfit = playerOutfit?.value;
            features.playerEquipmentLabel = playerOutfit?.labels?.[0]?.firstChild?.textContent?.trim();
            features.aiEquipmentLabel = operatorOutfit?.labels?.[0]?.firstChild?.textContent?.trim();
            features.defaultOperatorOutfit = operatorOutfit?.value;
            features.defaultOperatorOutfitLabel = operatorOutfit?.selectedOptions[0]?.textContent.trim();
            features.operatorOutfitByCount = !!operatorOutfit?.closest('.game-fields')?.querySelector('#opponent-count');
            features.equipmentSlots = document.querySelectorAll('.equipment-slot').length;
            features.attachments = document.querySelectorAll('.attachment-list summary, .attachment-empty').length;
            document.querySelector('.inventory-subhead button')?.click();
            await paint();
            features.itemPicker = !!document.querySelector('.gear-picker[open] .picker-item');
            document.querySelector('[aria-label="Close Item Picker"]')?.click();
            await paint();
          }
          const create = createButton?.getBoundingClientRect();
          const heading = document.querySelector('h1');
          const result = {heading: heading?.querySelector('img')?.alt ?? heading?.textContent,
            horizontalOverflow: document.documentElement.scrollWidth > innerWidth || main?.scrollWidth > main?.clientWidth ||
              (document.querySelector('.setup-body')?.scrollWidth ?? 0) > (document.querySelector('.setup-body')?.clientWidth ?? 0),
            error: document.querySelector('[role="alert"]')?.textContent ?? null,
            displayControls: page === 'Settings' ?
              !!document.querySelector('input[aria-label="Compass Size in Percent"]') &&
              !!document.querySelector('input[aria-label="Compass High Contrast"]') : undefined,
            opponentOptions: opponentSelect ? Array.from(opponentSelect.options).map(option => option.value) : undefined,
            defaultOpponents: opponentSelect?.value,
            footprintControlAbsent: !document.getElementById('enable-footprints'),
            startTime,
            ...features,
            createVisible: create ? create.top >= main.getBoundingClientRect().top - 1 &&
              create.bottom <= main.getBoundingClientRect().bottom + 1 : undefined};
          document.querySelector('.page-header button')?.click();
          await new Promise(resolve => requestAnimationFrame(resolve));
          return result;
        })()`)) as { heading: string; horizontalOverflow: boolean; error: string | null }
        pageChecks.push({ width, height, page, ...check })
      }
    }
    window.setSize(windowSize.width, windowSize.height)
    const result = (await window.webContents.executeJavaScript(`(async () => {
      const wordmark = document.querySelector('.wordmark img');
      await wordmark?.decode().catch(() => {});
      return {
      title: document.title,
      bridge: typeof window.predate?.overview === 'function',
      navigation: Array.from(document.querySelectorAll('nav button')).map(button => button.textContent),
      operationCount: document.querySelectorAll('#operation option').length,
      huntLabel: document.getElementById('operation')?.labels?.[0]?.firstChild?.textContent?.trim(),
      continueVisible: !!document.querySelector('button.play'),
      imageCount: document.images.length,
      wordmarkLoaded: !!wordmark?.naturalWidth,
      error: document.querySelector('[role="alert"]')?.textContent ?? null,
      horizontalOverflow: document.documentElement.scrollWidth > innerWidth
      };
    })()`)) as {
      bridge: boolean
      navigation: string[]
      operationCount: number
      huntLabel?: string
      continueVisible: boolean
      imageCount: number
      wordmarkLoaded: boolean
      error: string | null
      horizontalOverflow: boolean
    }
    const passed =
      result.bridge &&
      result.navigation.join(',') === 'New Hunt,Settings,Backups' &&
      result.operationCount > 0 &&
      result.huntLabel === 'Hunt' &&
      result.continueVisible &&
      result.imageCount === 1 &&
      result.wordmarkLoaded &&
      !result.error &&
      !result.horizontalOverflow &&
      pageChecks.every(
        (check) =>
          check.heading === (check.page === 'Continue' ? 'PREDATE' : check.page) &&
          !check.horizontalOverflow &&
          !check.error &&
          (check.page !== 'Settings' || check.displayControls === true) &&
          (check.page !== 'New Hunt' ||
            (check.createVisible === true &&
              check.startTime === true &&
              (check.kitOptions?.includes('hk417')
                ? check.defaultKit === 'hk417'
                : check.kitOptions?.includes('m4a1') && check.kitOptions.includes('ak74')) &&
              check.clothingOptions?.join(',') === 'spectral,ghost,operator,original-uniform' &&
              check.defaultPlayerOutfit === 'spectral' &&
              check.playerEquipmentLabel === 'Equipment' &&
              check.aiEquipmentLabel === 'AI Equipment' &&
              check.defaultOperatorOutfit === 'varied' &&
              check.defaultOperatorOutfitLabel === 'Default' &&
              check.operatorOutfitByCount === true &&
              check.equipmentSlots === 14 &&
              (check.attachments ?? 0) > 0 &&
              check.itemPicker === true &&
              check.setupSections?.join(',') === 'Game,Spawn,Loadout' &&
              check.setupFits === true &&
              (check.opponentOptions?.join(',') === OPPONENT_COUNTS.join(',') ||
                check.opponentOptions?.join(',') === '1,3,6') &&
              check.spawnMap === true &&
              check.defaultSpawn === 'original' &&
              check.defaultSpawnLabel === 'PREDATE Starting Area' &&
              check.classicSpawnGroup === 'Classic Spawns' &&
              check.spawnOptions?.includes('cherno') &&
              (!check.spawnOptions.includes('regional') ||
                check.spawnOptions.includes('northwest')) &&
              check.spawnOptions.includes('custom') &&
              check.customSpawnFields === true &&
              check.defaultOpponents === (check.opponentOptions?.includes('30') ? '30' : '1') &&
              check.footprintControlAbsent === true)),
      )
    mkdirSync(join(backend.root, 'build'), { recursive: true })
    writeFileSync(
      join(backend.root, 'build/launcher-ui-result.json'),
      JSON.stringify({ passed, ...result, pageChecks }, null, 2),
    )
    backend.close()
    app.exit(passed ? 0 : 1)
    return
  }
  timer = setInterval(async () => {
    if (polling || !window) return
    polling = true
    try {
      window.webContents.send('predate:status', await backend.status())
    } catch (error) {
      window?.webContents.send('predate:status', { phase: 'error', message: String(error) })
    } finally {
      polling = false
    }
  }, 500)
  if (process.argv.includes('--continue')) {
    const overview = await backend.overview()
    void backend
      .perform({ action: 'continue', profile: overview.preferences.selected_world })
      .catch((error) => {
        window?.webContents.send('predate:status', { phase: 'error', message: String(error) })
      })
  }
}

if (!app.requestSingleInstanceLock()) app.quit()
else {
  app.on('second-instance', (_event, argv) => {
    if (window?.isMinimized()) window.restore()
    window?.show()
    window?.focus()
    if (argv.includes('--continue') && backend)
      void backend
        .overview()
        .then((info) =>
          backend.perform({ action: 'continue', profile: info.preferences.selected_world }),
        )
        .catch((error) =>
          window?.webContents.send('predate:status', { phase: 'error', message: String(error) }),
        )
    if (argv.includes('--save-quit') && backend)
      void backend
        .perform({ action: 'quit' })
        .catch((error) =>
          window?.webContents.send('predate:status', { phase: 'error', message: String(error) }),
        )
  })
  app
    .whenReady()
    .then(async () => {
      if (process.argv.includes('--save-quit')) {
        backend = new Backend(findProject())
        await backend.perform({ action: 'quit' })
        app.quit()
      } else await createWindow()
    })
    .catch((error) => {
      const folder = app.getPath('userData')
      mkdirSync(folder, { recursive: true })
      writeFileSync(join(folder, 'startup-error.log'), String(error))
      backend?.close()
      app.exit(1)
    })
  app.on('window-all-closed', () => app.quit())
  app.on('before-quit', () => {
    if (timer) clearInterval(timer)
    backend?.close()
  })
}
