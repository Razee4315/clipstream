mod clipboard;
mod database;

use clipboard::{ClipboardContent, ClipboardListener, set_clipboard_text};
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
    
    set_clipboard_text(&entry.content)?;
    
    thread::sleep(Duration::from_millis(50));
    simulate_paste();
    
    Ok(())
}

#[tauri::command]
fn paste_formatted(id: i64, format: String) -> Result<(), String> {
    let entry = get_db()
        .get_by_id(id)
        .map_err(|e| e.to_string())?
        .ok_or("Entry not found")?;
    
    let formatted = match format.as_str() {
        "upper" => entry.content.to_uppercase(),
        "lower" => entry.content.to_lowercase(),
        "title" => to_title_case(&entry.content),
        "trim" => entry.content.trim().to_string(),
        "plain" | _ => entry.content.clone(),
    };
    
    set_clipboard_text(&formatted)?;
    
    thread::sleep(Duration::from_millis(50));
    simulate_paste();
    
    Ok(())
}

fn to_title_case(s: &str) -> String {
    s.split_whitespace()
        .map(|word| {
            let mut chars = word.chars();
            match chars.next() {
                None => String::new(),
                Some(first) => first.to_uppercase().chain(chars.flat_map(|c| c.to_lowercase())).collect(),
            }
        })
        .collect::<Vec<_>>()
        .join(" ")
}

