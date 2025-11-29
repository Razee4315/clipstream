use rusqlite::{Connection, params};
use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::sync::Mutex;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ClipboardEntry {
    pub id: i64,
    pub content: String,
    pub source_app: Option<String>,
    pub content_type: String,
    pub created_at: String,
    pub is_pinned: bool,
}

#[derive(Debug)]
pub struct Database {
    conn: Mutex<Connection>,
}

impl Database {
    pub fn new() -> Result<Self, rusqlite::Error> {
        let db_path = Self::get_db_path();
        if let Some(parent) = db_path.parent() {
            std::fs::create_dir_all(parent).ok();
        }
        
        let conn = Connection::open(&db_path)?;
        
        conn.execute_batch(
            "CREATE TABLE IF NOT EXISTS clipboard_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                content TEXT NOT NULL,
                source_app TEXT,
                content_type TEXT DEFAULT 'text',
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                is_pinned BOOLEAN DEFAULT 0
            );
            
            CREATE INDEX IF NOT EXISTS idx_created_at ON clipboard_history(created_at DESC);
            CREATE INDEX IF NOT EXISTS idx_source_app ON clipboard_history(source_app);
            "
        )?;
        
        // Create FTS5 virtual table if not exists
        conn.execute_batch(
            "CREATE VIRTUAL TABLE IF NOT EXISTS history_fts USING fts5(
                content,
                source_app,
                content='clipboard_history',
                content_rowid='id'
            );
            
            CREATE TRIGGER IF NOT EXISTS history_ai AFTER INSERT ON clipboard_history BEGIN
                INSERT INTO history_fts(rowid, content, source_app) VALUES (new.id, new.content, new.source_app);
            END;
            
            CREATE TRIGGER IF NOT EXISTS history_ad AFTER DELETE ON clipboard_history BEGIN
                INSERT INTO history_fts(history_fts, rowid, content, source_app) VALUES('delete', old.id, old.content, old.source_app);
            END;
            
