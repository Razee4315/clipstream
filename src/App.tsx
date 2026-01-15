import { useEffect, useState, useRef, useCallback } from 'preact/hooks';
import { invoke } from '@tauri-apps/api/core';
import './index.css';

interface ClipboardEntry {
  id: number;
  content: string;
  source_app: string | null;
  content_type: string;
  created_at: string;
  is_pinned: boolean;
  content_blob: string | null;
}

type ViewMode = 'list' | 'settings';
type PasteFormat = 'plain' | 'upper' | 'lower' | 'title' | 'trim';

const Icons = {
  search: (
    <svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor">
      <path d="M11.742 10.344a6.5 6.5 0 1 0-1.397 1.398h-.001c.03.04.062.078.098.115l3.85 3.85a1 1 0 0 0 1.415-1.414l-3.85-3.85a1.007 1.007 0 0 0-.115-.1zM12 6.5a5.5 5.5 0 1 1-11 0 5.5 5.5 0 0 1 11 0z" />
    </svg>
  ),
  clipboard: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M4 1.5H3a2 2 0 0 0-2 2V14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V3.5a2 2 0 0 0-2-2h-1v1h1a1 1 0 0 1 1 1V14a1 1 0 0 1-1 1H3a1 1 0 0 1-1-1V3.5a1 1 0 0 1 1-1h1v-1z" />
      <path d="M9.5 1a.5.5 0 0 1 .5.5v1a.5.5 0 0 1-.5.5h-3a.5.5 0 0 1-.5-.5v-1a.5.5 0 0 1 .5-.5h3zm-3-1A1.5 1.5 0 0 0 5 1.5v1A1.5 1.5 0 0 0 6.5 4h3A1.5 1.5 0 0 0 11 2.5v-1A1.5 1.5 0 0 0 9.5 0h-3z" />
    </svg>
  ),
  globe: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M0 8a8 8 0 1 1 16 0A8 8 0 0 1 0 8zm7.5-6.923c-.67.204-1.335.82-1.887 1.855A7.97 7.97 0 0 0 5.145 4H7.5V1.077zM4.09 4a9.267 9.267 0 0 1 .64-1.539 6.7 6.7 0 0 1 .597-.933A7.025 7.025 0 0 0 2.255 4H4.09zm-.582 3.5c.03-.877.138-1.718.312-2.5H1.674a6.958 6.958 0 0 0-.656 2.5h2.49zM4.847 5a12.5 12.5 0 0 0-.338 2.5H7.5V5H4.847zM8.5 5v2.5h2.99a12.495 12.495 0 0 0-.337-2.5H8.5zM4.51 8.5a12.5 12.5 0 0 0 .337 2.5H7.5V8.5H4.51zm3.99 0V11h2.653c.187-.765.306-1.608.338-2.5H8.5zM5.145 12c.138.386.295.744.468 1.068.552 1.035 1.218 1.65 1.887 1.855V12H5.145zm.182 2.472a6.696 6.696 0 0 1-.597-.933A9.268 9.268 0 0 1 4.09 12H2.255a7.024 7.024 0 0 0 3.072 2.472zM3.82 11a13.652 13.652 0 0 1-.312-2.5h-2.49c.062.89.291 1.733.656 2.5H3.82zm6.853 3.472A7.024 7.024 0 0 0 13.745 12H11.91a9.27 9.27 0 0 1-.64 1.539 6.688 6.688 0 0 1-.597.933zM8.5 12v2.923c.67-.204 1.335-.82 1.887-1.855.173-.324.33-.682.468-1.068H8.5zm3.68-1h2.146c.365-.767.594-1.61.656-2.5h-2.49a13.65 13.65 0 0 1-.312 2.5zm2.802-3.5a6.959 6.959 0 0 0-.656-2.5H12.18c.174.782.282 1.623.312 2.5h2.49zM11.27 2.461c.247.464.462.98.64 1.539h1.835a7.024 7.024 0 0 0-3.072-2.472c.218.284.418.598.597.933zM10.855 4a7.966 7.966 0 0 0-.468-1.068C9.835 1.897 9.17 1.282 8.5 1.077V4h2.355z" />
    </svg>
  ),
  code: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M5.854 4.854a.5.5 0 1 0-.708-.708l-3.5 3.5a.5.5 0 0 0 0 .708l3.5 3.5a.5.5 0 0 0 .708-.708L2.707 8l3.147-3.146zm4.292 0a.5.5 0 0 1 .708-.708l3.5 3.5a.5.5 0 0 1 0 .708l-3.5 3.5a.5.5 0 0 1-.708-.708L13.293 8l-3.147-3.146z" />
    </svg>
  ),
  link: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M4.715 6.542 3.343 7.914a3 3 0 1 0 4.243 4.243l1.828-1.829A3 3 0 0 0 8.586 5.5L8 6.086a1.002 1.002 0 0 0-.154.199 2 2 0 0 1 .861 3.337L6.88 11.45a2 2 0 1 1-2.83-2.83l.793-.792a4.018 4.018 0 0 1-.128-1.287z" />
      <path d="M6.586 4.672A3 3 0 0 0 7.414 9.5l.775-.776a2 2 0 0 1-.896-3.346L9.12 3.55a2 2 0 1 1 2.83 2.83l-.793.792c.112.42.155.855.128 1.287l1.372-1.372a3 3 0 1 0-4.243-4.243L6.586 4.672z" />
    </svg>
  ),
  image: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M6.002 5.5a1.5 1.5 0 1 1-3 0 1.5 1.5 0 0 1 3 0z" />
      <path d="M2.002 1a2 2 0 0 0-2 2v10a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V3a2 2 0 0 0-2-2h-12zm12 1a1 1 0 0 1 1 1v6.5l-3.777-1.947a.5.5 0 0 0-.577.093l-3.71 3.71-2.66-1.772a.5.5 0 0 0-.63.062L1.002 12V3a1 1 0 0 1 1-1h12z" />
    </svg>
  ),
  pin: (
    <svg width="10" height="10" viewBox="0 0 16 16" fill="currentColor">
      <path d="M9.828.722a.5.5 0 0 1 .354.146l4.95 4.95a.5.5 0 0 1 0 .707c-.48.48-1.072.588-1.503.588-.177 0-.335-.018-.46-.039l-3.134 3.134a5.927 5.927 0 0 1 .16 1.013c.046.702-.032 1.687-.72 2.375a.5.5 0 0 1-.707 0l-2.829-2.828-3.182 3.182c-.195.195-1.219.902-1.414.707-.195-.195.512-1.22.707-1.414l3.182-3.182-2.828-2.829a.5.5 0 0 1 0-.707c.688-.688 1.673-.767 2.375-.72a5.922 5.922 0 0 1 1.013.16l3.134-3.133a2.772 2.772 0 0 1-.04-.461c0-.43.108-1.022.589-1.503a.5.5 0 0 1 .353-.146z" />
    </svg>
  ),
  pinOutline: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M9.828.722a.5.5 0 0 1 .354.146l4.95 4.95a.5.5 0 0 1 0 .707c-.48.48-1.072.588-1.503.588-.177 0-.335-.018-.46-.039l-3.134 3.134a5.927 5.927 0 0 1 .16 1.013c.046.702-.032 1.687-.72 2.375a.5.5 0 0 1-.707 0l-2.829-2.828-3.182 3.182c-.195.195-1.219.902-1.414.707-.195-.195.512-1.22.707-1.414l3.182-3.182-2.828-2.829a.5.5 0 0 1 0-.707c.688-.688 1.673-.767 2.375-.72a5.922 5.922 0 0 1 1.013.16l3.134-3.133a2.772 2.772 0 0 1-.04-.461c0-.43.108-1.022.589-1.503a.5.5 0 0 1 .353-.146zm.122 2.112v-.002.002zm0-.002v.002a.5.5 0 0 1-.122.51L6.293 6.878a.5.5 0 0 1-.511.12H5.78l-.014-.004a4.507 4.507 0 0 0-.288-.076 4.922 4.922 0 0 0-.765-.116c-.422-.028-.836.008-1.175.15l5.51 5.509c.141-.34.177-.753.149-1.175a4.924 4.924 0 0 0-.192-1.054l-.004-.013v-.001a.5.5 0 0 1 .12-.512l3.536-3.535a.5.5 0 0 1 .532-.115l.096.022c.087.017.208.034.344.034.114 0 .23-.011.343-.04L9.927 2.028c-.029.113-.04.23-.04.343a1.779 1.779 0 0 0 .062.46z" />
    </svg>
  ),
  copy: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M4 2a2 2 0 0 1 2-2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V2zm2-1a1 1 0 0 0-1 1v8a1 1 0 0 0 1 1h8a1 1 0 0 0 1-1V2a1 1 0 0 0-1-1H6zM2 5a1 1 0 0 0-1 1v8a1 1 0 0 0 1 1h8a1 1 0 0 0 1-1v-1h1v1a2 2 0 0 1-2 2H2a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h1v1H2z" />
    </svg>
  ),
  trash: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M5.5 5.5A.5.5 0 0 1 6 6v6a.5.5 0 0 1-1 0V6a.5.5 0 0 1 .5-.5zm2.5 0a.5.5 0 0 1 .5.5v6a.5.5 0 0 1-1 0V6a.5.5 0 0 1 .5-.5zm3 .5a.5.5 0 0 0-1 0v6a.5.5 0 0 0 1 0V6z" />
      <path fillRule="evenodd" d="M14.5 3a1 1 0 0 1-1 1H13v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V4h-.5a1 1 0 0 1-1-1V2a1 1 0 0 1 1-1H6a1 1 0 0 1 1-1h2a1 1 0 0 1 1 1h3.5a1 1 0 0 1 1 1v1zM4.118 4 4 4.059V13a1 1 0 0 0 1 1h6a1 1 0 0 0 1-1V4.059L11.882 4H4.118zM2.5 3V2h11v1h-11z" />
    </svg>
  ),
  terminal: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M6 9a.5.5 0 0 1 .5-.5h3a.5.5 0 0 1 0 1h-3A.5.5 0 0 1 6 9zM3.854 4.146a.5.5 0 1 0-.708.708L4.793 6.5 3.146 8.146a.5.5 0 1 0 .708.708l2-2a.5.5 0 0 0 0-.708l-2-2z" />
      <path d="M2 1a2 2 0 0 0-2 2v10a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V3a2 2 0 0 0-2-2H2zm12 1a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H2a1 1 0 0 1-1-1V3a1 1 0 0 1 1-1h12z" />
    </svg>
  ),
  file: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M5.5 7a.5.5 0 0 0 0 1h5a.5.5 0 0 0 0-1h-5zM5 9.5a.5.5 0 0 1 .5-.5h5a.5.5 0 0 1 0 1h-5a.5.5 0 0 1-.5-.5zm0 2a.5.5 0 0 1 .5-.5h2a.5.5 0 0 1 0 1h-2a.5.5 0 0 1-.5-.5z" />
      <path d="M9.5 0H4a2 2 0 0 0-2 2v12a2 2 0 0 0 2 2h8a2 2 0 0 0 2-2V4.5L9.5 0zm0 1v2A1.5 1.5 0 0 0 11 4.5h2V14a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V2a1 1 0 0 1 1-1h5.5z" />
    </svg>
  ),
  text: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path fillRule="evenodd" d="M0 .5A.5.5 0 0 1 .5 0h4a.5.5 0 0 1 0 1h-4A.5.5 0 0 1 0 .5zm0 2A.5.5 0 0 1 .5 2h7a.5.5 0 0 1 0 1h-7a.5.5 0 0 1-.5-.5zm0 2A.5.5 0 0 1 .5 4h7a.5.5 0 0 1 0 1h-7a.5.5 0 0 1-.5-.5zm0 2A.5.5 0 0 1 .5 6h4a.5.5 0 0 1 0 1h-4a.5.5 0 0 1-.5-.5zm0 2A.5.5 0 0 1 .5 8h7a.5.5 0 0 1 0 1h-7a.5.5 0 0 1-.5-.5zm0 2a.5.5 0 0 1 .5-.5h7a.5.5 0 0 1 0 1h-7a.5.5 0 0 1-.5-.5zm0 2a.5.5 0 0 1 .5-.5h4a.5.5 0 0 1 0 1h-4a.5.5 0 0 1-.5-.5z" />
    </svg>
  ),
  settings: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M8 4.754a3.246 3.246 0 1 0 0 6.492 3.246 3.246 0 0 0 0-6.492zM5.754 8a2.246 2.246 0 1 1 4.492 0 2.246 2.246 0 0 1-4.492 0z" />
      <path d="M9.796 1.343c-.527-1.79-3.065-1.79-3.592 0l-.094.319a.873.873 0 0 1-1.255.52l-.292-.16c-1.64-.892-3.433.902-2.54 2.541l.159.292a.873.873 0 0 1-.52 1.255l-.319.094c-1.79.527-1.79 3.065 0 3.592l.319.094a.873.873 0 0 1 .52 1.255l-.16.292c-.892 1.64.901 3.434 2.541 2.54l.292-.159a.873.873 0 0 1 1.255.52l.094.319c.527 1.79 3.065 1.79 3.592 0l.094-.319a.873.873 0 0 1 1.255-.52l.292.16c1.64.893 3.434-.902 2.54-2.541l-.159-.292a.873.873 0 0 1 .52-1.255l.319-.094c1.79-.527 1.79-3.065 0-3.592l-.319-.094a.873.873 0 0 1-.52-1.255l.16-.292c.893-1.64-.902-3.433-2.541-2.54l-.292.159a.873.873 0 0 1-1.255-.52l-.094-.319zm-2.633.283c.246-.835 1.428-.835 1.674 0l.094.319a1.873 1.873 0 0 0 2.693 1.115l.291-.16c.764-.415 1.6.42 1.184 1.185l-.159.292a1.873 1.873 0 0 0 1.116 2.692l.318.094c.835.246.835 1.428 0 1.674l-.319.094a1.873 1.873 0 0 0-1.115 2.693l.16.291c.415.764-.42 1.6-1.185 1.184l-.291-.159a1.873 1.873 0 0 0-2.693 1.116l-.094.318c-.246.835-1.428.835-1.674 0l-.094-.319a1.873 1.873 0 0 0-2.692-1.115l-.292.16c-.764.415-1.6-.42-1.184-1.185l.159-.291A1.873 1.873 0 0 0 1.945 8.93l-.319-.094c-.835-.246-.835-1.428 0-1.674l.319-.094A1.873 1.873 0 0 0 3.06 4.377l-.16-.292c-.415-.764.42-1.6 1.185-1.184l.292.159a1.873 1.873 0 0 0 2.692-1.115l.094-.319z" />
    </svg>
  ),
  edit: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M12.146.146a.5.5 0 0 1 .708 0l3 3a.5.5 0 0 1 0 .708l-10 10a.5.5 0 0 1-.168.11l-5 2a.5.5 0 0 1-.65-.65l2-5a.5.5 0 0 1 .11-.168l10-10zM11.207 2.5 13.5 4.793 14.793 3.5 12.5 1.207 11.207 2.5zm1.586 3L10.5 3.207 4 9.707V10h.5a.5.5 0 0 1 .5.5v.5h.5a.5.5 0 0 1 .5.5v.5h.293l6.5-6.5zm-9.761 5.175-.106.106-1.528 3.821 3.821-1.528.106-.106A.5.5 0 0 1 5 12.5V12h-.5a.5.5 0 0 1-.5-.5V11h-.5a.5.5 0 0 1-.468-.325z" />
    </svg>
  ),
  back: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path fillRule="evenodd" d="M15 8a.5.5 0 0 0-.5-.5H2.707l3.147-3.146a.5.5 0 1 0-.708-.708l-4 4a.5.5 0 0 0 0 .708l4 4a.5.5 0 0 0 .708-.708L2.707 8.5H14.5A.5.5 0 0 0 15 8z" />
    </svg>
  ),
  plus: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M8 4a.5.5 0 0 1 .5.5v3h3a.5.5 0 0 1 0 1h-3v3a.5.5 0 0 1-1 0v-3h-3a.5.5 0 0 1 0-1h3v-3A.5.5 0 0 1 8 4z" />
    </svg>
  ),
};

