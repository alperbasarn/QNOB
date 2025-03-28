import tkinter as tk
from tkinter import ttk, scrolledtext
import serial.tools.list_ports
import serial
import threading
import time

class SerialSubTab:
    def __init__(self, notebook, app):
        self.app = app
        self.frame = ttk.Frame(notebook)
        self.serial_port = None
        self.reader_thread = None
        self.running = False
        
        # Create UI elements
        self.create_ui()
        
        # Initial port scan
        self.refresh_ports()
        
    def create_ui(self):
        # Top frame for connection controls
        controls_frame = ttk.Frame(self.frame)
        controls_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # Port selection
        ttk.Label(controls_frame, text="Port:").grid(row=0, column=0, padx=5, pady=5, sticky=tk.W)
        self.port_combo = ttk.Combobox(controls_frame, width=15)
        self.port_combo.grid(row=0, column=1, padx=5, pady=5, sticky=tk.W)
        
        # Baud rate
        ttk.Label(controls_frame, text="Baud:").grid(row=0, column=2, padx=5, pady=5, sticky=tk.W)
        self.baud_combo = ttk.Combobox(controls_frame, width=10, values=["9600", "115200"])
        self.baud_combo.grid(row=0, column=3, padx=5, pady=5, sticky=tk.W)
        self.baud_combo.current(0)  # Default to 9600
        
        # Refresh, Connect, Disconnect buttons
        self.refresh_btn = ttk.Button(controls_frame, text="Refresh", command=self.refresh_ports)
        self.refresh_btn.grid(row=0, column=4, padx=5, pady=5)
        
        self.connect_btn = ttk.Button(controls_frame, text="Connect", command=self.connect_serial)
        self.connect_btn.grid(row=0, column=5, padx=5, pady=5)
        
        self.disconnect_btn = ttk.Button(controls_frame, text="Disconnect", command=self.disconnect_serial, state=tk.DISABLED)
        self.disconnect_btn.grid(row=0, column=6, padx=5, pady=5)
        
        # Message display area - split into sent and received
        messages_frame = ttk.Frame(self.frame)
        messages_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Sent messages
        ttk.Label(messages_frame, text="Sent Messages:").pack(fill=tk.X, anchor=tk.W)
        self.sent_messages = scrolledtext.ScrolledText(messages_frame, height=10, wrap=tk.WORD)
        self.sent_messages.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        self.sent_messages.config(state=tk.DISABLED)
        
        # Received messages
        ttk.Label(messages_frame, text="Received Messages:").pack(fill=tk.X, anchor=tk.W)
        self.received_messages = scrolledtext.ScrolledText(messages_frame, height=10, wrap=tk.WORD)
        self.received_messages.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        self.received_messages.config(state=tk.DISABLED)
        
        # Manual command entry
        command_frame = ttk.Frame(self.frame)
        command_frame.pack(fill=tk.X, padx=5, pady=5)
        
        ttk.Label(command_frame, text="Command:").pack(side=tk.LEFT, padx=5)
        self.command_entry = ttk.Entry(command_frame, width=50)
        self.command_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        self.command_entry.bind("<Return>", self.send_command)
        
        self.send_btn = ttk.Button(command_frame, text="Send", command=lambda: self.send_command(None))
        self.send_btn.pack(side=tk.LEFT, padx=5)
        self.send_btn.config(state=tk.DISABLED)
    
    def refresh_ports(self):
        """Scan for available serial ports"""
        ports = serial.tools.list_ports.comports()
        port_names = [port.device for port in ports]
        self.port_combo['values'] = port_names
        
        if port_names:
            self.port_combo.current(0)
            self.app.handle_message(f"Found {len(port_names)} serial ports", "status")
        else:
            self.app.handle_message("No serial ports found", "status")
    
    def connect_serial(self):
        """Connect to the selected serial port"""
        if not self.port_combo.get():
            self.app.handle_message("No port selected", "error")
            return
            
        try:
            port = self.port_combo.get()
            baud = int(self.baud_combo.get())
            
            self.serial_port = serial.Serial(port, baud, timeout=0.1)
            
            # Update UI
            self.connect_btn.config(state=tk.DISABLED)
            self.disconnect_btn.config(state=tk.NORMAL)
            self.send_btn.config(state=tk.NORMAL)
            
            # Update app's connection status
            self.app.serial_connected = True
            self.app.connected_device = self.serial_port
            self.app.connection_type = "serial"
            self.app.update_connection_status()
            
            # Start reader thread
            self.running = True
            self.reader_thread = threading.Thread(target=self.read_serial, daemon=True)
            self.reader_thread.start()
            
            self.app.handle_message(f"Connected to {port} at {baud} baud", "serial_status")
        
        except Exception as e:
            self.app.handle_message(f"Serial connection error: {str(e)}", "error")
    
    def disconnect_serial(self):
        """Disconnect from the serial port"""
        if self.serial_port and self.serial_port.is_open:
            self.running = False
            if self.reader_thread:
                self.reader_thread.join(timeout=1.0)
            
            # Close the port
            self.serial_port.close()
            self.serial_port = None
            
            # Update UI
            self.connect_btn.config(state=tk.NORMAL)
            self.disconnect_btn.config(state=tk.DISABLED)
            self.send_btn.config(state=tk.DISABLED)
            
            # Update app's connection status
            self.app.serial_connected = False
            self.app.connected_device = None
            self.app.connection_type = None
            self.app.update_connection_status()
            
            self.app.handle_message("Serial disconnected", "serial_status")
    
    def read_serial(self):
        """Background thread to read from serial port"""
        while self.running and self.serial_port and self.serial_port.is_open:
            try:
                # Read data if available
                if self.serial_port.in_waiting:
                    data = self.serial_port.readline().decode('utf-8').strip()
                    if data:
                        # Process the received data
                        self.app.handle_message(data, "serial_received")
                
                # Short delay to prevent CPU overload
                time.sleep(0.05)
                
            except Exception as e:
                self.app.handle_message(f"Serial read error: {str(e)}", "error")
                # On error, break the loop and disconnect
                self.running = False
                self.app.root.after(100, self.disconnect_serial)
                break
    
    def send_command(self, event=None):
        """Send a command through the serial port"""
        if not self.serial_port or not self.serial_port.is_open:
            self.app.handle_message("Not connected to serial port", "error")
            return
            
        command = self.command_entry.get()
        if not command:
            return
            
        # Add newline if not already there
        if not command.endswith('\n'):
            command += '\n'
        
        try:
            self.serial_port.write(command.encode())
            self.app.handle_message(f"Sent: {command.strip()}", "serial_sent")
            # Clear the entry if sent successfully
            self.command_entry.delete(0, tk.END)
        except Exception as e:
            self.app.handle_message(f"Serial send error: {str(e)}", "error")
    
    def log_message(self, message, message_type="status"):
        """Log message to the appropriate text area"""
        timestamp = time.strftime("%H:%M:%S")
        formatted_message = f"[{timestamp}] {message}\n"
        
        if message_type in ["serial_sent", "sent"]:
            # Show in sent messages area
            self.sent_messages.config(state=tk.NORMAL)
            self.sent_messages.insert(tk.END, formatted_message)
            self.sent_messages.see(tk.END)
            self.sent_messages.config(state=tk.DISABLED)
            
        elif message_type in ["serial_received", "received"]:
            # Show in received messages area
            self.received_messages.config(state=tk.NORMAL)
            self.received_messages.insert(tk.END, formatted_message)
            self.received_messages.see(tk.END)
            self.received_messages.config(state=tk.DISABLED)
        
        elif message_type == "error":
            # Show errors in both areas with red color
            self.sent_messages.config(state=tk.NORMAL)
            self.sent_messages.insert(tk.END, formatted_message, "error")
            self.sent_messages.tag_config("error", foreground="red")
            self.sent_messages.see(tk.END)
            self.sent_messages.config(state=tk.DISABLED)
            
            self.received_messages.config(state=tk.NORMAL)
            self.received_messages.insert(tk.END, formatted_message, "error")
            self.received_messages.tag_config("error", foreground="red")
            self.received_messages.see(tk.END)
            self.received_messages.config(state=tk.DISABLED)
            
        elif message_type in ["status", "serial_status"]:
            # Show status messages in both areas with purple color
            self.sent_messages.config(state=tk.NORMAL)
            self.sent_messages.insert(tk.END, formatted_message, "status")
            self.sent_messages.tag_config("status", foreground="purple")
            self.sent_messages.see(tk.END)
            self.sent_messages.config(state=tk.DISABLED)
            
            self.received_messages.config(state=tk.NORMAL)
            self.received_messages.insert(tk.END, formatted_message, "status")
            self.received_messages.tag_config("status", foreground="purple")
            self.received_messages.see(tk.END)
            self.received_messages.config(state=tk.DISABLED)