            CREATE TRIGGER IF NOT EXISTS history_au AFTER UPDATE ON clipboard_history BEGIN
                INSERT INTO history_fts(history_fts, rowid, content, source_app) VALUES('delete', old.id, old.content, old.source_app);
                INSERT INTO history_fts(rowid, content, source_app) VALUES (new.id, new.content, new.source_app);
            END;"
        )?;
        
        Ok(Self { conn: Mutex::new(conn) })
    }
    
    fn get_db_path() -> PathBuf {
        let mut path = dirs::data_local_dir().unwrap_or_else(|| PathBuf::from("."));
        path.push("ClipStream");
        path.push("clipboard.db");
        path
    }
    
    pub fn insert(&self, content: &str, source_app: Option<&str>) -> Result<i64, rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        
        // Check for duplicate
        let exists: bool = conn.query_row(
            "SELECT EXISTS(SELECT 1 FROM clipboard_history WHERE content = ?1 ORDER BY created_at DESC LIMIT 1)",
            params![content],
            |row| row.get(0)
        ).unwrap_or(false);
        
        if exists {
            // Update timestamp of existing entry
            conn.execute(
                "UPDATE clipboard_history SET created_at = CURRENT_TIMESTAMP, source_app = COALESCE(?2, source_app) WHERE content = ?1",
                params![content, source_app]
            )?;
            let id: i64 = conn.query_row(
                "SELECT id FROM clipboard_history WHERE content = ?1",
                params![content],
                |row| row.get(0)
            )?;
            return Ok(id);
        }
        
        let content_type = Self::detect_content_type(content);
        
        conn.execute(
            "INSERT INTO clipboard_history (content, source_app, content_type) VALUES (?1, ?2, ?3)",
            params![content, source_app, content_type]
        )?;
        
        Ok(conn.last_insert_rowid())
    }
    
    fn detect_content_type(content: &str) -> &'static str {
        let trimmed = content.trim();
        if trimmed.starts_with("http://") || trimmed.starts_with("https://") {
            "url"
        } else if trimmed.contains("fn ") || trimmed.contains("function ") || 
                  trimmed.contains("def ") || trimmed.contains("class ") ||
                  trimmed.contains("const ") || trimmed.contains("let ") ||
                  trimmed.contains("import ") || trimmed.contains("#include") {
            "code"
        } else {
            "text"
        }
    }
    
    pub fn search(&self, query: &str, limit: usize) -> Result<Vec<ClipboardEntry>, rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        
        let query_trimmed = query.trim();
        
        if query_trimmed.is_empty() {
            let mut stmt = conn.prepare(
                "SELECT id, content, source_app, content_type, created_at, is_pinned 
                 FROM clipboard_history 
                 ORDER BY is_pinned DESC, created_at DESC 
                 LIMIT ?1"
            )?;
            
            let entries = stmt.query_map(params![limit as i64], |row| {
                Ok(ClipboardEntry {
                    id: row.get(0)?,
                    content: row.get(1)?,
                    source_app: row.get(2)?,
                    content_type: row.get(3)?,
                    created_at: row.get(4)?,
                    is_pinned: row.get(5)?,
                })
            })?.collect::<Result<Vec<_>, _>>()?;
            
            return Ok(entries);
        }
        
        // FTS5 search with highlighting
        let fts_query = format!("{}*", query_trimmed.replace("\"", ""));
        
        let mut stmt = conn.prepare(
            "SELECT h.id, h.content, h.source_app, h.content_type, h.created_at, h.is_pinned
             FROM clipboard_history h
             JOIN history_fts fts ON h.id = fts.rowid
             WHERE history_fts MATCH ?1
             ORDER BY h.is_pinned DESC, rank
             LIMIT ?2"
        )?;
        
        let entries = stmt.query_map(params![fts_query, limit as i64], |row| {
            Ok(ClipboardEntry {
                id: row.get(0)?,
                content: row.get(1)?,
                source_app: row.get(2)?,
                content_type: row.get(3)?,
                created_at: row.get(4)?,
                is_pinned: row.get(5)?,
            })
        })?.collect::<Result<Vec<_>, _>>()?;
        
        Ok(entries)
    }
    
    pub fn get_by_id(&self, id: i64) -> Result<Option<ClipboardEntry>, rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        
        let mut stmt = conn.prepare(
            "SELECT id, content, source_app, content_type, created_at, is_pinned 
             FROM clipboard_history WHERE id = ?1"
        )?;
        
        let mut rows = stmt.query(params![id])?;
        
        if let Some(row) = rows.next()? {
            Ok(Some(ClipboardEntry {
                id: row.get(0)?,
                content: row.get(1)?,
                source_app: row.get(2)?,
                content_type: row.get(3)?,
                created_at: row.get(4)?,
                is_pinned: row.get(5)?,
            }))
        } else {
            Ok(None)
        }
    }
    
    pub fn toggle_pin(&self, id: i64) -> Result<bool, rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.execute(
            "UPDATE clipboard_history SET is_pinned = NOT is_pinned WHERE id = ?1",
            params![id]
        )?;
        
        let is_pinned: bool = conn.query_row(
            "SELECT is_pinned FROM clipboard_history WHERE id = ?1",
            params![id],
            |row| row.get(0)
        )?;
        
        Ok(is_pinned)
    }
    
    pub fn delete(&self, id: i64) -> Result<(), rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.execute("DELETE FROM clipboard_history WHERE id = ?1", params![id])?;
        Ok(())
    }
    
    pub fn cleanup(&self, max_age_days: i64, max_entries: i64) -> Result<usize, rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        
        // Delete old entries (except pinned)
        let deleted_old = conn.execute(
            "DELETE FROM clipboard_history 
             WHERE is_pinned = 0 
             AND created_at < datetime('now', ?1)",
            params![format!("-{} days", max_age_days)]
        )?;
        
        // Keep only max_entries (except pinned)
        let deleted_excess = conn.execute(
            "DELETE FROM clipboard_history 
             WHERE is_pinned = 0 
             AND id NOT IN (
                SELECT id FROM clipboard_history 
                ORDER BY is_pinned DESC, created_at DESC 
                LIMIT ?1
             )",
            params![max_entries]
        )?;
        
        Ok(deleted_old + deleted_excess)
    }
    
    pub fn get_last_content(&self) -> Result<Option<String>, rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        
        let result: Result<String, _> = conn.query_row(
            "SELECT content FROM clipboard_history ORDER BY created_at DESC LIMIT 1",
            [],
            |row| row.get(0)
        );
        
        match result {
            Ok(content) => Ok(Some(content)),
            Err(rusqlite::Error::QueryReturnedNoRows) => Ok(None),
            Err(e) => Err(e),
        }
    }
}