function getAppIcon(app: string | null, contentType: string) {
  if (contentType === 'image') return Icons.image;
  if (!app) return Icons.clipboard;

  const appLower = app.toLowerCase();
  if (appLower.includes('chrome') || appLower.includes('firefox') || appLower.includes('edge') || appLower.includes('brave') || appLower.includes('safari')) {
    return Icons.globe;
  }
  if (appLower.includes('code') || appLower.includes('sublime') || appLower.includes('notepad++') || appLower.includes('vim') || appLower.includes('emacs')) {
    return Icons.code;
  }
  if (appLower.includes('terminal') || appLower.includes('cmd') || appLower.includes('powershell') || appLower.includes('iterm') || appLower.includes('konsole')) {
    return Icons.terminal;
  }
  if (appLower.includes('explorer') || appLower.includes('finder') || appLower.includes('nautilus')) {
    return Icons.file;
  }

  if (contentType === 'url') return Icons.link;
  if (contentType === 'code') return Icons.code;

  return Icons.text;
}

function formatTime(dateStr: string): string {
  const date = new Date(dateStr + 'Z');
  const now = new Date();
  const diff = Math.floor((now.getTime() - date.getTime()) / 1000);

  if (diff < 60) return 'now';
  if (diff < 3600) return `${Math.floor(diff / 60)}m`;
  if (diff < 86400) return `${Math.floor(diff / 3600)}h`;
  return `${Math.floor(diff / 86400)}d`;
}

