use arboard::{Clipboard, ImageData};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::Duration;
use std::borrow::Cow;

/// Result of a clipboard read operation
#[derive(Debug, Clone)]
pub enum ClipboardContent {
    Text(String),
    Image { data: Vec<u8>, width: usize, height: usize },
}

#[derive(Debug)]
pub struct ClipboardListener {
    running: Arc<AtomicBool>,
}

impl ClipboardListener {
    pub fn new() -> Self {
        Self {
            running: Arc::new(AtomicBool::new(false)),
        }
    }
    
    pub fn start<F>(&self, on_change: F)
    where
        F: Fn(ClipboardContent, Option<String>) + Send + 'static,
    {
        self.running.store(true, Ordering::SeqCst);
        let running = self.running.clone();
        
        thread::spawn(move || {
            let mut last_text: Option<String> = None;
            let mut last_image_hash: Option<u64> = None;
            
            while running.load(Ordering::SeqCst) {
                if let Ok(mut clipboard) = Clipboard::new() {
                    // Try to read text first
                    if let Ok(text) = clipboard.get_text() {
                        let text_trimmed = text.trim();
                        if !text_trimmed.is_empty() {
                            let should_update = match &last_text {
                                Some(last) => last != text_trimmed,
                                None => true,
                            };
                            
                            if should_update {
                                last_text = Some(text_trimmed.to_string());
                                last_image_hash = None; // Reset image hash when text changes
                                let source_app = get_foreground_app();
                                on_change(ClipboardContent::Text(text_trimmed.to_string()), source_app);
                            }
                        }
                    }
                    // Try to read image if no text update
                    else if let Ok(image) = clipboard.get_image() {
                        let hash = simple_hash(&image.bytes);
                        let should_update = match last_image_hash {
                            Some(last_hash) => last_hash != hash,
                            None => true,
                        };
                        
                        if should_update {
                            last_image_hash = Some(hash);
                            last_text = None; // Reset text when image changes
                            let source_app = get_foreground_app();
                            on_change(
                                ClipboardContent::Image {
                                    data: image.bytes.to_vec(),
                                    width: image.width,
                                    height: image.height,
                                },
                                source_app,
                            );
                        }
                    }
                }
                
                thread::sleep(Duration::from_millis(300));
            }
        });
    }
    
    pub fn stop(&self) {
        self.running.store(false, Ordering::SeqCst);
    }
}

/// Simple hash for image deduplication
fn simple_hash(data: &[u8]) -> u64 {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    let mut hasher = DefaultHasher::new();
    // Hash first 1KB + length for performance
    data.len().hash(&mut hasher);
    data.iter().take(1024).for_each(|b| b.hash(&mut hasher));
    hasher.finish()
}

pub fn set_clipboard_text(content: &str) -> Result<(), String> {
    let mut clipboard = Clipboard::new().map_err(|e| format!("Failed to access clipboard: {}", e))?;
    clipboard.set_text(content).map_err(|e| format!("Failed to set clipboard: {}", e))
}

pub fn set_clipboard_image(data: &[u8], width: usize, height: usize) -> Result<(), String> {
    let mut clipboard = Clipboard::new().map_err(|e| format!("Failed to access clipboard: {}", e))?;
    let image = ImageData {
        bytes: Cow::Borrowed(data),
        width,
        height,
    };
    clipboard.set_image(image).map_err(|e| format!("Failed to set clipboard image: {}", e))
}

// ============================================================================
// Platform-specific foreground app detection
// ============================================================================

#[cfg(target_os = "windows")]
fn get_foreground_app() -> Option<String> {
    use windows::{
        Win32::Foundation::*,
        Win32::UI::WindowsAndMessaging::*,
        Win32::System::Threading::*,
        core::PWSTR,
    };
    
    unsafe {
        let hwnd = GetForegroundWindow();
        if hwnd.0.is_null() {
            return None;
        }
        
        let mut process_id: u32 = 0;
        GetWindowThreadProcessId(hwnd, Some(&mut process_id));
        
        if process_id == 0 {
            return None;
        }
        
        let handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, process_id);
        if let Ok(handle) = handle {
            let mut buffer = [0u16; 260];
            let mut size = buffer.len() as u32;
            
            if QueryFullProcessImageNameW(handle, PROCESS_NAME_WIN32, PWSTR(buffer.as_mut_ptr()), &mut size).is_ok() {
                let _ = CloseHandle(handle);
                let path = String::from_utf16_lossy(&buffer[..size as usize]);
                return path.split('\\').last().map(|s| s.to_string());
            }
            let _ = CloseHandle(handle);
        }
        
        None
    }
}

#[cfg(target_os = "macos")]
fn get_foreground_app() -> Option<String> {
    use std::process::Command;
    
    // Use osascript to get the frontmost application name
    let output = Command::new("osascript")
        .args(["-e", "tell application \"System Events\" to get name of first process whose frontmost is true"])
        .output()
        .ok()?;
    
    if output.status.success() {
        let name = String::from_utf8_lossy(&output.stdout).trim().to_string();
        if !name.is_empty() {
            return Some(name);
        }
    }
    None
}

#[cfg(target_os = "linux")]
fn get_foreground_app() -> Option<String> {
    use std::process::Command;
    
    // Try xdotool first (X11)
    if let Ok(output) = Command::new("xdotool")
        .args(["getactivewindow", "getwindowname"])
        .output()
    {
        if output.status.success() {
            let name = String::from_utf8_lossy(&output.stdout).trim().to_string();
            if !name.is_empty() {
                return Some(name);
            }
        }
    }
    
    // Fallback: try to get from /proc for Wayland
    if let Ok(output) = Command::new("sh")
        .args(["-c", "cat /proc/$(xdotool getactivewindow getwindowpid)/comm 2>/dev/null"])
        .output()
    {
        if output.status.success() {
            let name = String::from_utf8_lossy(&output.stdout).trim().to_string();
            if !name.is_empty() {
                return Some(name);
            }
        }
    }
    
    None
}

#[cfg(not(any(target_os = "windows", target_os = "macos", target_os = "linux")))]
fn get_foreground_app() -> Option<String> {
    None
}
