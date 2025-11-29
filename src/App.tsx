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
}

const Icons = {
  search: (
    <svg width="14" height="14" viewBox="0 0 16 16" fill="currentColor">
      <path d="M11.742 10.344a6.5 6.5 0 1 0-1.397 1.398h-.001c.03.04.062.078.098.115l3.85 3.85a1 1 0 0 0 1.415-1.414l-3.85-3.85a1.007 1.007 0 0 0-.115-.1zM12 6.5a5.5 5.5 0 1 1-11 0 5.5 5.5 0 0 1 11 0z"/>
    </svg>
  ),
  clipboard: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M4 1.5H3a2 2 0 0 0-2 2V14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V3.5a2 2 0 0 0-2-2h-1v1h1a1 1 0 0 1 1 1V14a1 1 0 0 1-1 1H3a1 1 0 0 1-1-1V3.5a1 1 0 0 1 1-1h1v-1z"/>
      <path d="M9.5 1a.5.5 0 0 1 .5.5v1a.5.5 0 0 1-.5.5h-3a.5.5 0 0 1-.5-.5v-1a.5.5 0 0 1 .5-.5h3zm-3-1A1.5 1.5 0 0 0 5 1.5v1A1.5 1.5 0 0 0 6.5 4h3A1.5 1.5 0 0 0 11 2.5v-1A1.5 1.5 0 0 0 9.5 0h-3z"/>
    </svg>
  ),
  globe: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M0 8a8 8 0 1 1 16 0A8 8 0 0 1 0 8zm7.5-6.923c-.67.204-1.335.82-1.887 1.855A7.97 7.97 0 0 0 5.145 4H7.5V1.077zM4.09 4a9.267 9.267 0 0 1 .64-1.539 6.7 6.7 0 0 1 .597-.933A7.025 7.025 0 0 0 2.255 4H4.09zm-.582 3.5c.03-.877.138-1.718.312-2.5H1.674a6.958 6.958 0 0 0-.656 2.5h2.49zM4.847 5a12.5 12.5 0 0 0-.338 2.5H7.5V5H4.847zM8.5 5v2.5h2.99a12.495 12.495 0 0 0-.337-2.5H8.5zM4.51 8.5a12.5 12.5 0 0 0 .337 2.5H7.5V8.5H4.51zm3.99 0V11h2.653c.187-.765.306-1.608.338-2.5H8.5zM5.145 12c.138.386.295.744.468 1.068.552 1.035 1.218 1.65 1.887 1.855V12H5.145zm.182 2.472a6.696 6.696 0 0 1-.597-.933A9.268 9.268 0 0 1 4.09 12H2.255a7.024 7.024 0 0 0 3.072 2.472zM3.82 11a13.652 13.652 0 0 1-.312-2.5h-2.49c.062.89.291 1.733.656 2.5H3.82zm6.853 3.472A7.024 7.024 0 0 0 13.745 12H11.91a9.27 9.27 0 0 1-.64 1.539 6.688 6.688 0 0 1-.597.933zM8.5 12v2.923c.67-.204 1.335-.82 1.887-1.855.173-.324.33-.682.468-1.068H8.5zm3.68-1h2.146c.365-.767.594-1.61.656-2.5h-2.49a13.65 13.65 0 0 1-.312 2.5zm2.802-3.5a6.959 6.959 0 0 0-.656-2.5H12.18c.174.782.282 1.623.312 2.5h2.49zM11.27 2.461c.247.464.462.98.64 1.539h1.835a7.024 7.024 0 0 0-3.072-2.472c.218.284.418.598.597.933zM10.855 4a7.966 7.966 0 0 0-.468-1.068C9.835 1.897 9.17 1.282 8.5 1.077V4h2.355z"/>
    </svg>
  ),
  code: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M5.854 4.854a.5.5 0 1 0-.708-.708l-3.5 3.5a.5.5 0 0 0 0 .708l3.5 3.5a.5.5 0 0 0 .708-.708L2.707 8l3.147-3.146zm4.292 0a.5.5 0 0 1 .708-.708l3.5 3.5a.5.5 0 0 1 0 .708l-3.5 3.5a.5.5 0 0 1-.708-.708L13.293 8l-3.147-3.146z"/>
    </svg>
  ),
  link: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M4.715 6.542 3.343 7.914a3 3 0 1 0 4.243 4.243l1.828-1.829A3 3 0 0 0 8.586 5.5L8 6.086a1.002 1.002 0 0 0-.154.199 2 2 0 0 1 .861 3.337L6.88 11.45a2 2 0 1 1-2.83-2.83l.793-.792a4.018 4.018 0 0 1-.128-1.287z"/>
      <path d="M6.586 4.672A3 3 0 0 0 7.414 9.5l.775-.776a2 2 0 0 1-.896-3.346L9.12 3.55a2 2 0 1 1 2.83 2.83l-.793.792c.112.42.155.855.128 1.287l1.372-1.372a3 3 0 1 0-4.243-4.243L6.586 4.672z"/>
    </svg>
  ),
  pin: (
    <svg width="10" height="10" viewBox="0 0 16 16" fill="currentColor">
      <path d="M9.828.722a.5.5 0 0 1 .354.146l4.95 4.95a.5.5 0 0 1 0 .707c-.48.48-1.072.588-1.503.588-.177 0-.335-.018-.46-.039l-3.134 3.134a5.927 5.927 0 0 1 .16 1.013c.046.702-.032 1.687-.72 2.375a.5.5 0 0 1-.707 0l-2.829-2.828-3.182 3.182c-.195.195-1.219.902-1.414.707-.195-.195.512-1.22.707-1.414l3.182-3.182-2.828-2.829a.5.5 0 0 1 0-.707c.688-.688 1.673-.767 2.375-.72a5.922 5.922 0 0 1 1.013.16l3.134-3.133a2.772 2.772 0 0 1-.04-.461c0-.43.108-1.022.589-1.503a.5.5 0 0 1 .353-.146z"/>
    </svg>
  ),
  pinOutline: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M9.828.722a.5.5 0 0 1 .354.146l4.95 4.95a.5.5 0 0 1 0 .707c-.48.48-1.072.588-1.503.588-.177 0-.335-.018-.46-.039l-3.134 3.134a5.927 5.927 0 0 1 .16 1.013c.046.702-.032 1.687-.72 2.375a.5.5 0 0 1-.707 0l-2.829-2.828-3.182 3.182c-.195.195-1.219.902-1.414.707-.195-.195.512-1.22.707-1.414l3.182-3.182-2.828-2.829a.5.5 0 0 1 0-.707c.688-.688 1.673-.767 2.375-.72a5.922 5.922 0 0 1 1.013.16l3.134-3.133a2.772 2.772 0 0 1-.04-.461c0-.43.108-1.022.589-1.503a.5.5 0 0 1 .353-.146zm.122 2.112v-.002.002zm0-.002v.002a.5.5 0 0 1-.122.51L6.293 6.878a.5.5 0 0 1-.511.12H5.78l-.014-.004a4.507 4.507 0 0 0-.288-.076 4.922 4.922 0 0 0-.765-.116c-.422-.028-.836.008-1.175.15l5.51 5.509c.141-.34.177-.753.149-1.175a4.924 4.924 0 0 0-.192-1.054l-.004-.013v-.001a.5.5 0 0 1 .12-.512l3.536-3.535a.5.5 0 0 1 .532-.115l.096.022c.087.017.208.034.344.034.114 0 .23-.011.343-.04L9.927 2.028c-.029.113-.04.23-.04.343a1.779 1.779 0 0 0 .062.46z"/>
    </svg>
  ),
  copy: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M4 2a2 2 0 0 1 2-2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V2zm2-1a1 1 0 0 0-1 1v8a1 1 0 0 0 1 1h8a1 1 0 0 0 1-1V2a1 1 0 0 0-1-1H6zM2 5a1 1 0 0 0-1 1v8a1 1 0 0 0 1 1h8a1 1 0 0 0 1-1v-1h1v1a2 2 0 0 1-2 2H2a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h1v1H2z"/>
    </svg>
  ),
  trash: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M5.5 5.5A.5.5 0 0 1 6 6v6a.5.5 0 0 1-1 0V6a.5.5 0 0 1 .5-.5zm2.5 0a.5.5 0 0 1 .5.5v6a.5.5 0 0 1-1 0V6a.5.5 0 0 1 .5-.5zm3 .5a.5.5 0 0 0-1 0v6a.5.5 0 0 0 1 0V6z"/>
      <path fillRule="evenodd" d="M14.5 3a1 1 0 0 1-1 1H13v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V4h-.5a1 1 0 0 1-1-1V2a1 1 0 0 1 1-1H6a1 1 0 0 1 1-1h2a1 1 0 0 1 1 1h3.5a1 1 0 0 1 1 1v1zM4.118 4 4 4.059V13a1 1 0 0 0 1 1h6a1 1 0 0 0 1-1V4.059L11.882 4H4.118zM2.5 3V2h11v1h-11z"/>
    </svg>
  ),
  terminal: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M6 9a.5.5 0 0 1 .5-.5h3a.5.5 0 0 1 0 1h-3A.5.5 0 0 1 6 9zM3.854 4.146a.5.5 0 1 0-.708.708L4.793 6.5 3.146 8.146a.5.5 0 1 0 .708.708l2-2a.5.5 0 0 0 0-.708l-2-2z"/>
      <path d="M2 1a2 2 0 0 0-2 2v10a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V3a2 2 0 0 0-2-2H2zm12 1a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H2a1 1 0 0 1-1-1V3a1 1 0 0 1 1-1h12z"/>
    </svg>
  ),
  file: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path d="M5.5 7a.5.5 0 0 0 0 1h5a.5.5 0 0 0 0-1h-5zM5 9.5a.5.5 0 0 1 .5-.5h5a.5.5 0 0 1 0 1h-5a.5.5 0 0 1-.5-.5zm0 2a.5.5 0 0 1 .5-.5h2a.5.5 0 0 1 0 1h-2a.5.5 0 0 1-.5-.5z"/>
      <path d="M9.5 0H4a2 2 0 0 0-2 2v12a2 2 0 0 0 2 2h8a2 2 0 0 0 2-2V4.5L9.5 0zm0 1v2A1.5 1.5 0 0 0 11 4.5h2V14a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V2a1 1 0 0 1 1-1h5.5z"/>
    </svg>
  ),
  text: (
    <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
      <path fillRule="evenodd" d="M0 .5A.5.5 0 0 1 .5 0h4a.5.5 0 0 1 0 1h-4A.5.5 0 0 1 0 .5zm0 2A.5.5 0 0 1 .5 2h7a.5.5 0 0 1 0 1h-7a.5.5 0 0 1-.5-.5zm0 2A.5.5 0 0 1 .5 4h7a.5.5 0 0 1 0 1h-7a.5.5 0 0 1-.5-.5zm0 2A.5.5 0 0 1 .5 6h4a.5.5 0 0 1 0 1h-4a.5.5 0 0 1-.5-.5zm0 2A.5.5 0 0 1 .5 8h7a.5.5 0 0 1 0 1h-7a.5.5 0 0 1-.5-.5zm0 2a.5.5 0 0 1 .5-.5h7a.5.5 0 0 1 0 1h-7a.5.5 0 0 1-.5-.5zm0 2a.5.5 0 0 1 .5-.5h4a.5.5 0 0 1 0 1h-4a.5.5 0 0 1-.5-.5z"/>
    </svg>
  ),
};