function getAppDisplayName(app: string | null): string {
  if (!app) return 'Unknown';
  return app.replace('.exe', '').replace('.EXE', '').replace('.app', '');
}

// Settings View Component
function SettingsView({ onBack }: { onBack: () => void }) {
  const [ignoredApps, setIgnoredApps] = useState<string[]>([]);
  const [newApp, setNewApp] = useState('');

  useEffect(() => {
    loadIgnoredApps();
  }, []);

  const loadIgnoredApps = async () => {
    try {
      const apps = await invoke<string[]>('get_ignored_apps');
      setIgnoredApps(apps);
    } catch (e) {
      console.error('Failed to load ignored apps:', e);
    }
  };

  const addApp = async () => {
    if (!newApp.trim()) return;
    try {
      await invoke('add_ignored_app', { appName: newApp.trim() });
      setNewApp('');
      await loadIgnoredApps();
    } catch (e) {
      console.error('Failed to add app:', e);
    }
  };

  const removeApp = async (appName: string) => {
    try {
      await invoke('remove_ignored_app', { appName });
      await loadIgnoredApps();
    } catch (e) {
      console.error('Failed to remove app:', e);
    }
  };

  return (
    <div className="settings-view">
      <div className="settings-header">
        <button onClick={onBack} className="back-btn">{Icons.back}</button>
        <span>Settings</span>
      </div>

      <div className="settings-section">
        <h3>Ignored Apps</h3>
        <p className="settings-desc">Clipboard from these apps won't be saved</p>

        <div className="add-app-row">
          <input
            type="text"
            value={newApp}
            onInput={(e) => setNewApp((e.target as HTMLInputElement).value)}
            onKeyDown={(e) => e.key === 'Enter' && addApp()}
            placeholder="App name (e.g., 1Password)"
            className="app-input"
          />
          <button onClick={addApp} className="add-btn">{Icons.plus}</button>
        </div>

        <div className="ignored-apps-list">
          {ignoredApps.length === 0 ? (
            <div className="empty-apps">No ignored apps</div>
          ) : (
            ignoredApps.map((app) => (
              <div key={app} className="ignored-app-item">
                <span>{app}</span>
                <button onClick={() => removeApp(app)} className="remove-btn">{Icons.trash}</button>
              </div>
            ))
          )}
        </div>
      </div>
    </div>
  );
}

