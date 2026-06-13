const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('managerApi', {
  detectPath: (game) => ipcRenderer.invoke('detect-path', game),
  browsePath: (game) => ipcRenderer.invoke('browse-path', game),
  state: (game, gamePath) => ipcRenderer.invoke('state', game, gamePath),
  installDll: (game, gamePath) => ipcRenderer.invoke('install-dll', game, gamePath),
  uninstallDll: (game, gamePath) => ipcRenderer.invoke('uninstall-dll', game, gamePath),
  readConfig: (game, gamePath) => ipcRenderer.invoke('read-config', game, gamePath),
  writeConfig: (game, gamePath, jsonText) => ipcRenderer.invoke('write-config', game, gamePath, jsonText),
  testConfig: (jsonText) => ipcRenderer.invoke('test-config', jsonText),
  listPresets: () => ipcRenderer.invoke('list-presets'),
  savePreset: (name, jsonText) => ipcRenderer.invoke('save-preset', name, jsonText),
  deletePreset: (name) => ipcRenderer.invoke('delete-preset', name),
  getUpdateOptions: () => ipcRenderer.invoke('get-update-options'),
  saveUpdateSettings: (settings) => ipcRenderer.invoke('save-update-settings', settings),
  speedTestUpdateProxies: () => ipcRenderer.invoke('speed-test-update-proxies'),
  checkUpdate: (forceSpeedTest) => ipcRenderer.invoke('check-update', forceSpeedTest),
  downloadUpdate: (downloadUrl, fileName, proxyId) => ipcRenderer.invoke('download-update', downloadUrl, fileName, proxyId),
  onUpdateDownloadProgress: (callback) => {
    ipcRenderer.on('update-download-progress', (_event, progress) => callback(progress));
  }
});
