mod clipboard;
mod database;

use clipboard::{ClipboardListener, set_clipboard_content};
use database::{ClipboardEntry, Database};
use once_cell::sync::OnceCell;
use std::sync::Arc;
use std::thread;
use std::time::Duration;
use tauri::{
    AppHandle, Manager, Runtime, PhysicalPosition,
    menu::{MenuBuilder, MenuItemBuilder},
    tray::{TrayIconBuilder, TrayIconEvent, MouseButton, MouseButtonState},
    WindowEvent,
};
use tauri_plugin_global_shortcut::{Code, GlobalShortcutExt, Modifiers, Shortcut, ShortcutState};

static DB: OnceCell<Arc<Database>> = OnceCell::new();
static LISTENER: OnceCell<ClipboardListener> = OnceCell::new();

fn get_db() -> &'static Arc<Database> {
    DB.get().expect("Database not initialized")
}

#[tauri::command]
fn search_history(query: String) -> Result<Vec<ClipboardEntry>, String> {
    get_db()
        .search(&query, 50)
        .map_err(|e| e.to_string())
}

#[tauri::command]
fn get_entry(id: i64) -> Result<Option<ClipboardEntry>, String> {
    get_db()
        .get_by_id(id)
        .map_err(|e| e.to_string())
}

#[tauri::command]
fn paste_entry(id: i64) -> Result<(), String> {
    let entry = get_db()
        .get_by_id(id)
        .map_err(|e| e.to_string())?
        .ok_or("Entry not found")?;
    
    set_clipboard_content(&entry.content)?;
    
    thread::sleep(Duration::from_millis(50));
    simulate_paste();
    
    Ok(())
}

#[tauri::command]
fn copy_entry(id: i64) -> Result<(), String> {
    let entry = get_db()
        .get_by_id(id)
        .map_err(|e| e.to_string())?
        .ok_or("Entry not found")?;
    
    set_clipboard_content(&entry.content)
}

#[tauri::command]
fn toggle_pin(id: i64) -> Result<bool, String> {
    get_db()
        .toggle_pin(id)
        .map_err(|e| e.to_string())
}

#[tauri::command]
fn delete_entry(id: i64) -> Result<(), String> {
    get_db()
        .delete(id)
        .map_err(|e| e.to_string())
}

#[tauri::command]
fn hide_window(app: AppHandle) -> Result<(), String> {
    if let Some(window) = app.get_webview_window("main") {
        window.hide().map_err(|e| e.to_string())?;
    }
    Ok(())
}

fn simulate_paste() {
    use enigo::{Enigo, Key, Keyboard, Settings};
    
    let mut enigo = Enigo::new(&Settings::default()).unwrap();
    thread::sleep(Duration::from_millis(100));
    
    let _ = enigo.key(Key::Control, enigo::Direction::Press);
    let _ = enigo.key(Key::Unicode('v'), enigo::Direction::Click);
    let _ = enigo.key(Key::Control, enigo::Direction::Release);
}

#[cfg(windows)]
fn get_cursor_and_screen_info() -> Option<(i32, i32, i32, i32)> {
    use windows::Win32::UI::WindowsAndMessaging::{GetCursorPos, GetSystemMetrics, SM_CXSCREEN, SM_CYSCREEN};
    use windows::Win32::Foundation::POINT;
    
    unsafe {
        let mut point = POINT { x: 0, y: 0 };
        if GetCursorPos(&mut point).is_ok() {
            let screen_w = GetSystemMetrics(SM_CXSCREEN);
            let screen_h = GetSystemMetrics(SM_CYSCREEN);
            Some((point.x, point.y, screen_w, screen_h))
        } else {
            None
        }
    }
}

#[cfg(not(windows))]
fn get_cursor_and_screen_info() -> Option<(i32, i32, i32, i32)> {
    None
}

fn toggle_window<R: Runtime>(app: &AppHandle<R>) {
    if let Some(window) = app.get_webview_window("main") {
        if window.is_visible().unwrap_or(false) {
            let _ = window.hide();
        } else {
            let win_width = 340;
            let win_height = 420;
            
            if let Some((cursor_x, cursor_y, screen_w, screen_h)) = get_cursor_and_screen_info() {
                let mut x = cursor_x - win_width / 2;
                let mut y = cursor_y + 10;
                
                if x < 10 { x = 10; }
                if x + win_width > screen_w - 10 { x = screen_w - win_width - 10; }
                if y + win_height > screen_h - 50 { y = cursor_y - win_height - 10; }
                if y < 10 { y = 10; }
                
                let _ = window.set_position(PhysicalPosition::new(x, y));
            }
            let _ = window.show();
            let _ = window.set_focus();
        }
    }
}

fn setup_tray<R: Runtime>(app: &AppHandle<R>) -> Result<(), Box<dyn std::error::Error>> {
    let quit = MenuItemBuilder::with_id("quit", "Quit ClipStream").build(app)?;
    let show = MenuItemBuilder::with_id("show", "Open (Ctrl+Shift+V)").build(app)?;
    
    let menu = MenuBuilder::new(app)
        .item(&show)
        .separator()
        .item(&quit)
        .build()?;
    
    let _ = TrayIconBuilder::with_id("main-tray")
        .icon(app.default_window_icon().unwrap().clone())
        .menu(&menu)
        .tooltip("ClipStream - Clipboard Manager")
        .on_menu_event(|app, event| {
            match event.id().as_ref() {
                "quit" => app.exit(0),
                "show" => toggle_window(app),
                _ => {}
            }
        })
        .on_tray_icon_event(|tray, event| {
            if let TrayIconEvent::Click { button: MouseButton::Left, button_state: MouseButtonState::Up, .. } = event {
                toggle_window(tray.app_handle());
            }
        })
        .build(app)?;
    
    Ok(())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let db = Arc::new(Database::new().expect("Failed to initialize database"));
    DB.set(db.clone()).expect("Failed to set database");
    
    if let Err(e) = db.cleanup(7, 500) {
        eprintln!("Cleanup error: {}", e);
    }
    
    let listener = ClipboardListener::new();
    let db_clone = db.clone();
    listener.start(move |content, source_app| {
        if let Err(e) = db_clone.insert(&content, source_app.as_deref()) {
            eprintln!("Failed to save clipboard entry: {}", e);
        }
    });
    LISTENER.set(listener).expect("Failed to set listener");
    
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(
            tauri_plugin_global_shortcut::Builder::new()
                .with_handler(|app, _shortcut, event| {
                    if event.state() == ShortcutState::Pressed {
                        toggle_window(app);
                    }
                })
                .build(),
        )
        .invoke_handler(tauri::generate_handler![
            search_history,
            get_entry,
            paste_entry,
            copy_entry,
            toggle_pin,
            delete_entry,
            hide_window,
        ])
        .setup(|app| {
            setup_tray(app.handle())?;
            
            let shortcut = Shortcut::new(Some(Modifiers::CONTROL | Modifiers::SHIFT), Code::KeyV);
            app.global_shortcut().register(shortcut)?;
            
            let window = app.get_webview_window("main").unwrap();
            let window_clone = window.clone();
            window.on_window_event(move |event| {
                if let WindowEvent::Focused(false) = event {
                    let _ = window_clone.hide();
                }
            });
            
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
