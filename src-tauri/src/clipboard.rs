use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::Duration;

#[cfg(windows)]
use windows::{
    Win32::Foundation::*,
    Win32::UI::WindowsAndMessaging::*,
    Win32::System::Threading::*,
    core::PWSTR,
};

use clipboard_win::{formats, get_clipboard};

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
        F: Fn(String, Option<String>) + Send + 'static,
    {
        self.running.store(true, Ordering::SeqCst);
        let running = self.running.clone();
        
        thread::spawn(move || {
            let mut last_content: Option<String> = None;
            
            while running.load(Ordering::SeqCst) {
                if let Ok(content) = get_clipboard::<String, _>(formats::Unicode) {
                    let content_trimmed = content.trim();
                    if !content_trimmed.is_empty() {
                        let should_update = match &last_content {
                            Some(last) => last != content_trimmed,
                            None => true,
                        };
                        
                        if should_update {
                            last_content = Some(content_trimmed.to_string());
                            let source_app = Self::get_foreground_app();
                            on_change(content_trimmed.to_string(), source_app);
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
    
    #[cfg(windows)]
    fn get_foreground_app() -> Option<String> {
        unsafe {
            let hwnd = GetForegroundWindow();
            if hwnd.0 == std::ptr::null_mut() {
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
    
    #[cfg(not(windows))]
    fn get_foreground_app() -> Option<String> {
        None
    }
}

pub fn set_clipboard_content(content: &str) -> Result<(), String> {
    clipboard_win::set_clipboard(formats::Unicode, content)
        .map_err(|e| format!("Failed to set clipboard: {}", e))
}