export function App() {
  const [entries, setEntries] = useState<ClipboardEntry[]>([]);
  const [query, setQuery] = useState('');
  const [selectedIndex, setSelectedIndex] = useState(0);
  const [isDark, setIsDark] = useState(true);
  const [viewMode, setViewMode] = useState<ViewMode>('list');
  const [editingId, setEditingId] = useState<number | null>(null);
  const [editText, setEditText] = useState('');
  const [showPasteMenu, setShowPasteMenu] = useState(false);
  const searchRef = useRef<HTMLInputElement>(null);
  const listRef = useRef<HTMLDivElement>(null);
  const editRef = useRef<HTMLInputElement>(null);

  const loadEntries = useCallback(async () => {
    try {
      const results = await invoke<ClipboardEntry[]>('search_history', { query });
      setEntries(results);
      if (selectedIndex >= results.length) {
        setSelectedIndex(Math.max(0, results.length - 1));
      }
    } catch (e) {
      console.error('Failed to load entries:', e);
    }
  }, [query]);

  useEffect(() => {
    const mediaQuery = window.matchMedia('(prefers-color-scheme: dark)');
    setIsDark(mediaQuery.matches);
    const handler = (e: MediaQueryListEvent) => setIsDark(e.matches);
    mediaQuery.addEventListener('change', handler);
    return () => mediaQuery.removeEventListener('change', handler);
  }, []);

  useEffect(() => {
    loadEntries();
    const interval = setInterval(loadEntries, 1000);
    return () => clearInterval(interval);
  }, [loadEntries]);

  useEffect(() => {
    if (viewMode === 'list') {
      searchRef.current?.focus();
    }
  }, [viewMode]);

  useEffect(() => {
    if (listRef.current && entries.length > 0 && selectedIndex < entries.length) {
      const selectedEl = listRef.current.children[selectedIndex] as HTMLElement;
      selectedEl?.scrollIntoView({ block: 'nearest' });
    }
  }, [selectedIndex, entries.length]);

  useEffect(() => {
    if (editingId !== null && editRef.current) {
      editRef.current.focus();
      editRef.current.select();
    }
  }, [editingId]);

  const handleKeyDown = async (e: KeyboardEvent) => {
    if (editingId !== null) {
      if (e.key === 'Escape') {
        setEditingId(null);
        setEditText('');
      } else if (e.key === 'Enter') {
        await saveEdit();
      }
      return;
    }

    switch (e.key) {
      case 'ArrowDown':
        e.preventDefault();
        setSelectedIndex(i => Math.min(i + 1, entries.length - 1));
        break;
      case 'ArrowUp':
        e.preventDefault();
        setSelectedIndex(i => Math.max(i - 1, 0));
        break;
      case 'Enter':
        e.preventDefault();
        if (e.shiftKey) {
          setShowPasteMenu(!showPasteMenu);
        } else if (entries[selectedIndex]) {
          await handlePaste(entries[selectedIndex].id);
        }
        break;
      case 'Escape':
        e.preventDefault();
        if (showPasteMenu) {
          setShowPasteMenu(false);
        } else {
          await invoke('hide_window');
        }
        break;
      case 'Delete':
        if (e.shiftKey && entries[selectedIndex]) {
          e.preventDefault();
          await handleDelete(entries[selectedIndex].id);
        }
        break;
      case 'F2':
        if (entries[selectedIndex] && entries[selectedIndex].content_type !== 'image') {
          e.preventDefault();
          startEdit(entries[selectedIndex]);
        }
        break;
    }
  };

  const startEdit = (entry: ClipboardEntry) => {
    setEditingId(entry.id);
    setEditText(entry.content);
  };

  const saveEdit = async () => {
    if (editingId === null) return;
    try {
      await invoke('update_entry', { id: editingId, content: editText });
      await loadEntries();
    } catch (e) {
      console.error('Failed to save edit:', e);
    }
    setEditingId(null);
    setEditText('');
  };

  const handlePaste = async (id: number) => {
    try {
      await invoke('hide_window');
      await new Promise(resolve => setTimeout(resolve, 100));
      await invoke('paste_entry', { id });
    } catch (e) {
      console.error('Failed to paste:', e);
    }
  };

  const handlePasteFormatted = async (id: number, format: PasteFormat) => {
    try {
      setShowPasteMenu(false);
      await invoke('hide_window');
      await new Promise(resolve => setTimeout(resolve, 100));
      await invoke('paste_formatted', { id, format });
    } catch (e) {
      console.error('Failed to paste:', e);
    }
  };

  const handleCopy = async (id: number, e: MouseEvent) => {
    e.stopPropagation();
    try {
      await invoke('copy_entry', { id });
    } catch (err) {
      console.error('Failed to copy:', err);
    }
  };

  const handlePin = async (id: number, e: MouseEvent) => {
    e.stopPropagation();
    try {
      await invoke('toggle_pin', { id });
      await loadEntries();
    } catch (err) {
      console.error('Failed to pin:', err);
    }
  };

  const handleDelete = async (id: number, e?: MouseEvent) => {
    e?.stopPropagation();
    try {
      await invoke('delete_entry', { id });
      await loadEntries();
    } catch (err) {
      console.error('Failed to delete:', err);
    }
  };

  if (viewMode === 'settings') {
    return (
      <div className={`app-container ${isDark ? 'dark' : 'light'}`}>
        <SettingsView onBack={() => setViewMode('list')} />
      </div>
    );
  }

  return (
    <div
      className={`app-container ${isDark ? 'dark' : 'light'}`}
      onKeyDown={handleKeyDown}
    >
      <div className="search-container">
        <span className="search-icon">{Icons.search}</span>
        <input
          ref={searchRef}
          type="text"
          value={query}
          onInput={(e) => { setQuery((e.target as HTMLInputElement).value); setSelectedIndex(0); }}
          placeholder="Search..."
          className="search-input"
        />
        <button onClick={() => setViewMode('settings')} className="settings-btn" title="Settings">
          {Icons.settings}
        </button>
        <span className="item-count">{entries.length}</span>
      </div>

      <div ref={listRef} className="entries-list">
        {entries.length === 0 ? (
          <div className="empty-state">
            <span className="empty-icon">{Icons.clipboard}</span>
            <p>{query ? 'No results' : 'Empty'}</p>
          </div>
        ) : (
          entries.map((entry, index) => (
            <div
              key={entry.id}
              onClick={() => setSelectedIndex(index)}
              onDblClick={() => entry.content_type !== 'image' ? startEdit(entry) : handlePaste(entry.id)}
              className={`entry-item ${index === selectedIndex ? 'selected' : ''}`}
            >
              <div className="entry-icon">
                {getAppIcon(entry.source_app, entry.content_type)}
              </div>

              <div className="entry-content">
                {editingId === entry.id ? (
                  <input
                    ref={editRef}
                    type="text"
                    value={editText}
                    onInput={(e) => setEditText((e.target as HTMLInputElement).value)}
                    onBlur={saveEdit}
                    className="edit-input"
                  />
                ) : entry.content_type === 'image' && entry.content_blob ? (
                  <div className="image-preview">
                    <img src={`data:image/png;base64,${entry.content_blob}`} alt="Clipboard" />
                    <span className="image-label">{entry.content}</span>
                  </div>
                ) : (
                  <p className="entry-text">{entry.content}</p>
                )}
                <div className="entry-meta">
                  <span>{getAppDisplayName(entry.source_app)}</span>
                  <span>·</span>
                  <span>{formatTime(entry.created_at)}</span>
                </div>
              </div>

              <div className="entry-actions">
                {entry.is_pinned && (
                  <span className="pinned-indicator">{Icons.pin}</span>
                )}
                {index === selectedIndex && (
                  <>
                    {entry.content_type !== 'image' && (
                      <button onClick={(e) => { e.stopPropagation(); startEdit(entry); }} className="action-btn" title="Edit (F2)">
                        {Icons.edit}
                      </button>
                    )}
                    <button onClick={(e) => handlePin(entry.id, e)} className="action-btn" title="Pin">
                      {Icons.pinOutline}
                    </button>
                    <button onClick={(e) => handleCopy(entry.id, e)} className="action-btn" title="Copy">
                      {Icons.copy}
                    </button>
                    <button onClick={(e) => handleDelete(entry.id, e)} className="action-btn delete" title="Delete">
                      {Icons.trash}
                    </button>
                  </>
                )}
              </div>

              {/* Paste Format Menu */}
              {showPasteMenu && index === selectedIndex && entry.content_type !== 'image' && (
                <div className="paste-menu">
                  <button onClick={() => handlePasteFormatted(entry.id, 'plain')}>Plain</button>
                  <button onClick={() => handlePasteFormatted(entry.id, 'upper')}>UPPER</button>
                  <button onClick={() => handlePasteFormatted(entry.id, 'lower')}>lower</button>
                  <button onClick={() => handlePasteFormatted(entry.id, 'title')}>Title</button>
                  <button onClick={() => handlePasteFormatted(entry.id, 'trim')}>Trim</button>
                </div>
              )}
            </div>
          ))
        )}
      </div>

      <div className="footer">
        <div className="shortcuts">
          <span><kbd>↑↓</kbd>Nav</span>
          <span><kbd>↵</kbd>Paste</span>
          <span><kbd>⇧↵</kbd>Format</span>
          <span><kbd>F2</kbd>Edit</span>
          <span><kbd>Esc</kbd>Close</span>
        </div>
      </div>
    </div>
  );
}

export default App;