#[tauri::command]
fn copy_entry(id: i64) -> Result<(), String> {
    let entry = get_db()
        .get_by_id(id)
        .map_err(|e| e.to_string())?
        .ok_or("Entry not found")?;
    
    set_clipboard_text(&entry.content)
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
fn update_entry(id: i64, content: String) -> Result<(), String> {
    get_db()
        .update_content(id, &content)
        .map_err(|e| e.to_string())
}

#[tauri::command]
fn hide_window(app: AppHandle) -> Result<(), String> {
    if let Some(window) = app.get_webview_window("main") {
        window.hide().map_err(|e| e.to_string())?;
    }
    Ok(())
}

// ============================================================================
// Ignored Apps Management
// ============================================================================

#[tauri::command]
fn get_ignored_apps() -> Result<Vec<String>, String> {
    get_db()
        .get_ignored_apps()
        .map_err(|e| e.to_string())
}

#[tauri::command]
fn add_ignored_app(app_name: String) -> Result<(), String> {
    get_db()
        .add_ignored_app(&app_name)
        .map_err(|e| e.to_string())
}

#[tauri::command]
fn remove_ignored_app(app_name: String) -> Result<(), String> {
    get_db()
        .remove_ignored_app(&app_name)
        .map_err(|e| e.to_string())
}

// ============================================================================
// Settings Management
// ============================================================================

#[tauri::command]
fn get_setting(key: String) -> Result<Option<String>, String> {
    get_db()
        .get_setting(&key)
        .map_err(|e| e.to_string())
}

#[tauri::command]
fn set_setting(key: String, value: String) -> Result<(), String> {
    get_db()
        .set_setting(&key, &value)
        .map_err(|e| e.to_string())
}

// ============================================================================
// Paste Simulation (cross-platform)
// ============================================================================

fn simulate_paste() {
    use enigo::{Enigo, Key, Keyboard, Settings};
    
    let mut enigo = match Enigo::new(&Settings::default()) {
        Ok(e) => e,
        Err(_) => return,
    };
    
    thread::sleep(Duration::from_millis(100));
    
    #[cfg(target_os = "macos")]
    {
        // macOS uses Command+V
        let _ = enigo.key(Key::Meta, enigo::Direction::Press);
        let _ = enigo.key(Key::Unicode('v'), enigo::Direction::Click);
        let _ = enigo.key(Key::Meta, enigo::Direction::Release);
    }
    
    #[cfg(not(target_os = "macos"))]
    {
        // Windows and Linux use Ctrl+V
        let _ = enigo.key(Key::Control, enigo::Direction::Press);
        let _ = enigo.key(Key::Unicode('v'), enigo::Direction::Click);
        let _ = enigo.key(Key::Control, enigo::Direction::Release);
    }
}

// ============================================================================
// Window Positioning (cross-platform)
// ============================================================================

#[cfg(target_os = "windows")]
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

#[cfg(target_os = "macos")]
fn get_cursor_and_screen_info() -> Option<(i32, i32, i32, i32)> {
    use std::process::Command;
    
    // Get cursor position using cliclick or AppleScript
    let output = Command::new("osascript")
        .args(["-e", "tell application \"System Events\" to get position of (first window whose frontmost is true) as list"])
        .output()
        .ok()?;
    
    // Fallback: use default screen center for now
    // In production, you'd use Core Graphics APIs
    Some((500, 300, 1920, 1080))
}

#[cfg(target_os = "linux")]
fn get_cursor_and_screen_info() -> Option<(i32, i32, i32, i32)> {
    use std::process::Command;
    
    // Try xdotool for X11
    if let Ok(output) = Command::new("xdotool").args(["getmouselocation"]).output() {
        if output.status.success() {
            let text = String::from_utf8_lossy(&output.stdout);
            // Parse "x:123 y:456 ..."
            let mut x = 500i32;
            let mut y = 300i32;
            for part in text.split_whitespace() {
                if let Some(val) = part.strip_prefix("x:") {
                    x = val.parse().unwrap_or(500);
                } else if let Some(val) = part.strip_prefix("y:") {
                    y = val.parse().unwrap_or(300);
                }
            }
            return Some((x, y, 1920, 1080));
        }
    }
    
    Some((500, 300, 1920, 1080))
}

#[cfg(not(any(target_os = "windows", target_os = "macos", target_os = "linux")))]
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
    
    // Get ignored apps for filtering
    let ignored_apps = db.get_ignored_apps().unwrap_or_default();
    
    let listener = ClipboardListener::new();
    let db_clone = db.clone();
    listener.start(move |content, source_app| {
        // Check if source app is ignored
        if let Some(ref app) = source_app {
            let app_lower = app.to_lowercase();
            if ignored_apps.iter().any(|ignored| app_lower.contains(&ignored.to_lowercase())) {
                return; // Skip ignored apps
            }
        }
        
        match content {
            ClipboardContent::Text(text) => {
                if let Err(e) = db_clone.insert(&text, source_app.as_deref(), None) {
                    eprintln!("Failed to save clipboard entry: {}", e);
                }
            }
            ClipboardContent::Image { data, width, height } => {
                // Encode image as base64 for storage
                if let Ok(png_data) = encode_rgba_to_png(&data, width, height) {
                    let base64_str = base64::Engine::encode(&base64::engine::general_purpose::STANDARD, &png_data);
                    let preview = format!("[Image {}x{}]", width, height);
                    if let Err(e) = db_clone.insert(&preview, source_app.as_deref(), Some(&base64_str)) {
                        eprintln!("Failed to save clipboard image: {}", e);
                    }
                }
            }
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
            paste_formatted,
            copy_entry,
            toggle_pin,
            delete_entry,
            update_entry,
            hide_window,
            get_ignored_apps,
            add_ignored_app,
            remove_ignored_app,
            get_setting,
            set_setting,
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

/// Encode RGBA bytes to PNG
fn encode_rgba_to_png(data: &[u8], width: usize, height: usize) -> Result<Vec<u8>, String> {
    use image::{ImageBuffer, Rgba};
    
    let img: ImageBuffer<Rgba<u8>, Vec<u8>> = ImageBuffer::from_raw(
        width as u32,
        height as u32,
        data.to_vec(),
    ).ok_or("Failed to create image buffer")?;
    
    let mut png_bytes = Vec::new();
    let mut cursor = std::io::Cursor::new(&mut png_bytes);
    img.write_to(&mut cursor, image::ImageFormat::Png)
        .map_err(|e| format!("Failed to encode PNG: {}", e))?;
    
    Ok(png_bytes)
}
