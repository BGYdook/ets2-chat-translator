const { app, BrowserWindow, dialog, ipcMain } = require('electron');
const fs = require('fs');
const path = require('path');
const childProcess = require('child_process');

const DLL_NAME = 'ets2_chat_translator.dll';
const CONFIG_NAME = 'ets2_chat_translator_config.json';

let mainWindow;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1040,
    height: 760,
    minWidth: 880,
    minHeight: 640,
    title: 'ETS2 Chat Translator Manager',
    backgroundColor: '#f4f5f7',
    autoHideMenuBar: true,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false
    }
  });

  mainWindow.loadFile(path.join(__dirname, 'index.html'));
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});

function appRoot() {
  if (app.isPackaged) return path.dirname(process.execPath);
  return path.resolve(__dirname, '..', '..');
}

function sourceDllPath() {
  const packaged = path.join(process.resourcesPath || '', DLL_NAME);
  if (packaged && fs.existsSync(packaged)) return packaged;

  const besideExe = path.join(appRoot(), DLL_NAME);
  if (fs.existsSync(besideExe)) return besideExe;

  return path.join(appRoot(), 'build', DLL_NAME);
}

function pluginDir(ets2Path) {
  return path.join(ets2Path, 'bin', 'win_x64', 'plugins');
}

function installedDllPath(ets2Path) {
  return path.join(pluginDir(ets2Path), DLL_NAME);
}

function configPath(ets2Path) {
  return path.join(pluginDir(ets2Path), CONFIG_NAME);
}

function looksLikeEts2(ets2Path) {
  if (!ets2Path || !fs.existsSync(ets2Path)) return false;
  return fs.existsSync(path.join(ets2Path, 'bin', 'win_x64')) ||
    fs.existsSync(path.join(ets2Path, 'bin', 'win_x64', 'eurotrucks2.exe'));
}

function queryRegistryInstallDir(key) {
  try {
    const output = childProcess.execFileSync('reg', ['query', `HKLM\\${key}`, '/v', 'InstallDir'], {
      encoding: 'utf8',
      windowsHide: true
    });
    const line = output.split(/\r?\n/).find((item) => item.includes('InstallDir'));
    if (!line) return '';
    const parts = line.trim().split(/\s{2,}/);
    return parts[parts.length - 1] || '';
  } catch {
    return '';
  }
}

function detectEts2Path() {
  const candidates = [
    queryRegistryInstallDir('SOFTWARE\\SCS Software\\Euro Truck Simulator 2'),
    queryRegistryInstallDir('SOFTWARE\\WOW6432Node\\SCS Software\\Euro Truck Simulator 2'),
    'C:\\Program Files (x86)\\Steam\\steamapps\\common\\Euro Truck Simulator 2',
    'D:\\Steam\\steamapps\\common\\Euro Truck Simulator 2',
    'D:\\SteamLibrary\\steamapps\\common\\Euro Truck Simulator 2',
    'E:\\SteamLibrary\\steamapps\\common\\Euro Truck Simulator 2'
  ];

  return candidates.find((item) => item && fs.existsSync(item)) || '';
}

function installState(ets2Path) {
  if (!looksLikeEts2(ets2Path)) {
    return {
      ok: false,
      text: 'ETS2 路径未设置或不像有效安装目录',
      pluginDir: ''
    };
  }

  const dllInstalled = fs.existsSync(installedDllPath(ets2Path));
  const configExists = fs.existsSync(configPath(ets2Path));
  return {
    ok: true,
    dllInstalled,
    configExists,
    pluginDir: pluginDir(ets2Path),
    text: `${dllInstalled ? 'DLL 已安装' : 'DLL 未安装'}，${configExists ? '配置存在' : '配置不存在'}`
  };
}

ipcMain.handle('detect-path', () => detectEts2Path());

ipcMain.handle('browse-path', async () => {
  const result = await dialog.showOpenDialog(mainWindow, {
    title: '选择 Euro Truck Simulator 2 安装目录',
    properties: ['openDirectory']
  });
  return result.canceled ? '' : result.filePaths[0];
});

ipcMain.handle('state', (_event, ets2Path) => installState(ets2Path));

ipcMain.handle('install-dll', (_event, ets2Path) => {
  if (!looksLikeEts2(ets2Path)) throw new Error('请先选择有效的 ETS2 安装目录');
  const src = sourceDllPath();
  if (!fs.existsSync(src)) throw new Error(`未找到 ${DLL_NAME}，请先运行 build.bat`);
  fs.mkdirSync(pluginDir(ets2Path), { recursive: true });
  fs.copyFileSync(src, installedDllPath(ets2Path));
  return installState(ets2Path);
});

ipcMain.handle('uninstall-dll', (_event, ets2Path) => {
  const dll = installedDllPath(ets2Path);
  if (fs.existsSync(dll)) fs.unlinkSync(dll);
  return installState(ets2Path);
});

ipcMain.handle('read-config', (_event, ets2Path) => {
  const file = configPath(ets2Path);
  if (!fs.existsSync(file)) return null;
  return fs.readFileSync(file, 'utf8');
});

ipcMain.handle('write-config', (_event, ets2Path, jsonText) => {
  if (!looksLikeEts2(ets2Path)) throw new Error('请先选择有效的 ETS2 安装目录');
  const file = configPath(ets2Path);
  fs.mkdirSync(path.dirname(file), { recursive: true });
  JSON.parse(jsonText);
  fs.writeFileSync(file, jsonText, 'utf8');
  return installState(ets2Path);
});
