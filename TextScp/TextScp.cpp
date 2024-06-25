#include <windows.h>
#include <WinUser.h>
#include <conio.h>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>

// Global variables
int num_lines_to_scrape;
int line_spacing;
int num_scrolls;
int total_scroll_pixels;
POINT initial_mouse_position;
std::vector<std::wstring> copied_lines;
std::atomic<bool> stop_flag(false);
std::mutex mtx;

// Function declarations
void scroll_down(int pixels);
int click_and_copy_text(int x, int y);
std::wstring get_clipboard_text();
void user_input_thread();
void get_mouse_position();
void calculate_scroll_pixels();

void scroll_down(int total_pixels) {
	int scroll_step = 120; // Typically 120 units equals one line of scrolling
	int num_steps = total_pixels / scroll_step;
	for (int i = 0; i < num_steps; ++i) {
		mouse_event(MOUSEEVENTF_WHEEL, 0, 0, -scroll_step, 0);
		std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Delay between each step
	}
	int remaining_pixels = total_pixels % scroll_step;
	if (remaining_pixels != 0) {
		mouse_event(MOUSEEVENTF_WHEEL, 0, 0, -remaining_pixels, 0);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

std::wstring get_clipboard_text() {
	if (!OpenClipboard(nullptr)) {
		std::cerr << "Error: Unable to open clipboard.\n";
		return L"";
	}
	HANDLE hData = GetClipboardData(CF_UNICODETEXT);
	if (hData == nullptr) {
		std::cerr << "Error: Unable to get clipboard data.\n";
		CloseClipboard();
		return L"";
	}
	wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
	if (pszText == nullptr) {
		std::cerr << "Error: Unable to lock global memory.\n";
		CloseClipboard();
		return L"";
	}
	std::wstring text(pszText);
	GlobalUnlock(hData);
	CloseClipboard();
	return text;
}

int click_and_copy_text(int x, int y) {
	// Move cursor to the specified position
	SetCursorPos(x, y);

	// Simulate left mouse button down (single click) to focus the input field
	mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
	mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);

	std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Small delay to ensure the click is registered

	// Press Ctrl + A to select all text
	keybd_event(VK_CONTROL, 0, 0, 0); // Ctrl key down
	keybd_event(0x41, 0, 0, 0); // 'A' key down
	std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Hold for a short duration
	keybd_event(0x41, 0, KEYEVENTF_KEYUP, 0); // 'A' key up
	keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0); // Ctrl key up

	std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Small delay to ensure text selection is done

	// Press Ctrl + C to copy selected text
	keybd_event(VK_CONTROL, 0, 0, 0); // Ctrl key down
	keybd_event(0x43, 0, 0, 0); // 'C' key down
	std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Hold for a short duration
	keybd_event(0x43, 0, KEYEVENTF_KEYUP, 0); // 'C' key up
	keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0); // Ctrl key up

	std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Small delay to ensure the copy action is done

	// Error handling: check clipboard content
	std::wstring text = get_clipboard_text();
	if (text.empty()) {
		std::cerr << "Error: No text found in clipboard.\n";
		return 0; // Indicate error
	}

	return 1; // Indicate success (assuming text is copied)
}

void user_input_thread() {
	while (!stop_flag.load()) {
		if (_kbhit()) {
			int ch = _getch();
			if (ch == 27 || ch == '5') {
				stop_flag.store(true);
				break;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void get_mouse_position() {
	std::cout << "Move your mouse to the initial position and press Enter...\n";
	std::cin.get();
	if (!GetCursorPos(&initial_mouse_position)) {
		std::cerr << "Error: Unable to get cursor position.\n";
		exit(1);
	}
	std::cout << "Mouse position set to (" << initial_mouse_position.x << ", " << initial_mouse_position.y << ")\n";
}

void calculate_scroll_pixels() {
	total_scroll_pixels = line_spacing * num_lines_to_scrape - (line_spacing / 2);
	std::cout << "Scrolling currently is set to " << total_scroll_pixels << " pixels; would you like to adjust it to another number? (Enter 0 to keep current value): ";
	int user_input;
	std::cin >> user_input;
	if (user_input > 0) {
		total_scroll_pixels = user_input;
	}
}

int main() {
	std::wcout << L"Selecting the foreground window...\n";
	HWND selected_window = GetForegroundWindow();
	if (selected_window == nullptr) {
		std::cerr << "Error: No window in foreground.\n";
		return 1;
	}

	std::wcout << L"Enter number of lines to scrape: ";
	std::cin >> num_lines_to_scrape;

	std::wcout << L"Enter line spacing (pixels): ";
	std::cin >> line_spacing;

	std::wcout << L"Enter number of scrolls: ";
	std::cin >> num_scrolls;

	calculate_scroll_pixels();
	get_mouse_position();

	std::wcout << L"Scraping started...\n";
	std::thread input_thread(user_input_thread);

	for (int scroll_count = 0; scroll_count < num_scrolls; ++scroll_count) {
		for (int i = 0; i < num_lines_to_scrape; ++i) {
			if (stop_flag.load()) {
				break;
			}
			int current_y = initial_mouse_position.y + i * line_spacing;
			if (!click_and_copy_text(initial_mouse_position.x, current_y)) {
				std::cerr << "Error clicking and copying text.\n";
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Small delay to ensure text is copied
			std::wstring text = get_clipboard_text();
			if (!text.empty()) {
				std::lock_guard<std::mutex> lock(mtx);
				copied_lines.push_back(text);
				std::wcout << L"Copied line " << copied_lines.size() << L": " << text << L"\n";
			}
			else {
				std::wcout << L"No text found at line " << copied_lines.size() << L"\n";
				break;
			}
		}
		if (scroll_count < num_scrolls - 1) {
			// Move the mouse back to the initial position before scrolling
			SetCursorPos(initial_mouse_position.x, initial_mouse_position.y);
			scroll_down(total_scroll_pixels);
			std::this_thread::sleep_for(std::chrono::seconds(1)); // Small delay to ensure scroll is complete
		}
	}

	stop_flag.store(true);
	input_thread.join();

	if (stop_flag.load()) {
		std::wcout << L"Process stopped. Lines copied:\n";
		for (const auto& line : copied_lines) {
			std::wcout << line << L"\n";
		}
	}
	else {
		std::wcout << L"Copied " << copied_lines.size() << L" lines out of " << num_lines_to_scrape * num_scrolls << L".\n";
	}

	std::wstring final_text;
	for (const auto& line : copied_lines) {
		final_text += line + L" ";
	}
	if (!final_text.empty()) {
		final_text.pop_back(); // Remove the last space
	}

	// Clipboard operations
	if (!final_text.empty()) {
		std::wcout << L"Scraping finished.\n";
	}

	return 0;
}