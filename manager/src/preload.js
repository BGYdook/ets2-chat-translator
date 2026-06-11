const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('managerApi', {
  detectPath: () => ipcRenderer.invoke('detect-path'),
  browsePath: () => ipcRenderer.invoke('browse-path'),
  state: (ets2Path) => ipcRenderer.invoke('state', ets2Path),
  installDll: (ets2Path) => ipcRenderer.invoke('install-dll', ets2Path),
  uninstallDll: (ets2Path) => ipcRenderer.invoke('uninstall-dll', ets2Path),
  readConfig: (ets2Path) => ipcRenderer.invoke('read-config', ets2Path),
  writeConfig: (ets2Path, jsonText) => ipcRenderer.invoke('write-config', ets2Path, jsonText)
});