function getAppIcon(app: string | null, contentType: string) {
  if (!app) return Icons.clipboard;
  
  const appLower = app.toLowerCase();
  if (appLower.includes('chrome') || appLower.includes('firefox') || appLower.includes('edge') || appLower.includes('brave')) {
    return Icons.globe;
  }
  if (appLower.includes('code') || appLower.includes('sublime') || appLower.includes('notepad++')) {
    return Icons.code;
  }
  if (appLower.includes('terminal') || appLower.includes('cmd') || appLower.includes('powershell')) {
    return Icons.terminal;
  }
  if (appLower.includes('explorer')) {
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
  return app.replace('.exe', '').replace('.EXE', '');
}

export function App() {
  const [entries, setEntries] = useState<ClipboardEntry[]>([]);
  const [query, setQuery] = useState('');
  const [selectedIndex, setSelectedIndex] = useState(0);
  const [isDark, setIsDark] = useState(true);
  const searchRef = useRef<HTMLInputElement>(null);
  const listRef = useRef<HTMLDivElement>(null);

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
    searchRef.current?.focus();
  }, []);

  useEffect(() => {
    if (listRef.current && entries.length > 0 && selectedIndex < entries.length) {
      const selectedEl = listRef.current.children[selectedIndex] as HTMLElement;
      selectedEl?.scrollIntoView({ block: 'nearest' });
    }
  }, [selectedIndex, entries.length]);

  const handleKeyDown = async (e: KeyboardEvent) => {
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
        if (entries[selectedIndex]) {
          await handlePaste(entries[selectedIndex].id);
        }
        break;
      case 'Escape':
        e.preventDefault();
        await invoke('hide_window');
        break;
      case 'Delete':
        if (e.shiftKey && entries[selectedIndex]) {
          e.preventDefault();
          await handleDelete(entries[selectedIndex].id);
        }
        break;
    }
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
              onDblClick={() => handlePaste(entry.id)}
              className={`entry-item ${index === selectedIndex ? 'selected' : ''}`}
            >
              <div className="entry-icon">
                {getAppIcon(entry.source_app, entry.content_type)}
              </div>
              
              <div className="entry-content">
                <p className="entry-text">{entry.content}</p>
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
            </div>
          ))
        )}
      </div>

      <div className="footer">
        <div className="shortcuts">
          <span><kbd>↑↓</kbd>Nav</span>
          <span><kbd>↵</kbd>Paste</span>
          <span><kbd>Esc</kbd>Close</span>
        </div>
      </div>
    </div>
  );
}

export default App;
