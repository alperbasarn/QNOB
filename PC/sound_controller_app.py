import tkinter as tk
from tkinter import scrolledtext, ttk
import time
import re

# Import your other classes
from media_controller import MediaController
from mqtt_handler import MQTTHandler
from windows_audio_controller import WindowsAudioController

# Define a unique sender ID for this application
SENDER_ID = "PC"

class SoundControllerApp:
    """Main application class"""
    
    def __init__(self, root):
        self.root = root
        self.root.title("ESP32 Sound Controller")
        self.root.geometry("800x600")
        self.root.minsize(600, 400)
        
        # Initialize UI elements to None
        self.volume_label = None
        self.all_messages = None
        self.sent_messages = None
        self.received_messages = None
        self.topic_entry = None
        self.message_entry = None
        self.connect_button = None
        self.disconnect_button = None
        self.send_button = None
        self.status_label = None
        self.mqtt_host_entry = None
        self.mqtt_port_entry = None
        self.mqtt_username_entry = None
        self.mqtt_password_entry = None
        self.volume_slider = None
        self.play_button = None
        self.pause_button = None
        self.status_indicator = None
        self.player_indicator = None
        
        # Flag to track source of volume changes
        self.external_volume_change = False
        
        # Track last local volume to avoid feedback
        self.last_local_volume = None
        
        # Client ID to identify this application
        self.client_id = SENDER_ID
        
        # Add a flag to track media playing state
        self.is_playing = False
        
        # Initialize the MQTT handler with message callback
        self.mqtt_handler = MQTTHandler(self.handle_message)
        
        # Connect the MQTT handler to this app for state access
        self.mqtt_handler.set_app(self)
        
        # Initialize audio controller with volume callback
        self.audio_controller = WindowsAudioController(self.handle_volume_change)
        
        # Initialize media controller for controlling playback
        self.media_controller = MediaController()
        
        # Build UI
        self.build_ui()
        
        # Start volume monitoring after UI is built
        self.audio_controller.start_monitoring()
        
        # Initialize last local volume
        self.last_local_volume = self.audio_controller.get_volume_percent()
        
        # Start periodic media state checks
        self.schedule_media_state_check()
    
    def build_ui(self):
        """Build the user interface"""
        # Create a frame for MQTT connection controls
        connection_frame = ttk.LabelFrame(self.root, text="MQTT Connection")
        connection_frame.pack(fill=tk.X, padx=10, pady=5)
        
        # MQTT broker settings
        ttk.Label(connection_frame, text="Broker:").grid(row=0, column=0, padx=5, pady=5, sticky=tk.W)
        self.mqtt_host_entry = ttk.Entry(connection_frame, width=40)
        self.mqtt_host_entry.grid(row=0, column=1, padx=5, pady=5, sticky=tk.W)
        self.mqtt_host_entry.insert(0, "4b664591e7764720a1ae5483e793dfa0.s1.eu.hivemq.cloud")
        
        ttk.Label(connection_frame, text="Port:").grid(row=0, column=2, padx=5, pady=5, sticky=tk.W)
        self.mqtt_port_entry = ttk.Entry(connection_frame, width=6)
        self.mqtt_port_entry.grid(row=0, column=3, padx=5, pady=5, sticky=tk.W)
        self.mqtt_port_entry.insert(0, "8883")
        
        ttk.Label(connection_frame, text="Username:").grid(row=1, column=0, padx=5, pady=5, sticky=tk.W)
        self.mqtt_username_entry = ttk.Entry(connection_frame, width=20)
        self.mqtt_username_entry.grid(row=1, column=1, padx=5, pady=5, sticky=tk.W)
        self.mqtt_username_entry.insert(0, "test123")
        
        ttk.Label(connection_frame, text="Password:").grid(row=1, column=2, padx=5, pady=5, sticky=tk.W)
        self.mqtt_password_entry = ttk.Entry(connection_frame, width=20, show="*")
        self.mqtt_password_entry.grid(row=1, column=3, padx=5, pady=5, sticky=tk.W)
        self.mqtt_password_entry.insert(0, "Test1234")
        
        # Connect/Disconnect buttons
        self.connect_button = ttk.Button(connection_frame, text="Connect", command=self.connect_mqtt)
        self.connect_button.grid(row=2, column=1, padx=5, pady=5, sticky=tk.W)
        
        self.disconnect_button = ttk.Button(connection_frame, text="Disconnect", command=self.disconnect_mqtt, state=tk.DISABLED)
        self.disconnect_button.grid(row=2, column=2, padx=5, pady=5, sticky=tk.W)
        
        # Status label
        self.status_label = ttk.Label(connection_frame, text="Not connected", foreground="red")
        self.status_label.grid(row=2, column=3, padx=5, pady=5, sticky=tk.W)
        
        # Create a frame for volume control
        volume_frame = ttk.LabelFrame(self.root, text="Volume Control")
        volume_frame.pack(fill=tk.X, padx=10, pady=5)
        
        # Get current volume before creating the slider
        current_volume = self.audio_controller.get_volume_percent()
        
        # Volume label - creating this BEFORE the slider and its command
        self.volume_label = ttk.Label(volume_frame, text=f"Volume: {current_volume}%")
        self.volume_label.pack(pady=5)
        
        # Volume slider
        self.volume_slider = ttk.Scale(volume_frame, from_=0, to=100, orient=tk.HORIZONTAL,
                                     command=self.on_volume_slider_change)
        self.volume_slider.pack(fill=tk.X, padx=10, pady=5)
        
        # Set initial volume value
        self.volume_slider.set(current_volume)
        
        # Create a frame for media control
        media_frame = ttk.LabelFrame(self.root, text="Media Control")
        media_frame.pack(fill=tk.X, padx=10, pady=5)
        
        # Media control buttons
        buttons_frame = ttk.Frame(media_frame)
        buttons_frame.pack(pady=10)
        
        # Play button (only used to show state, not for local control)
        self.play_button = ttk.Button(buttons_frame, text="▶ Play", state=tk.DISABLED)
        self.play_button.grid(row=0, column=0, padx=10)
        
        # Pause button (only used to show state, not for local control)
        self.pause_button = ttk.Button(buttons_frame, text="⏸ Pause", state=tk.DISABLED)
        self.pause_button.grid(row=0, column=1, padx=10)
        
        # Media controls that send MQTT messages
        ttk.Button(buttons_frame, text="◀◀ Rewind", command=lambda: self.send_media_command("rewind")).grid(row=0, column=2, padx=10)
        ttk.Button(buttons_frame, text="▶▶ Forward", command=lambda: self.send_media_command("forward")).grid(row=0, column=3, padx=10)
        ttk.Button(buttons_frame, text="Toggle Play/Pause", command=self.toggle_play_pause).grid(row=0, column=4, padx=10)
        
        # Status indicator
        status_frame = ttk.Frame(media_frame)
        status_frame.pack(pady=5, fill=tk.X)
        
        ttk.Label(status_frame, text="Current Status:").pack(side=tk.LEFT, padx=10)
        self.status_indicator = ttk.Label(status_frame, text="Paused", foreground="red")
        self.status_indicator.pack(side=tk.LEFT)
        
        # Player indicator
        ttk.Label(status_frame, text="    Detected Player:").pack(side=tk.LEFT, padx=10)
        self.player_indicator = ttk.Label(status_frame, text="None", foreground="gray")
        self.player_indicator.pack(side=tk.LEFT)
        
        # Create a frame for message logs
        log_frame = ttk.LabelFrame(self.root, text="Message Log")
        log_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        # Message log with tabs for sent/received
        self.message_tabs = ttk.Notebook(log_frame)
        self.message_tabs.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # All messages tab
        all_tab = ttk.Frame(self.message_tabs)
        self.message_tabs.add(all_tab, text="All Messages")
        
        self.all_messages = scrolledtext.ScrolledText(all_tab, wrap=tk.WORD)
        self.all_messages.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Sent messages tab
        sent_tab = ttk.Frame(self.message_tabs)
        self.message_tabs.add(sent_tab, text="Sent")
        
        self.sent_messages = scrolledtext.ScrolledText(sent_tab, wrap=tk.WORD)
        self.sent_messages.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Received messages tab
        received_tab = ttk.Frame(self.message_tabs)
        self.message_tabs.add(received_tab, text="Received")
        
        self.received_messages = scrolledtext.ScrolledText(received_tab, wrap=tk.WORD)
        self.received_messages.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Create a frame for manual message sending
        send_frame = ttk.LabelFrame(self.root, text="Send Message")
        send_frame.pack(fill=tk.X, padx=10, pady=5)
        
        # Topic field
        ttk.Label(send_frame, text="Topic:").grid(row=0, column=0, padx=5, pady=5, sticky=tk.W)
        self.topic_entry = ttk.Entry(send_frame, width=30)
        self.topic_entry.grid(row=0, column=1, padx=5, pady=5, sticky=tk.W+tk.E)
        self.topic_entry.insert(0, "esp32/sound/setpoint")
        
        # Message field
        ttk.Label(send_frame, text="Message:").grid(row=1, column=0, padx=5, pady=5, sticky=tk.W)
        self.message_entry = ttk.Entry(send_frame, width=30)
        self.message_entry.grid(row=1, column=1, padx=5, pady=5, sticky=tk.W+tk.E)
        
        # Send button
        self.send_button = ttk.Button(send_frame, text="Send", command=self.send_message, state=tk.DISABLED)
        self.send_button.grid(row=1, column=2, padx=5, pady=5, sticky=tk.W)
        
        # Configure the grid to expand
        send_frame.columnconfigure(1, weight=1)
        
        # Add an initial message to the log
        self.handle_message(f"Application started with ID: {self.client_id}", "status")
        
        # Check initial media state
        self.update_media_state()
    
    def schedule_media_state_check(self):
        """Schedule periodic checks of media playback state"""
        self.update_media_state()
        # Schedule next check in 2 seconds
        self.root.after(2000, self.schedule_media_state_check)
    
    def update_media_state(self):
        """Update UI and MQTT state based on current media playback state"""
        # Check if media is playing
        is_playing = self.media_controller.get_playing_state()
        
        # Update player indicator
        current_player = self.media_controller.get_current_player()
        if current_player:
            # Truncate if too long
            if len(current_player) > 30:
                current_player = current_player[:27] + "..."
            self.player_indicator.config(text=current_player, foreground="blue")
        else:
            self.player_indicator.config(text="None", foreground="gray")
        
        # If state has changed, update UI and send MQTT update
        if is_playing != self.is_playing:
            self.is_playing = is_playing
            self.update_playing_ui()
            
            # Send state update to ESP32 if connected
            if self.mqtt_handler and self.mqtt_handler.is_connected():
                state = "playing" if is_playing else "paused"
                self.mqtt_handler.publish("esp32/sound/response", state)
                self.handle_message(f"Media state changed to {state}, notifying ESP32", "status")
    
    def connect_mqtt(self):
        """Connect to MQTT broker"""
        # Only proceed if mqtt_handler is initialized
        if not hasattr(self, 'mqtt_handler') or self.mqtt_handler is None:
            self.handle_message("MQTT handler not initialized", "error")
            return
            
        host = self.mqtt_host_entry.get()
        port = int(self.mqtt_port_entry.get())
        username = self.mqtt_username_entry.get()
        password = self.mqtt_password_entry.get()
        
        # Update UI to indicate connection attempt
        self.status_label.config(text="Connecting...", foreground="orange")
        self.root.update_idletasks()  # Force UI update
        
        self.mqtt_handler.set_broker(host, port, username, password)
        success = self.mqtt_handler.connect()
        
        if success:
            self.connect_button.config(state=tk.DISABLED)
            self.disconnect_button.config(state=tk.NORMAL)
            self.send_button.config(state=tk.NORMAL)
            # Status label will be updated by on_connect callback
        else:
            # If connect() returned False, update UI
            self.status_label.config(text="Connection failed", foreground="red")
    
    def disconnect_mqtt(self):
        """Disconnect from MQTT broker"""
        if not hasattr(self, 'mqtt_handler') or self.mqtt_handler is None:
            return
            
        self.status_label.config(text="Disconnecting...", foreground="orange")
        self.root.update_idletasks()  # Force UI update
        
        self.mqtt_handler.disconnect()
        
        # Update UI immediately (don't wait for callback which might not happen)
        self.connect_button.config(state=tk.NORMAL)
        self.disconnect_button.config(state=tk.DISABLED)
        self.send_button.config(state=tk.DISABLED)
        self.status_label.config(text="Disconnected", foreground="red")
    
    def handle_message(self, message, message_type):
        """Handle MQTT message and update UI"""
        # Check if UI components are initialized
        if not hasattr(self, 'all_messages') or self.all_messages is None:
            print(f"UI not initialized yet: {message}")
            return
            
        timestamp = time.strftime("%H:%M:%S")
        formatted_message = f"[{timestamp}] {message}\n"
        
        # Update all messages log
        self.all_messages.config(state=tk.NORMAL)
        
        if message_type == "sent":
            self.all_messages.insert(tk.END, formatted_message)
            tag_start = self.all_messages.index(f"end - {len(formatted_message) + 1}c")
            tag_end = self.all_messages.index("end-1c")
            self.all_messages.tag_add("sent", tag_start, tag_end)
            self.all_messages.tag_config("sent", foreground="blue")
            
            # Also update sent messages tab
            if self.sent_messages:
                self.sent_messages.config(state=tk.NORMAL)
                self.sent_messages.insert(tk.END, formatted_message)
                self.sent_messages.see(tk.END)
                self.sent_messages.config(state=tk.DISABLED)
            
            # Update status label on connection
            if "Connected to MQTT broker" in message and self.status_label:
                self.status_label.config(text="Connected", foreground="green")
                self.connect_button.config(state=tk.DISABLED)
                self.disconnect_button.config(state=tk.NORMAL)
                self.send_button.config(state=tk.NORMAL)
                
                # Initialize last local volume on connection
                self.last_local_volume = self.audio_controller.get_volume_percent()
                # Also set MQTT handler's last sent setpoint
                self.mqtt_handler.last_sent_setpoint = self.last_local_volume
            
        elif message_type == "received":
            self.all_messages.insert(tk.END, formatted_message)
            tag_start = self.all_messages.index(f"end - {len(formatted_message) + 1}c")
            tag_end = self.all_messages.index("end-1c")
            self.all_messages.tag_add("received", tag_start, tag_end)
            self.all_messages.tag_config("received", foreground="green")
            
            # Also update received messages tab
            if self.received_messages:
                self.received_messages.config(state=tk.NORMAL)
                self.received_messages.insert(tk.END, formatted_message)
                self.received_messages.see(tk.END)
                self.received_messages.config(state=tk.DISABLED)
            
            # Check if message is from self (contains our client ID) and ignore if it is
            if self.client_id in message and f"sender={self.client_id}" in message:
                self.handle_message(f"Ignoring message from self: {message}", "status")
                return
                
            # Process setpoint messages - note that control messages are handled in MQTTHandler class
            if "esp32/sound/setpoint" in message:
                # Check sender to avoid processing our own messages
                if f"sender={self.client_id}" in message:
                    self.handle_message("Ignoring setpoint from self", "status")
                    return
                    
                # Extract volume value and set Windows volume
                try:
                    # Try to extract the number from the message
                    if "setpoint:" in message:
                        # Extract just the numeric part after setpoint:
                        parts = message.split(",")
                        for part in parts:
                            if "setpoint:" in part:
                                volume_str = part.split(":")[-1].strip()
                                volume = int(volume_str)
                                break
                        else:
                            # If no setpoint found, try the old format
                            volume_str = message.split(":")[-1].strip()
                            volume = int(volume_str)
                    else:
                        self.handle_message(f"Unexpected message format: {message}", "status")
                        return
                    
                    # Check if we already have this volume value
                    current_volume = self.audio_controller.get_volume_percent()
                    if volume == current_volume:
                        self.handle_message(f"Received same volume ({volume}), ignoring", "status")
                        return
                    
                    # Set flag to indicate that the change is coming from external source
                    self.external_volume_change = True
                    
                    # First update tracking variables
                    self.last_local_volume = volume
                    self.mqtt_handler.last_sent_setpoint = volume
                    
                    # Then update volume and UI
                    self.audio_controller.set_volume_percent(volume)
                    if self.volume_slider:
                        self.volume_slider.set(volume)
                    if self.volume_label:
                        self.volume_label.config(text=f"Volume: {volume}%")
                    
                    self.handle_message(f"Updated volume from ESP32: {volume}", "status")
                    
                    # Wait a moment to make sure the volume change propagates
                    self.root.after(50, self.reset_external_flag)
                except (ValueError, IndexError) as e:
                    self.handle_message(f"Error parsing message: {str(e)}", "error")
                    
        elif message_type == "status":
            self.all_messages.insert(tk.END, formatted_message)
            tag_start = self.all_messages.index(f"end - {len(formatted_message) + 1}c")
            tag_end = self.all_messages.index("end-1c")
            self.all_messages.tag_add("status", tag_start, tag_end)
            self.all_messages.tag_config("status", foreground="purple")
            
            # Update status for disconnect messages
            if "Disconnected" in message and self.status_label:
                self.status_label.config(text="Disconnected", foreground="red")
                if self.connect_button:
                    self.connect_button.config(state=tk.NORMAL)
                if self.disconnect_button:
                    self.disconnect_button.config(state=tk.DISABLED)
                if self.send_button:
                    self.send_button.config(state=tk.DISABLED)
                    
        elif message_type == "error":
            self.all_messages.insert(tk.END, formatted_message)
            tag_start = self.all_messages.index(f"end - {len(formatted_message) + 1}c")
            tag_end = self.all_messages.index("end-1c")
            self.all_messages.tag_add("error", tag_start, tag_end)
            self.all_messages.tag_config("error", foreground="red")
            
            # Update status label on error
            if self.status_label:
                self.status_label.config(text="Error", foreground="red")
        
        elif message_type == "warning":
            self.all_messages.insert(tk.END, formatted_message)
            tag_start = self.all_messages.index(f"end - {len(formatted_message) + 1}c")
            tag_end = self.all_messages.index("end-1c")
            self.all_messages.tag_add("warning", tag_start, tag_end)
            self.all_messages.tag_config("warning", foreground="orange")
            
        else:  # Default case
            self.all_messages.insert(tk.END, formatted_message)
        
        # Always ensure we scroll to see new messages
        self.all_messages.see(tk.END)
        self.all_messages.config(state=tk.DISABLED)
    
    def reset_external_flag(self):
        """Reset the external volume change flag after a delay"""
        self.external_volume_change = False
        self.handle_message("External volume change flag reset", "status")
    
    def handle_volume_change(self, volume):
        """Handle volume change from audio system and update UI"""
        # Update UI if components are initialized
        if self.volume_slider:
            self.volume_slider.set(volume)
        if self.volume_label:
            self.volume_label.config(text=f"Volume: {volume}%")
        
        # Only send to ESP32 if this change originated locally (not from ESP32 message)
        # and if the volume is different from last sent value
        if not self.external_volume_change and hasattr(self, 'mqtt_handler') and self.mqtt_handler and self.mqtt_handler.is_connected():
            if volume != self.last_local_volume:
                # Log debug info
                self.handle_message(f"Local volume changed from {self.last_local_volume} to {volume}", "status")
                
                # Create a small delay before sending to avoid race conditions
                # Store the current volume to make sure we send the right value after the delay
                current_volume = volume
                
                def delayed_send():
                    # Check if volume is still the same (hasn't changed during delay)
                    if self.audio_controller.get_volume_percent() == current_volume:
                        # Send with client ID
                        self.mqtt_handler.publish("esp32/sound/setpoint", f"setpoint:{current_volume}")
                        self.mqtt_handler.last_sent_setpoint = current_volume
                        self.handle_message(f"Local volume {current_volume}% sent to ESP32", "status")
                
                # Update the last local volume immediately to avoid duplicate sending
                self.last_local_volume = volume
                
                # Send after a short delay to allow Windows audio system to stabilize
                self.root.after(100, delayed_send)
    
    def on_volume_slider_change(self, value):
        """Handle volume slider change"""
        # Only process if volume_label is initialized
        if self.volume_label:
            volume = int(float(value))
            self.volume_label.config(text=f"Volume: {volume}%")
            
            # Only update if value actually changed
            current_volume = self.audio_controller.get_volume_percent()
            if volume != current_volume:
                # Set Windows volume - this will trigger handle_volume_change via the audio controller
                self.audio_controller.set_volume_percent(volume)
    
    def send_message(self):
        """Send manual message from UI"""
        if not all([self.topic_entry, self.message_entry, self.mqtt_handler]):
            return
            
        topic = self.topic_entry.get()
        message = self.message_entry.get()
        
        if topic and message and self.mqtt_handler.is_connected():
            # Prepend client ID if not already included
            if not message.startswith(f"sender={self.client_id}"):
                full_message = f"sender={self.client_id},{message}"
            else:
                full_message = message
                
            self.mqtt_handler.publish(topic, full_message)
            self.message_entry.delete(0, tk.END)
        elif not self.mqtt_handler.is_connected():
            self.handle_message("Not connected to MQTT broker. Cannot send message.", "error")
    
    def toggle_play_pause(self):
        """Toggle between play and pause states"""
        # First check if there's actual media to control
        if not self.media_controller.get_current_player():
            self.handle_message("No media player detected. Cannot control playback.", "warning")
            return
            
        if not self.mqtt_handler or not self.mqtt_handler.is_connected():
            self.handle_message("Not connected to MQTT broker. Cannot control playback.", "error")
            return
        
        # Actually control media
        is_playing = self.media_controller.play_pause()
        
        # Update our internal state
        self.is_playing = is_playing
        self.update_playing_ui()
        
        # Send command to ESP32 to reflect state change
        state = "playing" if is_playing else "paused"
        self.mqtt_handler.publish("esp32/sound/response", state)
        self.handle_message(f"Media toggled to {state}, notifying ESP32", "status")
    
    def send_media_command(self, command):
        """Send media control command"""
        # First check if there's actual media to control
        if not self.media_controller.get_current_player():
            self.handle_message(f"No media player detected. Cannot send {command} command.", "warning")
            return
            
        if not self.mqtt_handler or not self.mqtt_handler.is_connected():
            self.handle_message(f"Not connected to MQTT broker. Cannot send {command} command.", "error")
            return
        
        # Execute command locally
        if command == "forward":
            self.media_controller.next_track()
        elif command == "rewind":
            self.media_controller.prev_track()
            
        # Send command to ESP32
        self.mqtt_handler.publish("esp32/sound/control", f"sender={self.client_id},{command}")
        self.handle_message(f"Sent {command} command and notified ESP32", "status")
        
        # For forward/rewind, check if playing state changed as a result
        self.root.after(500, self.update_media_state)
    
    def set_playing_state(self, is_playing):
        """Update the playing state"""
        # Only update if state has changed
        if self.is_playing != is_playing:
            self.is_playing = is_playing
            self.update_playing_ui()
            
            # Log the state change
            state_str = "Playing" if is_playing else "Paused"
            self.handle_message(f"Media state changed to: {state_str}", "status")
    
    def update_playing_ui(self):
        """Update UI to reflect current playing state"""
        if self.is_playing:
            # Playing state
            if self.status_indicator:
                self.status_indicator.config(text="Playing", foreground="green")
            # Visual indication of which button is "active"
            if self.play_button and self.pause_button:
                self.play_button.config(state=tk.DISABLED, default=tk.ACTIVE)
                self.pause_button.config(state=tk.DISABLED, default=tk.NORMAL)
        else:
            # Paused state
            if self.status_indicator:
                self.status_indicator.config(text="Paused", foreground="red")
            # Visual indication of which button is "active"
            if self.play_button and self.pause_button:
                self.play_button.config(state=tk.DISABLED, default=tk.NORMAL)
                self.pause_button.config(state=tk.DISABLED, default=tk.ACTIVE)